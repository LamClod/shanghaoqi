/**
 * @file platform_factory.cpp
 * @brief 平台工厂创建器实现文件
 * 
 * 本文件实现了 PlatformFactory 静态类的所有方法。
 * 
 * @details
 * 实现内容：
 * 
 * 1. 平台检测
 *    - 使用编译时宏检测当前平台
 *    - 支持 Windows、macOS、Linux
 * 
 * 2. 工厂创建
 *    - create(): 创建当前平台的工厂实例
 *    - 根据平台宏选择对应的具体工厂类
 * 
 * 3. 平台信息
 *    - currentPlatform(): 获取当前平台名称
 *    - isPlatformSupported(): 检查平台是否支持
 * 
 * 平台支持状态：
 * 
 * | 平台    | 状态     | 说明                     |
 * |---------|----------|--------------------------|
 * | Windows | 已实现   | 完整支持                 |
 * | macOS   | 待实现   | 抛出异常                 |
 * | Linux   | 待实现   | 抛出异常                 |
 * 
 * @copyright Copyright (c) 2026 LAMCLOD. All rights reserved.
 * @author LAMCLOD
 * @version 2.0.0
 */

#include "platform_factory.h"
#include <stdexcept>

// 平台检测宏
#if defined(_WIN32) || defined(_WIN64)
    #define PLATFORM_WINDOWS 1
#elif defined(__APPLE__) && defined(__MACH__)
    #define PLATFORM_MACOS 1
#elif defined(__linux__)
    #define PLATFORM_LINUX 1
#endif

// 根据平台包含对应的工厂头文件
#ifdef PLATFORM_WINDOWS
#include "windows/windows_platform_factory.h"
#elif defined(PLATFORM_MACOS)
#include "macos/macos_platform_factory.h"
#elif defined(PLATFORM_LINUX)
#include "linux/linux_platform_factory.h"
#endif

// ============================================================================
// 工厂创建
// ============================================================================

IPlatformFactoryPtr PlatformFactory::create()
{
#ifdef PLATFORM_WINDOWS
    return std::make_unique<WindowsPlatformFactory>();
#elif defined(PLATFORM_MACOS)
    return std::make_unique<MacOSPlatformFactory>();
#elif defined(PLATFORM_LINUX)
    return std::make_unique<LinuxPlatformFactory>();
#else
    throw std::runtime_error("Unsupported platform");
#endif
}

// ============================================================================
// 平台信息
// ============================================================================

std::string PlatformFactory::currentPlatform()
{
#ifdef PLATFORM_WINDOWS
    return "Windows";
#elif defined(PLATFORM_MACOS)
    return "macOS";
#elif defined(PLATFORM_LINUX)
    return "Linux";
#else
    return "Unknown";
#endif
}

bool PlatformFactory::isPlatformSupported()
{
#ifdef PLATFORM_WINDOWS
    return true;
#elif defined(PLATFORM_MACOS)
    return true;
#elif defined(PLATFORM_LINUX)
    return true;
#else
    return false;
#endif
}
