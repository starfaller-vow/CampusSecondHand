# 校园二手交易系统：第 15 次提交——交易统计与支付确认修复

负责人：黄广淼。本目录为累计源码快照，前置功能、新增功能与边界见本次提交说明。

本次将管理员“平台统计概览”的“交易总数”替换为四项：成功交易数（done）、交易中数量（paid）、已取消数量（cancelled）、已退款数量（refunded）。统计按交易当前状态计算，确认收货或超时自动结算后才计入成功交易。服务端和客户端需一起重新编译并更新。

支付确认已修复商品名及金额错位，并分区显示应付金额、余额、支付后余额和卖家手续费。余额不足时禁用支付；手续费由卖家结算时承担，买家无需另付。每次确认前刷新数据，服务端校验用户确认的价格。

使用 VS2026 的“打开文件夹”打开本目录，保持与原项目相同的 Qt/C++ 开发环境。三份 CMake 配置文件已从原 RAR 原样恢复，使用 Ninja、x64，提供 Qt-Debug 和 Qt-Release 预设，输出目录分别为 out/build/debug 和 out/build/release。

Qt 安装路径保留在 CMakeUserPresets.json 的 QT653 预设的 QTDIR 环境变量中：D:/QT/6.5.3/msvc2019_64。

先启动 CampusServer.exe，再启动 CampusSecondHand.exe，默认连接 127.0.0.1:9527。服务端在其可执行文件旁生成数据库和图片目录；阶段源码包没有加入原测试数据库。

预置演示管理员：admin / admin123。第 01 阶段只提供连接框架，第 02 阶段起开放登录。

在 VS x64 开发者命令提示符（需可找到 cmake 和 ninja）中，可运行：

```text
cmake --preset Qt-Release
cmake --build out/build/release
```

原配置没有 buildPresets，因此这里不使用 cmake --build --preset。切换 Debug 时改用 Qt-Debug 和 out/build/debug。

