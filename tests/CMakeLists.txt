# ShangHaoQi 测试配置
# 
# 使用 Qt Test 框架进行单元测试
# 
# @author ShangHaoQi Team
# @version 2.0.0

cmake_minimum_required(VERSION 3.16)

find_package(Qt6 REQUIRED COMPONENTS Core Test Network Concurrent)
find_package(yaml-cpp REQUIRED)
find_package(OpenSSL REQUIRED)

enable_testing()

# 公共包含目录
set(TEST_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/interfaces
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/qt
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/platform
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/platform/interfaces
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/platform/windows
)

# ============================================================================
# 测试：PlatformFactory（纯 C++ 接口测试）
# ============================================================================

add_executable(test_platform_factory 
    test_platform_factory.cpp
    ../src/platform/platform_factory.cpp
    ../src/platform/windows/windows_platform_factory.cpp
    ../src/platform/windows/windows_config_manager.cpp
    ../src/platform/windows/windows_cert_manager.cpp
    ../src/platform/windows/windows_privilege_manager.cpp
    ../src/platform/windows/windows_hosts_manager.cpp
    ../src/core/qt/log_manager.cpp
)

target_include_directories(test_platform_factory PRIVATE ${TEST_INCLUDE_DIRS})

target_link_libraries(test_platform_factory PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Concurrent
    yaml-cpp::yaml-cpp
    OpenSSL::SSL
    OpenSSL::Crypto
)

if(WIN32)
    target_link_libraries(test_platform_factory PRIVATE crypt32)
endif()

add_test(NAME PlatformFactoryTest COMMAND test_platform_factory)

# ============================================================================
# 测试：WindowsConfigManager
# ============================================================================

add_executable(test_config_manager 
    test_config_manager.cpp
    ../src/platform/windows/windows_config_manager.cpp
    ../src/core/qt/log_manager.cpp
)

target_include_directories(test_config_manager PRIVATE ${TEST_INCLUDE_DIRS})

target_link_libraries(test_config_manager PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Concurrent
    yaml-cpp::yaml-cpp
)

if(WIN32)
    target_link_libraries(test_config_manager PRIVATE crypt32)
endif()

add_test(NAME ConfigManagerTest COMMAND test_config_manager)

# ============================================================================
# 测试：LogManager
# ============================================================================

add_executable(test_log_manager 
    test_log_manager.cpp
    ../src/core/qt/log_manager.cpp
)

target_include_directories(test_log_manager PRIVATE ${TEST_INCLUDE_DIRS})

target_link_libraries(test_log_manager PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME LogManagerTest COMMAND test_log_manager)

# ============================================================================
# 测试：WindowsHostsManager
# ============================================================================

add_executable(test_hosts_manager 
    test_hosts_manager.cpp
    ../src/platform/windows/windows_hosts_manager.cpp
    ../src/core/qt/log_manager.cpp
)

target_include_directories(test_hosts_manager PRIVATE ${TEST_INCLUDE_DIRS})

target_link_libraries(test_hosts_manager PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME HostsManagerTest COMMAND test_hosts_manager)

# ============================================================================
# 测试：NetworkUtils（数据结构和工具函数）
# ============================================================================

add_executable(test_network_utils
    test_network_utils.cpp
)

target_include_directories(test_network_utils PRIVATE ${TEST_INCLUDE_DIRS})

target_link_libraries(test_network_utils PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Network
)

add_test(NAME NetworkUtilsTest COMMAND test_network_utils)
