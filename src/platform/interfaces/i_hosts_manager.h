/**
 * @file i_hosts_manager.h
 * @brief Hosts 文件管理器接口定义
 * 
 * 本文件定义了跨平台的系统 hosts 文件管理抽象接口。
 * 
 * @details
 * 功能概述：
 * 
 * Hosts 文件管理器负责管理系统的 hosts 文件，实现 DNS 劫持功能。
 * 通过修改 hosts 文件，将指定域名解析到本地代理服务器。
 * 
 * 平台实现：
 * 
 * | 平台         | Hosts 文件路径                              |
 * |--------------|---------------------------------------------|
 * | Windows      | C:\Windows\System32\drivers\etc\hosts       |
 * | macOS/Linux  | /etc/hosts                                  |
 * 
 * 功能模块：
 * 
 * 1. 条目管理
 *    - addEntry(): 添加 DNS 劫持条目
 *    - removeEntry(): 删除 DNS 劫持条目
 *    - hasEntry(): 检查条目是否存在
 * 
 * 2. 备份/恢复
 *    - backup(): 备份当前 hosts 文件
 *    - restore(): 从备份恢复 hosts 文件
 * 
 * 3. 编辑器
 *    - openInEditor(): 使用系统编辑器打开 hosts 文件
 * 
 * 4. 域名配置
 *    - hijackDomains(): 获取要劫持的域名列表
 *    - setHijackDomains(): 设置要劫持的域名列表
 * 
 * @note 所有写操作需要管理员/root 权限
 * @note 实现类应在修改前自动备份 hosts 文件
 * 
 * @copyright Copyright (c) 2026 LAMCLOD. All rights reserved.
 * @author LAMCLOD
 * @version 2.0.0
 */

#ifndef I_HOSTS_MANAGER_H
#define I_HOSTS_MANAGER_H

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include "../../core/interfaces/i_log_manager.h"

/**
 * @brief Hosts 文件管理器接口
 * 
 * 纯抽象接口，定义系统 hosts 文件管理的标准操作。
 * 不依赖任何 GUI 框架。
 * 
 * @note 所有写操作需要管理员/root 权限
 * @note 实现类应在修改前自动备份 hosts 文件
 */
class IHostsManager {
public:
    virtual ~IHostsManager() = default;
    
    // 禁止拷贝
    IHostsManager(const IHostsManager&) = delete;
    IHostsManager& operator=(const IHostsManager&) = delete;
    
    // ========== 条目管理 ==========
    
    /**
     * @brief 添加 DNS 劫持条目
     * 
     * 将预定义的域名劫持条目添加到 hosts 文件。
     * 添加的条目将域名指向 127.0.0.1 和 ::1。
     * 
     * @return true 表示添加成功
     * 
     * @note 如果条目已存在，应跳过添加并返回成功
     * @note 添加前会自动备份 hosts 文件
     */
    virtual bool addEntry() = 0;
    
    /**
     * @brief 删除 DNS 劫持条目
     * 
     * 从 hosts 文件中移除所有由本程序添加的条目。
     * 
     * @return true 表示删除成功
     */
    virtual bool removeEntry() = 0;
    
    /**
     * @brief 检查是否已添加条目
     * 
     * @return true 表示 hosts 文件中已存在劫持条目
     */
    [[nodiscard]] virtual bool hasEntry() const = 0;
    
    // ========== 备份/恢复 ==========
    
    /**
     * @brief 备份 hosts 文件
     * 
     * 将当前 hosts 文件内容备份到指定目录。
     * 
     * @return true 表示备份成功
     */
    virtual bool backup() = 0;
    
    /**
     * @brief 恢复 hosts 文件
     * 
     * 从备份文件恢复 hosts 文件内容。
     * 
     * @return true 表示恢复成功
     */
    virtual bool restore() = 0;
    
    // ========== 编辑器 ==========
    
    /**
     * @brief 使用系统编辑器打开 hosts 文件
     * 
     * 启动系统默认的文本编辑器打开 hosts 文件。
     * - Windows: notepad.exe
     * - macOS: TextEdit 或 open -e
     * - Linux: xdg-open 或 gedit
     * 
     * @return true 表示成功启动编辑器
     */
    virtual bool openInEditor() = 0;
    
    // ========== 路径信息 ==========
    
    /**
     * @brief 获取 hosts 文件路径
     * 
     * @return 系统 hosts 文件的完整路径
     */
    [[nodiscard]] virtual std::string hostsFilePath() const = 0;
    
    /**
     * @brief 获取备份文件路径
     * 
     * @return hosts 备份文件的完整路径
     */
    [[nodiscard]] virtual std::string backupFilePath() const = 0;
    
    // ========== 域名配置 ==========
    
    /**
     * @brief 获取要劫持的域名列表
     * 
     * @return 域名列表
     */
    [[nodiscard]] virtual std::vector<std::string> hijackDomains() const = 0;
    
    /**
     * @brief 设置要劫持的域名列表
     * 
     * @param domains 域名列表
     */
    virtual void setHijackDomains(const std::vector<std::string>& domains) = 0;
    
    // ========== 回调设置 ==========
    
    /**
     * @brief 设置日志回调
     * 
     * @param callback 日志回调函数
     */
    virtual void setLogCallback(LogCallback callback) = 0;

protected:
    IHostsManager() = default;
};

/// Hosts 管理器智能指针类型
using IHostsManagerPtr = std::unique_ptr<IHostsManager>;

#endif // I_HOSTS_MANAGER_H
