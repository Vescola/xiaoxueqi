-- =========================================================
-- 电动汽车充电桩应用管理平台
-- 数据库初始化脚本：database.sql
-- 数据库类型：SQLite
-- =========================================================


-- =========================================================
-- 0. 开启 SQLite 外键约束
-- SQLite 默认情况下可能不会自动启用外键约束
-- 开启后，FOREIGN KEY 才会真正生效
-- =========================================================
PRAGMA foreign_keys = ON;



-- =========================================================
-- 1. 用户表 users
--
-- 功能：
--   保存普通用户的登录信息、基本资料、钱包余额以及账号状态
--
-- 主要字段：
--   id          用户ID，主键
--   phone       手机号
--   password    用户密码
--   nickname    用户昵称
--   avatar_path 用户头像路径
--   balance     钱包余额
--   status      用户状态
--   created_at  注册时间
-- =========================================================
CREATE TABLE IF NOT EXISTS users (

    -- 用户ID
    -- INTEGER PRIMARY KEY AUTOINCREMENT 表示自动递增主键
    id INTEGER PRIMARY KEY AUTOINCREMENT,

    -- 用户手机号
    -- NOT NULL：不能为空
    -- UNIQUE：手机号不能重复
    phone TEXT NOT NULL UNIQUE,

    -- 用户密码
    password TEXT NOT NULL,

    -- 用户昵称
    nickname TEXT NOT NULL,

    -- 用户头像路径
    -- 如果没有设置头像，可以为空
    avatar_path TEXT,

    -- 用户钱包余额
    -- 默认余额为 0 元
    balance REAL NOT NULL DEFAULT 0.0,

    -- 用户账号状态
    --
    -- normal：正常
    -- frozen：冻结
    status TEXT NOT NULL DEFAULT 'normal'
        CHECK(status IN ('normal', 'frozen')),

    -- 用户注册时间
    -- 默认使用当前时间
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP
);



-- =========================================================
-- 2. 管理员表 admins
--
-- 功能：
--   保存后台管理员账号信息
--
-- 主要字段：
--   id          管理员ID
--   username    管理员用户名
--   password    管理员密码
--   created_at  创建时间
-- =========================================================
CREATE TABLE IF NOT EXISTS admins (

    -- 管理员ID，自增主键
    id INTEGER PRIMARY KEY AUTOINCREMENT,

    -- 管理员用户名
    -- UNIQUE 保证用户名唯一
    username TEXT NOT NULL UNIQUE,

    -- 管理员密码
    password TEXT NOT NULL,

    -- 管理员账号创建时间
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP
);



-- =========================================================
-- 3. 充电站表 stations
--
-- 功能：
--   保存充电站的基本信息和地理位置
--
-- 主要字段：
--   id          充电站ID
--   name        充电站名称
--   address     详细地址
--   longitude   经度
--   latitude    纬度
--   created_at  创建时间
-- =========================================================
CREATE TABLE IF NOT EXISTS stations (

    -- 充电站ID，自增主键
    id INTEGER PRIMARY KEY AUTOINCREMENT,

    -- 充电站名称
    name TEXT NOT NULL,

    -- 充电站详细地址
    address TEXT NOT NULL,

    -- 经度
    longitude REAL NOT NULL,

    -- 纬度
    latitude REAL NOT NULL,

    -- 充电站数据创建时间
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP
);



-- =========================================================
-- 4. 充电桩表 chargers
--
-- 功能：
--   保存每一个充电桩的详细信息
--
-- 说明：
--   本表不使用自增 id
--   直接使用 charger_code 作为主键
--
-- 主要字段：
--   charger_code    充电桩编号
--   station_id      所属充电站ID
--   type            充电桩类型
--   power_kw        充电功率
--   status          当前状态
--   charge_count    累计充电次数
--   total_duration  累计充电时长
-- =========================================================
CREATE TABLE IF NOT EXISTS chargers (

    -- 充电桩编号
    -- 例如：
    -- SZ001-01
    -- SZ001-02
    --
    -- 直接作为主键
    charger_code TEXT PRIMARY KEY,

    -- 所属充电站ID
    station_id INTEGER NOT NULL,

    -- 充电桩类型
    --
    -- fast：快充
    -- slow：慢充
    type TEXT NOT NULL
        CHECK(type IN ('fast', 'slow')),

    -- 充电功率
    -- 单位：kW
    power_kw REAL NOT NULL,

    -- 当前充电桩状态
    --
    -- idle      空闲
    -- charging  充电中
    -- fault     故障
    -- offline   离线
    status TEXT NOT NULL DEFAULT 'idle'
        CHECK(status IN (
            'idle',
            'charging',
            'fault',
            'offline'
        )),

    -- 累计完成充电次数
    charge_count INTEGER NOT NULL DEFAULT 0,

    -- 累计充电时长
    -- 单位统一使用分钟
    total_duration INTEGER NOT NULL DEFAULT 0,

    -- 外键约束：
    -- station_id 必须对应 stations 表中已经存在的 id
    FOREIGN KEY(station_id)
        REFERENCES stations(id)
        ON UPDATE CASCADE
        ON DELETE RESTRICT
);



-- =========================================================
-- 5. 充电订单表 orders
--
-- 功能：
--   保存用户每一次充电订单的信息
--
-- 说明：
--   用户选择空闲充电桩后：
--
--   创建订单
--       ↓
--   charging
--       ↓
--   unpaid
--       ↓
--   paid
--
--   异常情况下可以进入：
--   cancelled
--
--   本表使用 order_no 作为主键
--
-- 主要字段：
--   order_no          订单编号
--   user_id           用户ID
--   charger_code      充电桩编号
--   status            订单状态
--   start_time        开始充电时间
--   end_time          结束充电时间
--   duration_minutes  充电时长
--   energy_kwh        充电量
--   unit_price        本次订单单价
--   amount            最终金额
--   created_at        订单创建时间
-- =========================================================
CREATE TABLE IF NOT EXISTS orders (

    -- 订单编号
    -- 例如：
    -- C202609040001
    -- C202609040002
    --
    -- 直接作为主键
    order_no TEXT PRIMARY KEY,

    -- 下单用户ID
    user_id INTEGER NOT NULL,

    -- 使用的充电桩编号
    charger_code TEXT NOT NULL,

    -- 订单状态
    --
    -- charging   充电中
    -- unpaid     充电结束，等待支付
    -- paid       已支付
    -- cancelled  已取消
    status TEXT NOT NULL
        CHECK(status IN (
            'charging',
            'unpaid',
            'paid',
            'cancelled'
        )),

    -- 实际开始充电时间
    start_time DATETIME,

    -- 实际结束充电时间
    -- 充电尚未结束时可以为空
    end_time DATETIME,

    -- 本次充电时长
    -- 单位：分钟
    duration_minutes INTEGER NOT NULL DEFAULT 0,

    -- 本次充电电量
    -- 单位：kWh
    energy_kwh REAL NOT NULL DEFAULT 0.0,

    -- 本次订单采用的充电单价
    -- 即使以后计费规则变化
    -- 也可以保留历史订单当时的价格
    unit_price REAL NOT NULL DEFAULT 0.0,

    -- 本次订单最终金额
    amount REAL NOT NULL DEFAULT 0.0,

    -- 订单创建时间
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,

    -- 外键：
    -- user_id 对应 users.id
    FOREIGN KEY(user_id)
        REFERENCES users(id)
        ON UPDATE CASCADE
        ON DELETE RESTRICT,

    -- 外键：
    -- charger_code 对应 chargers.charger_code
    FOREIGN KEY(charger_code)
        REFERENCES chargers(charger_code)
        ON UPDATE CASCADE
        ON DELETE RESTRICT
);



-- =========================================================
-- 6. 钱包流水表 wallet_records
--
-- 功能：
--   保存用户钱包中的每一次资金变化
--
-- 主要用途：
--   1. 用户充值
--   2. 充电订单扣费
--
-- 主要字段：
--   id             流水ID
--   user_id        用户ID
--   type           流水类型
--   amount         本次金额变化
--   balance_after  操作后的账户余额
--   order_no       对应订单编号
--   created_at     流水创建时间
-- =========================================================
CREATE TABLE IF NOT EXISTS wallet_records (

    -- 钱包流水ID
    -- 使用自增主键
    id INTEGER PRIMARY KEY AUTOINCREMENT,

    -- 所属用户ID
    user_id INTEGER NOT NULL,

    -- 流水类型
    --
    -- recharge：充值
    -- charge：充电扣费
    type TEXT NOT NULL
        CHECK(type IN (
            'recharge',
            'charge'
        )),

    -- 本次金额变化
    --
    -- 例如：
    -- 充值 100 元：
    -- amount = 100
    --
    -- 充电消费 30 元：
    -- amount = -30
    amount REAL NOT NULL,

    -- 此次资金变化完成后的余额
    balance_after REAL NOT NULL,

    -- 对应的充电订单编号
    --
    -- 普通充值时：
    -- order_no 可以为空
    --
    -- 充电扣费时：
    -- order_no 保存对应订单编号
    order_no TEXT,

    -- 流水创建时间
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,

    -- 外键：
    -- user_id 对应 users.id
    FOREIGN KEY(user_id)
        REFERENCES users(id)
        ON UPDATE CASCADE
        ON DELETE RESTRICT,

    -- 外键：
    -- order_no 对应 orders.order_no
    FOREIGN KEY(order_no)
        REFERENCES orders(order_no)
        ON UPDATE CASCADE
        ON DELETE RESTRICT
);



-- =========================================================
-- 7. 初始化默认管理员账号
--
-- 项目默认管理员：
--
-- 用户名：admin
-- 密码：123456
--
-- INSERT OR IGNORE：
-- 如果数据库中已经存在 admin
-- 则不会再次重复插入
-- =========================================================
INSERT OR IGNORE INTO admins (
    username,
    password
)
VALUES (
    'admin',
    '123456'
);



-- =========================================================
-- 8. 创建索引
--
-- 索引可以提高一些常用查询的效率
-- =========================================================


-- 根据手机号快速查询用户
CREATE INDEX IF NOT EXISTS idx_users_phone
ON users(phone);


-- 根据充电站ID快速查询该站所有充电桩
CREATE INDEX IF NOT EXISTS idx_chargers_station_id
ON chargers(station_id);


-- 根据用户ID查询用户所有历史订单
CREATE INDEX IF NOT EXISTS idx_orders_user_id
ON orders(user_id);


-- 根据充电桩编号查询该桩历史订单
CREATE INDEX IF NOT EXISTS idx_orders_charger_code
ON orders(charger_code);


-- 根据订单状态查询订单
-- 例如查询所有 charging 或 unpaid 订单
CREATE INDEX IF NOT EXISTS idx_orders_status
ON orders(status);


-- 根据用户ID查询钱包流水
CREATE INDEX IF NOT EXISTS idx_wallet_user_id
ON wallet_records(user_id);


-- 根据订单编号查询对应钱包扣费流水
CREATE INDEX IF NOT EXISTS idx_wallet_order_no
ON wallet_records(order_no);



-- =========================================================
-- database.sql 结束
-- =========================================================