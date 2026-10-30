#ifndef USERMANAGER_H
#define USERMANAGER_H

#include <QString>
#include <QList>
#include "../models/Models.h"

/**
 * @brief 用户业务逻辑层
 * 在 DAO 之上加业务校验（学号格式、密码强度、敏感词等）
 */
class UserManager
{
public:
    static UserManager &instance();

    // ===== 注册 =====
    /**
     * @brief 注册新用户
     * @param username 3-20位，字母数字下划线
     * @param password ≥8位
     * @param studentNo 10位数字
     * @param college 可选
     * @param contact 可选
     * @param errorMsg 失败原因
     */
    bool registerUser(const QString &username, const QString &password,
                      const QString &studentNo, const QString &college,
                      const QString &contact, QString &errorMsg);

    /**
     * @brief C/S 注册：密码已在客户端哈希完成（passwordHash=64位sha256 hex）
     *        服务端直接入库哈希，不再校验明文密码强度（明文规则由客户端校验）
     */
    bool registerUserWithHash(const QString &username, const QString &passwordHash,
                              const QString &studentNo, const QString &college,
                              const QString &contact, QString &errorMsg);

    // ===== 登录 =====
    bool loginUser(const QString &username, const QString &password,
                   User &outUser, QString &errorMsg);

    // ===== 管理员操作 =====
    bool banUser(int userId, QString &errorMsg);
    bool unbanUser(int userId, QString &errorMsg);
    bool deleteUser(int userId, QString &errorMsg);  // 软删除
    QList<User> listAllUsers();

    // ===== 个人信息 =====
    bool changeContact(int userId, const QString &contact, QString &errorMsg);
    bool changePassword(int userId, const QString &oldPwd,
                        const QString &newPwd, QString &errorMsg);
    bool cancelAccount(int userId, QString &errorMsg);  // 注销=软删除

    // ===== 查询 =====
    User getUser(int userId);
    User getUserByUsername(const QString &username);
    User getUserByStudentNo(const QString &studentNo);

    // ===== 校验工具 =====
    static bool isValidUsername(const QString &username, QString &err);
    static bool isValidPassword(const QString &password, QString &err);
    static bool isValidStudentNo(const QString &studentNo, QString &err);

private:
    UserManager() = default;
};

#endif // USERMANAGER_H
