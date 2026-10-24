-- ============================================================
-- 校园二手交易系统 - 数据库初始化脚本
-- 首次启动由 DatabaseManager 自动执行
-- ============================================================

PRAGMA foreign_keys = ON;

-- ===== 用户表 =====
CREATE TABLE IF NOT EXISTS users (
    id             INTEGER PRIMARY KEY AUTOINCREMENT,
    username       TEXT NOT NULL UNIQUE,
    password_hash  TEXT NOT NULL,
    role           TEXT NOT NULL DEFAULT 'user',
    student_no     TEXT NOT NULL UNIQUE,
    college        TEXT,
    contact        TEXT,
    balance        REAL NOT NULL DEFAULT 0 CHECK(balance >= 0),
    status         TEXT NOT NULL DEFAULT 'active',
    create_time    TEXT NOT NULL DEFAULT (datetime('now','localtime'))
);

-- ===== 分类表 =====
CREATE TABLE IF NOT EXISTS categories (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    name       TEXT NOT NULL UNIQUE,
    sort_order INTEGER NOT NULL DEFAULT 0
);

-- ===== 商品表 =====
CREATE TABLE IF NOT EXISTS goods (
    id             INTEGER PRIMARY KEY AUTOINCREMENT,
    seller_id      INTEGER NOT NULL REFERENCES users(id),
    title          TEXT NOT NULL,
    description    TEXT NOT NULL,
    original_price REAL NOT NULL CHECK(original_price > 0),
    condition      TEXT NOT NULL,
    current_price  REAL NOT NULL CHECK(current_price > 0),
    category_id    INTEGER NOT NULL REFERENCES categories(id),
    images         TEXT,
    status         TEXT NOT NULL DEFAULT 'on_sale',
    off_reason     TEXT NOT NULL DEFAULT '',
    create_time    TEXT NOT NULL DEFAULT (datetime('now','localtime')),
    update_time    TEXT NOT NULL DEFAULT (datetime('now','localtime'))
);

-- ===== 交易表 =====
CREATE TABLE IF NOT EXISTS transactions (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    buyer_id    INTEGER NOT NULL REFERENCES users(id),
    seller_id   INTEGER NOT NULL REFERENCES users(id),
    goods_id    INTEGER NOT NULL REFERENCES goods(id),
    status      TEXT NOT NULL DEFAULT 'pending',
    remark      TEXT,
    fee_amount  REAL NOT NULL DEFAULT 0,
    fee_refunded INTEGER NOT NULL DEFAULT 0,
    create_time TEXT NOT NULL DEFAULT (datetime('now','localtime')),
    finish_time TEXT
);

-- ===== 收藏表 =====
CREATE TABLE IF NOT EXISTS favorites (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id     INTEGER NOT NULL REFERENCES users(id),
    goods_id    INTEGER NOT NULL REFERENCES goods(id),
    create_time TEXT NOT NULL DEFAULT (datetime('now','localtime')),
    UNIQUE(user_id, goods_id)
);

-- ===== 消息表 =====
CREATE TABLE IF NOT EXISTS messages (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id     INTEGER NOT NULL REFERENCES users(id),
    title       TEXT NOT NULL,
    content     TEXT,
    is_read     INTEGER NOT NULL DEFAULT 0,
    type        TEXT NOT NULL DEFAULT 'system',
    create_time TEXT NOT NULL DEFAULT (datetime('now','localtime'))
);

-- ===== 系统设置表 =====
CREATE TABLE IF NOT EXISTS settings (
    key   TEXT PRIMARY KEY,
    value TEXT NOT NULL
);

-- ===== 充值码表 =====
CREATE TABLE IF NOT EXISTS redeem_codes (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    code        TEXT NOT NULL UNIQUE,
    amount      REAL NOT NULL CHECK(amount > 0),
    used        INTEGER NOT NULL DEFAULT 0,
    used_by     INTEGER REFERENCES users(id),
    used_time   TEXT,
    create_time TEXT NOT NULL DEFAULT (datetime('now','localtime'))
);

-- ===== 余额流水表 =====
CREATE TABLE IF NOT EXISTS balance_logs (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id       INTEGER NOT NULL REFERENCES users(id),
    type          TEXT NOT NULL,
    amount        REAL NOT NULL,
    balance_after REAL NOT NULL CHECK(balance_after >= 0),
    ref_id        TEXT,
    remark        TEXT,
    create_time   TEXT NOT NULL DEFAULT (datetime('now','localtime'))
);

-- ===== 平台资金托管表（担保交易）=====
-- 买家付款后货款进托管；确认收货/超时自动结算 → released 打给卖家；
-- 取消/管理员退款 → refunded 直接退回买家。托管期间买卖双方均不可动用。
CREATE TABLE IF NOT EXISTS escrow (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    tx_id       INTEGER NOT NULL UNIQUE REFERENCES transactions(id),
    amount      REAL NOT NULL CHECK(amount > 0),
    fee         REAL NOT NULL DEFAULT 0,
    status      TEXT NOT NULL DEFAULT 'held',   -- held(托管中) / released(已结算) / refunded(已退还)
    create_time TEXT NOT NULL DEFAULT (datetime('now','localtime')),
    finish_time TEXT
);

-- ===== 举报表 =====
CREATE TABLE IF NOT EXISTS reports (
    id           INTEGER PRIMARY KEY AUTOINCREMENT,
    reporter_id  INTEGER NOT NULL REFERENCES users(id),
    target_type  TEXT NOT NULL,
    target_id    INTEGER NOT NULL,
    reason_type  TEXT NOT NULL,
    description  TEXT NOT NULL,
    status       TEXT NOT NULL DEFAULT 'pending',
    handler_id   INTEGER REFERENCES users(id),
    handler_note TEXT,
    create_time  TEXT NOT NULL DEFAULT (datetime('now','localtime')),
    handle_time  TEXT
);

-- ===== 初始化预设分类 =====
INSERT OR IGNORE INTO categories (name, sort_order) VALUES
    ('教材书籍',   1),
    ('电子数码',   2),
    ('生活用品',   3),
    ('服装鞋包',   4),
    ('运动户外',   5),
    ('美妆护肤',   6),
    ('其他',       7);

-- ===== 初始化系统设置 =====
INSERT OR IGNORE INTO settings (key, value) VALUES
    ('fee_threshold',        '100'),
    ('fee_rate',             '0.005'),
    ('coef_全新',            '0.80'),
    ('coef_九成新',          '0.60'),
    ('coef_八成新',          '0.50'),
    ('coef_七成新',          '0.40'),
    ('coef_六成新及以下',    '0.30'),
    ('transaction_timeout_days', '3');
