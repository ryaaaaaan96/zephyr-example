# .github 目录说明（CI 工作流）

`.github/` 主要用于 GitHub Actions CI。

## 1. workflows

- [build.yml](/home/hjp/zephyrproject/app/example-application/.github/workflows/build.yml)
  CI 构建相关工作流。
- [docs.yml](/home/hjp/zephyrproject/app/example-application/.github/workflows/docs.yml)
  文档构建相关工作流。

如果你后续新增板卡/驱动并希望在 CI 中覆盖到，可以从这些 workflow 入手调整矩阵与构建参数。

