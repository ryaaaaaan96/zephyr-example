# doc 目录学习文档（文档构建）

`doc/` 用于构建本仓库的文档（Sphinx + Doxygen）。

## 1. 目录内关键文件

- [index.rst](/home/hjp/zephyrproject/app/example-application/doc/index.rst)
  Sphinx 文档入口。
- [conf.py](/home/hjp/zephyrproject/app/example-application/doc/conf.py)
  Sphinx 配置。
- [requirements.txt](/home/hjp/zephyrproject/app/example-application/doc/requirements.txt)
  文档构建依赖（Python 包）。
- [Doxyfile](/home/hjp/zephyrproject/app/example-application/doc/Doxyfile)
  Doxygen 配置。
- [_doxygen](/home/hjp/zephyrproject/app/example-application/doc/_doxygen)
  Doxygen 输入文件（groups、main 等）。

## 2. 与板卡文档的关系

板卡文档通常放在板卡目录的 `doc/index.rst` 里，例如：

- [index.rst](/home/hjp/zephyrproject/app/example-application/boards/vendor/ats_stm32h745/doc/index.rst)

当 Zephyr 扫描 board 文档时，会把这些内容合入整体文档体系（具体取决于你使用的文档构建方式与配置）。

