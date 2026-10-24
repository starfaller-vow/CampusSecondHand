#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <QByteArray>
#include <QJsonObject>
#include <QList>
#include <QPair>
#include <QString>
#include <QtGlobal>

// ============================================================
// C/S 通信协议公共定义（客户端与服务端共享）
//
// 帧格式（解决 TCP 粘包/半包）：
//   [帧类型:1字节][负载长度:4字节 大端][负载]
// · 帧类型 0x01 = JSON 文本帧（请求/响应/推送）
// · 帧类型 0x02 = 图片二进制帧（图片上传/下载，M5 使用）
//
// JSON 请求： { "reqId":u32, "cmd":"auth.login", "token":"", "params":{...} }
// JSON 响应： { "reqId":u32, "ok":bool, "data":{...}, "err":"..." }
// JSON 推送： { "push":"message.new", "data":{...} }   // 无 reqId
// ============================================================

namespace cs {

// 单帧最大负载（防恶意长度：32MB）
constexpr quint32 MaxPayload = 32u * 1024u * 1024u;

enum class FrameType : quint8 {
    Json  = 0x01, // JSON 文本帧
    Image = 0x02  // 图片二进制帧
};

// ---------------- 命令名常量 ----------------
namespace cmd {
inline const char *Login            = "auth.login";
inline const char *Register         = "auth.register";

inline const char *Self             = "user.self";
inline const char *UserGet          = "user.get";          // 用户公开信息（交易对端显示）
inline const char *UpdateContact    = "user.updateContact";
inline const char *ChangePassword   = "user.changePassword";
inline const char *ListUsers        = "user.listAll";
inline const char *BanUser          = "user.ban";
inline const char *UnbanUser        = "user.unban";
inline const char *DeleteUser       = "user.delete";
inline const char *SuggestPrice     = "user.suggestPrice";

inline const char *ListCategories   = "category.list";
inline const char *CategoryAdd      = "category.add";
inline const char *CategoryUpdate   = "category.update";
inline const char *CategoryRemove   = "category.remove";

inline const char *ListGoods        = "goods.list";       // 浏览/搜索
inline const char *GoodsDetail      = "goods.detail";
inline const char *MineGoods        = "goods.mine";
inline const char *PublishGoods     = "goods.publish";
inline const char *EditGoods        = "goods.edit";
inline const char *DeleteGoods      = "goods.delete";
inline const char *OffShelf         = "goods.offshelf";
inline const char *OnShelf          = "goods.onshelf";
inline const char *ForceOffReport   = "goods.forceOffReport";   // 管理员举报下架
inline const char *RestoreReport    = "goods.restoreReport";    // 管理员恢复上架

inline const char *FavoriteList     = "favorite.list";
inline const char *FavoriteAdd      = "favorite.add";
inline const char *FavoriteRemove   = "favorite.remove";

inline const char *MessageList      = "message.list";
inline const char *MessageMarkRead  = "message.markRead";
inline const char *MessageReadAll   = "message.readAll";
inline const char *MessageUnread    = "message.unread";
inline const char *ContactAdmin     = "message.contactAdmin"; // 联系管理员（反馈直达管理员消息箱）

inline const char *ReportSubmit     = "report.submit";
inline const char *ReportList       = "report.list";
inline const char *ReportHandle     = "report.handle";     // resolved / rejected / refunded

inline const char *RedeemGenerate   = "redeem.generate";
inline const char *Redeem           = "redeem.redeem";

inline const char *BalanceLogs      = "balance.list";
inline const char *SettingsGet      = "settings.get";
inline const char *SettingsSet      = "settings.set";

inline const char *Purchase         = "tx.purchase";
inline const char *CancelTx         = "tx.cancel";
inline const char *ConfirmTx        = "tx.confirm";
inline const char *MyBuys           = "tx.myBuys";
inline const char *MySales          = "tx.mySales";
inline const char *AdminPaidTx      = "tx.adminPaid";
inline const char *AdminRefundedTx  = "tx.adminRefunded";
inline const char *AdminRefund      = "tx.adminRefund";

inline const char *AdminStats       = "admin.stats";

inline const char *ImageUpload      = "image.upload";
inline const char *ImageUploadDone  = "image.uploadDone";  // 图片帧发完后校验
inline const char *ImageDownload    = "image.download";

} // namespace cmd

// ---------------- 推送事件名（服务端 -> 客户端，无 reqId） ----------------
namespace push {
inline const char *Message   = "message.new";    // 来新消息，data:{unread:int}
inline const char *Kick      = "kick";           // 被管理员封禁踢下线，data:{reason:"..."}
inline const char *GoodsChg  = "goods.changed";  // 商品被举报下架/恢复，data:{goodsId:int, status:"off_shelf"}
} // namespace push

} // namespace cs

// ---------------- 帧编解码工具 ----------------

// 组装一帧：[类型1][长度4大端][负载]
QByteArray makeFrame(cs::FrameType type, const QByteArray &payload);

// 组装 JSON 帧（QJsonDocument::Compact 序列化）
QByteArray makeJsonFrame(const QJsonObject &obj);

// 累积式帧解析器：append 传入 socket 新收到的字节，
// 通过 outFrames 返回所有已解析完整的帧（处理粘包/半包）
class FrameReader
{
public:
    // @param data     新收到的字节流（可能为空）
    // @param outFrames 输出：解析出的完整帧列表 (帧类型, 负载)
    // @param err       出错信息（长度超限/帧类型非法等）
    // @return 出错返回 false，之后此 reader 应被丢弃（连接应断开）
    bool append(const QByteArray &data,
                QList<QPair<cs::FrameType, QByteArray>> &outFrames,
                QString &err);

private:
    QByteArray m_buf;
};

#endif // PROTOCOL_H