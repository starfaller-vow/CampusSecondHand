#ifndef USERDAO_H
#define USERDAO_H

#include <QSqlQuery>
#include <QSqlError>
#include <QString>
#include <QList>
#include <QStringList>

#include "../models/Models.h"

/**
 * @brief 用户数据访问层
 * 纯 SQL 操作，不做业务校验
 */
class UserDAO
{
public:
    // ===== 查询 =====
    static User getById(int id);
    static User getByUsername(const QString &username);
    static User getByStudentNo(const QString &studentNo);
    static QList<User> getAll();

    // ===== 存在性检查 =====
    static bool usernameExists(const QString &username);
    static bool studentNoExists(const QString &studentNo);

    // ===== 注册/登录 =====
    static bool insert(const User &user, QString &errorMsg);

    /**
     * @brief 验证登录
     * @param username 用户名
     * @param password 明文密码
     * @param outUser 成功时返回用户信息
     * @param errorMsg 失败时的原因
     * @return true=登录成功
     */
    static bool login(const QString &username, const QString &password,
                     User &outUser, QString &errorMsg);

    /**
     * @brief C/S 登录：客户端已把密码哈希，服务端【直接比对】传入哈希与库中值，
     *        绝不二次哈希（否则与库中单哈希不兼容）
     */
    static bool loginByHash(const QString &username, const QString &passwordHash,
                            User &outUser, QString &errorMsg);

    // ===== 更新 =====
    static bool updateBalance(int userId, double newBalance, QString &errorMsg);
    static bool updateContact(int userId, const QString &contact, QString &errorMsg);
    static bool updatePassword(int userId, const QString &newPasswordHash, QString &errorMsg);

    // ===== 余额变动（带事务）=====
    /**
     * @brief 给用户余额加减钱（自动计算新余额）
     * @param userId 用户ID
     * @param delta  变动量（正=加钱，负=扣钱）
     * @param newBalanceOut 成功时返回操作后余额
     * @param useTransaction true=内部自己开事务；false=调用方已开事务，跳过内部事务
     */
    static bool changeBalance(int userId, double delta, double &newBalanceOut,
                              QString &errorMsg, bool useTransaction = true);

    // ===== 工具：密码哈希 =====
    static QString hashPassword(const QString &password);
};

#endif // USERDAO_H
