# dts 目录学习文档（devicetree bindings）

本仓库作为 Zephyr module，通过 [module.yml](/home/hjp/zephyrproject/app/example-application/zephyr/module.yml) 的 `dts_root: .` 把本仓库的 `dts/` 加入 devicetree 搜索路径。

因此，你在这里新增的 binding 会被 Zephyr 的 devicetree 编译器识别。

这份文档的目标是把你已经学到的内容串起来：

- “binding YAML 写了什么规则”
- “DTS/overlay 怎么写节点来匹配这个 binding”
- “驱动代码怎么从 devicetree 取配置并生成 device”

## 1. 当前目录内容

- [bindings](/home/hjp/zephyrproject/app/example-application/dts/bindings)
  自定义 bindings（YAML）。

当前已有两个 binding 示例：

- [blink-gpio-leds.yaml](/home/hjp/zephyrproject/app/example-application/dts/bindings/blink/blink-gpio-leds.yaml)
  `compatible = "blink-gpio-led"`，供自定义 blink 驱动类的 GPIO LED 实现使用。
- [zephyr,example-sensor.yaml](/home/hjp/zephyrproject/app/example-application/dts/bindings/sensor/zephyr,example-sensor.yaml)
  `compatible = "zephyr,example-sensor"`，供示例 sensor 驱动使用。

## 2. binding YAML 是什么，解决什么问题

在 Zephyr 里，`dts/bindings/**.yaml`（binding）主要做三件事：

- 文档：告诉你某类设备节点应该怎么写（有哪些属性、每个属性什么意思）。
- 校验：构建时检查 DTS/overlay 的属性是否缺失、类型是否匹配等。
- 辅助代码生成：devicetree 会根据最终展开的 DTS 生成大量 `DT_*` 宏，供驱动读取属性。

注意：binding 本身并不会“自动生成设备节点”。它只是规则和说明。
真正的设备节点必须写在某个 DTS/overlay 里，并且 `compatible` 要匹配 binding。

## 3. compatible 与 DT_DRV_COMPAT 的对齐

### 3.1 compatible 的含义

`compatible` 是 devicetree 节点的“类型标识”。Zephyr 用它来：

- 找到应该套用哪个 binding（YAML）
- 找到应该由哪个驱动来实例化（驱动里经常以 DT_DRV_COMPAT 为入口）

### 3.2 vendor 前缀（例如 `zephyr,`）

`zephyr,example-sensor` 这种写法里的 `zephyr,` 是 vendor 前缀（命名空间）：

- 用于避免不同组织/项目之间 compatible 字符串冲突
- 语义上类似“域名/包名”的命名空间概念

而 `blink-gpio-led` 这个示例没有 vendor 前缀，是因为它刻意做成“通用概念示例”。

### 3.3 DT_DRV_COMPAT 如何写

驱动里常见写法：

- `#define DT_DRV_COMPAT <符号化的 compatible>`

符号化规则（最常见的一条）：把 `,` 和 `-` 转成 `_`。
例如：

- `"zephyr,example-sensor"` -> `zephyr_example_sensor`
- `"blink-gpio-led"` -> `blink_gpio_led`

这也是为什么你会在驱动里看到：

- `#define DT_DRV_COMPAT blink_gpio_led`
- `#define DT_DRV_COMPAT zephyr_example_sensor`（如果对应驱动这么写）

## 4. DTS/overlay 在哪里写（谁来“使用” binding）

binding 只是规则。要真正使用它，你需要在 DTS/overlay 里写节点：

- 板级 DTS：`boards/**/<board>.dts` 或 `boards/**/<board>*.dts`
- 应用 overlay：`app/boards/<board>.overlay`（常用于给 Zephyr 已有板子加自定义外设）
- 也可能在其他 overlay/片段文件中，由构建系统合并进最终设备树

本工程里，我们检索到使用了 `compatible = "blink-gpio-led"` 的文件：

- [custom_plank.dts](/home/hjp/zephyrproject/app/example-application/boards/vendor/custom_plank/custom_plank.dts)
  节点：`blink_led: blink-led { compatible = "blink-gpio-led"; ... };`
- [nucleo_f302r8.overlay](/home/hjp/zephyrproject/app/example-application/app/boards/nucleo_f302r8.overlay)
  节点：`blink_led: blink-led { compatible = "blink-gpio-led"; ... };`

快速检索技巧：

```sh
rg -n 'compatible\\s*=\\s*"blink-gpio-led"' -S .
rg -n 'compatible\\s*=\\s*"zephyr,example-sensor"' -S .
```

## 5. properties 如何映射到驱动代码（最关键）

### 5.1 `type: phandle-array` 是什么

`phandle-array` 常用来表达“引用某个控制器节点 + 参数列表”，特别适用于 GPIO/I2C/SPI：

例如 GPIO 常见写法：

```dts
led-gpios = <&gpio0 13 GPIO_ACTIVE_LOW>;
```

含义：

- `&gpio0`：phandle，引用 GPIO 控制器节点（某个 GPIO port 设备）
- `13`：pin 引脚号
- `GPIO_ACTIVE_LOW`：flags（极性/上下拉等）

### 5.2 驱动里如何读取 `phandle-array`（GPIO 的典型套路）

驱动通常不会手动拆 `&gpio0 13 flags`，而是用 Zephyr 提供的宏把它解析成结构体：

- `GPIO_DT_SPEC_INST_GET(inst, <property>)`
  把 `<property>` 解析成 `struct gpio_dt_spec`（包含 port/pin/dt_flags）

在本工程的 blink 驱动实现里：

- binding 属性名：`led-gpios`
- 驱动配置生成：`GPIO_DT_SPEC_INST_GET(inst, led_gpios)`
- init 阶段配置与校验：`gpio_is_ready_dt()`、`gpio_pin_configure_dt()` 等
- 运行时操作：`gpio_pin_toggle_dt()`、`gpio_pin_set_dt()` 等

### 5.3 int/string/boolean 等属性怎么读

常见读取宏示例：

- `DT_INST_PROP_OR(inst, <prop>, <default>)`：读取属性，缺省时用默认值
- `DT_INST_PROP(inst, <prop>)`：读取必选属性（缺失时会在构建阶段报错）

本工程 blink 驱动里：

- binding 属性：`blink-period-ms`（YAML 用 `blink-period-ms`，宏里用下划线 `blink_period_ms`）
- 驱动读取：`DT_INST_PROP_OR(inst, blink_period_ms, 0U)`

## 6. “节点标签”与“node label”在代码里怎么用

当你在 DTS 里写：

```dts
blink_led: blink-led {
    compatible = "blink-gpio-led";
    ...
};
```

这里 `blink_led:` 是“节点标签（node label）”。代码里可以用它拿到对应 device：

```c
const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(blink_led));
```

然后调用 driver class API（例如 blink）：

```c
blink_set_period_ms(dev, 1000);
```

## 7. 与驱动代码的对应关系（从 DTS 到 device 实例）

驱动里常见写法：

- `#define DT_DRV_COMPAT ...`
  与 binding 的 `compatible` 相对应（会做符号化转换，例如 `zephyr,example-sensor` -> `zephyr_example_sensor`）。
- Kconfig 使用 `DT_HAS_<DT_DRV_COMPAT>_ENABLED` 做依赖，确保 DTS 里有实例才启用驱动。

更完整的串联关系可参考：

- [README.md](/home/hjp/zephyrproject/app/example-application/drivers/README.md)

再用 blink-gpio-led 这条链路把“从 binding 到实例化”串一下：

- binding：定义 `led-gpios`、`blink-period-ms` 的规则
- DTS/overlay：写一个 `compatible = "blink-gpio-led"` 的节点，并提供 `led-gpios` 等属性
- 驱动：`DT_INST_FOREACH_STATUS_OKAY(BLINK_GPIO_LED_DEFINE)` 遍历所有 status="okay" 的实例
- `BLINK_GPIO_LED_DEFINE(inst)`：
  - 用 `GPIO_DT_SPEC_INST_GET`、`DT_INST_PROP_OR` 生成 config
  - 用 `DEVICE_DT_INST_DEFINE(..., &blink_gpio_led_api)` 生成 device，并绑定 `dev->config/dev->data/dev->api`
- 公共 API：`blink_set_period_ms()` -> `DEVICE_API_GET(blink, dev)->set_period_ms(...)`
