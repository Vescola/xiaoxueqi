# 东软电动汽车充电桩应用管理平台 · PC 服务器端

基于 **Qt 5.15.3 (GCC 11.2.0, 64 bit) / C++14** 编写的服务器端完整工程，
适配 **Qt Creator 6.0.2**。本工程同时包含：

1. **面向充电用户端的 Socket 服务**（`QTcpServer` + 单子线程异步 I/O + 二进制协议）；
2. **面向运营管理人员的后台 GUI**（管理员登录 + 5 个只读管理页 + QPainter 手绘折线图）。

---

## 1. 运行环境

| 项目 | 值 |
|---|---|
| 目标系统 | Linux (Ubuntu 22.04+，VMware) |
| IDE | Qt Creator 6.0.2 及以上 |
| Qt | 5.15.3 (GCC 11.2.0, 64 bit) |
| 构建系统 | qmake（`.pro`） |
| 依赖模块 | `Widgets`、`Sql`、`Network` |

> 不依赖 Qt Charts 模块（营收折线图由 `TrendChartWidget` 用 **QPainter 手绘**），
> 可避免 `Unknown module(s) in QT: charts` 报错。

## 2. 构建运行

```bash
qmake EVChargingServer.pro
make -j$(nproc)
./EVChargingServer
```

Qt Creator：打开 `EVChargingServer.pro` → 选 5.15.3 Kit → 构建 → 运行。
首次运行会自动在程序目录生成 `config.ini` 与 `charging.db`（建表 + 演示数据）。

## 3. 默认账号与配置

- 管理员：`admin / 123456`（来源项目说明书）
- 配置：`config.ini`（QSettings 管理，见 `config.ini.example`）

关键配置项：

| 组 | 键 | 说明 |
|---|---|---|
| `[Network]` | `ListenPort` | 监听端口，默认 8888 |
| `[Database]` | `DbPath` | SQLite 库路径，默认 `charging.db` |
| `[Billing]` | `PriceFast` / `PriceSlow` | 快充 / 慢充单价（元/kWh），两档价决策 |
| `[Security]` | `PasswordMode` | `plain`(当前) / `saltedhash`(预留加盐) |

## 4. 运行流程（与《思路.docx》一致）

1. 程序启动 → **后台业务核心（ServerCore）立即进入子线程运行**，监听端口、处理用户端请求；
2. 主界面（LauncherWindow）只显示标题 + "登录"按钮，代表业务常驻；
3. 点"登录" → 弹出管理员登录框 → 校验通过 → 打开独立的管理后台窗口；
4. 关闭管理后台窗口 = 退出登录；主界面不关闭，后台服务持续运行；
5. 只有点"关闭服务器"或关闭主界面，程序才退出。

## 5. 功能清单

### 5.1 用户端 Socket 服务（CS 通信协议 1.0）

| 命令 | 说明 |
|---|---|
| `REGISTER` / `LOGIN` | 注册 + 登录（**密码登录**与**短信验证码/免密登录**双模式；短信模式手机号不存在则自动建号） |
| `SMS_CODE` | 验证码（服务端模拟，打印到日志） |
| `LOGOUT` / `FIND_PWD` / `CHANGE_PWD` | 登出、找回、改密 |
| `RECHARGE` / `BALANCE_QUERY` / `TRANSACTION` | 充值、余额、流水分页 |
| `STATIONS` / `STATION_DETAIL` | 附近站点（Haversine 距离）+ 站点电桩明细 |
| `CHECK_ORDER` / `START_CHARGE` / `STOP_CHARGE` / `CHARGE_STATUS` | 充电全流程（检查/发起/结算/查询） |
| `HEARTBEAT` | 心跳 |

### 5.2 管理员后台（查询 + 统计 + 写操作）

- 销售业绩：今日/本月/总营收 + 近7日/近30日折线图（QPainter）
- 充电站管理：站点列表 + 总桩数/在线率 + 站内电桩明细 + **新增/编辑电站**
- 充电桩管理：状态分布（在用/闲置/故障 数量与占比）+ 电桩列表 + 状态筛选 + **新增/编辑/删除电桩 + 远程重启**
- 用户管理：列表 + 手机号模糊搜索 + **冻结/解冻**
- 订单管理：列表 + 按状态/手机号筛选 + **取消订单**

> 写操作遵循「惰性删除」决策：删除用户=冻结、删除电桩=置离线、删除订单=取消，
> 不物理删除、不加列。充电站删除因外键 RESTRICT 且无标记列，暂不提供。

## 6. 目录结构

```
EVChargingServer/
├── EVChargingServer.pro
├── README.md
├── config.ini.example
└── src/
    ├── main.cpp                 # 入口: 配置->数据库->启动子线程->主界面
    ├── common/
    │   ├── appconfig.{h,cpp}    # ini 配置(QSettings)
    │   ├── passwordutil.{h,cpp} # 口令处理(明文/加盐哈希可切换)
    │   └── protocol.{h,cpp}     # 命令码/错误码/封包解包(粘包处理)
    ├── db/
    │   ├── databasemanager.h     # 数据库端模块接口(同学提供的代码, 已扩展完整)
    │   └── databasemanager.cpp   # 数据访问实现(每线程独立连接)
    ├── net/
    │   ├── sessionmanager.{h,cpp} # token 会话(内存)
    │   ├── clientsession.{h,cpp}  # 单连接 + 粘包缓冲
    │   └── servercore.{h,cpp}     # ★业务核心(子线程 + 协议路由)
    └── ui/
        ├── launcherwindow.{ui,h,cpp}  # 主界面(常驻)
        ├── logindialog.{ui,h,cpp}     # 管理员登录
        ├── mainwindow.{ui,h,cpp}      # 管理后台窗口
        └── pages/
            ├── trendchartwidget.{h,cpp} # QPainter 折线图
            ├── salespage.{ui,h,cpp}
            ├── stationpage.{ui,h,cpp}
            ├── stationeditdialog.{ui,h,cpp}
            ├── pilepage.{ui,h,cpp}
            ├── chargereditdialog.{ui,h,cpp}
            ├── userpage.{ui,h,cpp}
            └── orderpage.{ui,h,cpp}
```

## 7. 与数据库端的关系（重要）

数据库端是服务器端的一个**模块**：同学提供的 `databasemanager.{h,cpp}` 已作为
数据访问层集成进本工程的 `src/db/` 目录，服务器端通过
`DatabaseManager::instance()` 这一内部 API 调用，直接读写 `database.sql`
定义的六张表：

`users` / `admins` / `stations` / `chargers` / `orders` / `wallet_records`

- 保留《数据库端与服务器端调用接口文档 V1.0》的 13 个接口签名不变；
- 为覆盖需求矩阵 / 通信协议的全部数据操作，额外补充了流水查询、营收统计、
  未完成订单、附近站点、订单/电桩/用户列表等接口（见 `databasemanager.h`）；
- 首启时 `ensureSchemaAndSeed()` 会以**与 database.sql 完全一致**的语句建表，
  并灌入演示数据。

### 已修复的数据库端问题

原 `databasemanager.{h,cpp}` 无法编译，已修复：`Money` 类型先用后定义、
`hashPassword` 未定义、9 处 `bool`/`DbErrorCode` 签名不匹配、`changePassword`
查询不存在的 `password_hash` 列等。详见项目内《数据库端能力缺口清单.md》。

## 8. 协议偏差说明（需与用户端约定一致）

以下是与《CS通信协议.md》原文的偏差，用户端须按此处实现保持一致：

| 项 | 协议原文 | 本工程处理 |
|---|---|---|
| 魔数 | `0xEVCB1234`（`V` 非法十六进制） | 改用 `0xE7CB1234` |
| 电桩标识 | `pileId` 数字（如 101） | 承载 `charger_code` 字符串（数据库主键，无自增 id） |
| 站点价格 | 单字段 `price` | 两档价：主价=快充价，另附 `priceFast`/`priceSlow` |
| `targetKwh`/`targetPercent` | 订单表无对应列 | 仅用于预估费用，不落库 |
| 充电电量计量 | 真实设备上报 | 服务端按"桩功率×时长×0.85"模拟 |

## 9. 密码安全（当前阶段）

当前按决策**沿用明文哈希比对**（客户端先 SHA-256，服务端直接比对该哈希），
未做加盐。切换方式：`config.ini -> [Security] PasswordMode=saltedhash`，
`PasswordUtil` 会自动改为 `SHA256(客户端哈希 + 随机盐)` 并存 `salt$hash` 复合串，
**无需改任何业务代码**。切换后旧明文账号需重置口令。

## 10. 后续开放点

1. 头像上传（.jpg）：协议未定义文件传输，属用户端功能，服务器端暂不实现；
2. 电桩故障→异常订单主动上报（需求矩阵 #25，归属"设备模拟"侧）；
3. 充电站删除：因外键 RESTRICT 且无标记列，如需物理/惰性删除需先改表结构；
4. 管理员"新增/编辑用户"：用户侧已有注册与改昵称，管理员端暂不重复提供。
