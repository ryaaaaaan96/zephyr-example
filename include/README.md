# include 目录学习文档（公共头文件）

`include/` 用于放本仓库对外暴露的公共头文件。它会被本仓库顶层 [CMakeLists.txt](/home/hjp/zephyrproject/app/example-application/CMakeLists.txt) 通过 `zephyr_include_directories(include)` 加入编译 include path。

## 1. 当前目录内容

- [blink.h](/home/hjp/zephyrproject/app/example-application/include/app/drivers/blink.h)
  自定义 blink 驱动类的公共 API 头文件（含 `__subsystem` 与 `__syscall` 示例）。
- [custom.h](/home/hjp/zephyrproject/app/example-application/include/app/lib/custom.h)
  自定义库 `lib/custom` 的公共 API 头文件。

## 2. 常见扩展点

- 新增一个驱动类的公共 API：建议放到 `include/app/drivers/<name>.h`。
- 新增一个库的公共 API：建议放到 `include/app/lib/<name>.h`。

如果头文件里包含 `__syscall`，需要确保 syscall 生成能扫描到该目录（本仓库已在顶层 CMake 中做了对应设置）。

