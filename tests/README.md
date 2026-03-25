# tests 目录学习文档（测试）

`tests/` 用于放本仓库的测试用例（通常与 Zephyr 的 twister 测试框架兼容）。

## 1. 当前目录内容

示例测试：

- [tests/lib/custom](/home/hjp/zephyrproject/app/example-application/tests/lib/custom)

其中包含：

- [CMakeLists.txt](/home/hjp/zephyrproject/app/example-application/tests/lib/custom/CMakeLists.txt)
- [prj.conf](/home/hjp/zephyrproject/app/example-application/tests/lib/custom/prj.conf)
- [testcase.yaml](/home/hjp/zephyrproject/app/example-application/tests/lib/custom/testcase.yaml)

## 2. 如何新增一个测试

1. 新建目录 `tests/<area>/<name>/`
2. 写 `testcase.yaml` 描述测试（平台、标签、额外配置等）
3. 写 `prj.conf`（如果需要）
4. 写测试源码（通常使用 Zephyr 的 ztest）

