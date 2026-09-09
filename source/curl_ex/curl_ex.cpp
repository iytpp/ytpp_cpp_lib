#include "curl_ex/curl_ex.h"

// 当前源码版本：4.2.7。

// 实现文件不依赖 Windows 的 min/max 宏，主动清理以避免后续标准库和实现代码受污染。
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#include <algorithm>
#include <array>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <limits>
#include <random>
#include <set>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <thread>
#include <iomanip>
#include <locale>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace ytpp::curl_ex {
namespace {

// ------------------------------------------------------------
// libcurl 全局生命周期管理
// ------------------------------------------------------------

class CurlGlobalRuntime {
public:
    CurlGlobalRuntime() : code_(curl_global_init(CURL_GLOBAL_ALL)) {}
    ~CurlGlobalRuntime() {
        if (code_ != CURLE_OK) {
            return;
        }

        // libcurl 7.84.0 之前的全局 cleanup 不保证可在任意多线程时刻调用。
        // 对旧版 libcurl 宁可让操作系统在进程退出时回收全局资源，也不在静态析构阶段冒险与其他线程竞态。
#if LIBCURL_VERSION_NUM >= 0x075400 && defined(CURL_VERSION_THREADSAFE)
        const curl_version_info_data* info = curl_version_info(CURLVERSION_NOW);
        if (info != nullptr &&
            info->version_num >= 0x075400 &&
            (info->features & CURL_VERSION_THREADSAFE) != 0) {
            curl_global_cleanup();
        }
#endif
    }

    CURLcode Code() const noexcept { return code_; }

private:
    CURLcode code_;
};

CurlGlobalRuntime& GlobalRuntime() {
    static CurlGlobalRuntime runtime;
    return runtime;
}

bool EnsureGlobal(HttpResponse* response = nullptr) {
    const CURLcode code = GlobalRuntime().Code();
    if (code == CURLE_OK) {
        return true;
    }
    if (response) {
        response->success = false;
        response->curl_code = code;
        response->error = std::string("curl_global_init failed: ") + curl_easy_strerror(code);
    }
    return false;
}

std::string TrimCopy(std::string_view value) {
    std::size_t first = 0;
    while (first < value.size() &&
           std::isspace(static_cast<unsigned char>(value[first])) != 0) {
        ++first;
    }

    std::size_t last = value.size();
    while (last > first &&
           std::isspace(static_cast<unsigned char>(value[last - 1])) != 0) {
        --last;
    }

    return std::string(value.substr(first, last - first));
}

std::string StripLineEnding(std::string_view line) {
    while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) {
        line.remove_suffix(1);
    }
    return std::string(line);
}

std::optional<std::chrono::system_clock::time_point> ParseHttpDate(const std::string& text) {
	std::tm tm{};

	std::istringstream ss(text);
	ss.imbue(std::locale::classic());

	ss >> std::get_time(&tm, "%a, %d %b %Y %H:%M:%S GMT");

	if (ss.fail()) {
		return std::nullopt;
	}

	const __time64_t timestamp = _mkgmtime64(&tm);

	if (timestamp == -1) {
		return std::nullopt;
	}

	return std::chrono::system_clock::from_time_t(timestamp);
}

std::string ToLowerCopy(std::string_view value) {
    std::string out(value);
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

std::string ToUpperCopy(std::string_view value) {
    std::string out(value);
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
    return out;
}

std::string ActiveTlsBackendNameFromVersionString(std::string_view sslVersion) {
    // MultiSSL 的 ssl_version 会把未激活 Backend 放在括号内，例如：
    //   OpenSSL/3.6.1 (Schannel)   -> 当前 Backend 是 OpenSSL
    //   (OpenSSL/3.6.1) Schannel   -> 当前 Backend 是 Schannel
    // 只保留括号外文本，避免把“可用但未激活”的 Backend 误判成当前 Backend。
    std::string active;
    int parenthesisDepth = 0;
    for (const char ch : sslVersion) {
        if (ch == '(') {
            ++parenthesisDepth;
            continue;
        }
        if (ch == ')') {
            if (parenthesisDepth > 0) {
                --parenthesisDepth;
            }
            continue;
        }
        if (parenthesisDepth == 0) {
            active.push_back(ch);
        }
    }
    return TrimCopy(active);
}

std::string ActiveTlsBackendName() {
    const curl_version_info_data* info = curl_version_info(CURLVERSION_NOW);
    if (info == nullptr || info->ssl_version == nullptr) {
        return {};
    }
    return ActiveTlsBackendNameFromVersionString(info->ssl_version);
}

bool IsSchannelBackend() {
    const std::string backend = ToLowerCopy(ActiveTlsBackendName());
    return backend == "schannel" || backend.rfind("schannel/", 0) == 0;
}

#ifdef _WIN32
bool IsOpenSslBackend() {
    const std::string backend = ToLowerCopy(ActiveTlsBackendName());
    return backend == "openssl" || backend.rfind("openssl/", 0) == 0;
}

bool NativeCaOptionSupportedAtRuntime() {
#if LIBCURL_VERSION_NUM >= 0x074700
    const curl_version_info_data* info = curl_version_info(CURLVERSION_NOW);
    // 本次修复只改变 Windows + OpenSSL 的默认 Trust Store；Schannel 本身已经使用
    // Windows Certificate Store，其他 Backend 继续保持 V4.2.6 的原有行为。
    return info != nullptr && info->version_num >= 0x074700 && IsOpenSslBackend();
#else
    return false;
#endif
}
#endif

bool ShouldUseNativeCa(const TlsOptions& tls) {
#ifdef _WIN32
    // 显式 caFile/caPath 代表调用方希望收窄或替换默认信任来源。
    // CURLSSLOPT_NATIVE_CA 与自定义 CA 是“追加”关系，因此这里主动互斥，避免默认 Native CA
    // 无意扩大显式自定义 CA 的信任集合。
    return tls.verifyPeer &&
           tls.useNativeCa &&
           tls.caFile.empty() &&
           tls.caPath.empty() &&
           NativeCaOptionSupportedAtRuntime();
#else
    (void)tls;
    return false;
#endif
}

void EmitDebug(const DebugOptions& debug, const std::string& text) noexcept;

void LogTlsConfigurationDebug(CURL* curl, const RequestOptions& options) noexcept {
    if (!options.debug.enabled || curl == nullptr) {
        return;
    }

    try {
        const curl_version_info_data* info = curl_version_info(CURLVERSION_NOW);
        const bool nativeCaApplied = ShouldUseNativeCa(options.tls);
        std::ostringstream out;
        out << "curl_ex TLS configuration:\n";
        out << "  curl: " << ((info && info->version) ? info->version : "<unknown>") << '\n';
        out << "  SSL: " << ((info && info->ssl_version) ? info->ssl_version : "<none>") << '\n';
        const std::string activeBackend = ActiveTlsBackendName();
        out << "  active TLS backend: " << (activeBackend.empty() ? "<unknown>" : activeBackend) << '\n';
        out << "  verify peer: " << (options.tls.verifyPeer ? "true" : "false") << '\n';
        out << "  verify host: " << (options.tls.verifyHost ? "true" : "false") << '\n';
        out << "  native CA requested: " << (options.tls.useNativeCa ? "true" : "false") << '\n';
        out << "  native CA option applied: " << (nativeCaApplied ? "true" : "false") << '\n';
        out << "  custom CA file: " << (options.tls.caFile.empty() ? "<none>" : options.tls.caFile) << '\n';
        out << "  custom CA path: " << (options.tls.caPath.empty() ? "<none>" : options.tls.caPath) << '\n';
        out << "  pinning: " << (options.tls.pinnedPublicKey.empty() ? "disabled" : "enabled") << '\n';

#if LIBCURL_VERSION_NUM >= 0x075400
        if (info != nullptr && info->version_num >= 0x075400) {
            char* defaultCaInfo = nullptr;
            char* defaultCaPath = nullptr;
            if (curl_easy_getinfo(curl, CURLINFO_CAINFO, &defaultCaInfo) == CURLE_OK) {
                out << "  libcurl default CAINFO: " << (defaultCaInfo ? defaultCaInfo : "<null>") << '\n';
            }
            if (curl_easy_getinfo(curl, CURLINFO_CAPATH, &defaultCaPath) == CURLE_OK) {
                out << "  libcurl default CAPATH: " << (defaultCaPath ? defaultCaPath : "<null>") << '\n';
            }
        }
#endif
        EmitDebug(options.debug, out.str());
    } catch (...) {
        // TLS 诊断属于 Debug 旁路能力，任何格式化/查询失败都不能影响请求结果。
    }
}

long SchannelRevocationOptions(CertificateRevocationPolicy policy) {
    switch (policy) {
        case CertificateRevocationPolicy::Strict:
            return 0L;
        case CertificateRevocationPolicy::BestEffort:
#if LIBCURL_VERSION_NUM >= 0x074600
            return static_cast<long>(CURLSSLOPT_REVOKE_BEST_EFFORT);
#else
            // libcurl 7.70.0 之前没有 BestEffort。安全语义必须 fail-closed：
            // 保持 Schannel 默认严格吊销检查，而不是静默退化为 NO_REVOKE。
            return 0L;
#endif
        case CertificateRevocationPolicy::Disabled:
            return static_cast<long>(CURLSSLOPT_NO_REVOKE);
    }
    return 0L;
}

bool IsHttpsProxy(const ProxyOptions& proxy) {
    if (proxy.type) {
        return *proxy.type == ProxyType::Https;
    }
    const std::string lower = ToLowerCopy(proxy.url);
    return lower.rfind("https://", 0) == 0;
}

bool ContainsCrLf(std::string_view value) noexcept {
    return value.find('\r') != std::string_view::npos ||
           value.find('\n') != std::string_view::npos;
}

bool ContainsNul(std::string_view value) noexcept {
    return value.find('\0') != std::string_view::npos;
}

bool IsValidHttpVersion(HttpVersion version) noexcept {
    switch (version) {
        case HttpVersion::Auto:
        case HttpVersion::Http1_0:
        case HttpVersion::Http1_1:
        case HttpVersion::Http2:
        case HttpVersion::Http2Tls:
        case HttpVersion::Http3:
            return true;
    }
    return false;
}

bool IsValidProxyType(ProxyType type) noexcept {
    switch (type) {
        case ProxyType::Http:
        case ProxyType::Https:
        case ProxyType::Socks4:
        case ProxyType::Socks4A:
        case ProxyType::Socks5:
        case ProxyType::Socks5Hostname:
            return true;
    }
    return false;
}

bool IsValidTlsVersion(TlsVersion version) noexcept {
    switch (version) {
        case TlsVersion::Default:
        case TlsVersion::Tls1_0:
        case TlsVersion::Tls1_1:
        case TlsVersion::Tls1_2:
        case TlsVersion::Tls1_3:
            return true;
    }
    return false;
}

bool IsValidRevocationPolicy(CertificateRevocationPolicy policy) noexcept {
    switch (policy) {
        case CertificateRevocationPolicy::Strict:
        case CertificateRevocationPolicy::BestEffort:
        case CertificateRevocationPolicy::Disabled:
            return true;
    }
    return false;
}

int TlsVersionRank(TlsVersion version) noexcept {
    switch (version) {
        case TlsVersion::Default: return 0;
        case TlsVersion::Tls1_0: return 10;
        case TlsVersion::Tls1_1: return 11;
        case TlsVersion::Tls1_2: return 12;
        case TlsVersion::Tls1_3: return 13;
    }
    return 0;
}

// 判断字符串是否符合 HTTP token 语法，可用于 Method 和 Header 名称等不允许空格的字段。
bool IsHttpToken(std::string_view value) noexcept {
    if (value.empty()) return false;
    for (const char ch : value) {
        const unsigned char c = static_cast<unsigned char>(ch);
        const bool alphaNum = (c >= '0' && c <= '9') ||
                              (c >= 'A' && c <= 'Z') ||
                              (c >= 'a' && c <= 'z');
        const bool tchar = alphaNum || ch == '!' || ch == '#' || ch == '$' ||
                           ch == '%' || ch == '&' || ch == '\'' || ch == '*' ||
                           ch == '+' || ch == '-' || ch == '.' || ch == '^' ||
                           ch == '_' || ch == '`' || ch == '|' || ch == '~';
        if (!tchar) return false;
    }
    return true;
}

// 判断 HTTP 字段值是否包含不允许的控制字符；水平制表符按 HTTP 语法允许。
bool IsSafeHttpFieldValue(std::string_view value) noexcept {
    for (const char ch : value) {
        const unsigned char c = static_cast<unsigned char>(ch);
        if ((c < 32U && c != 9U) || c == 127U) return false;
    }
    return true;
}

// 验证 rawHeaderLines 的单行语法。支持常规 "Name: value"、抑制内部 Header 的 "Name:"，
// 以及 libcurl 用于强制发送空值 Header 的 "Name;"；其他无字段名或无分隔符文本一律拒绝。
bool IsValidRawHeaderLine(std::string_view line) noexcept {
    if (line.empty() || ContainsCrLf(line) || !IsSafeHttpFieldValue(line)) return false;
    const std::size_t colon = line.find(':');
    if (colon != std::string_view::npos) return IsHttpToken(line.substr(0, colon));
    return line.back() == ';' && line.find(';') == line.size() - 1U &&
           IsHttpToken(line.substr(0, line.size() - 1U));
}

bool ContainsCode(const std::vector<CURLcode>& values, CURLcode value) {
    return std::find(values.begin(), values.end(), value) != values.end();
}

bool ContainsStatus(const std::vector<long>& values, long value) {
    return std::find(values.begin(), values.end(), value) != values.end();
}

bool IsIdempotentMethod(std::string_view method) {
    const std::string upper = ToUpperCopy(method);
    return upper == "GET" || upper == "HEAD" || upper == "PUT" ||
           upper == "DELETE" || upper == "OPTIONS" || upper == "TRACE";
}

bool IsSafePreTransferRetry(CURLcode code) {
    switch (code) {
        case CURLE_COULDNT_RESOLVE_PROXY:
        case CURLE_COULDNT_RESOLVE_HOST:
        case CURLE_COULDNT_CONNECT:
#if LIBCURL_VERSION_NUM >= 0x074900
        case CURLE_PROXY:
#endif
        case CURLE_SSL_CONNECT_ERROR:
            return true;
        default:
            return false;
    }
}

std::chrono::milliseconds ParseRetryAfter(const HttpResponse& response) {
    const std::string value = TrimCopy(response.headers.GetHeaderValue("Retry-After"));
    if (value.empty()) {
        return std::chrono::milliseconds{0};
    }

    // Retry-After 的秒数格式。
    try {
        std::size_t consumed = 0;
        const long long seconds = std::stoll(value, &consumed, 10);
        if (consumed == value.size() && seconds >= 0) {
            const long long maxSeconds = static_cast<long long>(
                std::chrono::duration_cast<std::chrono::seconds>(
                    (std::chrono::milliseconds::max)())
                    .count());
            const long long clamped = (std::min)(seconds, maxSeconds);
            return std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::seconds(clamped));
        }
    } catch (...) {
    }

    // HTTP 日期格式；curl_getdate 失败时返回 time_t(-1)，不能假设 time_t 一定是有符号类型。
    const std::time_t retryAt = curl_getdate(value.c_str(), nullptr);
    const std::time_t invalidTime = static_cast<std::time_t>(-1);
    if (retryAt != invalidTime) {
        const std::time_t now = std::time(nullptr);
        if (now != invalidTime && retryAt > now) {
            const long double deltaSeconds =
                static_cast<long double>(retryAt) - static_cast<long double>(now);
            const long double maxSeconds = static_cast<long double>(
                std::chrono::duration_cast<std::chrono::seconds>(
                    (std::chrono::milliseconds::max)())
                    .count());
            const long long clampedSeconds = static_cast<long long>(
                (std::min)(deltaSeconds, maxSeconds));
            return std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::seconds(clampedSeconds));
        }
    }

    return std::chrono::milliseconds{0};
}

std::chrono::milliseconds ComputeRetryDelay(const RetryPolicy& policy,
                                             int retryIndex,
                                             const HttpResponse& response) {
    if (policy.respectRetryAfter) {
        const auto retryAfter = ParseRetryAfter(response);
        if (retryAfter.count() > 0) {
            return (std::min)(retryAfter, policy.maxDelay);
        }
    }

    long double multiplier = 1.0L;
    if (policy.exponentialBackoff && retryIndex > 0) {
        const int exponent = (std::min)(retryIndex - 1, 20);
        multiplier = static_cast<long double>(std::uint64_t{1} << exponent);
    } else if (!policy.exponentialBackoff) {
        multiplier = static_cast<long double>((std::max)(retryIndex, 1));
    }

    long double delay = static_cast<long double>(policy.baseDelay.count()) * multiplier;
    delay = (std::min)(delay, static_cast<long double>(policy.maxDelay.count()));

    if (policy.jitter && delay > 0.0L) {
        try {
            thread_local std::mt19937 rng(std::random_device{}());
            std::uniform_real_distribution<long double> dist(0.85L, 1.15L);
            delay *= dist(rng);
            delay = (std::min)(delay, static_cast<long double>(policy.maxDelay.count()));
        } catch (...) {
            // 随机源不可用时退化为无抖动延迟，不能让重试策略本身导致请求异常退出。
        }
    }

    return std::chrono::milliseconds(static_cast<long long>((std::max)(0.0L, delay)));
}

bool SleepCancelable(std::chrono::milliseconds duration, std::atomic_bool* cancelFlag) {
    if (duration.count() <= 0) {
        return cancelFlag == nullptr || !cancelFlag->load(std::memory_order_relaxed);
    }

    constexpr auto kSlice = std::chrono::milliseconds(50);
    auto remaining = duration;
    while (remaining.count() > 0) {
        if (cancelFlag && cancelFlag->load(std::memory_order_relaxed)) {
            return false;
        }
        const auto slice = (std::min)(remaining, kSlice);
        std::this_thread::sleep_for(slice);
        remaining -= slice;
    }
    return cancelFlag == nullptr || !cancelFlag->load(std::memory_order_relaxed);
}

std::string CurlHttpVersionName(long version) {
    switch (version) {
        case CURL_HTTP_VERSION_1_0: return "HTTP/1.0";
        case CURL_HTTP_VERSION_1_1: return "HTTP/1.1";
#if LIBCURL_VERSION_NUM >= 0x072100
        case CURL_HTTP_VERSION_2_0: return "HTTP/2";
#endif
#if LIBCURL_VERSION_NUM >= 0x074200
        case CURL_HTTP_VERSION_3: return "HTTP/3";
#endif
        default: return {};
    }
}

std::optional<long> CurlProxyType(ProxyType type) {
    switch (type) {
        case ProxyType::Http: return static_cast<long>(CURLPROXY_HTTP);
#if LIBCURL_VERSION_NUM >= 0x073400
        case ProxyType::Https: return static_cast<long>(CURLPROXY_HTTPS);
#else
        case ProxyType::Https: return std::nullopt;
#endif
        case ProxyType::Socks4: return static_cast<long>(CURLPROXY_SOCKS4);
        case ProxyType::Socks4A: return static_cast<long>(CURLPROXY_SOCKS4A);
        case ProxyType::Socks5: return static_cast<long>(CURLPROXY_SOCKS5);
        case ProxyType::Socks5Hostname: return static_cast<long>(CURLPROXY_SOCKS5_HOSTNAME);
    }
    return std::nullopt;
}

long CurlHttpVersion(HttpVersion version) {
    switch (version) {
        case HttpVersion::Auto:
            return CURL_HTTP_VERSION_NONE;
        case HttpVersion::Http1_0:
            return CURL_HTTP_VERSION_1_0;
        case HttpVersion::Http1_1:
            return CURL_HTTP_VERSION_1_1;
        case HttpVersion::Http2:
#if LIBCURL_VERSION_NUM >= 0x072100
            return CURL_HTTP_VERSION_2_0;
#else
            return CURL_HTTP_VERSION_NONE;
#endif
        case HttpVersion::Http2Tls:
#if LIBCURL_VERSION_NUM >= 0x072F00
            return CURL_HTTP_VERSION_2TLS;
#elif LIBCURL_VERSION_NUM >= 0x072100
            return CURL_HTTP_VERSION_2_0;
#else
            return CURL_HTTP_VERSION_NONE;
#endif
        case HttpVersion::Http3:
#if LIBCURL_VERSION_NUM >= 0x074200
            return CURL_HTTP_VERSION_3;
#else
            return CURL_HTTP_VERSION_NONE;
#endif
    }
    return CURL_HTTP_VERSION_NONE;
}

long CurlTlsVersion(TlsVersion version) {
    switch (version) {
        case TlsVersion::Default: return CURL_SSLVERSION_DEFAULT;
        case TlsVersion::Tls1_0: return CURL_SSLVERSION_TLSv1_0;
        case TlsVersion::Tls1_1: return CURL_SSLVERSION_TLSv1_1;
        case TlsVersion::Tls1_2: return CURL_SSLVERSION_TLSv1_2;
        case TlsVersion::Tls1_3:
#if LIBCURL_VERSION_NUM >= 0x073400
            return CURL_SSLVERSION_TLSv1_3;
#else
            return CURL_SSLVERSION_DEFAULT;
#endif
    }
    return CURL_SSLVERSION_DEFAULT;
}

long CurlTlsMaxVersion(TlsVersion version) {
    switch (version) {
        case TlsVersion::Default: return CURL_SSLVERSION_MAX_DEFAULT;
        case TlsVersion::Tls1_0: return CURL_SSLVERSION_MAX_TLSv1_0;
        case TlsVersion::Tls1_1: return CURL_SSLVERSION_MAX_TLSv1_1;
        case TlsVersion::Tls1_2: return CURL_SSLVERSION_MAX_TLSv1_2;
        case TlsVersion::Tls1_3:
#if LIBCURL_VERSION_NUM >= 0x073400
            return CURL_SSLVERSION_MAX_TLSv1_3;
#else
            return CURL_SSLVERSION_MAX_DEFAULT;
#endif
    }
    return CURL_SSLVERSION_MAX_DEFAULT;
}

long ComposeTlsVersionOption(TlsVersion minVersion, TlsVersion maxVersion) {
    long value = CurlTlsVersion(minVersion);
    // maxVersion=Default 表示“不额外限制最高版本”。不要显式 OR MAX_DEFAULT：
    // 这在语义上没有必要，而且旧 wolfSSL/libcurl 对 MAX 宏存在历史兼容限制。
    if (maxVersion != TlsVersion::Default) {
        value |= CurlTlsMaxVersion(maxVersion);
    }
    return value;
}



// Debug 脱敏实现位于本文件后部；libcurl verbose 回调需要在请求配置阶段复用同一规则。
std::string RedactHeaderValue(const DebugOptions& debug,
                              std::string_view name,
                              std::string_view value);
std::string RedactUrlForDebug(const DebugOptions& debug, std::string_view input);

bool IsRedirectSensitiveHeaderName(const DebugOptions& debug, std::string_view name) {
    const std::string lower = ToLowerCopy(TrimCopy(name));
    static const std::array<std::string_view, 6> builtins{
        "x-api-key", "api-key", "x-auth-token", "x-access-token", "x-secret", "set-cookie"};
    for (const auto item : builtins) {
        if (lower == item) return true;
    }
    for (const auto& item : debug.sensitiveHeaders) {
        if (lower == ToLowerCopy(TrimCopy(item))) return true;
    }
    return false;
}

struct CurlSlistHolder {
    curl_slist* value = nullptr;
    ~CurlSlistHolder() { curl_slist_free_all(value); }

    bool Append(const std::string& line) {
        curl_slist* next = curl_slist_append(value, line.c_str());
        if (!next) {
            return false;
        }
        value = next;
        return true;
    }
};

struct CurlMimeHolder {
    curl_mime* value = nullptr;
    ~CurlMimeHolder() {
        if (value) {
            curl_mime_free(value);
        }
    }
};

struct TransferContext {
    HttpResponse* response = nullptr;
    const RequestOptions* options = nullptr;
    HttpHeaderBlock currentBlock;
    bool blockOpen = false;
    bool completedStatusBlock = false;
    std::string callbackError;
    HttpErrorCategory callbackErrorCategory = HttpErrorCategory::None;
    std::uint64_t receivedBytes = 0;
    std::uint64_t receivedHeaderBytes = 0;
    bool externalSinkTouched = false;

    void FinalizeCurrentBlock() {
        if (!blockOpen) {
            return;
        }
        if (currentBlock.statusCode != 0) {
            completedStatusBlock = true;
        }
        response->headerHistory.push_back(std::move(currentBlock));
        currentBlock = HttpHeaderBlock{};
        blockOpen = false;
    }

    void ResetForAttempt() {
        response->content.clear();
        response->org_headers.clear();
        response->headers.Clear();
        response->date.reset();
        response->trailers.Clear();
        response->rawTrailers.clear();
        response->cookies.Clear();
        response->final_raw_headers.clear();
        response->headerHistory.clear();
        response->code = 0;
        response->effectiveUrl.clear();
        response->contentType.clear();
        response->primaryIp.clear();
        response->negotiatedHttpVersion.clear();
        response->redirectCount = 0;
        response->proxyConnectCode = 0;
        response->downloadedBytes = 0;
        response->uploadedBytes = 0;
        response->totalTime = std::chrono::microseconds{0};
        response->nameLookupTime = std::chrono::microseconds{0};
        response->connectTime = std::chrono::microseconds{0};
        response->tlsHandshakeTime = std::chrono::microseconds{0};
        response->firstByteTime = std::chrono::microseconds{0};
        currentBlock = HttpHeaderBlock{};
        blockOpen = false;
        completedStatusBlock = false;
        callbackError.clear();
        callbackErrorCategory = HttpErrorCategory::None;
        receivedBytes = 0;
        receivedHeaderBytes = 0;
        externalSinkTouched = false;
    }
};

size_t WriteCallback(char* data, size_t size, size_t nmemb, void* userdata) {
    auto* context = static_cast<TransferContext*>(userdata);
    if (!context || !context->response || !context->options) {
        return 0;
    }

    if (size != 0 && nmemb > (std::numeric_limits<std::size_t>::max)() / size) {
        context->callbackError = "响应数据块大小溢出";
        context->callbackErrorCategory = HttpErrorCategory::Internal;
        return 0;
    }
    const std::size_t bytes = size * nmemb;

    try {
        const std::uint64_t chunkBytes = static_cast<std::uint64_t>(bytes);
        if (context->options->maxResponseSize > 0) {
            const std::uint64_t limit = static_cast<std::uint64_t>(context->options->maxResponseSize);
            if (context->receivedBytes > limit || chunkBytes > limit - context->receivedBytes) {
                context->callbackError = "响应大小超过 maxResponseSize 限制";
                context->callbackErrorCategory = HttpErrorCategory::Download;
                return 0;
            }
        }

        if (context->options->storeResponseBody &&
            context->options->maxInMemoryResponseSize > 0) {
            const std::size_t limit = context->options->maxInMemoryResponseSize;
            const std::size_t current = context->response->content.size();
            if (current > limit || bytes > limit - current) {
                context->callbackError = "响应 Body 超过 maxInMemoryResponseSize 内存缓存限制";
                context->callbackErrorCategory = HttpErrorCategory::Download;
                return 0;
            }
        }

        if (context->options->responseChunkCallback) {
            // 回调一旦被调用就可能已经产生外部副作用，即使随后返回 false 也必须视为 sink 已被触碰。
            context->externalSinkTouched = true;
            if (!context->options->responseChunkCallback(data, bytes)) {
                context->callbackError = "响应数据块回调主动终止传输";
                context->callbackErrorCategory = HttpErrorCategory::Cancelled;
                return 0;
            }
        }

        if (context->options->responseStream) {
            // ostream::write 失败前也可能已经写入部分数据，因此写入动作开始即标记。
            context->externalSinkTouched = true;
            context->options->responseStream->write(data, static_cast<std::streamsize>(bytes));
            if (!*context->options->responseStream) {
                context->callbackError = "响应输出流写入失败";
                context->callbackErrorCategory = HttpErrorCategory::Download;
                return 0;
            }
        }

        if (context->options->storeResponseBody) {
            context->response->content.append(data, bytes);
        }

        context->receivedBytes += chunkBytes;
        return bytes;
    } catch (const std::exception& ex) {
        context->callbackError = std::string("响应处理回调抛出异常: ") + ex.what();
        context->callbackErrorCategory = HttpErrorCategory::Internal;
        return 0;
    } catch (...) {
        context->callbackError = "响应处理回调抛出未知异常";
        context->callbackErrorCategory = HttpErrorCategory::Internal;
        return 0;
    }
}

long ParseStatusCode(std::string_view statusLine) {
    const auto firstSpace = statusLine.find(' ');
    if (firstSpace == std::string_view::npos) {
        return 0;
    }
    const auto rest = statusLine.substr(firstSpace + 1);
    try {
        std::size_t consumed = 0;
        const long code = std::stol(std::string(rest), &consumed, 10);
        return consumed > 0 ? code : 0;
    } catch (...) {
        return 0;
    }
}

size_t HeaderCallback(char* data, size_t size, size_t nmemb, void* userdata) {
    auto* context = static_cast<TransferContext*>(userdata);
    if (!context || !context->response) {
        return 0;
    }
    if (size != 0 && nmemb > (std::numeric_limits<std::size_t>::max)() / size) {
        context->callbackError = "响应 Header 数据块大小溢出";
        context->callbackErrorCategory = HttpErrorCategory::Internal;
        return 0;
    }
    const std::size_t bytes = size * nmemb;

    try {
        const std::uint64_t chunkBytes = static_cast<std::uint64_t>(bytes);
        if (context->options && context->options->maxResponseHeaderSize > 0) {
            const std::uint64_t limit =
                static_cast<std::uint64_t>(context->options->maxResponseHeaderSize);
            if (context->receivedHeaderBytes > limit ||
                chunkBytes > limit - context->receivedHeaderBytes) {
                context->callbackError = "响应 Header/Trailer 超过 maxResponseHeaderSize 限制";
                context->callbackErrorCategory = HttpErrorCategory::Download;
                return 0;
            }
        }
        context->receivedHeaderBytes += chunkBytes;

        const std::string_view rawLine(data, bytes);
        context->response->org_headers.append(data, bytes);
        const std::string line = StripLineEnding(rawLine);

        if (line.rfind("HTTP/", 0) == 0) {
            context->FinalizeCurrentBlock();
            context->blockOpen = true;
            context->currentBlock.statusLine = line;
            context->currentBlock.statusCode = ParseStatusCode(line);
            context->currentBlock.rawHeaders.append(data, bytes);
            return bytes;
        }

        // 已完成至少一个带状态行的响应块后，如果又收到没有新状态行的 Header，
        // 按 HTTP Trailer 处理，避免把 Trailer 误认为新的最终响应 Header 块。
        if (!context->blockOpen && context->completedStatusBlock) {
            if (line.empty()) {
                // Trailer 的结束空行只表示 Trailer 结束，不应生成一个新的空响应块。
                return bytes;
            }
            context->response->rawTrailers.append(data, bytes);
            const auto colon = line.find(':');
            if (colon != std::string::npos) {
                const std::string name = TrimCopy(std::string_view(line).substr(0, colon));
                const std::string value = TrimCopy(std::string_view(line).substr(colon + 1));
                if (!name.empty()) {
                    context->response->trailers.AddHeader(name, value);
                }
            }
            return bytes;
        }

        if (!context->blockOpen) {
            context->blockOpen = true;
        }
        context->currentBlock.rawHeaders.append(data, bytes);

        if (line.empty()) {
            context->FinalizeCurrentBlock();
            return bytes;
        }

        const auto colon = line.find(':');
        if (colon != std::string::npos) {
            const std::string name = TrimCopy(std::string_view(line).substr(0, colon));
            const std::string value = TrimCopy(std::string_view(line).substr(colon + 1));
            if (!name.empty()) {
                context->currentBlock.headers.AddHeader(name, value);
            }
        }

        return bytes;
    } catch (const std::exception& ex) {
        context->callbackError = std::string("响应 Header 回调抛出异常: ") + ex.what();
        context->callbackErrorCategory = HttpErrorCategory::Internal;
        return 0;
    } catch (...) {
        context->callbackError = "响应 Header 回调抛出未知异常";
        context->callbackErrorCategory = HttpErrorCategory::Internal;
        return 0;
    }
}

size_t UploadReadCallback(char* buffer, size_t size, size_t nmemb, void* userdata) {
    auto* context = static_cast<TransferContext*>(userdata);
    if (!context || !context->options || !context->options->uploadSource) {
        return 0;
    }
    if (size != 0 && nmemb > (std::numeric_limits<std::size_t>::max)() / size) {
        context->callbackError = "上传读取缓冲区大小溢出";
        context->callbackErrorCategory = HttpErrorCategory::Upload;
        return CURL_READFUNC_ABORT;
    }
    const std::size_t capacity = size * nmemb;
    try {
        const std::size_t bytes = context->options->uploadSource->Read(buffer, capacity);
        if (bytes > capacity) {
            context->callbackError = "流式上传源返回的字节数超过 libcurl 提供的缓冲区容量";
            context->callbackErrorCategory = HttpErrorCategory::Upload;
            return CURL_READFUNC_ABORT;
        }
        return bytes;
    } catch (const std::exception& ex) {
        context->callbackError = std::string("流式上传源读取数据时抛出异常: ") + ex.what();
        context->callbackErrorCategory = HttpErrorCategory::Upload;
        return CURL_READFUNC_ABORT;
    } catch (...) {
        context->callbackError = "流式上传源读取数据时抛出未知异常";
        context->callbackErrorCategory = HttpErrorCategory::Upload;
        return CURL_READFUNC_ABORT;
    }
}

int UploadSeekCallback(void* userdata, curl_off_t offset, int origin) {
    auto* context = static_cast<TransferContext*>(userdata);
    if (!context || !context->options || !context->options->uploadSource) {
        return CURL_SEEKFUNC_CANTSEEK;
    }

    // IUploadSource 只承诺 Rewind，因此只支持回到流开头；其他随机 seek 明确交回 libcurl 处理。
    if (origin != SEEK_SET || offset != 0) {
        return CURL_SEEKFUNC_CANTSEEK;
    }

    try {
        if (context->options->uploadSource->Rewind()) {
            return CURL_SEEKFUNC_OK;
        }
        return CURL_SEEKFUNC_CANTSEEK;
    } catch (const std::exception& ex) {
        context->callbackError = std::string("流式上传源回退时抛出异常: ") + ex.what();
        return CURL_SEEKFUNC_FAIL;
    } catch (...) {
        context->callbackError = "流式上传源回退时抛出未知异常";
        return CURL_SEEKFUNC_FAIL;
    }
}

int ProgressCallbackBridge(void* userdata,
                           curl_off_t dltotal,
                           curl_off_t dlnow,
                           curl_off_t ultotal,
                           curl_off_t ulnow) {
    auto* context = static_cast<TransferContext*>(userdata);
    if (!context || !context->options) {
        return 0;
    }

    try {
        if (context->options->cancelFlag &&
            context->options->cancelFlag->load(std::memory_order_relaxed)) {
            context->callbackError = "请求已取消";
            context->callbackErrorCategory = HttpErrorCategory::Cancelled;
            return 1;
        }

        if (context->options->progressCallback &&
            !context->options->progressCallback(dltotal, dlnow, ultotal, ulnow)) {
            context->callbackError = "进度回调主动终止传输";
            context->callbackErrorCategory = HttpErrorCategory::Cancelled;
            return 1;
        }
    } catch (const std::exception& ex) {
        context->callbackError = std::string("进度回调抛出异常: ") + ex.what();
        context->callbackErrorCategory = HttpErrorCategory::Internal;
        return 1;
    } catch (...) {
        context->callbackError = "进度回调抛出未知异常";
        context->callbackErrorCategory = HttpErrorCategory::Internal;
        return 1;
    }

    return 0;
}


void EmitCurlVerbose(const DebugOptions& debug, std::string text) noexcept {
    try {
        if (debug.logger) {
            debug.logger(text);
        } else {
            std::clog << text;
            if (text.empty() || text.back() != '\n') std::clog << '\n';
            std::clog.flush();
        }
    } catch (...) {
        // verbose 仅用于诊断，日志输出异常不能反向破坏网络传输。
    }
}

std::string RedactVerboseRequestLine(const DebugOptions& debug, std::string_view line) {
    if (!debug.hideSensitiveUrlData) return std::string(line);

    const std::size_t firstSpace = line.find(' ');
    if (firstSpace == std::string_view::npos) return std::string(line);
    const std::size_t secondSpace = line.find(' ', firstSpace + 1);
    if (secondSpace == std::string_view::npos) return std::string(line);

    std::string out;
    out.reserve(line.size());
    out.append(line.data(), firstSpace + 1);
    out += RedactUrlForDebug(debug, line.substr(firstSpace + 1, secondSpace - firstSpace - 1));
    out.append(line.data() + secondSpace, line.size() - secondSpace);
    return out;
}

std::string RedactVerboseHeaderBlock(const DebugOptions& debug,
                                     const char* data,
                                     std::size_t size,
                                     bool outgoing) {
    const std::string_view block(data, size);
    std::ostringstream out;
    std::size_t start = 0;
    bool firstLine = true;
    while (start < block.size()) {
        std::size_t end = block.find('\n', start);
        if (end == std::string_view::npos) end = block.size();
        std::string_view rawLine = block.substr(start, end - start);
        if (!rawLine.empty() && rawLine.back() == '\r') rawLine.remove_suffix(1);

        out << (outgoing ? "> " : "< ");
        const std::size_t colon = rawLine.find(':');
        if (firstLine && outgoing && colon == std::string_view::npos) {
            out << RedactVerboseRequestLine(debug, rawLine);
        } else if (colon != std::string_view::npos) {
            const std::string name = TrimCopy(rawLine.substr(0, colon));
            const std::string value = TrimCopy(rawLine.substr(colon + 1));
            out << name << ": " << RedactHeaderValue(debug, name, value);
        } else {
            out << rawLine;
        }
        out << '\n';
        firstLine = false;
        if (end == block.size()) break;
        start = end + 1;
    }
    return out.str();
}

int CurlVerboseCallback(CURL*, curl_infotype type, char* data, size_t size, void* userdata) {
    auto* context = static_cast<TransferContext*>(userdata);
    if (!context || !context->options || !data || size == 0) return 0;
    const DebugOptions& debug = context->options->debug;

    try {
        switch (type) {
            case CURLINFO_HEADER_OUT:
                EmitCurlVerbose(debug, RedactVerboseHeaderBlock(debug, data, size, true));
                break;
            case CURLINFO_HEADER_IN:
                EmitCurlVerbose(debug, RedactVerboseHeaderBlock(debug, data, size, false));
                break;
            case CURLINFO_TEXT:
                // CURLINFO_TEXT 是自由格式诊断文本，无法可靠识别其中的所有凭据/URL。
                // 只在调用方显式关闭两类脱敏时按原文输出；默认安全配置下宁可少记录，也不泄露秘密。
                if (!debug.hideSensitiveHeaders && !debug.hideSensitiveUrlData) {
                    EmitCurlVerbose(debug, std::string("* ") + std::string(data, size));
                }
                break;
            case CURLINFO_DATA_IN:
            case CURLINFO_DATA_OUT:
#ifdef CURLINFO_SSL_DATA_IN
            case CURLINFO_SSL_DATA_IN:
            case CURLINFO_SSL_DATA_OUT:
#endif
            default:
                // raw body / TLS record 不属于安全 verbose 输出，避免把请求 Body、响应 Body 或密钥材料写入日志。
                break;
        }
    } catch (...) {
        // libcurl debug callback 必须稳定返回 0；任何格式化/分配错误都降级为静默。
    }
    return 0;
}

template <typename T>
bool SetOpt(CURL* curl,
            CURLoption option,
            const char* optionName,
            T value,
            HttpResponse& response) {
    const CURLcode code = curl_easy_setopt(curl, option, value);
    if (code == CURLE_OK) {
        return true;
    }

    response.success = false;
    response.curl_code = code;
    response.error = std::string("curl_easy_setopt(") + optionName + ") failed: " +
                     curl_easy_strerror(code);
    return false;
}

bool ConfigureHeaders(const RequestOptions& options,
                      CurlSlistHolder& headerList,
                      HttpResponse& response) {
    for (const auto& item : options.headers.Items()) {
        if (!IsHttpToken(item.name) || !IsSafeHttpFieldValue(item.value)) {
            response.success = false;
            response.curl_code = CURLE_BAD_FUNCTION_ARGUMENT;
            response.errorCategory = HttpErrorCategory::InvalidArgument;
            response.error = "结构化 Header 名称或值包含非法字符";
            return false;
        }

        std::string line = item.name;
        if (item.value.empty()) {
            // libcurl 将 "Header:" 解释为抑制该 Header；"Header;" 才表示真正发送空值 Header。
            line += ';';
        } else {
            line += ": ";
            line += item.value;
        }
        if (!headerList.Append(line)) {
            response.success = false;
            response.curl_code = CURLE_OUT_OF_MEMORY;
            response.error = "curl_slist_append failed for request header";
            return false;
        }
    }

    for (const auto& raw : options.rawHeaderLines) {
        if (!IsValidRawHeaderLine(raw)) {
            response.success = false;
            response.curl_code = CURLE_BAD_FUNCTION_ARGUMENT;
            response.errorCategory = HttpErrorCategory::InvalidArgument;
            response.error = "rawHeaderLines 必须是合法的 Header 字段行，且不能包含非法控制字符";
            return false;
        }
        if (!headerList.Append(raw)) {
            response.success = false;
            response.curl_code = CURLE_OUT_OF_MEMORY;
            response.error = "curl_slist_append failed for raw request header";
            return false;
        }
    }

    return true;
}

bool ConfigureMime(CURL* curl,
                   const RequestOptions& options,
                   CurlMimeHolder& mime,
                   HttpResponse& response) {
    if (options.multipart.empty()) {
        return true;
    }

    mime.value = curl_mime_init(curl);
    if (!mime.value) {
        response.success = false;
        response.curl_code = CURLE_OUT_OF_MEMORY;
        response.error = "curl_mime_init failed";
        return false;
    }

    for (const auto& part : options.multipart) {
        if (part.name.empty() || !IsSafeHttpFieldValue(part.name) ||
            !IsSafeHttpFieldValue(part.fileName) || !IsSafeHttpFieldValue(part.contentType) || ContainsNul(part.filePath)) {
            response.success = false;
            response.curl_code = CURLE_BAD_FUNCTION_ARGUMENT;
            response.errorCategory = HttpErrorCategory::InvalidArgument;
            response.error = "Multipart 名称、文件名或 Content-Type 包含非法控制字符";
            return false;
        }

        curl_mimepart* curlPart = curl_mime_addpart(mime.value);
        if (!curlPart) {
            response.success = false;
            response.curl_code = CURLE_OUT_OF_MEMORY;
            response.error = "curl_mime_addpart failed";
            return false;
        }

        CURLcode code = curl_mime_name(curlPart, part.name.c_str());
        if (code != CURLE_OK) {
            response.success = false;
            response.curl_code = code;
            response.error = std::string("curl_mime_name failed: ") + curl_easy_strerror(code);
            return false;
        }

        if (part.IsFile()) {
            code = curl_mime_filedata(curlPart, part.filePath.c_str());
        } else {
            code = curl_mime_data(curlPart, part.data.data(), part.data.size());
        }
        if (code != CURLE_OK) {
            response.success = false;
            response.curl_code = code;
            response.error = std::string("curl_mime data setup failed: ") + curl_easy_strerror(code);
            return false;
        }

        if (!part.fileName.empty()) {
            code = curl_mime_filename(curlPart, part.fileName.c_str());
            if (code != CURLE_OK) {
                response.success = false;
                response.curl_code = code;
                response.error = std::string("curl_mime_filename failed: ") + curl_easy_strerror(code);
                return false;
            }
        }

        if (!part.contentType.empty()) {
            code = curl_mime_type(curlPart, part.contentType.c_str());
            if (code != CURLE_OK) {
                response.success = false;
                response.curl_code = code;
                response.error = std::string("curl_mime_type failed: ") + curl_easy_strerror(code);
                return false;
            }
        }
    }

    return true;
}

bool UsesCustomRequest(std::string_view method,
                       std::string_view body,
                       const RequestOptions& options) {
    const std::string upper = ToUpperCopy(method);
    if (!options.multipart.empty()) {
        return upper != "POST";
    }
    if (options.uploadSource) {
        return upper != "PUT";
    }
    if (upper == "HEAD") {
        return false;
    }
    if (upper == "GET" && body.empty()) {
        return false;
    }
    if (upper == "POST") {
        return false;
    }
    return true;
}

bool SupportsSafeCustomRedirectFollowing() noexcept {
#if LIBCURL_VERSION_NUM >= 0x080D00
    const curl_version_info_data* info = curl_version_info(CURLVERSION_NOW);
    return info != nullptr && info->version_num >= 0x080D00;
#else
    return false;
#endif
}

[[maybe_unused]] bool HasRawHeaderNamed(const RequestOptions& options, std::string_view wantedName) {
    const std::string wanted = ToLowerCopy(wantedName);
    for (const auto& raw : options.rawHeaderLines) {
        const std::size_t separator = raw.find_first_of(":;");
        const std::string name = ToLowerCopy(TrimCopy(std::string_view(raw).substr(0, separator)));
        if (name == wanted) return true;
    }
    return false;
}

bool HasRedirectSensitiveCustomHeader(const RequestOptions& options) {
    if (options.redirect.forwardSensitiveHeadersToOtherHosts) return false;
    for (const auto& item : options.headers.Items()) {
        if (IsRedirectSensitiveHeaderName(options.debug, item.name)) return true;
    }
    for (const auto& raw : options.rawHeaderLines) {
        const std::size_t separator = raw.find_first_of(":;");
        if (IsRedirectSensitiveHeaderName(options.debug,
                                          std::string_view(raw).substr(0, separator))) {
            return true;
        }
    }
    return false;
}

bool NeedsLegacyAuthorizationRedirectGuard(const RequestOptions& options) {
#if LIBCURL_VERSION_NUM < 0x073A00
    return !options.redirect.forwardAuthToOtherHosts &&
           (options.headers.IsExist("Authorization") || HasRawHeaderNamed(options, "Authorization"));
#else
    (void)options;
    return false;
#endif
}

bool NeedsLegacyCookieHeaderRedirectGuard(const RequestOptions& options) {
#if LIBCURL_VERSION_NUM < 0x074000
    return !options.redirect.forwardAuthToOtherHosts &&
           (options.headers.IsExist("Cookie") || HasRawHeaderNamed(options, "Cookie"));
#else
    (void)options;
    return false;
#endif
}

bool NeedsLegacyCustomRedirectGuard(std::string_view method,
                                    std::string_view body,
                                    const RequestOptions& options) {
    return options.redirect.follow &&
           UsesCustomRequest(method, body, options) &&
           !SupportsSafeCustomRedirectFollowing();
}

bool NeedsSensitiveRedirectGuard(std::string_view method,
                                 std::string_view body,
                                 const RequestOptions& options) {
    if (!options.redirect.follow) return false;
    if (NeedsLegacyCustomRedirectGuard(method, body, options)) return true;
    if (!options.tls.pinnedPublicKey.empty() && !options.redirect.allowUnpinnedRedirects) return true;
    if (!options.cookies.Empty() && !options.redirect.allowExplicitCookiesOnRedirects) return true;
    if (HasRedirectSensitiveCustomHeader(options)) return true;
    if (NeedsLegacyAuthorizationRedirectGuard(options)) return true;
    if (NeedsLegacyCookieHeaderRedirectGuard(options)) return true;
    return false;
}

void ApplyRedirectSafetyRefusal(CURL* curl,
                                std::string_view method,
                                std::string_view body,
                                const RequestOptions& options,
                                HttpResponse& response) {
    if (!curl || response.curl_code != CURLE_OK ||
        !NeedsSensitiveRedirectGuard(method, body, options)) {
        return;
    }

    char* redirectUrl = nullptr;
    const CURLcode infoCode = curl_easy_getinfo(curl, CURLINFO_REDIRECT_URL, &redirectUrl);
    if (infoCode != CURLE_OK) {
        response.success = false;
        response.curl_code = infoCode;
        response.errorCategory = HttpErrorCategory::Internal;
        response.error = std::string("读取安全重定向目标失败: ") + curl_easy_strerror(infoCode);
        return;
    }
    if (redirectUrl == nullptr || *redirectUrl == '\0') return;

    response.success = false;
    response.curl_code = CURLE_NOT_BUILT_IN;
    response.errorCategory = HttpErrorCategory::Internal;

    if (NeedsLegacyCustomRedirectGuard(method, body, options)) {
        response.error =
            "当前 libcurl 版本无法安全自动跟随使用 CURLOPT_CUSTOMREQUEST 的重定向；"
            "已在发送下一跳之前停止。请升级到 libcurl 8.13.0 或更高版本，"
            "或关闭 redirect.follow 并由业务层校验 Location 后重新发起请求";
        return;
    }
    if (!options.tls.pinnedPublicKey.empty() && !options.redirect.allowUnpinnedRedirects) {
        response.error =
            "检测到重定向，但当前请求配置了 pinnedPublicKey；为避免跳转到不受同一 Pin 保护的 Origin，"
            "已在发送下一跳之前停止。可关闭 redirect.follow 手动校验 Location，"
            "或在明确接受风险时设置 redirect.allowUnpinnedRedirects=true";
        return;
    }
    if (!options.cookies.Empty() && !options.redirect.allowExplicitCookiesOnRedirects) {
        response.error =
            "检测到重定向，但 RequestOptions::cookies 通过 CURLOPT_COOKIE 发送，libcurl 会把显式 Cookie 继续用于后续重定向；"
            "已在发送下一跳之前停止。可关闭 redirect.follow 手动处理，"
            "或在明确接受风险时设置 redirect.allowExplicitCookiesOnRedirects=true";
        return;
    }
    if (HasRedirectSensitiveCustomHeader(options)) {
        response.error =
            "检测到重定向且请求包含自定义敏感 Header；libcurl 无法保证跨主机跳转时自动剥离该 Header，"
            "已在发送下一跳之前停止。可关闭 redirect.follow 手动校验 Location，"
            "或在明确接受跨主机转发风险时设置 redirect.forwardSensitiveHeadersToOtherHosts=true";
        return;
    }
    if (NeedsLegacyAuthorizationRedirectGuard(options)) {
        response.error =
            "当前 libcurl 版本低于 7.58.0，无法安全自动保护跨主机重定向中的自定义 Authorization Header；"
            "已在发送下一跳之前停止";
        return;
    }
    if (NeedsLegacyCookieHeaderRedirectGuard(options)) {
        response.error =
            "当前 libcurl 版本低于 7.64.0，无法安全自动保护跨主机重定向中的自定义 Cookie Header；"
            "已在发送下一跳之前停止";
        return;
    }
}

bool ApplyMethod(CURL* curl,
                 std::string_view method,
                 std::string_view body,
                 const RequestOptions& options,
                 TransferContext& context,
                 CurlMimeHolder& mime,
                 HttpResponse& response) {
    if (!IsHttpToken(method)) {
        response.success = false;
        response.curl_code = CURLE_BAD_FUNCTION_ARGUMENT;
        response.errorCategory = HttpErrorCategory::InvalidArgument;
        response.error = "HTTP Method 不是合法的 token";
        return false;
    }

    const std::string upper = ToUpperCopy(method);
    const bool hasMultipart = !options.multipart.empty();

    if (hasMultipart) {
        if (!SetOpt(curl, CURLOPT_MIMEPOST, "CURLOPT_MIMEPOST", mime.value, response)) {
            return false;
        }
        if (upper != "POST") {
            const std::string methodText(method);
            if (!SetOpt(curl,
                        CURLOPT_CUSTOMREQUEST,
                        "CURLOPT_CUSTOMREQUEST",
                        methodText.c_str(),
                        response)) {
                return false;
            }
        }
        return true;
    }

    if (options.uploadSource) {
        if (!SetOpt(curl, CURLOPT_UPLOAD, "CURLOPT_UPLOAD", 1L, response)) return false;
        if (!SetOpt(curl, CURLOPT_READFUNCTION, "CURLOPT_READFUNCTION", UploadReadCallback, response)) return false;
        if (!SetOpt(curl, CURLOPT_READDATA, "CURLOPT_READDATA", &context, response)) return false;
        if (!SetOpt(curl, CURLOPT_SEEKFUNCTION, "CURLOPT_SEEKFUNCTION", UploadSeekCallback, response)) return false;
        if (!SetOpt(curl, CURLOPT_SEEKDATA, "CURLOPT_SEEKDATA", &context, response)) return false;

        std::optional<curl_off_t> sizeValue = options.uploadSize;
        if (!sizeValue) {
            try {
                sizeValue = options.uploadSource->Size();
            } catch (const std::exception& ex) {
                response.success = false;
                response.curl_code = CURLE_READ_ERROR;
                response.errorCategory = HttpErrorCategory::Upload;
                response.error = std::string("读取流式上传源大小时抛出异常: ") + ex.what();
                return false;
            } catch (...) {
                response.success = false;
                response.curl_code = CURLE_READ_ERROR;
                response.errorCategory = HttpErrorCategory::Upload;
                response.error = "读取流式上传源大小时抛出未知异常";
                return false;
            }
        }
        if (sizeValue && *sizeValue < 0) {
            response.success = false;
            response.curl_code = CURLE_BAD_FUNCTION_ARGUMENT;
            response.errorCategory = HttpErrorCategory::InvalidArgument;
            response.error = "流式上传源返回了负数大小";
            return false;
        }
        if (sizeValue && !SetOpt(curl, CURLOPT_INFILESIZE_LARGE, "CURLOPT_INFILESIZE_LARGE", *sizeValue, response)) {
            return false;
        }

        if (upper != "PUT") {
            const std::string methodText(method);
            if (!SetOpt(curl, CURLOPT_CUSTOMREQUEST, "CURLOPT_CUSTOMREQUEST", methodText.c_str(), response)) {
                return false;
            }
        }
        return true;
    }

    if (upper == "HEAD") {
        return SetOpt(curl, CURLOPT_NOBODY, "CURLOPT_NOBODY", 1L, response);
    }

    if (upper == "GET" && body.empty()) {
        return SetOpt(curl, CURLOPT_HTTPGET, "CURLOPT_HTTPGET", 1L, response);
    }

    if (upper == "POST") {
        if (!SetOpt(curl, CURLOPT_POST, "CURLOPT_POST", 1L, response)) {
            return false;
        }
        if (!SetOpt(curl,
                    CURLOPT_POSTFIELDS,
                    "CURLOPT_POSTFIELDS",
                    body.empty() ? "" : body.data(),
                    response)) {
            return false;
        }
        return SetOpt(curl,
                      CURLOPT_POSTFIELDSIZE_LARGE,
                      "CURLOPT_POSTFIELDSIZE_LARGE",
                      static_cast<curl_off_t>(body.size()),
                      response);
    }

    // PUT、PATCH、DELETE 和自定义请求方法统一使用 CUSTOMREQUEST。
    // POSTFIELDS 提供请求数据，同时显式设置 Method，避免请求行被自动改成 POST。
    const std::string methodText(method);
    if (!SetOpt(curl,
                CURLOPT_CUSTOMREQUEST,
                "CURLOPT_CUSTOMREQUEST",
                methodText.c_str(),
                response)) {
        return false;
    }

    if (!body.empty()) {
        if (!SetOpt(curl,
                    CURLOPT_POSTFIELDS,
                    "CURLOPT_POSTFIELDS",
                    body.data(),
                    response)) {
            return false;
        }
        if (!SetOpt(curl,
                    CURLOPT_POSTFIELDSIZE_LARGE,
                    "CURLOPT_POSTFIELDSIZE_LARGE",
                    static_cast<curl_off_t>(body.size()),
                    response)) {
            return false;
        }
    }

    return true;
}

bool ConfigureRequest(CURL* curl,
                      std::string_view method,
                      std::string_view url,
                      std::string_view body,
                      const RequestOptions& options,
                      TransferContext& context,
                      CurlSlistHolder& headerList,
                      CurlSlistHolder& resolveList,
                      CurlMimeHolder& mime,
                      std::array<char, CURL_ERROR_SIZE>& errorBuffer,
                      HttpResponse& response) {
    const auto containsUrlControl = [](std::string_view value) noexcept {
        for (const char ch : value) {
            const unsigned char c = static_cast<unsigned char>(ch);
            if (c < 32U || c == 127U) return true;
        }
        return false;
    };
    if (url.empty() || containsUrlControl(url)) {
        response.success = false;
        response.curl_code = CURLE_URL_MALFORMAT;
        response.errorCategory = HttpErrorCategory::InvalidArgument;
        response.error = "URL 为空或包含未百分号编码的控制字符";
        return false;
    }

    const auto invalidArgument = [&](std::string message) {
        response.success = false;
        response.curl_code = CURLE_BAD_FUNCTION_ARGUMENT;
        response.errorCategory = HttpErrorCategory::InvalidArgument;
        response.error = std::move(message);
        return false;
    };

    if (!IsValidHttpVersion(options.httpVersion)) {
        return invalidArgument("HttpVersion 枚举值无效");
    }
    if (!IsValidTlsVersion(options.tls.minVersion) ||
        !IsValidTlsVersion(options.tls.maxVersion)) {
        return invalidArgument("TlsVersion 枚举值无效");
    }
    if (!IsValidRevocationPolicy(options.tls.revocationPolicy)) {
        return invalidArgument("CertificateRevocationPolicy 枚举值无效");
    }
    if (options.proxy && options.proxy->type && !IsValidProxyType(*options.proxy->type)) {
        return invalidArgument("ProxyType 枚举值无效");
    }

    if (options.connectTimeout.count() < 0 || options.timeout.count() < 0) {
        return invalidArgument("超时配置不能为负数");
    }
    if (options.retry.maxRetries < 0 ||
        options.retry.baseDelay.count() < 0 ||
        options.retry.maxDelay.count() < 0) {
        return invalidArgument("重试次数和重试延迟不能为负数");
    }
    if (options.lowSpeedLimitBytesPerSecond < 0 || options.lowSpeedTime.count() < 0) {
        return invalidArgument("低速检测配置不能为负数");
    }
    if (options.redirect.maxRedirects < -1) {
        return invalidArgument("maxRedirects 不能小于 -1");
    }
    if (options.tcpKeepIdleSeconds < 0 || options.tcpKeepIntervalSeconds < 0) {
        return invalidArgument("TCP KeepAlive 时间不能为负数");
    }
    if (options.network.localPort < 0 || options.network.localPort > 65535 || options.network.localPortRange < 1) {
        return invalidArgument("本地端口或端口范围配置无效");
    }
    if (options.network.localPort == 0 && options.network.localPortRange > 1) {
        return invalidArgument("localPortRange 大于 1 时必须同时设置 localPort");
    }
    if (options.network.localPort > 0 &&
        options.network.localPortRange > 65536L - options.network.localPort) {
        return invalidArgument("localPort 与 localPortRange 组合超出有效端口范围");
    }
    for (const auto& entry : options.network.resolve) {
        if (entry.host.empty() || entry.port == 0 || entry.address.empty()) {
            return invalidArgument("DNS 强制解析项必须同时提供 host、port 和 address");
        }
    }
    if (options.speed.maxDownloadBytesPerSecond < 0 || options.speed.maxUploadBytesPerSecond < 0) {
        return invalidArgument("上传或下载限速不能为负数");
    }
    if (options.uploadSize && *options.uploadSize < 0) {
        return invalidArgument("uploadSize 不能为负数");
    }
    if (options.resumeFrom && *options.resumeFrom < 0) {
        return invalidArgument("resumeFrom 不能为负数");
    }
    if (options.resumeFrom && ToUpperCopy(method) != "GET") {
        return invalidArgument("resumeFrom 仅支持 GET 下载请求");
    }
    if (!options.multipart.empty() && options.uploadSource) {
        return invalidArgument("multipart 与 uploadSource 不能同时使用");
    }
    if (!body.empty() && (!options.multipart.empty() || options.uploadSource)) {
        return invalidArgument("内存 Body 不能与 multipart 或 uploadSource 同时使用");
    }
    if (options.uploadSize && !options.uploadSource) {
        return invalidArgument("uploadSize 只能与 uploadSource 一起使用");
    }
    if (!options.range.empty() && options.resumeFrom) {
        return invalidArgument("range 与 resumeFrom 不能同时使用");
    }
    if (!options.enableCookieEngine &&
        (options.newCookieSession || !options.cookieFile.empty() || !options.cookieJar.empty())) {
        return invalidArgument("Cookie Engine 关闭时不能同时配置 newCookieSession、cookieFile 或 cookieJar");
    }
    if (body.size() > static_cast<std::size_t>((std::numeric_limits<curl_off_t>::max)())) {
        return invalidArgument("请求 Body 大小超过 curl_off_t 可表示范围");
    }
    if (options.tls.minVersion != TlsVersion::Default &&
        options.tls.maxVersion != TlsVersion::Default &&
        TlsVersionRank(options.tls.minVersion) > TlsVersionRank(options.tls.maxVersion)) {
        return invalidArgument("TLS 最低版本不能高于最高版本");
    }
    if (options.tls.verifyStatus && !options.tls.verifyPeer) {
        return invalidArgument("verifyStatus 依赖证书链验证，verifyPeer=false 时不能启用");
    }

    if (!IsSafeHttpFieldValue(options.userAgent)) {
        return invalidArgument("User-Agent 包含非法控制字符");
    }
    if (!IsSafeHttpFieldValue(options.acceptEncoding)) {
        return invalidArgument("Accept-Encoding 包含非法控制字符");
    }
    if (!IsSafeHttpFieldValue(options.range)) {
        return invalidArgument("Range 包含非法控制字符");
    }
    if (!IsSafeHttpFieldValue(options.bearerToken)) {
        return invalidArgument("Bearer Token 包含非法控制字符");
    }
    const auto rejectNul = [&](std::string_view value, const char* name) -> bool {
        if (!ContainsNul(value)) return false;
        response.success = false;
        response.curl_code = CURLE_BAD_FUNCTION_ARGUMENT;
        response.errorCategory = HttpErrorCategory::InvalidArgument;
        response.error = std::string(name) + " 包含 NUL 字节";
        return true;
    };
    if (rejectNul(options.username, "username") || rejectNul(options.password, "password") ||
        rejectNul(options.cookieFile, "cookieFile") || rejectNul(options.cookieJar, "cookieJar") ||
        rejectNul(options.network.interfaceName, "interfaceName") || rejectNul(options.tls.caFile, "caFile") ||
        rejectNul(options.tls.caPath, "caPath") || rejectNul(options.tls.clientCertificate, "clientCertificate") ||
        rejectNul(options.tls.clientCertificateType, "clientCertificateType") || rejectNul(options.tls.clientKey, "clientKey") ||
        rejectNul(options.tls.clientKeyType, "clientKeyType") || rejectNul(options.tls.clientKeyPassword, "clientKeyPassword") ||
        rejectNul(options.tls.pinnedPublicKey, "pinnedPublicKey") || rejectNul(options.tls.cipherList, "cipherList") ||
        rejectNul(options.tls.tls13CipherList, "tls13CipherList") || rejectNul(options.tls.crlFile, "crlFile")) return false;
    if (options.proxy && (rejectNul(options.proxy->url, "proxy.url") || rejectNul(options.proxy->username, "proxy.username") ||
        rejectNul(options.proxy->password, "proxy.password") || rejectNul(options.proxy->noProxy, "proxy.noProxy"))) return false;
    if (options.proxy && !options.proxy->url.empty() &&
        (ContainsCrLf(options.proxy->url) || !IsSafeHttpFieldValue(options.proxy->url))) {
        return invalidArgument("proxy.url 包含非法控制字符");
    }
    for (const auto& entry : options.network.resolve) {
        if (rejectNul(entry.host, "resolve.host") || rejectNul(entry.address, "resolve.address")) return false;
        if (!IsSafeHttpFieldValue(entry.host) || !IsSafeHttpFieldValue(entry.address) ||
            ContainsCrLf(entry.host) || ContainsCrLf(entry.address)) {
            return invalidArgument("DNS 强制解析项包含非法控制字符");
        }
    }
    const auto hasRawHeader = [&](std::string_view wantedName) {
        const std::string wanted = ToLowerCopy(wantedName);
        for (const auto& raw : options.rawHeaderLines) {
            const std::size_t separator = raw.find_first_of(":;");
            const std::string name = ToLowerCopy(TrimCopy(std::string_view(raw).substr(0, separator)));
            if (name == wanted) return true;
        }
        return false;
    };
    if ((!options.bearerToken.empty() || !options.username.empty()) &&
        (options.headers.IsExist("Authorization") || hasRawHeader("Authorization"))) {
        return invalidArgument("不能同时使用 Authorization Header 与 bearerToken/username 认证配置");
    }
    if (options.proxy && !options.proxy->url.empty() &&
        (!options.proxy->username.empty() || !options.proxy->password.empty()) &&
        (options.headers.IsExist("Proxy-Authorization") || hasRawHeader("Proxy-Authorization"))) {
        return invalidArgument("不能同时使用 Proxy-Authorization Header 与代理用户名/密码认证配置");
    }
    for (const auto& cookie : options.cookies.GetAllCookies(false)) {
        if (!IsHttpToken(cookie.name) || !IsSafeHttpFieldValue(cookie.value) || cookie.value.find(';') != std::string::npos) {
            return invalidArgument("请求 Cookie 名称或值包含非法字符");
        }
    }


#if LIBCURL_VERSION_NUM < 0x072100
    if (options.httpVersion == HttpVersion::Http2 || options.httpVersion == HttpVersion::Http2Tls) {
        response.success = false;
        response.curl_code = CURLE_NOT_BUILT_IN;
        response.error = "当前 libcurl 版本不支持 HTTP/2";
        return false;
    }
#elif LIBCURL_VERSION_NUM < 0x072F00
    if (options.httpVersion == HttpVersion::Http2Tls) {
        response.success = false;
        response.curl_code = CURLE_NOT_BUILT_IN;
        response.error = "当前 libcurl 版本不支持 HTTP/2 TLS 模式";
        return false;
    }
#endif
#if LIBCURL_VERSION_NUM < 0x074200
    if (options.httpVersion == HttpVersion::Http3) {
        response.success = false;
        response.curl_code = CURLE_NOT_BUILT_IN;
        response.error = "当前 libcurl 版本不支持 HTTP/3";
        return false;
    }
#endif
#if LIBCURL_VERSION_NUM < 0x073400
    if (options.tls.minVersion == TlsVersion::Tls1_3 || options.tls.maxVersion == TlsVersion::Tls1_3) {
        response.success = false;
        response.curl_code = CURLE_NOT_BUILT_IN;
        response.error = "当前 libcurl 版本不支持 TLS 1.3 版本限制";
        return false;
    }
#endif

    if (!ConfigureHeaders(options, headerList, response)) {
        return false;
    }
    if (!ConfigureMime(curl, options, mime, response)) {
        return false;
    }

    const std::string urlText(url);
    if (!SetOpt(curl, CURLOPT_URL, "CURLOPT_URL", urlText.c_str(), response)) return false;
    if (!SetOpt(curl, CURLOPT_ERRORBUFFER, "CURLOPT_ERRORBUFFER", errorBuffer.data(), response)) return false;
    // 本封装同时提供同步客户端和多线程 curl_multi 客户端，统一禁用 libcurl 的信号路径，
    // 避免多线程环境中信号处理器恢复产生竞态。
    if (!SetOpt(curl, CURLOPT_NOSIGNAL, "CURLOPT_NOSIGNAL", 1L, response)) return false;
    if (!SetOpt(curl, CURLOPT_WRITEFUNCTION, "CURLOPT_WRITEFUNCTION", WriteCallback, response)) return false;
    if (!SetOpt(curl, CURLOPT_WRITEDATA, "CURLOPT_WRITEDATA", &context, response)) return false;
    if (!SetOpt(curl, CURLOPT_HEADERFUNCTION, "CURLOPT_HEADERFUNCTION", HeaderCallback, response)) return false;
    if (!SetOpt(curl, CURLOPT_HEADERDATA, "CURLOPT_HEADERDATA", &context, response)) return false;

    if (headerList.value &&
        !SetOpt(curl, CURLOPT_HTTPHEADER, "CURLOPT_HTTPHEADER", headerList.value, response)) {
        return false;
    }

    // HTTPS 经 HTTP 代理时不把 CONNECT 隧道 Header 混入目标服务器 Header。
    if (!SetOpt(curl, CURLOPT_SUPPRESS_CONNECT_HEADERS, "CURLOPT_SUPPRESS_CONNECT_HEADERS", 1L, response)) return false;

    const bool redirectSafetyGuard =
        NeedsSensitiveRedirectGuard(method, body, options);
#if LIBCURL_VERSION_NUM >= 0x080D00
    // 8.13.0 起 CURLFOLLOW_OBEYCODE 才能让 CUSTOMREQUEST 正确服从 301/302/303 语义。
    // 同时检查运行期版本，避免“新头文件 + 旧动态库”时把数值 2 当作普通 true 使用。
    const long followLocationMode = options.redirect.follow
        ? (redirectSafetyGuard ? 0L : static_cast<long>(CURLFOLLOW_OBEYCODE))
        : 0L;
#else
    // 旧版 libcurl 的 FOLLOWLOCATION 会让 CUSTOMREQUEST 覆盖后续重定向 Method。
    // 对这类请求先不自动跟随；完成首跳后若存在重定向，再返回明确的 fail-closed 错误。
    const long followLocationMode =
        (options.redirect.follow && !redirectSafetyGuard) ? 1L : 0L;
#endif
    if (!SetOpt(curl,
                CURLOPT_FOLLOWLOCATION,
                "CURLOPT_FOLLOWLOCATION",
                followLocationMode,
                response)) return false;
    if (!SetOpt(curl, CURLOPT_MAXREDIRS, "CURLOPT_MAXREDIRS", options.redirect.maxRedirects, response)) return false;
    if (!SetOpt(curl,
                CURLOPT_AUTOREFERER,
                "CURLOPT_AUTOREFERER",
                options.redirect.autoReferer ? 1L : 0L,
                response)) return false;

    if (options.autoDecompress) {
        const char* encoding = options.acceptEncoding.empty() ? "" : options.acceptEncoding.c_str();
        if (!SetOpt(curl, CURLOPT_ACCEPT_ENCODING, "CURLOPT_ACCEPT_ENCODING", encoding, response)) return false;
        if (!SetOpt(curl, CURLOPT_HTTP_CONTENT_DECODING, "CURLOPT_HTTP_CONTENT_DECODING", 1L, response)) return false;
    } else {
        if (!options.acceptEncoding.empty() &&
            !SetOpt(curl, CURLOPT_ACCEPT_ENCODING, "CURLOPT_ACCEPT_ENCODING", options.acceptEncoding.c_str(), response)) return false;
        if (!SetOpt(curl, CURLOPT_HTTP_CONTENT_DECODING, "CURLOPT_HTTP_CONTENT_DECODING", 0L, response)) return false;
    }

    if (!SetOpt(curl,
                CURLOPT_CONNECTTIMEOUT_MS,
                "CURLOPT_CONNECTTIMEOUT_MS",
                static_cast<long>((std::min)(options.connectTimeout.count(),
                                                  static_cast<std::int64_t>((std::numeric_limits<long>::max)()))),
                response)) return false;
    if (!SetOpt(curl,
                CURLOPT_TIMEOUT_MS,
                "CURLOPT_TIMEOUT_MS",
                static_cast<long>((std::min)(options.timeout.count(),
                                                  static_cast<std::int64_t>((std::numeric_limits<long>::max)()))),
                response)) return false;
    if (!SetOpt(curl,
                CURLOPT_LOW_SPEED_LIMIT,
                "CURLOPT_LOW_SPEED_LIMIT",
                options.lowSpeedLimitBytesPerSecond,
                response)) return false;
    if (!SetOpt(curl,
                CURLOPT_LOW_SPEED_TIME,
                "CURLOPT_LOW_SPEED_TIME",
                static_cast<long>((std::min)(options.lowSpeedTime.count(),
                                              static_cast<decltype(options.lowSpeedTime.count())>((std::numeric_limits<long>::max)()))),
                response)) return false;

    if (!SetOpt(curl,
                CURLOPT_SSL_VERIFYPEER,
                "CURLOPT_SSL_VERIFYPEER",
                options.tls.verifyPeer ? 1L : 0L,
                response)) return false;
    if (!SetOpt(curl,
                CURLOPT_SSL_VERIFYHOST,
                "CURLOPT_SSL_VERIFYHOST",
                options.tls.verifyHost ? 2L : 0L,
                response)) return false;

    // CURLOPT_SSL_OPTIONS 是 bitmask，必须一次性合并所有 TLS 行为，不能让后设置的选项
    // 覆盖前面的证书吊销或 Native CA 策略。
    long sslOptions = 0L;

    // Schannel 默认会执行证书吊销检查。BestEffort 在保留已知吊销证书拦截能力的同时，
    // 允许 CRL/OCSP 分发点缺失或暂时离线，适合常见代理、企业网络和抓包环境。
    if (options.tls.verifyPeer && IsSchannelBackend()) {
        sslOptions |= SchannelRevocationOptions(options.tls.revocationPolicy);
    }

#if defined(_WIN32) && LIBCURL_VERSION_NUM >= 0x074700
    // Windows + OpenSSL 默认从 Windows 原生证书库取得可信根。
    // Schannel 本身已经使用 Windows 证书库，因此不需要设置 Native CA bit。
    // 显式 caFile/caPath 时 ShouldUseNativeCa() 返回 false，使自定义 CA 保持可预测的独占语义。
    if (ShouldUseNativeCa(options.tls)) {
        sslOptions |= static_cast<long>(CURLSSLOPT_NATIVE_CA);
    }
#endif

    if (sslOptions != 0L &&
        !SetOpt(curl, CURLOPT_SSL_OPTIONS, "CURLOPT_SSL_OPTIONS", sslOptions, response)) {
        return false;
    }

    // HTTPS 代理自身也需要 TLS；沿用同一个 SSL_OPTIONS bitmask，避免目标 TLS 与代理 TLS
    // 在 Native CA / Schannel 吊销策略上出现不一致。
    if (options.proxy && !options.proxy->url.empty() && IsHttpsProxy(*options.proxy) &&
        sslOptions != 0L &&
        !SetOpt(curl, CURLOPT_PROXY_SSL_OPTIONS, "CURLOPT_PROXY_SSL_OPTIONS", sslOptions, response)) {
        return false;
    }

#if LIBCURL_VERSION_NUM >= 0x072900
    // OCSP Stapling 验证并非所有 TLS 后端都支持。
    // 默认关闭时不调用该选项，避免 Schannel 等后端即使传入 0L 也返回 CURLE_NOT_BUILT_IN。
    if (options.tls.verifyStatus &&
        !SetOpt(curl,
                CURLOPT_SSL_VERIFYSTATUS,
                "CURLOPT_SSL_VERIFYSTATUS",
                1L,
                response)) return false;
#endif

    if (!options.tls.caFile.empty() &&
        !SetOpt(curl, CURLOPT_CAINFO, "CURLOPT_CAINFO", options.tls.caFile.c_str(), response)) return false;
    if (!options.tls.caPath.empty() &&
        !SetOpt(curl, CURLOPT_CAPATH, "CURLOPT_CAPATH", options.tls.caPath.c_str(), response)) return false;

    LogTlsConfigurationDebug(curl, options);

    if (!options.tls.clientCertificate.empty() &&
        !SetOpt(curl, CURLOPT_SSLCERT, "CURLOPT_SSLCERT", options.tls.clientCertificate.c_str(), response)) return false;
    if (!options.tls.clientCertificateType.empty() &&
        !SetOpt(curl, CURLOPT_SSLCERTTYPE, "CURLOPT_SSLCERTTYPE", options.tls.clientCertificateType.c_str(), response)) return false;
    if (!options.tls.clientKey.empty() &&
        !SetOpt(curl, CURLOPT_SSLKEY, "CURLOPT_SSLKEY", options.tls.clientKey.c_str(), response)) return false;
    if (!options.tls.clientKeyType.empty() &&
        !SetOpt(curl, CURLOPT_SSLKEYTYPE, "CURLOPT_SSLKEYTYPE", options.tls.clientKeyType.c_str(), response)) return false;
    if (!options.tls.clientKeyPassword.empty() &&
        !SetOpt(curl, CURLOPT_KEYPASSWD, "CURLOPT_KEYPASSWD", options.tls.clientKeyPassword.c_str(), response)) return false;
    if (!options.tls.pinnedPublicKey.empty() &&
        !SetOpt(curl, CURLOPT_PINNEDPUBLICKEY, "CURLOPT_PINNEDPUBLICKEY", options.tls.pinnedPublicKey.c_str(), response)) return false;
    if (!options.tls.cipherList.empty() &&
        !SetOpt(curl, CURLOPT_SSL_CIPHER_LIST, "CURLOPT_SSL_CIPHER_LIST", options.tls.cipherList.c_str(), response)) return false;
#if LIBCURL_VERSION_NUM >= 0x073D00
    if (!options.tls.tls13CipherList.empty() &&
        !SetOpt(curl, CURLOPT_TLS13_CIPHERS, "CURLOPT_TLS13_CIPHERS", options.tls.tls13CipherList.c_str(), response)) return false;
#else
    if (!options.tls.tls13CipherList.empty()) { response.curl_code = CURLE_NOT_BUILT_IN; response.error = "当前 libcurl 版本不支持 TLS 1.3 Cipher 配置"; return false; }
#endif
    if (!options.tls.crlFile.empty() &&
        !SetOpt(curl, CURLOPT_CRLFILE, "CURLOPT_CRLFILE", options.tls.crlFile.c_str(), response)) return false;
    // V4.2.6 起默认最低 TLS 1.2；V4.2.7 继续保持该语义，避免在 libcurl 8.16.0 之前由运行库/TLS backend 默认值
    // 意外允许 TLS 1.0/1.1。调用方仍可显式选择更低版本承担兼容性风险。
    // HTTPS 代理同样应用这一版本边界，否则目标站点安全而代理握手仍可能退回旧 TLS。
    if (options.tls.minVersion != TlsVersion::Default ||
        options.tls.maxVersion != TlsVersion::Default) {
        const long tlsVersion = ComposeTlsVersionOption(options.tls.minVersion, options.tls.maxVersion);
        if (!SetOpt(curl, CURLOPT_SSLVERSION, "CURLOPT_SSLVERSION", tlsVersion, response)) return false;
        if (options.proxy && !options.proxy->url.empty() && IsHttpsProxy(*options.proxy) &&
            !SetOpt(curl, CURLOPT_PROXY_SSLVERSION, "CURLOPT_PROXY_SSLVERSION", tlsVersion, response)) return false;
    }

    // Auto 表示完全采用当前 libcurl 的默认 HTTP 协商策略。
    if (options.httpVersion != HttpVersion::Auto &&
        !SetOpt(curl,
                CURLOPT_HTTP_VERSION,
                "CURLOPT_HTTP_VERSION",
                CurlHttpVersion(options.httpVersion),
                response)) return false;

    // 本封装默认仅允许 HTTP/HTTPS，避免用户可控 URL 或重定向访问 FILE、FTP、SMB 等协议。
#if LIBCURL_VERSION_NUM >= 0x075500
    if (!SetOpt(curl, CURLOPT_PROTOCOLS_STR, "CURLOPT_PROTOCOLS_STR", "http,https", response)) return false;
    {
        // 关闭 HTTP 重定向目标后统一只允许 HTTPS。这样不仅保护初始 HTTPS 请求，
        // 也能阻止 HTTP→HTTPS→HTTP 这类多跳链路中的后续降级。
        const char* redirectProtocols = options.redirect.allowHttpsToHttp ? "http,https" : "https";
        if (!SetOpt(curl, CURLOPT_REDIR_PROTOCOLS_STR, "CURLOPT_REDIR_PROTOCOLS_STR", redirectProtocols, response)) return false;
    }
#else
    if (!SetOpt(curl,
                CURLOPT_PROTOCOLS,
                "CURLOPT_PROTOCOLS",
                static_cast<long>(CURLPROTO_HTTP | CURLPROTO_HTTPS),
                response)) return false;
    {
        const long redirectProtocols = options.redirect.allowHttpsToHttp
            ? static_cast<long>(CURLPROTO_HTTP | CURLPROTO_HTTPS)
            : static_cast<long>(CURLPROTO_HTTPS);
        if (!SetOpt(curl, CURLOPT_REDIR_PROTOCOLS, "CURLOPT_REDIR_PROTOCOLS", redirectProtocols, response)) return false;
    }
#endif

#if LIBCURL_VERSION_NUM >= 0x073D00
    if (!SetOpt(curl,
                CURLOPT_DISALLOW_USERNAME_IN_URL,
                "CURLOPT_DISALLOW_USERNAME_IN_URL",
                options.disallowUsernameInUrl ? 1L : 0L,
                response)) return false;
#else
    if (options.disallowUsernameInUrl) {
        response.success = false;
        response.curl_code = CURLE_NOT_BUILT_IN;
        response.errorCategory = HttpErrorCategory::InvalidArgument;
        response.error = "disallowUsernameInUrl=true 需要 libcurl 7.61.0 或更新版本";
        return false;
    }
#endif

    if (!SetOpt(curl,
                CURLOPT_FAILONERROR,
                "CURLOPT_FAILONERROR",
                options.failOnHttpError ? 1L : 0L,
                response)) return false;
    if (!SetOpt(curl, CURLOPT_UNRESTRICTED_AUTH, "CURLOPT_UNRESTRICTED_AUTH",
                options.redirect.forwardAuthToOtherHosts ? 1L : 0L, response)) return false;

    if (!SetOpt(curl,
                CURLOPT_TCP_KEEPALIVE,
                "CURLOPT_TCP_KEEPALIVE",
                options.tcpKeepAlive ? 1L : 0L,
                response)) return false;
    if (options.tcpKeepAlive) {
        if (!SetOpt(curl, CURLOPT_TCP_KEEPIDLE, "CURLOPT_TCP_KEEPIDLE", options.tcpKeepIdleSeconds, response)) return false;
        if (!SetOpt(curl, CURLOPT_TCP_KEEPINTVL, "CURLOPT_TCP_KEEPINTVL", options.tcpKeepIntervalSeconds, response)) return false;
    }

    if (options.maxResponseSize > 0) {
        const auto maxCurl = static_cast<curl_off_t>((std::min)(
            static_cast<std::uint64_t>(options.maxResponseSize),
            static_cast<std::uint64_t>((std::numeric_limits<curl_off_t>::max)())));
        if (!SetOpt(curl, CURLOPT_MAXFILESIZE_LARGE, "CURLOPT_MAXFILESIZE_LARGE", maxCurl, response)) return false;
    }

    if (!options.userAgent.empty() &&
        !SetOpt(curl, CURLOPT_USERAGENT, "CURLOPT_USERAGENT", options.userAgent.c_str(), response)) return false;

    if (options.enableCookieEngine) {
        const char* cookieFile = options.cookieFile.empty() ? "" : options.cookieFile.c_str();
        if (!SetOpt(curl, CURLOPT_COOKIEFILE, "CURLOPT_COOKIEFILE", cookieFile, response)) return false;
        if (options.newCookieSession &&
            !SetOpt(curl, CURLOPT_COOKIESESSION, "CURLOPT_COOKIESESSION", 1L, response)) return false;
    } else {
        // 显式关闭 Cookie Engine，避免复用 easy handle 时仅清空 Cookie 数据但引擎仍继续接收 Set-Cookie。
        const char* noCookieFile = nullptr;
        if (!SetOpt(curl, CURLOPT_COOKIEFILE, "CURLOPT_COOKIEFILE", noCookieFile, response)) return false;
    }

    if (!options.cookieJar.empty() &&
        !SetOpt(curl, CURLOPT_COOKIEJAR, "CURLOPT_COOKIEJAR", options.cookieJar.c_str(), response)) return false;

    // 显式空值 Cookie（例如 "flag="）是合法请求语义，不能把空字符串误当成“未设置”。
    const std::string explicitCookies = options.cookies.ToRequestCookieString(false);
    if (!explicitCookies.empty() &&
        !SetOpt(curl, CURLOPT_COOKIE, "CURLOPT_COOKIE", explicitCookies.c_str(), response)) return false;

    if (options.proxy) {
        // optional 已存在表示调用方明确接管代理策略；空字符串按 libcurl 语义显式禁用环境代理。
        if (!SetOpt(curl, CURLOPT_PROXY, "CURLOPT_PROXY", options.proxy->url.c_str(), response)) return false;
        if (!options.proxy->url.empty() && options.proxy->type) {
            const auto proxyType = CurlProxyType(*options.proxy->type);
            if (!proxyType) {
                response.success = false;
                response.curl_code = CURLE_NOT_BUILT_IN;
                response.error = "当前 libcurl 版本不支持所请求的代理类型";
                return false;
            }
            if (!SetOpt(curl,
                        CURLOPT_PROXYTYPE,
                        "CURLOPT_PROXYTYPE",
                        *proxyType,
                        response)) return false;
        }
        if (!options.proxy->url.empty()) {
            if (!options.proxy->username.empty() &&
                !SetOpt(curl, CURLOPT_PROXYUSERNAME, "CURLOPT_PROXYUSERNAME", options.proxy->username.c_str(), response)) return false;
            if (!options.proxy->password.empty() &&
                !SetOpt(curl, CURLOPT_PROXYPASSWORD, "CURLOPT_PROXYPASSWORD", options.proxy->password.c_str(), response)) return false;
            if (!SetOpt(curl, CURLOPT_PROXYAUTH, "CURLOPT_PROXYAUTH", static_cast<long>(options.proxy->auth), response)) return false;
        }
        if (!options.proxy->noProxy.empty() &&
            !SetOpt(curl, CURLOPT_NOPROXY, "CURLOPT_NOPROXY", options.proxy->noProxy.c_str(), response)) return false;
    }

    if (!options.bearerToken.empty()) {
#if LIBCURL_VERSION_NUM >= 0x073D00
        if (!SetOpt(curl, CURLOPT_HTTPAUTH, "CURLOPT_HTTPAUTH", static_cast<long>(CURLAUTH_BEARER), response)) return false;
        if (!SetOpt(curl, CURLOPT_XOAUTH2_BEARER, "CURLOPT_XOAUTH2_BEARER", options.bearerToken.c_str(), response)) return false;
#else
        response.success = false;
        response.curl_code = CURLE_NOT_BUILT_IN;
        response.errorCategory = HttpErrorCategory::InvalidArgument;
        response.error = "HTTP Bearer 认证需要 libcurl 7.61.0 或更新版本";
        return false;
#endif
    } else if (!options.username.empty()) {
        if (!SetOpt(curl, CURLOPT_USERNAME, "CURLOPT_USERNAME", options.username.c_str(), response)) return false;
        if (!SetOpt(curl, CURLOPT_PASSWORD, "CURLOPT_PASSWORD", options.password.c_str(), response)) return false;
        if (!SetOpt(curl, CURLOPT_HTTPAUTH, "CURLOPT_HTTPAUTH", static_cast<long>(options.httpAuth), response)) return false;
    }

    if (options.cancelFlag || options.progressCallback) {
        if (!SetOpt(curl, CURLOPT_NOPROGRESS, "CURLOPT_NOPROGRESS", 0L, response)) return false;
        if (!SetOpt(curl, CURLOPT_XFERINFOFUNCTION, "CURLOPT_XFERINFOFUNCTION", ProgressCallbackBridge, response)) return false;
        if (!SetOpt(curl, CURLOPT_XFERINFODATA, "CURLOPT_XFERINFODATA", &context, response)) return false;
    } else {
        if (!SetOpt(curl, CURLOPT_NOPROGRESS, "CURLOPT_NOPROGRESS", 1L, response)) return false;
    }

    if (!options.network.interfaceName.empty() &&
        !SetOpt(curl, CURLOPT_INTERFACE, "CURLOPT_INTERFACE", options.network.interfaceName.c_str(), response)) return false;
    if (options.network.localPort > 0 &&
        !SetOpt(curl, CURLOPT_LOCALPORT, "CURLOPT_LOCALPORT", options.network.localPort, response)) return false;
    if (options.network.localPortRange > 1 &&
        !SetOpt(curl, CURLOPT_LOCALPORTRANGE, "CURLOPT_LOCALPORTRANGE", options.network.localPortRange, response)) return false;
    for (const auto& entry : options.network.resolve) {
        if (entry.host.empty() || entry.port == 0 || entry.address.empty()) continue;
        std::string address = entry.address;
        if (address.find(':') != std::string::npos && !(address.size() >= 2 && address.front() == '[' && address.back() == ']')) {
            address = "[" + address + "]";
        }
        const std::string line = entry.host + ":" + std::to_string(entry.port) + ":" + address;
        if (!resolveList.Append(line)) {
            response.success = false;
            response.curl_code = CURLE_OUT_OF_MEMORY;
            response.error = "构造 CURLOPT_RESOLVE 列表失败";
            return false;
        }
    }
    if (resolveList.value &&
        !SetOpt(curl, CURLOPT_RESOLVE, "CURLOPT_RESOLVE", resolveList.value, response)) return false;

    if (options.speed.maxDownloadBytesPerSecond > 0 &&
        !SetOpt(curl, CURLOPT_MAX_RECV_SPEED_LARGE, "CURLOPT_MAX_RECV_SPEED_LARGE", options.speed.maxDownloadBytesPerSecond, response)) return false;
    if (options.speed.maxUploadBytesPerSecond > 0 &&
        !SetOpt(curl, CURLOPT_MAX_SEND_SPEED_LARGE, "CURLOPT_MAX_SEND_SPEED_LARGE", options.speed.maxUploadBytesPerSecond, response)) return false;
    if (!options.range.empty() &&
        !SetOpt(curl, CURLOPT_RANGE, "CURLOPT_RANGE", options.range.c_str(), response)) return false;
    if (options.resumeFrom &&
        !SetOpt(curl, CURLOPT_RESUME_FROM_LARGE, "CURLOPT_RESUME_FROM_LARGE", *options.resumeFrom, response)) return false;
    if (options.debug.enabled && options.debug.curlVerbose) {
        // debug.enabled 是封装层日志总开关。只有总开关与 curlVerbose 同时开启时才安装 verbose 回调。
        // 不使用 libcurl 默认 verbose sink：默认实现会把原始 Header/URL 写入 stderr，可能绕过封装层脱敏。
        if (!SetOpt(curl, CURLOPT_DEBUGFUNCTION, "CURLOPT_DEBUGFUNCTION", CurlVerboseCallback, response)) return false;
        if (!SetOpt(curl, CURLOPT_DEBUGDATA, "CURLOPT_DEBUGDATA", &context, response)) return false;
        if (!SetOpt(curl, CURLOPT_VERBOSE, "CURLOPT_VERBOSE", 1L, response)) return false;
    }

    if (!ApplyMethod(curl, method, body, options, context, mime, response)) {
        return false;
    }

    if (options.nativeCurlOptions) {
        try {
            options.nativeCurlOptions(curl);
        } catch (const std::exception& ex) {
            response.success = false;
            response.curl_code = CURLE_ABORTED_BY_CALLBACK;
            response.errorCategory = HttpErrorCategory::Internal;
            response.error = std::string("nativeCurlOptions 回调抛出异常: ") + ex.what();
            return false;
        } catch (...) {
            response.success = false;
            response.curl_code = CURLE_ABORTED_BY_CALLBACK;
            response.errorCategory = HttpErrorCategory::Internal;
            response.error = "nativeCurlOptions 回调抛出未知异常";
            return false;
        }
    }

    return true;
}

void FinalizeHeaderResult(TransferContext& context, HttpResponse& response) {
    context.FinalizeCurrentBlock();
    if (!response.headerHistory.empty()) {
        const auto& finalBlock = response.headerHistory.back();
        response.headers = finalBlock.headers;
        response.final_raw_headers = finalBlock.rawHeaders;
		// 从最终响应 Header 中解析服务器 Date。
        // Header 不存在或格式非法时保持 std::nullopt。
        response.date = ParseHttpDate(response.headers.GetHeaderValue("Date"));
        if (response.code == 0 && finalBlock.statusCode != 0) {
            response.code = finalBlock.statusCode;
        }
        // Cookie 只从最终常规响应 Header 提取；Trailer 中不应携带 Set-Cookie。
        response.cookies.ParseFromSetCookieHeaders(
            finalBlock.headers.GetHeaderValues("Set-Cookie"));
        for (const auto& trailer : response.trailers.Items()) {
            response.headers.AddHeader(trailer.name, trailer.value);
        }
    }
}

void CollectInfo(CURL* curl, HttpResponse& response) {
    long code = 0;
    if (curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code) == CURLE_OK) {
        response.code = code;
    }

    long proxyConnectCode = 0;
    if (curl_easy_getinfo(curl, CURLINFO_HTTP_CONNECTCODE, &proxyConnectCode) == CURLE_OK) {
        response.proxyConnectCode = proxyConnectCode;
    }

    char* text = nullptr;
    if (curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &text) == CURLE_OK && text) {
        response.effectiveUrl = text;
    }
    text = nullptr;
    if (curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &text) == CURLE_OK && text) {
        response.contentType = text;
    }
    text = nullptr;
    if (curl_easy_getinfo(curl, CURLINFO_PRIMARY_IP, &text) == CURLE_OK && text) {
        response.primaryIp = text;
    }

    long redirects = 0;
    if (curl_easy_getinfo(curl, CURLINFO_REDIRECT_COUNT, &redirects) == CURLE_OK) {
        response.redirectCount = redirects;
    }

#if LIBCURL_VERSION_NUM >= 0x073200
    long httpVersion = 0;
    if (curl_easy_getinfo(curl, CURLINFO_HTTP_VERSION, &httpVersion) == CURLE_OK) {
        response.negotiatedHttpVersion = CurlHttpVersionName(httpVersion);
    }
#endif

#if LIBCURL_VERSION_NUM >= 0x073700
    curl_off_t sizeValue = 0;
    if (curl_easy_getinfo(curl, CURLINFO_SIZE_DOWNLOAD_T, &sizeValue) == CURLE_OK) {
        response.downloadedBytes = sizeValue;
    }
    sizeValue = 0;
    if (curl_easy_getinfo(curl, CURLINFO_SIZE_UPLOAD_T, &sizeValue) == CURLE_OK) {
        response.uploadedBytes = sizeValue;
    }
#endif

#if LIBCURL_VERSION_NUM >= 0x073D00
    curl_off_t timeValue = 0;
    if (curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME_T, &timeValue) == CURLE_OK) {
        response.totalTime = std::chrono::microseconds(timeValue);
    }
    timeValue = 0;
    if (curl_easy_getinfo(curl, CURLINFO_NAMELOOKUP_TIME_T, &timeValue) == CURLE_OK) {
        response.nameLookupTime = std::chrono::microseconds(timeValue);
    }
    timeValue = 0;
    if (curl_easy_getinfo(curl, CURLINFO_CONNECT_TIME_T, &timeValue) == CURLE_OK) {
        response.connectTime = std::chrono::microseconds(timeValue);
    }
    timeValue = 0;
    if (curl_easy_getinfo(curl, CURLINFO_APPCONNECT_TIME_T, &timeValue) == CURLE_OK) {
        response.tlsHandshakeTime = std::chrono::microseconds(timeValue);
    }
    timeValue = 0;
    if (curl_easy_getinfo(curl, CURLINFO_STARTTRANSFER_TIME_T, &timeValue) == CURLE_OK) {
        response.firstByteTime = std::chrono::microseconds(timeValue);
    }
#else
    double seconds = 0.0;
    if (curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME, &seconds) == CURLE_OK) {
        response.totalTime = std::chrono::microseconds(static_cast<long long>(seconds * 1000000.0));
    }
    if (curl_easy_getinfo(curl, CURLINFO_NAMELOOKUP_TIME, &seconds) == CURLE_OK) {
        response.nameLookupTime = std::chrono::microseconds(static_cast<long long>(seconds * 1000000.0));
    }
    if (curl_easy_getinfo(curl, CURLINFO_CONNECT_TIME, &seconds) == CURLE_OK) {
        response.connectTime = std::chrono::microseconds(static_cast<long long>(seconds * 1000000.0));
    }
    if (curl_easy_getinfo(curl, CURLINFO_APPCONNECT_TIME, &seconds) == CURLE_OK) {
        response.tlsHandshakeTime = std::chrono::microseconds(static_cast<long long>(seconds * 1000000.0));
    }
    if (curl_easy_getinfo(curl, CURLINFO_STARTTRANSFER_TIME, &seconds) == CURLE_OK) {
        response.firstByteTime = std::chrono::microseconds(static_cast<long long>(seconds * 1000000.0));
    }
#endif
}

bool ShouldRetry(std::string_view method,
                 const RetryPolicy& policy,
                 HttpResponse& response,
                 int retryIndex) {
    if (retryIndex >= policy.maxRetries) {
        return false;
    }

    if (policy.customShouldRetry) {
        try {
            const auto decision = policy.customShouldRetry(response.curl_code,
                                                           response.code,
                                                           retryIndex);
            if (decision.has_value()) {
                return *decision;
            }
        } catch (const std::exception& ex) {
            response.success = false;
            response.curl_code = CURLE_ABORTED_BY_CALLBACK;
            response.errorCategory = HttpErrorCategory::Internal;
            response.error = std::string("自定义重试判定抛出异常: ") + ex.what();
            return false;
        } catch (...) {
            response.success = false;
            response.curl_code = CURLE_ABORTED_BY_CALLBACK;
            response.errorCategory = HttpErrorCategory::Internal;
            response.error = "自定义重试判定抛出未知异常";
            return false;
        }
    }

    const bool idempotent = IsIdempotentMethod(method);

    if (response.curl_code != CURLE_OK) {
        // CURLOPT_FAILONERROR 会把 HTTP >= 400 转换为 CURLE_HTTP_RETURNED_ERROR，
        // 这里仍允许基于状态码的重试策略继续判断。
        if (response.curl_code == CURLE_HTTP_RETURNED_ERROR &&
            ContainsStatus(policy.retryHttpStatusCodes, response.code)) {
            return idempotent || policy.retryNonIdempotent;
        }

        if (!ContainsCode(policy.retryCurlCodes, response.curl_code)) {
            return false;
        }
        if (idempotent || policy.retryNonIdempotent) {
            return true;
        }
        return IsSafePreTransferRetry(response.curl_code);
    }

    if (!ContainsStatus(policy.retryHttpStatusCodes, response.code)) {
        return false;
    }

    return idempotent || policy.retryNonIdempotent;
}

bool PrepareExternalResponseSinkForRetry(const RequestOptions& options,
                                         const TransferContext& context,
                                         HttpResponse& response) {
    const bool hasExternalSink = static_cast<bool>(options.responseChunkCallback) ||
                                 options.responseStream != nullptr;
    if (!hasExternalSink || !context.externalSinkTouched) {
        return true;
    }
    if (!options.responseRetryResetCallback) {
        if (!response.error.empty()) response.error += "; ";
        response.error += "已向外部响应接收器交付数据，为避免自动重试产生重复数据，本次不再重试";
        return false;
    }
    try {
        if (options.responseRetryResetCallback()) {
            return true;
        }
        if (!response.error.empty()) response.error += "; ";
        response.error += "外部响应接收器在重试前重置失败";
        return false;
    } catch (const std::exception& ex) {
        if (!response.error.empty()) response.error += "; ";
        response.error += std::string("外部响应接收器重置回调抛出异常: ") + ex.what();
        return false;
    } catch (...) {
        if (!response.error.empty()) response.error += "; ";
        response.error += "外部响应接收器重置回调抛出未知异常";
        return false;
    }
}

bool PrepareExternalSinkForLogicalReplay(const RequestOptions& options,
                                         HttpResponse& response) {
    const bool hasExternalSink = static_cast<bool>(options.responseChunkCallback) ||
                                 options.responseStream != nullptr;
    if (!hasExternalSink) return true;
    if (!options.responseRetryResetCallback) {
        if (!response.error.empty()) response.error += "; ";
        response.error += "请求需要认证重放，但配置了外部响应接收器且没有 responseRetryResetCallback，为避免重复数据已取消自动重放";
        return false;
    }
    try {
        if (options.responseRetryResetCallback()) return true;
        if (!response.error.empty()) response.error += "; ";
        response.error += "认证重放前外部响应接收器重置失败";
    } catch (const std::exception& ex) {
        if (!response.error.empty()) response.error += "; ";
        response.error += std::string("认证重放前外部响应接收器重置回调抛出异常: ") + ex.what();
    } catch (...) {
        if (!response.error.empty()) response.error += "; ";
        response.error += "认证重放前外部响应接收器重置回调抛出未知异常";
    }
    return false;
}

bool RewindUploadForLogicalReplay(const RequestOptions& options,
                                  HttpResponse& response) {
    if (!options.uploadSource) return true;
    try {
        if (options.uploadSource->Rewind()) return true;
        response.error = "认证重放前流式上传源无法回退";
    } catch (const std::exception& ex) {
        response.error = std::string("认证重放前流式上传源回退时抛出异常: ") + ex.what();
    } catch (...) {
        response.error = "认证重放前流式上传源回退时抛出未知异常";
    }
    response.success = false;
    response.curl_code = CURLE_SEND_FAIL_REWIND;
    response.errorCategory = HttpErrorCategory::Upload;
    return false;
}

HttpResponse PerformOnHandle(CURL* curl,
                             std::string_view method,
                             std::string_view url,
                             std::string_view body,
                             const RequestOptions& options) {
    HttpResponse response;
    if (!curl) {
        response.success = false;
        response.curl_code = CURLE_FAILED_INIT;
        response.error = "CURL handle is null";
        return response;
    }

    curl_easy_reset(curl);

    TransferContext context;
    context.response = &response;
    context.options = &options;

    CurlSlistHolder headerList;
    CurlSlistHolder resolveList;
    CurlMimeHolder mime;
    std::array<char, CURL_ERROR_SIZE> errorBuffer{};

    if (!ConfigureRequest(curl,
                          method,
                          url,
                          body,
                          options,
                          context,
                          headerList,
                          resolveList,
                          mime,
                          errorBuffer,
                          response)) {
        return response;
    }

    const int maxRetries = (std::max)(options.retry.maxRetries, 0);
    for (int retryIndex = 0; retryIndex <= maxRetries; ++retryIndex) {
        response.attempts = retryIndex + 1;
        context.ResetForAttempt();
        errorBuffer.fill('\0');

        if (options.cancelFlag &&
            options.cancelFlag->load(std::memory_order_relaxed)) {
            response.success = false;
            response.curl_code = CURLE_ABORTED_BY_CALLBACK;
            response.error = "request cancelled";
            break;
        }

        // 重试时强制建立新连接；下一次逻辑请求开始时 curl_easy_reset 会清除此选项，
        // 同时仍保留客户端的连接、Cookie、DNS 与 TLS Session 缓存。
        const CURLcode freshCode = curl_easy_setopt(curl, CURLOPT_FRESH_CONNECT, retryIndex > 0 ? 1L : 0L);
        if (freshCode != CURLE_OK) {
            response.success = false;
            response.curl_code = freshCode;
            response.errorCategory = HttpErrorCategory::Internal;
            response.error = std::string("设置 CURLOPT_FRESH_CONNECT 失败: ") + curl_easy_strerror(freshCode);
            break;
        }

        if (retryIndex > 0 && options.uploadSource) {
            try {
                if (!options.uploadSource->Rewind()) {
                    response.success = false;
                    response.curl_code = CURLE_SEND_FAIL_REWIND;
                    response.error = "流式上传源无法回退，不能继续重试";
                    break;
                }
            } catch (const std::exception& ex) {
                response.success = false;
                response.curl_code = CURLE_SEND_FAIL_REWIND;
                response.error = std::string("流式上传源回退时抛出异常: ") + ex.what();
                break;
            } catch (...) {
                response.success = false;
                response.curl_code = CURLE_SEND_FAIL_REWIND;
                response.error = "流式上传源回退时抛出未知异常";
                break;
            }
        }

        const CURLcode result = curl_easy_perform(curl);
        response.curl_code = result;
        response.success = (result == CURLE_OK);

        CollectInfo(curl, response);
        FinalizeHeaderResult(context, response);
        ApplyRedirectSafetyRefusal(curl, method, body, options, response);
        if (result != CURLE_OK && response.proxyConnectCode != 0 && response.headerHistory.empty()) {
            // 只有代理 CONNECT 成功而目标服务器尚未返回 HTTP 响应时，code 不冒充目标状态码。
            response.code = 0;
        }

        if (response.curl_code != CURLE_OK) {
            if (!context.callbackError.empty()) {
                response.error = context.callbackError;
                if (context.callbackErrorCategory != HttpErrorCategory::None) {
                    response.errorCategory = context.callbackErrorCategory;
                }
            } else if (errorBuffer[0] != '\0') {
                response.error = errorBuffer.data();
            } else if (response.error.empty()) {
                response.error = curl_easy_strerror(response.curl_code);
            }
        } else {
            response.error.clear();
        }

        if (!ShouldRetry(method, options.retry, response, retryIndex)) {
            break;
        }
        if (!PrepareExternalResponseSinkForRetry(options, context, response)) {
            break;
        }

        const auto delay = ComputeRetryDelay(options.retry, retryIndex + 1, response);
        if (!SleepCancelable(delay, options.cancelFlag)) {
            response.success = false;
            response.curl_code = CURLE_ABORTED_BY_CALLBACK;
            response.error = "request cancelled during retry delay";
            break;
        }
    }

    return response;
}

HttpResponse DownloadWithInvoker(const std::string& filePath,
                                  RequestOptions options,
                                  const std::function<HttpResponse(const RequestOptions&)>& invoker) {
    const curl_off_t resumeOffset = options.resumeFrom.value_or(0);
    if (resumeOffset < 0) {
        HttpResponse response;
        response.curl_code = CURLE_BAD_FUNCTION_ARGUMENT;
        response.error = "下载断点续传偏移不能为负数";
        response.errorCategory = HttpErrorCategory::InvalidArgument;
        return response;
    }

    if (resumeOffset > 0) {
        std::ifstream existing(filePath, std::ios::binary | std::ios::ate);
        if (!existing) {
            HttpResponse response;
            response.curl_code = CURLE_BAD_FUNCTION_ARGUMENT;
            response.error = "断点续传要求本地文件已存在: " + filePath;
            response.errorCategory = HttpErrorCategory::InvalidArgument;
            return response;
        }
        const std::streamoff existingSize = existing.tellg();
        if (existingSize < 0 || static_cast<std::uintmax_t>(existingSize) != static_cast<std::uintmax_t>(resumeOffset)) {
            HttpResponse response;
            response.curl_code = CURLE_BAD_FUNCTION_ARGUMENT;
            response.error = "本地文件大小与 resumeFrom 不一致，拒绝继续写入";
            response.errorCategory = HttpErrorCategory::InvalidArgument;
            return response;
        }
    }

    const bool resumeMode = resumeOffset > 0;
    const auto makeFileSuffix = [] {
        static std::atomic_uint64_t counter{1};
        const auto ticks = std::chrono::steady_clock::now().time_since_epoch().count();
        const auto id = counter.fetch_add(1, std::memory_order_relaxed);
        std::ostringstream out;
        out << std::hex << ticks << '-' << id;
        return out.str();
    };
    const std::string workingPath = resumeMode
        ? filePath
        : filePath + ".curl_ex_tmp_" + makeFileSuffix();

    std::ofstream file;
    const auto openFile = [&]() -> bool {
        file.close();
        file.clear();
        file.open(workingPath,
                  resumeMode ? (std::ios::binary | std::ios::app)
                             : (std::ios::binary | std::ios::trunc));
        return static_cast<bool>(file);
    };
    if (!openFile()) {
        HttpResponse response;
        response.curl_code = CURLE_WRITE_ERROR;
        response.error = "无法打开下载临时文件: " + workingPath;
        response.errorCategory = HttpErrorCategory::Download;
        return response;
    }

    const auto userChunkCallback = options.responseChunkCallback;
    const auto userResetCallback = options.responseRetryResetCallback;
    const bool hasUserExternalSink = static_cast<bool>(userChunkCallback) || options.responseStream != nullptr;
    std::size_t callbackBytes = 0;
    options.storeResponseBody = false;
    options.responseChunkCallback = [&file, &callbackBytes, userChunkCallback](const char* data, std::size_t size) {
        file.write(data, static_cast<std::streamsize>(size));
        if (!file) return false;
        callbackBytes += size;
        return !userChunkCallback || userChunkCallback(data, size);
    };
    options.responseRetryResetCallback = [&file, &callbackBytes, workingPath, resumeOffset,
                                          userResetCallback, hasUserExternalSink, resumeMode]() {
        try {
            file.flush();
            file.close();
            const std::uintmax_t rollbackSize = resumeMode
                ? static_cast<std::uintmax_t>(resumeOffset)
                : std::uintmax_t{0};
            std::filesystem::resize_file(workingPath, rollbackSize);
            file.clear();
            file.open(workingPath,
                      resumeMode ? (std::ios::binary | std::ios::app)
                                 : (std::ios::binary | std::ios::trunc));
            callbackBytes = 0;
            if (!file) return false;
            if (hasUserExternalSink && !userResetCallback) return false;
            return !userResetCallback || userResetCallback();
        } catch (...) {
            return false;
        }
    };

    HttpResponse response = invoker(options);

    // 自定义 Transport 可能不执行 responseChunkCallback；此时以返回 content 作为兼容回退。
    if (callbackBytes == 0 && !response.content.empty()) {
        file.write(response.content.data(), static_cast<std::streamsize>(response.content.size()));
    }
    file.flush();
    const bool fileOk = static_cast<bool>(file);
    file.close();

    if (!fileOk && response.TransportOk()) {
        response.success = false;
        response.curl_code = CURLE_WRITE_ERROR;
        response.error = "写入下载文件失败: " + workingPath;
        response.errorCategory = HttpErrorCategory::Download;
    }

    if (!response.Ok() || !fileOk) {
        std::error_code ec;
        if (resumeMode) {
            std::filesystem::resize_file(filePath, static_cast<std::uintmax_t>(resumeOffset), ec);
            if (ec) {
                const std::string originalError = response.error;
                response.success = false;
                response.curl_code = CURLE_WRITE_ERROR;
                response.errorCategory = HttpErrorCategory::Download;
                response.error = "断点下载失败后回滚本地文件失败: " + ec.message();
                if (!originalError.empty()) {
                    response.error += "; 原请求错误: " + originalError;
                }
            }
        } else {
            std::filesystem::remove(workingPath, ec);
            if (ec) {
                if (!response.error.empty()) response.error += "; ";
                response.error += "清理失败的下载临时文件失败: " + ec.message();
                response.errorCategory = HttpErrorCategory::Download;
            }
        }
        return response;
    }

    if (resumeMode) {
        return response;
    }

    // 临时文件与目标文件位于同一目录，以便最终提交尽量使用同文件系统的原子替换语义。
#ifdef _WIN32
    const std::filesystem::path sourcePath(workingPath);
    const std::filesystem::path targetPath(filePath);
    std::error_code existsEc;
    const bool targetExists = std::filesystem::exists(targetPath, existsEc);
    if (existsEc) {
        std::error_code removeEc;
        std::filesystem::remove(sourcePath, removeEc);
        response.success = false;
        response.curl_code = CURLE_WRITE_ERROR;
        response.errorCategory = HttpErrorCategory::Download;
        response.error = "下载完成但无法检查目标文件状态: " + filePath;
        return response;
    }

    BOOL committed = FALSE;
    if (targetExists) {
        // ReplaceFileW 的 REPLACEFILE_WRITE_THROUGH 标志在 Windows 文档中明确标注为不受支持；
        // 这里仅依赖 ReplaceFileW 的替换语义，不声明不存在的 write-through 保证。
        committed = ::ReplaceFileW(targetPath.c_str(),
                                   sourcePath.c_str(),
                                   nullptr,
                                   0,
                                   nullptr,
                                   nullptr);
    } else {
        committed = ::MoveFileExW(sourcePath.c_str(),
                                  targetPath.c_str(),
                                  MOVEFILE_WRITE_THROUGH);
    }
    if (!committed) {
        const DWORD winError = ::GetLastError();
        std::error_code removeEc;
        std::filesystem::remove(sourcePath, removeEc);
        response.success = false;
        response.curl_code = CURLE_WRITE_ERROR;
        response.errorCategory = HttpErrorCategory::Download;
        response.error = "下载完成但 Windows 原子提交目标文件失败，错误码=" +
                         std::to_string(static_cast<unsigned long>(winError));
        return response;
    }
    return response;
#else
    std::error_code ec;
    std::filesystem::rename(workingPath, filePath, ec);
    if (!ec) return response;

    std::error_code removeEc;
    std::filesystem::remove(workingPath, removeEc);
    response.success = false;
    response.curl_code = CURLE_WRITE_ERROR;
    response.errorCategory = HttpErrorCategory::Download;
    response.error = "下载完成但原子提交目标文件失败: " + filePath + "；" + ec.message();
    return response;
#endif
}

 } // 匿名命名空间

CURLcode Initialize() {
    return GlobalRuntime().Code();
}

// ============================================================
// URL 辅助功能
// ============================================================

std::string UrlEncode(std::string_view input) {
    if (input.empty()) {
        return {};
    }
    if (input.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)())) {
        throw std::length_error("UrlEncode input exceeds libcurl int length limit");
    }
    if (!EnsureGlobal()) {
        return {};
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        return {};
    }

    char* output = curl_easy_escape(curl,
                                    input.data(),
                                    static_cast<int>(input.size()));
    std::string result;
    if (output) {
        result = output;
        curl_free(output);
    }
    curl_easy_cleanup(curl);
    return result;
}

std::string UrlDecode(std::string_view input) {
    if (input.empty()) {
        return {};
    }
    if (input.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)())) {
        throw std::length_error("UrlDecode input exceeds libcurl int length limit");
    }
    if (!EnsureGlobal()) {
        return {};
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        return {};
    }

    int outputLength = 0;
    char* output = curl_easy_unescape(curl,
                                      input.data(),
                                      static_cast<int>(input.size()),
                                      &outputLength);
    std::string result;
    if (output) {
        result.assign(output, static_cast<std::size_t>(outputLength));
        curl_free(output);
    }
    curl_easy_cleanup(curl);
    return result;
}

UrlParams::UrlParams(std::string_view query) {
    Parse(query);
}

void UrlParams::Parse(std::string_view query) {
    params_.clear();
    if (!query.empty() && query.front() == '?') {
        query.remove_prefix(1);
    }

    std::size_t start = 0;
    while (start <= query.size()) {
        const std::size_t end = query.find('&', start);
        const std::size_t actualEnd = end == std::string_view::npos ? query.size() : end;
        const std::string_view pair = query.substr(start, actualEnd - start);

        if (!pair.empty()) {
            const auto equal = pair.find('=');
            UrlParamItem item;
            if (equal == std::string_view::npos) {
                item.key = UrlDecode(pair);
                item.value.clear();
                item.hasEquals = false;
            } else {
                item.key = UrlDecode(pair.substr(0, equal));
                item.value = UrlDecode(pair.substr(equal + 1));
                item.hasEquals = true;
            }
            params_.push_back(std::move(item));
        }

        if (end == std::string_view::npos) {
            break;
        }
        start = end + 1;
    }
}

std::string UrlParams::ToString(bool leadingQuestionMark) const {
    std::ostringstream out;
    if (leadingQuestionMark && !params_.empty()) {
        out << '?';
    }

    bool first = true;
    for (const auto& item : params_) {
        if (!first) {
            out << '&';
        }
        first = false;
        out << UrlEncode(item.key);
        if (item.hasEquals) {
            out << '=' << UrlEncode(item.value);
        }
    }
    return out.str();
}

std::string UrlParams::Get(const std::string& key) const {
    const auto it = std::find_if(params_.begin(), params_.end(), [&](const UrlParamItem& item) {
        return item.key == key;
    });
    return it == params_.end() ? std::string{} : it->value;
}

std::vector<std::string> UrlParams::GetAll(const std::string& key) const {
    std::vector<std::string> values;
    for (const auto& item : params_) {
        if (item.key == key) {
            values.push_back(item.value);
        }
    }
    return values;
}

void UrlParams::Set(const std::string& key, const std::string& value) {
    Remove(key);
    Add(key, value, true);
}

void UrlParams::Add(const std::string& key, const std::string& value, bool hasEquals) {
    params_.push_back(UrlParamItem{key, value, hasEquals});
}

void UrlParams::Remove(const std::string& key) {
    params_.erase(std::remove_if(params_.begin(), params_.end(), [&](const UrlParamItem& item) {
                      return item.key == key;
                  }),
                  params_.end());
}

bool UrlParams::Has(const std::string& key) const {
    return std::any_of(params_.begin(), params_.end(), [&](const UrlParamItem& item) {
        return item.key == key;
    });
}

std::size_t UrlParams::Size() const noexcept {
    return params_.size();
}

bool UrlParams::Empty() const noexcept {
    return params_.empty();
}

void UrlParams::Clear() noexcept {
    params_.clear();
}

std::vector<std::string> UrlParams::GetAllParams() const {
    std::vector<std::string> result;
    result.reserve(params_.size());
    for (const auto& item : params_) {
        if (item.hasEquals) {
            result.push_back(item.key + "=" + item.value);
        } else {
            result.push_back(item.key);
        }
    }
    return result;
}

std::vector<std::string> UrlParams::GetAllParamNames() const {
    std::vector<std::string> names;
    names.reserve(params_.size());
    for (const auto& item : params_) {
        if (std::find(names.begin(), names.end(), item.key) == names.end()) {
            names.push_back(item.key);
        }
    }
    return names;
}

std::string& UrlParams::operator[](const std::string& key) {
    auto it = std::find_if(params_.begin(), params_.end(), [&](const UrlParamItem& item) {
        return item.key == key;
    });
    if (it == params_.end()) {
        params_.push_back(UrlParamItem{key, {}, true});
        return params_.back().value;
    }
    it->hasEquals = true;
    return it->value;
}

// ============================================================
// HTTP 头部处理
// ============================================================

HttpHeadersWrapper::HttpHeadersWrapper(std::string_view rawHeaders) {
    ParseHeaders(rawHeaders);
}

std::string HttpHeadersWrapper::Trim(std::string_view str) {
    return TrimCopy(str);
}

bool HttpHeadersWrapper::EqualsIgnoreCase(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

bool HttpHeadersWrapper::IsSafeHeaderName(std::string_view value) noexcept {
    return IsHttpToken(value);
}

bool HttpHeadersWrapper::IsSafeHeaderValue(std::string_view value) noexcept {
    return IsSafeHttpFieldValue(value);
}

void HttpHeadersWrapper::ParseHeaders(std::string_view rawHeaders) {
    headers_.clear();
    std::size_t start = 0;
    while (start < rawHeaders.size()) {
        std::size_t end = rawHeaders.find_first_of("\r\n", start);
        if (end == std::string_view::npos) {
            end = rawHeaders.size();
        }
        const std::string_view line = rawHeaders.substr(start, end - start);
        const auto colon = line.find(':');
        if (colon != std::string_view::npos) {
            AddHeader(Trim(line.substr(0, colon)), Trim(line.substr(colon + 1)));
        }

        if (end >= rawHeaders.size()) {
            break;
        }
        if (rawHeaders[end] == '\r' && end + 1 < rawHeaders.size() && rawHeaders[end + 1] == '\n') {
            start = end + 2;
        } else {
            start = end + 1;
        }
    }
}

bool HttpHeadersWrapper::AddHeader(const std::string& key, const std::string& value) {
    const std::string name = Trim(key);
    const std::string val = Trim(value);
    if (!IsSafeHeaderName(name) || !IsSafeHeaderValue(val)) {
        return false;
    }
    headers_.push_back({name, val});
    return true;
}

bool HttpHeadersWrapper::SetHeader(const std::string& key, const std::string& value) {
    const std::string name = Trim(key);
    const std::string val = Trim(value);
    if (!IsSafeHeaderName(name) || !IsSafeHeaderValue(val)) {
        return false;
    }
    EraseHeader(name);
    headers_.push_back({name, val});
    return true;
}

bool HttpHeadersWrapper::SetDefaultHeader(const std::string& key, const std::string& value) {
    if (IsExist(key)) {
        return false;
    }
    return SetHeader(key, value);
}

bool HttpHeadersWrapper::AppendHeader(const std::string& key,
                                      const std::string& value,
                                      const std::string& delimiter) {
    const std::string name = Trim(key);
    const std::string val = Trim(value);
    if (!IsSafeHeaderName(name) || !IsSafeHeaderValue(val) || !IsSafeHttpFieldValue(delimiter)) {
        return false;
    }

    auto it = std::find_if(headers_.begin(), headers_.end(), [&](const HttpHeaderItem& item) {
        return EqualsIgnoreCase(item.name, name);
    });
    if (it == headers_.end()) {
        headers_.push_back({name, val});
    } else {
        it->value += delimiter;
        it->value += val;
    }
    return true;
}

bool HttpHeadersWrapper::EraseHeader(const std::string& key) {
    const std::string name = Trim(key);
    const auto oldSize = headers_.size();
    headers_.erase(std::remove_if(headers_.begin(), headers_.end(), [&](const HttpHeaderItem& item) {
                       return EqualsIgnoreCase(item.name, name);
                   }),
                   headers_.end());
    return headers_.size() != oldSize;
}

bool HttpHeadersWrapper::IsExist(const std::string& key) const {
    const std::string name = Trim(key);
    return std::any_of(headers_.begin(), headers_.end(), [&](const HttpHeaderItem& item) {
        return EqualsIgnoreCase(item.name, name);
    });
}

std::string HttpHeadersWrapper::GetHeaderValue(const std::string& key) const {
    const std::string name = Trim(key);
    const auto it = std::find_if(headers_.begin(), headers_.end(), [&](const HttpHeaderItem& item) {
        return EqualsIgnoreCase(item.name, name);
    });
    return it == headers_.end() ? std::string{} : it->value;
}

std::vector<std::string> HttpHeadersWrapper::GetHeaderValues(const std::string& key) const {
    const std::string name = Trim(key);
    std::vector<std::string> values;
    for (const auto& item : headers_) {
        if (EqualsIgnoreCase(item.name, name)) {
            values.push_back(item.value);
        }
    }
    return values;
}

std::vector<std::string> HttpHeadersWrapper::GetKeys(bool unique) const {
    std::vector<std::string> keys;
    for (const auto& item : headers_) {
        if (!unique) {
            keys.push_back(item.name);
            continue;
        }

        const bool alreadyPresent = std::any_of(keys.begin(), keys.end(), [&](const std::string& key) {
            return EqualsIgnoreCase(key, item.name);
        });
        if (!alreadyPresent) {
            keys.push_back(item.name);
        }
    }
    return keys;
}

std::string HttpHeadersWrapper::GetAllHeaders() const {
    std::ostringstream out;
    for (std::size_t i = 0; i < headers_.size(); ++i) {
        if (i != 0) {
            out << "\r\n";
        }
        out << headers_[i].name << ": " << headers_[i].value;
    }
    return out.str();
}

// ============================================================
// Cookie 处理
// ============================================================

std::string Cookie::ToSetCookieString() const {
    std::ostringstream out;
    out << name << '=' << value;
    if (path) out << "; Path=" << *path;
    if (domain) out << "; Domain=" << *domain;
    if (expires) out << "; Expires=" << *expires;
    if (maxAge) out << "; Max-Age=" << *maxAge;
    if (secure) out << "; Secure";
    if (httpOnly) out << "; HttpOnly";
    if (sameSite) out << "; SameSite=" << *sameSite;
    return out.str();
}

HttpCookiesWrapper::HttpCookiesWrapper(const std::vector<std::string>& setCookieHeaders) {
    ParseFromSetCookieHeaders(setCookieHeaders);
}

HttpCookiesWrapper::HttpCookiesWrapper(std::string_view requestCookieString) {
    ParseFromCookieString(requestCookieString);
}

std::string HttpCookiesWrapper::Trim(std::string_view str) {
    return TrimCopy(str);
}

bool HttpCookiesWrapper::SameIdentity(const Cookie& a, const Cookie& b) {
    const auto normalizeDomain = [](const std::optional<std::string>& domain) {
        if (!domain) return std::string{};
        std::string value = ToLowerCopy(TrimCopy(*domain));
        while (!value.empty() && value.front() == '.') value.erase(value.begin());
        return value;
    };
    return a.name == b.name &&
           normalizeDomain(a.domain) == normalizeDomain(b.domain) &&
           a.path == b.path;
}

void HttpCookiesWrapper::Merge(const HttpCookiesWrapper& other, bool overwrite) {
    for (const auto& cookie : other.cookies_) {
        auto it = std::find_if(cookies_.begin(), cookies_.end(), [&](const Cookie& existing) {
            return SameIdentity(existing, cookie);
        });
        if (it == cookies_.end()) {
            cookies_.push_back(cookie);
        } else if (overwrite) {
            *it = cookie;
        }
    }
}

HttpCookiesWrapper HttpCookiesWrapper::MergedWith(const HttpCookiesWrapper& other,
                                                  bool overwrite) const {
    HttpCookiesWrapper result = *this;
    result.Merge(other, overwrite);
    return result;
}

void HttpCookiesWrapper::ParseFromSetCookieHeaders(const std::vector<std::string>& setCookieHeaders) {
    cookies_.clear();

    for (const auto& header : setCookieHeaders) {
        Cookie cookie;
        bool first = true;
        std::size_t start = 0;

        while (start <= header.size()) {
            const std::size_t end = header.find(';', start);
            const std::size_t actualEnd = end == std::string::npos ? header.size() : end;
            const std::string segment = Trim(std::string_view(header).substr(start, actualEnd - start));

            if (!segment.empty()) {
                const auto equal = segment.find('=');
                const std::string key = Trim(std::string_view(segment).substr(0, equal));
                const std::string value = equal == std::string::npos
                                              ? std::string{}
                                              : Trim(std::string_view(segment).substr(equal + 1));

                if (first) {
                    if (equal != std::string::npos && !key.empty()) {
                        cookie.name = key;
                        cookie.value = value;
                    }
                    first = false;
                } else {
                    const std::string lower = ToLowerCopy(key);
                    if (lower == "path") cookie.path = value;
                    else if (lower == "domain") cookie.domain = value;
                    else if (lower == "expires") cookie.expires = value;
                    else if (lower == "max-age") {
                        try {
                            std::size_t consumed = 0;
                            const auto parsed = std::stoll(value, &consumed, 10);
                            if (consumed == value.size()) cookie.maxAge = parsed;
                        } catch (...) {
                        }
                    } else if (lower == "secure") cookie.secure = true;
                    else if (lower == "httponly") cookie.httpOnly = true;
                    else if (lower == "samesite") cookie.sameSite = value;
                }
            }

            if (end == std::string::npos) {
                break;
            }
            start = end + 1;
        }

        if (!cookie.name.empty()) {
            SetCookie(cookie);
        }
    }
}

void HttpCookiesWrapper::ParseFromCookieString(std::string_view cookieString) {
    cookies_.clear();
    std::size_t start = 0;
    while (start <= cookieString.size()) {
        const std::size_t end = cookieString.find(';', start);
        const std::size_t actualEnd = end == std::string_view::npos ? cookieString.size() : end;
        const std::string token = Trim(cookieString.substr(start, actualEnd - start));
        const auto equal = token.find('=');
        if (equal != std::string::npos) {
            const std::string name = Trim(std::string_view(token).substr(0, equal));
            const std::string value = Trim(std::string_view(token).substr(equal + 1));
            if (!name.empty()) {
                SetCookie(name, value);
            }
        }
        if (end == std::string_view::npos) {
            break;
        }
        start = end + 1;
    }
}

std::vector<std::string> HttpCookiesWrapper::ExtractSetCookieHeaders(std::string_view rawHeaders) {
    HttpHeadersWrapper parsed(rawHeaders);
    return parsed.GetHeaderValues("Set-Cookie");
}

void HttpCookiesWrapper::SetCookie(const Cookie& cookie) {
    if (cookie.name.empty()) {
        return;
    }
    auto it = std::find_if(cookies_.begin(), cookies_.end(), [&](const Cookie& existing) {
        return SameIdentity(existing, cookie);
    });
    if (it == cookies_.end()) {
        cookies_.push_back(cookie);
    } else {
        *it = cookie;
    }
}

void HttpCookiesWrapper::SetCookie(const std::string& name, const std::string& value) {
    const std::string trimmedName = Trim(name);
    if (trimmedName.empty()) {
        return;
    }
    Cookie cookie;
    cookie.name = trimmedName;
    cookie.value = Trim(value);
    SetCookie(cookie);
}

bool HttpCookiesWrapper::EraseCookie(const std::string& name) {
    const auto oldSize = cookies_.size();
    cookies_.erase(std::remove_if(cookies_.begin(), cookies_.end(), [&](const Cookie& cookie) {
                       return cookie.name == Trim(name);
                   }),
                   cookies_.end());
    return cookies_.size() != oldSize;
}

bool HttpCookiesWrapper::EraseCookie(const std::string& name,
                                     const std::optional<std::string>& domain,
                                     const std::optional<std::string>& path) {
    Cookie identity;
    identity.name = Trim(name);
    identity.path = path;
    identity.domain = domain;
    const auto oldSize = cookies_.size();
    cookies_.erase(std::remove_if(cookies_.begin(), cookies_.end(), [&](const Cookie& cookie) {
                       return SameIdentity(cookie, identity);
                   }),
                   cookies_.end());
    return cookies_.size() != oldSize;
}

bool HttpCookiesWrapper::IsExist(const std::string& name) const {
    const std::string key = Trim(name);
    return std::any_of(cookies_.begin(), cookies_.end(), [&](const Cookie& cookie) {
        return cookie.name == key;
    });
}

void HttpCookiesWrapper::RemoveEmptyCookies() {
    cookies_.erase(std::remove_if(cookies_.begin(), cookies_.end(), [](const Cookie& cookie) {
                       return cookie.value.empty();
                   }),
                   cookies_.end());
}

std::string HttpCookiesWrapper::GetCookieValue(const std::string& name) const {
    const std::string key = Trim(name);
    const auto it = std::find_if(cookies_.begin(), cookies_.end(), [&](const Cookie& cookie) {
        return cookie.name == key;
    });
    return it == cookies_.end() ? std::string{} : it->value;
}

std::vector<Cookie> HttpCookiesWrapper::GetCookies(const std::string& name) const {
    const std::string key = Trim(name);
    std::vector<Cookie> result;
    for (const auto& cookie : cookies_) {
        if (cookie.name == key) {
            result.push_back(cookie);
        }
    }
    return result;
}

std::vector<std::string> HttpCookiesWrapper::GetAllKeys(bool ignoreNull) const {
    std::vector<std::string> keys;
    for (const auto& cookie : cookies_) {
        if (ignoreNull && cookie.value.empty()) {
            continue;
        }
        if (std::find(keys.begin(), keys.end(), cookie.name) == keys.end()) {
            keys.push_back(cookie.name);
        }
    }
    return keys;
}

std::string HttpCookiesWrapper::ToRequestCookieString(bool ignoreNull) const {
    std::ostringstream out;
    bool first = true;
    for (const auto& cookie : cookies_) {
        if (ignoreNull && cookie.value.empty()) {
            continue;
        }
        if (!first) {
            out << "; ";
        }
        first = false;
        out << cookie.name << '=' << cookie.value;
    }
    return out.str();
}

std::vector<std::string> HttpCookiesWrapper::ToSetCookieHeaders(bool ignoreNull) const {
    std::vector<std::string> result;
    for (const auto& cookie : cookies_) {
        if (ignoreNull && cookie.value.empty()) {
            continue;
        }
        result.push_back("Set-Cookie: " + cookie.ToSetCookieString());
    }
    return result;
}

std::vector<Cookie> HttpCookiesWrapper::GetAllCookies(bool ignoreNull) const {
    std::vector<Cookie> result;
    for (const auto& cookie : cookies_) {
        if (ignoreNull && cookie.value.empty()) {
            continue;
        }
        result.push_back(cookie);
    }
    return result;
}

// ============================================================
// 配置类型
// ============================================================

std::string_view ToString(HttpMethod method) noexcept {
    switch (method) {
        case HttpMethod::Get: return "GET";
        case HttpMethod::Head: return "HEAD";
        case HttpMethod::Post: return "POST";
        case HttpMethod::Put: return "PUT";
        case HttpMethod::Delete: return "DELETE";
        case HttpMethod::Patch: return "PATCH";
        case HttpMethod::Options: return "OPTIONS";
        case HttpMethod::Trace: return "TRACE";
        case HttpMethod::Connect: return "CONNECT";
    }
    // 非法枚举值不能静默退化为 GET；返回空 token 让后续请求参数校验明确失败。
    return {};
}

RetryPolicy::RetryPolicy()
    : retryCurlCodes{
          CURLE_COULDNT_RESOLVE_PROXY,
          CURLE_COULDNT_RESOLVE_HOST,
          CURLE_COULDNT_CONNECT,
          CURLE_OPERATION_TIMEDOUT,
          CURLE_RECV_ERROR,
          CURLE_SEND_ERROR,
#if LIBCURL_VERSION_NUM >= 0x074900
          CURLE_PROXY,
#endif
          CURLE_SSL_CONNECT_ERROR,
          CURLE_GOT_NOTHING,
          CURLE_PARTIAL_FILE},
      retryHttpStatusCodes{408, 425, 429, 500, 502, 503, 504} {}

MultipartPart MultipartPart::Field(std::string name, std::string value) {
    MultipartPart part;
    part.name = std::move(name);
    part.data = std::move(value);
    return part;
}

MultipartPart MultipartPart::File(std::string name,
                                  std::string filePath,
                                  std::string contentType,
                                  std::string fileName) {
    MultipartPart part;
    part.name = std::move(name);
    part.filePath = std::move(filePath);
    part.contentType = std::move(contentType);
    part.fileName = std::move(fileName);
    return part;
}

// ============================================================
// 流式上传源
// ============================================================

MemoryUploadSource::MemoryUploadSource(std::string data)
    : data_(std::move(data)) {}

std::size_t MemoryUploadSource::Read(char* buffer, std::size_t capacity) {
    if (!buffer || capacity == 0 || offset_ >= data_.size()) return 0;
    const std::size_t bytes = (std::min)(capacity, data_.size() - offset_);
    std::memcpy(buffer, data_.data() + offset_, bytes);
    offset_ += bytes;
    return bytes;
}

bool MemoryUploadSource::Rewind() {
    offset_ = 0;
    return true;
}

std::optional<curl_off_t> MemoryUploadSource::Size() const {
    return static_cast<curl_off_t>(data_.size());
}

CallbackUploadSource::CallbackUploadSource(ReadCallback readCallback,
                                           RewindCallback rewindCallback,
                                           std::optional<curl_off_t> size)
    : read_(std::move(readCallback)),
      rewind_(std::move(rewindCallback)),
      size_(size) {}

std::size_t CallbackUploadSource::Read(char* buffer, std::size_t capacity) {
    return read_ ? read_(buffer, capacity) : 0;
}

bool CallbackUploadSource::Rewind() {
    return rewind_ ? rewind_() : false;
}

std::optional<curl_off_t> CallbackUploadSource::Size() const {
    return size_;
}

struct FileUploadSource::Impl {
    std::ifstream stream;
    std::optional<curl_off_t> size;
};

FileUploadSource::FileUploadSource(std::string filePath)
    : impl_(std::make_unique<Impl>()), filePath_(std::move(filePath)) {
    impl_->stream.open(filePath_, std::ios::binary);
    if (!impl_->stream) return;
    impl_->stream.seekg(0, std::ios::end);
    const auto end = impl_->stream.tellg();
    if (end >= 0) impl_->size = static_cast<curl_off_t>(end);
    impl_->stream.clear();
    impl_->stream.seekg(0, std::ios::beg);
}

FileUploadSource::~FileUploadSource() = default;

std::size_t FileUploadSource::Read(char* buffer, std::size_t capacity) {
    if (!impl_ || !impl_->stream || !buffer || capacity == 0) return 0;
    impl_->stream.read(buffer, static_cast<std::streamsize>(capacity));
    return static_cast<std::size_t>(impl_->stream.gcount());
}

bool FileUploadSource::Rewind() {
    if (!impl_ || !impl_->stream.is_open()) return false;
    impl_->stream.clear();
    impl_->stream.seekg(0, std::ios::beg);
    return static_cast<bool>(impl_->stream);
}

std::optional<curl_off_t> FileUploadSource::Size() const {
    return impl_ ? impl_->size : std::nullopt;
}

bool FileUploadSource::Valid() const noexcept {
    return impl_ && impl_->stream.is_open();
}

namespace {

std::string GenerateRequestId() {
    static std::atomic_uint64_t counter{1};
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto id = counter.fetch_add(1, std::memory_order_relaxed);
    std::ostringstream out;
    out << std::hex << now << '-' << id;
    return out.str();
}

HttpErrorCategory ClassifyError(const HttpResponse& response) {
    if (response.errorCategory != HttpErrorCategory::None) return response.errorCategory;
    if (response.proxyConnectCode >= 400 && response.code == 0) return HttpErrorCategory::Proxy;
    if (response.curl_code == CURLE_OK) {
        return (response.code >= 400) ? HttpErrorCategory::Http : HttpErrorCategory::None;
    }
    switch (response.curl_code) {
        case CURLE_ABORTED_BY_CALLBACK: return HttpErrorCategory::Cancelled;
        case CURLE_COULDNT_RESOLVE_HOST: return HttpErrorCategory::Dns;
        case CURLE_COULDNT_CONNECT: return HttpErrorCategory::Connect;
#if LIBCURL_VERSION_NUM >= 0x074900
        case CURLE_PROXY:
#endif
        case CURLE_COULDNT_RESOLVE_PROXY: return HttpErrorCategory::Proxy;
        case CURLE_SSL_CONNECT_ERROR:
        case CURLE_PEER_FAILED_VERIFICATION:
        case CURLE_SSL_CERTPROBLEM:
        case CURLE_SSL_CIPHER:
        case CURLE_SSL_CACERT_BADFILE:
        case CURLE_SSL_CRL_BADFILE:
        case CURLE_SSL_ISSUER_ERROR:
        case CURLE_SSL_PINNEDPUBKEYNOTMATCH:
            return HttpErrorCategory::Tls;
        case CURLE_OPERATION_TIMEDOUT: return HttpErrorCategory::Timeout;
        case CURLE_READ_ERROR:
        case CURLE_SEND_ERROR:
        case CURLE_SEND_FAIL_REWIND: return HttpErrorCategory::Upload;
        case CURLE_WRITE_ERROR:
        case CURLE_RECV_ERROR:
        case CURLE_PARTIAL_FILE:
        case CURLE_FILESIZE_EXCEEDED:
        case CURLE_RANGE_ERROR:
        case CURLE_GOT_NOTHING:
        case CURLE_BAD_CONTENT_ENCODING:
            return HttpErrorCategory::Download;
        case CURLE_HTTP_RETURNED_ERROR:
        case CURLE_TOO_MANY_REDIRECTS: return HttpErrorCategory::Http;
        case CURLE_BAD_FUNCTION_ARGUMENT:
        case CURLE_URL_MALFORMAT:
        case CURLE_UNSUPPORTED_PROTOCOL: return HttpErrorCategory::InvalidArgument;
        default: return HttpErrorCategory::Internal;
    }
}

std::string RedactHeaderValue(const DebugOptions& debug,
                              std::string_view name,
                              std::string_view value) {
    const std::string lower = ToLowerCopy(name);

    if (debug.hideSensitiveHeaders) {
        // 必须覆盖所有封装层默认认定为凭据/秘密的常见 Header，避免
        // “重定向保护认为敏感，但 Debug 仍明文打印”的安全策略漂移。
        static const std::array<std::string_view, 9> builtins{
            "authorization", "proxy-authorization", "cookie", "set-cookie",
            "x-api-key", "api-key", "x-auth-token", "x-access-token", "x-secret"};
        for (auto item : builtins) {
            if (lower == item) return "***";
        }
        for (const auto& item : debug.sensitiveHeaders) {
            if (lower == ToLowerCopy(TrimCopy(item))) return "***";
        }
    }

    // URL 数据和 Header 整体敏感性是两个独立开关。Location/Referer/Link 等字段
    // 可能携带 userinfo、access_token 或业务自定义 Query 凭据，即使调用方关闭了
    // hideSensitiveHeaders，只要 hideSensitiveUrlData 仍开启就继续执行 URL 脱敏。
    if (debug.hideSensitiveUrlData) {
        // Link 可以在一个字段值中携带多个 URI-reference；单 URL 脱敏器无法可靠覆盖
        // 逗号分隔的后续链接。安全模式下宁可牺牲该字段的诊断细节，也不能漏出第二个 URL 的凭据。
        if (lower == "link") return "***";
        static const std::array<std::string_view, 5> urlBearingHeaders{
            "location", "content-location", "referer", "destination", "refresh"};
        for (auto item : urlBearingHeaders) {
            if (lower == item) return RedactUrlForDebug(debug, value);
        }
    }

    return std::string(value);
}

std::string RedactUrlForDebug(const DebugOptions& debug, std::string_view input) {
    if (!debug.hideSensitiveUrlData) return std::string(input);

    std::string url(input);

    // 脱敏 scheme://userinfo@host 中的 userinfo，避免用户名或密码进入日志。
    const std::size_t scheme = url.find("://");
    if (scheme != std::string::npos) {
        const std::size_t authorityStart = scheme + 3;
        const std::size_t authorityEnd = url.find_first_of("/?#", authorityStart);
        const std::size_t at = url.find('@', authorityStart);
        if (at != std::string::npos &&
            (authorityEnd == std::string::npos || at < authorityEnd)) {
            url.replace(authorityStart, at - authorityStart, "***");
        }
    }

    static const std::array<std::string_view, 10> builtins{
        "access_token", "token", "api_key", "apikey", "key",
        "password", "passwd", "secret", "client_secret", "auth"};

    const auto isSensitive = [&](std::string_view encodedName) {
        const std::string decoded = ToLowerCopy(UrlDecode(encodedName));
        for (const auto item : builtins) {
            if (decoded == item) return true;
        }
        for (const auto& item : debug.sensitiveQueryParameters) {
            if (decoded == ToLowerCopy(TrimCopy(item))) return true;
        }
        return false;
    };

    const auto redactPairs = [&](std::string_view text) {
        std::string output;
        output.reserve(text.size());
        std::size_t start = 0;
        bool first = true;
        while (start <= text.size()) {
            const std::size_t amp = text.find('&', start);
            const std::size_t end = amp == std::string_view::npos ? text.size() : amp;
            const std::string_view item = text.substr(start, end - start);
            if (!first) output.push_back('&');
            first = false;

            const std::size_t equal = item.find('=');
            if (equal != std::string_view::npos && isSensitive(item.substr(0, equal))) {
                output.append(item.data(), equal + 1);
                output += "***";
            } else {
                output.append(item.data(), item.size());
            }

            if (amp == std::string_view::npos) break;
            start = amp + 1;
        }
        return output;
    };

    const std::size_t fragment = url.find('#');
    const std::size_t query = url.find('?');
    const bool hasQuery = query != std::string::npos &&
                          (fragment == std::string::npos || query < fragment);
    const std::size_t mainEnd = hasQuery ? query :
                                (fragment == std::string::npos ? url.size() : fragment);

    std::string output;
    output.reserve(url.size());
    output.append(url.data(), mainEnd);

    if (hasQuery) {
        const std::size_t queryEnd = fragment == std::string::npos ? url.size() : fragment;
        output.push_back('?');
        output += redactPairs(std::string_view(url).substr(query + 1, queryEnd - query - 1));
    }

    if (fragment != std::string::npos) {
        output.push_back('#');
        const std::string_view fragmentText(url.data() + fragment + 1,
                                            url.size() - fragment - 1);
        // Fragment 可能直接使用 key=value，也可能是 SPA 路由后再跟 ?key=value。
        const std::size_t fragmentQuery = fragmentText.find('?');
        if (fragmentQuery != std::string_view::npos) {
            output.append(fragmentText.data(), fragmentQuery + 1);
            output += redactPairs(fragmentText.substr(fragmentQuery + 1));
        } else if (fragmentText.find('=') != std::string_view::npos) {
            output += redactPairs(fragmentText);
        } else {
            output.append(fragmentText.data(), fragmentText.size());
        }
    }

    return output;
}

void EmitDebug(const DebugOptions& debug, const std::string& text) noexcept {
    if (!debug.enabled) return;
    try {
        if (debug.logger) debug.logger(text);
        else std::clog << text << std::endl;
    } catch (...) {
        // 调试日志属于旁路能力，日志回调异常不能改变网络请求结果。
    }
}

void LogRequestDebug(const HttpRequestData& request, const RequestOptions& options) noexcept {
    if (!options.debug.enabled) return;
    try {
        std::ostringstream out;
        out << "===== HTTP REQUEST " << request.requestId << " =====\n";
        out << request.method << ' ' << RedactUrlForDebug(options.debug, request.url) << '\n';
        if (options.debug.logRequestHeaders) {
            for (const auto& h : options.headers.Items()) {
                out << h.name << ": " << RedactHeaderValue(options.debug, h.name, h.value) << '\n';
            }
        }
        if (options.debug.logRequestBody && !request.body.empty()) {
            const std::size_t n = (std::min)(request.body.size(), options.debug.maxBodyLogBytes);
            out << "\n" << request.body.substr(0, n);
            if (n < request.body.size()) out << "\n...[Body 已截断]";
            out << '\n';
        }
        EmitDebug(options.debug, out.str());
    } catch (...) {
        // Debug 是旁路能力；格式化、脱敏或内存分配失败都不能改变请求控制流。
    }
}

void LogResponseDebug(const HttpRequestData& request,
                      const RequestOptions& options,
                      const HttpResponse& response) noexcept {
    if (!options.debug.enabled) return;
    try {
        std::ostringstream out;
        out << "===== HTTP RESPONSE " << request.requestId << " =====\n";
        out << "HTTP " << response.code << " CURL " << static_cast<int>(response.curl_code) << '\n';
        if (options.debug.logResponseHeaders) {
            for (const auto& h : response.headers.Items()) {
                out << h.name << ": " << RedactHeaderValue(options.debug, h.name, h.value) << '\n';
            }
        }
        if (options.debug.logResponseBody && !response.content.empty()) {
            const std::size_t n = (std::min)(response.content.size(), options.debug.maxBodyLogBytes);
            out << "\n" << response.content.substr(0, n);
            if (n < response.content.size()) out << "\n...[Body 已截断]";
            out << '\n';
        }
        out << "总耗时(us): " << response.totalTime.count()
            << " DNS(us): " << response.nameLookupTime.count()
            << " Connect(us): " << response.connectTime.count()
            << " TLS(us): " << response.tlsHandshakeTime.count()
            << " TTFB(us): " << response.firstByteTime.count()
            << " Attempts: " << response.attempts << '\n';
        EmitDebug(options.debug, out.str());
    } catch (...) {
        // 与请求日志相同，响应 Debug 的任何异常只能降级为少记日志，不能影响完成交付。
    }
}

HttpMetricsEvent MakeMetricsEvent(const HttpRequestData& request,
                                  const RequestOptions& options,
                                  const HttpResponse& response) {
    HttpMetricsEvent event;
    event.method = request.method;
    // Metrics 属于长期可观测数据，URL 脱敏独立于临时 Debug 明文开关，避免凭据进入监控存储。
    DebugOptions metricsDebug = options.debug;
    metricsDebug.hideSensitiveUrlData = true;
    event.url = RedactUrlForDebug(metricsDebug, request.url);
    event.statusCode = response.code;
    event.curlCode = response.curl_code;
    event.transportOk = response.TransportOk();
    event.attempts = response.attempts;
    event.downloadedBytes = response.downloadedBytes;
    event.uploadedBytes = response.uploadedBytes;
    event.totalTime = response.totalTime;
    return event;
}

void EmitMetricsSafely(const std::shared_ptr<IMetricsCollector>& collector,
                       const HttpRequestData& request,
                       const RequestOptions& options,
                       const HttpResponse& response) noexcept {
    if (!collector) return;
    try {
        collector->OnRequestCompleted(MakeMetricsEvent(request, options, response));
    } catch (...) {
        // Metrics 属于旁路可观测性能力，收集器异常不能改变业务请求结果。
    }
}

bool ValidateClientOptions(const HttpClientOptions& options, HttpResponse& response) {
    if (!std::isfinite(options.rateLimit.requestsPerSecond) || options.rateLimit.requestsPerSecond < 0.0) {
        response.curl_code = CURLE_BAD_FUNCTION_ARGUMENT;
        response.errorCategory = HttpErrorCategory::InvalidArgument;
        response.error = "requestsPerSecond 必须是有限的非负数";
        return false;
    }
    if (options.rateLimit.requestsPerSecond > 0.0 && options.rateLimit.burst == 0) {
        response.curl_code = CURLE_BAD_FUNCTION_ARGUMENT;
        response.errorCategory = HttpErrorCategory::InvalidArgument;
        response.error = "启用请求速率限制时 burst 不能为 0";
        return false;
    }
    if (options.circuitBreaker.failureThreshold > 0 && options.circuitBreaker.halfOpenMaxRequests == 0) {
        response.curl_code = CURLE_BAD_FUNCTION_ARGUMENT;
        response.errorCategory = HttpErrorCategory::InvalidArgument;
        response.error = "启用 Circuit Breaker 时 halfOpenMaxRequests 不能为 0";
        return false;
    }
    if (options.circuitBreaker.openDuration.count() < 0) {
        response.curl_code = CURLE_BAD_FUNCTION_ARGUMENT;
        response.errorCategory = HttpErrorCategory::InvalidArgument;
        response.error = "Circuit Breaker openDuration 不能为负数";
        return false;
    }
    if (options.maxTotalConnections < 0 || options.maxHostConnections < 0 || options.maxConcurrentStreams < 0) {
        response.curl_code = CURLE_BAD_FUNCTION_ARGUMENT;
        response.errorCategory = HttpErrorCategory::InvalidArgument;
        response.error = "curl_multi 连接与并发流限制不能为负数";
        return false;
    }
    return true;
}

class RateLimiterState {
public:
    explicit RateLimiterState(RateLimitPolicy policy)
        : policy_(std::move(policy)),
          tokens_(static_cast<double>((std::max)(policy_.burst, std::size_t{1}))),
          lastRefill_(std::chrono::steady_clock::now()) {}

    bool Acquire(std::atomic_bool* cancelFlag) {
        std::unique_lock<std::mutex> lock(mutex_);
        for (;;) {
            if (cancelFlag && cancelFlag->load(std::memory_order_relaxed)) return false;
            RefillLocked();
            const bool concurrencyOk = policy_.maxConcurrent == 0 || active_ < policy_.maxConcurrent;
            const bool rateOk = policy_.requestsPerSecond <= 0.0 || tokens_ >= 1.0;
            if (concurrencyOk && rateOk) {
                if (policy_.requestsPerSecond > 0.0) tokens_ -= 1.0;
                ++active_;
                return true;
            }
            cv_.wait_for(lock, std::chrono::milliseconds(10));
        }
    }

    bool TryAcquire() {
        std::lock_guard<std::mutex> lock(mutex_);
        RefillLocked();
        const bool concurrencyOk = policy_.maxConcurrent == 0 || active_ < policy_.maxConcurrent;
        const bool rateOk = policy_.requestsPerSecond <= 0.0 || tokens_ >= 1.0;
        if (!concurrencyOk || !rateOk) return false;
        if (policy_.requestsPerSecond > 0.0) tokens_ -= 1.0;
        ++active_;
        return true;
    }

    void Release() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (active_ > 0) --active_;
        cv_.notify_all();
    }

private:
    void RefillLocked() {
        if (policy_.requestsPerSecond <= 0.0) return;
        const auto now = std::chrono::steady_clock::now();
        const double elapsed = std::chrono::duration<double>(now - lastRefill_).count();
        lastRefill_ = now;
        const double capacity = static_cast<double>((std::max)(policy_.burst, std::size_t{1}));
        tokens_ = (std::min)(capacity, tokens_ + elapsed * policy_.requestsPerSecond);
    }

    RateLimitPolicy policy_;
    std::mutex mutex_;
    std::condition_variable cv_;
    double tokens_ = 0.0;
    std::size_t active_ = 0;
    std::chrono::steady_clock::time_point lastRefill_;
};

class RateLease {
public:
    explicit RateLease(std::shared_ptr<RateLimiterState> limiter) : limiter_(std::move(limiter)) {}
    ~RateLease() { if (limiter_) limiter_->Release(); }
    RateLease(const RateLease&) = delete;
    RateLease& operator=(const RateLease&) = delete;
private:
    std::shared_ptr<RateLimiterState> limiter_;
};

class CircuitBreakerState {
public:
    explicit CircuitBreakerState(CircuitBreakerPolicy policy) : policy_(std::move(policy)) {}

    bool Allow(bool& halfOpenAdmission) {
        halfOpenAdmission = false;
        if (policy_.failureThreshold == 0) return true;
        std::lock_guard<std::mutex> lock(mutex_);
        const auto now = std::chrono::steady_clock::now();
        if (state_ == State::Open) {
            if (now - openedAt_ < policy_.openDuration) return false;
            state_ = State::HalfOpen;
            halfOpenInFlight_ = 0;
            halfOpenSuccesses_ = 0;
        }
        if (state_ == State::HalfOpen) {
            if (halfOpenInFlight_ >= (std::max)(policy_.halfOpenMaxRequests, std::size_t{1})) return false;
            ++halfOpenInFlight_;
            halfOpenAdmission = true;
        }
        return true;
    }

    void Cancel(bool halfOpenAdmission) {
        if (!halfOpenAdmission || policy_.failureThreshold == 0) return;
        std::lock_guard<std::mutex> lock(mutex_);
        if (state_ == State::HalfOpen && halfOpenInFlight_ > 0) {
            --halfOpenInFlight_;
        }
    }

    void Record(const HttpResponse& response, bool halfOpenAdmission) {
        if (policy_.failureThreshold == 0) return;

        const HttpErrorCategory category = ClassifyError(response);
        bool relevant = true;
        bool failed = false;
        switch (category) {
            case HttpErrorCategory::Dns:
            case HttpErrorCategory::Connect:
            case HttpErrorCategory::Proxy:
            case HttpErrorCategory::Tls:
            case HttpErrorCategory::Timeout:
                failed = true;
                break;
            case HttpErrorCategory::Upload:
                // 读取源/回退失败属于客户端本地问题；发送失败才反映远端或网络健康度。
                relevant = response.curl_code == CURLE_SEND_ERROR;
                failed = relevant;
                break;
            case HttpErrorCategory::Download:
                // 写文件/用户回调失败不应打开服务熔断器。
                relevant = response.curl_code == CURLE_RECV_ERROR ||
                           response.curl_code == CURLE_PARTIAL_FILE ||
                           response.curl_code == CURLE_GOT_NOTHING;
                failed = relevant;
                break;
            case HttpErrorCategory::Http:
                relevant = true;
                failed = policy_.countHttp5xx && response.code >= 500 && response.code < 600;
                break;
            case HttpErrorCategory::None:
                relevant = true;
                failed = false;
                break;
            case HttpErrorCategory::InvalidArgument:
            case HttpErrorCategory::Cancelled:
            case HttpErrorCategory::RateLimited:
            case HttpErrorCategory::CircuitOpen:
            case HttpErrorCategory::Internal:
                relevant = false;
                break;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        if (halfOpenAdmission) {
            if (state_ != State::HalfOpen) return;
            if (halfOpenInFlight_ > 0) --halfOpenInFlight_;
            if (!relevant) return;
            if (failed) {
                OpenLocked();
                return;
            }
            ++halfOpenSuccesses_;
            if (halfOpenInFlight_ == 0 && halfOpenSuccesses_ > 0) {
                state_ = State::Closed;
                failures_ = 0;
                halfOpenSuccesses_ = 0;
            }
            return;
        }

        // 一个在 Closed 状态放行的旧请求完成时，不应改变已经进入 Open/HalfOpen 的新一轮状态。
        if (state_ != State::Closed || !relevant) return;
        if (failed) {
            ++failures_;
            if (failures_ >= policy_.failureThreshold) OpenLocked();
        } else {
            failures_ = 0;
        }
    }

private:
    enum class State { Closed, Open, HalfOpen };
    void OpenLocked() {
        state_ = State::Open;
        openedAt_ = std::chrono::steady_clock::now();
        halfOpenInFlight_ = 0;
        halfOpenSuccesses_ = 0;
    }

    CircuitBreakerPolicy policy_;
    std::mutex mutex_;
    State state_ = State::Closed;
    std::size_t failures_ = 0;
    std::size_t halfOpenInFlight_ = 0;
    std::size_t halfOpenSuccesses_ = 0;
    std::chrono::steady_clock::time_point openedAt_{};
};
 } // 匿名命名空间

// ============================================================
// Transport 与 Mock 实现
// ============================================================

struct CurlTransport::Impl {
    CURL* curl = nullptr;
    mutable std::mutex mutex;

    Impl() {
        if (EnsureGlobal()) curl = curl_easy_init();
    }
    ~Impl() {
        if (curl) curl_easy_cleanup(curl);
    }
};

CurlTransport::CurlTransport() : impl_(std::make_unique<Impl>()) {}
CurlTransport::~CurlTransport() = default;

bool CurlTransport::Valid() const noexcept {
    return impl_ && impl_->curl;
}

CURL* CurlTransport::NativeHandle() noexcept {
    return impl_ ? impl_->curl : nullptr;
}

HttpResponse CurlTransport::Send(const HttpRequestData& request, const RequestOptions& options) {
    HttpResponse response;
    if (!impl_ || !impl_->curl) {
        response.curl_code = CURLE_FAILED_INIT;
        response.error = "CurlTransport 没有有效的 CURL handle";
        response.errorCategory = HttpErrorCategory::Internal;
        response.requestId = request.requestId;
        return response;
    }
    std::lock_guard<std::mutex> lock(impl_->mutex);

    std::vector<std::string> savedCookies;
    if (!options.enableCookieEngine) {
        curl_slist* list = nullptr;
        const CURLcode listCode = curl_easy_getinfo(impl_->curl, CURLINFO_COOKIELIST, &list);
        if (listCode != CURLE_OK) {
            curl_slist_free_all(list);
            response.curl_code = listCode;
            response.errorCategory = HttpErrorCategory::Internal;
            response.error = std::string("读取 Cookie Engine 状态失败: ") + curl_easy_strerror(listCode);
            response.requestId = request.requestId;
            return response;
        }
        for (curl_slist* item = list; item; item = item->next) {
            if (item->data) savedCookies.emplace_back(item->data);
        }
        curl_slist_free_all(list);
        const CURLcode clearCode = curl_easy_setopt(impl_->curl, CURLOPT_COOKIELIST, "ALL");
        if (clearCode != CURLE_OK) {
            response.curl_code = clearCode;
            response.errorCategory = HttpErrorCategory::Internal;
            response.error = std::string("临时隔离 Cookie Engine 失败: ") + curl_easy_strerror(clearCode);
            response.requestId = request.requestId;
            return response;
        }
    }

    LogRequestDebug(request, options);
    response = PerformOnHandle(impl_->curl, request.method, request.url, request.body, options);

    if (!options.enableCookieEngine) {
        CURLcode restoreCode = curl_easy_setopt(impl_->curl, CURLOPT_COOKIELIST, "ALL");
        if (restoreCode == CURLE_OK) {
            for (const auto& cookie : savedCookies) {
                restoreCode = curl_easy_setopt(impl_->curl, CURLOPT_COOKIELIST, cookie.c_str());
                if (restoreCode != CURLE_OK) break;
            }
        }
        if (restoreCode != CURLE_OK) {
            response.success = false;
            response.curl_code = restoreCode;
            response.errorCategory = HttpErrorCategory::Internal;
            if (!response.error.empty()) response.error += "; ";
            response.error += std::string("恢复 Cookie Engine 状态失败: ") + curl_easy_strerror(restoreCode);
        }
    }

    response.requestId = request.requestId;
    response.errorCategory = ClassifyError(response);
    LogResponseDebug(request, options, response);
    return response;
}

std::vector<std::string> CurlTransport::GetCookieList() const {
    std::vector<std::string> result;
    if (!impl_ || !impl_->curl) return result;
    std::lock_guard<std::mutex> lock(impl_->mutex);
    curl_slist* list = nullptr;
    if (curl_easy_getinfo(impl_->curl, CURLINFO_COOKIELIST, &list) != CURLE_OK) return result;
    for (curl_slist* p = list; p; p = p->next) {
        if (p->data) result.emplace_back(p->data);
    }
    curl_slist_free_all(list);
    return result;
}

bool CurlTransport::ClearCookies() {
    if (!impl_ || !impl_->curl) return false;
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return curl_easy_setopt(impl_->curl, CURLOPT_COOKIELIST, "ALL") == CURLE_OK;
}

bool CurlTransport::ClearSessionCookies() {
    if (!impl_ || !impl_->curl) return false;
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return curl_easy_setopt(impl_->curl, CURLOPT_COOKIELIST, "SESS") == CURLE_OK;
}

bool CurlTransport::FlushCookies() {
    if (!impl_ || !impl_->curl) return false;
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return curl_easy_setopt(impl_->curl, CURLOPT_COOKIELIST, "FLUSH") == CURLE_OK;
}

struct MockTransport::Impl {
    struct Rule { MockPredicate predicate; MockResponder responder; };
    std::vector<Rule> rules;
    std::mutex mutex;
};

MockTransport::MockTransport() : impl_(std::make_unique<Impl>()) {}
MockTransport::~MockTransport() = default;

void MockTransport::AddRule(MockPredicate predicate, MockResponder responder) {
    if (!predicate || !responder) return;
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->rules.push_back({std::move(predicate), std::move(responder)});
}

void MockTransport::AddStaticResponse(std::string method, std::string url, HttpResponse response) {
    AddRule(
        [method = ToUpperCopy(method), url](const HttpRequestData& request, const RequestOptions&) {
            return ToUpperCopy(request.method) == method && request.url == url;
        },
        [response = std::move(response)](const HttpRequestData&, const RequestOptions&) mutable {
            return response;
        });
}

void MockTransport::Clear() {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->rules.clear();
}

HttpResponse MockTransport::Send(const HttpRequestData& request, const RequestOptions& options) {
    try {
        std::vector<Impl::Rule> rules;
        {
            std::lock_guard<std::mutex> lock(impl_->mutex);
            rules = impl_->rules;
        }
        for (auto& rule : rules) {
            if (rule.predicate && rule.predicate(request, options)) {
                HttpResponse response = rule.responder(request, options);
                response.requestId = request.requestId;
                if (response.attempts <= 0) response.attempts = 1;
                response.errorCategory = ClassifyError(response);
                return response;
            }
        }
        HttpResponse response;
        response.success = false;
        response.curl_code = CURLE_COULDNT_CONNECT;
        response.error = "没有匹配当前请求的 Mock 规则";
        response.errorCategory = HttpErrorCategory::Connect;
        response.requestId = request.requestId;
        return response;
    } catch (const std::exception& ex) {
        HttpResponse response;
        response.success = false;
        response.curl_code = CURLE_ABORTED_BY_CALLBACK;
        response.error = std::string("Mock 规则回调抛出异常: ") + ex.what();
        response.errorCategory = HttpErrorCategory::Internal;
        response.requestId = request.requestId;
        return response;
    } catch (...) {
        HttpResponse response;
        response.success = false;
        response.curl_code = CURLE_ABORTED_BY_CALLBACK;
        response.error = "Mock 规则回调抛出未知异常";
        response.errorCategory = HttpErrorCategory::Internal;
        response.requestId = request.requestId;
        return response;
    }
}

// ============================================================
// 认证、中间件与指标统计
// ============================================================

bool IAuthProvider::CanRefresh(const HttpResponse&) const { return false; }
bool IAuthProvider::Refresh() { return false; }
std::uint64_t IAuthProvider::Version() const noexcept { return 0; }

BasicAuthProvider::BasicAuthProvider(std::string username, std::string password, unsigned long auth)
    : username_(std::move(username)), password_(std::move(password)), auth_(auth) {}

void BasicAuthProvider::Apply(HttpRequestData&, RequestOptions& options) {
    options.headers.EraseHeader("Authorization");
    options.bearerToken.clear();
    options.username = username_;
    options.password = password_;
    options.httpAuth = auth_;
}

BearerAuthProvider::BearerAuthProvider(std::string token, RefreshCallback refreshCallback)
    : token_(std::move(token)), refresh_(std::move(refreshCallback)) {}

void BearerAuthProvider::Apply(HttpRequestData&, RequestOptions& options) {
    options.headers.EraseHeader("Authorization");
    std::lock_guard<std::mutex> lock(mutex_);
    options.username.clear();
    options.password.clear();
    options.bearerToken = token_;
}

bool BearerAuthProvider::CanRefresh(const HttpResponse& response) const {
    return response.code == 401 && static_cast<bool>(refresh_);
}

bool BearerAuthProvider::Refresh() {
    if (!refresh_) return false;
    const std::uint64_t observedVersion = version_.load(std::memory_order_acquire);
    std::lock_guard<std::mutex> refreshLock(refreshMutex_);
    // 另一个线程已经完成了同一轮刷新时，当前调用直接复用新 Token。
    if (version_.load(std::memory_order_acquire) != observedVersion) return true;
    try {
        auto token = refresh_();
        if (!token) return false;

        // Refresh 回调可能耗时较长；回调执行期间外部 SetToken() 可能已经写入更新 Token。
        // 再次比较版本，只在本轮刷新仍然是最新写入者时提交结果，防止陈旧刷新覆盖外部更新。
        std::lock_guard<std::mutex> tokenLock(mutex_);
        if (version_.load(std::memory_order_acquire) != observedVersion) {
            return true;
        }
        token_ = std::move(*token);
        version_.fetch_add(1, std::memory_order_release);
        return true;
    } catch (...) {
        return false;
    }
}

std::uint64_t BearerAuthProvider::Version() const noexcept {
    return version_.load(std::memory_order_acquire);
}

void BearerAuthProvider::SetToken(std::string token) {
    std::lock_guard<std::mutex> lock(mutex_);
    token_ = std::move(token);
    // Token 与版本在同一临界区内更新，保证观察到新版本的线程随后一定能读取到对应的新 Token。
    version_.fetch_add(1, std::memory_order_release);
}

std::string BearerAuthProvider::GetToken() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return token_;
}

ApiKeyAuthProvider::ApiKeyAuthProvider(std::string name, std::string value, ApiKeyLocation location)
    : name_(std::move(name)), value_(std::move(value)), location_(location) {
    if (location_ != ApiKeyLocation::Header && location_ != ApiKeyLocation::Query) {
        throw std::invalid_argument("ApiKeyLocation 枚举值无效");
    }
    if (name_.empty()) {
        throw std::invalid_argument("API Key 名称不能为空");
    }
    if (location_ == ApiKeyLocation::Header &&
        (!IsHttpToken(name_) || !IsSafeHttpFieldValue(value_))) {
        throw std::invalid_argument("API Key Header 名称或值不合法");
    }
}

void ApiKeyAuthProvider::Apply(HttpRequestData& request, RequestOptions& options) {
    if (location_ == ApiKeyLocation::Header) {
        if (!options.headers.SetHeader(name_, value_)) {
            throw std::invalid_argument("API Key Header 名称或值不合法");
        }
        const std::string lowerName = ToLowerCopy(name_);
        const bool alreadySensitive = std::any_of(
            options.debug.sensitiveHeaders.begin(), options.debug.sensitiveHeaders.end(),
            [&](const std::string& item) { return ToLowerCopy(TrimCopy(item)) == lowerName; });
        if (!alreadySensitive) options.debug.sensitiveHeaders.push_back(name_);
        return;
    }

    // Query 形式的 API Key 名称可能是业务自定义文本，不一定命中内置 token/api_key 等规则。
    // 认证提供器既然知道这个参数承载凭据，就必须自动登记为敏感 Query，确保 Debug 与 Metrics 一致脱敏。
    const std::string lowerName = ToLowerCopy(name_);
    const bool alreadySensitive = std::any_of(
        options.debug.sensitiveQueryParameters.begin(), options.debug.sensitiveQueryParameters.end(),
        [&](const std::string& item) { return ToLowerCopy(TrimCopy(item)) == lowerName; });
    if (!alreadySensitive) options.debug.sensitiveQueryParameters.push_back(name_);

    const std::string encodedName = UrlEncode(name_);
    const std::string encodedValue = UrlEncode(value_);
    const std::string pair = encodedName + "=" + encodedValue;

    const std::size_t fragmentPos = request.url.find('#');
    const std::size_t mainEnd =
        fragmentPos == std::string::npos ? request.url.size() : fragmentPos;
    const std::string fragment =
        fragmentPos == std::string::npos ? std::string{} : request.url.substr(fragmentPos);

    const std::size_t queryPos = request.url.find('?');
    const bool hasQuery = queryPos != std::string::npos && queryPos < mainEnd;
    const std::string prefix =
        hasQuery ? request.url.substr(0, queryPos) : request.url.substr(0, mainEnd);

    std::vector<std::string> kept;
    if (hasQuery) {
        const std::string_view queryText(request.url.data() + queryPos + 1,
                                         mainEnd - queryPos - 1);
        std::size_t start = 0;
        while (start <= queryText.size()) {
            const std::size_t amp = queryText.find('&', start);
            const std::size_t end =
                amp == std::string_view::npos ? queryText.size() : amp;
            const std::string_view item = queryText.substr(start, end - start);
            if (!item.empty()) {
                const std::size_t equal = item.find('=');
                const std::string decodedName =
                    UrlDecode(item.substr(0, equal == std::string_view::npos
                                                ? item.size()
                                                : equal));
                if (decodedName != name_) {
                    kept.emplace_back(item);
                }
            }
            if (amp == std::string_view::npos) break;
            start = amp + 1;
        }
    }

    std::ostringstream rebuilt;
    rebuilt << prefix << '?';
    bool first = true;
    for (const auto& item : kept) {
        if (!first) rebuilt << '&';
        first = false;
        rebuilt << item;
    }
    if (!first) rebuilt << '&';
    rebuilt << pair << fragment;
    request.url = rebuilt.str();
}

bool IHttpMiddleware::Before(HttpRequestData&, RequestOptions&, HttpResponse&) { return true; }
void IHttpMiddleware::After(const HttpRequestData&, const RequestOptions&, HttpResponse&) {}

struct BasicMetricsCollector::Impl {
    mutable std::mutex mutex;
    HttpMetricsSnapshot snapshot;
};

BasicMetricsCollector::BasicMetricsCollector() : impl_(std::make_unique<Impl>()) {}
BasicMetricsCollector::~BasicMetricsCollector() = default;

void BasicMetricsCollector::OnRequestCompleted(const HttpMetricsEvent& event) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    auto& s = impl_->snapshot;
    ++s.totalRequests;
    if (event.transportOk) ++s.transportSuccess;
    else ++s.failedRequests;
    if (event.statusCode >= 200 && event.statusCode < 300) ++s.http2xx;
    else if (event.statusCode >= 300 && event.statusCode < 400) ++s.http3xx;
    else if (event.statusCode >= 400 && event.statusCode < 500) ++s.http4xx;
    else if (event.statusCode >= 500 && event.statusCode < 600) ++s.http5xx;
    if (event.attempts > 1) s.totalRetries += static_cast<std::uint64_t>(event.attempts - 1);
    if (event.downloadedBytes > 0) s.downloadedBytes += static_cast<std::uint64_t>(event.downloadedBytes);
    if (event.uploadedBytes > 0) s.uploadedBytes += static_cast<std::uint64_t>(event.uploadedBytes);
}

HttpMetricsSnapshot BasicMetricsCollector::Snapshot() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->snapshot;
}

void BasicMetricsCollector::Reset() {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->snapshot = {};
}

// ============================================================
// HttpRequest 一次性请求 API
// ============================================================

HttpResponse HttpRequest::Send(HttpRequestData request, const RequestOptions& options) {
    if (request.requestId.empty()) request.requestId = GenerateRequestId();
    CurlTransport transport;
    return transport.Send(request, options);
}

HttpResponse HttpRequest::Request(HttpMethod method,
                                  std::string_view url,
                                  std::string_view body,
                                  const RequestOptions& options) {
    return Request(ToString(method), url, body, options);
}

HttpResponse HttpRequest::Request(std::string_view method,
                                  std::string_view url,
                                  std::string_view body,
                                  const RequestOptions& options) {
    HttpRequestData request;
    request.method.assign(method.data(), method.size());
    request.url.assign(url.data(), url.size());
    request.body.assign(body.data(), body.size());
    return Send(std::move(request), options);
}

HttpResponse HttpRequest::Get(std::string_view url, const RequestOptions& options) { return Request(HttpMethod::Get, url, {}, options); }
HttpResponse HttpRequest::Head(std::string_view url, const RequestOptions& options) { return Request(HttpMethod::Head, url, {}, options); }
HttpResponse HttpRequest::Post(std::string_view url, std::string_view body, const RequestOptions& options) { return Request(HttpMethod::Post, url, body, options); }
HttpResponse HttpRequest::Put(std::string_view url, std::string_view body, const RequestOptions& options) { return Request(HttpMethod::Put, url, body, options); }
HttpResponse HttpRequest::Delete(std::string_view url, std::string_view body, const RequestOptions& options) { return Request(HttpMethod::Delete, url, body, options); }
HttpResponse HttpRequest::Patch(std::string_view url, std::string_view body, const RequestOptions& options) { return Request(HttpMethod::Patch, url, body, options); }
HttpResponse HttpRequest::Options(std::string_view url, const RequestOptions& options) { return Request(HttpMethod::Options, url, {}, options); }
HttpResponse HttpRequest::Trace(std::string_view url, const RequestOptions& options) { return Request(HttpMethod::Trace, url, {}, options); }
HttpResponse HttpRequest::Connect(std::string_view url, const RequestOptions& options) { return Request(HttpMethod::Connect, url, {}, options); }

HttpResponse HttpRequest::RequestJson(std::string_view method, std::string_view url, std::string_view json, RequestOptions options) {
    options.headers.SetDefaultHeader("Content-Type", "application/json");
    options.headers.SetDefaultHeader("Accept", "application/json");
    return Request(method, url, json, options);
}
HttpResponse HttpRequest::PostJson(std::string_view url, std::string_view json, RequestOptions options) { return RequestJson("POST", url, json, std::move(options)); }
HttpResponse HttpRequest::PutJson(std::string_view url, std::string_view json, RequestOptions options) { return RequestJson("PUT", url, json, std::move(options)); }
HttpResponse HttpRequest::PatchJson(std::string_view url, std::string_view json, RequestOptions options) { return RequestJson("PATCH", url, json, std::move(options)); }

HttpResponse HttpRequest::PostForm(std::string_view url, const UrlParams& form, RequestOptions options) {
    options.headers.SetDefaultHeader("Content-Type", "application/x-www-form-urlencoded");
    return Post(url, form.ToString(), options);
}

HttpResponse HttpRequest::UploadMultipart(std::string_view url, std::vector<MultipartPart> parts, RequestOptions options) {
    options.multipart = std::move(parts);
    return Post(url, {}, options);
}

HttpResponse HttpRequest::UploadFile(std::string_view url,
                                     const std::string& filePath,
                                     RequestOptions options,
                                     std::string_view method) {
    auto source = std::make_shared<FileUploadSource>(filePath);
    if (!source->Valid()) {
        HttpResponse response;
        response.curl_code = CURLE_READ_ERROR;
        response.error = "无法打开上传文件: " + filePath;
        response.errorCategory = HttpErrorCategory::Upload;
        return response;
    }
    options.uploadSource = source;
    return Request(method, url, {}, options);
}

HttpResponse HttpRequest::Download(std::string_view url, const std::string& filePath, RequestOptions options) {
    return DownloadWithInvoker(filePath, std::move(options), [&](const RequestOptions& finalOptions) {
        return Get(url, finalOptions);
    });
}

// ============================================================
// HttpClient 会话接口
// ============================================================

namespace {

void SetExtensionException(HttpResponse& response, const std::string& message) {
    response.success = false;
    response.curl_code = CURLE_ABORTED_BY_CALLBACK;
    response.errorCategory = HttpErrorCategory::Internal;
    if (response.error.empty()) response.error = message;
    else response.error += "; " + message;
}

void RunMiddlewareAfterSafely(const std::vector<std::shared_ptr<IHttpMiddleware>>& middleware,
                              std::size_t passed,
                              const HttpRequestData& request,
                              const RequestOptions& options,
                              HttpResponse& response) {
    passed = (std::min)(passed, middleware.size());
    for (std::size_t i = passed; i > 0; --i) {
        const auto& item = middleware[i - 1];
        if (!item) continue;
        try {
            item->After(request, options, response);
        } catch (const std::exception& ex) {
            SetExtensionException(response, std::string("Middleware After 抛出异常: ") + ex.what());
        } catch (...) {
            SetExtensionException(response, "Middleware After 抛出未知异常");
        }
    }
}

void RunResponseInterceptorsSafely(const std::vector<ResponseInterceptor>& interceptors,
                                   const HttpRequestData& request,
                                   const RequestOptions& options,
                                   HttpResponse& response) {
    for (const auto& interceptor : interceptors) {
        if (!interceptor) continue;
        try {
            interceptor(request, options, response);
        } catch (const std::exception& ex) {
            SetExtensionException(response, std::string("响应拦截器抛出异常: ") + ex.what());
        } catch (...) {
            SetExtensionException(response, "响应拦截器抛出未知异常");
        }
    }
}

} // 匿名命名空间

struct HttpClient::Impl {
    RequestOptions defaultOptions;
    HttpClientOptions clientOptions;
    std::shared_ptr<IHttpTransport> transport;
    mutable std::mutex configMutex;
    std::mutex authRefreshMutex;
    std::shared_ptr<RateLimiterState> rateLimiter;
    std::shared_ptr<CircuitBreakerState> circuitBreaker;

    Impl(RequestOptions defaults, HttpClientOptions client, std::shared_ptr<IHttpTransport> transportValue)
        : defaultOptions(std::move(defaults)),
          clientOptions(std::move(client)),
          transport(std::move(transportValue)),
          rateLimiter(std::make_shared<RateLimiterState>(clientOptions.rateLimit)),
          circuitBreaker(std::make_shared<CircuitBreakerState>(clientOptions.circuitBreaker)) {
        if (!transport) transport = std::make_shared<CurlTransport>();
    }

    HttpResponse Execute(HttpRequestData request, RequestOptions options) {
        if (request.requestId.empty()) request.requestId = GenerateRequestId();

        HttpClientOptions client;
        std::shared_ptr<IHttpTransport> currentTransport;
        std::shared_ptr<RateLimiterState> limiter;
        std::shared_ptr<CircuitBreakerState> breaker;
        {
            std::lock_guard<std::mutex> lock(configMutex);
            client = clientOptions;
            currentTransport = transport;
            limiter = rateLimiter;
            breaker = circuitBreaker;
        }

        HttpResponse response;
        response.requestId = request.requestId;
        std::size_t passed = 0;
        bool circuitAdmitted = false;
        bool circuitHalfOpenAdmission = false;

        const auto finish = [&](HttpResponse result, std::size_t afterCount) {
            result.requestId = request.requestId;
            result.errorCategory = ClassifyError(result);
            RunMiddlewareAfterSafely(client.middleware, afterCount, request, options, result);
            RunResponseInterceptorsSafely(client.responseInterceptors, request, options, result);
            EmitMetricsSafely(client.metricsCollector, request, options, result);
            return result;
        };

        if (!ValidateClientOptions(client, response)) {
            RunResponseInterceptorsSafely(client.responseInterceptors, request, options, response);
            EmitMetricsSafely(client.metricsCollector, request, options, response);
            return response;
        }

        try {
            for (auto& interceptor : client.requestInterceptors) {
                if (interceptor) interceptor(request, options);
            }

            std::uint64_t authVersion = 0;
            if (client.authProvider) {
                authVersion = client.authProvider->Version();
                client.authProvider->Apply(request, options);
            }

            for (const auto& middleware : client.middleware) {
                if (!middleware) {
                    ++passed;
                    continue;
                }
                const bool proceed = middleware->Before(request, options, response);
                if (!proceed) {
                    if (!response.success && response.curl_code == CURLE_OK &&
                        response.code == 0 && response.error.empty()) {
                        response.curl_code = CURLE_ABORTED_BY_CALLBACK;
                        response.errorCategory = HttpErrorCategory::Internal;
                        response.error = "Middleware 已终止请求，但没有提供响应或错误信息";
                    }
                    return finish(std::move(response), passed);
                }
                ++passed;
            }

            if (limiter && !limiter->Acquire(options.cancelFlag)) {
                response.curl_code = CURLE_ABORTED_BY_CALLBACK;
                response.error = "请求在等待限流许可时被取消";
                response.errorCategory = HttpErrorCategory::Cancelled;
                return finish(std::move(response), passed);
            }
            RateLease lease(limiter);

            if (breaker && !breaker->Allow(circuitHalfOpenAdmission)) {
                response.curl_code = CURLE_OK;
                response.error = "Circuit Breaker 当前处于熔断状态";
                response.errorCategory = HttpErrorCategory::CircuitOpen;
                return finish(std::move(response), passed);
            }
            circuitAdmitted = static_cast<bool>(breaker);

            try {
                response = currentTransport->Send(request, options);
            } catch (const std::exception& ex) {
                response.success = false;
                response.curl_code = CURLE_ABORTED_BY_CALLBACK;
                response.errorCategory = HttpErrorCategory::Internal;
                response.error = std::string("Transport::Send 抛出异常: ") + ex.what();
            } catch (...) {
                response.success = false;
                response.curl_code = CURLE_ABORTED_BY_CALLBACK;
                response.errorCategory = HttpErrorCategory::Internal;
                response.error = "Transport::Send 抛出未知异常";
            }
            if (response.attempts <= 0) response.attempts = 1;

            if (client.authProvider &&
                client.refreshAuthOnUnauthorized &&
                client.authProvider->CanRefresh(response)) {
                std::lock_guard<std::mutex> refreshLock(authRefreshMutex);
                bool refreshed = client.authProvider->Version() != authVersion;
                if (!refreshed) refreshed = client.authProvider->Refresh();
                if (refreshed &&
                    PrepareExternalSinkForLogicalReplay(options, response) &&
                    RewindUploadForLogicalReplay(options, response)) {
                    HttpRequestData retryRequest = request;
                    RequestOptions retryOptions = options;
                    client.authProvider->Apply(retryRequest, retryOptions);
                    const int previousAttempts = (std::max)(response.attempts, 1);
                    try {
                        response = currentTransport->Send(retryRequest, retryOptions);
                    } catch (const std::exception& ex) {
                        response = {};
                        response.success = false;
                        response.curl_code = CURLE_ABORTED_BY_CALLBACK;
                        response.errorCategory = HttpErrorCategory::Internal;
                        response.error = std::string("认证重放时 Transport::Send 抛出异常: ") + ex.what();
                    } catch (...) {
                        response = {};
                        response.success = false;
                        response.curl_code = CURLE_ABORTED_BY_CALLBACK;
                        response.errorCategory = HttpErrorCategory::Internal;
                        response.error = "认证重放时 Transport::Send 抛出未知异常";
                    }
                    response.attempts = (std::max)(response.attempts, 1) + previousAttempts;
                }
            }

            if (breaker && circuitAdmitted) {
                breaker->Record(response, circuitHalfOpenAdmission);
                circuitAdmitted = false;
            }
            return finish(std::move(response), passed);
        } catch (const std::exception& ex) {
            if (breaker && circuitAdmitted) {
                breaker->Cancel(circuitHalfOpenAdmission);
            }
            response.success = false;
            response.curl_code = CURLE_ABORTED_BY_CALLBACK;
            response.errorCategory = HttpErrorCategory::Internal;
            response.error = std::string("HttpClient 扩展回调抛出异常: ") + ex.what();
            return finish(std::move(response), passed);
        } catch (...) {
            if (breaker && circuitAdmitted) {
                breaker->Cancel(circuitHalfOpenAdmission);
            }
            response.success = false;
            response.curl_code = CURLE_ABORTED_BY_CALLBACK;
            response.errorCategory = HttpErrorCategory::Internal;
            response.error = "HttpClient 扩展回调抛出未知异常";
            return finish(std::move(response), passed);
        }
    }
};

HttpClient::HttpClient() : impl_(std::make_unique<Impl>(RequestOptions{}, HttpClientOptions{}, nullptr)) {}
HttpClient::HttpClient(RequestOptions defaultOptions) : impl_(std::make_unique<Impl>(std::move(defaultOptions), HttpClientOptions{}, nullptr)) {}
HttpClient::HttpClient(HttpClientOptions clientOptions, std::shared_ptr<IHttpTransport> transport) : impl_(std::make_unique<Impl>(RequestOptions{}, std::move(clientOptions), std::move(transport))) {}
HttpClient::HttpClient(RequestOptions defaultOptions, HttpClientOptions clientOptions, std::shared_ptr<IHttpTransport> transport) : impl_(std::make_unique<Impl>(std::move(defaultOptions), std::move(clientOptions), std::move(transport))) {}
HttpClient::~HttpClient() = default;
HttpClient::HttpClient(HttpClient&& other) noexcept = default;
HttpClient& HttpClient::operator=(HttpClient&& other) noexcept = default;

RequestOptions& HttpClient::DefaultOptions() noexcept { return impl_->defaultOptions; }
const RequestOptions& HttpClient::DefaultOptions() const noexcept { return impl_->defaultOptions; }
void HttpClient::SetDefaultOptions(RequestOptions options) { std::lock_guard<std::mutex> lock(impl_->configMutex); impl_->defaultOptions = std::move(options); }
const HttpClientOptions& HttpClient::ClientOptions() const noexcept { return impl_->clientOptions; }
void HttpClient::SetClientOptions(HttpClientOptions options) {
    std::lock_guard<std::mutex> lock(impl_->configMutex);
    impl_->clientOptions = std::move(options);
    impl_->rateLimiter = std::make_shared<RateLimiterState>(impl_->clientOptions.rateLimit);
    impl_->circuitBreaker = std::make_shared<CircuitBreakerState>(impl_->clientOptions.circuitBreaker);
}
void HttpClient::SetTransport(std::shared_ptr<IHttpTransport> transport) { if (!transport) transport = std::make_shared<CurlTransport>(); std::lock_guard<std::mutex> lock(impl_->configMutex); impl_->transport = std::move(transport); }
void HttpClient::AddMiddleware(std::shared_ptr<IHttpMiddleware> middleware) { if (!middleware) return; std::lock_guard<std::mutex> lock(impl_->configMutex); impl_->clientOptions.middleware.push_back(std::move(middleware)); }
void HttpClient::AddRequestInterceptor(RequestInterceptor interceptor) { if (!interceptor) return; std::lock_guard<std::mutex> lock(impl_->configMutex); impl_->clientOptions.requestInterceptors.push_back(std::move(interceptor)); }
void HttpClient::AddResponseInterceptor(ResponseInterceptor interceptor) { if (!interceptor) return; std::lock_guard<std::mutex> lock(impl_->configMutex); impl_->clientOptions.responseInterceptors.push_back(std::move(interceptor)); }

HttpResponse HttpClient::Send(HttpRequestData request) { RequestOptions options; { std::lock_guard<std::mutex> lock(impl_->configMutex); options = impl_->defaultOptions; } return impl_->Execute(std::move(request), std::move(options)); }
HttpResponse HttpClient::Send(HttpRequestData request, const RequestOptions& options) { return impl_->Execute(std::move(request), options); }

HttpResponse HttpClient::Request(HttpMethod method, std::string_view url, std::string_view body) { return Request(ToString(method), url, body); }
HttpResponse HttpClient::Request(HttpMethod method, std::string_view url, std::string_view body, const RequestOptions& options) { return Request(ToString(method), url, body, options); }
HttpResponse HttpClient::Request(std::string_view method, std::string_view url, std::string_view body) { HttpRequestData r; r.method.assign(method.data(), method.size()); r.url.assign(url.data(), url.size()); r.body.assign(body.data(), body.size()); return Send(std::move(r)); }
HttpResponse HttpClient::Request(std::string_view method, std::string_view url, std::string_view body, const RequestOptions& options) { HttpRequestData r; r.method.assign(method.data(), method.size()); r.url.assign(url.data(), url.size()); r.body.assign(body.data(), body.size()); return Send(std::move(r), options); }

HttpResponse HttpClient::Get(std::string_view url) { return Request(HttpMethod::Get, url); }
HttpResponse HttpClient::Get(std::string_view url, const RequestOptions& options) { return Request(HttpMethod::Get, url, {}, options); }
HttpResponse HttpClient::Head(std::string_view url) { return Request(HttpMethod::Head, url); }
HttpResponse HttpClient::Head(std::string_view url, const RequestOptions& options) { return Request(HttpMethod::Head, url, {}, options); }
HttpResponse HttpClient::Post(std::string_view url, std::string_view body) { return Request(HttpMethod::Post, url, body); }
HttpResponse HttpClient::Post(std::string_view url, std::string_view body, const RequestOptions& options) { return Request(HttpMethod::Post, url, body, options); }
HttpResponse HttpClient::Put(std::string_view url, std::string_view body) { return Request(HttpMethod::Put, url, body); }
HttpResponse HttpClient::Put(std::string_view url, std::string_view body, const RequestOptions& options) { return Request(HttpMethod::Put, url, body, options); }
HttpResponse HttpClient::Delete(std::string_view url, std::string_view body) { return Request(HttpMethod::Delete, url, body); }
HttpResponse HttpClient::Delete(std::string_view url, std::string_view body, const RequestOptions& options) { return Request(HttpMethod::Delete, url, body, options); }
HttpResponse HttpClient::Patch(std::string_view url, std::string_view body) { return Request(HttpMethod::Patch, url, body); }
HttpResponse HttpClient::Patch(std::string_view url, std::string_view body, const RequestOptions& options) { return Request(HttpMethod::Patch, url, body, options); }
HttpResponse HttpClient::Options(std::string_view url) { return Request(HttpMethod::Options, url); }
HttpResponse HttpClient::Options(std::string_view url, const RequestOptions& options) { return Request(HttpMethod::Options, url, {}, options); }

HttpResponse HttpClient::RequestJson(std::string_view method, std::string_view url, std::string_view json) { RequestOptions o; { std::lock_guard<std::mutex> lock(impl_->configMutex); o = impl_->defaultOptions; } return RequestJson(method, url, json, std::move(o)); }
HttpResponse HttpClient::RequestJson(std::string_view method, std::string_view url, std::string_view json, RequestOptions options) { options.headers.SetDefaultHeader("Content-Type", "application/json"); options.headers.SetDefaultHeader("Accept", "application/json"); return Request(method, url, json, options); }
HttpResponse HttpClient::PostJson(std::string_view url, std::string_view json) { return RequestJson("POST", url, json); }
HttpResponse HttpClient::PostJson(std::string_view url, std::string_view json, RequestOptions options) { return RequestJson("POST", url, json, std::move(options)); }
HttpResponse HttpClient::PutJson(std::string_view url, std::string_view json) { return RequestJson("PUT", url, json); }
HttpResponse HttpClient::PutJson(std::string_view url, std::string_view json, RequestOptions options) { return RequestJson("PUT", url, json, std::move(options)); }
HttpResponse HttpClient::PatchJson(std::string_view url, std::string_view json) { return RequestJson("PATCH", url, json); }
HttpResponse HttpClient::PatchJson(std::string_view url, std::string_view json, RequestOptions options) { return RequestJson("PATCH", url, json, std::move(options)); }
HttpResponse HttpClient::PostForm(std::string_view url, const UrlParams& form) { RequestOptions o; { std::lock_guard<std::mutex> lock(impl_->configMutex); o = impl_->defaultOptions; } return PostForm(url, form, std::move(o)); }
HttpResponse HttpClient::PostForm(std::string_view url, const UrlParams& form, RequestOptions options) { options.headers.SetDefaultHeader("Content-Type", "application/x-www-form-urlencoded"); return Post(url, form.ToString(), options); }
HttpResponse HttpClient::UploadMultipart(std::string_view url, std::vector<MultipartPart> parts) { RequestOptions o; { std::lock_guard<std::mutex> lock(impl_->configMutex); o = impl_->defaultOptions; } return UploadMultipart(url, std::move(parts), std::move(o)); }
HttpResponse HttpClient::UploadMultipart(std::string_view url, std::vector<MultipartPart> parts, RequestOptions options) { options.multipart = std::move(parts); return Post(url, {}, options); }
HttpResponse HttpClient::UploadFile(std::string_view url, const std::string& filePath, std::string_view method) { RequestOptions o; { std::lock_guard<std::mutex> lock(impl_->configMutex); o = impl_->defaultOptions; } return UploadFile(url, filePath, std::move(o), method); }
HttpResponse HttpClient::UploadFile(std::string_view url, const std::string& filePath, RequestOptions options, std::string_view method) { auto source = std::make_shared<FileUploadSource>(filePath); if (!source->Valid()) { HttpResponse r; r.curl_code=CURLE_READ_ERROR; r.error="无法打开上传文件: "+filePath; r.errorCategory=HttpErrorCategory::Upload; return r; } options.uploadSource=source; return Request(method,url,{},options); }
HttpResponse HttpClient::Download(std::string_view url, const std::string& filePath) { RequestOptions o; { std::lock_guard<std::mutex> lock(impl_->configMutex); o = impl_->defaultOptions; } return Download(url, filePath, std::move(o)); }
HttpResponse HttpClient::Download(std::string_view url, const std::string& filePath, RequestOptions options) { return DownloadWithInvoker(filePath, std::move(options), [&](const RequestOptions& finalOptions) { return Get(url, finalOptions); }); }

std::shared_ptr<IHttpTransport> HttpClient::Transport() const { std::lock_guard<std::mutex> lock(impl_->configMutex); return impl_->transport; }
std::vector<std::string> HttpClient::GetCookieList() const { auto t = std::dynamic_pointer_cast<CurlTransport>(Transport()); return t ? t->GetCookieList() : std::vector<std::string>{}; }
bool HttpClient::ClearCookies() { auto t = std::dynamic_pointer_cast<CurlTransport>(Transport()); return t && t->ClearCookies(); }
bool HttpClient::ClearSessionCookies() { auto t = std::dynamic_pointer_cast<CurlTransport>(Transport()); return t && t->ClearSessionCookies(); }
bool HttpClient::FlushCookies() { auto t = std::dynamic_pointer_cast<CurlTransport>(Transport()); return t && t->FlushCookies(); }

// ============================================================
// AsyncHttpClient 与 curl_multi 异步实现
// ============================================================

namespace {

class AsyncCompletionDispatcher {
public:
    AsyncCompletionDispatcher() {
        const unsigned int hardware = std::thread::hardware_concurrency();
        const std::size_t workerCount = (std::max)(
            std::size_t{2},
            (std::min)(std::size_t{4},
                       hardware == 0 ? std::size_t{2} : static_cast<std::size_t>(hardware)));
        workers_.reserve(workerCount);
        try {
            for (std::size_t i = 0; i < workerCount; ++i) {
                workers_.emplace_back([this] { Run(); });
            }
        } catch (...) {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                stopping_ = true;
            }
            cv_.notify_all();
            for (auto& worker : workers_) {
                if (worker.joinable()) worker.join();
            }
            throw;
        }
    }

    ~AsyncCompletionDispatcher() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stopping_ = true;
        }
        cv_.notify_all();
        for (auto& worker : workers_) {
            if (worker.joinable()) worker.join();
        }
    }

    void Post(std::function<void()> task) {
        if (!task) return;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (stopping_) {
                throw std::runtime_error("异步完成分发器正在关闭");
            }
            tasks_.push_back(std::move(task));
        }
        cv_.notify_one();
    }

private:
    void Run() noexcept {
        for (;;) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                cv_.wait(lock, [&] { return stopping_ || !tasks_.empty(); });
                if (stopping_ && tasks_.empty()) return;
                task = std::move(tasks_.front());
                tasks_.pop_front();
            }
            try { task(); } catch (...) {}
        }
    }

    std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<std::function<void()>> tasks_;
    bool stopping_ = false;
    std::vector<std::thread> workers_;
};

AsyncCompletionDispatcher& CompletionDispatcher() {
    static AsyncCompletionDispatcher dispatcher;
    return dispatcher;
}

template <typename Poster>
void DispatchAsyncCompletionWith(AsyncCompletionCallback callback,
                                 HttpResponse response,
                                 Poster&& poster) noexcept {
    if (!callback) return;
    // lambda 本体只做 noexcept move，不先转换成可能分配内存的 std::function。
    // 因此即使 poster 内部复制/入队任务失败，本地 task 仍保有唯一 callback 和 response，可同步兜底。
    auto task = [callback = std::move(callback), response = std::move(response)]() mutable {
        try { callback(std::move(response)); } catch (...) {}
    };
    try {
        poster(task);
    } catch (...) {
        try { task(); } catch (...) {}
    }
}

void DispatchAsyncCompletion(AsyncCompletionCallback callback, HttpResponse response) noexcept {
    DispatchAsyncCompletionWith(
        std::move(callback),
        std::move(response),
        [](const std::function<void()>& task) { CompletionDispatcher().Post(task); });
}

} // 匿名命名空间

struct AsyncHttpClient::Impl {
    struct Operation {
        HttpRequestData request;
        RequestOptions options;
        std::promise<HttpResponse> promise;
        AsyncCompletionCallback completion;
        CURL* easy = nullptr;
        HttpResponse response;
        TransferContext context;
        CurlSlistHolder headerList;
        CurlSlistHolder resolveList;
        CurlMimeHolder mime;
        std::array<char, CURL_ERROR_SIZE> errorBuffer{};
        int retryIndex = 0;
        int totalAttempts = 0;
        bool authReplayUsed = false;
        bool rateHeld = false;
        bool circuitAdmitted = false;
        bool circuitHalfOpenAdmission = false;
        bool transferStarted = false;
        std::uint64_t authVersion = 0;
        std::chrono::steady_clock::time_point readyAt = std::chrono::steady_clock::now();

        Operation(HttpRequestData r, RequestOptions o, AsyncCompletionCallback cb = {})
            : request(std::move(r)), options(std::move(o)), completion(std::move(cb)) {
            easy = curl_easy_init();
            context.response = &response;
            context.options = &options;
        }
        ~Operation() {
            // MIME 与 slist 可能仍被 easy handle 引用，先释放/解绑这些请求级资源，再销毁 easy handle。
            ResetHolders();
            if (easy) curl_easy_cleanup(easy);
        }
        void ResetHolders() {
            if (headerList.value) { curl_slist_free_all(headerList.value); headerList.value = nullptr; }
            if (resolveList.value) { curl_slist_free_all(resolveList.value); resolveList.value = nullptr; }
            if (mime.value) { curl_mime_free(mime.value); mime.value = nullptr; }
        }
    };

    struct QuarantinedCurlResources {
        CURL* easy = nullptr;
        curl_slist* headerList = nullptr;
        curl_slist* resolveList = nullptr;
        curl_mime* mime = nullptr;
    };

    RequestOptions defaultOptions;
    HttpClientOptions clientOptions;
    CURLM* multi = nullptr;
    CURLSH* cookieShare = nullptr;
    std::thread worker;
    std::thread authWorker;
    mutable std::mutex mutex;
    std::condition_variable cv;
    std::mutex authMutex;
    std::condition_variable authCv;
    std::deque<std::unique_ptr<Operation>> authQueue;
    std::deque<std::unique_ptr<Operation>> incoming;
    std::vector<std::unique_ptr<Operation>> delayed;
    std::unordered_map<CURL*, std::unique_ptr<Operation>> active;
    // multi 已进入 fatal 状态且 remove_handle 失败时，不能提前 easy_cleanup。
    // 暂存这批请求级 CURL 资源，待 multi 栈销毁后再回收。
    std::vector<QuarantinedCurlResources> quarantinedCurlResources;
    std::shared_ptr<std::atomic_size_t> pendingCount = std::make_shared<std::atomic_size_t>(0);
    std::atomic_bool shuttingDown{false};
    std::atomic_bool stopping{false};
    bool authStopping = false;
    std::atomic_int fatalMultiCode{static_cast<int>(CURLM_OK)};
    mutable std::mutex fatalMultiMutex;
    std::string fatalMultiMessage;
    std::string initializationError;
    std::shared_ptr<RateLimiterState> rateLimiter;
    std::shared_ptr<CircuitBreakerState> circuitBreaker;

    Impl(RequestOptions defaults, HttpClientOptions client)
        : defaultOptions(std::move(defaults)), clientOptions(std::move(client)),
          rateLimiter(std::make_shared<RateLimiterState>(clientOptions.rateLimit)),
          circuitBreaker(std::make_shared<CircuitBreakerState>(clientOptions.circuitBreaker)) {
        if (!EnsureGlobal()) {
            initializationError = "libcurl 全局初始化失败";
        } else {
            multi = curl_multi_init();
            cookieShare = curl_share_init();
            if (!multi) initializationError = "curl_multi_init 失败";
            if (!cookieShare && initializationError.empty()) initializationError = "curl_share_init 失败";
            if (cookieShare) {
                const CURLSHcode shareCode = curl_share_setopt(cookieShare, CURLSHOPT_SHARE, CURL_LOCK_DATA_COOKIE);
                if (shareCode != CURLSHE_OK) {
                    initializationError = std::string("共享 Cookie 状态初始化失败: ") + curl_share_strerror(shareCode);
                }
            }
        }
        if (multi && cookieShare && initializationError.empty()) {
            const auto setMultiOption = [&](CURLMoption option, long value, const char* name) {
                const CURLMcode code = curl_multi_setopt(multi, option, value);
                if (code == CURLM_OK) return true;
                initializationError = std::string("设置 ") + name + " 失败: " + curl_multi_strerror(code);
                return false;
            };
            bool multiOptionsOk = true;
#if LIBCURL_VERSION_NUM >= 0x072B00 && LIBCURL_VERSION_NUM < 0x073E00
            multiOptionsOk = setMultiOption(CURLMOPT_PIPELINING, static_cast<long>(CURLPIPE_MULTIPLEX), "CURLMOPT_PIPELINING");
#endif
#if LIBCURL_VERSION_NUM >= 0x071E00
            if (multiOptionsOk && clientOptions.maxTotalConnections > 0)
                multiOptionsOk = setMultiOption(CURLMOPT_MAX_TOTAL_CONNECTIONS, clientOptions.maxTotalConnections, "CURLMOPT_MAX_TOTAL_CONNECTIONS");
            if (multiOptionsOk && clientOptions.maxHostConnections > 0)
                multiOptionsOk = setMultiOption(CURLMOPT_MAX_HOST_CONNECTIONS, clientOptions.maxHostConnections, "CURLMOPT_MAX_HOST_CONNECTIONS");
#endif
#if LIBCURL_VERSION_NUM >= 0x074300
            if (multiOptionsOk && clientOptions.maxConcurrentStreams > 0)
                multiOptionsOk = setMultiOption(CURLMOPT_MAX_CONCURRENT_STREAMS, clientOptions.maxConcurrentStreams, "CURLMOPT_MAX_CONCURRENT_STREAMS");
#endif
            if (multiOptionsOk) {
                try {
                    // 在 AsyncHttpClient 完成构造前初始化完成分发器，避免全局/静态客户端析构时出现静态销毁顺序问题。
                    (void)CompletionDispatcher();
                    authWorker = std::thread([this] { AuthLoop(); });
                    try {
                        worker = std::thread([this] { WorkerLoop(); });
                    } catch (...) {
                        {
                            std::lock_guard<std::mutex> lock(authMutex);
                            authStopping = true;
                        }
                        authCv.notify_all();
                        if (authWorker.joinable()) authWorker.join();
                        throw;
                    }
                } catch (const std::exception& ex) {
                    initializationError = std::string("创建 AsyncHttpClient 工作线程失败: ") + ex.what();
                } catch (...) {
                    initializationError = "创建 AsyncHttpClient 工作线程失败: 未知异常";
                }
            }
        }
        if (!initializationError.empty()) {
            if (multi) { curl_multi_cleanup(multi); multi = nullptr; }
            if (cookieShare) { curl_share_cleanup(cookieShare); cookieShare = nullptr; }
        }
    }

    ~Impl() {
        // 先禁止新的逻辑请求和认证重放，再停止认证线程，最后停止 curl_multi 线程。
        // 这样认证刷新回调不会与已经销毁的 Impl 并发访问。
        shuttingDown.store(true, std::memory_order_release);
        {
            std::lock_guard<std::mutex> lock(authMutex);
            authStopping = true;
        }
        authCv.notify_all();
        if (authWorker.joinable()) authWorker.join();

        stopping.store(true, std::memory_order_release);
        cv.notify_all();
        if (worker.joinable()) worker.join();

        // 正常路径所有 easy 已先 remove；只有 multi fatal 且 remove_handle 失败时才会进入隔离区。
        // 此时先销毁 multi 栈，使这些 easy 不再可能附着于 multi，再安全回收请求级资源和 easy。
        if (multi) {
            (void)curl_multi_cleanup(multi);
            multi = nullptr;
        }
        for (auto& resources : quarantinedCurlResources) {
            if (resources.headerList) curl_slist_free_all(resources.headerList);
            if (resources.resolveList) curl_slist_free_all(resources.resolveList);
            if (resources.mime) curl_mime_free(resources.mime);
            if (resources.easy) curl_easy_cleanup(resources.easy);
        }
        quarantinedCurlResources.clear();
        if (cookieShare) {
            (void)curl_share_cleanup(cookieShare);
            cookieShare = nullptr;
        }
    }

    void QuarantineCurlResources(Operation& op) {
        QuarantinedCurlResources resources;
        resources.easy = op.easy;
        resources.headerList = op.headerList.value;
        resources.resolveList = op.resolveList.value;
        resources.mime = op.mime.value;
        op.easy = nullptr;
        op.headerList.value = nullptr;
        op.resolveList.value = nullptr;
        op.mime.value = nullptr;
        quarantinedCurlResources.push_back(resources);
    }

    bool DetachOrQuarantine(Operation& op, CURL* easy) noexcept {
        if (!easy || !multi) return true;
        const CURLMcode removeCode = curl_multi_remove_handle(multi, easy);
        if (removeCode == CURLM_OK) return true;

        // multi 状态已经不可继续使用时，宁可延迟回收资源，也不能违反 libcurl 的
        // “remove easy -> easy_cleanup”生命周期要求。
        try {
            MarkMultiFatal(removeCode, "curl_multi_remove_handle");
        } catch (...) {
            fatalMultiCode.store(static_cast<int>(removeCode), std::memory_order_release);
        }
        try {
            QuarantineCurlResources(op);
        } catch (...) {
            // 分配隔离容器失败属于进程级资源枯竭。为了避免 attached easy 的 UAF/非法 cleanup，
            // 直接释放所有权形成受控泄漏；进程退出时由 OS 回收。
            op.easy = nullptr;
            op.headerList.value = nullptr;
            op.resolveList.value = nullptr;
            op.mime.value = nullptr;
        }
        return false;
    }

    bool HasFatalMultiError() const noexcept {
        return fatalMultiCode.load(std::memory_order_acquire) != static_cast<int>(CURLM_OK);
    }

    HttpResponse MakeMultiFailure(std::string requestId = {}) const {
        HttpResponse response;
        response.success = false;
        response.curl_code = CURLE_FAILED_INIT;
        response.errorCategory = HttpErrorCategory::Internal;
        response.requestId = std::move(requestId);
        {
            std::lock_guard<std::mutex> lock(fatalMultiMutex);
            response.error = fatalMultiMessage.empty() ? "curl_multi 运行环境已失效" : fatalMultiMessage;
        }
        return response;
    }

    void MarkMultiFatal(CURLMcode code, const char* operation) {
        if (code == CURLM_OK) return;
        const int expectedOk = static_cast<int>(CURLM_OK);
        int expected = expectedOk;
        if (fatalMultiCode.compare_exchange_strong(expected, static_cast<int>(code),
                                                    std::memory_order_acq_rel,
                                                    std::memory_order_acquire)) {
            std::lock_guard<std::mutex> lock(fatalMultiMutex);
            fatalMultiMessage = std::string(operation) + " 失败，curl_multi 状态不可继续使用: " +
                                curl_multi_strerror(code);
        }
    }

    void FailAllForMultiError() {
        std::vector<std::unique_ptr<Operation>> failed;

        {
            std::lock_guard<std::mutex> lock(mutex);
            while (!incoming.empty()) {
                failed.push_back(std::move(incoming.front()));
                incoming.pop_front();
            }
        }

        for (auto& op : delayed) {
            if (op) failed.push_back(std::move(op));
        }
        delayed.clear();

        for (auto& item : active) {
            if (item.second) {
                (void)DetachOrQuarantine(*item.second, item.first);
                failed.push_back(std::move(item.second));
            }
        }
        active.clear();

        {
            std::lock_guard<std::mutex> lock(authMutex);
            while (!authQueue.empty()) {
                failed.push_back(std::move(authQueue.front()));
                authQueue.pop_front();
            }
        }

        for (auto& op : failed) {
            if (!op) continue;
            HttpResponse response = MakeMultiFailure(op->request.requestId);
            Complete(std::move(op), std::move(response));
        }
    }

    std::future<HttpResponse> ReadyFuture(HttpResponse response) {
        std::promise<HttpResponse> p;
        auto f = p.get_future();
        p.set_value(std::move(response));
        return f;
    }

    bool Preprocess(HttpRequestData& request, RequestOptions& options, HttpResponse& early, std::uint64_t& authVersion) {
        if (request.requestId.empty()) request.requestId = GenerateRequestId();
        early.requestId = request.requestId;
        if (!ValidateClientOptions(clientOptions, early)) {
            RunResponseInterceptorsSafely(clientOptions.responseInterceptors, request, options, early);
            EmitMetricsSafely(clientOptions.metricsCollector, request, options, early);
            return false;
        }

        std::size_t passed = 0;
        try {
            for (auto& interceptor : clientOptions.requestInterceptors) if (interceptor) interceptor(request, options);
            if (clientOptions.authProvider) {
                authVersion = clientOptions.authProvider->Version();
                clientOptions.authProvider->Apply(request, options);
            }
            for (const auto& m : clientOptions.middleware) {
                if (!m) { ++passed; continue; }
                if (!m->Before(request, options, early)) {
                    if (!early.success && early.curl_code == CURLE_OK && early.code == 0 && early.error.empty()) {
                        early.curl_code = CURLE_ABORTED_BY_CALLBACK;
                        early.errorCategory = HttpErrorCategory::Internal;
                        early.error = "Middleware 已终止请求，但没有提供响应或错误信息";
                    } else {
                        early.errorCategory = ClassifyError(early);
                    }
                    RunMiddlewareAfterSafely(clientOptions.middleware, passed, request, options, early);
                    RunResponseInterceptorsSafely(clientOptions.responseInterceptors, request, options, early);
                    EmitMetricsSafely(clientOptions.metricsCollector, request, options, early);
                    return false;
                }
                ++passed;
            }
            return true;
        } catch (const std::exception& ex) {
            SetExtensionException(early, std::string("异步请求预处理扩展回调抛出异常: ") + ex.what());
        } catch (...) {
            SetExtensionException(early, "异步请求预处理扩展回调抛出未知异常");
        }
        RunMiddlewareAfterSafely(clientOptions.middleware, passed, request, options, early);
        RunResponseInterceptorsSafely(clientOptions.responseInterceptors, request, options, early);
        EmitMetricsSafely(clientOptions.metricsCollector, request, options, early);
        return false;
    }

    std::future<HttpResponse> Submit(HttpRequestData request, RequestOptions options) {
        if (request.requestId.empty()) request.requestId = GenerateRequestId();
        if (shuttingDown.load(std::memory_order_acquire)) {
            HttpResponse r; r.requestId=request.requestId; r.curl_code=CURLE_ABORTED_BY_CALLBACK; r.error="AsyncHttpClient 正在关闭"; r.errorCategory=HttpErrorCategory::Cancelled;
            return ReadyFuture(std::move(r));
        }
        if (!multi || !cookieShare) { HttpResponse r; r.requestId=request.requestId; r.curl_code=CURLE_FAILED_INIT; r.error=initializationError.empty() ? "curl_multi 或 Cookie Share 初始化失败" : initializationError; r.errorCategory=HttpErrorCategory::Internal; return ReadyFuture(std::move(r)); }
        if (HasFatalMultiError()) return ReadyFuture(MakeMultiFailure(request.requestId));
        HttpResponse early;
        std::uint64_t authVersion = 0;
        if (!Preprocess(request, options, early, authVersion)) return ReadyFuture(std::move(early));
        auto op = std::make_unique<Operation>(std::move(request), std::move(options));
        op->authVersion = authVersion;
        auto f = op->promise.get_future();
        pendingCount->fetch_add(1, std::memory_order_relaxed);
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (shuttingDown.load(std::memory_order_acquire) || HasFatalMultiError()) {
                pendingCount->fetch_sub(1, std::memory_order_relaxed);
                HttpResponse rejected = shuttingDown.load(std::memory_order_acquire)
                    ? HttpResponse{}
                    : MakeMultiFailure(op->request.requestId);
                if (shuttingDown.load(std::memory_order_acquire)) {
                    rejected.curl_code = CURLE_ABORTED_BY_CALLBACK;
                    rejected.error = "AsyncHttpClient 正在关闭";
                    rejected.errorCategory = HttpErrorCategory::Cancelled;
                }
                try { op->promise.set_value(std::move(rejected)); } catch (...) {}
                return f;
            }
            incoming.push_back(std::move(op));
        }
        cv.notify_one();
        return f;
    }

    void SubmitCallback(HttpRequestData request, RequestOptions options, AsyncCompletionCallback callback) {
        if (!callback) return;
        if (request.requestId.empty()) request.requestId = GenerateRequestId();
        if (shuttingDown.load(std::memory_order_acquire)) {
            HttpResponse r; r.requestId=request.requestId; r.curl_code=CURLE_ABORTED_BY_CALLBACK; r.error="AsyncHttpClient 正在关闭"; r.errorCategory=HttpErrorCategory::Cancelled;
            DispatchAsyncCompletion(std::move(callback), std::move(r)); return;
        }
        if (!multi || !cookieShare) { HttpResponse r; r.requestId=request.requestId; r.curl_code=CURLE_FAILED_INIT; r.error=initializationError.empty() ? "curl_multi 或 Cookie Share 初始化失败" : initializationError; r.errorCategory=HttpErrorCategory::Internal; DispatchAsyncCompletion(std::move(callback), std::move(r)); return; }
        if (HasFatalMultiError()) { DispatchAsyncCompletion(std::move(callback), MakeMultiFailure(request.requestId)); return; }
        HttpResponse early;
        std::uint64_t authVersion = 0;
        if (!Preprocess(request, options, early, authVersion)) { DispatchAsyncCompletion(std::move(callback), std::move(early)); return; }
        auto op = std::make_unique<Operation>(std::move(request), std::move(options), std::move(callback));
        op->authVersion = authVersion;
        pendingCount->fetch_add(1, std::memory_order_relaxed);
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (shuttingDown.load(std::memory_order_acquire) || HasFatalMultiError()) {
                pendingCount->fetch_sub(1, std::memory_order_relaxed);
                HttpResponse rejected = shuttingDown.load(std::memory_order_acquire)
                    ? HttpResponse{}
                    : MakeMultiFailure(op->request.requestId);
                if (shuttingDown.load(std::memory_order_acquire)) {
                    rejected.curl_code = CURLE_ABORTED_BY_CALLBACK;
                    rejected.error = "AsyncHttpClient 正在关闭";
                    rejected.errorCategory = HttpErrorCategory::Cancelled;
                }
                auto rejectedCallback = std::move(op->completion);
                DispatchAsyncCompletion(std::move(rejectedCallback), std::move(rejected));
                return;
            }
            incoming.push_back(std::move(op));
        }
        cv.notify_one();
    }

    void ReleaseAdmissionForAuthPause(Operation& op) {
        if (op.rateHeld && rateLimiter) {
            rateLimiter->Release();
            op.rateHeld = false;
        }
        if (op.circuitAdmitted && circuitBreaker) {
            if (op.transferStarted) {
                circuitBreaker->Record(op.response, op.circuitHalfOpenAdmission);
            } else {
                circuitBreaker->Cancel(op.circuitHalfOpenAdmission);
            }
            op.circuitAdmitted = false;
            op.circuitHalfOpenAdmission = false;
        }
        op.transferStarted = false;
    }

    void QueueAuthRefresh(std::unique_ptr<Operation> op) {
        if (!op) return;

        // Cookie Share 官方不支持多个并发线程共同使用。认证刷新会把 Operation
        // 从 curl_multi 网络线程转交给认证线程，因此必须先在网络线程解除 Share 绑定。
        // 重放时 Prepare() 会重新绑定同一个 Share，Cookie 数据本身仍保留在 Share 中。
        if (op->easy && op->options.enableCookieEngine) {
            const CURLcode detachCode = curl_easy_setopt(op->easy, CURLOPT_SHARE, nullptr);
            if (detachCode != CURLE_OK) {
                HttpResponse response = std::move(op->response);
                response.success = false;
                response.curl_code = detachCode;
                response.errorCategory = HttpErrorCategory::Internal;
                response.error = std::string("认证刷新前解除 Cookie Share 绑定失败: ") +
                                 curl_easy_strerror(detachCode);
                Complete(std::move(op), std::move(response));
                return;
            }
        }

        if (HasFatalMultiError()) {
            HttpResponse r = MakeMultiFailure(op->request.requestId);
            Complete(std::move(op), std::move(r));
            return;
        }
        ReleaseAdmissionForAuthPause(*op);
        if (shuttingDown.load(std::memory_order_acquire)) {
            HttpResponse r = std::move(op->response);
            r.success = false;
            r.curl_code = CURLE_ABORTED_BY_CALLBACK;
            r.errorCategory = HttpErrorCategory::Cancelled;
            r.error = "AsyncHttpClient 正在关闭，已取消认证刷新";
            Complete(std::move(op), std::move(r));
            return;
        }
        {
            std::lock_guard<std::mutex> lock(authMutex);
            if (authStopping) {
                HttpResponse r = std::move(op->response);
                r.success = false;
                r.curl_code = CURLE_ABORTED_BY_CALLBACK;
                r.errorCategory = HttpErrorCategory::Cancelled;
                r.error = "AsyncHttpClient 认证线程正在关闭";
                Complete(std::move(op), std::move(r));
                return;
            }
            authQueue.push_back(std::move(op));
        }
        authCv.notify_one();
    }

    void HandleAuthRefresh(std::unique_ptr<Operation> op) {
        if (!op) return;
        if (HasFatalMultiError()) {
            HttpResponse r = MakeMultiFailure(op->request.requestId);
            Complete(std::move(op), std::move(r));
            return;
        }
        if (shuttingDown.load(std::memory_order_acquire)) {
            HttpResponse r = std::move(op->response);
            r.success = false;
            r.curl_code = CURLE_ABORTED_BY_CALLBACK;
            r.errorCategory = HttpErrorCategory::Cancelled;
            r.error = "AsyncHttpClient 正在关闭，已取消认证刷新";
            Complete(std::move(op), std::move(r));
            return;
        }

        try {
            bool refreshed = clientOptions.authProvider &&
                             clientOptions.authProvider->Version() != op->authVersion;
            if (!refreshed && clientOptions.authProvider) {
                refreshed = clientOptions.authProvider->Refresh();
            }
            if (!refreshed) {
                HttpResponse finalResponse = std::move(op->response);
                Complete(std::move(op), std::move(finalResponse));
                return;
            }

            if (!PrepareExternalSinkForLogicalReplay(op->options, op->response)) {
                HttpResponse finalResponse = std::move(op->response);
                Complete(std::move(op), std::move(finalResponse));
                return;
            }

            op->authReplayUsed = true;
            op->authVersion = clientOptions.authProvider->Version();
            clientOptions.authProvider->Apply(op->request, op->options);
            op->retryIndex = 0;
            op->readyAt = std::chrono::steady_clock::now();

            if (HasFatalMultiError()) {
                HttpResponse r = MakeMultiFailure(op->request.requestId);
                Complete(std::move(op), std::move(r));
                return;
            }
            if (shuttingDown.load(std::memory_order_acquire)) {
                HttpResponse r = std::move(op->response);
                r.success = false;
                r.curl_code = CURLE_ABORTED_BY_CALLBACK;
                r.errorCategory = HttpErrorCategory::Cancelled;
                r.error = "AsyncHttpClient 正在关闭，已取消认证重放";
                Complete(std::move(op), std::move(r));
                return;
            }

            {
                std::lock_guard<std::mutex> lock(mutex);
                if (shuttingDown.load(std::memory_order_acquire) || HasFatalMultiError()) {
                    HttpResponse rejected = shuttingDown.load(std::memory_order_acquire)
                        ? HttpResponse{}
                        : MakeMultiFailure(op->request.requestId);
                    if (shuttingDown.load(std::memory_order_acquire)) {
                        rejected.curl_code = CURLE_ABORTED_BY_CALLBACK;
                        rejected.error = "AsyncHttpClient 正在关闭，已取消认证重放";
                        rejected.errorCategory = HttpErrorCategory::Cancelled;
                    }
                    Complete(std::move(op), std::move(rejected));
                    return;
                }
                incoming.push_back(std::move(op));
            }
            cv.notify_one();
        } catch (const std::exception& ex) {
            op->response.success = false;
            op->response.curl_code = CURLE_ABORTED_BY_CALLBACK;
            op->response.errorCategory = HttpErrorCategory::Internal;
            op->response.error = std::string("异步认证扩展回调抛出异常: ") + ex.what();
            HttpResponse finalResponse = std::move(op->response);
            Complete(std::move(op), std::move(finalResponse));
        } catch (...) {
            op->response.success = false;
            op->response.curl_code = CURLE_ABORTED_BY_CALLBACK;
            op->response.errorCategory = HttpErrorCategory::Internal;
            op->response.error = "异步认证扩展回调抛出未知异常";
            HttpResponse finalResponse = std::move(op->response);
            Complete(std::move(op), std::move(finalResponse));
        }
    }

    void AuthLoop() noexcept {
        for (;;) {
            std::unique_ptr<Operation> op;
            {
                std::unique_lock<std::mutex> lock(authMutex);
                authCv.wait(lock, [&] { return authStopping || !authQueue.empty(); });
                if (authStopping && authQueue.empty()) return;
                op = std::move(authQueue.front());
                authQueue.pop_front();
            }
            HandleAuthRefresh(std::move(op));
        }
    }

    bool Prepare(Operation& op) {
        op.transferStarted = false;
        if (!op.easy) { op.response.curl_code=CURLE_FAILED_INIT; op.response.error="curl_easy_init 失败"; return false; }
        op.ResetHolders();
        curl_easy_reset(op.easy);
        if (op.options.enableCookieEngine &&
            curl_easy_setopt(op.easy, CURLOPT_SHARE, cookieShare) != CURLE_OK) {
            op.response.curl_code = CURLE_FAILED_INIT;
            op.response.error = "为异步请求绑定 Cookie Share 失败";
            op.response.errorCategory = HttpErrorCategory::Internal;
            return false;
        }
        op.response = {};
        op.response.requestId = op.request.requestId;
        op.context = {};
        op.context.response = &op.response;
        op.context.options = &op.options;
        op.errorBuffer.fill('\0');
        if (op.totalAttempts > 0 && op.options.uploadSource) {
            try {
                if (!op.options.uploadSource->Rewind()) {
                    op.response.curl_code = CURLE_SEND_FAIL_REWIND;
                    op.response.error = "流式上传源无法回退，不能继续重试";
                    return false;
                }
            } catch (const std::exception& ex) {
                op.response.curl_code = CURLE_SEND_FAIL_REWIND;
                op.response.error = std::string("流式上传源回退时抛出异常: ") + ex.what();
                return false;
            } catch (...) {
                op.response.curl_code = CURLE_SEND_FAIL_REWIND;
                op.response.error = "流式上传源回退时抛出未知异常";
                return false;
            }
        }
        if (!ConfigureRequest(op.easy, op.request.method, op.request.url, op.request.body, op.options,
                              op.context, op.headerList, op.resolveList, op.mime, op.errorBuffer, op.response)) return false;
        ++op.totalAttempts;
        op.response.attempts = op.totalAttempts;
        const CURLcode freshCode = curl_easy_setopt(op.easy, CURLOPT_FRESH_CONNECT, op.totalAttempts > 1 ? 1L : 0L);
        if (freshCode != CURLE_OK) {
            op.response.curl_code = freshCode;
            op.response.errorCategory = HttpErrorCategory::Internal;
            op.response.error = std::string("设置 CURLOPT_FRESH_CONNECT 失败: ") + curl_easy_strerror(freshCode);
            return false;
        }
        return true;
    }

    void Complete(std::unique_ptr<Operation> op, HttpResponse response) {
        if (!op) return;
        if (op->rateHeld && rateLimiter) {
            rateLimiter->Release();
            op->rateHeld = false;
        }
        response.requestId = op->request.requestId;
        response.errorCategory = ClassifyError(response);
        if (op->circuitAdmitted && circuitBreaker) {
            if (op->transferStarted) {
                circuitBreaker->Record(response, op->circuitHalfOpenAdmission);
            } else {
                circuitBreaker->Cancel(op->circuitHalfOpenAdmission);
            }
            op->circuitAdmitted = false;
        }

        // 普通 future 请求若没有任何完成后扩展，直接兑现 promise。
        // 这样高并发基础请求不会无意义进入完成线程池并堆积队列。
        if (!op->completion && clientOptions.middleware.empty() &&
            clientOptions.responseInterceptors.empty() &&
            !clientOptions.metricsCollector && !op->options.debug.enabled) {
            // 先移出 Pending 集合，再兑现 future；这样 future.get() 返回后 PendingCount 不会仍短暂包含该请求。
            pendingCount->fetch_sub(1, std::memory_order_release);
            try { op->promise.set_value(std::move(response)); } catch (...) {}
            return;
        }

        auto middleware = clientOptions.middleware;
        auto interceptors = clientOptions.responseInterceptors;
        auto metrics = clientOptions.metricsCollector;
        auto request = std::move(op->request);
        auto options = std::move(op->options);
        auto completion = std::move(op->completion);
        auto counter = pendingCount;
        std::shared_ptr<std::promise<HttpResponse>> promise;
        if (!completion) {
            promise = std::make_shared<std::promise<HttpResponse>>(std::move(op->promise));
        }

        std::function<void()> task =
            [middleware = std::move(middleware),
             interceptors = std::move(interceptors),
             metrics = std::move(metrics),
             request = std::move(request),
             options = std::move(options),
             response = std::move(response),
             completion = std::move(completion),
             promise = std::move(promise),
             counter = std::move(counter)]() mutable {
                RunMiddlewareAfterSafely(middleware, middleware.size(), request, options, response);
                RunResponseInterceptorsSafely(interceptors, request, options, response);
                EmitMetricsSafely(metrics, request, options, response);
                LogResponseDebug(request, options, response);

                if (completion) {
                    counter->fetch_sub(1, std::memory_order_relaxed);
                    try { completion(std::move(response)); } catch (...) {}
                } else {
                    // 与 callback 路径保持同一“交付边界”：先从 PendingCount 移除，再令 future 就绪。
                    counter->fetch_sub(1, std::memory_order_release);
                    try { promise->set_value(std::move(response)); } catch (...) {}
                }
            };

        try {
            CompletionDispatcher().Post(task);
        } catch (...) {
            // 极端资源不足或进程退出阶段无法入队时，同步执行作为最后兜底。
            try { task(); } catch (...) {}
        }
    }

    void QueueDelay(std::unique_ptr<Operation> op, std::chrono::milliseconds delay) {
        const auto now = std::chrono::steady_clock::now();
        const auto maxDelay = std::chrono::duration_cast<std::chrono::milliseconds>(
            (std::chrono::steady_clock::time_point::max)() - now);
        op->readyAt = delay >= maxDelay
            ? (std::chrono::steady_clock::time_point::max)()
            : now + delay;
        delayed.push_back(std::move(op));
    }

    void FinalizeTransfer(std::unique_ptr<Operation> op, CURLcode result) {
        if (!op) return;
        op->response.curl_code = result;
        op->response.success = (result == CURLE_OK);
        CollectInfo(op->easy, op->response);
        FinalizeHeaderResult(op->context, op->response);
        ApplyRedirectSafetyRefusal(op->easy, op->request.method, op->request.body, op->options, op->response);
        if (result != CURLE_OK && op->response.proxyConnectCode != 0 && op->response.headerHistory.empty()) {
            op->response.code = 0;
        }
        op->response.attempts = op->totalAttempts;
        if (op->response.curl_code != CURLE_OK) {
            if (!op->context.callbackError.empty()) {
                op->response.error = op->context.callbackError;
                if (op->context.callbackErrorCategory != HttpErrorCategory::None) {
                    op->response.errorCategory = op->context.callbackErrorCategory;
                }
            } else if (op->errorBuffer[0] != '\0') op->response.error = op->errorBuffer.data();
            else if (op->response.error.empty()) op->response.error = curl_easy_strerror(op->response.curl_code);
        }

        try {
            if (!op->authReplayUsed &&
                clientOptions.authProvider &&
                clientOptions.refreshAuthOnUnauthorized &&
                clientOptions.authProvider->CanRefresh(op->response)) {
                QueueAuthRefresh(std::move(op));
                return;
            }
        } catch (const std::exception& ex) {
            op->response.success = false;
            op->response.curl_code = CURLE_ABORTED_BY_CALLBACK;
            op->response.errorCategory = HttpErrorCategory::Internal;
            op->response.error = std::string("异步认证判定回调抛出异常: ") + ex.what();
            HttpResponse finalResponse = std::move(op->response);
            Complete(std::move(op), std::move(finalResponse));
            return;
        } catch (...) {
            op->response.success = false;
            op->response.curl_code = CURLE_ABORTED_BY_CALLBACK;
            op->response.errorCategory = HttpErrorCategory::Internal;
            op->response.error = "异步认证判定回调抛出未知异常";
            HttpResponse finalResponse = std::move(op->response);
            Complete(std::move(op), std::move(finalResponse));
            return;
        }
        if (ShouldRetry(op->request.method, op->options.retry, op->response, op->retryIndex)) {
            if (!PrepareExternalResponseSinkForRetry(op->options, op->context, op->response)) {
                HttpResponse finalResponse = std::move(op->response);
                Complete(std::move(op), std::move(finalResponse));
                return;
            }
            const auto delay = ComputeRetryDelay(op->options.retry, op->retryIndex + 1, op->response);
            ++op->retryIndex;
            QueueDelay(std::move(op), delay);
            return;
        }
        HttpResponse finalResponse = std::move(op->response);
        Complete(std::move(op), std::move(finalResponse));
    }

    void StartReadyOperations() {
        std::deque<std::unique_ptr<Operation>> local;
        { std::lock_guard<std::mutex> lock(mutex); local.swap(incoming); }
        while (!local.empty()) { delayed.push_back(std::move(local.front())); local.pop_front(); }
        const auto now = std::chrono::steady_clock::now();
        for (std::size_t i=0; i<delayed.size();) {
            auto& op = delayed[i];
            if (op->readyAt > now) { ++i; continue; }
            if (op->options.cancelFlag && op->options.cancelFlag->load(std::memory_order_relaxed)) {
                HttpResponse r; r.curl_code=CURLE_ABORTED_BY_CALLBACK; r.error="请求已取消"; r.errorCategory=HttpErrorCategory::Cancelled;
                auto done=std::move(op); delayed.erase(delayed.begin()+static_cast<std::ptrdiff_t>(i)); Complete(std::move(done),std::move(r)); continue;
            }
            if (!op->rateHeld) {
                if (rateLimiter && !rateLimiter->TryAcquire()) { op->readyAt=now+std::chrono::milliseconds(10); ++i; continue; }
                op->rateHeld = static_cast<bool>(rateLimiter);
            }
            if (!op->circuitAdmitted) {
                if (circuitBreaker && !circuitBreaker->Allow(op->circuitHalfOpenAdmission)) {
                    HttpResponse r; r.curl_code=CURLE_OK; r.error="Circuit Breaker 当前处于熔断状态"; r.errorCategory=HttpErrorCategory::CircuitOpen;
                    auto done=std::move(op); delayed.erase(delayed.begin()+static_cast<std::ptrdiff_t>(i)); Complete(std::move(done),std::move(r)); continue;
                }
                op->circuitAdmitted=true;
            }
            if (!Prepare(*op)) {
                auto done = std::move(op); HttpResponse response = std::move(done->response); delayed.erase(delayed.begin() + static_cast<std::ptrdiff_t>(i)); Complete(std::move(done), std::move(response)); continue;
            }
            LogRequestDebug(op->request, op->options);
            CURL* easy=op->easy;
            CURLMcode mc=curl_multi_add_handle(multi,easy);
            if (mc!=CURLM_OK) {
                HttpResponse r=std::move(op->response); r.curl_code=CURLE_FAILED_INIT; r.error=curl_multi_strerror(mc);
                auto done=std::move(op); delayed.erase(delayed.begin()+static_cast<std::ptrdiff_t>(i)); Complete(std::move(done),std::move(r)); continue;
            }
            op->transferStarted = true;
            active.emplace(easy,std::move(op));
            delayed.erase(delayed.begin()+static_cast<std::ptrdiff_t>(i));
        }
    }

    void ProcessMessages() {
        int left=0;
        while (CURLMsg* msg=curl_multi_info_read(multi,&left)) {
            if (msg->msg!=CURLMSG_DONE) continue;
            CURL* easy=msg->easy_handle;
            auto it=active.find(easy);
            if (it==active.end()) continue;
            const CURLMcode removeCode = curl_multi_remove_handle(multi, easy);
            if (removeCode != CURLM_OK) {
                MarkMultiFatal(removeCode, "curl_multi_remove_handle");
                return;
            }
            auto op = std::move(it->second);
            active.erase(it);
            FinalizeTransfer(std::move(op), msg->data.result);
        }
    }

    void CancelAll() {
        std::deque<std::unique_ptr<Operation>> queued;
        { std::lock_guard<std::mutex> lock(mutex); queued.swap(incoming); }
        for (auto& op: queued) { HttpResponse r; r.curl_code=CURLE_ABORTED_BY_CALLBACK; r.error="AsyncHttpClient 正在关闭"; r.errorCategory=HttpErrorCategory::Cancelled; Complete(std::move(op),std::move(r)); }
        for (auto& op: delayed) { HttpResponse r; r.curl_code=CURLE_ABORTED_BY_CALLBACK; r.error="AsyncHttpClient 正在关闭"; r.errorCategory=HttpErrorCategory::Cancelled; Complete(std::move(op),std::move(r)); }
        delayed.clear();
        std::vector<std::unique_ptr<Operation>> ops;
        for (auto& item : active) {
            if (!item.second) continue;
            (void)DetachOrQuarantine(*item.second, item.first);
            ops.push_back(std::move(item.second));
        }
        active.clear();
        for (auto& op: ops) { HttpResponse r; r.curl_code=CURLE_ABORTED_BY_CALLBACK; r.error="AsyncHttpClient 正在关闭"; r.errorCategory=HttpErrorCategory::Cancelled; Complete(std::move(op),std::move(r)); }
    }

    void WorkerLoop() {
        int running = 0;
        while (!stopping.load(std::memory_order_acquire)) {
            StartReadyOperations();
            if (HasFatalMultiError()) break;

            CURLMcode performCode;
            do {
                performCode = curl_multi_perform(multi, &running);
            } while (performCode == CURLM_CALL_MULTI_PERFORM);

            if (performCode != CURLM_OK) {
                MarkMultiFatal(performCode, "curl_multi_perform");
                FailAllForMultiError();
                break;
            }

            ProcessMessages();
            if (HasFatalMultiError()) {
                FailAllForMultiError();
                break;
            }

            if (active.empty()) {
                std::unique_lock<std::mutex> lock(mutex);
                cv.wait_for(lock,
                            std::chrono::milliseconds(incoming.empty() && delayed.empty() ? 50 : 10));
            } else {
                int numfds = 0;
#if LIBCURL_VERSION_NUM >= 0x074200
                const CURLMcode waitCode = curl_multi_poll(multi, nullptr, 0, 50, &numfds);
#else
                const CURLMcode waitCode = curl_multi_wait(multi, nullptr, 0, 50, &numfds);
#endif
                if (waitCode != CURLM_OK) {
                    MarkMultiFatal(waitCode,
#if LIBCURL_VERSION_NUM >= 0x074200
                                   "curl_multi_poll"
#else
                                   "curl_multi_wait"
#endif
                    );
                    FailAllForMultiError();
                    break;
                }
            }
        }

        if (!HasFatalMultiError()) {
            CancelAll();
        }
    }
};

AsyncHttpClient::AsyncHttpClient() : impl_(std::make_unique<Impl>(RequestOptions{},HttpClientOptions{})) {}
AsyncHttpClient::AsyncHttpClient(RequestOptions defaultOptions) : impl_(std::make_unique<Impl>(std::move(defaultOptions),HttpClientOptions{})) {}
AsyncHttpClient::AsyncHttpClient(RequestOptions defaultOptions,HttpClientOptions clientOptions) : impl_(std::make_unique<Impl>(std::move(defaultOptions),std::move(clientOptions))) {}
AsyncHttpClient::~AsyncHttpClient() = default;
std::future<HttpResponse> AsyncHttpClient::SendAsync(HttpRequestData request,RequestOptions options) { return impl_->Submit(std::move(request),std::move(options)); }
std::future<HttpResponse> AsyncHttpClient::SendAsync(HttpRequestData request) { return impl_->Submit(std::move(request),impl_->defaultOptions); }
void AsyncHttpClient::SendWithCallback(HttpRequestData request,RequestOptions options,AsyncCompletionCallback callback) { impl_->SubmitCallback(std::move(request),std::move(options),std::move(callback)); }
std::future<HttpResponse> AsyncHttpClient::GetAsync(std::string_view url,RequestOptions options) { return RequestAsync("GET",url,{},std::move(options)); }
std::future<HttpResponse> AsyncHttpClient::GetAsync(std::string_view url) { return RequestAsync("GET",url,{},impl_->defaultOptions); }
std::future<HttpResponse> AsyncHttpClient::HeadAsync(std::string_view url,RequestOptions options) { return RequestAsync("HEAD",url,{},std::move(options)); }
std::future<HttpResponse> AsyncHttpClient::HeadAsync(std::string_view url) { return RequestAsync("HEAD",url,{},impl_->defaultOptions); }
std::future<HttpResponse> AsyncHttpClient::PostAsync(std::string_view url,std::string_view body,RequestOptions options) { return RequestAsync("POST",url,body,std::move(options)); }
std::future<HttpResponse> AsyncHttpClient::PostAsync(std::string_view url,std::string_view body) { return RequestAsync("POST",url,body,impl_->defaultOptions); }
std::future<HttpResponse> AsyncHttpClient::PutAsync(std::string_view url,std::string_view body,RequestOptions options) { return RequestAsync("PUT",url,body,std::move(options)); }
std::future<HttpResponse> AsyncHttpClient::PutAsync(std::string_view url,std::string_view body) { return RequestAsync("PUT",url,body,impl_->defaultOptions); }
std::future<HttpResponse> AsyncHttpClient::DeleteAsync(std::string_view url,std::string_view body,RequestOptions options) { return RequestAsync("DELETE",url,body,std::move(options)); }
std::future<HttpResponse> AsyncHttpClient::DeleteAsync(std::string_view url,std::string_view body) { return RequestAsync("DELETE",url,body,impl_->defaultOptions); }
std::future<HttpResponse> AsyncHttpClient::PatchAsync(std::string_view url,std::string_view body,RequestOptions options) { return RequestAsync("PATCH",url,body,std::move(options)); }
std::future<HttpResponse> AsyncHttpClient::PatchAsync(std::string_view url,std::string_view body) { return RequestAsync("PATCH",url,body,impl_->defaultOptions); }
std::future<HttpResponse> AsyncHttpClient::OptionsAsync(std::string_view url,RequestOptions options) { return RequestAsync("OPTIONS",url,{},std::move(options)); }
std::future<HttpResponse> AsyncHttpClient::OptionsAsync(std::string_view url) { return RequestAsync("OPTIONS",url,{},impl_->defaultOptions); }
std::future<HttpResponse> AsyncHttpClient::PostJsonAsync(std::string_view url,std::string_view json,RequestOptions options) { options.headers.SetDefaultHeader("Content-Type","application/json"); options.headers.SetDefaultHeader("Accept","application/json"); return RequestAsync("POST",url,json,std::move(options)); }
std::future<HttpResponse> AsyncHttpClient::PostJsonAsync(std::string_view url,std::string_view json) { auto o=impl_->defaultOptions; o.headers.SetDefaultHeader("Content-Type","application/json"); o.headers.SetDefaultHeader("Accept","application/json"); return RequestAsync("POST",url,json,std::move(o)); }
std::future<HttpResponse> AsyncHttpClient::PutJsonAsync(std::string_view url,std::string_view json,RequestOptions options) { options.headers.SetDefaultHeader("Content-Type","application/json"); options.headers.SetDefaultHeader("Accept","application/json"); return RequestAsync("PUT",url,json,std::move(options)); }
std::future<HttpResponse> AsyncHttpClient::PutJsonAsync(std::string_view url,std::string_view json) { auto o=impl_->defaultOptions; o.headers.SetDefaultHeader("Content-Type","application/json"); o.headers.SetDefaultHeader("Accept","application/json"); return RequestAsync("PUT",url,json,std::move(o)); }
std::future<HttpResponse> AsyncHttpClient::PatchJsonAsync(std::string_view url,std::string_view json,RequestOptions options) { options.headers.SetDefaultHeader("Content-Type","application/json"); options.headers.SetDefaultHeader("Accept","application/json"); return RequestAsync("PATCH",url,json,std::move(options)); }
std::future<HttpResponse> AsyncHttpClient::PatchJsonAsync(std::string_view url,std::string_view json) { auto o=impl_->defaultOptions; o.headers.SetDefaultHeader("Content-Type","application/json"); o.headers.SetDefaultHeader("Accept","application/json"); return RequestAsync("PATCH",url,json,std::move(o)); }
std::future<HttpResponse> AsyncHttpClient::RequestAsync(std::string_view method,std::string_view url,std::string_view body,RequestOptions options) { HttpRequestData r; r.method.assign(method.data(),method.size()); r.url.assign(url.data(),url.size()); r.body.assign(body.data(),body.size()); return impl_->Submit(std::move(r),std::move(options)); }
std::future<HttpResponse> AsyncHttpClient::RequestAsync(std::string_view method,std::string_view url,std::string_view body) { return RequestAsync(method,url,body,impl_->defaultOptions); }
std::size_t AsyncHttpClient::PendingCount() const { return impl_->pendingCount->load(std::memory_order_relaxed); }

#if CURL_EX_HAS_COROUTINES
struct HttpAwaitable::State {
    std::recursive_mutex mutex;
    std::optional<HttpResponse> response;
    std::coroutine_handle<> handle{};

    void Complete(HttpResponse value) {
        // 在同一个递归互斥区内取走句柄并恢复协程，使正常 Awaitable 析构/移动注销与完成恢复串行化。
        // 这只保护本封装管理的注册句柄生命周期；调用方若从其他线程直接 destroy coroutine frame，
        // 仍必须自行与可能发生的 resume() 做同步，不能把该互斥量视为外部 frame 生命周期锁。
        std::lock_guard<std::recursive_mutex> lock(mutex);
        response = std::move(value);
        const std::coroutine_handle<> resumeHandle = handle;
        handle = {};
        if (resumeHandle) {
            resumeHandle.resume();
        }
    }

    void Unregister() noexcept {
        try {
            std::lock_guard<std::recursive_mutex> lock(mutex);
            handle = {};
        } catch (...) {
        }
    }
};

HttpAwaitable::HttpAwaitable(std::shared_ptr<State> state) : state_(std::move(state)) {}
HttpAwaitable::~HttpAwaitable() {
    if (state_) state_->Unregister();
}
HttpAwaitable::HttpAwaitable(HttpAwaitable&& other) noexcept
    : state_(std::move(other.state_)) {}
HttpAwaitable& HttpAwaitable::operator=(HttpAwaitable&& other) noexcept {
    if (this == &other) return *this;
    if (state_) state_->Unregister();
    state_ = std::move(other.state_);
    return *this;
}
bool HttpAwaitable::await_ready() const {
    std::lock_guard<std::recursive_mutex> lock(state_->mutex);
    return state_->response.has_value();
}
bool HttpAwaitable::await_suspend(std::coroutine_handle<> handle) {
    std::lock_guard<std::recursive_mutex> lock(state_->mutex);
    if (state_->response) return false;
    state_->handle = handle;
    return true;
}
HttpResponse HttpAwaitable::await_resume() {
    std::lock_guard<std::recursive_mutex> lock(state_->mutex);
    if (!state_->response) {
        HttpResponse response;
        response.success = false;
        response.curl_code = CURLE_ABORTED_BY_CALLBACK;
        response.errorCategory = HttpErrorCategory::Internal;
        response.error = "协程在 HTTP 结果尚未就绪时被恢复";
        return response;
    }
    return std::move(*state_->response);
}
HttpAwaitable GetAwaitable(AsyncHttpClient& client,std::string_view url,RequestOptions options) {
    auto state=std::make_shared<HttpAwaitable::State>();
    HttpRequestData r; r.method="GET"; r.url.assign(url.data(),url.size());
    client.SendWithCallback(std::move(r),std::move(options),[state](HttpResponse response){ state->Complete(std::move(response)); });
    return HttpAwaitable(std::move(state));
}
HttpAwaitable PostAwaitable(AsyncHttpClient& client,std::string_view url,std::string_view body,RequestOptions options) {
    auto state=std::make_shared<HttpAwaitable::State>();
    HttpRequestData r; r.method="POST"; r.url.assign(url.data(),url.size()); r.body.assign(body.data(),body.size());
    client.SendWithCallback(std::move(r),std::move(options),[state](HttpResponse response){ state->Complete(std::move(response)); });
    return HttpAwaitable(std::move(state));
}
#endif

} // 命名空间 ytpp::curl_ex
