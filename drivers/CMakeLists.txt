# Copyright (c) 2021 Nordic Semiconductor ASA
# SPDX-License-Identifier: Apache-2.0

#
# drivers/CMakeLists.txt 的角色（非常重要的理解点）：
#
# - 这个文件不“决定”驱动是否启用，它只是根据 Kconfig 的结果来决定
#   哪些驱动子目录需要参与编译。
# - `CONFIG_...` 变量来自 Kconfig 配置阶段（最终写入构建目录里的 `.config`），
#   CMake 在生成构建系统时会读取这些配置结果，并把它们映射为 CMake 变量。
#
# 常见链路（简化版）：
# 1) 你在 app 的 `prj.conf` / overlay config / board defconfig 里设置或间接选择某个 `CONFIG_xxx`
# 2) Kconfig 解析依赖/默认值，生成最终 `.config`
# 3) CMake 根据 `.config` 生成构建系统，此时 `CONFIG_xxx` 才能用于 `if(...)` / `*_ifdef(...)`
#
# `add_subdirectory_ifdef(<cond> <dir>)` 的含义：
# - 如果 <cond> 为真（例如 `CONFIG_BLINK` 为 y），就把 <dir> 子目录加入 CMake 构建
# - 否则跳过该目录（该目录下的源文件/库不会被编译）
#

# 例子 1：自定义驱动类（Custom driver class）
# `blink/` 演示如何定义自己的 driver class API（见 include/app/drivers/blink.h），
# 并提供一个具体实现（GPIO LED 闪烁驱动）。
add_subdirectory_ifdef(CONFIG_BLINK blink)

# 例子 2：复用 Zephyr 已有驱动类（Existing driver class）
# `sensor/` 演示如何实现 Zephyr 标准的 sensor 驱动类 API（见 zephyr/drivers/sensor.h）。
add_subdirectory_ifdef(CONFIG_SENSOR sensor)
