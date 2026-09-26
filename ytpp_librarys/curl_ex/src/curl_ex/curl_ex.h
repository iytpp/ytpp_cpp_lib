#pragma once

// 当前源码版本：4.2.7。

// 临时屏蔽 Windows.h 可能定义的 min/max 宏，避免污染本头文件及其包含的标准库头文件。
#ifdef min
#pragma push_macro("min")
#undef min
#define CURL_EX_RESTORE_MIN_MACRO 1
#endif

#ifdef max
#pragma push_macro("max")
#undef max
#define CURL_EX_RESTORE_MAX_MACRO 1
#endif

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <ostream>
#include <sal.h>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <curl/curl.h>

#if LIBCURL_VERSION_NUM < 0x073800
#error "curl_ex v4.2.7 requires libcurl 7.56.0 or newer"
#endif

// 统一获取当前语言标准版本；MSVC 未启用 /Zc:__cplusplus 时使用 _MSVC_LANG。
#if defined(_MSVC_LANG)
#define CURL_EX_CPLUSPLUS _MSVC_LANG
#else
#define CURL_EX_CPLUSPLUS __cplusplus
#endif

#if CURL_EX_CPLUSPLUS >= 202002L && defined(__has_include)
#if __has_include(<coroutine>)
#include <coroutine>
#define CURL_EX_HAS_COROUTINES 1
#else
#define CURL_EX_HAS_COROUTINES 0
#endif
#else
#define CURL_EX_HAS_COROUTINES 0
#endif

namespace ytpp::curl_ex {

/// 当前封装版本。
inline constexpr std::string_view kCurlExVersion = "4.2.7";

/// 显式初始化 libcurl 全局运行环境；使用 libcurl 7.84.0 之前的版本时应在创建其他线程前调用。
/// @return libcurl 全局初始化结果。
CURLcode Initialize();

// ============================================================
// URL 与查询参数
// ============================================================

/// 对字符串执行 URL 百分号编码。
/// @param input 原始字符串。
/// @throws std::length_error 输入长度超过 libcurl int 长度接口上限。
std::string UrlEncode(_In_ std::string_view input);

/// 对 URL 百分号编码字符串执行解码。
/// @param input 已编码字符串。
/// @throws std::length_error 输入长度超过 libcurl int 长度接口上限。
std::string UrlDecode(_In_ std::string_view input);

/// 单个 URL 查询参数。
struct UrlParamItem {
    std::string key;       ///< 参数名称。
    std::string value;     ///< 参数值。
    bool hasEquals = true; ///< 是否保留 key= 中的等号。
};

/// 保序并允许重复名称的 URL 查询参数容器。
class UrlParameters {
  public:
    using Container = std::vector<UrlParamItem>;     ///< 底层容器类型。
    using Iterator = Container::iterator;            ///< 可写迭代器。
    using ConstIterator = Container::const_iterator; ///< 只读迭代器。

    /// 创建空参数集合。
    UrlParameters() = default;

    /// 从查询字符串创建参数集合。
    /// @param query 查询字符串。
    explicit UrlParameters(_In_ std::string_view query);

    /// 解析并替换当前参数。
    /// @param query 查询字符串。
    void Parse(_In_ std::string_view query);

    /// 序列化查询参数。
    /// @param leadingQuestionMark 是否添加开头问号。
    std::string ToString(_In_ bool leadingQuestionMark = false) const;

    /// 获取第一个同名参数值。
    /// @param key 参数名称。
    std::string Get(_In_ const std::string& key) const;

    /// 获取全部同名参数值。
    /// @param key 参数名称。
    std::vector<std::string> GetAll(_In_ const std::string& key) const;

    /// 设置参数并覆盖全部同名参数。
    /// @param key 参数名称。
    /// @param value 参数值。
    void Set(_In_ const std::string& key, _In_ const std::string& value);

    /// 追加参数。
    /// @param key 参数名称。
    /// @param value 参数值。
    /// @param hasEquals 是否序列化等号。
    void Add(_In_ const std::string& key, _In_ const std::string& value, _In_ bool hasEquals = true);

    /// 删除全部同名参数。
    /// @param key 参数名称。
    void Remove(_In_ const std::string& key);

    /// 判断参数是否存在。
    /// @param key 参数名称。
    bool Has(_In_ const std::string& key) const;

    /// 获取参数总数量。
    std::size_t GetSize() const noexcept;

    /// 判断参数集合是否为空。
    bool IsEmpty() const noexcept;

    /// 清空全部参数。
    void Clear() noexcept;

    /// 获取全部 key/value 文本项。
    std::vector<std::string> GetAllParameters() const;

    /// 获取参数名称并去重。
    std::vector<std::string> GetAllParameterNames() const;

    /// 获取或创建指定参数的第一个值。
    /// @param key 参数名称。
    std::string& operator[](_In_ const std::string& key);

    /// @brief 调用 begin 完成对应操作。
    Iterator begin() noexcept {
        return params_.begin();
    } ///< 获取起始可写迭代器。

    /// @brief 调用 end 完成对应操作。
    Iterator end() noexcept {
        return params_.end();
    } ///< 获取结束可写迭代器。

    /// @brief 调用 begin 完成对应操作。
    ConstIterator begin() const noexcept {
        return params_.begin();
    } ///< 获取起始只读迭代器。

    /// @brief 调用 end 完成对应操作。
    ConstIterator end() const noexcept {
        return params_.end();
    } ///< 获取结束只读迭代器。

    /// @brief 调用 cbegin 完成对应操作。
    ConstIterator cbegin() const noexcept {
        return params_.cbegin();
    } ///< 获取起始常量迭代器。

    /// @brief 调用 cend 完成对应操作。
    ConstIterator cend() const noexcept {
        return params_.cend();
    } ///< 获取结束常量迭代器。

  private:
    Container params_; ///< 按原顺序保存的参数列表。
};

// ============================================================
// HTTP 头部处理
// ============================================================

/// 单个 HTTP Header。
struct HttpHeaderItem {
    std::string name;  ///< Header 名称。
    std::string value; ///< Header 值。
};

/// 大小写不敏感并允许重复字段的 HTTP Header 容器。
class HttpHeaders {
  public:
    using Container = std::vector<HttpHeaderItem>;   ///< 底层容器类型。
    using Iterator = Container::iterator;            ///< 可写迭代器。
    using ConstIterator = Container::const_iterator; ///< 只读迭代器。

    /// 创建空 Header 集合。
    HttpHeaders() = default;

    /// 从原始 Header 文本创建。
    /// @param rawHeaders 原始多行 Header。
    explicit HttpHeaders(_In_ std::string_view rawHeaders);

    /// 解析并替换当前 Header。
    /// @param rawHeaders 原始多行 Header。
    void ParseHeaders(_In_ std::string_view rawHeaders);

    /// 追加 Header，不覆盖同名字段。
    /// @param key Header 名称。
    /// @param value Header 值。
    bool AddHeader(_In_ const std::string& key, _In_ const std::string& value);

    /// 设置 Header，并覆盖全部同名字段。
    /// @param key Header 名称。
    /// @param value Header 值。
    bool SetHeader(_In_ const std::string& key, _In_ const std::string& value);

    /// 仅在不存在时设置 Header。
    /// @param key Header 名称。
    /// @param value Header 值。
    bool SetDefaultHeader(_In_ const std::string& key, _In_ const std::string& value);

    /// 向第一个同名 Header 后追加值。
    /// @param key Header 名称。
    /// @param value 要追加的值。
    /// @param delimiter 连接分隔符。
    bool AppendHeader(_In_ const std::string& key, _In_ const std::string& value,
                      _In_ const std::string& delimiter = ", ");

    /// 删除全部同名 Header。
    /// @param key Header 名称。
    bool EraseHeader(_In_ const std::string& key);

    /// 判断 Header 是否存在。
    /// @param key Header 名称。
    bool Contains(_In_ const std::string& key) const;

    /// 获取第一个同名 Header 值。
    /// @param key Header 名称。
    std::string GetHeaderValue(_In_ const std::string& key) const;

    /// 获取全部同名 Header 值。
    /// @param key Header 名称。
    std::vector<std::string> GetHeaderValues(_In_ const std::string& key) const;

    /// 获取 Header 名称列表。
    /// @param unique 是否去重名称。
    std::vector<std::string> GetKeys(_In_ bool unique = true) const;

    /// 序列化全部 Header。
    std::string GetAllHeaders() const;

    /// 获取底层 Header 项。
    const Container& GetItems() const noexcept {
        return headers_;
    }

    /// 清空全部 Header。
    void Clear() noexcept {
        headers_.clear();
    }

    /// 判断集合是否为空。
    bool IsEmpty() const noexcept {
        return headers_.empty();
    }

    /// 获取 Header 项数量。
    std::size_t GetSize() const noexcept {
        return headers_.size();
    }

    /// @brief 调用 begin 完成对应操作。
    Iterator begin() noexcept {
        return headers_.begin();
    } ///< 获取起始可写迭代器。

    /// @brief 调用 end 完成对应操作。
    Iterator end() noexcept {
        return headers_.end();
    } ///< 获取结束可写迭代器。

    /// @brief 调用 begin 完成对应操作。
    ConstIterator begin() const noexcept {
        return headers_.begin();
    } ///< 获取起始只读迭代器。

    /// @brief 调用 end 完成对应操作。
    ConstIterator end() const noexcept {
        return headers_.end();
    } ///< 获取结束只读迭代器。

  private:
    static std::string Trim(_In_ std::string_view str); ///< 去除首尾空白。
    static bool EqualsIgnoreCase(_In_ std::string_view a, _In_ std::string_view b) noexcept; ///< 大小写不敏感比较。
    /// @param value 要读取、写入或处理的值。
    static bool IsSafeHeaderName(_In_ std::string_view value) noexcept; ///< 验证 Header 名称安全性。
    /// @param value 要读取、写入或处理的值。
    static bool IsSafeHeaderValue(_In_ std::string_view value) noexcept; ///< 验证 Header 值安全性。
    Container headers_;                                                  ///< Header 列表。
};

// ============================================================
// Cookie 处理
// ============================================================

/// 单个 HTTP Cookie。
struct Cookie {
    std::string name;                    ///< Cookie 名称。
    std::string value;                   ///< Cookie 值。
    std::optional<std::string> path;     ///< 可选 Path。
    std::optional<std::string> domain;   ///< 可选 Domain。
    std::optional<std::string> expires;  ///< 可选 Expires 文本。
    std::optional<std::int64_t> maxAge;  ///< 可选 Max-Age。
    bool secure = false;                 ///< 是否包含 Secure。
    bool httpOnly = false;               ///< 是否包含 HttpOnly。
    std::optional<std::string> sameSite; ///< 可选 SameSite 属性。

    /// 序列化为 Set-Cookie 值。
    std::string ToSetCookieString() const;
};

/// 支持 Name/Domain/Path 身份的 Cookie 容器。
class HttpCookies {
  public:
    /// 创建空 Cookie 集合。
    HttpCookies() = default;

    /// 从多条 Set-Cookie 值创建。
    /// @param setCookieHeaders Set-Cookie 值列表。
    explicit HttpCookies(_In_ const std::vector<std::string>& setCookieHeaders);

    /// 从请求 Cookie 字符串创建。
    /// @param requestCookieString Cookie 请求头值。
    explicit HttpCookies(_In_ std::string_view requestCookieString);

    /// 合并 Cookie。
    /// @param other 其他 Cookie 集合。
    /// @param overwrite 是否覆盖相同身份 Cookie。
    void Merge(_In_ const HttpCookies& other, _In_ bool overwrite = true);

    /// 返回合并后的新 Cookie 集合。
    /// @param other 其他 Cookie 集合。
    /// @param overwrite 是否覆盖相同身份 Cookie。
    HttpCookies MergedWith(_In_ const HttpCookies& other, _In_ bool overwrite = true) const;

    /// 解析 Set-Cookie 列表。
    /// @param setCookieHeaders Set-Cookie 值列表。
    void ParseFromSetCookieHeaders(_In_ const std::vector<std::string>& setCookieHeaders);

    /// 解析请求 Cookie 字符串。
    /// @param cookieString Cookie 请求头值。
    void ParseFromCookieString(_In_ std::string_view cookieString);

    /// 从原始响应 Header 提取 Set-Cookie。
    /// @param rawHeaders 原始响应 Header。
    static std::vector<std::string> ExtractSetCookieHeaders(_In_ std::string_view rawHeaders);

    /// 设置完整 Cookie。
    /// @param cookie Cookie 对象。
    void SetCookie(_In_ const Cookie& cookie);

    /// 设置简单 Name/Value Cookie。
    /// @param name Cookie 名称。
    /// @param value Cookie 值。
    void SetCookie(_In_ const std::string& name, _In_ const std::string& value);

    /// 删除全部同名 Cookie。
    /// @param name Cookie 名称。
    bool EraseCookie(_In_ const std::string& name);

    /// 删除指定 Name/Domain/Path Cookie。
    /// @param name Cookie 名称。
    /// @param domain 可选 Domain。
    /// @param path 可选 Path。
    bool EraseCookie(_In_ const std::string& name, _In_ const std::optional<std::string>& domain,
                     _In_ const std::optional<std::string>& path);

    /// 判断同名 Cookie 是否存在。
    /// @param name Cookie 名称。
    bool Contains(_In_ const std::string& name) const;

    /// 删除值为空的 Cookie。
    void RemoveEmptyCookies();

    /// 获取第一个同名 Cookie 值。
    /// @param name Cookie 名称。
    std::string GetCookieValue(_In_ const std::string& name) const;

    /// 获取全部同名 Cookie。
    /// @param name Cookie 名称。
    std::vector<Cookie> GetCookies(_In_ const std::string& name) const;

    /// 获取 Cookie 名称列表。
    /// @param ignoreNull 是否忽略空值 Cookie。
    std::vector<std::string> GetAllKeys(_In_ bool ignoreNull = false) const;

    /// 序列化为请求 Cookie 值。
    /// @param ignoreNull 是否忽略空值 Cookie。
    std::string ToRequestCookieString(_In_ bool ignoreNull = false) const;

    /// 序列化为多条 Set-Cookie Header。
    /// @param ignoreNull 是否忽略空值 Cookie。
    std::vector<std::string> ToSetCookieHeaders(_In_ bool ignoreNull = false) const;

    /// 获取全部 Cookie 对象。
    /// @param ignoreNull 是否忽略空值 Cookie。
    std::vector<Cookie> GetAllCookies(_In_ bool ignoreNull = false) const;

    /// 清空全部 Cookie。
    void Clear() noexcept {
        cookies_.clear();
    }

    /// 判断 Cookie 集合是否为空。
    bool IsEmpty() const noexcept {
        return cookies_.empty();
    }

    /// 获取 Cookie 数量。
    std::size_t GetSize() const noexcept {
        return cookies_.size();
    }

  private:
    static std::string Trim(_In_ std::string_view str); ///< 去除首尾空白。
    static bool HasSameIdentity(_In_ const Cookie& a, _In_ const Cookie& b); ///< 比较 Name/Domain/Path 身份。
    std::vector<Cookie> cookies_;                                            ///< Cookie 列表。
};

// ============================================================
// 请求基础类型与配置
// ============================================================

/// 常用 HTTP 请求方法。
enum class HttpMethod {
    Get,     ///< GET 请求。
    Head,    ///< HEAD 请求。
    Post,    ///< POST 请求。
    Put,     ///< PUT 请求。
    Delete,  ///< DELETE 请求。
    Patch,   ///< PATCH 请求。
    Options, ///< OPTIONS 请求。
    Trace,   ///< TRACE 请求。
    Connect  ///< CONNECT 请求。
};

/// 将 HTTP 请求方法枚举转换为文本；非法枚举值返回空字符串，后续请求校验会明确拒绝。
/// @param method HTTP 请求方法。
std::string_view ToString(_In_ HttpMethod method) noexcept;

/// HTTP 协议版本偏好。
enum class HttpVersion {
    Auto,     ///< 由 libcurl 自动选择协议版本。
    Http1_0,  ///< 强制使用 HTTP/1.0。
    Http1_1,  ///< 强制使用 HTTP/1.1。
    Http2,    ///< 优先使用 HTTP/2。
    Http2Tls, ///< 仅在 TLS 连接中尝试 HTTP/2。
    Http3     ///< 尝试使用 HTTP/3。
};

/// 代理协议类型。
enum class ProxyType {
    Http,          ///< HTTP 代理。
    Https,         ///< HTTPS 代理。
    Socks4,        ///< SOCKS4 代理。
    Socks4A,       ///< SOCKS4A 代理。
    Socks5,        ///< SOCKS5 代理。
    Socks5Hostname ///< 由代理端解析域名的 SOCKS5 代理。
};

/// TLS 版本限制。
enum class TlsVersion {
    Default, ///< 使用 libcurl 默认 TLS 版本策略。
    Tls1_0,  ///< 使用 TLS 1.0。
    Tls1_1,  ///< 使用 TLS 1.1。
    Tls1_2,  ///< 使用 TLS 1.2。
    Tls1_3   ///< 使用 TLS 1.3。
};

/// Schannel 证书吊销检查策略。
enum class CertificateRevocationPolicy {
    Strict,     ///< 严格执行吊销检查，无法查询吊销状态时请求失败。
    BestEffort, ///< 尽力检查吊销状态，分发点缺失或离线时允许继续。
    Disabled    ///< 完全关闭 Schannel 吊销检查。
};

/// 封装层错误分类。
enum class HttpErrorCategory {
    None,            ///< 没有错误。
    InvalidArgument, ///< 请求参数无效。
    Cancelled,       ///< 请求被取消。
    Dns,             ///< DNS 解析失败。
    Connect,         ///< 网络连接失败。
    Proxy,           ///< 代理相关错误。
    Tls,             ///< TLS 或证书相关错误。
    Timeout,         ///< 请求超时。
    Upload,          ///< 上传过程错误。
    Download,        ///< 下载过程错误。
    Http,            ///< HTTP 状态相关错误。
    RateLimited,     ///< 客户端限流拒绝或取消。
    CircuitOpen,     ///< Circuit Breaker 处于熔断状态。
    Internal         ///< 封装层或扩展回调内部错误。
};

/// 代理配置。
struct ProxyOptions {
    std::string url;                   ///< 代理地址。
    std::optional<ProxyType> type;     ///< 可选代理类型；为空时由 libcurl 判断。
    std::string username;              ///< 代理用户名。
    std::string password;              ///< 代理密码。
    std::string noProxy;               ///< 不经过代理的主机列表。
    unsigned long auth = CURLAUTH_ANY; ///< 代理认证方式位掩码。
};

/// TLS 与证书配置。
struct TlsOptions {
    bool verifyPeer = true;    ///< 是否验证证书链。
    bool verifyHost = true;    ///< 是否验证证书主机名。
    bool verifyStatus = false; ///< 是否启用 OCSP Stapling 验证。
    CertificateRevocationPolicy revocationPolicy =
        CertificateRevocationPolicy::BestEffort; ///< Schannel 证书吊销检查策略。
    TlsVersion minVersion = TlsVersion::Tls1_2;  ///< 最低 TLS 版本；生产默认强制 TLS 1.2，旧协议必须显式降级。
    TlsVersion maxVersion = TlsVersion::Default; ///< 最高 TLS 版本。
    std::string caFile;                          ///< 自定义 CA 文件。
    std::string caPath;                          ///< 自定义 CA 目录。
    std::string clientCertificate;               ///< 客户端证书路径。
    std::string clientCertificateType;           ///< 客户端证书类型。
    std::string clientKey;                       ///< 客户端私钥路径。
    std::string clientKeyType;                   ///< 客户端私钥类型。
    std::string clientKeyPassword;               ///< 客户端私钥密码。
    std::string pinnedPublicKey;                 ///< 公钥 Pinning 值或文件路径。
    std::string cipherList;                      ///< TLS 1.2 及以下 Cipher 列表。
    std::string tls13CipherList;                 ///< TLS 1.3 Cipher 列表。
    std::string crlFile;                         ///< CRL 文件路径。
#ifdef _WIN32
    bool useNativeCa = true; ///< Windows 默认请求使用系统原生 CA Store；显式 caFile/caPath 时不额外叠加 Native CA bit。
#else
    bool useNativeCa = false; ///< 非 Windows 默认不改变现有 CA Trust Store 策略。
#endif
};

/// 重试策略。
struct RetryPolicy {
    int maxRetries = 3;                       ///< 最大重试次数，不包含首次请求。
    std::chrono::milliseconds baseDelay{500}; ///< 基础重试延迟。
    std::chrono::milliseconds maxDelay{8000}; ///< 最大重试延迟。
    bool exponentialBackoff = true;           ///< 是否指数退避。
    bool jitter = true;                       ///< 是否加入随机抖动。
    bool respectRetryAfter = true;            ///< 是否遵循 Retry-After。
    bool retryNonIdempotent = false;          ///< 是否允许重试非幂等请求。
    std::vector<CURLcode> retryCurlCodes;     ///< 可重试 CURLcode 列表。
    std::vector<long> retryHttpStatusCodes;   ///< 可重试 HTTP 状态码列表。
    std::function<std::optional<bool>(_In_ CURLcode, _In_ long, _In_ int)> customShouldRetry; ///< 自定义重试判定。

    /// 创建默认重试策略。
    RetryPolicy();
};

/// 重定向策略。
struct RedirectOptions {
    bool follow = true;            ///< 是否自动跟随重定向。
    long maxRedirects = 10;        ///< 最大重定向次数。
    bool autoReferer = false;      ///< 是否自动更新 Referer，默认关闭以避免无意泄露完整来源 URL。
    bool allowHttpsToHttp = false; ///< 是否允许自动重定向目标使用 HTTP；生产默认关闭，重定向目标仅允许 HTTPS。需要兼容
                                   ///< HTTP 重定向时必须显式开启。
    bool forwardAuthToOtherHosts = false; ///< 是否向其他主机转发 libcurl 标准认证信息。
    bool forwardSensitiveHeadersToOtherHosts =
        false; ///< 是否允许自定义敏感 Header 进入自动重定向链；默认关闭时首跳仍正常发送，真实出现重定向后会在下一跳前
               ///< fail-closed，避免潜在跨主机泄露。
    bool allowExplicitCookiesOnRedirects =
        false; ///< 是否允许 RequestOptions::cookies
               ///< 进入自动重定向链；默认关闭时首跳仍正常发送，真实出现重定向后会在下一跳前 fail-closed。
    bool allowUnpinnedRedirects = false; ///< 配置公钥 Pinning 时是否允许进入自动重定向链；默认关闭时首跳仍执行 Pin
                                         ///< 校验，真实出现重定向后在下一跳前 fail-closed。
};

/// 调试与跟踪配置。
struct DebugOptions {
    bool enabled = false;                      ///< 是否启用封装层日志。
    bool logRequestHeaders = true;             ///< 是否记录请求 Header。
    bool logRequestBody = false;               ///< 是否记录请求 Body。
    bool logResponseHeaders = true;            ///< 是否记录响应 Header。
    bool logResponseBody = false;              ///< 是否记录响应 Body。
    bool hideSensitiveHeaders = true;          ///< 是否自动脱敏敏感 Header。
    bool hideSensitiveUrlData = true;          ///< 是否自动脱敏 URL 用户信息和常见敏感查询参数。
    bool curlVerbose = false;                  ///< 是否启用经封装层脱敏过滤的 libcurl verbose。
    std::size_t maxBodyLogBytes = 4096;        ///< 单次最多记录的 Body 字节数。
    std::vector<std::string> sensitiveHeaders; ///< 额外敏感 Header 名称；同时用于 Debug 脱敏和跨主机重定向保护。
    std::vector<std::string> sensitiveQueryParameters; ///< 额外敏感 URL 查询参数名称。
    std::function<void(const std::string&)> logger;    ///< 自定义日志输出回调。
};

/// DNS 强制解析项。
struct DnsResolveEntry {
    std::string host;       ///< 域名。
    std::uint16_t port = 0; ///< 端口。
    std::string address;    ///< 强制解析 IP。
};

/// 网络层配置。
struct NetworkOptions {
    std::vector<DnsResolveEntry> resolve; ///< 静态 DNS 解析项。
    std::string interfaceName;            ///< 指定出口网卡或本地地址。
    long localPort = 0;                   ///< 本地源端口，0 表示自动。
    long localPortRange = 1;              ///< 本地端口尝试范围。
};

/// 上传下载限速配置。
struct TransferSpeedOptions {
    curl_off_t maxDownloadBytesPerSecond = 0; ///< 最大下载速度，0 表示不限。
    curl_off_t maxUploadBytesPerSecond = 0;   ///< 最大上传速度，0 表示不限。
};

/// Multipart 表单字段。
struct MultipartPart {
    std::string name;        ///< 字段名称。
    std::string data;        ///< 普通字段数据。
    std::string filePath;    ///< 文件字段本地路径。
    std::string fileName;    ///< 可选上传文件名。
    std::string contentType; ///< 可选内容类型。

    /// 创建普通字段。
    /// @param name 字段名称。
    /// @param value 字段值。
    static MultipartPart Field(_In_ std::string name, _In_ std::string value);

    /// 创建文件字段。
    /// @param name 字段名称。
    /// @param filePath 本地文件路径。
    /// @param contentType 可选 Content-Type。
    /// @param fileName 可选上传文件名。
    static MultipartPart File(_In_ std::string name, _In_ std::string filePath, _In_ std::string contentType = {},
                              _In_ std::string fileName = {});

    /// 判断是否为文件字段。
    bool IsFile() const noexcept {
        return !filePath.empty();
    }
};

/// 流式上传数据源接口。
class IUploadSource {
  public:
    /// 释放上传源。
    virtual ~IUploadSource() = default;

    /// 读取上传数据。
    /// @param buffer 输出缓冲区。
    /// @param capacity 缓冲区容量。
    virtual std::size_t Read(_Out_ char* buffer, _In_ std::size_t capacity) = 0;

    /// 将读取位置回到开头。
    virtual bool Rewind() = 0;

    /// 获取上传总大小，未知时返回空。
    virtual std::optional<curl_off_t> GetSize() const = 0;
};

/// 内存流式上传源。
class MemoryUploadSource final : public IUploadSource {
  public:
    /// 使用内存数据创建上传源。
    /// @param data 上传数据。
    explicit MemoryUploadSource(_In_ std::string data);
    /// @param buffer 读写缓冲区。
    /// @param capacity 缓冲区容量。
    std::size_t Read(_Out_ char* buffer, _In_ std::size_t capacity) override; ///< 读取数据。
    bool Rewind() override;                                                   ///< 回到开头。
    std::optional<curl_off_t> GetSize() const override;                       ///< 获取总大小。
  private:
    std::string data_;       ///< 内存数据。
    std::size_t offset_ = 0; ///< 当前读取位置。
};

/// 回调式流式上传源。
class CallbackUploadSource final : public IUploadSource {
  public:
    using ReadCallback = std::function<std::size_t(char*, _In_ std::size_t)>; ///< 数据读取回调。
    using RewindCallback = std::function<bool()>;                             ///< 回退回调。

    /// 使用回调创建上传源。
    /// @param readCallback 数据读取回调。
    /// @param rewindCallback 可选回退回调。
    /// @param size 可选总大小。
    CallbackUploadSource(_In_ ReadCallback readCallback, _In_ RewindCallback rewindCallback = {},
                         _In_ std::optional<curl_off_t> size = std::nullopt);
    /// @param buffer 读写缓冲区。
    /// @param capacity 缓冲区容量。
    std::size_t Read(_Out_ char* buffer, _In_ std::size_t capacity) override; ///< 调用读取回调。
    bool Rewind() override;                                                   ///< 调用回退回调。
    std::optional<curl_off_t> GetSize() const override;                       ///< 返回配置总大小。
  private:
    ReadCallback read_;              ///< 数据读取回调。
    RewindCallback rewind_;          ///< 回退回调。
    std::optional<curl_off_t> size_; ///< 可选总大小。
};

/// 文件流式上传源。
class FileUploadSource final : public IUploadSource {
  public:
    /// 打开文件作为上传源。
    /// @param filePath 文件路径。
    explicit FileUploadSource(_In_ std::string filePath);
    ~FileUploadSource() override; ///< 关闭文件。
    /// @param buffer 读写缓冲区。
    /// @param capacity 缓冲区容量。
    std::size_t Read(_Out_ char* buffer, _In_ std::size_t capacity) override; ///< 读取文件。
    bool Rewind() override;                                                   ///< 回到文件开头。
    std::optional<curl_off_t> GetSize() const override;                       ///< 获取文件大小。
    bool IsValid() const noexcept;                                            ///< 判断文件是否成功打开。

    /// @brief 调用 GetPath 完成对应操作。
    const std::string& GetPath() const noexcept {
        return filePath_;
    } ///< 获取文件路径。
  private:
    struct Impl;                 ///< 文件实现细节。
    std::unique_ptr<Impl> impl_; ///< 文件流实现。
    std::string filePath_;       ///< 文件路径。
};

using CurlOptionsCallback = std::function<void(CURL*)>; ///< 原生 CURL 配置回调。
using ProgressCallback =
    std::function<bool(_In_ curl_off_t, _In_ curl_off_t, _In_ curl_off_t, _In_ curl_off_t)>; ///< 进度回调。
using ResponseChunkCallback = std::function<bool(const char*, _In_ std::size_t)>;            ///< 响应数据块回调。
using ResponseRetryResetCallback = std::function<bool()>; ///< 外部响应接收器在自动重试前的重置回调。

/// 单次 HTTP 请求配置。
struct RequestOptions {
    HttpHeaders headers;                         ///< 请求头部。
    std::vector<std::string> rawHeaderLines;     ///< 原生 HTTP 头部行。
    HttpCookies cookies;                         ///< 显式请求 Cookie。
    std::optional<ProxyOptions> proxy;           ///< 可选代理配置。
    TlsOptions tls;                              ///< TLS 配置。
    RetryPolicy retry;                           ///< 重试策略。
    RedirectOptions redirect;                    ///< 重定向策略。
    DebugOptions debug;                          ///< 调试配置。
    NetworkOptions network;                      ///< 网络层配置。
    TransferSpeedOptions speed;                  ///< 传输限速配置。
    HttpVersion httpVersion = HttpVersion::Auto; ///< HTTP 版本偏好。

    std::chrono::milliseconds connectTimeout{6000}; ///< 连接超时。
    std::chrono::milliseconds timeout{0}; ///< 单次传输尝试总超时，0 表示不限制；需要时建议业务按场景显式设置。
    long lowSpeedLimitBytesPerSecond = 0; ///< 低速判定阈值，0 表示关闭低速中止。
    std::chrono::seconds lowSpeedTime{0}; ///< 低速持续时间，0 表示关闭低速中止。

    bool autoDecompress = true;         ///< 是否自动解压响应。
    std::string acceptEncoding;         ///< 自定义 Accept-Encoding。
    bool failOnHttpError = false;       ///< 是否把 HTTP >=400 转为 CURL 错误。
    bool tcpKeepAlive = true;           ///< 是否启用 TCP KeepAlive。
    long tcpKeepIdleSeconds = 30;       ///< KeepAlive 空闲时间。
    long tcpKeepIntervalSeconds = 10;   ///< KeepAlive 探测间隔。
    bool disallowUsernameInUrl = false; ///< 是否禁止 URL 携带用户名密码，默认允许以保持常规兼容性。

    bool enableCookieEngine = true; ///< 是否启用 libcurl Cookie Engine。
    bool newCookieSession = false;  ///< 是否开始新的 Cookie Session。
    std::string cookieFile;         ///< Cookie 读取文件。
    std::string cookieJar;          ///< Cookie 持久化文件。

    std::string userAgent;                 ///< 请求使用的 User-Agent 字符串。
    std::string username;                  ///< 原始服务器认证用户名。
    std::string password;                  ///< 原始服务器认证密码。
    unsigned long httpAuth = CURLAUTH_ANY; ///< HTTP 认证方式位掩码。
    std::string bearerToken;               ///< 直接设置 Bearer Token。

    std::size_t maxResponseSize = 0; ///< 整个响应 Body 最大字节数，0 表示不限；流式或文件下载可按业务显式设置。
    std::size_t maxInMemoryResponseSize =
        128U * 1024U * 1024U; ///< content 内存缓存上限，默认 128 MiB，0 表示不限；不影响纯流式/文件下载。
    std::size_t maxResponseHeaderSize =
        2U * 1024U * 1024U;                      ///< 单次尝试累计响应 Header/Trailer 上限，默认 2 MiB，0 表示不限。
    bool storeResponseBody = true;               ///< 是否保存响应 Body 到 content。
    ResponseChunkCallback responseChunkCallback; ///< 响应数据块回调。
    std::ostream* responseStream = nullptr;      ///< 可选响应输出流，生命周期由调用方负责。
    ResponseRetryResetCallback
        responseRetryResetCallback; ///< 已向外部接收器写入数据后，自动重试前用于回滚接收状态；为空时为避免重复数据不会重试。

    std::atomic_bool* cancelFlag = nullptr; ///< 可选取消标记，生命周期由调用方负责。
    ProgressCallback progressCallback;      ///< 传输进度回调。

    std::vector<MultipartPart> multipart;        ///< 多部分表单字段列表。
    std::shared_ptr<IUploadSource> uploadSource; ///< 可选流式上传源。
    std::optional<curl_off_t> uploadSize;        ///< 可选显式上传大小。

    std::string range;                    ///< Range 范围，例如 0-1023。
    std::optional<curl_off_t> resumeFrom; ///< 可选下载断点续传偏移；Download() 会校验本地文件大小后追加写入。

    CurlOptionsCallback nativeCurlOptions; ///< 最后执行的原生 CURL 配置接口。
};

/// 完整 HTTP 请求模型。
struct HttpRequestData {
    std::string method = "GET";                        ///< HTTP Method 文本。
    std::string url;                                   ///< 请求 URL。
    std::string body;                                  ///< 内存请求 Body。
    std::string requestId;                             ///< 请求追踪 ID，为空时自动生成。
    std::unordered_map<std::string, std::string> tags; ///< 用户自定义元数据。
};

// ============================================================
// HTTP 响应
// ============================================================

/// 单个响应 Header 块。
struct HttpHeaderBlock {
    long statusCode = 0;    ///< Header 块状态码。
    std::string statusLine; ///< HTTP 状态行。
    std::string rawHeaders; ///< 原始 Header 文本。
    HttpHeaders headers;    ///< 结构化 Header。
};

/// HTTP 响应数据。
struct HttpResponse {
    bool success = false;        ///< 传输层是否成功完成。
    std::string error;           ///< 错误描述。
    std::string content;         ///< 响应体。
    long code = 0;               ///< 最终 HTTP 状态码。
    std::string originalHeaders; ///< 全部原始 Header 块。
    HttpHeaders headers;         ///< 最终响应 Header，Trailer 也会合并到此处便于统一查询。
    std::optional<std::chrono::system_clock::time_point> date; ///< 最终响应头中的Date。
    HttpHeaders trailers;                                      ///< HTTP Trailer 头部。
    std::string rawTrailers;                                   ///< 原始 HTTP Trailer 文本。
    HttpCookies cookies;                                       ///< 最终响应 Set-Cookie。
    CURLcode curlCode = CURLE_OK;                              ///< 原始 CURLcode。
    HttpErrorCategory errorCategory = HttpErrorCategory::None; ///< 封装层错误分类。

    std::string requestId;                      ///< 请求追踪 ID。
    std::string finalRawHeaders;                ///< 最终响应原始 Header。
    std::vector<HttpHeaderBlock> headerHistory; ///< 重定向 Header 历史。
    std::string effectiveUrl;                   ///< 最终有效 URL。
    std::string contentType;                    ///< 响应 Content-Type。
    std::string primaryIp;                      ///< 实际远端 IP。
    std::string negotiatedHttpVersion;          ///< 实际 HTTP 版本。
    long redirectCount = 0;                     ///< 重定向次数。
    long proxyConnectCode = 0;                  ///< HTTP 代理 CONNECT 隧道响应码。
    int attempts = 0;                           ///< 请求尝试次数。
    curl_off_t downloadedBytes = 0;             ///< 下载字节数。
    curl_off_t uploadedBytes = 0;               ///< 上传字节数。

    std::chrono::microseconds totalTime{0};        ///< 总耗时。
    std::chrono::microseconds nameLookupTime{0};   ///< DNS 耗时。
    std::chrono::microseconds connectTime{0};      ///< TCP 连接耗时。
    std::chrono::microseconds tlsHandshakeTime{0}; ///< TLS 握手时间点。
    std::chrono::microseconds firstByteTime{0};    ///< 首字节时间。

    /// 判断 CURL 传输是否成功。
    bool IsTransportSuccessful() const noexcept {
        return success && curlCode == CURLE_OK;
    }

    /// 判断传输成功且 HTTP 状态码属于 2xx。
    bool IsSuccessful() const noexcept {
        return IsTransportSuccessful() && code >= 200 && code < 300;
    }
};

// ============================================================
// Transport 与 Mock
// ============================================================

/// HTTP 传输层抽象接口。
class IHttpTransport {
  public:
    /// 释放传输层。
    virtual ~IHttpTransport() = default;

    /// 执行请求。
    /// @param request 请求模型。
    /// @param options 请求配置。
    virtual HttpResponse Send(_In_ const HttpRequestData& request, _In_ const RequestOptions& options) = 0;
};

/// 基于 libcurl easy handle 的真实传输层。
class CurlTransport final : public IHttpTransport {
  public:
    /// 创建可复用 CURL 传输层。
    CurlTransport();
    ~CurlTransport() override;                               ///< 释放 CURL handle。
    CurlTransport(const CurlTransport&) = delete;            ///< 禁止复制。
    CurlTransport& operator=(const CurlTransport&) = delete; ///< 禁止复制赋值。

    /// 执行真实网络请求。
    /// @param request 请求模型。
    /// @param options 请求配置。
    HttpResponse Send(_In_ const HttpRequestData& request, _In_ const RequestOptions& options) override;

    /// 判断底层 CURL handle 是否有效。
    bool IsValid() const noexcept;

    /// 获取底层 CURL handle；调用方不得与 Send() 并发操作该 handle。
    CURL* GetNativeHandle() noexcept;

    /// 获取 Cookie Engine 列表。
    std::vector<std::string> GetCookieList() const;

    /// 清空全部 Cookie。
    bool ClearCookies();

    /// 清空 Session Cookie。
    bool ClearSessionCookies();

    /// 刷新 Cookie Jar 文件。
    bool FlushCookies();

  private:
    struct Impl;                 ///< CURL 传输实现细节。
    std::unique_ptr<Impl> impl_; ///< 传输实现。
};

using MockPredicate = std::function<bool(const HttpRequestData&, const RequestOptions&)>;         ///< Mock 匹配函数。
using MockResponder = std::function<HttpResponse(const HttpRequestData&, const RequestOptions&)>; ///< Mock 响应函数。

/// 可编程 Mock 传输层。
class MockTransport final : public IHttpTransport {
  public:
    /// 创建空 Mock 传输层。
    MockTransport();
    ~MockTransport() override; ///< 释放 Mock 规则。

    /// 添加动态 Mock 规则。
    /// @param predicate 请求匹配函数。
    /// @param responder 响应生成函数。
    void AddRule(_In_ MockPredicate predicate, _In_ MockResponder responder);

    /// 添加 Method + URL 精确匹配的固定响应。
    /// @param method HTTP 请求方法。
    /// @param url 请求 URL。
    /// @param response 固定响应。
    void AddStaticResponse(_In_ std::string method, _In_ std::string url, _In_ HttpResponse response);

    /// 清空全部 Mock 规则。
    void Clear();

    /// 根据规则执行请求。
    /// @param request 请求模型。
    /// @param options 请求配置。
    HttpResponse Send(_In_ const HttpRequestData& request, _In_ const RequestOptions& options) override;

  private:
    struct Impl;                 ///< Mock 实现细节。
    std::unique_ptr<Impl> impl_; ///< Mock 实现。
};

// ============================================================
// 认证、Middleware、Metrics 与客户端策略
// ============================================================

/// HTTP 认证提供器接口。
class IAuthProvider {
  public:
    /// 释放认证提供器。
    virtual ~IAuthProvider() = default;

    /// 在请求发送前应用认证信息。
    /// @param request 请求模型。
    /// @param options 请求配置。
    virtual void Apply(_Out_ HttpRequestData& request, _Inout_ RequestOptions& options) = 0;

    /// 判断当前响应是否适合刷新认证。
    /// @param response 当前响应。
    virtual bool CanRefresh(_In_ const HttpResponse& response) const;

    /// 刷新认证信息。
    virtual bool Refresh();

    /// 获取认证信息版本。
    virtual std::uint64_t Version() const noexcept;
};

/// Basic Auth 认证提供器。
class BasicAuthProvider final : public IAuthProvider {
  public:
    /// 创建 Basic Auth。
    /// @param username 用户名。
    /// @param password 密码。
    /// @param auth libcurl 认证位掩码。
    BasicAuthProvider(_In_ std::string username, _In_ std::string password, _In_ unsigned long auth = CURLAUTH_BASIC);

    /// 应用 Basic Auth。
    /// @param request 请求模型。
    /// @param options 请求配置。
    void Apply(_Out_ HttpRequestData& request, _Inout_ RequestOptions& options) override;

  private:
    std::string username_; ///< 用户名。
    std::string password_; ///< 密码。
    unsigned long auth_;   ///< 认证方式位掩码。
};

/// Bearer Token 认证提供器。
class BearerAuthProvider final : public IAuthProvider {
  public:
    using RefreshCallback = std::function<std::optional<std::string>()>; ///< Token 刷新回调。

    /// 创建 Bearer Auth。
    /// @param token 初始 Token。
    /// @param refreshCallback 可选刷新回调。
    BearerAuthProvider(_In_ std::string token, _In_ RefreshCallback refreshCallback = {});

    /// 应用 Bearer Token。
    /// @param request 请求模型。
    /// @param options 请求配置。
    void Apply(_Out_ HttpRequestData& request, _Inout_ RequestOptions& options) override;

    /// 判断响应是否允许刷新。
    /// @param response 当前响应。
    bool CanRefresh(_In_ const HttpResponse& response) const override;

    /// 调用刷新回调更新 Token。
    bool Refresh() override;

    /// 获取 Token 版本。
    std::uint64_t Version() const noexcept override;

    /// 主动更新 Token。
    /// @param token 新 Token。
    void SetToken(_In_ std::string token);

    /// 获取当前 Token 副本。
    std::string GetToken() const;

  private:
    mutable std::mutex mutex_;        ///< 保护 Token。
    std::mutex refreshMutex_;         ///< 合并并发 Token 刷新，避免多个客户端同时触发刷新风暴。
    std::string token_;               ///< 当前 Token。
    RefreshCallback refresh_;         ///< Token 刷新回调。
    std::atomic_uint64_t version_{1}; ///< Token 版本。
};

/// API Key 放置位置。
enum class ApiKeyLocation {
    Header, ///< 将 API Key 写入请求头部。
    Query   ///< 将 API Key 写入查询参数。
};

/// API Key 认证提供器。
class ApiKeyAuthProvider final : public IAuthProvider {
  public:
    /// 创建 API Key Auth。
    /// @param name Header 或查询参数名称。
    /// @param value API Key 值。
    /// @param location 放置位置；非法枚举值会抛出 std::invalid_argument。
    ApiKeyAuthProvider(_In_ std::string name, _In_ std::string value,
                       _In_ ApiKeyLocation location = ApiKeyLocation::Header);

    /// 应用 API Key。
    /// @param request 请求模型。
    /// @param options 请求配置。
    void Apply(_Out_ HttpRequestData& request, _Inout_ RequestOptions& options) override;

  private:
    std::string name_;        ///< Header 或参数名称。
    std::string value_;       ///< API Key 值。
    ApiKeyLocation location_; ///< 放置位置。
};

/// Middleware 接口。
class IHttpMiddleware {
  public:
    /// 释放 Middleware。
    virtual ~IHttpMiddleware() = default;

    /// 请求发送前执行；返回 false 可短路请求。
    /// @param request 请求模型。
    /// @param options 请求配置。
    /// @param earlyResponse 短路时使用的响应。
    virtual bool Before(_Out_ HttpRequestData& request, _Inout_ RequestOptions& options,
                        _Inout_ HttpResponse& earlyResponse);

    /// 请求完成后执行。
    /// @param request 请求模型。
    /// @param options 请求配置。
    /// @param response 请求响应。
    virtual void After(_In_ const HttpRequestData& request, _In_ const RequestOptions& options,
                       _Out_ HttpResponse& response);
};

using RequestInterceptor = std::function<void(HttpRequestData&, RequestOptions&)>; ///< 请求前快捷拦截器。
using ResponseInterceptor =
    std::function<void(const HttpRequestData&, const RequestOptions&, HttpResponse&)>; ///< 响应后快捷拦截器。

/// 客户端限流策略。
struct RateLimitPolicy {
    double requestsPerSecond = 0.0; ///< 每秒最大请求数，0 表示不限。
    std::size_t burst = 1;          ///< Token Bucket 突发容量。
    std::size_t maxConcurrent = 0;  ///< 最大并发请求数，0 表示不限。
};

/// 客户端熔断策略。
struct CircuitBreakerPolicy {
    std::size_t failureThreshold = 0;              ///< 连续失败多少次后熔断，0 表示关闭。
    std::chrono::milliseconds openDuration{30000}; ///< 熔断保持时间。
    std::size_t halfOpenMaxRequests = 1;           ///< 半开状态探测请求数。
    bool countHttp5xx = true;                      ///< 是否把 HTTP 5xx 计为失败。
};

/// 单次 Metrics 事件。
struct HttpMetricsEvent {
    std::string method;                     ///< HTTP 请求方法。
    std::string url;                        ///< 请求 URL。
    long statusCode = 0;                    ///< HTTP 状态码。
    CURLcode curlCode = CURLE_OK;           ///< libcurl 错误码。
    bool transportOk = false;               ///< 传输是否成功。
    int attempts = 0;                       ///< 请求尝试次数。
    curl_off_t downloadedBytes = 0;         ///< 下载字节数。
    curl_off_t uploadedBytes = 0;           ///< 上传字节数。
    std::chrono::microseconds totalTime{0}; ///< 总耗时。
};

/// Metrics 收集器接口。
class IMetricsCollector {
  public:
    /// 释放 Metrics 收集器。
    virtual ~IMetricsCollector() = default;

    /// 接收请求完成事件。
    /// @param event Metrics 事件。
    virtual void OnRequestCompleted(_In_ const HttpMetricsEvent& event) = 0;
};

/// 内置 Metrics 快照。
struct HttpMetricsSnapshot {
    std::uint64_t totalRequests = 0;    ///< 总请求数。
    std::uint64_t transportSuccess = 0; ///< 传输成功数。
    std::uint64_t http2xx = 0;          ///< 2xx 数量。
    std::uint64_t http3xx = 0;          ///< 3xx 数量。
    std::uint64_t http4xx = 0;          ///< 4xx 数量。
    std::uint64_t http5xx = 0;          ///< 5xx 数量。
    std::uint64_t failedRequests = 0;   ///< 失败请求数。
    std::uint64_t totalRetries = 0;     ///< 总重试次数。
    std::uint64_t downloadedBytes = 0;  ///< 总下载字节数。
    std::uint64_t uploadedBytes = 0;    ///< 总上传字节数。
};

/// 内置线程安全 Metrics 收集器。
class BasicMetricsCollector final : public IMetricsCollector {
  public:
    /// 创建空统计器。
    BasicMetricsCollector();
    ~BasicMetricsCollector() override; ///< 释放统计器。

    /// 累积请求指标。
    /// @param event 请求完成事件。
    void OnRequestCompleted(_In_ const HttpMetricsEvent& event) override;

    /// 获取当前指标快照。
    HttpMetricsSnapshot GetSnapshot() const;

    /// 清零全部指标。
    void Reset();

  private:
    struct Impl;                 ///< Metrics 实现细节。
    std::unique_ptr<Impl> impl_; ///< Metrics 实现。
};

/// 可复用客户端长期配置。
struct HttpClientOptions {
    std::shared_ptr<IAuthProvider> authProvider;              ///< 可选认证提供器。
    std::vector<std::shared_ptr<IHttpMiddleware>> middleware; ///< Middleware 列表。
    std::vector<RequestInterceptor> requestInterceptors;      ///< 请求前拦截器。
    std::vector<ResponseInterceptor> responseInterceptors;    ///< 响应后拦截器。
    RateLimitPolicy rateLimit;                                ///< 客户端限流策略。
    CircuitBreakerPolicy circuitBreaker;                      ///< 客户端熔断策略。
    std::shared_ptr<IMetricsCollector> metricsCollector;      ///< 可选 Metrics 收集器。
    bool refreshAuthOnUnauthorized = true;                    ///< 401 时是否自动刷新认证并重放一次。
    long maxTotalConnections = 0;                             ///< curl_multi 最大总连接数。
    long maxHostConnections = 0;                              ///< curl_multi 单主机最大连接数。
    long maxConcurrentStreams = 0;                            ///< HTTP/2/3 最大并发流建议值。
};

// ============================================================
// 高频一次性请求 API
// ============================================================

/// 一次性同步 HTTP 请求入口。
class HttpRequest {
  public:
    /// 使用完整请求模型发送请求。
    /// @param request 请求模型。
    /// @param options 请求配置。
    static HttpResponse Send(_In_ HttpRequestData request, _In_ const RequestOptions& options = {});

    /// 使用常用 Method 发送请求。
    /// @param method HTTP 请求方法。
    /// @param url 请求地址。
    /// @param body 请求 Body。
    /// @param options 请求配置。
    static HttpResponse Request(_In_ HttpMethod method, _In_ std::string_view url, _In_ std::string_view body = {},
                                _In_ const RequestOptions& options = {});

    /// 使用任意 Method 文本发送请求。
    /// @param method Method 文本。
    /// @param url 请求地址。
    /// @param body 请求 Body。
    /// @param options 请求配置。
    static HttpResponse Request(_In_ std::string_view method, _In_ std::string_view url,
                                _In_ std::string_view body = {}, _In_ const RequestOptions& options = {});

    /// 发送 GET。@param url 请求地址。@param options 请求配置。
    static HttpResponse Get(_In_ std::string_view url, _In_ const RequestOptions& options = {});
    /// 发送 HEAD。@param url 请求地址。@param options 请求配置。
    static HttpResponse Head(_In_ std::string_view url, _In_ const RequestOptions& options = {});
    /// 发送 POST。@param url 请求地址。@param body 请求 Body。@param options 请求配置。
    static HttpResponse Post(_In_ std::string_view url, _In_ std::string_view body = {},
                             _In_ const RequestOptions& options = {});
    /// 发送 PUT。@param url 请求地址。@param body 请求 Body。@param options 请求配置。
    static HttpResponse Put(_In_ std::string_view url, _In_ std::string_view body = {},
                            _In_ const RequestOptions& options = {});
    /// 发送 DELETE。@param url 请求地址。@param body 可选 Body。@param options 请求配置。
    static HttpResponse Delete(_In_ std::string_view url, _In_ std::string_view body = {},
                               _In_ const RequestOptions& options = {});
    /// 发送 PATCH。@param url 请求地址。@param body 请求 Body。@param options 请求配置。
    static HttpResponse Patch(_In_ std::string_view url, _In_ std::string_view body = {},
                              _In_ const RequestOptions& options = {});
    /// 发送 OPTIONS。@param url 请求地址。@param options 请求配置。
    static HttpResponse Options(_In_ std::string_view url, _In_ const RequestOptions& options = {});
    /// 发送 TRACE。@param url 请求地址。@param options 请求配置。
    static HttpResponse Trace(_In_ std::string_view url, _In_ const RequestOptions& options = {});
    /// 发送 CONNECT。@param url 请求地址。@param options 请求配置。
    static HttpResponse Connect(_In_ std::string_view url, _In_ const RequestOptions& options = {});

    /// 发送任意 Method JSON。@param method Method。@param url 请求地址。@param json JSON 文本。@param options
    /// 请求配置。
    static HttpResponse RequestJson(_In_ std::string_view method, _In_ std::string_view url, _In_ std::string_view json,
                                    _In_ RequestOptions options = {});
    /// 发送 JSON POST。@param url 请求地址。@param json JSON 文本。@param options 请求配置。
    static HttpResponse PostJson(_In_ std::string_view url, _In_ std::string_view json,
                                 _In_ RequestOptions options = {});
    /// 发送 JSON PUT。@param url 请求地址。@param json JSON 文本。@param options 请求配置。
    static HttpResponse PutJson(_In_ std::string_view url, _In_ std::string_view json,
                                _In_ RequestOptions options = {});
    /// 发送 JSON PATCH。@param url 请求地址。@param json JSON 文本。@param options 请求配置。
    static HttpResponse PatchJson(_In_ std::string_view url, _In_ std::string_view json,
                                  _In_ RequestOptions options = {});
    /// 发送表单 POST。@param url 请求地址。@param form 表单参数。@param options 请求配置。
    static HttpResponse PostForm(_In_ std::string_view url, _In_ const UrlParameters& form,
                                 _In_ RequestOptions options = {});
    /// 上传 Multipart。@param url 请求地址。@param parts 表单字段。@param options 请求配置。
    static HttpResponse UploadMultipart(_In_ std::string_view url, _In_ std::vector<MultipartPart> parts,
                                        _In_ RequestOptions options = {});
    /// 流式上传文件。@param url 请求地址。@param filePath 文件路径。@param options 请求配置。@param method 上传
    /// Method。
    static HttpResponse UploadFile(_In_ std::string_view url, _In_ const std::string& filePath,
                                   _In_ RequestOptions options = {}, _In_ std::string_view method = "PUT");
    /// 下载文件。@param url 请求地址。@param filePath 保存路径。@param options 请求配置。
    static HttpResponse Download(_In_ std::string_view url, _In_ const std::string& filePath,
                                 _In_ RequestOptions options = {});
};

// ============================================================
// Session 同步客户端
// ============================================================

/// 可复用 Cookie、连接、DNS 和 TLS Session 的同步客户端。
class HttpClient {
  public:
    /// 使用默认配置创建客户端。
    HttpClient();

    /// 使用默认请求配置创建客户端。
    /// @param defaultOptions 默认请求配置。
    explicit HttpClient(_In_ RequestOptions defaultOptions);

    /// 使用客户端配置和自定义 Transport 创建客户端。
    /// @param clientOptions 客户端长期配置。
    /// @param transport 自定义 Transport；为空时使用 CurlTransport。
    explicit HttpClient(_In_ HttpClientOptions clientOptions, _In_ std::shared_ptr<IHttpTransport> transport = {});

    /// 使用完整配置创建客户端。
    /// @param defaultOptions 默认请求配置。
    /// @param clientOptions 客户端长期配置。
    /// @param transport 自定义 Transport；为空时使用 CurlTransport。
    HttpClient(_In_ RequestOptions defaultOptions, _In_ HttpClientOptions clientOptions,
               _In_ std::shared_ptr<IHttpTransport> transport = {});

    ~HttpClient();                                     ///< 释放客户端。
    HttpClient(const HttpClient&) = delete;            ///< 禁止复制。
    HttpClient& operator=(const HttpClient&) = delete; ///< 禁止复制赋值。
    HttpClient(_Inout_ HttpClient&& other) noexcept; ///< 支持移动构造。
    HttpClient& operator=(_Inout_ HttpClient&& other) noexcept; ///< 支持移动赋值。

    /// 获取可写默认请求配置；仅建议在没有并发请求时直接修改该引用。
    RequestOptions& GetDefaultOptions() noexcept;
    /// 获取只读默认请求配置；返回引用本身不提供并发快照语义，必须保证读取期间没有 SetDefaultOptions()/可写
    /// GetDefaultOptions() 并发修改。
    const RequestOptions& GetDefaultOptions() const noexcept;
    /// 设置默认请求配置。@param options 新默认配置。
    void SetDefaultOptions(_In_ RequestOptions options);

    /// 获取只读客户端长期配置；返回引用本身不提供并发快照语义，必须保证读取期间没有 SetClientOptions()/Add*()
    /// 并发修改。
    const HttpClientOptions& GetClientOptions() const noexcept;
    /// 设置客户端长期配置。@param options 新客户端配置。
    void SetClientOptions(_In_ HttpClientOptions options);
    /// 替换 Transport。@param transport 新 Transport；为空时恢复 CurlTransport。
    void SetTransport(_In_ std::shared_ptr<IHttpTransport> transport);
    /// 添加 Middleware。@param middleware Middleware 对象。
    void AddMiddleware(_In_ std::shared_ptr<IHttpMiddleware> middleware);
    /// 添加请求拦截器。@param interceptor 拦截器回调。
    void AddRequestInterceptor(_In_ RequestInterceptor interceptor);
    /// 添加响应拦截器。@param interceptor 拦截器回调。
    void AddResponseInterceptor(_In_ ResponseInterceptor interceptor);

    /// 发送完整请求模型。@param request 请求模型。
    HttpResponse Send(_In_ HttpRequestData request);
    /// 使用指定配置发送完整请求模型。@param request 请求模型。@param options 请求配置。
    HttpResponse Send(_In_ HttpRequestData request, _In_ const RequestOptions& options);

    /// 使用默认配置发送常用 Method。@param method Method。@param url 请求地址。@param body 请求 Body。
    HttpResponse Request(_In_ HttpMethod method, _In_ std::string_view url, _In_ std::string_view body = {});
    /// 使用指定配置发送常用 Method。@param method Method。@param url 请求地址。@param body Body。@param options
    /// 请求配置。
    HttpResponse Request(_In_ HttpMethod method, _In_ std::string_view url, _In_ std::string_view body,
                         _In_ const RequestOptions& options);
    /// 使用默认配置发送任意 Method。@param method Method。@param url 请求地址。@param body 请求 Body。
    HttpResponse Request(_In_ std::string_view method, _In_ std::string_view url, _In_ std::string_view body = {});
    /// 使用指定配置发送任意 Method。@param method Method。@param url 请求地址。@param body Body。@param options
    /// 请求配置。
    HttpResponse Request(_In_ std::string_view method, _In_ std::string_view url, _In_ std::string_view body,
                         _In_ const RequestOptions& options);

    /// @param url 请求地址。
    HttpResponse Get(_In_ std::string_view url); ///< 发送 GET；url 为请求地址。
    /// @param url 请求地址。
    /// @param options 本次操作配置。
    HttpResponse Get(_In_ std::string_view url, _In_ const RequestOptions& options); ///< 发送 GET；options 为请求配置。
    /// @param url 请求地址。
    HttpResponse Head(_In_ std::string_view url); ///< 发送 HEAD；url 为请求地址。
    /// @param url 请求地址。
    /// @param options 本次操作配置。
    HttpResponse Head(_In_ std::string_view url,
                      _In_ const RequestOptions& options); ///< 发送 HEAD；options 为请求配置。
    /// @param url 请求地址。
    /// @param body 请求正文。
    HttpResponse Post(_In_ std::string_view url, _In_ std::string_view body = {}); ///< 发送 POST；body 为请求 Body。
    /// @param url 请求地址。
    /// @param body 请求正文。
    /// @param options 本次操作配置。
    HttpResponse Post(_In_ std::string_view url, _In_ std::string_view body,
                      _In_ const RequestOptions& options); ///< 发送 POST；options 为请求配置。
    /// @param url 请求地址。
    /// @param body 请求正文。
    HttpResponse Put(_In_ std::string_view url, _In_ std::string_view body = {}); ///< 发送 PUT；body 为请求 Body。
    /// @param url 请求地址。
    /// @param body 请求正文。
    /// @param options 本次操作配置。
    HttpResponse Put(_In_ std::string_view url, _In_ std::string_view body,
                     _In_ const RequestOptions& options); ///< 发送 PUT；options 为请求配置。
    /// @param url 请求地址。
    /// @param body 请求正文。
    HttpResponse Delete(_In_ std::string_view url,
                        _In_ std::string_view body = {}); ///< 发送 DELETE；body 为可选 Body。
    /// @param url 请求地址。
    /// @param body 请求正文。
    /// @param options 本次操作配置。
    HttpResponse Delete(_In_ std::string_view url, _In_ std::string_view body,
                        _In_ const RequestOptions& options); ///< 发送 DELETE；options 为请求配置。
    /// @param url 请求地址。
    /// @param body 请求正文。
    HttpResponse Patch(_In_ std::string_view url, _In_ std::string_view body = {}); ///< 发送 PATCH；body 为请求 Body。
    /// @param url 请求地址。
    /// @param body 请求正文。
    /// @param options 本次操作配置。
    HttpResponse Patch(_In_ std::string_view url, _In_ std::string_view body,
                       _In_ const RequestOptions& options); ///< 发送 PATCH；options 为请求配置。
    /// @param url 请求地址。
    HttpResponse Options(_In_ std::string_view url); ///< 发送 OPTIONS；url 为请求地址。
    /// @param url 请求地址。
    /// @param options 本次操作配置。
    HttpResponse Options(_In_ std::string_view url,
                         _In_ const RequestOptions& options); ///< 发送 OPTIONS；options 为请求配置。

    /// @param method HTTP 方法或操作名称。
    /// @param url 请求地址。
    HttpResponse RequestJson(_In_ std::string_view method, _In_ std::string_view url,
                             _In_ std::string_view json); ///< 使用默认配置发送任意 Method JSON。
    /// @param method HTTP 方法或操作名称。
    /// @param url 请求地址。
    /// @param options 本次操作配置。
    HttpResponse RequestJson(_In_ std::string_view method, _In_ std::string_view url, _In_ std::string_view json,
                             _In_ RequestOptions options); ///< 使用指定配置发送任意 Method JSON。
    /// @param url 请求地址。
    HttpResponse PostJson(_In_ std::string_view url, _In_ std::string_view json); ///< 使用默认配置发送 JSON POST。
    /// @param url 请求地址。
    /// @param options 本次操作配置。
    HttpResponse PostJson(_In_ std::string_view url, _In_ std::string_view json,
                          _In_ RequestOptions options); ///< 使用指定配置发送 JSON POST。
    /// @param url 请求地址。
    HttpResponse PutJson(_In_ std::string_view url, _In_ std::string_view json); ///< 使用默认配置发送 JSON PUT。
    /// @param url 请求地址。
    /// @param options 本次操作配置。
    HttpResponse PutJson(_In_ std::string_view url, _In_ std::string_view json,
                         _In_ RequestOptions options); ///< 使用指定配置发送 JSON PUT。
    /// @param url 请求地址。
    HttpResponse PatchJson(_In_ std::string_view url, _In_ std::string_view json); ///< 使用默认配置发送 JSON PATCH。
    /// @param url 请求地址。
    /// @param options 本次操作配置。
    HttpResponse PatchJson(_In_ std::string_view url, _In_ std::string_view json,
                           _In_ RequestOptions options); ///< 使用指定配置发送 JSON PATCH。
    /// @param url 请求地址。
    HttpResponse PostForm(_In_ std::string_view url, _In_ const UrlParameters& form); ///< 发送表单 POST。
    /// @param url 请求地址。
    /// @param options 本次操作配置。
    HttpResponse PostForm(_In_ std::string_view url, _In_ const UrlParameters& form,
                          _In_ RequestOptions options); ///< 使用指定配置发送表单 POST。
    /// @param url 请求地址。
    HttpResponse UploadMultipart(_In_ std::string_view url,
                                 _In_ std::vector<MultipartPart> parts); ///< 上传 Multipart。
    /// @param url 请求地址。
    /// @param options 本次操作配置。
    HttpResponse UploadMultipart(_In_ std::string_view url, _In_ std::vector<MultipartPart> parts,
                                 _In_ RequestOptions options); ///< 使用指定配置上传 Multipart。
    /// @param url 请求地址。
    /// @param filePath 文件路径。
    /// @param method HTTP 方法或操作名称。
    HttpResponse UploadFile(_In_ std::string_view url, _In_ const std::string& filePath,
                            _In_ std::string_view method = "PUT"); ///< 使用默认配置流式上传文件。
    /// @param url 请求地址。
    /// @param filePath 文件路径。
    /// @param options 本次操作配置。
    /// @param method HTTP 方法或操作名称。
    HttpResponse UploadFile(_In_ std::string_view url, _In_ const std::string& filePath, _In_ RequestOptions options,
                            _In_ std::string_view method = "PUT"); ///< 使用指定配置流式上传文件。
    /// @param url 请求地址。
    /// @param filePath 文件路径。
    HttpResponse Download(_In_ std::string_view url, _In_ const std::string& filePath); ///< 下载文件。
    /// @param url 请求地址。
    /// @param filePath 文件路径。
    /// @param options 本次操作配置。
    HttpResponse Download(_In_ std::string_view url, _In_ const std::string& filePath,
                          _In_ RequestOptions options); ///< 使用指定配置下载文件。

    /// 获取 CurlTransport Cookie 列表。
    std::vector<std::string> GetCookieList() const;
    /// 清空 CurlTransport 全部 Cookie。
    bool ClearCookies();
    /// 清空 CurlTransport Session Cookie。
    bool ClearSessionCookies();
    /// 刷新 CurlTransport Cookie Jar。
    bool FlushCookies();
    /// 获取当前 Transport。
    std::shared_ptr<IHttpTransport> GetTransport() const;

  private:
    struct Impl;                 ///< 客户端实现细节。
    std::unique_ptr<Impl> impl_; ///< 客户端实现。
};

// ============================================================
// curl_multi 异步客户端
// ============================================================

using AsyncCompletionCallback = std::function<void(_In_ HttpResponse)>; ///< 异步完成回调。

/// 基于 curl_multi 的异步客户端。
class AsyncHttpClient {
  public:
    /// 使用默认配置创建异步客户端。
    AsyncHttpClient();
    /// 使用默认请求配置创建异步客户端。@param defaultOptions 默认请求配置。
    explicit AsyncHttpClient(_In_ RequestOptions defaultOptions);
    /// 使用完整配置创建异步客户端。@param defaultOptions 默认请求配置。@param clientOptions 客户端长期配置。
    AsyncHttpClient(_In_ RequestOptions defaultOptions, _In_ HttpClientOptions clientOptions);
    ~AsyncHttpClient();                                          ///< 停止事件循环并释放资源。
    AsyncHttpClient(const AsyncHttpClient&) = delete;            ///< 禁止复制。
    AsyncHttpClient& operator=(const AsyncHttpClient&) = delete; ///< 禁止复制赋值。

    /// 异步发送完整请求。@param request 请求模型。@param options 请求配置。
    std::future<HttpResponse> SendAsync(_In_ HttpRequestData request, _In_ RequestOptions options);
    /// 使用默认配置异步发送完整请求。@param request 请求模型。
    std::future<HttpResponse> SendAsync(_In_ HttpRequestData request);
    /// 以回调方式异步发送。@param request 请求模型。@param options 请求配置。@param callback 完成回调。
    void SendWithCallback(_In_ HttpRequestData request, _In_ RequestOptions options,
                          _In_ AsyncCompletionCallback callback);

    /// @param url 请求地址。
    /// @param options 本次操作配置。
    std::future<HttpResponse> GetAsync(_In_ std::string_view url, _In_ RequestOptions options); ///< 异步 GET。
    /// @param url 请求地址。
    std::future<HttpResponse> GetAsync(_In_ std::string_view url); ///< 使用默认配置异步 GET。
    /// @param url 请求地址。
    /// @param options 本次操作配置。
    std::future<HttpResponse> HeadAsync(_In_ std::string_view url, _In_ RequestOptions options); ///< 异步 HEAD。
    /// @param url 请求地址。
    std::future<HttpResponse> HeadAsync(_In_ std::string_view url); ///< 使用默认配置异步 HEAD。
    /// @param url 请求地址。
    /// @param body 请求正文。
    /// @param options 本次操作配置。
    std::future<HttpResponse> PostAsync(_In_ std::string_view url, _In_ std::string_view body,
                                        _In_ RequestOptions options); ///< 异步 POST。
    /// @param url 请求地址。
    /// @param body 请求正文。
    std::future<HttpResponse> PostAsync(_In_ std::string_view url,
                                        _In_ std::string_view body = {}); ///< 使用默认配置异步 POST。
    /// @param url 请求地址。
    /// @param body 请求正文。
    /// @param options 本次操作配置。
    std::future<HttpResponse> PutAsync(_In_ std::string_view url, _In_ std::string_view body,
                                       _In_ RequestOptions options); ///< 异步 PUT。
    /// @param url 请求地址。
    /// @param body 请求正文。
    std::future<HttpResponse> PutAsync(_In_ std::string_view url,
                                       _In_ std::string_view body = {}); ///< 使用默认配置异步 PUT。
    /// @param url 请求地址。
    /// @param body 请求正文。
    /// @param options 本次操作配置。
    std::future<HttpResponse> DeleteAsync(_In_ std::string_view url, _In_ std::string_view body,
                                          _In_ RequestOptions options); ///< 异步 DELETE。
    /// @param url 请求地址。
    /// @param body 请求正文。
    std::future<HttpResponse> DeleteAsync(_In_ std::string_view url,
                                          _In_ std::string_view body = {}); ///< 使用默认配置异步 DELETE。
    /// @param url 请求地址。
    /// @param body 请求正文。
    /// @param options 本次操作配置。
    std::future<HttpResponse> PatchAsync(_In_ std::string_view url, _In_ std::string_view body,
                                         _In_ RequestOptions options); ///< 异步 PATCH。
    /// @param url 请求地址。
    /// @param body 请求正文。
    std::future<HttpResponse> PatchAsync(_In_ std::string_view url,
                                         _In_ std::string_view body = {}); ///< 使用默认配置异步 PATCH。
    /// @param url 请求地址。
    /// @param options 本次操作配置。
    std::future<HttpResponse> OptionsAsync(_In_ std::string_view url, _In_ RequestOptions options); ///< 异步 OPTIONS。
    /// @param url 请求地址。
    std::future<HttpResponse> OptionsAsync(_In_ std::string_view url); ///< 使用默认配置异步 OPTIONS。

    /// @param url 请求地址。
    /// @param options 本次操作配置。
    std::future<HttpResponse> PostJsonAsync(_In_ std::string_view url, _In_ std::string_view json,
                                            _In_ RequestOptions options); ///< 异步 JSON POST。
    /// @param url 请求地址。
    std::future<HttpResponse> PostJsonAsync(_In_ std::string_view url,
                                            _In_ std::string_view json); ///< 使用默认配置异步 JSON POST。
    /// @param url 请求地址。
    /// @param options 本次操作配置。
    std::future<HttpResponse> PutJsonAsync(_In_ std::string_view url, _In_ std::string_view json,
                                           _In_ RequestOptions options); ///< 异步 JSON PUT。
    /// @param url 请求地址。
    std::future<HttpResponse> PutJsonAsync(_In_ std::string_view url,
                                           _In_ std::string_view json); ///< 使用默认配置异步 JSON PUT。
    /// @param url 请求地址。
    /// @param options 本次操作配置。
    std::future<HttpResponse> PatchJsonAsync(_In_ std::string_view url, _In_ std::string_view json,
                                             _In_ RequestOptions options); ///< 异步 JSON PATCH。
    /// @param url 请求地址。
    std::future<HttpResponse> PatchJsonAsync(_In_ std::string_view url,
                                             _In_ std::string_view json); ///< 使用默认配置异步 JSON PATCH。

    /// 异步发送任意 Method。@param method Method。@param url 请求地址。@param body Body。@param options 请求配置。
    std::future<HttpResponse> RequestAsync(_In_ std::string_view method, _In_ std::string_view url,
                                           _In_ std::string_view body, _In_ RequestOptions options);
    /// 使用默认配置异步发送任意 Method。@param method Method。@param url 请求地址。@param body Body。
    std::future<HttpResponse> RequestAsync(_In_ std::string_view method, _In_ std::string_view url,
                                           _In_ std::string_view body = {});

    /// 获取当前等待和执行中的请求数量。
    std::size_t GetPendingCount()
        const; ///< 近实时观测值；在 future 置为 ready 或 callback 开始前先移出计数，不应作为完成同步原语。

  private:
    struct Impl;                 ///< curl_multi 实现细节。
    std::unique_ptr<Impl> impl_; ///< 异步实现。
};

#if CURL_EX_HAS_COROUTINES
/// C++20 协程等待对象。
class HttpAwaitable {
  public:
    ~HttpAwaitable(); ///< 注销仍处于挂起状态的恢复句柄；外部并发销毁 coroutine frame 仍需调用方自行同步。
    HttpAwaitable(const HttpAwaitable&) = delete;            ///< 禁止复制，避免多个 Awaitable 竞争同一恢复句柄。
    HttpAwaitable& operator=(const HttpAwaitable&) = delete; ///< 禁止复制赋值。
    HttpAwaitable(_Inout_ HttpAwaitable&& other) noexcept; ///< 支持移动构造。
    HttpAwaitable& operator=(_Inout_ HttpAwaitable&& other) noexcept; ///< 支持移动赋值。
    bool await_ready() const;                                         ///< 判断结果是否已完成。
    /// @param handle 系统或库资源句柄。
    bool await_suspend(_In_ std::coroutine_handle<> handle); ///< 注册协程恢复句柄。
    HttpResponse await_resume();                             ///< 获取请求结果。
  private:
    struct State; ///< 协程共享状态。
    explicit HttpAwaitable(_In_ std::shared_ptr<State> state); ///< 使用内部状态创建 Awaitable。
    std::shared_ptr<State> state_;                             ///< 协程共享状态。
    /// @brief 调用 GetAwaitable 完成对应操作。
    friend HttpAwaitable GetAwaitable(AsyncHttpClient&, _In_ std::string_view, _In_ RequestOptions);
    /// @brief 调用 PostAwaitable 完成对应操作。
    friend HttpAwaitable PostAwaitable(AsyncHttpClient&, _In_ std::string_view, _In_ std::string_view,
                                       _In_ RequestOptions);
};

/// 创建 GET 协程等待对象。@param client 异步客户端。@param url 请求地址。@param options 请求配置。
HttpAwaitable GetAwaitable(_Inout_ AsyncHttpClient& client, _In_ std::string_view url,
                           _In_ RequestOptions options = {});

/// 创建 POST 协程等待对象。@param client 异步客户端。@param url 请求地址。@param body Body。@param options 请求配置。
HttpAwaitable PostAwaitable(_Inout_ AsyncHttpClient& client, _In_ std::string_view url, _In_ std::string_view body = {},
                            _In_ RequestOptions options = {});
#endif

} // namespace ytpp::curl_ex

// 恢复调用方在包含本头文件之前已有的 Windows min/max 宏。
#ifdef CURL_EX_RESTORE_MAX_MACRO
#pragma pop_macro("max")
#undef CURL_EX_RESTORE_MAX_MACRO
#endif

#ifdef CURL_EX_RESTORE_MIN_MACRO
#pragma pop_macro("min")
#undef CURL_EX_RESTORE_MIN_MACRO
#endif

#ifdef CURL_EX_CPLUSPLUS
#undef CURL_EX_CPLUSPLUS
#endif
