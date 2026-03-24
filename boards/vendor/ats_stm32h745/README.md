# ATS STM32H745IIT6 板卡上电调试学习笔记

这份文档的目标是帮助你通过“做出来并能跑起来”的方式理解 Zephyr 的 Board/BSP 是怎么组织的。
我们以本仓库里的 `boards/vendor/ats_stm32h745/` 为例，讲清楚：

1. Zephyr 如何发现一个 board（为什么放在这里就能被 `-b ...` 找到）
2. 双核 STM32H745 (M7/M4) 为什么会有两个 build target
3. 每个文件在 BSP 里到底负责什么
4. 把 console 绑定到 UART7、并基于 25MHz HSE 的时钟配置应该写在哪里
5. 初次上电调试最容易踩的坑（pinctrl、memory、chosen 等）

注意：你当前这个工作区里没有安装 `west` 命令（终端里运行会报 `command not found`），所以我在文档里给的 `west build` 命令是标准 Zephyr 用法，你在有 Zephyr SDK/环境的机器上执行即可。

## 0. Zephyr 为什么能看到这个 board

关键在 [zephyr/module.yml](/home/hjp/zephyrproject/app/example-application/zephyr/module.yml)：

- `board_root: .`
  Zephyr 会把“本仓库根目录”当成一个额外的 board root，从而扫描 `./boards/**`。
- `dts_root: .`
  Zephyr 也会把 `./dts/**` 加入 DTS 搜索路径（如果你后续要自定义 bindings 或 SoC 片段会用到）。

因此，只要你的板卡目录位于：

`boards/vendor/<board_name>/`

就会进入 Zephyr 的 board 发现流程。

## 1. 命名约定：board name 与 qualifier

### 1.1 board name

本例的 board name 是 `ats_stm32h745`，对应目录：

`boards/vendor/ats_stm32h745/`

### 1.2 为什么会出现 `.../stm32h745xx/m7` 和 `.../stm32h745xx/m4`

STM32H745 是双核：

- Cortex-M7：通常跑“主应用”（更多 RAM/更高主频/更多外设归属）
- Cortex-M4：通常跑协处理任务或另外一个 Zephyr image

在 Zephyr 里，一个“板卡”可以有多个 build target（也叫 board variant/board revision/qualifier）。
STM32H745 的常见做法是用 qualifier 表示“同一块板 + 同一 SoC + 不同 CPU core”：

- `ats_stm32h745/stm32h745xx/m7`
- `ats_stm32h745/stm32h745xx/m4`

这两个 target 的差异主要体现在：

- DTS：`chosen { zephyr,flash = &flash0|&flash1; zephyr,sram = &sram0|&sram1; }`
- defconfig：每个核的默认 Kconfig 选择不同
- runner：OpenOCD target-handle 指向 cpu0(cpu M7) 或 cpu1(cpu M4)

### 1.3 “IIT6” 与 `stm32h745Xi_m7.dtsi` 的关系（是否需要改设计）

你可能会疑惑：MCU 料号是 `STM32H745IIT6`，为什么板级 DTS 里 include 的是：

- `<st/h7/stm32h745Xi_m7.dtsi>`
- `<st/h7/stm32h745Xi_m4.dtsi>`

结论：通常不需要为了 “IIT6” 另起一套 SoC `.dtsi`，当前设计逻辑是合理的。

原因（结合你当前 Zephyr 源码树的实际情况）：

- 在 `/home/hjp/zephyrproject/zephyr/dts/arm/st/h7/` 目录下，STM32H745 只有这三份 SoC 级 dtsi：
  - `stm32h745.dtsi`：双核 SoC 的公共描述（flash-controller、sram0/1/2/3/4 等）
  - `stm32h745Xi_m7.dtsi`：面向 M7 的 SoC “视图”（删除 cpu@1、删除 flash1，并把 flash0 size 设置为 1024KB 等）
  - `stm32h745Xi_m4.dtsi`：面向 M4 的 SoC “视图”（删除 cpu@0、删除 flash0/itcm/dtcm，并把 flash1 size 设置为 1024KB 等）
- Zephyr 往往不会为每一个具体料号（如 IIT6）都提供独立的 SoC dtsi，而是按“系列/容量档/核”提供通用描述，再由板级 DTS 做最终选择与覆盖。
- 对 STM32H745IIT6 来说，2MB Flash 通常以两个 1MB bank 的形式存在，因此 SoC dtsi 里对 `flash0/flash1` 的 1024KB 配置在语义上是合理的。

什么时候才需要你自己“新增 SoC 变体 dtsi”或在板级覆盖？

- 你发现 SoC dtsi 里的 flash/sram 容量与你实际芯片不一致
- 你把两核镜像放到不同的 bank/分区策略（例如只用 bank1，bank2 做数据区）
- 你需要把一组 SoC 覆盖（不只一个节点）复用到多个板卡

实践建议：

- 差异点少：优先在板级 `.dts` 里覆盖少量节点属性（例如调整 `flash0`/`flash1` 的 `reg` size）。
- 差异点多且要复用：再新增一个板级 SoC 变体 `.dtsi`（例如 `ats_stm32h745-socfix.dtsi`），专门做这些覆盖，然后在 m7/m4 顶层 DTS 里 include 它。

## 2. 目录结构一览（你要改什么）

本例目录（最小可用骨架）：

- `board.yml`
  Board 元数据入口（board name、vendor、关联 SoC）。
- `Kconfig.ats_stm32h745`
  定义 `CONFIG_BOARD_ATS_STM32H745`，并按 m7/m4 target 选择对应 SoC。
- `Kconfig.defconfig`
  放板级“全局默认项”（可为空，后面按需加）。
- `board.cmake`
  runner（烧录/调试）参数，尤其是双核的 cpu0/cpu1 选择。
- `ats_stm32h745.dtsi`
  板级公共 DTS（两核共享，放 LED、按键、mailbox、电源、公共时钟分频等）。
- `ats_stm32h745_stm32h745xx_m7.dts`
  M7 核的顶层 DTS（console/clock/外设归属等）。
- `ats_stm32h745_stm32h745xx_m4.dts`
  M4 核的顶层 DTS。
- `ats_stm32h745_stm32h745xx_m7_defconfig`
  M7 的默认配置。
- `ats_stm32h745_stm32h745xx_m4_defconfig`
  M4 的默认配置。
- `ats_stm32h745_stm32h745xx_m7.yaml`、`..._m4.yaml`
  Board 描述（identifier、RAM/Flash metadata、supported 外设列表）。
- `doc/index.rst`
  Zephyr board 文档入口（Sphinx 用）。
- `support/openocd.cfg`
  OpenOCD 覆盖（可先保持最小占位）。

## 2.1 逐文件讲解（建议你按这个顺序阅读）

这一节把 `boards/vendor/ats_stm32h745/` 目录下每个文件的职责、和其它文件的关系、以及你上电调试时最常改的点写清楚。

### board.yml（已经完成理解）

文件：[board.yml](/home/hjp/zephyrproject/app/example-application/boards/vendor/ats_stm32h745/board.yml)

用途：
Zephyr 的 board 元数据入口。它告诉 Zephyr：

- 这块板叫啥（`board.name`）
- vendor 是谁（`board.vendor`）
- 这个 board 关联哪些 SoC family（`board.socs`）

你通常要改什么：

- `name`：建议保持目录名一致，例如 `ats_stm32h745`
- `full_name`：给人看的全称
- `vendor`：字符串即可，但建议稳定（以后 CI/文档会用到）
- `socs`：对 STM32H745 这类双核，通常仍然是 `stm32h745xx`

### Kconfig.ats_stm32h745（已经完成理解）

文件：[Kconfig.ats_stm32h745](/home/hjp/zephyrproject/app/example-application/boards/vendor/ats_stm32h745/Kconfig.ats_stm32h745)

用途：
定义板级 Kconfig 符号 `CONFIG_BOARD_ATS_STM32H745`，并根据你构建的是 M7 还是 M4 target 去选择不同的 SoC 变体：

- M7 target 选择 `SOC_STM32H745XX_M7`
- M4 target 选择 `SOC_STM32H745XX_M4`

这一步非常关键，因为它决定了 Zephyr 的 SoC 级启动代码、链接脚本、外设定义等走哪个核的路径。

你通常要改什么：

- 只需要改符号名和注释（如果你换 board 名）
- 逻辑上一般不要动（除非你在做非标准的单核/自定义 SoC 变体）

### Kconfig.defconfig（已经完成理解）

文件：[Kconfig.defconfig](/home/hjp/zephyrproject/app/example-application/boards/vendor/ats_stm32h745/Kconfig.defconfig)

用途：
放“板卡全局默认 Kconfig”。它的特点是：

- 通常对 M7 和 M4 都适用（因为 `if BOARD_ATS_STM32H745`）
- 适合放板级身份相关的默认值（例如某个板默认带以太网，默认选择某个 L2）

你通常要改什么：

- 初期上电调试可以为空
- 等你确认硬件稳定后，再逐步把“确实是板级默认”的配置放进来

### ats_stm32h745.dtsi（已经完成理解）

文件：[ats_stm32h745.dtsi](/home/hjp/zephyrproject/app/example-application/boards/vendor/ats_stm32h745/ats_stm32h745.dtsi)

用途：
板级公共 DTS 片段，两核共享。这里适合放：

- LED/按键/蜂鸣器等板级资源
- 板载外设（I2C/SPI 设备）以及它们的 GPIO 连接
- mailbox 之类双核共享资源
- 一些公共的 RCC prescaler、电源配置

它同时负责 include “pinctrl 定义文件”。你现在看到的：

`#include <st/h7/stm32h745iitx-pinctrl.dtsi>`

这里的 `<...>` include 文件并不在本仓库里，而来自：

`/home/hjp/zephyrproject/modules/hal/stm32/dts/st/h7/stm32h745iitx-pinctrl.dtsi`

也就是说：board DTS 里写 `<st/h7/...>`，最终会由 Zephyr 的 DTS include path 去 module 里展开。

你通常要改什么：

- 选择正确的 pinctrl include（必须与你芯片封装匹配）
- 把你板子的 LED、按键等添加进来（初期可以先不加）

### ats_stm32h745_stm32h745xx_m7.dts（已经完成理解）

文件：[ats_stm32h745_stm32h745xx_m7.dts](/home/hjp/zephyrproject/app/example-application/boards/vendor/ats_stm32h745/ats_stm32h745_stm32h745xx_m7.dts)

用途：
M7 核的顶层 DTS。它做三件最关键的事：

1. include SoC M7 核的基础描述：`#include <st/h7/stm32h745Xi_m7.dtsi>`
2. include 板级公共描述：`#include "ats_stm32h745.dtsi"`
3. 用 `chosen {}` 指定 M7 的资源归属与系统关键句柄

本例里你最关心的是 console：

- `zephyr,console = &uart7;`
- `zephyr,shell-uart = &uart7;`

以及 HSE/PLL/RCC 的时钟树配置（25MHz HSE 基线）。

你通常要改什么（上电调试顺序建议）：

- 先改 `&uart7` 的 `pinctrl-0 = <...>`，保证和你板子 UART7 真实引脚一致
- 再确认 `chosen` 的 `zephyr,sram`、`zephyr,flash` 选择符合你的分配策略
- 最后再调整 PLL（不稳定时钟会让串口、以太网等外设表现得像“随机坏”）

### ats_stm32h745_stm32h745xx_m4.dts（已经完成理解）

文件：[ats_stm32h745_stm32h745xx_m4.dts](/home/hjp/zephyrproject/app/example-application/boards/vendor/ats_stm32h745/ats_stm32h745_stm32h745xx_m4.dts)

用途：
M4 核的顶层 DTS。组织方式与 M7 类似，只是 include 的 SoC 文件不同：

`#include <st/h7/stm32h745Xi_m4.dtsi>`

并且通常把 `chosen` 里的 `zephyr,sram` / `zephyr,flash` 指向 M4 侧的资源（例如 `&sram1` / `&flash1`）。

特别注意：
真实硬件上不建议 M7/M4 共享同一个 UART 作为 console，否则会产生外设/引脚冲突与输出互相干扰。
本例当前的默认配置是：M7 使用 UART7 作为 console，M4 默认不启用串口/console。

### ats_stm32h745_stm32h745xx_m7_defconfig（已经完成理解）

文件：[ats_stm32h745_stm32h745xx_m7_defconfig](/home/hjp/zephyrproject/app/example-application/boards/vendor/ats_stm32h745/ats_stm32h745_stm32h745xx_m7_defconfig)

用途：
M7 target 的默认 Kconfig 配置。典型用法是“把 M7 默认需要的能力打开”，例如：

- `CONFIG_SERIAL=y`
- `CONFIG_CONSOLE=y`
- `CONFIG_UART_CONSOLE=y`
- `CONFIG_ARM_MPU=y`

你通常要改什么：

- 上电调试初期建议保持很少的配置，只开 console/gpio 之类
- 等外设加进 DTS 后，再启用对应驱动相关的 Kconfig（尽量避免一次开太多导致排错困难）

### ats_stm32h745_stm32h745xx_m4_defconfig（已经完成理解）

文件：[ats_stm32h745_stm32h745xx_m4_defconfig](/home/hjp/zephyrproject/app/example-application/boards/vendor/ats_stm32h745/ats_stm32h745_stm32h745xx_m4_defconfig)

用途：
M4 target 的默认 Kconfig 配置。通常比 M7 更“精简”，因为很多板子把复杂外设留给 M7。

你通常要改什么：

- 如果最终产品里 M4 不需要串口/console，可以保持 `CONFIG_SERIAL/CONFIG_CONSOLE/CONFIG_UART_CONSOLE` 关闭

### ats_stm32h745_stm32h745xx_m7.yaml / ats_stm32h745_stm32h745xx_m4.yaml （已经完成理解）

文件：
[ats_stm32h745_stm32h745xx_m7.yaml](/home/hjp/zephyrproject/app/example-application/boards/vendor/ats_stm32h745/ats_stm32h745_stm32h745xx_m7.yaml)
[ats_stm32h745_stm32h745xx_m4.yaml](/home/hjp/zephyrproject/app/example-application/boards/vendor/ats_stm32h745/ats_stm32h745_stm32h745xx_m4.yaml)

用途：
board 描述文件，核心字段是 `identifier`，决定了你 `west build -b` 时应该怎么写：

- `identifier: ats_stm32h745/stm32h745xx/m7`
- `identifier: ats_stm32h745/stm32h745xx/m4`

其它字段（`ram`/`flash`/`supported`）是元数据，主要用于 tooling、测试筛选、文档展示。

你通常要改什么：

- `ram`/`flash`：建议改成你的实际容量（避免误导后续使用者）
- `supported`：按你板子真实带的外设逐步补齐

补充：`type`/`arch` 有哪些常见选项？

这些值本质上也是元数据，最靠谱的方式是“以你当前 Zephyr 源码树中 `boards/*.yaml` 实际使用过的取值为准”。你可以用下面两条命令直接统计当前版本的唯一取值：

```sh
rg -n '^type: ' /home/hjp/zephyrproject/zephyr/boards -S --no-filename | cut -d: -f2- | sed 's/^type: //' | sort -u
rg -n '^arch: ' /home/hjp/zephyrproject/zephyr/boards -S --no-filename | cut -d: -f2- | sed 's/^arch: //' | sort -u
```

在你当前这棵 Zephyr 树里，常见情况是：

- `type` 常见取值：`mcu`、`qemu`、`sim`、`native`
- `arch` 常见取值：`arm`、`arm64`、`riscv`、`x86`、`xtensa`、`arc`、`mips`、`openrisc`、`rx`、`sparc`、`posix`

### board.cmake （已经完成理解）

文件：[board.cmake](/home/hjp/zephyrproject/app/example-application/boards/vendor/ats_stm32h745/board.cmake)

用途：
配置 `west flash/debug/attach` 的 runner 参数（OpenOCD/J-Link/STM32CubeProgrammer）。

双核最关键的逻辑是：

- M7：OpenOCD 选择 `cpu0`
- M4：OpenOCD 选择 `cpu1`

你通常要改什么：

- J-Link 的 `--device=` 字符串（不同版本的 J-Link 设备库可能需要不同名称）
- 如果你有自定义的 OpenOCD interface/target 脚本，通常在 `support/openocd.cfg` 里补

### support/openocd.cfg

文件：[openocd.cfg](/home/hjp/zephyrproject/app/example-application/boards/vendor/ats_stm32h745/support/openocd.cfg)

用途：
给 OpenOCD 提供 board 级覆盖配置（例如 reset 方式、adapter speed、特殊 flash bank、额外脚本等）。

本例目前是最小占位，后续你如果遇到 OpenOCD 连接/复位问题，这里就是你加东西的地方。

### doc/index.rst

文件：[index.rst](/home/hjp/zephyrproject/app/example-application/boards/vendor/ats_stm32h745/doc/index.rst)

用途：
Zephyr board 文档入口（Sphinx）。它不是编译必须，但对团队协作非常重要。

建议你在这里补充：

- UART7 接哪组引脚、默认波特率
- HSE 频率、PLL 目标频率
- 如何分别构建 M7/M4（已经写了示例）

## 3. 构建命令（你真正会用的那两条）

这一节专门说明：`identifier` 和你实际 `west build -b ...` 之间的对应关系。

在本板卡目录下有两份 `*.yaml` 描述文件：

- M7：[ats_stm32h745_stm32h745xx_m7.yaml](/home/hjp/zephyrproject/app/example-application/boards/vendor/ats_stm32h745/ats_stm32h745_stm32h745xx_m7.yaml)
  其中 `identifier: ats_stm32h745/stm32h745xx/m7`
- M4：[ats_stm32h745_stm32h745xx_m4.yaml](/home/hjp/zephyrproject/app/example-application/boards/vendor/ats_stm32h745/ats_stm32h745_stm32h745xx_m4.yaml)
  其中 `identifier: ats_stm32h745/stm32h745xx/m4`

因此构建时 `-b` 就直接使用对应的 `identifier` 字符串。

M7：

```sh
west build -b ats_stm32h745/stm32h745xx/m7 -p auto app
```

M4：

```sh
west build -b ats_stm32h745/stm32h745xx/m4 -p auto app
```

如果你想把 build 输出放到单独目录（推荐）：

```sh
west build -b ats_stm32h745/stm32h745xx/m7 -d build_m7 -p auto app
west build -b ats_stm32h745/stm32h745xx/m4 -d build_m4 -p auto app
```

## 4. console = UART7 要写在哪里

Zephyr 的 console/shell 选择通常在 DTS 的 `chosen {}` 中指定：

- 在 [ats_stm32h745_stm32h745xx_m7.dts](/home/hjp/zephyrproject/app/example-application/boards/vendor/ats_stm32h745/ats_stm32h745_stm32h745xx_m7.dts)：
  `zephyr,console = &uart7; zephyr,shell-uart = &uart7;`
- 在 [ats_stm32h745_stm32h745xx_m4.dts](/home/hjp/zephyrproject/app/example-application/boards/vendor/ats_stm32h745/ats_stm32h745_stm32h745xx_m4.dts)：
  同样指向 `&uart7`

同时还要确保 `&uart7 { status = "okay"; ... }` 并正确配置 pinmux（pinctrl）。

## 5. HSE = 25MHz 与 PLL 要写在哪里

常见做法：

1. 使能 `&clk_hse`，并设置 `clock-frequency = <DT_FREQ_M(25)>;`
2. 配置 `&pll`（div/mul/dividers）
3. 在 `&rcc` 中选择 `clocks = <&pll>;` 并给出期望系统频率（例如 `480MHz`）

本例把这些都写在 M7 的 DTS 中（参考 disco 的 25MHz baseline），位置见：
[ats_stm32h745_stm32h745xx_m7.dts](/home/hjp/zephyrproject/app/example-application/boards/vendor/ats_stm32h745/ats_stm32h745_stm32h745xx_m7.dts)。

经验建议：

- 初次上电调试先用“参考板卡能跑的 PLL 配置”，确认串口输出稳定后，再按你的功耗/外设需求优化。
- 如果你只想最小化风险，可以先不改 PLL（或者把复杂外设时钟先关掉），先把 console 打通。

## 6. 最容易踩坑的点（强烈建议你逐个对照）

### 6.0 STM32H745IIT6 需要“重做 pinctrl 映射”吗

一般不需要。

在 Zephyr 里，STM32 的 pinmux/pinctrl 大多不是“每块板都从 0 写一套”，而是来自 SoC/系列的 pinctrl `.dtsi`（由 Zephyr/`hal_stm32` 提供）。你创建 board 时通常只做两件事：

1. 选择正确的 pinctrl include（必须匹配你的芯片系列与封装变体）
2. 在板级 DTS 里引用正确的 pinctrl phandle（例如 `&uart7_tx_*`、`&uart7_rx_*`）

对比参考板卡就能看到原因（这些 pinctrl 文件通常来自 `hal_stm32` 模块，不一定在 `zephyr/` 主仓库里能直接看到）：

- `nucleo_h745zi_q` include 的是 `stm32h745zitx-pinctrl.dtsi`（`ZI` + 特定封装）
- `stm32h745i_disco` include 的是 `stm32h745xihx-pinctrl.dtsi`（`XI` + 特定封装）

你的 MCU 是 `STM32H745IIT6`，在你当前 west 工作区的 `hal_stm32` 里存在一个更贴近命名的 pinctrl 文件：

- `/home/hjp/zephyrproject/modules/hal/stm32/dts/st/h7/stm32h745iitx-pinctrl.dtsi`

所以本例已经改为 include：

- `<st/h7/stm32h745iitx-pinctrl.dtsi>`（见 [ats_stm32h745.dtsi](/home/hjp/zephyrproject/app/example-application/boards/vendor/ats_stm32h745/ats_stm32h745.dtsi)）

什么时候才需要“自己设计/补充” pinctrl 映射？

- 你确定你的封装/变体和 `xihx` 不一致，并且 Zephyr 已经提供了更匹配的 pinctrl include：这时应优先切换 include。
- 你使用了某组引脚，但 SoC pinctrl include 里没有对应的 `uart7_tx_*`/`uart7_rx_*`（或其它外设的 pinctrl 组）：这时才需要新增一个板级 `*-pinctrl.dtsi`，只补齐你用到的那几组 pin 配置即可。

### 6.1 UART7 的 pinctrl 符号是否存在

在 DTS 里用了类似 `&uart7_tx_pf7` 这种 pinctrl phandle，它们来自你 include 的 pinctrl `.dtsi`（本例在公共文件里 include 了 `stm32h745iitx-pinctrl.dtsi`）。

如果你实际硬件用了别的引脚（例如 PBxx/PDxx），你需要：

- 把 `pinctrl-0 = <...>` 换成对应的 `uart7_tx_*`、`uart7_rx_*`。
- 如果对应符号在 pinctrl include 里不存在，就需要新增/调整 pinctrl 定义（通常放到板级 `.dtsi` 或单独 `*-pinctrl.dtsi`）。

### 6.2 chosen 里的 flash/sram 归属

双核时，常见约定：

- M7：`zephyr,sram = &sram0; zephyr,flash = &flash0;`
- M4：`zephyr,sram = &sram1; zephyr,flash = &flash1;`

如果你把两核都指向 `flash0` 或 `sram0`，可能会导致链接/运行冲突（尤其是你后面要做多镜像、共享内存时）。

### 6.3 `*.yaml` 的 RAM/Flash 只是元数据，但最好别乱填

`ats_stm32h745_stm32h745xx_m7.yaml` / `_m4.yaml` 里的 `ram:`/`flash:` 主要用于元数据（文档、测试筛选等），不一定参与链接脚本。
但如果你填得离谱，后续别人看 board capability 会被误导。

你可以先用一个“接近”的值，后面再按 `STM32H745IIT6` 的实际存储容量更新。

## 7. 下一步你可以怎么练习（建议路线）

1. 先只保证 M7 build 通过，并在 UART7 上有 `Hello World` 输出。
2. 再把 M4 也 build 通过（默认不启用串口/console），后续如需 M4 输出日志，建议走核间转发或使用另一组 UART。
3. 开始把你板子的 LED、按键、以太网、QSPI 等外设逐个加到 `.dtsi`，每加一个就写一个最小 sample 验证。
