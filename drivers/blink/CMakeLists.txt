# Copyright (c) 2024 Nordic Semiconductor ASA
# SPDX-License-Identifier: Apache-2.0

zephyr_library()

# zephyr_library_sources_ifdef(<Kconfig符号> <源文件...>)
# - ifdef 的意思是：当且仅当该 Kconfig 符号为 y 时，才把这些源文件加入构建
# - 这里的 CONFIG_BLINK_GPIO_LED 来自 drivers/blink/Kconfig.gpio_led
# - 作用：把 gpio_led.c 这个“GPIO LED 版本的 blink 驱动实现”做成可选项
#   没启用该配置时，此文件不会被编译进最终固件里
zephyr_library_sources_ifdef(CONFIG_BLINK_GPIO_LED gpio_led.c)
