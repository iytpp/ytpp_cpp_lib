# sys_core 使用手册

`sys_core` 汇集 Windows 系统、文件、编码、日期时间、环境变量、字符串、摘要、密码学和设备信息功能。各头文件对应独立子命名空间；`sys_core/sys_core.h` 是统一入口。

## 引入方式

```cmake
find_package(ytpp_cpp_lib CONFIG REQUIRED COMPONENTS sys_core)
target_link_libraries(my_app PRIVATE ytpp::sys_core)
```

```cpp
#include <sys_core/sys_core.h>
```

统一入口会在 `ytpp::sys_core` 内导入各子模块名称。大型项目仍建议直接使用完整名称，例如 `ytpp::sys_core::encoding::Utf8ToWide()`，以明确来源。

## 模块索引

| 头文件 | 命名空间 | 用途 |
|---|---|---|
| `date_time.h` | `ytpp::sys_core::date_time` | 日期计算、本地时间、HTTPS 网络时间 |
| `disk_manipulation.h` | `ytpp::sys_core::disk_manipulation` | 路径、文件、资源和 INI 操作 |
| `encoding.h` | `ytpp::sys_core::encoding` | ANSI、UTF-8、UTF-16 转换 |
| `encryption.h` | `ytpp::sys_core::encryption` | OpenSSL 摘要、KDF、对称/非对称加密和文件加密 |
| `environment.h` | `ytpp::sys_core::environment` | 进程、用户、系统环境变量与 PATH |
| `hash.h` | `ytpp::sys_core::hash` | 简单摘要和非密码学伪随机数 |
| `log.hpp` | 宏 | 控制台彩色日志 |
| `machine_feature.h` | `ytpp::sys_core::machine_feature` | Windows 系统信息和机器特征 |
| `string_ex.h` | `ytpp::sys_core::string_ex` | 常用字符串处理 |
| `sys_processing.h` | `ytpp::sys_core::sys_processing` | 单实例、光标、屏幕和管理员权限 |

## 日期时间 `date_time`

`DateTime` 由 `year`、`month`、`day`、`hour`、`minute`、`second`、`millisecond` 组成。它不携带时区信息，调用方必须明确一个值代表 UTC、本地时间还是固定偏移时间。

### 基础操作

```cpp
using namespace ytpp::sys_core::date_time;

DateTime now = GetLocalDateTime();
const std::string text = FormatDateTime(now);
const int weekday = GetWeekday(now); // 周一为 1，周日为 7。
const int days = GetDaysInMonth(2028, 2);
```

`GetDaysInMonth()` 支持年份 100～9999、月份 1～12。日期参数无效时相关函数可能抛出异常，调用外部输入前应先校验。

### 日期运算

```cpp
DateTime expiry = now;
AddDateTimePart(expiry, DateTimePart::Month, 1);
const std::int64_t daysLeft = GetDateTimeDifference(expiry, now, DateTimePart::Day);
```

`DateTimePart` 支持 Year、Quarter、Month、Week、Day、Hour、Minute、Second、Millisecond。`AddDateTimePart()` 原地修改对象，负值表示减少；跨月时日期会按目标月份合法范围调整。差值返回 `first - second` 的完整单位数。

### 网络时间和时区

```cpp
DateTime utc = GetNetworkUtcDateTime();
DateTime beijing = ConvertUtcToTimeZone(utc, TimeZone::UtcPlus8);
DateTime direct = GetNetworkDateTime(kBeijingTimeZone);
```

`TimeZone` 是 UTC-12 至 UTC+14 的固定整小时偏移，不处理夏令时或历史时区规则。需要 IANA/Windows 动态时区规则时应使用专门时区库。

网络时间通过 HTTPS 响应的 `Date` 头获取，可能因网络、TLS、证书或响应格式失败并抛出异常。`expectedCertificateSha256` 可提供大写或规范化后的 SHA-256 证书指纹进行额外 Pinning；为空时只使用系统证书验证。`GetBaiduServerCertificateSha256()` 返回当前目标服务器证书指纹，证书轮换后值会变化，不能把一次查询结果永久视为可信配置。

## 文件与路径 `disk_manipulation`

### 路径查询

```cpp
using namespace ytpp::sys_core::disk_manipulation;

const std::filesystem::path exe = GetExePath();
const std::wstring exeDir = GetExecutableDirectoryW();
const std::wstring documents = GetKnownFolderPathW(FOLDERID_Documents);
```

- `GetKnownFolderPathW()` 返回 UTF-16 路径；`GetKnownFolderPathUtf8()` 返回 UTF-8。
- `trailingSlash` / `withSlash` 控制目录末尾是否保留分隔符。
- `GetExecutableDirectoryA()` 使用当前 Windows ANSI 代码页，只建议兼容旧接口；新代码优先 W 或 UTF-8 版本。

### 文件检查与覆盖写入

```cpp
if (FileExists(LR"(C:\data\config.bin)")) {
    // 文件存在。
}

std::vector<char> bytes{1, 2, 3};
const bool ok = WriteDataToFile(LR"(C:\data\output.bin)", bytes);
```

`WriteDataToFile()` 对 `std::string` / `std::wstring` 路径以及字节数组、指针缓冲区、窄/宽字符串提供重载。函数覆盖目标文件，不会自动创建父目录。宽字符串按内存中的 UTF-16 字节写入，不自动添加 BOM；窄字符串不会转码。

### 资源导出

`WriteResourceToFileA/W(module, resourceName, resourceType, outputPath)` 从 PE 模块资源中读取原始字节并写入文件。`module`、资源名称/ID和类型必须与资源表一致。

### INI 配置

`WriteProfileValueA/W()`、`ReadProfileValueA/W()` 读写字符串项；`WriteProfileStructA/W()`、`ReadProfileStructA/W()` 读写固定大小二进制结构。

```cpp
WriteProfileValueW(L"settings.ini", L"General", L"Language", L"zh-CN");
const std::wstring language = ReadProfileValueW(
    L"settings.ini", L"General", L"Language", L"en-US");
```

结构体接口只适合同一 ABI、结构布局和版本下的数据，不适合长期持久化或跨编译器交换；结构变化时应使用显式序列化格式。

## 编码转换 `encoding`

```cpp
using namespace ytpp::sys_core::encoding;

const std::wstring wide = Utf8ToWide(u8"中文");
const std::string utf8 = WideToUtf8(wide);
```

| 函数 | 转换 |
|---|---|
| `AnsiToUtf8` | 当前 Windows ANSI 代码页 → UTF-8 |
| `Utf8ToAnsi` | UTF-8 → 当前 ANSI 代码页 |
| `AnsiToWide` | 当前 ANSI 代码页 → UTF-16 |
| `WideToAnsi` | UTF-16 → 当前 ANSI 代码页 |
| `Utf8ToWide` | UTF-8 → UTF-16 |
| `WideToUtf8` | UTF-16 → UTF-8 |

ANSI 结果依赖机器区域设置，无法表示的字符可能丢失。新接口、网络协议和持久化数据应优先使用 UTF-8/UTF-16。

## 环境变量 `environment`

提供三个相同接口族：

- `ProcessEnvironment`：当前进程环境。
- `UserEnvironment`：当前用户持久环境。
- `MachineEnvironment`：系统级持久环境，通常需要管理员权限。

### 读取、写入和删除

```cpp
using namespace ytpp::sys_core::environment;

std::wstring value;
DWORD error = ERROR_SUCCESS;
if (UserEnvironment::TryGetW(L"MY_SETTING", value, &error)) {
    // 使用 value。
}

UserEnvironment::SetW(L"MY_SETTING", L"enabled", &error);
UserEnvironment::RemoveW(L"MY_SETTING", &error);
```

`Try...` 函数返回状态并可输出 Win32 错误码；`Get...` 返回 `std::optional`；`GetOr...` 在变量不存在时返回 fallback；`Exists...` 只判断存在性。A 版本使用窄字符串，W 版本使用 UTF-16。

`TryExpandA/W()` 与 `ExpandA/W()` 展开 `%NAME%` 引用。列表接口 `TryList...()` / `List...()` 返回 `EnvironmentEntryA/W`，`includeHidden` 控制是否包含以 `=` 开头的隐藏环境项。

### PATH 处理

每个环境类都提供以下 A/W 版本：

- `GetPath`：按分号拆分。
- `GetExpandedPath`：展开变量引用。
- `GetUniquePath`：去重。
- `GetExpandedUniquePath`：展开并去重。
- `GetExistingPath`：仅保留存在的目录。
- `GetExpandedExistingPath`：展开、去重并仅保留存在目录。

对应的 `TryGet...` 版本通过输出参数和 Win32 错误码报告失败。

修改用户或系统持久环境后，其他已运行进程不会自动刷新其环境块；需要相关应用重新读取或重启。修改系统环境通常要求提升权限。

## 摘要与随机数 `hash`

```cpp
using namespace ytpp::sys_core::hash;

const std::string digest = ComputeHash("payload", HashType::Sha256);
const std::vector<std::uint8_t> bytes = GeneratePseudoRandomBytes(32);
```

`HashType` 支持 Md5、Sha1、Sha256、Sha512，`ComputeHash()` 返回小写十六进制文本。MD5 和 SHA-1 不适合安全签名或抗碰撞用途。

`GeneratePseudoRandomBytes()` 明确是非密码学随机数，不得用于密钥、Token、Nonce、盐或 IV。密码学随机数据使用 `encryption::RandomBytes()`。

## 密码学 `encryption`

本模块基于 OpenSSL。主要类型：

- `Bytes`：`std::vector<unsigned char>`。
- `CipherPack`：密文、IV 和认证标签。
- `PemKeyPair`：PEM 公钥和私钥。
- `FileEncryptResult` / `PasswordFileEncryptResult`：文件加密元数据。
- `OpenSslException`：OpenSSL 操作失败异常。

### 编码、随机数和摘要

`ToBytes()` / `ToString()` 转换字节与字符串；`HexEncode/Decode()`、`Base64Encode/Decode()` 处理文本编码；`RandomBytes()` 和 `RandomHex()` 使用密码学随机源。

摘要包括 `Md5`、`Sha1`、`Sha224`、`Sha256`、`Sha384`、`Sha512`，OpenSSL 支持时还包括 `Sm3`。HMAC 包括 MD5、SHA-1、SHA-256、SHA-512；新设计推荐 HMAC-SHA-256 或更强算法。

### 密钥派生

- `Pbkdf2HmacSha256()` / `Pbkdf2HmacSha512()`：基于口令和盐派生指定长度密钥。
- `Scrypt()`：内存硬 KDF，可设置 N、r、p 和最大内存。
- `HkdfSha256()`：从已有高熵密钥材料派生上下文密钥。
- `DeriveAes256KeyFromPassword...()`：直接派生 AES-256 密钥。

盐必须随机且与密文共同保存；盐无需保密。迭代次数和 Scrypt 参数应根据部署硬件定期评估。

### 对称加密

提供 AES-128/256 CBC、CTR、GCM、ChaCha20-Poly1305，以及 OpenSSL 支持时的 SM4 CBC/CTR。

```cpp
using namespace ytpp::sys_core::encryption;

Bytes key = RandomBytes(32);
Bytes iv = RandomBytes(12);
Bytes plaintext = ToBytes("secret");
CipherPack pack = EncryptAes256Gcm(plaintext, key, iv);
Bytes restored = DecryptAes256Gcm(pack, key);
```

新业务优先使用 GCM 或 ChaCha20-Poly1305 等带认证算法。CBC/CTR 本身不验证完整性，必须配合独立 MAC，并采用 Encrypt-then-MAC。相同密钥下不得重复使用 GCM/CTR/ChaCha20 的 IV/Nonce。

`EncryptAes()` / `DecryptAes()` 是字符串便利接口；需要明确盐、IV、认证数据和参数的新代码优先使用结构化接口。

### 非对称密钥、加密和签名

- `GenerateRsaKeyPair()`、`GenerateEcP256KeyPair()`、`GenerateEd25519KeyPair()` 生成 PEM 密钥。
- `PublicKeyPemFromPrivateKeyPem()` 提取公钥。
- RSA-OAEP-SHA256 用于小数据加解密。
- RSA-PSS-SHA256、RSA-PKCS#1 v1.5 SHA-256、ECDSA P-256 SHA-256、Ed25519 用于签名和验证。
- OpenSSL 支持时提供 SM2/SM3 签名。

私钥 PEM 必须按敏感数据保护。RSA 不适合直接加密大文件，应随机生成对称密钥加密数据，再用 RSA 封装对称密钥。

### 文件加密 `FileCrypto`

`EncryptFileAes256Gcm()` / `DecryptFileAes256Gcm()` 使用调用方提供的 key、IV、tag 和 AAD。口令版本使用 PBKDF2-SHA256，并返回或接收盐、IV、tag。容器版本把必要元数据和密文放入一个文件：

```cpp
FileCrypto::EncryptFileToContainerWithPasswordAes256GcmPbkdf2(
    "input.dat", "input.dat.ytpp", password);

FileCrypto::DecryptFileFromContainerWithPasswordAes256GcmPbkdf2(
    "input.dat.ytpp", "restored.dat", password);
```

当前容器格式为版本 2：头部使用固定小端字段编码，头部、盐和 IV 都作为 GCM AAD 接受认证，并限制 KDF 迭代次数及元数据长度。版本 2 不兼容旧的原始结构体版本 1 文件，需要先使用旧版本库解密再重新加密。

所有 GCM 文件解密先写入同目录临时文件，只有认证标签和明文长度验证成功后才替换正式输出。认证失败不会覆盖已有目标文件，临时文件会自动清理。

`chunkSize` 控制流式缓冲区，默认 1 MiB，必须大于零。解密认证失败会抛出异常且不会发布临时输出。异常可通过 `OpenSslException` 或 `std::exception` 捕获，`GetOpenSslErrors()` 可用于诊断，但错误日志中不要包含密钥、口令或明文。

## 设备信息 `machine_feature`

`GetSystemInformation()` 返回 `SystemInformation`，包括设备名、CPU、内存、设备/产品 ID、系统类型、Windows 版本、安装日期、构建号以及虚拟机判断。

`QueryWmi(wql, field)` 执行 WMI 查询并读取首条记录字段；WQL 和字段来自外部输入时应设置白名单，避免允许任意系统信息查询。

```cpp
const std::string code = ytpp::sys_core::machine_feature::GetMachineCode(
    "product-v1", true, true, true, false, false, true,
    ytpp::sys_core::hash::HashType::Sha256);
```

机器码由选定硬件特征计算，不保证设备维修、驱动变化、虚拟化迁移或系统升级后稳定，也不能作为独立身份认证凭据。`GetMachineFeatures()` 返回原始特征文本，可能包含敏感设备信息，不应写入公开日志。

## 字符串 `string_ex`

| 函数 | 作用 |
|---|---|
| `ExtractBetween` | 提取左右边界之间文本，可返回结束位置 |
| `Split` | 按字符或字符串分隔 |
| `ReplaceAll` | 替换全部匹配子串 |
| `Trim` | 去除首尾空白 |
| `ToUpper` / `ToLower` | 按当前 C locale 转换大小写 |

`ExtractBetween()` 返回空字符串既可能表示未找到，也可能表示确实匹配到空内容；需要区分时使用 `endPosition`。大小写转换不提供完整 Unicode 规则。

## 系统操作 `sys_processing`

### 单实例

```cpp
using ytpp::sys_core::sys_processing::SingleInstanceGuard;

SingleInstanceGuard guard(L"MyCompany.MyApp", SingleInstanceGuard::MutexNamespace::Local);
if (!guard.IsValid() || !guard.IsFirstInstance()) {
    return 0;
}
```

`DefaultLocal` 不添加前缀，`Local` 使用当前会话命名空间，`Global` 跨会话且可能需要额外权限。对象不可复制，生命周期内持有互斥体。

### 光标、屏幕和权限

- `SetCursorPosition(x, y)` 直接设置两个坐标。
- `UpdateCursorPosition(optionalX, optionalY)` 只修改提供的分量，坐标 0 是合法值。
- `GetCursorPosition()` 输出坐标；X/Y 便利函数失败时返回 -1。
- `GetPrimaryScreenWidth/Height()` 返回主显示器像素尺寸，不代表整个虚拟桌面。
- `IsRunningAsAdministrator()` 判断当前进程令牌是否提升。
- `RestartAsAdministrator()` 通过 UAC 启动提升后的新进程；成功只表示启动请求已发出，当前进程不会自动退出。

## 控制台日志 `log.hpp`

```cpp
#define YTPP_LOG_ENABLE 1
#include <sys_core/log.hpp>

YTPP_LOG_INFO("starting: " << taskName);
YTPP_LOG_SUCCESS("completed");
YTPP_LOG_ERROR("failed: " << errorMessage);
```

可用级别宏包括 `YTPP_LOG_INFO`、`SUCCESS`、`WARNING`、`ERROR`、`DEBUG`、`TRACE`、`FATAL`、`NOTICE`，并提供颜色宏。将 `YTPP_LOG_ENABLE` 设为 `0` 可编译为空操作。宏参数只在启用时求值，不应在日志表达式里放置必要副作用。

## 错误处理总则

- 返回 `bool`/`BOOL` 的系统与文件函数由调用方判断失败；需要 Win32 细节的环境变量接口可接收 `DWORD* error`。
- OpenSSL 模块主要通过异常报告失败。
- 网络时间依赖外部网络和证书状态，应设置上层降级策略，不能假设始终可用。
- A/W 接口中优先使用 W 或明确 UTF-8 的版本，避免当前 ANSI 代码页造成不可逆损失。
