# drivers 目录学习文档（Out-of-tree Drivers）

本仓库是一个 Zephyr module。`drivers/` 目录用于放“仓库外置(out-of-tree)”驱动，也就是不在 Zephyr 主仓库里的驱动实现，但仍然复用 Zephyr 的构建系统、Kconfig、devicetree、device model 等机制。

这份文档按“照着现有例子学会自己加驱动”的目标来写，结构和 `boards/` 的学习方式一致：先讲入口与发现机制，再讲目录结构与每个文件职责，然后给新增驱动的最小步骤清单。

## 0. Zephyr 为什么会编译到本仓库的 drivers

关键链路如下：

1. Zephyr 通过 [zephyr/module.yml](/home/hjp/zephyrproject/app/example-application/zephyr/module.yml) 把本仓库作为 module 引入（其中 `build.cmake: .` 和 `build.kconfig: Kconfig`）。
2. 本仓库顶层 [CMakeLists.txt](/home/hjp/zephyrproject/app/example-application/CMakeLists.txt) 会 `add_subdirectory(drivers)`，因此 `drivers/CMakeLists.txt` 会进入 Zephyr 构建。
3. 本仓库顶层 [Kconfig](/home/hjp/zephyrproject/app/example-application/Kconfig) 会通过 module 机制被 source 到 Zephyr 的 Kconfig 树里，而 [drivers/Kconfig](/home/hjp/zephyrproject/app/example-application/drivers/Kconfig) 又把驱动子目录的 Kconfig 组织成一个菜单。
4. `zephyr/module.yml` 里配置了 `dts_root: .`，所以本仓库的 [dts/bindings](/home/hjp/zephyrproject/app/example-application/dts/bindings) 也会被 Zephyr 当作额外的 devicetree binding 搜索路径。

总结：只要 Kconfig 选项打开，并且 CMakeLists 挂上了子目录/源文件，对应驱动就会被编译进来。

### 0.1 `.config` 在哪里、什么时候生成（理解 CONFIG_xxx 的前提）

很多人第一次看 `drivers/CMakeLists.txt` 会疑惑：`CONFIG_BLINK` 这种变量到底在哪里被“决定”为 y/n？

答案是：它来自 Kconfig 的配置结果，最终落在构建输出目录的 `.config` 文件里。CMake 只是读取它来决定是否编译某些目录。

`.config` 位置（典型）：

- `<build_dir>/zephyr/.config`

例如你这样构建：

```sh
west build -b ats_stm32h745/stm32h745xx/m7 -d build_m7 app
```

那么 `.config` 就在：

- `build_m7/zephyr/.config`

生成时机：

- 每次你执行 `west build` 时（更准确说：CMake 配置阶段会触发 Kconfig），Kconfig 会综合以下输入生成/更新 `.config`：
  - board 的 `*_defconfig`（board目录下）
  - 应用的 `prj.conf`（以及 overlay config）
  - 所有依赖关系与默认值计算

如果你改了 `prj.conf`、Kconfig 文件、板卡 defconfig 等，并使用 `west build -p auto` 重新配置，`.config` 就会相应更新。

## 1. 当前仓库 drivers 目录结构

你现在的 `drivers/` 里有两条典型路线：

1. 自定义驱动类（Custom driver class）
   - 例子：`drivers/blink/`
   - 特点：你自己定义一套新的设备类 API（类似 Zephyr 自带的 `sensor`、`gpio` 类），然后为这个类实现具体驱动。
2. 复用 Zephyr 已有驱动类（Existing driver class）
   - 例子：`drivers/sensor/example_sensor/`
   - 特点：不发明新类，而是直接实现 Zephyr 已有的 driver API（例如 `sensor_driver_api`），这样应用层就用 Zephyr 标准接口访问。

## 2. drivers 目录的“总入口文件”分别是谁

### 2.1 drivers/CMakeLists.txt

文件：[drivers/CMakeLists.txt](/home/hjp/zephyrproject/app/example-application/drivers/CMakeLists.txt)

用途：
按 Kconfig 开关决定是否进入子目录编译。例如：

- `add_subdirectory_ifdef(CONFIG_BLINK blink)`
- `add_subdirectory_ifdef(CONFIG_SENSOR sensor)`

你新增一个驱动子系统目录时，通常要在这里加一行 `add_subdirectory_ifdef(...)`。

### 2.2 drivers/Kconfig

文件：[drivers/Kconfig](/home/hjp/zephyrproject/app/example-application/drivers/Kconfig)

用途：
把驱动相关的 Kconfig 入口组织起来（menu/rsource）。例如：

- `menu "Drivers"`
- `rsource "blink/Kconfig"`
- `rsource "sensor/Kconfig"`

你新增驱动目录时，通常要在这里 `rsource` 你的新目录 Kconfig。

## 3. 示例 1：自定义驱动类 blink（从 0 定义一套 driver API）

目录：`drivers/blink/`

### 3.1 这个示例想教你什么

它演示了“我想要一类新的设备：blink 设备（能以周期闪烁 LED）”，并且：

1. 定义一个新的 driver class API（头文件在 [blink.h](/home/hjp/zephyrproject/app/example-application/include/app/drivers/blink.h)）
2. 提供一个具体实现：GPIO LED 闪烁驱动（代码在 [gpio_led.c](/home/hjp/zephyrproject/app/example-application/drivers/blink/gpio_led.c)）
3. 配套 Kconfig/CMake/devicetree binding，让它能被 Kconfig 选中、被 DTS 描述并自动实例化

### 3.2 自定义 driver class API 在哪里

文件：[blink.h](/home/hjp/zephyrproject/app/example-application/include/app/drivers/blink.h)

你会看到几个关键点：

- `__subsystem struct blink_driver_api { ... }`
  这是 Zephyr 的“子系统/驱动类 API”模式：定义一组操作函数指针。
- `DEVICE_API_GET(blink, dev)->set_period_ms(...)`
  这是通过 device model 获取该设备实例对应的 API 实现。
- `__syscall int blink_set_period_ms(...)`
  表示这个 API 支持系统调用封装（如果需要用户态/内核态隔离时会用到）。

为什么顶层 CMake 里有 `zephyr_syscall_include_directories(include)`：
它让 Zephyr 能扫描 `include/` 下带 `__syscall` 的头文件，生成对应的 syscall glue 代码。

### 3.3 具体驱动实现 gpio_led.c 的关键套路

文件：[gpio_led.c](/home/hjp/zephyrproject/app/example-application/drivers/blink/gpio_led.c)

关键点按阅读顺序解释：

- `#define DT_DRV_COMPAT blink_gpio_led`
  把驱动代码绑定到某个 devicetree `compatible`。这个宏的值是把 binding 里的字符串做了规范化转换得到的符号名。
- `struct ..._config` / `struct ..._data`
  Zephyr 驱动常见写法：config 放常量配置（来自 DTS），data 放运行时状态。
- `DEVICE_API(blink, ...)`
  表示这个设备实现了 blink driver class 的操作表。
- `DEVICE_DT_INST_DEFINE(...)` + `DT_INST_FOREACH_STATUS_OKAY(...)`
  经典的“按 DTS 实例自动生成设备”方式：devicetree 里出现多少个 `status = "okay"` 的实例，就生成多少个 device。

#### 3.3.1 `DEVICE_API` 这一组宏到底在做什么

在 Zephyr 的设备模型里（见 Zephyr 源码 `include/zephyr/device.h`），常见会一起出现三类宏：

- `DEVICE_API(<class>, <name>)`
  定义某个 driver class 的 API 表（vtable）。该宏会把 vtable 放进一个“可迭代 section”
  （iterable section），便于系统在链接后拿到“某类设备 API 表”的地址范围。
- `DEVICE_API_GET(<class>, dev)`
  从 `dev->api` 取出并强转为该 class 的 API 指针类型（例如 blink 就是 `struct blink_driver_api *`）。
- `DEVICE_API_IS(<class>, dev)`
  利用 section 的起止地址判断 `dev->api` 是否属于该 class，从而在运行时做“类型校验”。

在本仓库的 blink driver class 里，[blink.h](/home/hjp/zephyrproject/app/example-application/include/app/drivers/blink.h) 使用了：

- `__ASSERT_NO_MSG(DEVICE_API_IS(blink, dev));`

这能在调试阶段尽早发现“把不是 blink 类设备的 device 指针传进来”的错误。

### 3.4 Kconfig 与 CMake 怎么把它编进去

文件：

- [drivers/blink/Kconfig](/home/hjp/zephyrproject/app/example-application/drivers/blink/Kconfig)
  这里定义顶层开关 `CONFIG_BLINK`，并提供日志模板配置。
- [drivers/blink/Kconfig.gpio_led](/home/hjp/zephyrproject/app/example-application/drivers/blink/Kconfig.gpio_led)
  这里定义具体实现开关 `CONFIG_BLINK_GPIO_LED`，并 `depends on DT_HAS_BLINK_GPIO_LED_ENABLED`。
- [drivers/blink/CMakeLists.txt](/home/hjp/zephyrproject/app/example-application/drivers/blink/CMakeLists.txt)
  `zephyr_library_sources_ifdef(CONFIG_BLINK_GPIO_LED gpio_led.c)`

其中 `DT_HAS_BLINK_GPIO_LED_ENABLED` 来自 devicetree：只有当 DTS 里存在 compatible 为 `blink-gpio-led` 且 status=okay 的节点时，这个宏才会为 y。

### 3.5 devicetree binding 在哪里

文件：[blink-gpio-leds.yaml](/home/hjp/zephyrproject/app/example-application/dts/bindings/blink/blink-gpio-leds.yaml)

它定义了：

- `compatible: "blink-gpio-led"`
- `led-gpios`、`blink-period-ms` 等属性

驱动里 `DT_DRV_COMPAT blink_gpio_led` 就是由 `blink-gpio-led` 规范化而来。

## 4. 示例 2：复用 Zephyr 现有驱动类 sensor/example_sensor（实现 Zephyr 标准 API）

目录：`drivers/sensor/example_sensor/`

### 4.1 这个示例想教你什么

它演示“我实现一个传感器”，但不定义新 driver class，而是直接实现 Zephyr 的 `sensor` 子系统：

- 代码 include 了 `<zephyr/drivers/sensor.h>`
- 提供 `DEVICE_API(sensor, example_sensor_api)`

这样应用就能用标准的 `sensor_sample_fetch()` / `sensor_channel_get()` 访问它。

### 4.2 Kconfig / CMake / DTS binding 的对应关系

文件：

- [drivers/sensor/example_sensor/Kconfig](/home/hjp/zephyrproject/app/example-application/drivers/sensor/example_sensor/Kconfig)
  `depends on DT_HAS_ZEPHYR_EXAMPLE_SENSOR_ENABLED`，表示 devicetree 有实例才允许打开。
- [drivers/sensor/example_sensor/CMakeLists.txt](/home/hjp/zephyrproject/app/example-application/drivers/sensor/example_sensor/CMakeLists.txt)
  `zephyr_library_sources(example_sensor.c)`
- binding：[zephyr,example-sensor.yaml](/home/hjp/zephyrproject/app/example-application/dts/bindings/sensor/zephyr,example-sensor.yaml)
  `compatible: "zephyr,example-sensor"`
- 驱动代码：[example_sensor.c](/home/hjp/zephyrproject/app/example-application/drivers/sensor/example_sensor/example_sensor.c)
  `#define DT_DRV_COMPAT zephyr_example_sensor`

## 5. 新增一个驱动：最小步骤清单（照着做）

下面给你一个“从 0 到能编译”的最小 checklist，你可以先用最少代码跑通，再逐步增强。

### 5.1 决策：你是实现“现有驱动类”还是“自定义驱动类”

- 实现现有驱动类：优先选这个。
  例如你做传感器就实现 `sensor`，做 LED 就看是否能用 `led`/`gpio`/`pwm-leds` 等机制。
- 自定义驱动类：只有当 Zephyr 没有合适的子系统抽象，或你想统一一组设备的上层 API 时再做。

### 5.2 建目录与入口

假设你要加一个新驱动 `drivers/foo/bar/`：

1. 新建 `drivers/foo/Kconfig`、`drivers/foo/CMakeLists.txt`
2. 新建 `drivers/foo/bar/Kconfig`、`drivers/foo/bar/CMakeLists.txt`、`drivers/foo/bar/bar.c`
3. 修改 [drivers/Kconfig](/home/hjp/zephyrproject/app/example-application/drivers/Kconfig) 增加 `rsource "foo/Kconfig"`
4. 修改 [drivers/CMakeLists.txt](/home/hjp/zephyrproject/app/example-application/drivers/CMakeLists.txt) 增加 `add_subdirectory_ifdef(CONFIG_FOO foo)`

### 5.3 写 binding（让 DTS 能描述你的设备）

在本仓库新增一个 binding，例如：

`dts/bindings/foo/vendor,bar.yaml` 或 `dts/bindings/foo/bar.yaml`

至少包含：

- `compatible: "..."`（字符串）
- `properties:`（你驱动要用到的属性）

然后在驱动里：

- `#define DT_DRV_COMPAT ...`（与 compatible 规范化后的符号一致）
- 用 `DT_INST_PROP()`、`GPIO_DT_SPEC_INST_GET()` 等宏读属性

### 5.4 Kconfig 典型写法

常见做法是让驱动选项依赖 “DTS 里真的有这个设备”：

`depends on DT_HAS_<DT_DRV_COMPAT>_ENABLED`

这样避免没有设备节点时还把驱动编译进去。

### 5.5 代码实例化典型写法

最常见的模板：

- `DEVICE_DT_INST_DEFINE(inst, ...)`
- `DT_INST_FOREACH_STATUS_OKAY(MY_INIT_MACRO)`

如果你只想先做单实例，也可以先写死一个实例，但不推荐长期保持。

## 6. 常见踩坑（提前知道能省很多时间）

- `DT_DRV_COMPAT` 与 binding `compatible` 不匹配。
  现象：`DT_HAS_...` 不生效、`DT_INST_FOREACH...` 为 0，或者编译期找不到属性。
- Kconfig 没接到总入口。
  现象：menuconfig 里搜不到你的 `CONFIG_...`。
- CMake 没接到总入口。
  现象：Kconfig 选项打开了，但源文件没参与编译（没有被 `add_subdirectory_ifdef` / `zephyr_library_sources...` 引入）。
- binding 没被 Zephyr 找到。
  现象：devicetree 编译报 “unknown compatible”。
  解决：确认 module.yml 的 `dts_root: .` 生效，binding 放在 `dts/bindings/**.yaml` 下。
