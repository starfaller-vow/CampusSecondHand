# 校园二手交易系统：09 管理员举报审核

负责人：黄广淼。本目录为累计源码快照，前置功能、新增功能与边界见本次提交说明。

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
