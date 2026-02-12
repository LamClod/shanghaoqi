cmake_minimum_required(VERSION 3.28)
project(shanghaoqi VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# ── Qt 6 ──
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network Concurrent Test)

# ── Include path: all headers reference from src/ root ──
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

# ============================================================================
# Semantic Kernel (pure Qt Core, no Network dependency at interface level)
# ============================================================================
add_library(semantic STATIC
    src/semantic/failure.cpp
    src/semantic/validate.cpp
    src/semantic/policy.cpp
    src/semantic/processor.cpp
    src/semantic/stream_session.cpp
    src/semantic/features/stream_aggregator.cpp
    src/semantic/features/stream_splitter.cpp
)
target_link_libraries(semantic PUBLIC Qt6::Core Qt6::Network)

# ============================================================================
# Config
# ============================================================================
add_library(config STATIC
    src/config/config_store.cpp
    src/config/model_list_request_builder.cpp
)
target_link_libraries(config PUBLIC Qt6::Core semantic)

# ============================================================================
# Platform
# ============================================================================
set(PLATFORM_SOURCES src/platform/platform_factory.cpp)

if(WIN32)
    list(APPEND PLATFORM_SOURCES
        src/platform/windows/win_cert_manager.cpp
        src/platform/windows/win_hosts_manager.cpp
        src/platform/windows/win_privilege_manager.cpp
    )
elseif(APPLE)
    list(APPEND PLATFORM_SOURCES
        src/platform/macos/mac_cert_manager.cpp
        src/platform/macos/mac_hosts_manager.cpp
        src/platform/macos/mac_privilege_manager.cpp
    )
else()
    list(APPEND PLATFORM_SOURCES
        src/platform/linux/linux_cert_manager.cpp
        src/platform/linux/linux_hosts_manager.cpp
        src/platform/linux/linux_privilege_manager.cpp
    )
endif()

add_library(platform STATIC ${PLATFORM_SOURCES})
target_link_libraries(platform PUBLIC Qt6::Core Qt6::Network)

if(WIN32)
    target_link_libraries(platform PRIVATE shell32 advapi32 crypt32)
endif()

# ============================================================================
# Core (LogManager, Bootstrap)
# ============================================================================
add_library(core STATIC
    src/core/log_manager.cpp
    src/core/bootstrap.cpp
)
target_link_libraries(core PUBLIC Qt6::Core Qt6::Network config platform)

# ============================================================================
# Adapters — Inbound
# ============================================================================
add_library(adapters_inbound STATIC
    src/adapters/inbound/multi_router.cpp
    src/adapters/inbound/openai_chat.cpp
    src/adapters/inbound/openai_responses.cpp
    src/adapters/inbound/anthropic.cpp
    src/adapters/inbound/gemini.cpp
    src/adapters/inbound/aisdk.cpp
    src/adapters/inbound/jina.cpp
    src/adapters/inbound/codex.cpp
    src/adapters/inbound/claudecode.cpp
    src/adapters/inbound/antigravity.cpp
)
target_link_libraries(adapters_inbound PUBLIC Qt6::Core semantic)

# ============================================================================
# Adapters — Outbound
# ============================================================================
add_library(adapters_outbound STATIC
    src/adapters/outbound/multi_router.cpp
    src/adapters/outbound/openai.cpp
    src/adapters/outbound/openai_compat.cpp
    src/adapters/outbound/anthropic.cpp
    src/adapters/outbound/gemini.cpp
    src/adapters/outbound/zai.cpp
    src/adapters/outbound/bailian.cpp
    src/adapters/outbound/modelscope.cpp
    src/adapters/outbound/codex.cpp
    src/adapters/outbound/claudecode.cpp
    src/adapters/outbound/antigravity.cpp
)
target_link_libraries(adapters_outbound PUBLIC Qt6::Core semantic)

# ============================================================================
# Adapters — Executor + Capability
# ============================================================================
add_library(adapters_infra STATIC
    src/adapters/executor/qt_executor.cpp
    src/adapters/capability/static_resolver.cpp
)
target_link_libraries(adapters_infra PUBLIC Qt6::Core Qt6::Network semantic)

# ============================================================================
# Pipeline
# ============================================================================
add_library(pipeline STATIC
    src/pipeline/pipeline.cpp
    src/pipeline/middlewares/auth_middleware.cpp
    src/pipeline/middlewares/model_mapping_middleware.cpp
    src/pipeline/middlewares/stream_mode_middleware.cpp
    src/pipeline/middlewares/debug_middleware.cpp
)
target_link_libraries(pipeline PUBLIC Qt6::Core semantic core)

# ============================================================================
# Proxy Server
# ============================================================================
add_library(proxy STATIC
    src/proxy/proxy_server.cpp
    src/proxy/request_router.cpp
    src/proxy/sse_writer.cpp
    src/proxy/connection_pool.cpp
)
target_link_libraries(proxy PUBLIC Qt6::Core Qt6::Network semantic pipeline config)

# ============================================================================
# UI (Qt Widgets)
# ============================================================================
add_library(ui STATIC
    src/ui/main_widget.cpp
    src/ui/config_group_panel.cpp
    src/ui/runtime_options_panel.cpp
    src/ui/log_panel.cpp
    src/ui/test_result_dialog.cpp
    src/ui/global_settings_page.cpp
)
target_link_libraries(ui PUBLIC Qt6::Core Qt6::Widgets Qt6::Network core config)

# ============================================================================
# Main Executable
# ============================================================================
set(SHANGHAOQI_APP_RESOURCES
    resources/resources.qrc
)

if(WIN32)
    list(APPEND SHANGHAOQI_APP_RESOURCES
        resources/app_icon.rc
    )
endif()

add_executable(shanghaoqi WIN32
    src/main.cpp
    ${SHANGHAOQI_APP_RESOURCES}
)

target_link_libraries(shanghaoqi PRIVATE
    ui
    proxy
    pipeline
    adapters_inbound
    adapters_outbound
    adapters_infra
    core
    config
    platform
    semantic
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    Qt6::Concurrent
)

# ============================================================================
# Tests
# ============================================================================
enable_testing()

function(add_shanghaoqi_test TEST_NAME TEST_SOURCE)
    add_executable(${TEST_NAME} ${TEST_SOURCE})
    target_link_libraries(${TEST_NAME} PRIVATE
        semantic
        adapters_inbound
        adapters_outbound
        adapters_infra
        pipeline
        proxy
        config
        core
        platform
        Qt6::Core
        Qt6::Network
        Qt6::Test
    )
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endfunction()

add_shanghaoqi_test(tst_validate      tests/tst_validate.cpp)
add_shanghaoqi_test(tst_aggregator    tests/tst_aggregator.cpp)
add_shanghaoqi_test(tst_splitter      tests/tst_splitter.cpp)
add_shanghaoqi_test(tst_policy        tests/tst_policy.cpp)
add_shanghaoqi_test(tst_pipeline      tests/tst_pipeline.cpp)
add_shanghaoqi_test(tst_sse_parser    tests/tst_sse_parser.cpp)
add_shanghaoqi_test(tst_routers       tests/tst_routers.cpp)
add_shanghaoqi_test(tst_openai_roundtrip     tests/tst_openai_roundtrip.cpp)
add_shanghaoqi_test(tst_anthropic_roundtrip  tests/tst_anthropic_roundtrip.cpp)
add_shanghaoqi_test(tst_config_store  tests/tst_config_store.cpp)
add_shanghaoqi_test(tst_hosts_manager tests/tst_hosts_manager.cpp)
add_shanghaoqi_test(tst_cert_manager  tests/tst_cert_manager.cpp)

# ── Install ──
install(TARGETS shanghaoqi RUNTIME DESTINATION bin)
