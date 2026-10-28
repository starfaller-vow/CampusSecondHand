#include "UserDAO.h"
#include "../db/DatabaseManager.h"
#include "../shared/PasswordUtil.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QCryptographicHash>

// ==================== 工具：密码哈希 ====================

QString UserDAO::hashPassword(const QString &password)
{
    // 统一委托公共工具：客户端（注册/登录前哈希）与服务端共用同一实现
    return passutil::sha256Hex(password);
}

// ==================== 查询 ====================

User UserDAO::getById(int id)
{
    User u;
    QSqlQuery q(DatabaseManager::instance().db());
    q.prepare("SELECT * FROM users WHERE id = ?");
    q.addBindValue(id);
    if (q.exec() && q.next()) {
        u.id            = q.value("id").toInt();
        u.username      = q.value("username").toString();
        u.passwordHash  = q.value("password_hash").toString();
        u.role          = q.value("role").toString();
        u.studentNo     = q.value("student_no").toString();
        u.college       = q.value("college").toString();
        u.contact       = q.value("contact").toString();
        u.balance       = q.value("balance").toDouble();
        u.status        = q.value("status").toString();
        u.createTime    = q.value("create_time").toString();
    }
    return u;
}

User UserDAO::getByUsername(const QString &username)
{
    User u;
    QSqlQuery q(DatabaseManager::instance().db());
    q.prepare("SELECT * FROM users WHERE username = ?");
    q.addBindValue(username);
    if (q.exec() && q.next()) {
        u.id            = q.value("id").toInt();//主键，自增整数 ，全局唯一标识用户。⚠️ 特殊用法：上层用`u.id == 0` 判断「没查到用户」（因为空结构体默认 id=0）
        u.username      = q.value("username").toString();//用户名
        u.passwordHash  = q.value("password_hash").toString();//密码
        u.role          = q.value("role").toString();//角色
        u.studentNo     = q.value("student_no").toString();//学号
        u.college       = q.value("college").toString();//学院
        u.contact       = q.value("contact").toString();//联系方式
        u.balance       = q.value("balance").toDouble();//余额
        u.status        = q.value("status").toString();//账号状态
        u.createTime    = q.value("create_time").toString();//注册时间
    }
    return u;
}

User UserDAO::getByStudentNo(const QString &studentNo)
{
    User u;
    QSqlQuery q(DatabaseManager::instance().db());
    q.prepare("SELECT * FROM users WHERE student_no = ?");
    q.addBindValue(studentNo);
    if (q.exec() && q.next()) {
        u.id            = q.value("id").toInt();
        u.username      = q.value("username").toString();
        u.passwordHash  = q.value("password_hash").toString();
        u.role          = q.value("role").toString();
        u.studentNo     = q.value("student_no").toString();
        u.college       = q.value("college").toString();
        u.contact       = q.value("contact").toString();
        u.balance       = q.value("balance").toDouble();
        u.status        = q.value("status").toString();
        u.createTime    = q.value("create_time").toString();
    }
    return u;
}

QList<User> UserDAO::getAll()
{
    QList<User> list;
    QSqlQuery q(DatabaseManager::instance().db());
    q.exec("SELECT * FROM users ORDER BY id ASC");
    while (q.next()) {
        User u;
        u.id            = q.value("id").toInt();
        u.username      = q.value("username").toString();
        u.role          = q.value("role").toString();
        u.studentNo     = q.value("student_no").toString();
        u.college       = q.value("college").toString();
        u.contact       = q.value("contact").toString();
        u.balance       = q.value("balance").toDouble();
        u.status        = q.value("status").toString();
        u.createTime    = q.value("create_time").toString();
        list.append(u);
    }
    return list;
}

// ==================== 存在性检查 ====================

bool UserDAO::usernameExists(const QString &username)
{
    QSqlQuery q(DatabaseManager::instance().db());
    q.prepare("SELECT COUNT(*) FROM users WHERE username = ?");
    q.addBindValue(username);
    return q.exec() && q.next() && q.value(0).toInt() > 0;
}

bool UserDAO::studentNoExists(const QString &studentNo)
{
    QSqlQuery q(DatabaseManager::instance().db());
    q.prepare("SELECT COUNT(*) FROM users WHERE student_no = ?");
    q.addBindValue(studentNo);
    return q.exec() && q.next() && q.value(0).toInt() > 0;
}

// ==================== 注册 ====================

bool UserDAO::insert(const User &user, QString &errorMsg)
{
    QSqlQuery q(DatabaseManager::instance().db());
    q.prepare(
        "INSERT INTO users (username, password_hash, role, student_no, college, contact) "
        "VALUES (?, ?, ?, ?, ?, ?)"
    );
    q.addBindValue(user.username);
    q.addBindValue(user.passwordHash);
    q.addBindValue(user.role.isEmpty() ? QString("user") : user.role);
    q.addBindValue(user.studentNo);
    q.addBindValue(user.college);
    q.addBindValue(user.contact);

    if (!q.exec()) {
        errorMsg = q.lastError().text();
        return false;
    }
    return true;
}

// ==================== 登录 ====================

bool UserDAO::login(const QString &username, const QString &password,
                    User &outUser, QString &errorMsg)
{
    // 明文入口：先哈希，再走 C/S 同一套比对逻辑（保持两端行为一致）
    return loginByHash(username, hashPassword(password), outUser, errorMsg);
}

bool UserDAO::loginByHash(const QString &username, const QString &passwordHash,
                          User &outUser, QString &errorMsg)
{
    User u = getByUsername(username);
    if (u.id == 0) {
        errorMsg = QStringLiteral("用户名或密码错误");
        return false;
    }

    // 直接比对哈希，不二次哈希
    if (passwordHash != u.passwordHash) {
        errorMsg = QStringLiteral("用户名或密码错误");
        return false;
    }

    if (u.isBanned()) {
        errorMsg = QStringLiteral("账号已被封禁，请联系管理员");
        return false;
    }

    if (u.isDeleted()) {
        errorMsg = QStringLiteral("该账号已注销");
        return false;
    }

    outUser = u;
    return true;
}

// ==================== 更新 ====================

bool UserDAO::updateBalance(int userId, double newBalance, QString &errorMsg)
{
    QSqlQuery q(DatabaseManager::instance().db());
    q.prepare("UPDATE users SET balance = ? WHERE id = ?");
    q.addBindValue(newBalance);
    q.addBindValue(userId);
    if (!q.exec()) {
        errorMsg = q.lastError().text();
        return false;
    }
    return true;
}



bool UserDAO::updateContact(int userId, const QString &contact, QString &errorMsg)
{
    QSqlQuery q(DatabaseManager::instance().db());
    q.prepare("UPDATE users SET contact = ? WHERE id = ?");
    q.addBindValue(contact);
    q.addBindValue(userId);
    if (!q.exec()) {
        errorMsg = q.lastError().text();
        return false;
    }
    return true;
}

bool UserDAO::updatePassword(int userId, const QString &newPasswordHash, QString &errorMsg)
{
    QSqlQuery q(DatabaseManager::instance().db());
    q.prepare("UPDATE users SET password_hash = ? WHERE id = ?");
    q.addBindValue(newPasswordHash);
    q.addBindValue(userId);
    if (!q.exec()) {
        errorMsg = q.lastError().text();
        return false;
    }
    return true;
}

bool UserDAO::changeBalance(int userId, double delta, double &newBalanceOut,
                            QString &errorMsg, bool useTransaction)
{
    QSqlDatabase db = DatabaseManager::instance().db();

    // 仅在调用方未开事务时，自己开事务
    if (useTransaction) {
        if (!db.transaction()) {
            errorMsg = QStringLiteral("无法开启事务");
            return false;
        }
    }

    // 1. 读取当前余额
    QSqlQuery q(db);
    q.prepare("SELECT balance FROM users WHERE id = ?");
    q.addBindValue(userId);
    if (!q.exec() || !q.next()) {
        errorMsg = QStringLiteral("用户不存在");
        if (useTransaction) db.rollback();
        return false;
    }

    double currentBalance = q.value(0).toDouble();
    double newBalance = currentBalance + delta;

    if (newBalance < 0) {
        errorMsg = QStringLiteral("余额不足");
        if (useTransaction) db.rollback();
        return false;
    }

    // 2. 更新余额
    q.prepare("UPDATE users SET balance = ? WHERE id = ?");
    q.addBindValue(newBalance);
    q.addBindValue(userId);
    if (!q.exec()) {
        errorMsg = q.lastError().text();
        if (useTransaction) db.rollback();
        return false;
    }

    // 仅自己开事务时才 commit
    if (useTransaction) {
        if (!db.commit()) {
            errorMsg = db.lastError().text();
            return false;
        }
    }

    newBalanceOut = newBalance;
    return true;
}
