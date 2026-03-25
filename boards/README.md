# boards 目录学习文档（自定义板卡与 runner）

本仓库作为 Zephyr module，通过 [module.yml](/home/hjp/zephyrproject/app/example-application/zephyr/module.yml) 的 `board_root: .` 把本仓库的 `boards/` 加入 Zephyr 的板卡搜索路径。

因此，`boards/` 的内容会被 `west build -b ...` 发现并使用。

## 1. 目录结构

- [common](/home/hjp/zephyrproject/app/example-application/boards/common)
  放“通用 runner / 通用 CMake 片段”等可复用内容。
- [vendor](/home/hjp/zephyrproject/app/example-application/boards/vendor)
  放实际板卡目录（本仓库目前把自定义板卡也放在 `vendor/` 下统一管理）。

## 2. vendor 目录说明

vendor 目录有更详细的创建指南与逻辑说明：

- [README.md](/home/hjp/zephyrproject/app/example-application/boards/vendor/README.md)

你自定义的 STM32H745 板卡例子在：

- [ats_stm32h745](/home/hjp/zephyrproject/app/example-application/boards/vendor/ats_stm32h745)

## 3. common 目录说明

示例 runner 入口：

- [example_runner.board.cmake](/home/hjp/zephyrproject/app/example-application/boards/common/example_runner.board.cmake)

这类文件通常会被 `board.cmake` include，用于实现 `west flash/debug` 的 runner 行为。

