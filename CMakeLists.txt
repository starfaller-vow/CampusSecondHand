cmake_minimum_required(VERSION 3.16)
project(CampusSecondHand VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ========== Qt6 ==========
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Sql
    Network
)

# ========== 共享源（两端都编译：模型 + 协议/序列化/密码工具）==========
file(GLOB_RECURSE SHARED_SOURCES CONFIGURE_DEPENDS
    src/models/*.h src/models/*.cpp
    src/shared/*.h src/shared/*.cpp
)

# ========== 数据/业务源（服务端专属：SQLite 与业务逻辑全部在服务端）==========
file(GLOB_RECURSE BIZ_SOURCES CONFIGURE_DEPENDS
    src/db/*.h src/db/*.cpp
    src/dao/*.h src/dao/*.cpp
    src/manager/*.h src/manager/*.cpp
)

# ========== 服务端源 ==========
file(GLOB_RECURSE SERVER_SOURCES CONFIGURE_DEPENDS
    server/*.h server/*.cpp
)

# ========== 客户端 UI 源 ==========
file(GLOB_RECURSE UI_SOURCES CONFIGURE_DEPENDS
    src/ui/*.h src/ui/*.cpp
)

# ============================================================
# 客户端（桌面窗口程序，保留原工程名；纯 UI + 网络，无数据库）
# ============================================================
add_executable(CampusSecondHand WIN32
    main.cpp
    ${UI_SOURCES}
    ${SHARED_SOURCES}
)
target_include_directories(CampusSecondHand PRIVATE
    src
    src/models
    src/ui
    src/shared
)
target_link_libraries(CampusSecondHand PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
)

# ============================================================
# 服务端（控制台程序，输出运行日志）
# ============================================================
add_executable(CampusServer
    ${SERVER_SOURCES}
    ${SHARED_SOURCES}
    ${BIZ_SOURCES}
)
target_include_directories(CampusServer PRIVATE
    src
    src/db
    src/models
    src/dao
    src/manager
    src/shared
    server
)
target_link_libraries(CampusServer PRIVATE
    Qt6::Core
    Qt6::Sql
    Qt6::Network
)

# ============================================================
# 运行时文件拷贝（init.sql/words.txt 数据归服务端，客户端不再需要）
# ============================================================
add_custom_command(TARGET CampusServer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/init.sql"
        "$<TARGET_FILE_DIR:CampusServer>/init.sql"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/words.txt"
        "$<TARGET_FILE_DIR:CampusServer>/words.txt"
)