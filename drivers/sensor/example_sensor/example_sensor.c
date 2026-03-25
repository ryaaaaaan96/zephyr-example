/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * 这个文件演示“实现 Zephyr 现有 driver class”的方式：实现 sensor 子系统。
 *
 * 与 blink 示例不同：
 * - blink 示例是自定义 driver class（API 在本仓库 include 里定义）
 * - 这里直接实现 Zephyr 标准 sensor API（<zephyr/drivers/sensor.h>）
 *
 * devicetree + binding：
 * - binding: dts/bindings/sensor/zephyr,example-sensor.yaml
 * - compatible: "zephyr,example-sensor"
 * - 属性 input-gpios：指定一个 GPIO 输入引脚
 *
 * 驱动行为：
 * - sample_fetch() 读取 input GPIO 电平，写入 data->state
 * - channel_get() 以 SENSOR_CHAN_PROX 通道返回该电平
 */

#define DT_DRV_COMPAT zephyr_example_sensor

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>

#include <zephyr/logging/log.h>
/*
 * 这里使用的是 sensor 子系统的日志等级配置 CONFIG_SENSOR_LOG_LEVEL。
 * 这是 Zephyr 自带的 sensor 框架常见做法：共享一组 log level 配置。
 */
LOG_MODULE_REGISTER(example_sensor, CONFIG_SENSOR_LOG_LEVEL);

/* 运行时数据（每个 devicetree 实例对应一份 data） */
struct example_sensor_data {
	int state;
};

/* 常量配置（来自 devicetree 的 input-gpios） */
struct example_sensor_config {
	struct gpio_dt_spec input;
};

/*
 * sensor_sample_fetch():
 * - Zephyr sensor 标准 API：采样/刷新传感器数据
 * - 本例把 GPIO 电平读出来，放到 data->state
 *
 * 注意：
 * - 这里没有处理 chan（简单示例），实际驱动通常会根据 chan 采不同数据。
 */
static int example_sensor_sample_fetch(const struct device *dev,
				      enum sensor_channel chan)
{
	const struct example_sensor_config *config = dev->config;
	struct example_sensor_data *data = dev->data;

	data->state = gpio_pin_get_dt(&config->input);

	return 0;
}

/*
 * sensor_channel_get():
 * - Zephyr sensor 标准 API：读取某个通道的当前值
 * - 本例只支持 SENSOR_CHAN_PROX（“接近”通道），用来演示 API
 * - 返回值通过 struct sensor_value 传出
 */
static int example_sensor_channel_get(const struct device *dev,
				     enum sensor_channel chan,
				     struct sensor_value *val)
{
	struct example_sensor_data *data = dev->data;

	if (chan != SENSOR_CHAN_PROX) {
		return -ENOTSUP;
	}

	val->val1 = data->state;

	return 0;
}

/*
 * 该设备实现的 sensor API 操作表。
 * Zephyr sensor 框架会通过这个表调用到 sample_fetch/channel_get。
 */
static DEVICE_API(sensor, example_sensor_api) = {
	.sample_fetch = &example_sensor_sample_fetch,
	.channel_get = &example_sensor_channel_get,
};

/*
 * 设备初始化：
 * - 检查 input GPIO 对应的 port 设备是否 ready
 * - 把 input GPIO 配置为输入
 */
static int example_sensor_init(const struct device *dev)
{
	const struct example_sensor_config *config = dev->config;

	int ret;

	if (!device_is_ready(config->input.port)) {
		LOG_ERR("Input GPIO not ready");
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(&config->input, GPIO_INPUT);
	if (ret < 0) {
		LOG_ERR("Could not configure input GPIO (%d)", ret);
		return ret;
	}

	return 0;
}

/*
 * 设备实例化宏：
 * - 对每个 DTS 实例生成 data/config/device
 * - GPIO_DT_SPEC_INST_GET(i, input_gpios) 会读取 binding 里的 input-gpios 属性
 * - DEVICE_DT_INST_DEFINE 会注册该 device，并指定 init 阶段/优先级/API
 *
 * 这里的 init priority 使用 Zephyr 自带的 CONFIG_SENSOR_INIT_PRIORITY。
 */
#define EXAMPLE_SENSOR_INIT(i)						       \
	static struct example_sensor_data example_sensor_data_##i;	       \
									       \
	static const struct example_sensor_config example_sensor_config_##i = {\
		.input = GPIO_DT_SPEC_INST_GET(i, input_gpios),		       \
	};								       \
									       \
	DEVICE_DT_INST_DEFINE(i, example_sensor_init, NULL,		       \
			      &example_sensor_data_##i,			       \
			      &example_sensor_config_##i, POST_KERNEL,	       \
			      CONFIG_SENSOR_INIT_PRIORITY, &example_sensor_api);

/* 按 DTS 中 status="okay" 的实例数量自动展开 EXAMPLE_SENSOR_INIT(i) */
DT_INST_FOREACH_STATUS_OKAY(EXAMPLE_SENSOR_INIT)
