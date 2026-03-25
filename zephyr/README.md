# zephyr 目录学习文档（module 元数据）

注意：这个仓库里的 `zephyr/` 目录不是 Zephyr 主仓库源码，而是本仓库作为 Zephyr module 的入口元数据目录。

## 1. module.yml

文件：[module.yml](/home/hjp/zephyrproject/app/example-application/zephyr/module.yml)

它定义了：

- `build.kconfig`：本仓库的 Kconfig 入口
- `build.cmake`：本仓库的 CMake 入口
- `settings.board_root`：本仓库板卡根目录（使 `boards/` 生效）
- `settings.dts_root`：本仓库 DTS 根目录（使 `dts/bindings` 生效）
- `runners`：额外 runner 扩展

你在做“板卡/驱动/绑定”这类扩展时，通常都绕不开这个文件。

