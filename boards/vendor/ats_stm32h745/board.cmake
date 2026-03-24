# SPDX-License-Identifier: Apache-2.0

# 这个文件用于配置烧录/调试的 “runner”（OpenOCD/J-Link/STM32CubeProgrammer）。
# 它不参与应用编译，但对以下命令非常关键：
# - west flash
# - west debug
# - west attach
#
# 对于双核 STM32H745，最重要的是告诉 OpenOCD 你要控制哪个核：
# - cpu0: Cortex-M7
# - cpu1: Cortex-M4

# keep first：在 include runner .cmake 之前先设置 runner 参数
board_runner_args(stm32cubeprogrammer "--port=swd" "--reset-mode=hw")

# J-Link 的 --device 字符串依赖你安装的 J-Link 设备库/pack 版本。
# 如果 J-Link 提示 "unknown device"，可以用 J-Link Commander 列出支持的设备名后再调整这里。
board_runner_args(jlink "--device=STM32H745II" "--speed=4000")

# 对 OpenOCD，使用 target-handle 确保 'west flash/debug' 连接到双核芯片的正确核。
if(CONFIG_BOARD_ATS_STM32H745_STM32H745XX_M7)
  board_runner_args(openocd --target-handle=_CHIPNAME.cpu0)
elseif(CONFIG_BOARD_ATS_STM32H745_STM32H745XX_M4)
  board_runner_args(openocd --target-handle=_CHIPNAME.cpu1)
endif()

# keep first：引入 Zephyr 的标准 runner 实现
include(${ZEPHYR_BASE}/boards/common/stm32cubeprogrammer.board.cmake)
include(${ZEPHYR_BASE}/boards/common/openocd-stm32.board.cmake)
include(${ZEPHYR_BASE}/boards/common/jlink.board.cmake)
