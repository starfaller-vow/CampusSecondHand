#ifndef PASSWORDUTIL_H
#define PASSWORDUTIL_H

#include <QString>

// 密码哈希公共工具（客户端与服务端共享同一实现，保证两边结果一致）
namespace passutil {

// 对明文密码做 SHA256 并转成小写十六进制字符串
// 注意：C/S 架构下，哈希只在【客户端】执行；服务端直接拿"哈希值"与库中 password_hash 比对，
// 绝不二次哈希（否则与单机版老库中的单哈希不兼容）
QString sha256Hex(const QString &plain);

} // namespace passutil

#endif // PASSWORDUTIL_H