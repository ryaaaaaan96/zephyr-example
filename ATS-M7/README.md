# app 目录学习文档（应用与 Sample）

`app/` 是本仓库提供的 Zephyr 应用(sample)目录。你最终执行 `west build ... app` 时，这里的 `CMakeLists.txt` 会作为应用入口被 Zephyr 构建系统加载。

## 1. 目录内常见文件与作用

- [CMakeLists.txt](/home/hjp/zephyrproject/app/example-application/app/CMakeLists.txt)
  应用 CMake 入口，定义应用的源文件、包含路径等。
- [Kconfig](/home/hjp/zephyrproject/app/example-application/app/Kconfig)
  应用级 Kconfig 入口（定义本应用自身的配置项）。
- [prj.conf](/home/hjp/zephyrproject/app/example-application/app/prj.conf)
  应用默认配置（Kconfig 赋值）。你大多数时候会从这里开始开关功能。
- [src/main.c](/home/hjp/zephyrproject/app/example-application/app/src/main.c)
  应用主入口代码。
- [sample.yaml](/home/hjp/zephyrproject/app/example-application/app/sample.yaml)
  Sample 元数据（用于测试/展示/CI）。
- [debug.conf](/home/hjp/zephyrproject/app/example-application/app/debug.conf)
  常用于 debug 构建的额外配置（例如打开更多日志、断言等）。
- [boards](/home/hjp/zephyrproject/app/example-application/app/boards)
  放与“某个板卡运行该应用”有关的 overlay（例如 `*.overlay`）。
- [VERSION](/home/hjp/zephyrproject/app/example-application/app/VERSION)
  应用版本信息（在部分 Zephyr 流程中会被读取）。

## 2. 如何构建

构建命令由板卡 `identifier` 决定，例如（以你自定义板卡为例）：

```sh
west build -b ats_stm32h745/stm32h745xx/m7 -d build_m7 -p auto app
west build -b ats_stm32h745/stm32h745xx/m4 -d build_m4 -p auto app
```

## 3. 常见扩展点

- 想加应用配置项：改 [Kconfig](/home/hjp/zephyrproject/app/example-application/app/Kconfig) 并在 [prj.conf](/home/hjp/zephyrproject/app/example-application/app/prj.conf) 里设置默认值。
- 想做板级差异：在 [boards](/home/hjp/zephyrproject/app/example-application/app/boards) 放对应的 `*.overlay`。

## 4. QSPI Flash 前期验证（读/写/擦）

- 设备树里已经有 `qspi_flash` 节点（`qspi-nor-flash@0`），应用侧用 `DT_NODELABEL(qspi_flash)` 直接拿设备。
- 验证代码在 `ATS-M7/src/main.c` 的 `qspi_flash_smoke_test()`：会对 `QSPI_TEST_OFFSET` 所在的一个 erase block 做 `erase -> write -> read` 校验（注意会破坏该偏移处的数据）。
- 后续上 littlefs 的时候，建议再加 `fixed-partitions` + `zephyr,storage`，把文件系统固定在一个分区里，避免误擦到别的区域。
