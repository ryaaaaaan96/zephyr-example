# scripts 目录学习文档（west 命令与 runner 扩展）

`scripts/` 用于放本仓库对 `west` 的扩展命令，以及 runner 示例。

## 1. 与 west 的关系

本仓库的 [west.yml](/home/hjp/zephyrproject/app/example-application/west.yml) 配置了：

- `west-commands: scripts/west-commands.yml`

因此 `west` 会加载这里定义的扩展命令。

## 2. 当前目录内容

- [west-commands.yml](/home/hjp/zephyrproject/app/example-application/scripts/west-commands.yml)
  west 扩展命令清单入口。
- [example_west_command.py](/home/hjp/zephyrproject/app/example-application/scripts/example_west_command.py)
  west 扩展命令示例实现。
- [example_runner.py](/home/hjp/zephyrproject/app/example-application/scripts/example_runner.py)
  runner 示例实现（与烧录/调试流程相关）。

## 3. 如何新增一个 west 命令

1. 在 `scripts/` 下新增一个 `*.py`
2. 在 [west-commands.yml](/home/hjp/zephyrproject/app/example-application/scripts/west-commands.yml) 注册该命令
3. 在命令实现中按 Zephyr/west 的方式解析参数并执行逻辑

