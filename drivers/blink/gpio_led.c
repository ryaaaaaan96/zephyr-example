/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * 这个文件实现的是 blink 自定义 driver class 的一个具体驱动：
 * - 通过 GPIO 控制一个 LED 引脚，并用 Zephyr 的 k_timer 周期性翻转引脚电平
 * - 对外暴露的 API 是自定义的 blink driver class（见 include/app/drivers/blink.h）
 *
 * 你可以把它理解为：
 * - devicetree 描述一个 “blink-gpio-led” 设备实例（指定 led-gpios、blink-period-ms）
 * - Kconfig 打开 CONFIG_BLINK 和 CONFIG_BLINK_GPIO_LED
 * - 编译时根据 DTS 的实例数量自动生成 device（DEVICE_DT_INST_DEFINE）
 * - 运行时应用通过 blink_set_period_ms() 去控制闪烁周期
 */

#define DT_DRV_COMPAT blink_gpio_led

#include <zephyr/device.h>

#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <app/drivers/blink.h>

/*
 * LOG_MODULE_REGISTER：
 * - 注册该 C 文件的日志模块名（这里是 blink_gpio_led）
 * - 第二个参数是日志等级的 Kconfig 值（来自 drivers/blink/Kconfig 的日志模板）
 *
 * 这样你就可以在 prj.conf/menuconfig 里用 CONFIG_BLINK_LOG_LEVEL 控制该驱动日志详细程度。
 *
 * 使用示例（写在 app/prj.conf 里）：
 *
 *   # 打开 Zephyr 日志子系统（很多工程默认已开启；若未开启需要显式打开）
 *   CONFIG_LOG=y
 *
 *   # 设置该模块日志等级（常见取值：0=OFF, 1=ERR, 2=WRN, 3=INF, 4=DBG）
 *   CONFIG_BLINK_LOG_LEVEL=4
 *
 * 然后在代码里就可以按需要打印：
 * - LOG_ERR(...)：错误信息（等级最低，最容易看到）
 * - LOG_INF(...)：一般信息
 * - LOG_DBG(...)：调试细节（通常只在 DBG 等级下显示）
 */
LOG_MODULE_REGISTER(blink_gpio_led, CONFIG_BLINK_LOG_LEVEL);

/*
 * Zephyr 驱动常见写法：把“运行时数据(data)”和“常量配置(config)”分开。
 *
 * - data：运行时会变化的状态（定时器对象、缓存值、锁等）
 * - config：从 devicetree 读取来的常量配置（GPIO 引脚、周期、地址等）
 *
 * 这样做的好处：
 * - config 可以放在 flash/rodata
 * - data 放在 RAM
 * - 多实例时每个 device 都有自己的一份 config/data
 */
struct blink_gpio_led_data {
	struct k_timer timer;
};

struct blink_gpio_led_config {
	struct gpio_dt_spec led;
	unsigned int period_ms;
};

/*
 * 定时器回调：每次到期就翻转一次 LED 引脚电平。
 *
 * 注意：这里用 k_timer_user_data_set() 绑定了 device 指针，所以回调里能拿到 dev，
 * 从而拿到 config（GPIO）与 data（timer）。
 */
static void blink_gpio_led_on_timer_expire(struct k_timer *timer)
{
	const struct device *dev = k_timer_user_data_get(timer);
	const struct blink_gpio_led_config *config = dev->config;
	int ret;

	ret = gpio_pin_toggle_dt(&config->led);
	if (ret < 0) {
		LOG_ERR("Could not toggle LED GPIO (%d)", ret);
	}
}

/*
 * 这是 blink driver class 的一个操作实现：设置闪烁周期。
 *
 * 约定：
 * - period_ms == 0：停止闪烁，并把 LED 关掉（拉低）
 * - period_ms > 0：启动周期定时器，以 period_ms 为周期翻转 LED
 *
 * 这个函数最终会通过 include/app/drivers/blink.h 里的
 * blink_set_period_ms() -> DEVICE_API_GET(blink, dev)->set_period_ms(...)
 * 被应用层调用到。
 */
static int blink_gpio_led_set_period_ms(const struct device *dev,
					unsigned int period_ms)
{
	const struct blink_gpio_led_config *config = dev->config;
	struct blink_gpio_led_data *data = dev->data;

	if (period_ms == 0) {
		/* 约定：period=0 表示停止闪烁 */
		k_timer_stop(&data->timer);
		/* 顺手把 LED 关掉，避免停在“亮”的状态 */
		return gpio_pin_set_dt(&config->led, 0);
	}

	/*
	 * 启动一个周期性定时器：
	 * - 第一个参数：首次触发延迟
	 * - 第二个参数：周期
	 *
	 * 这里两者都用 period_ms，表示“立刻按该周期开始闪烁”。
	 * 定时器回调里会 toggle GPIO，因此 LED 会以该周期翻转。
	 */
	k_timer_start(&data->timer, K_MSEC(period_ms), K_MSEC(period_ms));

	return 0;
}

/*
 * DEVICE_API(blink, ...):
 * - 声明“这个驱动实现了 blink 这个 driver class”的操作表（API vtable）
 *
 * Zephyr 设备模型里，每个 device 实例都会有一个 api 指针，指向一张“操作函数表”。
 * 对于自定义 driver class（本例 blink）来说：
 *
 * 1) 在 include/app/drivers/blink.h 里定义了：
 *      __subsystem struct blink_driver_api { int (*set_period_ms)(...); };
 *
 * 2) 这里用 DEVICE_API(blink, blink_gpio_led_api) 定义一张操作表，
 *    并把 set_period_ms 指向我们当前驱动的实现函数。
 *
 * 3) 在下方 DEVICE_DT_INST_DEFINE(..., &blink_gpio_led_api) 时，
 *    这张操作表会被绑定到该 device 实例上。
 *
 * 4) 应用层调用 blink_set_period_ms(dev, ...) 时，会走到 blink.h 里的：
 *      DEVICE_API_GET(blink, dev)->set_period_ms(dev, period_ms)
 *    从而间接调用到这里注册的 blink_gpio_led_set_period_ms()。
 *
 * 这组宏在 Zephyr 里具体做了什么（理解“为什么能 GET/IS”很重要）：
 *
 * - DEVICE_API(blink, blink_gpio_led_api)
 *   实际会把该 vtable 声明为 `const struct blink_driver_api`，并放进一个“可迭代 section”
 *   （iterable section）。这样系统在链接后就能拿到“所有 blink 类 API 表”的地址范围。
 *
 * - DEVICE_API_GET(blink, dev)
 *   本质就是把 `dev->api` 强转成 `const struct blink_driver_api *`。
 *
 * - DEVICE_API_IS(blink, dev)
 *   用 section 的起止地址判断：dev->api 是否落在 blink 类 API 表的地址范围内。
 *   这就是 blink.h 里 `__ASSERT_NO_MSG(DEVICE_API_IS(blink, dev))` 能做“类型校验”的原因。
 *
 * 这就是“driver class API + 具体驱动实现”的解耦逻辑：应用只依赖 driver class，
 * 不关心底层是 GPIO LED、地址灯带，还是其他实现。
 *
 * 注意：
 * - 结构体字段名必须与 blink_driver_api 中的函数指针字段一致，否则编译不过或逻辑不对。
 */
static DEVICE_API(blink, blink_gpio_led_api) = {
	.set_period_ms = &blink_gpio_led_set_period_ms,
};

/*
 * 设备初始化函数：
 * - 校验 GPIO 是否 ready
 * - 配置 LED GPIO 为输出并默认关闭
 * - 初始化定时器，并把 dev 绑定为 timer user_data
 * - 如果 devicetree 里配置了初始周期，则启动定时器
 *
 * 注意：init 会在系统启动阶段被自动调用（由 DEVICE_DT_INST_DEFINE 注册）。
 */
static int blink_gpio_led_init(const struct device *dev)
{
	/* dev 是 Zephyr 设备模型的“设备实例指针”，由 DEVICE_DT_INST_DEFINE 生成并传入。 */

	/*
	 * dev->config：
	 * - 指向“只读配置”（通常放在 ROM/Flash）
	 * - 里面一般是从 devicetree 解析出来的参数
	 * - 本驱动里包含：LED 的 GPIO 描述、默认闪烁周期等
	 */
	const struct blink_gpio_led_config *config = dev->config;

	/*
	 * dev->data：
	 * - 指向“运行时数据”（通常在 RAM）
	 * - 存放状态、缓存、同步对象、定时器等
	 * - 本驱动里包含：k_timer 定时器句柄、当前周期等运行期信息
	 */
	struct blink_gpio_led_data *data = dev->data;

	/* 通用返回值：Zephyr 里惯例用 0 表示成功，负 errno 表示失败原因。 */
	int ret;

	/*
	 * 1) 确认 GPIO 设备是否 ready
	 *
	 * gpio_is_ready_dt() 会检查 config->led 中引用的 GPIO controller 设备
	 * 是否已经初始化就绪（例如 GPIO 端口对应的驱动已经完成 init）。
	 *
	 * 如果不 ready：
	 * - 后续对 GPIO 的任何操作都会失败或行为不确定
	 * - 所以这里直接返回 -ENODEV（设备不存在/不可用）
	 */
	if (!gpio_is_ready_dt(&config->led)) {
		LOG_ERR("LED GPIO not ready");
		return -ENODEV;
	}

	/*
	 * 2) 配置 LED 引脚方向和初始电平
	 *
	 * gpio_pin_configure_dt() 会综合 devicetree 里该 GPIO 的 flags（上拉/下拉/极性等）
	 * 并在此基础上配置为输出模式。
	 *
	 * GPIO_OUTPUT_INACTIVE：
	 * - 把引脚配置为输出
	 * - 并设置为“非激活态”（inactive）
	 *
	 * 注意：inactive/active 受 devicetree 的 GPIO_ACTIVE_LOW/HIGH 影响。
	 * - 如果 LED 是低电平点亮（ACTIVE_LOW），inactive 可能对应“输出高电平（灭）”
	 * - 如果 LED 是高电平点亮（ACTIVE_HIGH），inactive 可能对应“输出低电平（灭）”
	 */
	ret = gpio_pin_configure_dt(&config->led, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) {
		LOG_ERR("Could not configure LED GPIO (%d)", ret);
		return ret;
	}

	/*
	 * 3) 初始化定时器
	 *
	 * k_timer_init():
	 * - 第 2 个参数：超时回调（每次定时器到期会调用）
	 * - 第 3 个参数：停止回调（这里不需要，用 NULL）
	 *
	 * 回调 blink_gpio_led_on_timer_expire() 的职责是“翻转 LED 电平”，实现闪烁。
	 */
	k_timer_init(&data->timer, blink_gpio_led_on_timer_expire, NULL);

	/*
	 * 4) 把当前 device 指针绑定到 timer 的 user_data 上
	 *
	 * 这样在超时回调里可以通过 k_timer_user_data_get(timer) 取回 dev，
	 * 进而访问 dev->config/dev->data，知道该翻转哪个 GPIO、使用哪个运行时数据。
	 *
	 * 这里的 (void *)dev 是把 const struct device * 作为“只读指针”塞进 user_data。
	 * 取回时通常再转换回 const struct device * 使用。
	 */
	k_timer_user_data_set(&data->timer, (void *)dev);

		/*
		 * 5) 根据 devicetree 的默认周期决定是否启动闪烁
		 *
		 * - 如果 period_ms > 0：启动周期定时器
		 * - 如果 period_ms == 0：不自动闪烁，等待应用层调用 blink_set_period_ms()
		 *
		 * 你可能会问：为什么这里不“复用” blink_gpio_led_set_period_ms(dev, ...)？
		 *
		 * 结论：能复用，但这里选择直接 k_timer_start() 是一种常见驱动写法。
		 *
		 * 原因 1：分层与耦合
		 * - blink_gpio_led_set_period_ms() / blink_set_period_ms() 属于“对外 API 语义”，
		 *   面向应用层调用（例如 period_ms==0 时顺手把 LED 关掉）。
		 * - init 的目标是把硬件带到基础可用状态，通常会直接操作底层资源（GPIO/timer），
		 *   尽量不依赖公共 API 的行为细节，避免以后改 API 语义时影响 init 流程。
		 *
		 * 原因 2：初始化时序更直观
		 * - set_period_ms() 的实现会直接 k_timer_start()/stop()，它默认 timer 已经 init。
		 * - init 里当前顺序是：k_timer_init() -> k_timer_user_data_set() -> (可选) start。
		 *   这样你一眼就能看出“先准备好回调需要的数据，再启动定时器”。
		 *
		 * 如果你确实想复用代码（减少重复），推荐这样写（且必须在 timer 初始化之后）：
		 * - if (config->period_ms > 0) { blink_gpio_led_set_period_ms(dev, config->period_ms); }
		 *
		 * k_timer_start(timer, duration, period):
		 * - duration：第一次到期的延时
		 * - period：后续重复周期
	 * 这里两者都设置为 period_ms，表示按固定周期持续触发。
	 */
	if (config->period_ms > 0) {
		k_timer_start(&data->timer, K_MSEC(config->period_ms),
			      K_MSEC(config->period_ms));
	}

	/* 初始化成功。此时 GPIO 已配置好；若 period_ms>0，则定时器已在跑。 */
	return 0;
}

/*
 * 设备实例化宏（关键套路）：
 *
 * - inst 是 devicetree 的实例编号（0,1,2...）
 * - 为每个 inst 生成：
 *   - 一份 data（运行时）
 *   - 一份 config（从 DTS 取配置）
 *   - 一个 device 定义（DEVICE_DT_INST_DEFINE）
 *
 * 其中：
 * - GPIO_DT_SPEC_INST_GET(inst, led_gpios)
 *   会读取 binding 里定义的 `led-gpios` 属性，生成 gpio_dt_spec（port/pin/flags）
 * - DT_INST_PROP_OR(inst, blink_period_ms, 0U)
 *   读取 `blink-period-ms`，如果没写则默认为 0（表示不自动启动闪烁）
 *
 * DEVICE_DT_INST_DEFINE 的几个关键参数：
 * - init：blink_gpio_led_init
 * - level：POST_KERNEL（在内核启动后初始化，属于 device 类常见阶段）
 * - priority：CONFIG_BLINK_INIT_PRIORITY（可配置初始化优先级）
 * - api：blink_gpio_led_api（实现 blink driver class）
 */
#define BLINK_GPIO_LED_DEFINE(inst)                                            \
	static struct blink_gpio_led_data data##inst;                          \
                                                                               \
	static const struct blink_gpio_led_config config##inst = {             \
	    .led = GPIO_DT_SPEC_INST_GET(inst, led_gpios),                     \
	    .period_ms = DT_INST_PROP_OR(inst, blink_period_ms, 0U),           \
	};                                                                     \
                                                                               \
	DEVICE_DT_INST_DEFINE(inst, blink_gpio_led_init, NULL, &data##inst,    \
			      &config##inst, POST_KERNEL,                      \
			      CONFIG_BLINK_INIT_PRIORITY,                      \
			      &blink_gpio_led_api);

/*
 * DT_INST_FOREACH_STATUS_OKAY：
 * - 遍历所有 compatible=DT_DRV_COMPAT 且 status="okay" 的 devicetree 实例
 * - 对每个实例展开一次 BLINK_GPIO_LED_DEFINE(inst)
 *
 * 结果就是：DTS 里有几个 blink-gpio-led 节点（status=okay），就生成几个 device。
 */
DT_INST_FOREACH_STATUS_OKAY(BLINK_GPIO_LED_DEFINE)
