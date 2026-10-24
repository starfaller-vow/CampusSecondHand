#include "DatabaseManager.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QFile>
#include <QTextStream>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDebug>

DatabaseManager &DatabaseManager::instance()
{
    static DatabaseManager inst;
    return inst;
}

bool DatabaseManager::init(QString &errorMsg)
{
    if (!openConnection(errorMsg)) return false;
    if (!initSchema(errorMsg)) return false;
    if (!applyMigrations(errorMsg)) return false;
    if (!ensureDefaultAdmin(errorMsg)) return false;
    if (!loadSensitiveWords(errorMsg)) return false;
    return true;
}

bool DatabaseManager::openConnection(QString &errorMsg)
{
    m_ownerThread = QThread::currentThreadId();
    m_db = QSqlDatabase::addDatabase("QSQLITE");
    // 数据库文件 = 程序同目录 + data.db
    QString dbPath = QCoreApplication::applicationDirPath() + "/data.db";
    m_db.setDatabaseName(dbPath);

    if (!m_db.open()) {
        errorMsg = m_db.lastError().text();
        return false;
    }

    // 启用外键约束；写锁被占时最多等 5 秒（避免偶尔撞锁直接报错）
    QSqlQuery q(m_db);
    q.exec("PRAGMA foreign_keys = ON");
    q.exec("PRAGMA busy_timeout = 5000");
    // WAL：多连接可并发读、单写，读不阻塞写、写不阻塞读（服务端多线程并发的基础）
    if (!q.exec("PRAGMA journal_mode = WAL")) {
        qWarning() << "设置 WAL 模式失败：" << q.lastError().text();
    }
    q.exec("PRAGMA synchronous = NORMAL");

    qDebug() << "数据库已打开（WAL）：" << dbPath;
    return true;
}

QSqlDatabase DatabaseManager::db() const
{
    // 初始化线程（通常是主线程）直接使用默认连接
    if (QThread::currentThreadId() == m_ownerThread) {
        return m_db;
    }
    const Qt::HANDLE tid = QThread::currentThreadId();
    QMutexLocker locker(&m_connMutex);
    auto it = m_threadConnNames.constFind(tid);
    if (it != m_threadConnNames.constEnd()) {
        return QSqlDatabase::database(it.value(), false);
    }
    // 工作线程首次访问：创建该线程专属命名连接（SQLite/Qt 连接不可跨线程使用）
    const QString connName = QStringLiteral("worker_%1")
                                 .arg(reinterpret_cast<quintptr>(tid));
    {
        // 释放锁后再 addDatabase 也行，但连接创建极快，持锁更简单安全
        QSqlDatabase c = QSqlDatabase::addDatabase("QSQLITE", connName);
        c.setDatabaseName(QCoreApplication::applicationDirPath() + "/data.db");
        if (!c.open()) {
            qWarning() << "工作线程数据库连接打开失败：" << c.lastError().text();
            return m_db; // 兜底（理论上不应发生）
        }
        QSqlQuery pq(c);
        pq.exec("PRAGMA foreign_keys = ON");
        pq.exec("PRAGMA busy_timeout = 5000");
        pq.exec("PRAGMA journal_mode = WAL");
        pq.exec("PRAGMA synchronous = NORMAL");
    }
    m_threadConnNames.insert(tid, connName);
    qDebug() << "工作线程数据库连接已创建：" << connName;
    return QSqlDatabase::database(connName, false);
}

void DatabaseManager::releaseCurrentThreadConnection()
{
    const Qt::HANDLE tid = QThread::currentThreadId();
    if (tid == m_ownerThread) return; // 默认连接不在此释放
    QString connName;
    {
        QMutexLocker locker(&m_connMutex);
        auto it = m_threadConnNames.find(tid);
        if (it == m_threadConnNames.end()) return;
        connName = it.value();
        m_threadConnNames.erase(it);
    }
    // 必须在拥有连接的线程内关闭并移除（此时无在途查询）
    {
        QSqlDatabase c = QSqlDatabase::database(connName, false);
        if (c.isValid()) c.close();
    }
    QSqlDatabase::removeDatabase(connName);
    qDebug() << "工作线程数据库连接已释放：" << connName;
}

bool DatabaseManager::initSchema(QString &errorMsg)
{
    // 尝试从 init.sql 读取
    QString sqlPath = QCoreApplication::applicationDirPath() + "/init.sql";
    QFile file(sqlPath);

    QString sqlScript;
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream ts(&file);
        ts.setEncoding(QStringConverter::Utf8);
        sqlScript = ts.readAll();
        file.close();
        qDebug() << "已从 init.sql 读取建表脚本";
    } else {
        // init.sql 不存在时的兜底：直接内嵌 SQL
        qWarning() << "init.sql 不存在，使用内嵌建表脚本";
        // 简略版兜底 SQL（实际 init.sql 会覆盖更完整的）
        sqlScript = R"(
PRAGMA foreign_keys = ON;
CREATE TABLE IF NOT EXISTS users (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    username TEXT NOT NULL UNIQUE,
    password_hash TEXT NOT NULL,
    role TEXT NOT NULL DEFAULT 'user',
    student_no TEXT NOT NULL UNIQUE,
    college TEXT,
    contact TEXT,
    balance REAL NOT NULL DEFAULT 0 CHECK(balance >= 0),
    status TEXT NOT NULL DEFAULT 'active',
    create_time TEXT NOT NULL DEFAULT (datetime('now','localtime'))
);
CREATE TABLE IF NOT EXISTS categories (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL UNIQUE,
    sort_order INTEGER NOT NULL DEFAULT 0
);
CREATE TABLE IF NOT EXISTS goods (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    seller_id INTEGER NOT NULL REFERENCES users(id),
    title TEXT NOT NULL,
    description TEXT NOT NULL,
    original_price REAL NOT NULL CHECK(original_price > 0),
    condition TEXT NOT NULL,
    current_price REAL NOT NULL CHECK(current_price > 0),
    category_id INTEGER NOT NULL REFERENCES categories(id),
    images TEXT,
status TEXT NOT NULL DEFAULT 'on_sale',
off_reason TEXT NOT NULL DEFAULT '',
create_time TEXT NOT NULL DEFAULT (datetime('now','localtime')),
update_time TEXT NOT NULL DEFAULT (datetime('now','localtime'))
);
CREATE TABLE IF NOT EXISTS transactions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    buyer_id INTEGER NOT NULL REFERENCES users(id),
    seller_id INTEGER NOT NULL REFERENCES users(id),
    goods_id INTEGER NOT NULL REFERENCES goods(id),
    status TEXT NOT NULL DEFAULT 'pending',
    remark TEXT,
    fee_amount REAL NOT NULL DEFAULT 0,
    fee_refunded INTEGER NOT NULL DEFAULT 0,
    create_time TEXT NOT NULL DEFAULT (datetime('now','localtime')),
    finish_time TEXT
);
CREATE TABLE IF NOT EXISTS favorites (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id INTEGER NOT NULL REFERENCES users(id),
    goods_id INTEGER NOT NULL REFERENCES goods(id),
    create_time TEXT NOT NULL DEFAULT (datetime('now','localtime')),
    UNIQUE(user_id, goods_id)
);
CREATE TABLE IF NOT EXISTS messages (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id INTEGER NOT NULL REFERENCES users(id),
    title TEXT NOT NULL,
    content TEXT,
    is_read INTEGER NOT NULL DEFAULT 0,
    type TEXT NOT NULL DEFAULT 'system',
    create_time TEXT NOT NULL DEFAULT (datetime('now','localtime'))
);
CREATE TABLE IF NOT EXISTS settings (
    key TEXT PRIMARY KEY,
    value TEXT NOT NULL
);
CREATE TABLE IF NOT EXISTS redeem_codes (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    code TEXT NOT NULL UNIQUE,
    amount REAL NOT NULL CHECK(amount > 0),
    used INTEGER NOT NULL DEFAULT 0,
    used_by INTEGER REFERENCES users(id),
    used_time TEXT,
    create_time TEXT NOT NULL DEFAULT (datetime('now','localtime'))
);
CREATE TABLE IF NOT EXISTS balance_logs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id INTEGER NOT NULL REFERENCES users(id),
    type TEXT NOT NULL,
    amount REAL NOT NULL,
    balance_after REAL NOT NULL CHECK(balance_after >= 0),
    ref_id TEXT,
    remark TEXT,
    create_time TEXT NOT NULL DEFAULT (datetime('now','localtime'))
);
CREATE TABLE IF NOT EXISTS escrow (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tx_id INTEGER NOT NULL UNIQUE REFERENCES transactions(id),
    amount REAL NOT NULL CHECK(amount > 0),
    fee REAL NOT NULL DEFAULT 0,
    status TEXT NOT NULL DEFAULT 'held',
    create_time TEXT NOT NULL DEFAULT (datetime('now','localtime')),
    finish_time TEXT
);
CREATE TABLE IF NOT EXISTS reports (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    reporter_id INTEGER NOT NULL REFERENCES users(id),
    target_type TEXT NOT NULL,
    target_id INTEGER NOT NULL,
    reason_type TEXT NOT NULL,
    description TEXT NOT NULL,
    status TEXT NOT NULL DEFAULT 'pending',
    handler_id INTEGER REFERENCES users(id),
    handler_note TEXT,
    create_time TEXT NOT NULL DEFAULT (datetime('now','localtime')),
    handle_time TEXT
);
INSERT OR IGNORE INTO categories (name, sort_order) VALUES
    ('教材书籍',1),('电子数码',2),('生活用品',3),('服装鞋包',4),('运动户外',5),('美妆护肤',6),('其他',7);
INSERT OR IGNORE INTO settings (key,value) VALUES
    ('fee_threshold','100'),('fee_rate','0.005'),
    ('coef_全新','0.80'),('coef_九成新','0.60'),('coef_八成新','0.50'),
    ('coef_七成新','0.40'),('coef_六成新及以下','0.30'),
    ('transaction_timeout_days','3');
)";
    }

    if (!execScript(sqlScript, errorMsg)) {
        qWarning() << "建表脚本执行失败：" << errorMsg;
        return false;
    }

    qDebug() << "数据库表结构初始化完成";
    return true;
}

bool DatabaseManager::applyMigrations(QString &errorMsg)
{
    // 幂等迁移：老库补 goods.off_reason 列（举报下架原因）
    QSqlQuery check(m_db);
    if (!check.exec("PRAGMA table_info(goods)")) {
        errorMsg = check.lastError().text();
        return false;
    }
    bool hasOffReason = false;
    while (check.next()) {
        if (check.value(1).toString() == "off_reason") {
            hasOffReason = true;
            break;
        }
    }
    if (!hasOffReason) {
        QSqlQuery alter(m_db);
        if (!alter.exec("ALTER TABLE goods ADD COLUMN off_reason TEXT NOT NULL DEFAULT ''")) {
            errorMsg = alter.lastError().text();
            return false;
        }
        qWarning() << "已迁移：goods 表新增 off_reason 列";
    }
    return true;
}

bool DatabaseManager::ensureDefaultAdmin(QString &errorMsg)
{
    QSqlQuery q(m_db);

    // 检查 users 表是否为空
    q.exec("SELECT COUNT(*) FROM users");
    if (q.next() && q.value(0).toInt() > 0) {
        qDebug() << "用户表已有数据，跳过默认管理员创建";
        return true;
    }

    // 首次启动：创建默认管理员
    QString pwdHash = QString::fromLatin1(
        QCryptographicHash::hash(
            QByteArray(DEFAULT_ADMIN_PASSWORD),
            QCryptographicHash::Sha256
        ).toHex()
    );

    q.prepare(
        "INSERT INTO users (username, password_hash, role, student_no, balance) "
        "VALUES (?, ?, 'admin', ?, 9999)"
    );
    q.addBindValue(QString::fromLatin1(DEFAULT_ADMIN_USERNAME));
    q.addBindValue(pwdHash);
    q.addBindValue(QString::fromLatin1(DEFAULT_ADMIN_STUDENT_NO));

    if (!q.exec()) {
        errorMsg = q.lastError().text();
        return false;
    }

    qDebug() << "默认管理员已创建：admin / admin123";
    return true;
}

bool DatabaseManager::loadSensitiveWords(QString &errorMsg)
{
    Q_UNUSED(errorMsg);

    QString path = QCoreApplication::applicationDirPath() + "/words.txt";
    QFile file(path);
    m_sensitiveWords.clear();

    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream ts(&file);
        ts.setEncoding(QStringConverter::Utf8);
        while (!ts.atEnd()) {
            QString word = ts.readLine().trimmed();
            if (!word.isEmpty()) {
                m_sensitiveWords.append(word);
            }
        }
        file.close();
    }

    qDebug() << "已加载" << m_sensitiveWords.size() << "个敏感词";
    return true;
}

void DatabaseManager::reloadSensitiveWords()
{
    QString err;
    loadSensitiveWords(err);
}

bool DatabaseManager::execScript(const QString &sql, QString &errorMsg)
{
    // SQLite 不支持一次执行多语句，用分号分割逐个执行
    QStringList statements = sql.split(';', Qt::SkipEmptyParts);

    QSqlQuery q(m_db);
    for (const QString &stmt : statements) {
        QString trimmed = stmt.trimmed();
        if (trimmed.isEmpty()) continue;

        if (!q.exec(trimmed)) {
            errorMsg = QString("执行失败:\n%1\n错误: %2").arg(trimmed.left(200), q.lastError().text());
            return false;
        }
    }
    return true;
}
