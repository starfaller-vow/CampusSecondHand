#ifndef DBLOCK_H
#define DBLOCK_H

#include <QReadWriteLock>

// 全局数据库读写锁：
// 服务端每个工作线程持有独立 SQLite 连接，配合 WAL：
//   - 只读请求（查询/详情/列表/统计）持【读锁】，多连接可并发执行；
//   - 写请求（注册/下单/改余额/状态流转等，含事务）持【写锁】，全局独占。
// 所有请求的业务处理（含事务）必须【全程持锁】执行，处理完再释放。
QReadWriteLock &serverDbLock();

#endif // DBLOCK_H
