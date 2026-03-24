# boards/vendor 板级支持包(BSP)创建指南

本仓库作为一个 Zephyr module，通过 [zephyr/module.yml](/home/hjp/zephyrproject/app/example-application/zephyr/module.yml) 中的 `board_root: .` 将本仓库的 `boards/` 目录加入 Zephyr 的 Board 搜索路径。因此，把自定义板卡放到 `boards/vendor/<board_name>/` 后，`west build -b <board>` 即可直接发现并构建。

下面以 `boards/vendor/nucleo_h745zi_q/` 与 `boards/vendor/stm32h745i_disco/` 的组织方式为参考，说明如何为自定义板卡 `ATS-stm32H745` 创建 BSP。
如果你希望看一个“带详细注释的、可照着改的”完整例子，可直接参考本仓库生成的教程板卡目录：
[boards/vendor/ats_stm32h745/README.md](/home/hjp/zephyrproject/app/example-application/boards/vendor/ats_stm32h745/README.md)。

## 1. 命名与“多核”模型

Zephyr 的板卡名建议使用小写 + 下划线，例如把 `ATS-stm32H745` 映射为目录与 board name：`ats_stm32h745`。

STM32H745 为双核(M7/M4)。参考工程采用“同一块板 + SoC + core qualifier”的 board 标识方式：

```text
<board_name>/stm32h745xx/m7
<board_name>/stm32h745xx/m4
```

对应落地到文件名时，通常会把三段信息拼在一起：

```text
<board_name>_stm32h745xx_m7.*
<board_name>_stm32h745xx_m4.*
```

## 2. 建议的目录结构(最小可用)

为 `ats_stm32h745` 新建目录：

```text
boards/vendor/ats_stm32h745/
  board.yml
  Kconfig.ats_stm32h745
  Kconfig.defconfig
  board.cmake
  support/
    openocd.cfg
  doc/
    index.rst
  ats_stm32h745.dtsi
  ats_stm32h745_stm32h745xx_m7.dts
  ats_stm32h745_stm32h745xx_m4.dts
  ats_stm32h745_stm32h745xx_m7_defconfig
  ats_stm32h745_stm32h745xx_m4_defconfig
  ats_stm32h745_stm32h745xx_m7.yaml
  ats_stm32h745_stm32h745xx_m4.yaml
```

如果你的板子没有 OpenOCD/JLink/CubeProgrammer 需求，可以先不放 `board.cmake` 与 `support/openocd.cfg`，但建议后续补齐以便统一烧录体验。

## 3. 各文件的职责(按参考板卡的设计逻辑)

`board.yml`
Zephyr 的 board 元数据入口，描述 board 名称、vendor、以及关联 SoC(此处是 `stm32h745xx`)。

`Kconfig.<board_name>`
定义 `CONFIG_BOARD_<...>`，并根据 m7/m4 的 board 变体选择正确的 SoC 配置，参考：
`select SOC_STM32H745XX_M7 if ..._M7`、`select SOC_STM32H745XX_M4 if ..._M4`。

`Kconfig.defconfig`
放置该板卡“全局默认”的 Kconfig 选项(例如默认网络 L2、某些外设默认使能等)。

`<board_name>.dtsi`
板级公共硬件描述，通常包含：
LED/按键/连接器(Arduino header)、mailbox(双核通信)、电源与 RCC 一些共用设置、以及 pinctrl `.dtsi` 的 include。

`<board_name>_stm32h745xx_m7.dts` 与 `_m4.dts`
每个核一个顶层 DTS。典型写法是：

```c
/dts-v1/;
#include <st/h7/stm32h745Xi_m7.dtsi> /* 或 m4 */
#include "<board_name>.dtsi"
```

然后在 `chosen {}` 里明确该核“归属”的资源，例如：
console/shell-uart、flash0/flash1、sram0/sram1、(M7 的) dtcm、以及某些外设归属(例如 canbus)。

`*_defconfig`
每个核一份默认配置，用于“同一块板不同核”走不同默认能力集(例如：M7 开 console/UART，M4 默认关闭某些特性或忽略某些测试标签)。

`*.yaml`
每个核一个描述文件，核心字段是 `identifier: <board_name>/stm32h745xx/m7|m4`，并填写 RAM/Flash 大小与 `supported:` 列表，供测试/工具链/文档等使用。

`board.cmake` 与 `support/openocd.cfg`
定义烧录/调试 runner 参数。对双核板卡，常见做法是在 `board.cmake` 里根据 `CONFIG_BOARD_<...>_M7` / `_M4` 设置 OpenOCD 的 `--target-handle` 指向 `cpu0` 或 `cpu1`。

`doc/index.rst`
板卡说明文档，至少写清楚：
如何分别构建 M7 与 M4(使用 `.../m7` 或 `.../m4` 作为 `-b` 参数)，以及串口/时钟/外设连接等关键注意事项。

## 4. 推荐的创建流程(照着做即可)

1. 复制一个最接近的参考板卡目录作为起点(通常从 `stm32h745i_disco` 或 `nucleo_h745zi_q` 拷贝)到 `boards/vendor/ats_stm32h745/`。
2. 全局重命名文件名与内部字符串：把 `stm32h745i_disco` 或 `nucleo_h745zi_q` 替换为 `ats_stm32h745`，并同步更新 `board.yml` 的 `name/full_name/vendor`。
3. 修改 `Kconfig.<board_name>`：保持“board 选择 SoC”的逻辑不变，只更新 `CONFIG_BOARD_...` 的名字与注释。
4. 修改 DTS：
确认 include 的 SoC dtsi 选择正确(`stm32h745Xi_m7.dtsi`/`stm32h745Xi_m4.dtsi`)，并把 console、flash、sram、外设 pinctrl 等改成你板子真实连线。
5. 修改 `*_defconfig` 与 `*.yaml`：
填入真实的 RAM/Flash、默认外设能力，以及你希望的默认 Kconfig。
6. 最后补齐 `board.cmake`/OpenOCD/JLink/CubeProgrammer 参数，确保 M7/M4 都能单独烧录与调试。

## 5. 最小验证(能被发现并开始编译)

目录与文件就位后，可以先用下面的方式验证 Board 发现与基本编译链路(以 M7 为例)：

```sh
west build -b ats_stm32h745/stm32h745xx/m7 -p auto app
```

M4 对应把 `m7` 改为 `m4`：

```sh
west build -b ats_stm32h745/stm32h745xx/m4 -p auto app
```
