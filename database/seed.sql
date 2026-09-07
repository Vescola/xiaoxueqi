-- =========================================================
-- 东软电动汽车充电桩应用管理平台
-- 测试数据初始化脚本 seed.sql
-- =========================================================

PRAGMA foreign_keys = ON;


-- =========================================================
-- 1. 插入测试用户
-- =========================================================
INSERT INTO users (
    phone,
    password,
    nickname,
    avatar_path,
    balance,
    status
)
VALUES
(
    '13800138001',
    '123456',
    '用户8001',
    NULL,
    100.00,
    'normal'
);

INSERT INTO users (
    phone,
    password,
    nickname,
    avatar_path,
    balance,
    status
)
VALUES
(
    '13800138002',
    '123456',
    '小明',
    NULL,
    200.00,
    'normal'
);

INSERT INTO users (
    phone,
    password,
    nickname,
    avatar_path,
    balance,
    status
)
VALUES
(
    '13800138003',
    '123456',
    '测试用户',
    NULL,
    50.00,
    'frozen'
);


-- =========================================================
-- 2. 插入测试充电站
-- =========================================================
INSERT INTO stations (
    name,
    address,
    longitude,
    latitude
)
VALUES
(
    '深圳市民中心充电站',
    '深圳市福田区福中三路',
    114.0579,
    22.5431
);

INSERT INTO stations (
    name,
    address,
    longitude,
    latitude
)
VALUES
(
    '深圳北站充电站',
    '深圳市龙华区深圳北站附近',
    114.0295,
    22.6098
);

INSERT INTO stations (
    name,
    address,
    longitude,
    latitude
)
VALUES
(
    '南山科技园充电站',
    '深圳市南山区科技园',
    113.9448,
    22.5405
);


-- =========================================================
-- 3. 插入测试充电桩
-- =========================================================
INSERT INTO chargers (
    charger_code,
    station_id,
    type,
    power_kw,
    status,
    charge_count,
    total_duration
)
VALUES
(
    'SZ001-01',
    1,
    'fast',
    120,
    'idle',
    12,
    360
);

INSERT INTO chargers (
    charger_code,
    station_id,
    type,
    power_kw,
    status,
    charge_count,
    total_duration
)
VALUES
(
    'SZ001-02',
    1,
    'fast',
    120,
    'charging',
    8,
    240
);

INSERT INTO chargers (
    charger_code,
    station_id,
    type,
    power_kw,
    status,
    charge_count,
    total_duration
)
VALUES
(
    'SZ001-03',
    1,
    'slow',
    7,
    'fault',
    20,
    900
);

INSERT INTO chargers (
    charger_code,
    station_id,
    type,
    power_kw,
    status,
    charge_count,
    total_duration
)
VALUES
(
    'SZ002-01',
    2,
    'fast',
    120,
    'idle',
    15,
    500
);

INSERT INTO chargers (
    charger_code,
    station_id,
    type,
    power_kw,
    status,
    charge_count,
    total_duration
)
VALUES
(
    'SZ003-01',
    3,
    'slow',
    7,
    'idle',
    6,
    320
);


-- =========================================================
-- 4. 插入测试订单
-- =========================================================

-- 已完成并支付的订单
INSERT INTO orders (
    order_no,
    user_id,
    charger_code,
    status,
    start_time,
    end_time,
    duration_minutes,
    energy_kwh,
    unit_price,
    amount
)
VALUES
(
    'C202609040001',
    1,
    'SZ001-01',
    'paid',
    '2026-09-04 09:00:00',
    '2026-09-04 10:00:00',
    60,
    20.0,
    1.5,
    30.0
);

-- 正在充电的订单
INSERT INTO orders (
    order_no,
    user_id,
    charger_code,
    status,
    start_time,
    duration_minutes,
    energy_kwh,
    unit_price,
    amount
)
VALUES
(
    'C202609040002',
    2,
    'SZ001-02',
    'charging',
    '2026-09-04 13:30:00',
    0,
    0.0,
    1.5,
    0.0
);


-- =========================================================
-- 5. 插入钱包流水
-- =========================================================

-- 用户1充值100元
INSERT INTO wallet_records (
    user_id,
    type,
    amount,
    balance_after,
    order_no
)
VALUES
(
    1,
    'recharge',
    100.0,
    100.0,
    NULL
);

-- 用户1充电消费30元
INSERT INTO wallet_records (
    user_id,
    type,
    amount,
    balance_after,
    order_no
)
VALUES
(
    1,
    'charge',
    -30.0,
    70.0,
    'C202609040001'
);