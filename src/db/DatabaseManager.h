#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QSqlDatabase>
#include <QString>
#include <QStringList>
#include <QHash>
#include <QMutex>
#include <QThread>

/**
 * @brief 数据库管理器（单例）
 * 负责 SQLite 连接、初始化建表、首次启动创建默认管理员
 */
class DatabaseManager
{
public:
    static DatabaseManager &instance();

    /**
     * @brief 初始化数据库（打开连接 + 建表 + 首次启动数据）
     * @param errorMsg 失败时的错误信息输出
     * @return true=成功
     */
    bool init(QString &errorMsg);

    /**
     * @brief 获取当前线程专属数据库连接（已打开）
     * 主线程/初始化线程返回默认连接；服务端工作线程首次调用时按需创建
     * 独立命名连接（SQLite 连接有线程亲和性），配合 WAL 实现多读单写并发。
     */
    QSqlDatabase db() const;

    /**
     * @brief 释放当前工作线程的专属连接（线程结束时在其自身线程内调用）
     */
    void releaseCurrentThreadConnection();

    /**
     * @brief 执行 SQL 脚本（多语句，用 ; 分隔）
     */
    bool execScript(const QString &sql, QString &errorMsg);

    /**
     * @brief 加载敏感词列表（words.txt）
     */
    const QStringList &sensitiveWords() const { return m_sensitiveWords; }

    /**
     * @brief 重新加载敏感词
     */
    void reloadSensitiveWords();

    // ===== 默认管理员配置 =====
    static constexpr const char *DEFAULT_ADMIN_USERNAME = "admin";
    static constexpr const char *DEFAULT_ADMIN_PASSWORD = "admin123";
    static constexpr const char *DEFAULT_ADMIN_STUDENT_NO = "0000000000";

private:
    DatabaseManager() = default;
    ~DatabaseManager() = default;
    DatabaseManager(const DatabaseManager &) = delete;
    DatabaseManager &operator=(const DatabaseManager &) = delete;

    bool openConnection(QString &errorMsg);
    bool initSchema(QString &errorMsg);
    bool applyMigrations(QString &errorMsg);
    bool ensureDefaultAdmin(QString &errorMsg);
    bool loadSensitiveWords(QString &errorMsg);

    QSqlDatabase m_db;
    QStringList m_sensitiveWords;

    Qt::HANDLE m_ownerThread = nullptr;   // 执行 init 的线程（默认连接归属）
    mutable QMutex m_connMutex;
    mutable QHash<Qt::HANDLE, QString> m_threadConnNames; // 工作线程 id -> 连接名
};

#endif // DATABASEMANAGER_H
