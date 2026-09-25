#pragma once

#include "sys_core/hash.h"

#include <sal.h>
#include <string>

namespace ytpp::sys_core::machine_feature {

/// @brief 汇总Windows设备和系统版本信息。
struct SystemInformation {
    std::string deviceName;
    std::string processor;
    std::string installedRam;
    std::string deviceId;
    std::string productId;
    std::string systemType;
    std::string penAndTouch;
    std::string edition;
    std::string version;
    std::string installDate;
    std::string osBuild;
    std::string experience;
    bool isVirtualMachine = false;
};

/// @brief 查询当前Windows设备及系统信息。
/// @return 当前计算机的设备、Windows版本和虚拟机检测结果。
SystemInformation GetSystemInformation();

/// @brief 执行WMI查询并读取首条结果的指定字段。
/// @param[in] wql WMI查询语言语句。
/// @param[in] field 要读取的字段名。
/// @return 字段的UTF-8文本；查询失败或没有结果时返回空字符串。
/// @param wql 传递给 QueryWmi 的 wql 参数。
/// @param field 传递给 QueryWmi 的 field 参数。
std::string QueryWmi(_In_ const std::string& wql, _In_ const std::string& field);

/// @brief 根据选定硬件特征生成机器码。
/// @param[in] signature 添加到机器特征中的业务签名。
/// @param[in] cpu 是否包含CPU信息。
/// @param[in] baseBoard 是否包含主板信息。
/// @param[in] diskDrive 是否包含磁盘信息。
/// @param[in] gpu 是否包含显卡信息。
/// @param[in] physicalMemory 是否包含物理内存信息。
/// @param[in] mac 是否包含MAC地址。
/// @param[in] hashType 机器特征摘要算法。
/// @return 机器特征的十六进制摘要。
/// @param signature 传递给 GetMachineCode 的 signature 参数。
/// @param cpu 传递给 GetMachineCode 的 cpu 参数。
/// @param baseBoard 传递给 GetMachineCode 的 baseBoard 参数。
/// @param diskDrive 传递给 GetMachineCode 的 diskDrive 参数。
/// @param gpu 传递给 GetMachineCode 的 gpu 参数。
/// @param physicalMemory 传递给 GetMachineCode 的 physicalMemory 参数。
/// @param mac 传递给 GetMachineCode 的 mac 参数。
/// @param hashType 控制对应功能是否启用。
std::string GetMachineCode(_In_ std::string signature = {}, _In_ bool cpu = true, _In_ bool baseBoard = true,
                           _In_ bool diskDrive = true, _In_ bool gpu = true, _In_ bool physicalMemory = true,
                           _In_ bool mac = true, _In_ hash::HashType hashType = hash::HashType::Sha256);

/// @brief 收集选定的机器硬件特征文本。
/// @param[in] cpu 是否包含CPU信息。
/// @param[in] baseBoard 是否包含主板信息。
/// @param[in] diskDrive 是否包含磁盘信息。
/// @param[in] gpu 是否包含显卡信息。
/// @param[in] physicalMemory 是否包含物理内存信息。
/// @param[in] mac 是否包含MAC地址。
/// @return 用于生成机器码的原始特征文本。
/// @param cpu 传递给 GetMachineFeatures 的 cpu 参数。
/// @param baseBoard 传递给 GetMachineFeatures 的 baseBoard 参数。
/// @param diskDrive 传递给 GetMachineFeatures 的 diskDrive 参数。
/// @param gpu 传递给 GetMachineFeatures 的 gpu 参数。
/// @param physicalMemory 传递给 GetMachineFeatures 的 physicalMemory 参数。
/// @param mac 传递给 GetMachineFeatures 的 mac 参数。
std::string GetMachineFeatures(_In_ bool cpu, _In_ bool baseBoard, _In_ bool diskDrive, _In_ bool gpu,
                               _In_ bool physicalMemory, _In_ bool mac);

/// @brief 获取当前Windows用户名的ANSI字符串。
/// @return 当前用户名；读取失败时返回错误说明文本。
std::string GetUserNameA();

/// @brief 获取当前Windows用户名的UTF-16字符串。
/// @return 当前用户名；读取失败时返回错误说明文本。
std::wstring GetUserNameW();

} // namespace ytpp::sys_core::machine_feature
