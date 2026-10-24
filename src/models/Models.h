#ifndef MODELS_H
#define MODELS_H

#include <QString>
#include <QStringList>
#include <QDateTime>

// ============== 用户 ==============
struct User
{
    int id = 0;
    QString username;
    QString passwordHash;
    QString role;       // 'user' / 'admin'
    QString studentNo;  // 10位学号
    QString college;
    QString contact;
    double balance = 0;
    QString status;     // 'active' / 'banned' / 'deleted'
    QString createTime;

    bool isActive() const { return status == "active"; }//活跃
    bool isBanned()  const { return status == "banned"; }//封禁
    bool isDeleted() const { return status == "deleted"; }//注销
    bool isAdmin()   const { return role == "admin"; }
};

// ============== 分类 ==============
struct Category
{
    int id = 0;
    QString name;
    int sortOrder = 0;
};

// ============== 商品 ==============
struct Goods
{
    int id = 0;
    int sellerId = 0;
    QString title;
    QString description;
    double originalPrice = 0;
    QString condition;      // 全新/九成新/八成新/七成新/六成新及以下
    double currentPrice = 0;
    int categoryId = 0;
    QString imagesJson;     // JSON数组字符串
    QString status;         // on_sale / reserved / sold / off_shelf
    QString offReason;      // 下架原因：''=普通（主动/封禁）下架，'report'=被举报下架（卖家不可自行上架）
    QString createTime;
    QString updateTime;

    // 仅展示用（非数据库字段）：卡片上直接显示，服务端随商品 JSON 一起下发
    QString categoryName;   // 分类名
    QString sellerName;     // 卖家用户名

    // 辅助：把 imagesJson 转成 QStringList
    QStringList imageList() const;

    bool isOnSale()    const { return status == "on_sale"; }
    bool isReserved()  const { return status == "reserved"; }
    bool isSold()      const { return status == "sold"; }
    bool isOffShelf()  const { return status == "off_shelf"; }
    bool isReportOff() const { return status == "off_shelf" && offReason == "report"; }
};

// ============== 交易 ==============
struct Transaction
{
    int id = 0;
    int buyerId = 0;
    int sellerId = 0;
    int goodsId = 0;
    QString status;         // pending / paid / done / cancelled / refunded
    QString remark;
    double feeAmount = 0;
    int feeRefunded = 0;
    QString createTime;
    QString finishTime;

    // 仅展示用（非数据库字段）：列表接口一次性附带下发，避免客户端逐行 N+1 查询
    QString goodsTitle;         // 商品标题
    double goodsPrice = 0;      // 成交时价格（取商品现价）
    bool goodsExists = false;   // 商品是否仍存在（可能被删除）
    QString goodsStatus;        // 商品当前状态（管理员视图，用于举报下架判断）
    QString goodsOffReason;     // 商品下架原因（管理员视图）
    QString peerName;           // 交易对端用户名（买家视角=卖家，卖家视角=买家）
    QString peerStudentNo;      // 对端学号
    QString buyerName;          // 管理员视图：买家用户名
    QString buyerStudentNo;
    QString sellerName;         // 管理员视图：卖家用户名
    QString sellerStudentNo;
};

// ============== 收藏 ==============
struct Favorite
{
    int id = 0;
    int userId = 0;
    int goodsId = 0;
    QString createTime;
};

// ============== 消息 ==============
struct Message
{
    int id = 0;
    int userId = 0;
    QString title;
    QString content;
    int isRead = 0;
    QString type;       // system / trade / report
    QString createTime;

    bool read() const { return isRead == 1; }
};

// ============== 充值码 ==============
struct RedeemCode
{
    int id = 0;
    QString code;
    double amount = 0;
    int used = 0;
    int usedBy = 0;
    QString usedTime;
    QString createTime;
};

// ============== 余额流水 ==============
struct BalanceLog
{
    int id = 0;
    int userId = 0;
    QString type;           // recharge / consume / sale / refund / fee_deduct / fee_return
    double amount = 0;      // 正=加钱，负=扣钱
    double balanceAfter = 0;
    QString refId;
    QString remark;
    QString createTime;
};

// ============== 举报 ==============
struct Report
{
    int id = 0;
    int reporterId = 0;
    QString targetType;     // goods / user / transaction
    int targetId = 0;
    QString reasonType;     // 虚假商品 / 描述不符 / 交易纠纷 / 欺诈行为 / 其他
    QString description;
    QString status;         // pending / resolved / rejected / refunded
    int handlerId = 0;
    QString handlerNote;
    QString createTime;
    QString handleTime;

    // 仅展示用（非数据库字段）：举报列表附带被举报商品信息，避免客户端逐行查询
    QString targetTitle;
    bool targetExists = false;     // 被举报商品是否仍存在
    QString targetStatus;          // 被举报商品状态（on_sale/off_shelf/sold）
    QString targetOffReason;       // 被举报商品下架原因
};

#endif // MODELS_H
