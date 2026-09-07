# 东软电动汽车充电桩应用管理平台

本仓库为课程小项目代码，包含用户端 `ChargingUserClient`、服务端 `EVChargingServer`、需求/说明文档以及自动验证脚本。

## 项目结构

| 路径 | 说明 |
| --- | --- |
| `ChargingUserClient/` | Qt 6 用户端，包含登录注册、个人资料、钱包流水、附近充电站、电桩详情、充电流程等功能 |
| `EVChargingServer/` | Qt 6 服务端，监听 `8888` 端口，为用户端提供注册、登录、钱包、电站、电桩和充电订单接口 |
| `用户端技术点总结.md` | 从需求矩阵出发整理的用户端技术实现说明 |
| `验证说明.txt` | 自动验证脚本使用说明 |
| `validate_all.ps1` | Windows 一键验证入口 |
| `guest_validate_charging.sh` | Ubuntu 虚拟机内编译和验证脚本 |
| `socket_smoke.cpp` / `socket_smoke.pro` | 用户端到服务端的 Socket 冒烟测试程序 |

## 开发环境

- Ubuntu 22.04
- Qt 6
- qmake6 + make
- C++17
- SQLite
- Qt Network

## 用户端测试账号

```text
手机号：13800138000
密码：Demo@123
```

## 编译用户端

```bash
cd ChargingUserClient
qmake6 ChargingUserClient.pro
make
./ChargingUserClient
```

## 编译服务端

```bash
cd EVChargingServer
qmake6 EVChargingServer.pro
make
./EVChargingServer
```

服务端默认监听端口为 `8888`。

## 一键验证

在 Windows PowerShell 中进入项目根目录后执行：

```powershell
.\validate_all.ps1
```

验证内容包括：

- 编译服务端
- 编译用户端
- 启动或复用服务端 `8888` 监听
- 编译并运行 `socket_smoke`
- 验证用户端能通过 Socket 完成登录、余额查询、站点查询和桩位查询

成功时可看到：

```text
VALIDATION_OK
SOCKET_OK userId=1 nickname=张伟 balance=120.50 stations=3 firstStation=朝阳公园充电站 piles=10 records=0
```
