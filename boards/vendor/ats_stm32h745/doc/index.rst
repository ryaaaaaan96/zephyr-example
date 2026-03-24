.. zephyr:board:: ats_stm32h745

ATS STM32H745IIT6 板卡
######################

概述
****

这是一个自定义 STM32H745（双核 Cortex-M7 + Cortex-M4）板卡。

构建
****

构建 M7 镜像：

.. code-block:: console

   west build -b ats_stm32h745/stm32h745xx/m7 -p auto app

构建 M4 镜像：

.. code-block:: console

   west build -b ats_stm32h745/stm32h745xx/m4 -p auto app

Console 串口
***********

默认 console 使用 UART7（M7 侧）。如果你的硬件实际走线使用的是不同引脚，请在 DTS 中更新 pinmux（pinctrl）。

默认情况下 M4 侧不启用串口/console（避免与 M7 共享同一 UART 产生冲突）。
