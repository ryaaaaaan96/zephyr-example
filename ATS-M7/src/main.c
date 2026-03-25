/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

int main(void)
{
    while (1) {
        k_sleep(K_MSEC(500));
        printk("Hello World\n");
    }
}
