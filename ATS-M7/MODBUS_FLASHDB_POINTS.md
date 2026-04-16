# ATS-M7 Modbus 点位与 FlashDB 持久化

## 说明

- Modbus 使用 `RTU Server`，`unit_id=1`，串口为 `USART2`，DE 引脚为 `PD3`。  
- 已接入第三方开源 **FlashDB 官方库**（`lib/FlashDB`）并启用 `KVDB`。  
- 底层存储分区为 QSPI 的 `flashdb_partition`（标签 `flashdb`，前半区）。  
- Holding Register 和 Coil 都做了持久化。  
- 读取时通过 FlashDB `KVDB` 读取当前值。  
- 写入时通过 FlashDB `KVDB` 更新并落盘。  

## Holding Register（可读可写）

| 地址 | 名称 | 默认值 | 说明 |
|---|---|---:|---|
| `0` | `device_id` | `1` | 设备参数示例 |
| `1` | `threshold` | `100` | 阈值参数 |
| `2` | `retry_ms` | `500` | 重试时间（毫秒） |
| `3` | `user_word` | `0x1234` | 用户自定义字 |

对应 FlashDB KV 键：
- 地址 `0` -> `hr0`
- 地址 `1` -> `hr1`
- 地址 `2` -> `hr2`
- 地址 `3` -> `hr3`

常用功能码：
- 读：`FC03`
- 写单个：`FC06`
- 写多个：`FC16`

## Coil（可读可写）

| 地址 | 名称 | 默认值 | 说明 |
|---|---|---:|---|
| `0` | `enable` | `1` | 功能使能 |
| `1` | `save_en` | `0` | 保存开关示例 |

对应 FlashDB KV 键：
- 地址 `0` -> `coil0`
- 地址 `1` -> `coil1`

常用功能码：
- 读：`FC01`
- 写单个：`FC05`
- 写多个：`FC15`

## Input Register（只读）

| 地址 | 名称 | 说明 |
|---|---|---|
| `0` | `heartbeat` | 每秒自增一次，用于在线观察 |

常用功能码：
- 读：`FC04`
