# lib 目录学习文档（本仓库自定义库）

`lib/` 用于放本仓库自定义的库代码（非驱动类）。

## 1. 入口文件

- [CMakeLists.txt](/home/hjp/zephyrproject/app/example-application/lib/CMakeLists.txt)
  决定哪些库子目录加入构建。
- [Kconfig](/home/hjp/zephyrproject/app/example-application/lib/Kconfig)
  决定哪些库配置项出现在 Kconfig 菜单中。

## 2. 示例：custom 库

- 代码：[custom.c](/home/hjp/zephyrproject/app/example-application/lib/custom/custom.c)
- 公共头文件：[custom.h](/home/hjp/zephyrproject/app/example-application/include/app/lib/custom.h)
- Kconfig：[Kconfig](/home/hjp/zephyrproject/app/example-application/lib/custom/Kconfig)
- CMake：[CMakeLists.txt](/home/hjp/zephyrproject/app/example-application/lib/custom/CMakeLists.txt)

## 3. 如何新增一个库

1. 建目录：`lib/<your_lib>/`
2. 写 `lib/<your_lib>/CMakeLists.txt`、`lib/<your_lib>/Kconfig`、源文件
3. 在 [lib/CMakeLists.txt](/home/hjp/zephyrproject/app/example-application/lib/CMakeLists.txt) 和 [lib/Kconfig](/home/hjp/zephyrproject/app/example-application/lib/Kconfig) 中挂入新子目录
4. 如需对外提供 API，把头文件放到 `include/` 并在代码里 include 使用

