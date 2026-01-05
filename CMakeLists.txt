# ShangHaoQi 上号器 - 本地 HTTPS 代理服务器
#
# 项目描述：
#   基于 Qt6 的跨平台本地 HTTPS 代理服务器，用于 API 请求拦截和转发
#   采用抽象工厂模式和依赖倒置原则实现跨平台支持
#
# 支持平台：
#   - Windows 完整支持
#   - macOS 计划中
#   - Linux 计划中
#
# ShangHaoQi Team
# Version 2.0.0

cmake_minimum_required(VERSION 3.16)
project(ShangHaoQi VERSION 2.0.0 LANGUAGES CXX)

# 编译器设置

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 依赖查找

# Qt6
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network Concurrent)

# OpenSSL
find_package(OpenSSL REQUIRED)

# yaml-cpp
find_package(yaml-cpp REQUIRED)

# FluxFix 整流中间件库
set(FLUXFIX_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../FluxFix")
set(FLUXFIX_INCLUDE_DIR "${FLUXFIX_ROOT}/include")
set(FLUXFIX_LIB_DIR "${FLUXFIX_ROOT}/target/release")

# 平台接口：跨平台抽象层

set(PLATFORM_INTERFACE_HEADERS
    src/platform/interfaces/i_config_manager.h
    src/platform/interfaces/i_cert_manager.h
    src/platform/interfaces/i_privilege_manager.h
    src/platform/interfaces/i_hosts_manager.h
    src/platform/platform_factory.h
)

set(PLATFORM_INTERFACE_SOURCES
    src/platform/platform_factory.cpp
)

# 平台实现：根据目标平台选择

if(WIN32)
    # Windows 平台实现
    set(PLATFORM_IMPL_HEADERS
        src/platform/windows/windows_config_manager.h
        src/platform/windows/windows_cert_manager.h
        src/platform/windows/windows_privilege_manager.h
        src/platform/windows/windows_hosts_manager.h
        src/platform/windows/windows_platform_factory.h
    )
    set(PLATFORM_IMPL_SOURCES
        src/platform/windows/windows_config_manager.cpp
        src/platform/windows/windows_cert_manager.cpp
        src/platform/windows/windows_privilege_manager.cpp
        src/platform/windows/windows_hosts_manager.cpp
        src/platform/windows/windows_platform_factory.cpp
    )
elseif(APPLE)
    # macOS 平台实现
    set(PLATFORM_IMPL_HEADERS
        src/platform/macos/macos_config_manager.h
        src/platform/macos/macos_cert_manager.h
        src/platform/macos/macos_privilege_manager.h
        src/platform/macos/macos_hosts_manager.h
        src/platform/macos/macos_platform_factory.h
    )
    set(PLATFORM_IMPL_SOURCES
        src/platform/macos/macos_config_manager.cpp
        src/platform/macos/macos_cert_manager.cpp
        src/platform/macos/macos_privilege_manager.cpp
        src/platform/macos/macos_hosts_manager.cpp
        src/platform/macos/macos_platform_factory.cpp
    )
elseif(UNIX AND NOT APPLE)
    # Linux 平台实现
    set(PLATFORM_IMPL_HEADERS
        src/platform/linux/linux_config_manager.h
        src/platform/linux/linux_cert_manager.h
        src/platform/linux/linux_privilege_manager.h
        src/platform/linux/linux_hosts_manager.h
        src/platform/linux/linux_platform_factory.h
    )
    set(PLATFORM_IMPL_SOURCES
        src/platform/linux/linux_config_manager.cpp
        src/platform/linux/linux_cert_manager.cpp
        src/platform/linux/linux_privilege_manager.cpp
        src/platform/linux/linux_hosts_manager.cpp
        src/platform/linux/linux_platform_factory.cpp
    )
endif()

# 核心模块：跨平台

# 核心接口：纯 C++
set(CORE_INTERFACE_HEADERS
    src/core/interfaces/i_log_manager.h
    src/core/interfaces/i_network_manager.h
)

# 核心类型定义：纯 C++
set(CORE_HEADERS
    src/core/operation_result.h
)

# 标准 C++ 实现
set(CORE_STD_HEADERS
    src/core/std/fluxfix_wrapper.h
)

set(CORE_STD_SOURCES
    src/core/std/fluxfix_wrapper.cpp
)

# Qt 实现
set(CORE_QT_HEADERS
    src/core/qt/log_manager.h
    src/core/qt/network_manager.h
    src/core/qt/fluxfix_wrapper.h
)

set(CORE_QT_SOURCES
    src/core/qt/log_manager.cpp
    src/core/qt/network_manager.cpp
    src/core/qt/fluxfix_wrapper.cpp
)

# UI 模块

set(UI_HEADERS
    src/ui/main_widget.h
    src/ui/log_panel.h
    src/ui/config_group_panel.h
    src/ui/test_result_dialog.h
    src/ui/runtime_options_panel.h
)

set(UI_SOURCES
    src/ui/main_widget.cpp
    src/ui/log_panel.cpp
    src/ui/config_group_panel.cpp
    src/ui/test_result_dialog.cpp
    src/ui/runtime_options_panel.cpp
)

# 汇总所有源文件

set(ALL_SOURCES
    src/main.cpp
    ${PLATFORM_INTERFACE_HEADERS}
    ${PLATFORM_INTERFACE_SOURCES}
    ${PLATFORM_IMPL_HEADERS}
    ${PLATFORM_IMPL_SOURCES}
    ${CORE_INTERFACE_HEADERS}
    ${CORE_HEADERS}
    ${CORE_STD_HEADERS}
    ${CORE_STD_SOURCES}
    ${CORE_QT_HEADERS}
    ${CORE_QT_SOURCES}
    ${UI_HEADERS}
    ${UI_SOURCES}
)

# Windows 资源文件

if(WIN32)
    set(APP_ICON_RC "${CMAKE_CURRENT_SOURCE_DIR}/resources/app_icon.rc")
    list(APPEND ALL_SOURCES ${APP_ICON_RC})
endif()

# 可执行文件

add_executable(${PROJECT_NAME} ${ALL_SOURCES})

# 编译警告设置
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /WX-)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()

# 包含目录
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/interfaces
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/qt
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/src/platform
    ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/interfaces
    ${FLUXFIX_INCLUDE_DIR}
)

# 链接库
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    Qt6::Concurrent
    OpenSSL::SSL
    OpenSSL::Crypto
    yaml-cpp::yaml-cpp
    ${FLUXFIX_LIB_DIR}/fluxfix.dll.lib
)

# 平台特定设置

if(WIN32)
    # Windows: 链接 crypt32 库用于 DPAPI 和证书操作
    target_link_libraries(${PROJECT_NAME} PRIVATE crypt32)

    # Windows: 设置为 GUI 应用程序不显示控制台窗口
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
elseif(APPLE)
    # macOS: 链接 Security 和 CoreFoundation 框架
    target_link_libraries(${PROJECT_NAME} PRIVATE
        "-framework Security"
        "-framework CoreFoundation"
    )
elseif(UNIX AND NOT APPLE)
    # Linux: 可选链接 libsecret（如果可用）
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(LIBSECRET libsecret-1)
        if(LIBSECRET_FOUND)
            target_include_directories(${PROJECT_NAME} PRIVATE ${LIBSECRET_INCLUDE_DIRS})
            target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBSECRET_LIBRARIES})
            target_compile_definitions(${PROJECT_NAME} PRIVATE HAVE_LIBSECRET)
        endif()
    endif()
endif()

# 测试

enable_testing()
add_subdirectory(tests)

# 复制 FluxFix DLL 到输出目录
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${FLUXFIX_LIB_DIR}/fluxfix.dll"
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
    COMMENT "Copying fluxfix.dll to output directory"
)

# 安装规则：可选

# install(TARGETS ${PROJECT_NAME}
#     RUNTIME DESTINATION bin
#     LIBRARY DESTINATION lib
#     ARCHIVE DESTINATION lib
# )
