#include "UserManager.h"
#include "../dao/UserDAO.h"
#include "../db/DatabaseManager.h"
#include "../shared/Validators.h"

#include <QRegularExpression>

UserManager &UserManager::instance()
{
    static UserManager inst;
    return inst;
}

// ==================== 校验工具（委托共享实现，客户端/服务端规则一致）====================

bool UserManager::isValidUsername(const QString &username, QString &err)
{
    return uservalid::isValidUsername(username, err);
}

bool UserManager::isValidPassword(const QString &password, QString &err)
{
    return uservalid::isValidPassword(password, err);
}

bool UserManager::isValidStudentNo(const QString &studentNo, QString &err)
{
    return uservalid::isValidStudentNo(studentNo, err);
}

// ==================== 注册 ====================

bool UserManager::registerUser(const QString &username, const QString &password,
                               const QString &studentNo, const QString &college,
                               const QString &contact, QString &errorMsg)
{
    // 明文入口：先哈希，再走 C/S 同一套注册逻辑（保持两端行为一致）
    return registerUserWithHash(username, UserDAO::hashPassword(password),
                                studentNo, college, contact, errorMsg);
}

bool UserManager::registerUserWithHash(const QString &username, const QString &passwordHash,
                                       const QString &studentNo, const QString &college,
                                       const QString &contact, QString &errorMsg)
{
    // 1. 格式校验（密码强度已由客户端在明文阶段校验，这里不再校验哈希格式）
    if (!isValidUsername(username, errorMsg)) return false;
    if (!isValidStudentNo(studentNo, errorMsg)) return false;

    // 2. 唯一性校验
    if (UserDAO::usernameExists(username)) {
        errorMsg = QStringLiteral("用户名已被注册");
        return false;
    }
    if (UserDAO::studentNoExists(studentNo)) {
        errorMsg = QStringLiteral("该学号已被注册");
        return false;
    }

    // 3. 敏感词检查
    const QStringList &words = DatabaseManager::instance().sensitiveWords();
    for (const QString &w : words) {
        if (username.contains(w) || contact.contains(w)) {
            errorMsg = QStringLiteral("用户名或联系方式包含敏感词");
            return false;
        }
    }

    // 4. 组装 User 对象（密码直接以哈希入库）
    User u;
    u.username     = username;
    u.passwordHash = passwordHash;
    u.role         = "user";
    u.studentNo    = studentNo;
    u.college      = college;
    u.contact      = contact;
    u.balance      = 0;
    u.status       = "active";

    return UserDAO::insert(u, errorMsg);
}

// ==================== 登录 ====================

bool UserManager::loginUser(const QString &username, const QString &password,
                            User &outUser, QString &errorMsg)
{
    return UserDAO::login(username, password, outUser, errorMsg);
}

// ==================== 管理员操作 ====================

bool UserManager::banUser(int userId, QString &errorMsg)
{
    return UserDAO::updateStatus(userId, "banned", errorMsg);
}

bool UserManager::unbanUser(int userId, QString &errorMsg)
{
    return UserDAO::updateStatus(userId, "active", errorMsg);
}

bool UserManager::deleteUser(int userId, QString &errorMsg)
{
    return UserDAO::updateStatus(userId, "deleted", errorMsg);
}

QList<User> UserManager::listAllUsers()
{
    return UserDAO::getAll();
}

// ==================== 个人信息 ====================

bool UserManager::changeContact(int userId, const QString &contact, QString &errorMsg)
{
    return UserDAO::updateContact(userId, contact, errorMsg);
}

bool UserManager::changePassword(int userId, const QString &oldPwd,
                                 const QString &newPwd, QString &errorMsg)
{
    User u = UserDAO::getById(userId);
    if (u.id == 0) {
        errorMsg = QStringLiteral("用户不存在");
        return false;
    }

    // 验证旧密码
    if (UserDAO::hashPassword(oldPwd) != u.passwordHash) {
        errorMsg = QStringLiteral("旧密码错误");
        return false;
    }

    // 校验新密码
    if (!isValidPassword(newPwd, errorMsg)) return false;

    // 更新
    return UserDAO::updatePassword(userId, UserDAO::hashPassword(newPwd), errorMsg);
}

bool UserManager::cancelAccount(int userId, QString &errorMsg)
{
    return UserDAO::updateStatus(userId, "deleted", errorMsg);
}

// ==================== 查询 ====================

User UserManager::getUser(int userId)
{
    return UserDAO::getById(userId);
}

User UserManager::getUserByUsername(const QString &username)
{
    return UserDAO::getByUsername(username);
}

User UserManager::getUserByStudentNo(const QString &studentNo)
{
    return UserDAO::getByStudentNo(studentNo);
}
