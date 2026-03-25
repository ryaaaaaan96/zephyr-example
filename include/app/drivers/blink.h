/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef APP_DRIVERS_BLINK_H_
#define APP_DRIVERS_BLINK_H_

#include <zephyr/device.h>
#include <zephyr/toolchain.h>

/**
 * @defgroup drivers_blink Blink drivers
 * @ingroup drivers
 * @{
 *
 * @brief A custom driver class to blink LEDs
 *
 * This driver class is provided as an example of how to create custom driver
 * classes. It provides an interface to blink an LED at a configurable rate.
 * Implementations could include simple GPIO-controlled LEDs, addressable LEDs,
 * etc.
 */

/**
 * @defgroup drivers_blink_ops Blink driver operations
 * @{
 *
 * @brief blink 驱动类（driver class）的“操作表”（operations / vtable）。
 *
 * driver class 的常见设计是：
 *
 * - 在头文件里定义一个“操作表结构体”（本例是 struct blink_driver_api）。
 * - 每个具体驱动都需要提供该结构体的一份实例（也就是实现这些函数指针）。
 * - 公共 API（例如 blink_set_period_ms）并不直接知道底层驱动是什么，
 *   它会通过 Zephyr 设备模型从 `dev->api` 取回这张操作表，并调用其中的函数。
 *
 * 与本仓库的 blink 驱动实现对应关系（帮助你对齐前面学到的点）：
 *
 * - 在驱动 `.c` 里会看到：
 *     static DEVICE_API(blink, blink_gpio_led_api) = { .set_period_ms = ... };
 *   这就是“给 struct blink_driver_api 填函数指针”的具体实现。
 *
 * - 在公共 API 里会看到：
 *     DEVICE_API_GET(blink, dev)->set_period_ms(dev, period_ms)
 *   这就是“通过 device 指针取回操作表并调用”的那一步。
 *
 * 如果需要支持 system call（用户态调用驱动 API）：
 *
 * - 操作表结构体需要用 `__subsystem` 标记
 * - 且命名需遵循 `${class}_driver_api`（例如 blink -> blink_driver_api）
 *   这是 Zephyr 的 syscall/kobject 相关脚本识别驱动类的重要约定之一
 */

/** @brief Blink driver class operations */
__subsystem struct blink_driver_api {
	/**
	 * @brief Configure the LED blink period.
	 *
	 * @param dev Blink device instance.
	 * @param period_ms Period of the LED blink in milliseconds, 0 to
	 * disable blinking.
	 *
	 * @retval 0 if successful.
	 * @retval -EINVAL if @p period_ms can not be set.
	 * @retval -errno Other negative errno code on failure.
	 */
	int (*set_period_ms)(const struct device *dev, unsigned int period_ms);
};

/** @} */

/**
 * @defgroup drivers_blink_api Blink driver API
 * @{
 *
 * @brief Public API provided by the blink driver class.
 *
 * The public API is the interface that is used by applications to interact with
 * devices that implement the blink driver class. If support for system calls is
 * needed, functions accessing device fields need to be tagged with `__syscall`
 * and provide an implementation that follows the `z_impl_${function_name}`
 * naming scheme.
 */

/**
 * @brief Configure the LED blink period.
 *
 *
 * @param dev Blink device instance.
 * @param period_ms Period of the LED blink in milliseconds.
 *
 * @retval 0 if successful.
 * @retval -EINVAL if @p period_ms can not be set.
 * @retval -errno Other negative errno code on failure.
 */
__syscall int blink_set_period_ms(const struct device *dev,
				  unsigned int period_ms);

static inline int z_impl_blink_set_period_ms(const struct device *dev,
					     unsigned int period_ms)
{
	__ASSERT_NO_MSG(DEVICE_API_IS(blink, dev));

	/*
	 * 关键点：怎么“取回操作表（vtable）”
	 *
	 * - Zephyr 的 `struct device` 里有一个成员 `const void *api;`
	 *   每个 device 实例都会把它指向“该设备所属驱动类的操作表结构体”（vtable）。
	 *
	 * - 对 blink driver class 来说，这张表的类型就是：
	 *     `struct blink_driver_api`
	 *
	 * - DEVICE_API_GET(blink, dev) 在 Zephyr 里本质等价于：
	 *     `((const struct blink_driver_api *)dev->api)`
	 *   也就是把 `dev->api` 这个 `void *` 强转成正确的操作表类型指针。
	 *
	 * - 那 `dev->api` 是什么时候被赋值/绑定的？
	 *   在具体驱动的 `.c` 文件里（例如 drivers/blink/gpio_led.c），会：
	 *     1) 定义一张操作表实例：
	 *          `static DEVICE_API(blink, blink_gpio_led_api) = { ... };`
	 *     2) 在 DEVICE_DT_INST_DEFINE(..., &blink_gpio_led_api) 里把它作为参数传进去，
	 *        从而绑定到该 device 实例的 `api` 字段上。
	 *
	 * 所以这里这一行，就是“通过 device 指针取回操作表，并调用其 set_period_ms()”。
	 */
	return DEVICE_API_GET(blink, dev)->set_period_ms(dev, period_ms);
}

/**
 * @brief Turn LED blinking off.
 *
 * This is a convenience function to turn off the LED blinking. It also shows
 * how to create convenience functions that re-use other driver functions, or
 * driver operations, to provide a higher-level API.
 *
 * @param dev Blink device instance.
 *
 * @return See blink_set_period_ms().
 */
static inline int blink_off(const struct device *dev)
{
	return blink_set_period_ms(dev, 0);
}

#include <syscalls/blink.h>

/** @} */

/** @} */

#endif /* APP_DRIVERS_BLINK_H_ */
