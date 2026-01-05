/**
 * @file windows_hosts_manager.cpp
 * @brief Windows Hosts 文件管理器实现
 * 
 * 实现 IHostsManager 接口，管理 Windows 系统的 hosts 文件。
 * 
 * @details
 * 实现细节：
 * - 编码检测：通过 BOM 和字节序列分析自动检测文件编码
 * - 编码保持：读取和写入时保持原有编码格式
 * - 条目管理：使用标记块管理添加的条目，便于识别和清理
 * - 备份机制：修改前自动备份到指定目录
 * 
 * 支持的编码格式：
 * - UTF-8（带或不带 BOM）
 * - UTF-16LE（带 BOM）
 * - UTF-16BE（带 BOM）
 * - GBK（简体中文 Windows 默认编码）
 * 
 * 条目操作流程：
 * 1. addEntry: 备份 -> 读取 -> 检查存在 -> 追加 -> 写入
 * 2. removeEntry: 读取 -> 正则匹配删除 -> 写入
 * 3. hasEntry: 读取 -> 检查所有域名是否存在
 * 
 * @copyright Copyright (c) 2026 LAMCLOD. All rights reserved.
 * @author LAMCLOD
 * @version 2.0.0
 */

#include "windows_hosts_manager.h"

// Qt 文件和目录操作
#include <QDir>              // 目录操作
#include <QFile>             // 文件读写
#include <QFileInfo>         // 文件信息查询

// Qt 进程管理
#include <QProcess>          // 启动外部进程（记事本）

// Qt 正则表达式
#include <QRegularExpression>  // 正则表达式匹配和替换

// Qt 编码转换
#include <QStringDecoder>    // 字符串解码器（从字节到 QString）
#include <QStringEncoder>    // 字符串编码器（从 QString 到字节）

// ============================================================================
// 构造函数
// ============================================================================

/**
 * @brief 构造函数
 * 
 * 初始化 Hosts 管理器，设置默认配置。
 * 
 * @param backupDir 备份文件存储目录
 * @param parent Qt 父对象，用于内存管理
 * 
 * @details
 * 初始化步骤：
 * 1. 设置 hosts 文件路径为 Windows 默认路径
 * 2. 设置默认编码为 UTF-8
 * 3. 初始化默认劫持域名列表
 * 4. 确保备份目录存在
 * 5. 设置备份文件路径
 */
WindowsHostsManager::WindowsHostsManager(const QString& backupDir, QObject* parent)
    : QObject(parent)                           // 初始化 Qt 父对象
    , m_hostsPath(getDefaultHostsPath())        // 使用动态获取的 hosts 文件路径
    , m_backupDir(backupDir)                    // 保存备份目录路径
    , m_detectedEncoding("UTF-8")               // 默认编码
    , m_logCallback(nullptr)                    // 日志回调初始化为空
{
    // 初始化默认劫持域名列表
    // 这些是常见的 AI API 域名
    m_hijackDomains << "api.openai.com"         // OpenAI API
                    << "api.anthropic.com";     // Anthropic API
    
    // 确保备份目录存在
    // 如果目录不存在，创建它（包括所有父目录）
    QDir dir(backupDir);
    if (!dir.exists()) {
        dir.mkpath(".");  // "." 表示创建当前路径
    }
    
    // 设置备份文件的完整路径
    m_backupPath = QDir(backupDir).filePath("hosts.backup");
}

// ============================================================================
// 日志辅助方法
// ============================================================================

/**
 * @brief 发送日志消息
 * 
 * 同时通过回调函数和 Qt 信号两种方式发送日志。
 * 这样可以同时支持纯 C++ 代码和 Qt UI 组件。
 * 
 * @param message 日志消息内容
 * @param level 日志级别
 */
void WindowsHostsManager::emitLog(const std::string& message, LogLevel level)
{
    // 调用纯 C++ 回调（如果已设置）
    // 这允许非 Qt 代码接收日志
    if (m_logCallback) {
        m_logCallback(message, level);
    }
    
    // 同时发送 Qt 信号（用于 UI 集成）
    // 将 std::string 转换为 QString
    emit logMessage(QString::fromStdString(message), level);
}

/**
 * @brief 设置日志回调函数
 * @param callback 日志回调函数
 */
void WindowsHostsManager::setLogCallback(LogCallback callback)
{
    m_logCallback = callback;
}

// ============================================================================
// 编码处理
// ============================================================================

/**
 * @brief 检测文件编码
 * 
 * 通过分析文件内容自动检测编码格式。
 * 
 * @param filePath 文件路径
 * @return 编码名称（UTF-8、GBK、UTF-16LE、UTF-16BE）
 * 
 * @details
 * 检测逻辑：
 * 1. 首先检查 BOM（字节顺序标记）
 *    - EF BB BF: UTF-8 with BOM
 *    - FF FE: UTF-16LE
 *    - FE FF: UTF-16BE
 * 2. 如果没有 BOM，分析字节序列
 *    - 验证是否为有效的 UTF-8 多字节序列
 *    - 如果有高字节但不是有效 UTF-8，假设为 GBK
 * 3. 纯 ASCII 文件默认使用 UTF-8
 */
QString WindowsHostsManager::detectEncoding(const QString& filePath)
{
    // 打开文件进行读取
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        // 无法打开文件，返回默认编码
        return "UTF-8";
    }
    
    // 读取文件头部用于检测编码
    // 4096 字节足够检测大多数编码特征
    QByteArray data = file.read(4096);
    file.close();
    
    // 空文件使用默认编码
    if (data.isEmpty()) {
        return "UTF-8";
    }
    
    // ========== 检查 BOM (Byte Order Mark) ==========
    
    // UTF-8 BOM: EF BB BF
    if (data.startsWith("\xEF\xBB\xBF")) {
        return "UTF-8";
    }
    
    // UTF-16LE BOM: FF FE（小端序）
    if (data.startsWith("\xFF\xFE")) {
        return "UTF-16LE";
    }
    
    // UTF-16BE BOM: FE FF（大端序）
    if (data.startsWith("\xFE\xFF")) {
        return "UTF-16BE";
    }
    
    // ========== 检测 UTF-8 编码 ==========
    
    bool isValidUtf8 = true;   // 是否为有效的 UTF-8
    bool hasHighBytes = false;  // 是否包含高字节（>= 0x80）
    
    // 遍历所有字节进行分析
    for (int i = 0; i < data.size(); ++i) {
        // 将字节转换为无符号类型以便比较
        unsigned char c = static_cast<unsigned char>(data[i]);
        
        // 检查是否为高字节（非 ASCII）
        if (c >= 0x80) {
            hasHighBytes = true;
            
            // 计算 UTF-8 多字节序列的预期长度
            // UTF-8 编码规则：
            // - 110xxxxx: 2 字节序列，后跟 1 个 10xxxxxx
            // - 1110xxxx: 3 字节序列，后跟 2 个 10xxxxxx
            // - 11110xxx: 4 字节序列，后跟 3 个 10xxxxxx
            int expectedBytes = 0;
            if ((c & 0xE0) == 0xC0) {
                expectedBytes = 1;      // 110xxxxx: 2字节字符
            } else if ((c & 0xF0) == 0xE0) {
                expectedBytes = 2;      // 1110xxxx: 3字节字符
            } else if ((c & 0xF8) == 0xF0) {
                expectedBytes = 3;      // 11110xxx: 4字节字符
            } else {
                // 无效的 UTF-8 起始字节
                isValidUtf8 = false;
                break;
            }
            
            // 验证后续字节是否都是 10xxxxxx 格式
            for (int j = 0; j < expectedBytes && i + 1 < data.size(); ++j) {
                ++i;  // 移动到下一个字节
                unsigned char next = static_cast<unsigned char>(data[i]);
                
                // 后续字节必须是 10xxxxxx 格式
                if ((next & 0xC0) != 0x80) {
                    isValidUtf8 = false;
                    break;
                }
            }
        }
    }
    
    // 如果有高字节且是有效的 UTF-8，返回 UTF-8
    if (hasHighBytes && isValidUtf8) {
        return "UTF-8";
    }
    
    // 如果有高字节但不是有效 UTF-8，假设是 GBK
    // GBK 是简体中文 Windows 的默认编码
    if (hasHighBytes) {
        return "GBK";
    }
    
    // 纯 ASCII 文件，使用 UTF-8（ASCII 是 UTF-8 的子集）
    return "UTF-8";
}

/**
 * @brief 读取文件内容（自动检测编码）
 * 
 * 读取文件并根据检测到的编码进行解码。
 * 
 * @param filePath 文件路径
 * @param content 输出参数，存储解码后的内容
 * @return true 读取成功
 * @return false 读取失败
 * 
 * @details
 * 处理流程：
 * 1. 打开文件并读取全部内容
 * 2. 检测文件编码
 * 3. 根据编码进行解码
 *    - UTF-8: 移除 BOM 后直接转换
 *    - UTF-16LE/BE: 移除 BOM，处理字节序
 *    - GBK: 使用 QStringDecoder 解码
 */
bool WindowsHostsManager::readFileContent(const QString& filePath, QString& content)
{
    // 打开文件
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        emitLog("无法打开文件: " + filePath.toStdString(), LogLevel::Error);
        return false;
    }
    
    // 读取全部内容
    QByteArray data = file.readAll();
    file.close();
    
    // 检测编码并保存（用于后续写入时保持编码一致）
    m_detectedEncoding = detectEncoding(filePath);
    
    // ========== 根据编码解码内容 ==========
    
    if (m_detectedEncoding == "UTF-8") {
        // UTF-8 解码
        // 首先移除 BOM（如果存在）
        if (data.startsWith("\xEF\xBB\xBF")) {
            data = data.mid(3);  // 跳过 3 字节的 BOM
        }
        content = QString::fromUtf8(data);
    } 
    else if (m_detectedEncoding == "UTF-16LE") {
        // UTF-16 小端序解码
        // 移除 BOM
        if (data.startsWith("\xFF\xFE")) {
            data = data.mid(2);  // 跳过 2 字节的 BOM
        }
        // 将字节数组解释为 UTF-16 字符
        content = QString::fromUtf16(
            reinterpret_cast<const char16_t*>(data.constData()),
            data.size() / 2  // 每个字符 2 字节
        );
    } 
    else if (m_detectedEncoding == "UTF-16BE") {
        // UTF-16 大端序解码
        // 移除 BOM
        if (data.startsWith("\xFE\xFF")) {
            data = data.mid(2);
        }
        // 交换字节序（大端转小端）
        // 因为 QString::fromUtf16 期望小端序
        for (int i = 0; i < data.size() - 1; i += 2) {
            std::swap(data[i], data[i + 1]);
        }
        content = QString::fromUtf16(
            reinterpret_cast<const char16_t*>(data.constData()),
            data.size() / 2
        );
    } 
    else {
        // GBK 或其他编码
        // 使用 Qt 的字符串解码器
        auto decoder = QStringDecoder(m_detectedEncoding.toLatin1().constData());
        if (decoder.isValid()) {
            content = decoder(data);
        } else {
            // 解码器无效，使用系统本地编码
            content = QString::fromLocal8Bit(data);
        }
    }
    
    return true;
}

/**
 * @brief 以指定编码写入文件
 * 
 * 将内容以指定编码写入文件。
 * 
 * @param filePath 文件路径
 * @param content 要写入的内容
 * @param encoding 编码名称
 * @return true 写入成功
 * @return false 写入失败
 * 
 * @details
 * 处理流程：
 * 1. 打开文件（截断模式）
 * 2. 根据编码进行编码转换
 *    - UTF-8: 直接转换
 *    - UTF-16LE/BE: 添加 BOM，处理字节序
 *    - GBK: 使用 QStringEncoder 编码
 * 3. 写入文件并验证写入字节数
 */
bool WindowsHostsManager::writeWithEncoding(const QString& filePath, 
                                             const QString& content,
                                             const QString& encoding)
{
    // 打开文件，使用截断模式（清空原有内容）
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        emitLog("无法写入文件: " + filePath.toStdString(), LogLevel::Error);
        return false;
    }
    
    // 存储编码后的数据
    QByteArray data;
    
    // ========== 根据编码进行转换 ==========
    
    if (encoding == "UTF-8") {
        // UTF-8 编码（不添加 BOM，这是更常见的做法）
        data = content.toUtf8();
    } 
    else if (encoding == "UTF-16LE") {
        // UTF-16 小端序编码
        // 添加 BOM: FF FE
        data.append("\xFF\xFE", 2);
        // 追加 UTF-16 编码的内容
        data.append(
            reinterpret_cast<const char*>(content.utf16()),
            content.size() * 2  // 每个字符 2 字节
        );
    } 
    else if (encoding == "UTF-16BE") {
        // UTF-16 大端序编码
        // 添加 BOM: FE FF
        data.append("\xFE\xFF", 2);
        // 获取 UTF-16 数据并交换字节序
        const ushort* utf16 = content.utf16();
        for (int i = 0; i < content.size(); ++i) {
            // 大端序：高字节在前
            data.append(static_cast<char>((utf16[i] >> 8) & 0xFF));  // 高字节
            data.append(static_cast<char>(utf16[i] & 0xFF));          // 低字节
        }
    } 
    else {
        // GBK 或其他编码
        // 使用 Qt 的字符串编码器
        auto encoder = QStringEncoder(encoding.toLatin1().constData());
        if (encoder.isValid()) {
            data = encoder(content);
        } else {
            // 编码器无效，使用系统本地编码
            data = content.toLocal8Bit();
        }
    }
    
    // 写入文件
    qint64 written = file.write(data);
    file.close();
    
    // 验证写入是否完整
    return written == data.size();
}

// ============================================================================
// 条目处理
// ============================================================================

/**
 * @brief 生成要添加的 hosts 条目
 * 
 * 生成包含所有劫持域名的 hosts 条目文本。
 * 
 * @return 条目文本，包含标记块和所有域名映射
 * 
 * @details
 * 生成格式：
 * @code
 * # ShangHaoQi Proxy - Added automatically
 * 127.0.0.1 api.openai.com
 * ::1 api.openai.com
 * 127.0.0.1 api.anthropic.com
 * ::1 api.anthropic.com
 * # End ShangHaoQi Proxy
 * @endcode
 * 
 * 每个域名生成两条记录：
 * - IPv4: 127.0.0.1 指向本地
 * - IPv6: ::1 指向本地
 */
QString WindowsHostsManager::generateEntries() const
{
    QString entries;
    
    // 添加开始标记（便于后续识别和删除）
    entries += QStringLiteral("\n# ShangHaoQi Proxy - Added automatically\n");
    
    // 为每个域名生成 IPv4 和 IPv6 条目
    for (const QString& domain : m_hijackDomains) {
        // IPv4 条目：将域名指向本地回环地址
        entries += QStringLiteral("127.0.0.1 %1\n").arg(domain);
        // IPv6 条目：将域名指向本地回环地址
        entries += QStringLiteral("::1 %1\n").arg(domain);
    }
    
    // 添加结束标记
    entries += QStringLiteral("# End ShangHaoQi Proxy\n");
    
    return entries;
}

/**
 * @brief 从内容中移除相关条目
 * 
 * 使用正则表达式从 hosts 文件内容中移除所有相关条目。
 * 
 * @param content 原始 hosts 文件内容
 * @return 移除条目后的内容
 * 
 * @details
 * 移除策略：
 * 1. 首先尝试移除完整的标记块（推荐方式）
 * 2. 然后移除单独的条目（兼容旧版本或手动添加的条目）
 * 
 * 正则表达式说明：
 * - 标记块：匹配从开始标记到结束标记的所有内容
 * - 单独条目：匹配 127.0.0.1 或 ::1 后跟域名的行
 */
QString WindowsHostsManager::removeEntriesFromContent(const QString& content) const
{
    QString result = content;
    
    // ========== 移除标记块 ==========
    // 正则表达式匹配完整的标记块
    // DotMatchesEverythingOption 使 . 匹配换行符
    QRegularExpression blockRegex(
        R"(\n?# ShangHaoQi Proxy - Added automatically\n.*?# End ShangHaoQi Proxy\n?)",
        QRegularExpression::DotMatchesEverythingOption
    );
    result.remove(blockRegex);
    
    // ========== 移除单独的条目（兼容旧版本）==========
    // 遍历所有劫持域名
    for (const QString& domain : m_hijackDomains) {
        // 构造 IPv4 条目的正则表达式
        // 匹配格式：127.0.0.1 域名
        // 需要转义域名中的点号
        QRegularExpression ipv4Regex(
            QStringLiteral(R"(\n?127\.0\.0\.1\s+%1\s*\n?)")
                .arg(QRegularExpression::escape(domain))
        );
        
        // 构造 IPv6 条目的正则表达式
        // 匹配格式：::1 域名
        QRegularExpression ipv6Regex(
            QStringLiteral(R"(\n?::1\s+%1\s*\n?)")
                .arg(QRegularExpression::escape(domain))
        );
        
        // 移除匹配的条目
        result.remove(ipv4Regex);
        result.remove(ipv6Regex);
    }
    
    return result;
}

// ============================================================================
// 主要操作
// ============================================================================

/**
 * @brief 备份 hosts 文件
 * 
 * 将当前 hosts 文件备份到指定目录，保持原有编码格式。
 * 
 * @return true 备份成功
 * @return false 备份失败
 * 
 * @details
 * 备份流程：
 * 1. 读取 hosts 文件内容（自动检测编码）
 * 2. 以相同编码写入备份文件
 * 
 * @note 备份文件路径在构造函数中设置
 */
bool WindowsHostsManager::backup()
{
    emitLog("正在备份 hosts 文件到: " + m_backupPath.toStdString(), LogLevel::Info);
    
    // 读取 hosts 文件内容
    QString content;
    if (!readFileContent(m_hostsPath, content)) {
        emitLog("备份失败: 无法读取 hosts 文件", LogLevel::Error);
        return false;
    }
    
    // 以相同编码写入备份文件
    if (!writeWithEncoding(m_backupPath, content, m_detectedEncoding)) {
        emitLog("备份失败: 无法写入备份文件", LogLevel::Error);
        return false;
    }
    
    emitLog("hosts 文件备份成功", LogLevel::Info);
    return true;
}

/**
 * @brief 恢复 hosts 文件
 * 
 * 从备份文件恢复 hosts 文件。
 * 
 * @return true 恢复成功
 * @return false 恢复失败（备份不存在或写入失败）
 * 
 * @details
 * 恢复流程：
 * 1. 检查备份文件是否存在
 * 2. 读取备份文件内容
 * 3. 写入 hosts 文件
 */
bool WindowsHostsManager::restore()
{
    emitLog("正在恢复 hosts 文件...", LogLevel::Info);
    
    // 检查备份文件是否存在
    QFileInfo backupInfo(m_backupPath);
    if (!backupInfo.exists()) {
        emitLog("恢复失败: 备份文件不存在", LogLevel::Error);
        return false;
    }
    
    // 检测备份文件的编码
    QString content;
    QString backupEncoding = detectEncoding(m_backupPath);
    m_detectedEncoding = backupEncoding;
    
    // 读取备份文件内容
    if (!readFileContent(m_backupPath, content)) {
        emitLog("恢复失败: 无法读取备份文件", LogLevel::Error);
        return false;
    }
    
    // 写入 hosts 文件
    if (!writeWithEncoding(m_hostsPath, content, m_detectedEncoding)) {
        emitLog("恢复失败: 无法写入 hosts 文件", LogLevel::Error);
        return false;
    }
    
    emitLog("hosts 文件恢复成功", LogLevel::Info);
    return true;
}

/**
 * @brief 添加 hosts 条目
 * 
 * 向 hosts 文件添加域名劫持条目。
 * 
 * @return true 添加成功（或条目已存在）
 * @return false 添加失败
 * 
 * @details
 * 添加流程：
 * 1. 先备份当前 hosts 文件
 * 2. 读取 hosts 文件内容
 * 3. 检查条目是否已存在
 * 4. 如果不存在，追加新条目
 * 5. 写入 hosts 文件
 * 
 * @note 需要管理员权限才能修改 hosts 文件
 */
bool WindowsHostsManager::addEntry()
{
    emitLog("正在添加 hosts 条目...", LogLevel::Info);
    
    // 先备份当前 hosts 文件
    if (!backup()) {
        return false;
    }
    
    // 读取 hosts 文件内容
    QString content;
    if (!readFileContent(m_hostsPath, content)) {
        return false;
    }
    
    // 检查是否已存在
    if (hasEntry()) {
        emitLog("hosts 条目已存在，跳过添加", LogLevel::Info);
        return true;
    }
    
    // 追加新条目
    content += generateEntries();
    
    // 写入 hosts 文件
    if (!writeWithEncoding(m_hostsPath, content, m_detectedEncoding)) {
        emitLog("添加 hosts 条目失败", LogLevel::Error);
        return false;
    }
    
    emitLog("hosts 条目添加成功", LogLevel::Info);
    return true;
}

/**
 * @brief 删除 hosts 条目
 * 
 * 从 hosts 文件中删除域名劫持条目。
 * 
 * @return true 删除成功（或条目不存在）
 * @return false 删除失败
 * 
 * @details
 * 删除流程：
 * 1. 读取 hosts 文件内容
 * 2. 使用正则表达式移除相关条目
 * 3. 如果内容有变化，写入 hosts 文件
 * 
 * @note 需要管理员权限才能修改 hosts 文件
 */
bool WindowsHostsManager::removeEntry()
{
    emitLog("正在删除 hosts 条目...", LogLevel::Info);
    
    // 读取 hosts 文件内容
    QString content;
    if (!readFileContent(m_hostsPath, content)) {
        return false;
    }
    
    // 移除相关条目
    QString newContent = removeEntriesFromContent(content);
    
    // 检查内容是否有变化
    if (newContent == content) {
        emitLog("未找到需要删除的 hosts 条目", LogLevel::Info);
        return true;
    }
    
    // 写入修改后的内容
    if (!writeWithEncoding(m_hostsPath, newContent, m_detectedEncoding)) {
        emitLog("删除 hosts 条目失败", LogLevel::Error);
        return false;
    }
    
    emitLog("hosts 条目删除成功", LogLevel::Info);
    return true;
}

/**
 * @brief 检查 hosts 条目是否存在
 * 
 * 检查所有劫持域名是否都已添加到 hosts 文件。
 * 
 * @return true 所有条目都存在
 * @return false 至少有一个条目不存在
 * 
 * @details
 * 检查逻辑：
 * - 对于每个劫持域名，检查是否存在 127.0.0.1 或 ::1 的映射
 * - 只有所有域名都存在映射时才返回 true
 */
bool WindowsHostsManager::hasEntry() const
{
    // 打开 hosts 文件
    QFile file(m_hostsPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }
    
    // 读取文件内容（使用 UTF-8，因为只是检查）
    QString content = QString::fromUtf8(file.readAll());
    file.close();
    
    // 检查是否包含所有域名条目
    for (const QString& domain : m_hijackDomains) {
        // 构造正则表达式，匹配 127.0.0.1 或 ::1 后跟域名
        QRegularExpression regex(
            QStringLiteral(R"((127\.0\.0\.1|::1)\s+%1)")
                .arg(QRegularExpression::escape(domain))
        );
        
        // 如果任何一个域名不存在，返回 false
        if (!regex.match(content).hasMatch()) {
            return false;
        }
    }
    
    // 所有域名都存在
    return true;
}

/**
 * @brief 在编辑器中打开 hosts 文件
 * 
 * 使用 Windows 记事本打开 hosts 文件进行编辑。
 * 
 * @return true 成功启动记事本
 * @return false 启动失败
 * 
 * @note 记事本以分离进程方式启动，不会阻塞当前程序
 */
bool WindowsHostsManager::openInEditor()
{
    emitLog("正在打开 hosts 文件...", LogLevel::Info);
    
    // 创建进程对象
    // 使用 this 作为父对象，确保内存管理
    QProcess* process = new QProcess(this);
    
    // 设置要执行的程序
    process->setProgram("notepad.exe");
    
    // 设置命令行参数（hosts 文件路径）
    process->setArguments({m_hostsPath});
    
    // 以分离模式启动（不等待进程结束）
    bool started = process->startDetached();
    
    if (started) {
        emitLog("已启动记事本编辑 hosts 文件", LogLevel::Info);
    } else {
        emitLog("无法启动记事本", LogLevel::Error);
    }
    
    return started;
}

// ============================================================================
// 路径和配置（返回 std::string / std::vector<std::string>）
// ============================================================================

/**
 * @brief 获取 hosts 文件路径
 * @return hosts 文件的完整路径
 */
std::string WindowsHostsManager::hostsFilePath() const
{
    return m_hostsPath.toStdString();
}

/**
 * @brief 获取备份文件路径
 * @return 备份文件的完整路径
 */
std::string WindowsHostsManager::backupFilePath() const
{
    return m_backupPath.toStdString();
}

/**
 * @brief 获取劫持域名列表
 * 
 * 将内部的 QStringList 转换为 std::vector<std::string> 返回。
 * 
 * @return 劫持域名列表
 */
std::vector<std::string> WindowsHostsManager::hijackDomains() const
{
    // 创建结果容器并预分配空间
    std::vector<std::string> result;
    result.reserve(m_hijackDomains.size());
    
    // 将 QString 转换为 std::string
    for (const QString& domain : m_hijackDomains) {
        result.push_back(domain.toStdString());
    }
    
    return result;
}

/**
 * @brief 设置劫持域名列表
 * 
 * 将 std::vector<std::string> 转换为内部的 QStringList。
 * 
 * @param domains 新的劫持域名列表
 */
void WindowsHostsManager::setHijackDomains(const std::vector<std::string>& domains)
{
    // 清空现有列表
    m_hijackDomains.clear();
    
    // 将 std::string 转换为 QString 并添加
    for (const std::string& domain : domains) {
        m_hijackDomains.append(QString::fromStdString(domain));
    }
}
