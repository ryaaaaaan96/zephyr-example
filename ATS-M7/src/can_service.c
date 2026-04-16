#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/can.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>

#include <errno.h>
#include <string.h>

#include "can_service.h"

#define CAN_NODE DT_NODELABEL(fdcan1)
#define CAN_RX_MSGQ_LEN 16
#define CAN_BITRATE DT_PROP_OR(CAN_NODE, bus_speed, 500000)
#define CAN_SEND_TIMEOUT K_MSEC(100)

static const struct device *can_dev = DEVICE_DT_GET(CAN_NODE);
CAN_MSGQ_DEFINE(can_rx_msgq, CAN_RX_MSGQ_LEN);

static int can_rx_filter_std_id = -1;
static int can_rx_filter_ext_id = -1;

static void can_remove_all_filters(void)
{
	if (can_rx_filter_std_id >= 0) {
		can_remove_rx_filter(can_dev, can_rx_filter_std_id);
		can_rx_filter_std_id = -1;
	}

	if (can_rx_filter_ext_id >= 0) {
		can_remove_rx_filter(can_dev, can_rx_filter_ext_id);
		can_rx_filter_ext_id = -1;
	}
}

static int can_service_set_rx_filter_all(void)
{
	struct can_filter std_all = {
		.id = 0U,
		.mask = 0U,
		.flags = 0U,
	};
	struct can_filter ext_all = {
		.id = 0U,
		.mask = 0U,
		.flags = CAN_FILTER_IDE,
	};
	int ret;
	int ext_ret;

	can_remove_all_filters();
	k_msgq_purge(&can_rx_msgq);

	ret = can_add_rx_filter_msgq(can_dev, &can_rx_msgq, &std_all);
	if (ret < 0) {
		return ret;
	}
	can_rx_filter_std_id = ret;

	ext_ret = can_add_rx_filter_msgq(can_dev, &can_rx_msgq, &ext_all);
	if (ext_ret >= 0) {
		can_rx_filter_ext_id = ext_ret;
		printk("CAN RX filter: all standard + all extended\n");
	} else {
		/* Some setups may not expose ext filters, standard traffic still works. */
		printk("CAN RX filter: all standard (extended add failed: %d)\n", ext_ret);
	}

	return 0;
}

static const char *can_state_name(enum can_state state)
{
	switch (state) {
	case CAN_STATE_ERROR_ACTIVE:
		return "error-active";
	case CAN_STATE_ERROR_WARNING:
		return "error-warning";
	case CAN_STATE_ERROR_PASSIVE:
		return "error-passive";
	case CAN_STATE_BUS_OFF:
		return "bus-off";
	case CAN_STATE_STOPPED:
		return "stopped";
	default:
		return "unknown";
	}
}

static void can_state_change_cb(const struct device *dev, enum can_state state,
				struct can_bus_err_cnt err_cnt, void *user_data)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(user_data);

	printk("CAN state=%s tx_err=%u rx_err=%u\n",
	       can_state_name(state), err_cnt.tx_err_cnt, err_cnt.rx_err_cnt);
}

static int can_service_send_internal(uint32_t id, uint8_t flags,
				     const uint8_t *data, size_t len)
{
	struct can_frame frame = {0};

	if ((data == NULL) && (len > 0U)) {
		return -EINVAL;
	}

	if (len > CAN_MAX_DLC) {
		return -EINVAL;
	}

	if ((flags & CAN_FRAME_IDE) != 0U) {
		if (id > CAN_EXT_ID_MASK) {
			return -EINVAL;
		}
	} else {
		if (id > CAN_STD_ID_MASK) {
			return -EINVAL;
		}
	}

	frame.id = id;
	frame.flags = flags;
	frame.dlc = can_bytes_to_dlc((uint8_t)len);

	if (len > 0U) {
		memcpy(frame.data, data, len);
	}

	return can_send(can_dev, &frame, CAN_SEND_TIMEOUT, NULL, NULL);
}

int can_service_init(void)
{
	int ret;
	uint32_t core_clk = 0U;

	if (!device_is_ready(can_dev)) {
		printk("CAN not ready\n");
		return -ENODEV;
	}

	can_set_state_change_callback(can_dev, can_state_change_cb, NULL);

	ret = can_stop(can_dev);
	if ((ret != 0) && (ret != -EALREADY)) {
		printk("CAN stop failed: %d\n", ret);
		return ret;
	}

	ret = can_set_mode(can_dev, CAN_MODE_NORMAL);
	if (ret != 0) {
		printk("CAN set mode failed: %d\n", ret);
		return ret;
	}

	ret = can_set_bitrate(can_dev, CAN_BITRATE);
	if (ret != 0) {
		printk("CAN set bitrate failed: %d\n", ret);
		return ret;
	}

	ret = can_service_set_rx_filter_all();
	if (ret != 0) {
		printk("CAN set filter failed: %d\n", ret);
		return ret;
	}

	ret = can_start(can_dev);
	if (ret != 0) {
		printk("CAN start failed: %d\n", ret);
		return ret;
	}

	ret = can_get_core_clock(can_dev, &core_clk);
	if (ret == 0) {
		printk("CAN init OK dev=%s bitrate=%u core_clk=%u\n",
		       can_dev->name, CAN_BITRATE, core_clk);
	} else {
		printk("CAN init OK dev=%s bitrate=%u core_clk=n/a (ret=%d)\n",
		       can_dev->name, CAN_BITRATE, ret);
	}

	return 0;
}

int can_service_set_rx_filter(uint32_t id, uint32_t mask, uint8_t flags)
{
	struct can_filter filter = {
		.id = id,
		.mask = mask,
		.flags = flags,
	};
	int filter_id;

	if ((flags & CAN_FILTER_IDE) != 0U) {
		if ((id > CAN_EXT_ID_MASK) || (mask > CAN_EXT_ID_MASK)) {
			return -EINVAL;
		}
	} else {
		if ((id > CAN_STD_ID_MASK) || (mask > CAN_STD_ID_MASK)) {
			return -EINVAL;
		}
	}

	can_remove_all_filters();

	k_msgq_purge(&can_rx_msgq);

	filter_id = can_add_rx_filter_msgq(can_dev, &can_rx_msgq, &filter);
	if (filter_id < 0) {
		return filter_id;
	}

	if ((flags & CAN_FILTER_IDE) != 0U) {
		can_rx_filter_ext_id = filter_id;
	} else {
		can_rx_filter_std_id = filter_id;
	}

	return 0;
}

int can_service_send(uint32_t id, const uint8_t *data, size_t len)
{
	return can_service_send_internal(id, 0, data, len);
}

int can_service_send_ext(uint32_t id, const uint8_t *data, size_t len)
{
	return can_service_send_internal(id, CAN_FRAME_IDE, data, len);
}

int can_service_read(struct can_frame *frame, k_timeout_t timeout)
{
	if (frame == NULL) {
		return -EINVAL;
	}

	return k_msgq_get(&can_rx_msgq, frame, timeout);
}
