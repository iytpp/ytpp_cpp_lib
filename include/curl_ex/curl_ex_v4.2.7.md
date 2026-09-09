# curl_ex v4.2.7 使用手册

> 本文档只对应本次 V4.2.7 交付的以下三个文件：
>
> - `curl_ex.h`（内部 `kCurlExVersion == "4.2.7"`）
> - `curl_ex.cpp`（源码头部版本注释为 4.2.7）
> - `curl_ex_v4.2.7.md`
>
> **版本管理规则：源码文件名保持项目现有的 `curl_ex.h / curl_ex.cpp` 不变，实际版本以源码内版本常量/注释与本说明文档为准。不要把旧版同名源码覆盖到 V4.2.7 上。**

---

## 1. 项目定位

## 1.1 V4.2.7 重要修复与生产语义

V4.2.7 是在 V4.2.6 基础上的**定向 TLS Trust Store 修复版**。本次不重构现有 `HttpRequest / HttpClient / AsyncHttpClient / CurlTransport` 架构，不改变请求、重试、重定向、认证、Cookie、下载、异步等既有设计，只处理 Windows + MultiSSL/OpenSSL 证书信任来源这一条关键问题，并把已经存在于源码中的 `HttpResponse::date` 正式补入文档。

本次 V4.2.7 实际修改如下：

- **Windows 默认启用 Native CA Store**：`TlsOptions` 新增 `useNativeCa`。Windows 默认 `true`；当 **Windows + 当前激活 Backend 为 OpenSSL**、`verifyPeer=true`、没有显式 `caFile/caPath` 且运行时 libcurl >= 7.71.0 时，封装把 `CURLSSLOPT_NATIVE_CA` 合并进 `CURLOPT_SSL_OPTIONS`。这样 OpenSSL Backend 可以继续负责 TLS，同时使用 Windows Certificate Store 作为可信根来源，不再依赖偶然存在的 `cacert.pem`、当前目录、PATH 或第三方软件附带的 CA Bundle。
- **非 Windows 不改变原有 CA 策略**：`useNativeCa` 默认 `false`，继续使用当前 libcurl/TLS Backend 的既有 Trust Store 配置。
- **显式自定义 CA 优先**：只要设置 `tls.caFile` 或 `tls.caPath`，V4.2.7 就**不再额外设置** `CURLSSLOPT_NATIVE_CA`。原因是 libcurl 的 Native CA 与其他 CA 来源本来是叠加关系；封装主动互斥可以避免调用方本来想通过自定义 CA 收窄信任集合，却被系统根证书再次扩大。注意：各 TLS Backend 对 `CAINFO/CAPATH` 的最终实现仍有差异，尤其 Schannel 自身就是 Windows 原生 TLS Backend。
- **证书验证仍然 fail-closed**：`verifyPeer=true` 仍对应 `CURLOPT_SSL_VERIFYPEER=1`；`verifyHost=true` 仍对应 `CURLOPT_SSL_VERIFYHOST=2`。本次没有加入任何“失败后关闭证书验证再重试”的逻辑。
- **Public Key Pinning 完全保留且独立执行**：`tls.pinnedPublicKey` 仍对应 `CURLOPT_PINNEDPUBLICKEY`。Native CA 只解决“证书链信任根从哪里来”，Pinning 继续作为第二层服务器身份约束；两者不是互相替代关系。
- **统一合并 `CURLOPT_SSL_OPTIONS` bitmask**：Schannel 的 `CURLSSLOPT_REVOKE_BEST_EFFORT / CURLSSLOPT_NO_REVOKE` 与 Windows Native CA 不再分别设置同一个 option，而是先构造一个 `sslOptions` bitmask，再一次性设置到 `CURLOPT_SSL_OPTIONS`；HTTPS Proxy 同样复用该 bitmask，避免后设置覆盖前设置。
- **修复 MultiSSL Backend 误判**：旧实现只要 `curl_version_info()->ssl_version` 字符串中出现 `Schannel` 就认为当前 Backend 是 Schannel。对 `OpenSSL/3.6.1 (Schannel)` 这种 MultiSSL 输出会误判。V4.2.7 按 libcurl 约定只读取**括号外的当前激活 Backend**，因此 `OpenSSL/3.6.1 (Schannel)` 正确识别为 OpenSSL，`(OpenSSL/3.6.1) Schannel` 才识别为 Schannel。
- **增加 TLS Debug 诊断**：`debug.enabled=true` 时，真实 CurlTransport 配置阶段会输出 curl 版本、完整 SSL 字符串、当前激活 Backend、verifyPeer/verifyHost、Native CA 是否请求/实际应用、自定义 `caFile/caPath`、Pinning 是否启用，以及 libcurl 7.84.0+ 可查询到的默认 `CAINFO/CAPATH`。不会输出 Pin 值、客户端私钥、私钥密码或 Token。
- **正式文档化 `HttpResponse::date`**：该字段已经存在于你提供的 4.2.6 源码中，本次不改变其实现，只把它纳入完整响应模型、默认行为和使用说明。它表示**最终响应 Header 的 `Date`**；Header 缺失或无法按 HTTP-date 解析时为 `std::nullopt`。
- **不新增全局 Backend 配置对象**：问题说明中建议过 `TlsBackend / CurlGlobalOptions`。本次为了保持现有架构和语义不变，没有引入这一层。需要显式切换 MultiSSL Backend 时，仍可在任何 `curl_ex::Initialize()` / `curl_easy_init()` / 请求发生之前直接调用 `curl_global_sslset(...)`，然后再调用 `Initialize()`。

### V4.2.7 默认 Windows TLS 决策表

| 条件 | Native CA bit | 自定义 CA | 结果语义 |
|---|---:|---:|---|
| `verifyPeer=true`，未设置 `caFile/caPath`，`useNativeCa=true`，libcurl >= 7.71.0 | 开 | 无 | Windows 系统 CA Store 参与验证；OpenSSL 不再依赖外部 CA Bundle |
| `verifyPeer=true`，设置 `caFile` 或 `caPath` | 关 | 有 | 不主动叠加 Native CA；按显式 CA 和当前 Backend 规则验证 |
| `useNativeCa=false`，未设置自定义 CA | 关 | 无 | 完全交给 libcurl/OpenSSL 构建时或运行环境的默认 Trust Store；若 `CAINFO/CAPATH` 为空，可能出现 error 20 |
| `verifyPeer=false` | 不需要 | 任意 | 证书链验证被调用方显式关闭；不推荐生产使用 |
| Schannel Backend | 不设置 Native CA bit | Backend 相关 | Schannel 本身使用 Windows Certificate Store；吊销策略仍按原逻辑合并 |

### libcurl 版本要求与 2026 Native CA 安全说明

- `CURLSSLOPT_NATIVE_CA` 在 Windows + OpenSSL 从 libcurl **7.71.0** 起支持。V4.2.7 仍保持封装最低 libcurl 7.56.0 的编译兼容：7.56.0～7.70.x 上不会设置该 bit，因此这些旧版本如果自身没有有效 CA Trust Store，仍需要升级 libcurl 或显式配置 CA。
- `CURLINFO_CAINFO / CURLINFO_CAPATH` 从 libcurl **7.84.0** 起可用于诊断默认 CA 路径；更旧版本只是少两项 Debug 信息，不影响请求本身。
- curl 官方 2026-06-24 公告 CVE-2026-11564 涉及 Native CA 与连接复用场景，官方列出的受影响版本为 8.17.0～8.20.0，8.21.0 及以上不受影响。你的当前运行版本是 8.21.0，因此已经包含官方修复。若以后降级或换运行库，不建议在 8.17.0～8.20.0 上复用 easy handle 并在 Native CA / 自定义 CA 信任策略之间切换。

官方参考：

```text
https://curl.se/libcurl/c/CURLOPT_SSL_OPTIONS.html
https://curl.se/libcurl/c/curl_version_info.html
https://curl.se/docs/CVE-2026-11564.html
```

## 1.2 V4.2.6 基线审计与生产语义（V4.2.7 全部保留）

V4.2.6 是把 V4.2.5 当作陌生实现重新进行生产级审计后的修订版。本次不是功能扩张，而是继续收紧默认安全边界、修复配置与日志语义漂移，并补齐可复现回归。V4.2.5 已有的认证重放、异步完成分发、资源上限、下载提交、重试副作用保护等能力继续保留。

本次 V4.2.6 实际确认并修复：

- **自动重定向默认改为拒绝 HTTP 目标**：`RedirectOptions::allowHttpsToHttp` 从 `true` 改为 `false`。默认仍会跟随允许的 HTTPS 重定向，但任何 HTTP 重定向目标都会被 libcurl 协议白名单拒绝，因此也会拒绝 `HTTP → HTTP`。需要兼容 HTTP 重定向的业务必须显式设置 `allowHttpsToHttp=true`。
- **生产默认最低 TLS 固定为 1.2**：`TlsOptions::minVersion` 从 `Default` 改为 `Tls1_2`，避免在 libcurl 8.16.0 之前或不同 TLS Backend 上因运行库默认值不同而意外允许 TLS 1.0/1.1。HTTPS Proxy 同样应用该 TLS 版本边界。遗留系统如确需旧 TLS，必须显式降低 `minVersion` 并自行承担风险。
- **Debug 内置敏感 Header 脱敏表与重定向敏感判定对齐**：除 `Authorization`、`Proxy-Authorization`、`Cookie`、`Set-Cookie`、`X-API-Key` 外，默认还会脱敏 `Api-Key`、`X-Auth-Token`、`X-Access-Token`、`X-Secret`，避免“重定向层认为是秘密、日志层却明文输出”的策略漂移。
- **自定义敏感 Query 名称先 Trim 再匹配**：`debug.sensitiveQueryParameters` 中意外带首尾空白时仍能正确参与 URL/Metrics 脱敏，不再静默失效。
- **显式空值 Cookie 不再被丢弃**：`RequestOptions::cookies.SetCookie("flag", "")` 会按 `flag=` 发送。空字符串表示合法 Cookie 值，不再被误判为“没有 Cookie”。
- **TLS 最高版本为 `Default` 时不再注入 `CURL_SSLVERSION_MAX_DEFAULT`**：`maxVersion=Default` 的真实语义是“不额外限制最高版本”，因此只传最低版本值；只有显式配置最高版本时才 OR 对应 MAX 位。这样既保持默认 TLS 1.2+ 基线，也减少旧 libcurl/wolfSSL 对 MAX 宏历史兼容差异带来的风险。HTTPS Proxy 使用同一组合规则。
- **复杂 `Link` Header 改为 fail-safe 整体脱敏**：HTTP `Link` 字段可在单个字段值中包含多个 URI-reference，通用“单 URL”脱敏器无法可靠覆盖后续链接。`hideSensitiveUrlData=true` 时 V4.2.6 直接把 `Link` 值输出为 `***`，避免第二个及后续 URL 的 token/query 凭据进入 Debug/verbose 日志。
- **手册语义同步**：修正 V4.2.5 中关于安全重定向默认值的前后矛盾，并明确 `MockTransport` 本身不会执行 `CurlTransport` 的自动重试循环。

V4.2.5 已有并在 V4.2.6 继续保留的主要安全语义包括：Debug 总开关与过滤 verbose、敏感凭据真实遇到重定向后的 fail-closed、旧 libcurl 自定义 Method 重定向保护、URL 编解码长度检查、非法枚举 fail-closed、Schannel 吊销兼容处理、外部响应 sink 重试保护、异步完成/资源清理兜底、默认 128 MiB Body 与 2 MiB Header/Trailer 上限、非 Resume 下载的同目录临时文件提交，以及认证刷新重放与 Session Cookie 隔离。

### 默认资源限制的设计决定

V4.2.6 保留 `128 MiB` 内存响应缓存上限和 `2 MiB` 单 attempt Header/Trailer 上限。这两个值对常规业务足够宽松，同时可以避免失控响应无限消耗内存；需要处理超大内存响应或异常巨大的 Header 时，可显式设为更大值或 `0`。对于大文件，推荐直接使用流式 sink 或文件下载接口，而不是依赖 `response.content`。


`curl_ex` 是基于 libcurl 的 C++17 HTTP 封装层，设计目标是：

1. 高频功能保持简单：GET、POST、JSON、Form、文件上传下载尽量一行完成。
2. 中频配置统一进入 `RequestOptions`，避免函数签名不断增长。
3. 高级能力单独分层：Session、认证、Middleware、Mock、Metrics、限流、熔断、异步并发。
4. 保留 libcurl 原生逃生口，封装没有覆盖的能力仍可通过 `nativeCurlOptions` 设置。
5. 最低要求 C++17；C++20 下自动增加 coroutine 等待接口。
6. 最低支持 libcurl 7.56.0；较新的可选能力会按 libcurl 版本和 TLS Backend 做条件启用。
7. 除 libcurl 和 C++ 标准库外，不强制依赖 JSON、Boost、fmt、spdlog 等第三方库。

整体层次：

```text
高频 API
├─ HttpRequest
├─ HttpClient
└─ AsyncHttpClient
        │
        ▼
HttpRequestData + RequestOptions
        │
        ▼
认证 / Middleware / Interceptor
        │
        ▼
Retry / RateLimit / CircuitBreaker / Metrics
        │
        ▼
IHttpTransport
├─ CurlTransport
└─ MockTransport
        │
        ▼
libcurl easy / multi
```

---

# 第一部分：快速开始

## 2. 最简单的 GET

```cpp
#include "curl_ex/curl_ex.h"

using namespace ytpp::curl_ex;

auto response = HttpRequest::Get("https://example.com/");

if (response.Ok()) {
    // response.content 即响应 Body。
}
```

`HttpRequest` 适合一次性、彼此没有 Session 关系的请求。

---

## 3. 最简单的 POST

```cpp
auto response = HttpRequest::Post(
    "https://example.com/api",
    "hello world"
);
```

Body 使用 `std::string_view` 接收，但内部会按明确长度发送，因此二进制字符串中包含 `\0` 时不会因为 C 字符串结束符而截断。

---

## 4. JSON 请求

```cpp
auto response = HttpRequest::PostJson(
    "https://example.com/api",
    R"({"name":"test"})"
);
```

`PostJson()` 会在你没有显式设置时自动补充：

```text
Content-Type: application/json
Accept: application/json
```

还提供：

```cpp
HttpRequest::PutJson(...);
HttpRequest::PatchJson(...);
HttpRequest::RequestJson("PROPFIND", ...);
```

---

## 5. 使用 RequestOptions

大部分单次请求配置都集中在 `RequestOptions`：

```cpp
RequestOptions options;

options.timeout = std::chrono::seconds(20);
options.connectTimeout = std::chrono::seconds(5);
options.headers.SetHeader("User-Agent", "MyApp/1.0");
options.retry.maxRetries = 2;

auto response = HttpRequest::Get(
    "https://example.com/",
    options
);
```

推荐原则：

```text
请求本身是什么               -> HttpRequestData
这一请求应该怎样发送         -> RequestOptions
多个请求之间共享的长期策略   -> HttpClientOptions
```

---

## 6. 判断请求是否成功

### TransportOk()

```cpp
if (!response.TransportOk()) {
    // DNS、连接、TLS、超时、取消、读写等传输层失败。
}
```

### Ok()

```cpp
if (response.Ok()) {
    // CURL 成功，并且 HTTP 状态码为 2xx。
}
```

例如服务器正常返回 HTTP 404 时：

```text
TransportOk() == true
Ok()          == false
```

这是推荐的语义区分。

---

# 第二部分：高频功能

## 7. 支持的常用 Method

同步一次性 API：

```cpp
HttpRequest::Get(url);
HttpRequest::Head(url);
HttpRequest::Post(url, body);
HttpRequest::Put(url, body);
HttpRequest::Delete(url, body);
HttpRequest::Patch(url, body);
HttpRequest::Options(url);
HttpRequest::Trace(url);
HttpRequest::Connect(url);
```

也可以统一使用枚举：

```cpp
auto response = HttpRequest::Request(
    HttpMethod::Patch,
    url,
    body,
    options
);
```

---

## 8. 自定义 HTTP Method

WebDAV 或私有协议可直接传文本：

```cpp
auto response = HttpRequest::Request(
    "PROPFIND",
    "https://example.com/webdav/",
    body,
    options
);
```

还可以使用：

```text
PROPPATCH
MKCOL
COPY
MOVE
LOCK
UNLOCK
以及服务端支持的其他自定义 Method
```

注意：真正的 HTTP Proxy CONNECT 隧道语义不应简单等价理解为“只把 Method 字符串改成 CONNECT”；如需要特殊代理隧道行为，应使用对应 libcurl 配置。

---

## 9. Form 表单

```cpp
UrlParams form;
form.Set("username", "demo");
form.Set("password", "123456");

auto response = HttpRequest::PostForm(
    "https://example.com/login",
    form
);
```

会自动使用：

```text
application/x-www-form-urlencoded
```

---

## 10. Multipart

普通字段：

```cpp
auto field = MultipartPart::Field("name", "test");
```

文件字段：

```cpp
auto file = MultipartPart::File(
    "file",
    "C:\\data\\image.jpg",
    "image/jpeg"
);
```

完整请求：

```cpp
std::vector<MultipartPart> parts;
parts.push_back(MultipartPart::Field("name", "test"));
parts.push_back(MultipartPart::File("file", "C:\\data\\image.jpg", "image/jpeg"));

auto response = HttpRequest::UploadMultipart(
    "https://example.com/upload",
    std::move(parts)
);
```

内部使用 libcurl `curl_mime`，无需自己处理 boundary。

---

## 11. 文件上传

```cpp
auto response = HttpRequest::UploadFile(
    "https://example.com/upload.bin",
    "C:\\data\\big.bin"
);
```

默认 Method 为：

```text
PUT
```

也可以指定：

```cpp
auto response = HttpRequest::UploadFile(
    url,
    filePath,
    options,
    "POST"
);
```

该接口使用流式读取，不需要把整个文件一次性放入内存。

---

## 12. 文件下载

V4.2.6 的普通下载默认使用目标同目录临时文件。只有请求完整成功并通过最终检查后才提交到目标路径，因此网络中断、HTTP 失败或回调失败不会提前破坏已有文件。Resume 模式为了续写现有文件仍会直接操作目标文件。


```cpp
auto response = HttpRequest::Download(
    "https://example.com/big.zip",
    "C:\\download\\big.zip"
);
```

下载数据直接写入文件，不要求完整 Body 保存在 `response.content` 中。

---

# 第三部分：URL、Header 与 Cookie

## 13. URL 编解码

```cpp
auto encoded = UrlEncode("a b+c");
auto decoded = UrlDecode(encoded);
```

空字符串输入已经单独处理，避免 libcurl 长度参数为 0 时退回 NUL 结尾读取造成 `string_view` 越界语义问题。输入长度若超过 libcurl escape/unescape API 的 `int` 上限会抛出 `std::length_error`，不会静默截断。

---

## 14. UrlParams

`UrlParams` 的特点：

- 保留参数原始顺序。
- 允许重复参数名。
- 区分 `flag` 与 `flag=`。
- 适合构造 Query，也可用于本封装的 `PostForm()` 序列化。
- `Parse()` 使用百分号解码语义，**不会**把 `+` 自动转换为空格；因此不要把它当成任意第三方 `application/x-www-form-urlencoded` 文本的完整通用解码器。如输入来源把 `+` 定义为空格，调用方应先按该协议语义处理。

示例：

```cpp
UrlParams params("a=1&a=2&empty=&flag");
```

重新序列化：

```text
a=1&a=2&empty=&flag
```

常用接口：

```cpp
params.Get("a");
params.GetAll("a");
params.Set("a", "3");
params.Add("a", "4");
params.Remove("a");
params.Has("a");
params.Size();
params.Empty();
params.Clear();
params.ToString();
params.ToString(true); // 开头带 ?
```

---

## 15. Header

增加 Header：

```cpp
options.headers.AddHeader("X-Test", "A");
options.headers.AddHeader("X-Test", "B");
```

设置并覆盖全部同名 Header：

```cpp
options.headers.SetHeader("Authorization", "Bearer xxx");
```

仅在不存在时设置：

```cpp
options.headers.SetDefaultHeader("Content-Type", "application/json");
```

HTTP Header 名称比较大小写不敏感：

```text
Content-Type
content-type
CONTENT-TYPE
```

都视为同一个名字。

获取重复 Header：

```cpp
auto values = response.headers.GetHeaderValues("Set-Cookie");
```

### rawHeaderLines

对于极少数需要完全交给 libcurl 的原始头行，可使用：

```cpp
options.rawHeaderLines.push_back("Expect:");
```

普通 Header 推荐优先使用 `HttpHeadersWrapper`，因为它会进行名称和值的安全检查。

---

## 16. Cookie

单个 Cookie：

```cpp
Cookie cookie;
cookie.name = "sid";
cookie.value = "123";
cookie.path = "/";
cookie.httpOnly = true;
cookie.secure = true;
```

请求 Cookie：

```cpp
options.cookies.SetCookie("sid", "123");
```

同名但不同 Domain / Path 的 Cookie 可以同时存在。

注意：`HttpCookiesWrapper` 是结构化便利容器；真正的 Session Cookie 路由和 Cookie Jar 更推荐让 libcurl Cookie Engine 管理。

V4.2.6 保留显式空值 Cookie：

```cpp
options.cookies.SetCookie("flag", "");
// 请求中发送：Cookie: flag=
```

如果调用方确实希望删除 Cookie，应调用 `EraseCookie()`，不要依赖空字符串被自动忽略。

---

# 第四部分：RequestOptions 完整说明

## 17. Header / Cookie / Proxy

```cpp
HttpHeadersWrapper headers;
std::vector<std::string> rawHeaderLines;
HttpCookiesWrapper cookies;
std::optional<ProxyOptions> proxy;
```

`proxy` 使用 `std::optional` 是为了明确区分：

```text
没有代理配置
与
存在一个代理配置对象
```

创建代理：

```cpp
RequestOptions options;
options.proxy.emplace();
options.proxy->url = "http://127.0.0.1:8888";
```

---

## 18. ProxyOptions

```cpp
ProxyOptions proxy;
proxy.url = "http://127.0.0.1:8888";
proxy.type = ProxyType::Http;
proxy.username = "user";
proxy.password = "pass";
proxy.noProxy = "localhost,127.0.0.1";
proxy.auth = CURLAUTH_ANY;
```

支持的代理类型：

```text
Http
Https
Socks4
Socks4A
Socks5
Socks5Hostname
```

认证位掩码使用 `unsigned long` 保存，避免 `CURLAUTH_ANY` 在 MSVC 下产生 `unsigned long -> long` 隐式收缩警告；真正传递给需要 `long` 的 libcurl option 时由实现显式转换。

---

## 19. 超时

```cpp
options.connectTimeout = std::chrono::seconds(6);
options.timeout = std::chrono::milliseconds(0); // 0 表示不限制整个传输时间
```

默认只限制连接阶段为 6 秒，不限制整个请求传输时间。业务如果有明确 SLA，建议按接口自行设置 `timeout`。

低速检测默认关闭；需要时再显式开启：

```cpp
options.lowSpeedLimitBytesPerSecond = 1024;
options.lowSpeedTime = std::chrono::seconds(10);
```

含义：连续指定时间内速度低于阈值时终止传输。

---

## 20. HTTP 版本

```cpp
options.httpVersion = HttpVersion::Auto;
```

支持：

```text
Auto
Http1_0
Http1_1
Http2
Http2Tls
Http3
```

推荐默认保持 `Auto`，除非目标服务存在明确兼容性要求。v4.2.7 在 `Auto` 时不会主动设置 `CURLOPT_HTTP_VERSION`，而是完全沿用当前 libcurl 的默认 HTTP 协商策略。

---

## 21. TLS

```cpp
options.tls.verifyPeer = true;
options.tls.verifyHost = true;
```

Windows Native CA（V4.2.7 新增）：

```cpp
// Windows 默认就是 true。
options.tls.useNativeCa = true;
```

默认情况下，Windows + 当前激活 OpenSSL Backend 会通过 `CURLSSLOPT_NATIVE_CA` 使用 Windows Certificate Store。Schannel 本身已经使用 Windows Certificate Store，因此不会额外设置该 bit。若显式设置 `caFile` 或 `caPath`，封装不会再额外叠加 Native CA bit；如果你明确希望恢复旧行为，可以设置 `options.tls.useNativeCa = false`。

自定义 CA：

```cpp
options.tls.caFile = "ca.pem";
options.tls.caPath = "ca-directory";
```

客户端证书：

```cpp
options.tls.clientCertificate = "client.pem";
options.tls.clientCertificateType = "PEM";
options.tls.clientKey = "client.key";
options.tls.clientKeyType = "PEM";
options.tls.clientKeyPassword = "password";
```

Public Key Pinning：

```cpp
options.tls.pinnedPublicKey = "sha256//...";
```

TLS 版本：

```cpp
options.tls.minVersion = TlsVersion::Tls1_2;
options.tls.maxVersion = TlsVersion::Tls1_3;
```

Cipher：

```cpp
options.tls.cipherList = "...";
options.tls.tls13CipherList = "...";
```

CRL / OCSP：

```cpp
options.tls.crlFile = "list.crl";
options.tls.verifyStatus = true;
```

Schannel 证书吊销策略：

```cpp
options.tls.revocationPolicy =
    CertificateRevocationPolicy::BestEffort;
```

三种模式：

```text
Strict      严格检查；吊销状态无法获取也会失败
BestEffort  默认；明确吊销仍失败，但分发点缺失/离线时允许继续
Disabled    完全关闭 Schannel 吊销检查
```

`verifyStatus` 默认关闭，因为 `CURLOPT_SSL_VERIFYSTATUS` 并非所有 TLS Backend 都支持。只有明确需要 OCSP Stapling 验证时再开启。具体 TLS 能力仍取决于当前 libcurl 使用的 TLS Backend。

### V4.2.7 CA Trust Store 优先级

封装层使用以下明确顺序：

```text
1. verifyPeer / verifyHost 决定是否执行证书链和主机名验证
2. 如果显式设置 caFile -> 设置 CURLOPT_CAINFO
3. 如果显式设置 caPath -> 设置 CURLOPT_CAPATH
4. Windows + 当前激活 Backend 为 OpenSSL，且没有 caFile/caPath，并且 useNativeCa=true -> 合并 CURLSSLOPT_NATIVE_CA
5. pinnedPublicKey 若非空 -> 继续设置 CURLOPT_PINNEDPUBLICKEY，独立于上面的 CA 验证
```

这里的“优先级”不是把 `caFile` 和 `caPath` 二选一：如果两者都非空，封装仍会把两者都传给 libcurl；只是只要任意一个显式 CA 来源存在，就不再额外叠加 Native CA bit。

Windows + OpenSSL 下不建议依赖 `caPath` 作为主要方案；libcurl 官方文档指出 OpenSSL 的 `CURLOPT_CAPATH` 在 Windows 上存在限制。需要自定义 CA 时优先使用 `caFile`；普通系统 HTTPS 则优先保留默认 `useNativeCa=true`。

### TLS Backend 与 libcurl 版本兼容性

V4.2.7 的 `TlsOptions` 继续保留 V4.2.6 的 TLS 版本基线，并新增 Native CA 默认值：

```cpp
TlsOptions tls;
// tls.minVersion == TlsVersion::Tls1_2
// tls.maxVersion == TlsVersion::Default
// Windows:     tls.useNativeCa == true
// non-Windows: tls.useNativeCa == false
```

因此正常请求会显式要求 **TLS 1.2 或更高版本**，最高版本仍由当前 libcurl/TLS Backend 决定；如果使用 HTTPS Proxy，同一最低/最高版本组合也会应用到代理 TLS 握手。这样可以让项目支持 libcurl 7.56.0 的同时，不依赖旧运行库各自不同的 TLS 默认最低版本。

当 `maxVersion == TlsVersion::Default` 时，V4.2.6 **不会**再额外 OR `CURL_SSLVERSION_MAX_DEFAULT`，因为 `Default` 在这里就是“不限制最高版本”。只有显式设置 `Tls1_0`～`Tls1_3` 最高版本时才加入对应 MAX 位。这避免了无必要地触发旧 libcurl/TLS Backend（尤其历史 wolfSSL 组合）对 MAX 宏的兼容差异，同时不改变显式最高版本限制。

只有调用方把 `minVersion` 和 `maxVersion` 都显式设为 `TlsVersion::Default` 时，封装才完全沿用当前 libcurl/TLS Backend 的默认 TLS 版本策略。这个兼容模式会失去 V4.2.6 的 TLS 1.2 安全基线，不建议生产环境无理由使用。

需要特别注意：

- `verifyStatus=true` 对应 OCSP Stapling；当前 libcurl 文档仅列出 OpenSSL/GnuTLS Backend 支持，默认关闭。
- `CertificateRevocationPolicy::BestEffort` 主要用于 Windows Schannel。`CURLSSLOPT_REVOKE_BEST_EFFORT` 从 libcurl 7.70.0 提供；更旧版本无法表达 BestEffort，V4.2.6 会保持 Schannel 默认严格吊销检查，而不是退化为 `NO_REVOKE`。
- `CURLOPT_PINNEDPUBLICKEY` 的 Schannel 支持从 7.58.1 开始；`CURLOPT_CAINFO` 的 Schannel 支持从 7.60.0 开始；Schannel 对 `CURLOPT_SSL_CIPHER_LIST` 的支持从 7.61.0 开始。因此“封装最低支持 libcurl 7.56.0”不等于每个 TLS 高级选项在每个 Backend 上都可用。
- `CURLOPT_TLS13_CIPHERS` 当前支持 OpenSSL、wolfSSL、mbedTLS、Rustls，不支持 GnuTLS/Schannel；`caPath`、`crlFile`、客户端证书/私钥格式等也存在 Backend 差异。封装对显式设置的 libcurl option 检查返回值；Backend 不支持时请求会失败，而不是把该安全配置当作成功。
- HTTP Bearer 原生认证需要 libcurl 7.61.0 或更新版本；更旧版本会返回明确的“不支持”错误，不影响 Basic/Digest 等基础 HTTP 认证。
- HTTPS Proxy 的 Backend 支持版本差异较大。部署前应以实际运行时 `curl_version_info()` 和对应 libcurl 官方 TLS/Proxy 能力表为准，不要只依据编译期版本号推断。

---

## 22. RetryPolicy

默认重试策略在 `RetryPolicy()` 构造函数中初始化。

常用设置：

```cpp
options.retry.maxRetries = 3;
options.retry.baseDelay = std::chrono::milliseconds(500);
options.retry.maxDelay = std::chrono::seconds(8);
options.retry.exponentialBackoff = true;
options.retry.jitter = true;
options.retry.respectRetryAfter = true;
```

非幂等请求默认更保守：

```cpp
options.retry.retryNonIdempotent = false;
```

如果业务接口有幂等保障，可以显式打开：

```cpp
options.retry.retryNonIdempotent = true;
```

自定义判定：

```cpp
options.retry.customShouldRetry =
    [](CURLcode curlCode, long httpCode, int retryIndex)
        -> std::optional<bool> {
        if (httpCode == 409)
            return true;
        return std::nullopt;
    };
```

返回值：

```text
true         强制重试
false        强制不重试
nullopt      继续使用内置策略
```

---

## 23. RedirectOptions

```cpp
options.redirect.follow = true;
options.redirect.maxRedirects = 10;
options.redirect.autoReferer = false;
options.redirect.allowHttpsToHttp = false; // V4.2.6 安全默认值
options.redirect.forwardAuthToOtherHosts = false;
options.redirect.forwardSensitiveHeadersToOtherHosts = false;
options.redirect.allowExplicitCookiesOnRedirects = false;
options.redirect.allowUnpinnedRedirects = false;
```

普通无凭据请求仍默认跟随重定向。`forwardAuthToOtherHosts=false` 对应 libcurl 标准认证信息的跨主机保护；V4.2.6 对封装无法在 FOLLOWLOCATION 内安全逐跳判断的敏感数据采用“首跳正常、下一跳前 fail-closed”：

- 自定义敏感 Header（内置常见名称或 `debug.sensitiveHeaders`）：首跳照常发送；如果服务器真实返回重定向，默认停止，不把该 Header 自动带入下一跳。明确接受自动重定向链可能跨主机转发的风险时才设置 `forwardSensitiveHeadersToOtherHosts=true`。
- `RequestOptions::cookies`：它最终使用 `CURLOPT_COOKIE`，libcurl 会把这种显式 Cookie 继续用于后续重定向。默认在真实出现重定向后停止；明确接受风险时才设置 `allowExplicitCookiesOnRedirects=true`。Session Cookie Engine 仍按 libcurl 自身的 Domain/Path/Secure 等 Cookie 规则处理，不等同于这个显式 Cookie override。
- `tls.pinnedPublicKey`：首跳仍正常执行 Pin 校验；如果出现重定向，默认停止，因为同一个 `CURLOPT_PINNEDPUBLICKEY` 不能被封装视为“已经安全覆盖所有其他 Origin”。明确接受后续跳转风险时才设置 `allowUnpinnedRedirects=true`；更安全的做法是关闭自动重定向，校验 `Location` 后为目标 Origin 建立新的请求和 Pin。
- libcurl 8.13.0 之前，自定义 Method 使用 `CURLOPT_CUSTOMREQUEST` 时，FOLLOWLOCATION 可能错误保持 Method，无法可靠服从 301/302/303 的 Method 改写规则。V4.2.6 在这类旧运行库上同样执行首跳，若真实出现重定向则在下一跳前停止；8.13.0+ 使用 `CURLFOLLOW_OBEYCODE`。

由于最低支持 libcurl 7.56.0，V4.2.6 还会保护旧版本缺失的跨主机行为：7.58.0 之前的自定义 `Authorization` Header、7.64.0 之前的自定义 `Cookie` Header，在默认不放宽认证转发时遇到重定向也会 fail-closed。

V4.2.6 默认 `allowHttpsToHttp=false`，因此允许的**重定向目标协议**收紧为仅 HTTPS。这个定义比“只阻止 HTTPS→HTTP”更严格：即使初始请求本身是 HTTP，`HTTP → HTTP` 的自动重定向目标也会被拒绝。这是生产安全默认值，而不是额外开启的严格模式。

只有明确需要兼容 HTTP 重定向时才放宽：

```cpp
options.redirect.allowHttpsToHttp = true;
```

放宽后重定向目标允许 HTTP/HTTPS，但其他凭据、Cookie、Pinning 和跨主机认证保护仍按各自开关执行。

响应中的所有 Header 块可通过：

```cpp
response.headerHistory
```

HTTP Trailer 单独保存在：

```cpp
response.trailers;
response.rawTrailers;
```

为了查询方便，Trailer 也会合并进 `response.headers`，但不会覆盖 `final_raw_headers` 的主响应 Header 语义；Trailer 中的 `Set-Cookie` 不会被误当成普通响应 Cookie。

HTTPS 经 HTTP 代理时，代理 `CONNECT` 的响应码单独保存在：

```cpp
response.proxyConnectCode;
```

这样 TLS 握手失败时不会把代理隧道的 `200` 误认为目标服务器的 HTTP 状态码。

查看，而 `response.headers` 表示最终响应 Header。

---

## 24. 自动解压

```cpp
options.autoDecompress = true;
```

也可以指定：

```cpp
options.acceptEncoding = "gzip, br";
```

如果为空且开启自动解压，则由 libcurl 使用其支持的编码能力。

---

## 25. Cookie Engine

`enableCookieEngine=false` 的 V4.2.6 语义是“本次请求不使用当前 Session Cookie 状态”。即使复用 `HttpClient` easy handle 或 `AsyncHttpClient` 的共享 Cookie Jar，本次请求也不会携带历史 Cookie；请求完成后原有 Cookie Session 仍可继续使用。


```cpp
options.enableCookieEngine = true;
options.newCookieSession = false;
options.cookieFile = "cookies.txt";
options.cookieJar = "cookies.txt";
```

连续请求更推荐通过 `HttpClient` 使用 Cookie Engine，因为单独的 `HttpRequest` 每次都会创建新的 `CurlTransport`。

---

## 26. 原始服务器认证字段

低层直接配置：

```cpp
options.username = "user";
options.password = "pass";
options.httpAuth = CURLAUTH_BASIC;
options.bearerToken = "token";
```

对于多个请求，推荐使用后面介绍的 `IAuthProvider`，避免业务层重复填写。

---

## 27. 响应 Body 策略

整个响应 Body 的传输总量默认不设额外上限：

```cpp
options.maxResponseSize = 0;
```

但当 `storeResponseBody=true` 时，`response.content` 默认另有 **128 MiB** 的内存缓存上限：

```cpp
options.maxInMemoryResponseSize = 128ULL * 1024ULL * 1024ULL;
```

因此“整个响应不限”不等于“内存无限缓存”。大文件应使用 `responseChunkCallback`、`responseStream` 或 `Download()`。如果业务还要限制整个传输 Body，可显式设置：

```cpp
options.maxResponseSize = 32ULL * 1024ULL * 1024ULL;
```

是否保存到 `response.content`：

```cpp
options.storeResponseBody = true;
```

收到数据块时回调：

```cpp
options.responseChunkCallback =
    [](const char* data, std::size_t size) {
        // 消费 data[0..size)
        return true;
    };
```

返回 `false` 会终止接收。

还可以写入现有 `ostream`：

```cpp
std::ofstream out("file.bin", std::ios::binary);
options.responseStream = &out;
```

调用方必须保证流对象在请求结束前一直有效。如果多个并发请求共享同一个 `responseStream` 或有状态 `responseChunkCallback`，其线程安全、输出分帧和业务级数据隔离由调用方保证；更推荐每个并发请求使用独立 sink。

### 外部响应接收器与自动重试

当已经向 `responseChunkCallback` 或 `responseStream` 写出部分数据后，封装层默认不会贸然自动重试，否则同一段响应可能被重复投递。

高级场景如果外部接收器能够回滚，可以提供：

```cpp
options.responseRetryResetCallback = [] {
    // 清空或回滚外部接收状态
    return true;
};
```

返回 `true` 后才允许安全地重新开始一次响应接收。`HttpRequest::Download()` 内部会自动处理文件回滚。

---

## 28. Cancel 与 Progress

取消标记：

```cpp
std::atomic_bool cancel{false};
options.cancelFlag = &cancel;
```

其他线程：

```cpp
cancel.store(true);
```

进度：

```cpp
options.progressCallback =
    [](curl_off_t downloadTotal,
       curl_off_t downloadNow,
       curl_off_t uploadTotal,
       curl_off_t uploadNow) {
        return true;
    };
```

返回 `false` 会请求终止传输。

调用方必须保证 `cancelFlag` 指向对象的生命周期覆盖整个请求。

---

## 29. Range 与 Resume

HTTP Range：

```cpp
options.range = "0-1023";
```

断点偏移：

```cpp
options.resumeFrom = 1024;
```

`resumeFrom` 在 v4.2.6 中明确用于 **GET 下载断点续传**。使用 `HttpRequest::Download()` 时，本地文件必须已经存在且大小与 `resumeFrom` 完全一致，之后才会以追加方式继续下载。

不要同时设置 `range` 与 `resumeFrom`，这两个语义会被本地参数校验拒绝。

适合断点下载或服务端支持的断点上传场景。

---

## 30. 限速

```cpp
options.speed.maxDownloadBytesPerSecond = 1024 * 1024;
options.speed.maxUploadBytesPerSecond = 512 * 1024;
```

0 表示不限制。

---

## 31. 自定义 DNS

```cpp
DnsResolveEntry entry;
entry.host = "example.com";
entry.port = 443;
entry.address = "203.0.113.10";

options.network.resolve.push_back(entry);
```

内部映射到 libcurl 静态 resolve 能力。

这与修改系统 hosts 不同，仅影响当前请求配置。

---

## 32. 绑定网卡 / 本地地址

```cpp
options.network.interfaceName = "192.168.1.100";
```

也可以根据 libcurl backend 和系统支持情况指定接口名称。

本地源端口：

```cpp
options.network.localPort = 50000;
options.network.localPortRange = 20;
```

---

## 33. DebugOptions

```cpp
options.debug.enabled = true;
options.debug.logRequestHeaders = true;
options.debug.logRequestBody = false;
options.debug.logResponseHeaders = true;
options.debug.logResponseBody = false;
options.debug.hideSensitiveHeaders = true;
options.debug.hideSensitiveUrlData = true;
options.debug.curlVerbose = false;
options.debug.maxBodyLogBytes = 4096;
```

自定义 logger：

```cpp
options.debug.logger = [](const std::string& line) {
    MyLog(line);
};
```

增加自定义敏感 Header：

```cpp
options.debug.sensitiveHeaders.push_back("X-Private-Token");
options.debug.sensitiveQueryParameters.push_back("customCredential");
```

默认敏感 Header 包括 `Authorization`、`Proxy-Authorization`、`Cookie`、`Set-Cookie`、`X-API-Key`、`Api-Key`、`X-Auth-Token`、`X-Access-Token`、`X-Secret`。自定义 `sensitiveQueryParameters` 名称比较会忽略首尾空白并按大小写不敏感处理。

`hideSensitiveUrlData=true` 不只处理请求 URL，也会处理响应中可能携带 URL 的 Header。`Location`、`Content-Location`、`Referer`、`Destination`、`Refresh` 会执行 URL/query 脱敏；`Link` 因为一个字段值可以包含多个 URI-reference，为避免后续 URL 的凭据漏出，会直接把整个字段值脱敏为 `***`。`ApiKeyAuthProvider` 使用 Header/Query 模式时会自动登记它自己的字段名为敏感字段。

`DebugOptions::enabled` 是日志总开关；`enabled=false` 时即使遗留 `curlVerbose=true` 也不会启用 verbose。`curlVerbose=true` 在 V4.2.6 中不再直接打开 libcurl 原始 stderr verbose，而是安装封装层过滤回调：URL/Header 仍按上述规则脱敏，原始 Body 和 TLS record 不记录。若同时主动关闭 Header/URL 脱敏，日志就可能包含凭据；生产环境仍应把日志输出视为敏感数据资产。

V4.2.7 在真实 `CurlTransport` 配置 TLS 时还会额外输出一段 TLS Trust Store 诊断。例如：

```text
curl_ex TLS configuration:
  curl: 8.21.0
  SSL: OpenSSL/3.6.1 (Schannel)
  active TLS backend: OpenSSL/3.6.1
  verify peer: true
  verify host: true
  native CA requested: true
  native CA option applied: true
  custom CA file: <none>
  custom CA path: <none>
  pinning: disabled
  libcurl default CAINFO: <null>
  libcurl default CAPATH: <null>
```

`CAINFO/CAPATH` 两项显示的是 libcurl 的**默认内建路径诊断值**；显式 `caFile/caPath` 会在上面的 `custom CA ...` 行单独显示。由于 `nativeCurlOptions` 是最后执行的逃生口，如果调用方随后又覆盖 TLS option，则这段日志只能代表封装层在执行 `nativeCurlOptions` 之前的配置。日志只显示 `pinning: enabled/disabled`，不会输出 Pin 内容、客户端私钥或私钥密码。

建议生产环境保持 Body 日志关闭，避免凭据、个人数据或大型响应进入日志。

---

## 34. nativeCurlOptions

封装没有覆盖到的 libcurl option：

```cpp
options.nativeCurlOptions = [](CURL* curl) {
    curl_easy_setopt(curl, CURLOPT_TCP_NODELAY, 1L);
};
```

这是高级逃生口，而且在内部标准配置完成后最后执行。它可以覆盖 TLS 校验、代理、重定向、协议限制、回调等封装安全策略，因此一旦使用 `nativeCurlOptions`，相关 option 的安全正确性由调用方重新承担，不能再假设封装默认值仍然生效。

注意：如果自行覆盖以下内部依赖的 callback：

```text
CURLOPT_WRITEFUNCTION
CURLOPT_HEADERFUNCTION
CURLOPT_XFERINFOFUNCTION
CURLOPT_READFUNCTION
```

可能破坏封装自己的响应收集、进度、取消和流式上传逻辑。

---

# 第五部分：完整请求与响应模型

## 35. HttpRequestData

```cpp
HttpRequestData request;
request.method = "POST";
request.url = "https://example.com/api";
request.body = "data";
request.requestId = "my-request-001";
request.tags["business"] = "login";
```

字段职责：

```text
method       HTTP Method 文本
url          请求 URL
body         内存 Body
requestId    追踪 ID；为空时自动生成
tags         用户自定义元数据，供 Middleware / Interceptor 使用
```

发送：

```cpp
auto response = HttpRequest::Send(request, options);
```

---

## 36. HttpResponse

`HttpResponse` 是所有同步、异步、Mock、Middleware 最终统一使用的响应模型。以下为 V4.2.7 **全部公开字段**：

| 字段 | 作用 / 语义 |
|---|---|
| `success` | libcurl/传输层是否成功完成。它不代表 HTTP 一定是 2xx；推荐通过 `TransportOk()` 判断传输成功。 |
| `error` | 封装层或 libcurl 的错误描述。成功时通常为空。 |
| `content` | 内存中的响应 Body；是否保存受 `storeResponseBody`、流式 sink 和大小上限影响。 |
| `code` | 最终 HTTP 状态码，例如 200、404。 |
| `org_headers` | 本次请求收到的全部原始 Header 块，包括重定向链中的中间响应。 |
| `headers` | 最终响应的结构化 Header；最终 Trailer 也会在结束时合并进这里，便于统一查询。 |
| `date` | **最终常规响应 Header 的 `Date`**，解析成功时为 `std::chrono::system_clock::time_point`；Header 不存在或无法按当前实现支持的 IMF-fixdate 形式（例如 `Wed, 09 Sep 2026 10:00:00 GMT`）解析时为 `std::nullopt`。中间重定向响应的 Date 不会覆盖它，Trailer 中的 Date 也不会用于该字段。 |
| `trailers` | 最终 HTTP Trailer 的结构化字段。 |
| `rawTrailers` | 原始 Trailer 文本。 |
| `cookies` | 从最终常规响应 `Set-Cookie` Header 解析出的 Cookie 集合。 |
| `curl_code` | 原始 libcurl `CURLcode`；网络/TLS/超时等底层错误排查首先看它。 |
| `errorCategory` | 封装层粗粒度错误分类 `HttpErrorCategory`。 |
| `requestId` | 请求追踪 ID；请求未提供时封装会生成。 |
| `final_raw_headers` | 最终常规响应 Header 块的原始文本。 |
| `headerHistory` | 每个响应 Header 块的结构化历史，适合查看 1xx/重定向链。 |
| `effectiveUrl` | libcurl 最终实际 URL。 |
| `contentType` | libcurl 报告的最终 Content-Type。 |
| `primaryIp` | 实际远端 IP。 |
| `negotiatedHttpVersion` | 实际协商的 HTTP 版本文本。 |
| `redirectCount` | 实际发生的重定向次数。 |
| `proxyConnectCode` | HTTP/HTTPS Proxy CONNECT 隧道响应码。 |
| `attempts` | 实际请求 attempt 次数，包含首次请求和自动重试。 |
| `downloadedBytes` | libcurl 统计的下载字节数。 |
| `uploadedBytes` | libcurl 统计的上传字节数。 |
| `totalTime` | 总耗时，微秒。 |
| `nameLookupTime` | DNS 阶段时间点/耗时指标，微秒。 |
| `connectTime` | TCP 连接阶段时间点/耗时指标，微秒。 |
| `tlsHandshakeTime` | TLS 握手阶段时间点/耗时指标，微秒。 |
| `firstByteTime` | 首字节时间（TTFB）指标，微秒。 |

成员函数：

```cpp
bool TransportOk() const noexcept;
bool Ok() const noexcept;
```

语义：

```text
TransportOk() = success && curl_code == CURLE_OK
Ok()          = TransportOk() && code >= 200 && code < 300
```

`date` 使用示例：

```cpp
auto response = HttpRequest::Get(url);
if (response.date) {
    const std::time_t serverTime =
        std::chrono::system_clock::to_time_t(*response.date);
    // serverTime 表示 HTTP Date 对应的 UTC 时间点；显示成本地时间由业务层决定。
}
```

---

## 37. HttpErrorCategory

封装层提供较粗粒度错误分类：

```text
None
InvalidArgument
Cancelled
Dns
Connect
Proxy
Tls
Timeout
Upload
Download
Http
RateLimited
CircuitOpen
Internal
```

业务层可以先判断 `errorCategory`，需要更精确时再查看原始：

```cpp
response.curl_code
```

---

# 第六部分：HttpClient Session

## 38. 为什么使用 HttpClient

一次性：

```cpp
HttpRequest::Get(url);
```

每次都创建独立 `CurlTransport`，适合彼此无状态的简单请求。

连续业务：

```cpp
HttpClient client;

auto login = client.Post(loginUrl, loginBody);
auto user = client.Get(userUrl);
auto data = client.Get(dataUrl);
```

`HttpClient` 默认复用同一个 `CurlTransport`，适合利用：

```text
Cookie Engine
持久连接
DNS cache
TLS Session
```

---

## 39. 默认 RequestOptions

```cpp
RequestOptions defaults;
defaults.timeout = std::chrono::seconds(20);
defaults.headers.SetHeader("User-Agent", "MyApp/1.0");

HttpClient client(defaults);
```

以后：

```cpp
client.Get(url);
```

会使用默认配置。

### 重要：显式 options 不是“增量合并”

当前 v4.2.7 中：

```cpp
client.Get(url, options);
```

会使用你传入的 `options`，不会自动把它与 `DefaultOptions()` 深度合并。

因此：

```text
无 options 的重载 -> 使用 DefaultOptions
有 options 的重载 -> 使用显式 RequestOptions
```

这是当前源码的真实语义。

---

## 40. Cookie Session 管理

```cpp
auto list = client.GetCookieList();
client.ClearCookies();
client.ClearSessionCookies();
client.FlushCookies();
```

这些便利函数只有当前 Transport 是 `CurlTransport` 时才有效。

如果你将 Client 替换为 `MockTransport` 或自定义 Transport，它们会返回空列表或失败值。

---

# 第七部分：认证系统

## 41. BasicAuthProvider

```cpp
HttpClientOptions clientOptions;
clientOptions.authProvider =
    std::make_shared<BasicAuthProvider>(
        "user",
        "password",
        CURLAUTH_BASIC
    );

HttpClient client(clientOptions);
```

Provider 会在发送请求前写入 `RequestOptions` 的用户名、密码和认证方式。

---

## 42. BearerAuthProvider

固定 Token：

```cpp
clientOptions.authProvider =
    std::make_shared<BearerAuthProvider>("access-token");
```

可刷新 Token：

```cpp
auto auth = std::make_shared<BearerAuthProvider>(
    "old-token",
    []() -> std::optional<std::string> {
        // 调用你的刷新接口。
        return std::string("new-token");
    }
);

clientOptions.authProvider = auth;
```

默认：

```cpp
clientOptions.refreshAuthOnUnauthorized = true;
```

当 Provider 判断 401 可以刷新时，`HttpClient` 会执行一次协调后的 Token Refresh，并重新发送一次原请求。

同步 Client 使用互斥锁避免同一个 Client 内多个线程同时触发刷新逻辑。

可以手动更新：

```cpp
auth->SetToken("new-token");
```

读取 Token 副本：

```cpp
auto token = auth->GetToken();
```

---

## 43. ApiKeyAuthProvider

Header：

```cpp
clientOptions.authProvider =
    std::make_shared<ApiKeyAuthProvider>(
        "X-API-Key",
        "secret",
        ApiKeyLocation::Header
    );
```

Query：

```cpp
clientOptions.authProvider =
    std::make_shared<ApiKeyAuthProvider>(
        "api_key",
        "secret",
        ApiKeyLocation::Query
    );
```

Query 模式会解析原 URL 的查询参数，再设置对应 API Key 参数。V4.2.6 会自动把 Query 参数名登记到 `debug.sensitiveQueryParameters`，Header 模式则自动登记到 `debug.sensitiveHeaders`，因此自定义 API Key 名称也会参与 Debug/Metrics 脱敏及重定向保护。

---

# 第八部分：Middleware 与 Interceptor

## 44. IHttpMiddleware

自定义 Middleware：

```cpp
class MyMiddleware final : public IHttpMiddleware {
public:
    bool Before(
        HttpRequestData& request,
        RequestOptions& options,
        HttpResponse& earlyResponse) override {

        options.headers.SetHeader("X-App-Version", "1.0");
        request.tags["middleware"] = "yes";
        return true;
    }

    void After(
        const HttpRequestData& request,
        const RequestOptions& options,
        HttpResponse& response) override {
        // 记录响应、统计等。
    }
};
```

注册：

```cpp
client.AddMiddleware(
    std::make_shared<MyMiddleware>()
);
```

### 短路请求

`Before()` 返回 `false` 时，请求不会进入 Transport。

此时应设置：

```cpp
earlyResponse
```

作为最终结果。

已执行过 `Before()` 的 Middleware 会按逆序执行 `After()`。

---

## 45. RequestInterceptor

简单修改请求时不必专门定义类：

```cpp
client.AddRequestInterceptor(
    [](HttpRequestData& request, RequestOptions& options) {
        request.tags["source"] = "desktop";
        options.headers.SetHeader("X-Client", "Windows");
    }
);
```

---

## 46. ResponseInterceptor

```cpp
client.AddResponseInterceptor(
    [](const HttpRequestData& request,
       const RequestOptions& options,
       HttpResponse& response) {
        // 统一日志、错误映射等。
    }
);
```

---

# 第九部分：限流、熔断和 Metrics

## 47. RateLimitPolicy

```cpp
clientOptions.rateLimit.requestsPerSecond = 10.0;
clientOptions.rateLimit.burst = 2;
clientOptions.rateLimit.maxConcurrent = 4;
```

语义：

```text
requestsPerSecond  Token Bucket 平均速率；0 表示不限速
burst              Token Bucket 最大突发容量
maxConcurrent      最大同时执行请求数；0 表示不限
```

这是 Client 级策略，因为它描述的是多个请求之间的关系。

---

## 48. CircuitBreakerPolicy

开启方式不是 `enabled=true`，而是：

```cpp
clientOptions.circuitBreaker.failureThreshold = 5;
```

`failureThreshold == 0` 表示关闭熔断。

完整示例：

```cpp
clientOptions.circuitBreaker.failureThreshold = 5;
clientOptions.circuitBreaker.openDuration = std::chrono::seconds(30);
clientOptions.circuitBreaker.halfOpenMaxRequests = 1;
clientOptions.circuitBreaker.countHttp5xx = true;
```

状态概念：

```text
Closed
  ↓ 连续失败达到阈值
Open
  ↓ openDuration 到期
Half-Open
  ↓ 成功 / 失败
Closed / Open
```

熔断时返回：

```cpp
response.errorCategory == HttpErrorCategory::CircuitOpen
```

---

## 49. BasicMetricsCollector

```cpp
auto metrics = std::make_shared<BasicMetricsCollector>();
clientOptions.metricsCollector = metrics;
```

读取：

```cpp
auto snapshot = metrics->Snapshot();
```

统计：

```text
totalRequests
transportSuccess
http2xx
http3xx
http4xx
http5xx
failedRequests
totalRetries
downloadedBytes
uploadedBytes
```

清零：

```cpp
metrics->Reset();
```

如需接 Prometheus 或自有系统，实现 `IMetricsCollector` 即可。

---

# 第十部分：Transport 与 Mock

## 50. IHttpTransport

接口非常简单：

```cpp
class IHttpTransport {
public:
    virtual ~IHttpTransport() = default;

    virtual HttpResponse Send(
        const HttpRequestData& request,
        const RequestOptions& options
    ) = 0;
};
```

因此上层业务不必与 CURL 强耦合。

---

## 51. CurlTransport

默认真实网络实现：

```cpp
auto transport = std::make_shared<CurlTransport>();

HttpResponse response = transport->Send(
    request,
    options
);
```

还提供：

```cpp
transport->Valid();
transport->NativeHandle();
transport->GetCookieList();
transport->ClearCookies();
transport->ClearSessionCookies();
transport->FlushCookies();
```

---

## 52. MockTransport

固定响应：

```cpp
auto mock = std::make_shared<MockTransport>();

HttpResponse response;
response.success = true;
response.curl_code = CURLE_OK;
response.code = 200;
response.content = "mock";

mock->AddStaticResponse(
    "GET",
    "mock://user",
    response
);
```

交给 HttpClient：

```cpp
HttpClient client(
    HttpClientOptions{},
    mock
);
```

注意：Transport 不属于 `HttpClientOptions` 字段，而是通过 `HttpClient` 构造函数参数或 `SetTransport()` 设置。

---

## 53. 动态 Mock

```cpp
mock->AddRule(
    [](const HttpRequestData& request,
       const RequestOptions& options) {
        return request.method == "POST" &&
               request.url == "mock://login";
    },
    [](const HttpRequestData& request,
       const RequestOptions& options) {
        HttpResponse response;
        response.success = true;
        response.curl_code = CURLE_OK;
        response.code = 200;
        response.content = "ok";
        return response;
    }
);
```

特别适合测试：

```text
401 Token Refresh
Middleware
认证 Header
Circuit Breaker
Metrics
业务错误处理
```

注意：`MockTransport::Send()` 每次只按规则返回一次响应，**不会执行 `CurlTransport` 内部的自动重试循环**。因此 429/503、`Retry-After`、回退延迟和上传/响应 sink 重放等重试语义，应使用真实 `CurlTransport` 配合本地测试服务器验证，或为专门的测试 Transport 自行实现等价的重试行为；不要把 Mock 的单次返回误认为已经覆盖自动重试。

---

# 第十一部分：AsyncHttpClient

## 54. 为什么不是 std::async

`AsyncHttpClient` 内部使用 libcurl multi：

```text
一个事件循环工作线程
        │
        ├─ easy handle A
        ├─ easy handle B
        ├─ easy handle C
        └─ ...
```

它不是“一个请求启动一个线程”的封装。

---

## 55. Future API

```cpp
AsyncHttpClient client;

auto f1 = client.GetAsync(url1);
auto f2 = client.GetAsync(url2);
auto f3 = client.PostAsync(url3, body);

auto r1 = f1.get();
auto r2 = f2.get();
auto r3 = f3.get();
```

常用接口：

```text
GetAsync
HeadAsync
PostAsync
PutAsync
DeleteAsync
PatchAsync
OptionsAsync
PostJsonAsync
PutJsonAsync
PatchJsonAsync
RequestAsync
```

---

## 56. Callback API

```cpp
HttpRequestData request;
request.method = "GET";
request.url = url;

client.SendWithCallback(
    request,
    options,
    [](HttpResponse response) {
        // 请求完成。
    }
);
```

---

## 57. PendingCount

```cpp
auto count = client.PendingCount();

// 近实时统计排队、延迟重试、传输、认证刷新和完成分发等待中的请求。
// future 置为 ready 之前、completion callback 开始之前，会先把该请求移出计数。
```

`PendingCount()` 是观测指标，不是等待全部请求完成的同步原语。计数递减与 `promise.set_value()` / callback 调用之间存在极短的执行窗口，因此业务需要确认某个请求完成时必须等待对应 future、callback 或 coroutine 结果，而不是轮询 `PendingCount()==0`。

---

## 58. AsyncHttpClientOptions

`AsyncHttpClient` 构造函数：

```cpp
AsyncHttpClient(RequestOptions defaults,
                HttpClientOptions clientOptions);
```

`HttpClientOptions` 中这些策略会参与异步执行：

```text
authProvider
middleware
requestInterceptors
responseInterceptors
rateLimit
circuitBreaker
metricsCollector
refreshAuthOnUnauthorized
maxTotalConnections
maxHostConnections
maxConcurrentStreams
```

异步客户端当前没有注入 `IHttpTransport` 的构造参数，因为其核心目的就是直接管理 `curl_multi + easy handle`。

---

## 59. HTTP/2 / HTTP/3 多路复用相关设置

```cpp
clientOptions.maxTotalConnections = 64;
clientOptions.maxHostConnections = 16;
clientOptions.maxConcurrentStreams = 100;
```

实际多路复用是否发生取决于：

```text
libcurl 编译能力
TLS backend
服务端协议支持
ALPN
HTTP 版本配置
连接可复用条件
```

---

# 第十二部分：C++20 Coroutine

## 60. 条件启用

头文件自动判断：

```cpp
#if __cplusplus >= 202002L && __has_include(<coroutine>)
#define CURL_EX_HAS_COROUTINES 1
#else
#define CURL_EX_HAS_COROUTINES 0
#endif
```

因此 C++17 不受影响。

---

## 61. GET Awaitable

C++20 下：

```cpp
auto response = co_await GetAwaitable(
    client,
    "https://example.com/"
);
```

POST：

```cpp
auto response = co_await PostAwaitable(
    client,
    url,
    body,
    options
);
```

`HttpAwaitable` 只是 coroutine 适配层，真正的网络 I/O 仍由 `AsyncHttpClient` 的 curl_multi 事件循环执行。Awaitable 移动/析构会注销尚未恢复的句柄，完成与注销在内部互斥区串行化。

这不等于封装可以替调用方同步任意 coroutine frame 生命周期：如果业务持有原始 `coroutine_handle` 并准备从另一线程直接 `destroy()`，必须自行与可能发生的 HTTP 完成 `resume()` 做同步。不要在没有外部同步的情况下并发 `destroy()` 与完成恢复。

---

# 第十三部分：线程安全与生命周期

## 62. HttpRequest

每次调用创建独立 `CurlTransport`，多个线程分别调用 `HttpRequest::Get/Post/...` 时互不共享 easy handle。

---

## 63. HttpClient

当前 v4.2.7 默认 `CurlTransport` 内部拥有一个可复用 easy handle。

因此推荐：

```text
同一个 HttpClient 用于顺序 Session 请求
```

如果要高并发，不要依赖多个线程同时调用同一个默认 `HttpClient` 来共享一个 easy handle；推荐直接使用：

```cpp
AsyncHttpClient
```

或者每个并发执行单元使用独立 `HttpClient`。

`SetDefaultOptions()`、`SetClientOptions()`、`SetTransport()`、`Add*()` 等受控写接口内部有互斥保护，请求执行时也会复制配置快照。**但 `DefaultOptions()` / `ClientOptions()` 返回的是内部对象引用，不是加锁快照**：直接通过可写 `DefaultOptions()` 修改，或者在其他线程调用 Setter/Add* 的同时读取这些引用，仍可能形成数据竞争。直接引用接口只应在没有并发配置修改/请求的初始化阶段使用；运行期动态配置优先使用 Setter，并由业务层保证配置读取者与写入者的同步边界。

---

## 64. 回调生命周期

以下字段保存的是外部对象指针或回调：

```cpp
cancelFlag
responseStream
nativeCurlOptions
progressCallback
responseChunkCallback
```

调用方需要保证涉及的外部对象在请求结束前有效。

异步请求尤其要注意不要捕获已经离开作用域的引用。异步认证刷新、Middleware After、ResponseInterceptor、Metrics、completion callback/协程恢复可能发生在内部完成线程，而不是提交请求的线程；共享状态和 logger 必须满足相应线程安全要求。

---

# 第十四部分：Windows / MSVC 注意事项

## 65. min / max 宏

v4.2.7 源码在涉及标准库 `min/max` 时使用了防宏冲突形式，例如：

```cpp
(std::max)(a, b)
```

以及：

```cpp
(std::numeric_limits<long>::max)()
```

公共头文件和实现本身已经对 Windows `min/max` 宏污染做防护，因此即使调用方没有定义 `NOMINMAX`，封装自身也不会依赖用户手工修改。

如果你的整个 Windows 工程都希望禁用 `Windows.h` 的 `min/max` 宏，仍推荐在项目级额外定义：

```text
NOMINMAX
```

---

## 66. CURLAUTH_ANY 类型

配置字段使用：

```cpp
unsigned long httpAuth = CURLAUTH_ANY;
```

而不是直接使用 signed `long` 接收，避免 MSVC 收缩转换警告。

---

## 67. UTF-8

源码文件使用 UTF-8。MSVC 工程推荐为相关目标增加：

```text
/utf-8
```

这样可以明确让编译器按 UTF-8 解释中文注释和中文字符串。

---

# 第十五部分：CMake 集成

## 68. 本次正式交付文件

v4.2.7 的正式交付只包含以下三个互相配套的文件：

```text
curl_ex.h
curl_ex.cpp
curl_ex_v4.2.7.md
```

源码文件名延续项目既有命名，不在文件名里加版本号；请通过 `kCurlExVersion == "4.2.7"`、CPP 头部版本注释和 `curl_ex_v4.2.7.md` 三处共同确认版本，避免旧同名源码混入。

---

## 69. 最小 target

如果你的工程已经配置好 libcurl：

```cmake
add_library(curl_ex STATIC
    curl_ex.cpp
)

target_include_directories(curl_ex
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(curl_ex
    PUBLIC
        CURL::libcurl
)
```

Windows 推荐：

```cmake
target_compile_definitions(curl_ex
    PUBLIC
        NOMINMAX
)
```

---

## 70. vcpkg

典型方式：

```cmake
find_package(CURL CONFIG REQUIRED)
```

然后：

```cmake
target_link_libraries(your_target
    PRIVATE
        curl_ex
)
```

如果使用 vcpkg Toolchain，`find_package(CURL CONFIG REQUIRED)` 通常可以直接获得 `CURL::libcurl`；实际依赖解析仍以你的 triplet 和 libcurl 构建方式为准。

---

# 第十六部分：常用配方

## 71. 带代理 GET

```cpp
RequestOptions options;
options.proxy.emplace();
options.proxy->url = "http://127.0.0.1:8888";

auto response = HttpRequest::Get(url, options);
```

---

## 72. SOCKS5 代理

```cpp
options.proxy.emplace();
options.proxy->url = "127.0.0.1:1080";
options.proxy->type = ProxyType::Socks5Hostname;
```

`Socks5Hostname` 表示域名由代理侧解析。

---

## 73. Certificate Pinning

```cpp
RequestOptions options;
options.tls.pinnedPublicKey = "sha256//BASE64_HASH";

auto response = HttpRequest::Get(url, options);
```

Pinning 失败属于 TLS 传输错误。V4.2.6 不会因为 `redirect.follow=true` 就预先拒绝带 Pin 的普通请求：首跳照常执行 Pin 校验；只有服务器真实返回重定向时，默认才会在下一跳前 fail-closed。生产环境优先关闭自动重定向并逐 Origin 校验；只有明确接受后续跳转风险时才设置 `options.redirect.allowUnpinnedRedirects = true`。

---

## 74. 取消下载

```cpp
std::atomic_bool cancel{false};

RequestOptions options;
options.cancelFlag = &cancel;

auto response = HttpRequest::Download(
    url,
    filePath,
    options
);
```

其他线程：

```cpp
cancel.store(true);
```

---

## 75. 自定义流式上传

实现 `IUploadSource`：

```cpp
class MySource : public IUploadSource {
public:
    std::size_t Read(char* buffer, std::size_t capacity) override;
    bool Rewind() override;
    std::optional<curl_off_t> Size() const override;
};
```

使用：

```cpp
options.uploadSource = std::make_shared<MySource>();

auto response = HttpRequest::Request(
    "PUT",
    url,
    {},
    options
);
```

重试时封装可能要求上传源 `Rewind()`；不能回退的流式源无法安全重放。`IUploadSource` 通常包含读取游标状态，因此同一个实例默认应只属于一个正在进行的逻辑请求；不要把同一个有状态 `uploadSource` 同时交给多个并发请求，除非你的实现明确自行完成并发隔离/多游标语义。

---

## 76. CallbackUploadSource

```cpp
auto source = std::make_shared<CallbackUploadSource>(
    [](char* buffer, std::size_t capacity) -> std::size_t {
        // 将下一块数据写入 buffer。
        return bytesRead;
    },
    []() {
        // 将上游读取位置恢复到开头。
        return true;
    },
    totalSize
);

options.uploadSource = source;
```

---

## 77. 响应流式消费

```cpp
RequestOptions options;
options.storeResponseBody = false;
options.responseChunkCallback =
    [](const char* data, std::size_t size) {
        Process(data, size);
        return true;
    };

auto response = HttpRequest::Get(url, options);
```

适合超大 Body 或边收边解析。

---

## 78. 自定义 Mock 测试 Token Refresh

```cpp
auto mock = std::make_shared<MockTransport>();

mock->AddRule(
    [](const HttpRequestData&, const RequestOptions&) {
        return true;
    },
    [](const HttpRequestData&, const RequestOptions& options) {
        HttpResponse r;
        r.success = true;
        r.curl_code = CURLE_OK;
        r.code = options.bearerToken == "new" ? 200 : 401;
        return r;
    }
);
```

再配合 `BearerAuthProvider` 的 RefreshCallback，即可在完全不联网情况下测试自动刷新流程。

---

# 第十七部分：设计边界

## 79. 为什么 RequestOptions 不包含所有 Client 状态

像下面这些功能具有跨请求状态：

```text
Token Refresh 协调
RateLimiter Token Bucket
Circuit Breaker 状态
Metrics Collector
Middleware 列表
curl_multi 连接限制
```

因此放在：

```cpp
HttpClientOptions
```

而不是每个请求重新构造一份状态。

---

## 80. 为什么 AsyncHttpClient 不继承 HttpClient

同步 Client 的核心是单 easy handle Session；异步 Client 的核心是一个 multi handle 管理多个 easy handle。

两者共享请求模型和策略类型，但生命周期和执行器本质不同，因此保持并列 API 比强行继承更清晰。

---

## 81. 为什么保留 IHttpTransport

它主要解决：

```text
单元测试
Mock
未来替换网络后端
业务层与 CURL 解耦
```

高频业务代码完全可以不知道它存在。

---

# 第十八部分：API 使用层级建议

## 82. 高频：优先使用

```text
HttpRequest::Get
HttpRequest::Post
HttpRequest::PostJson
HttpRequest::PostForm
HttpRequest::UploadMultipart
HttpRequest::UploadFile
HttpRequest::Download
RequestOptions
HttpClient
```

这些应该覆盖绝大多数业务。

---

## 83. 进阶：需要时使用

```text
ProxyOptions
TlsOptions
RetryPolicy
RedirectOptions
DebugOptions
NetworkOptions
Range / Resume
Progress / Cancel
IUploadSource
responseChunkCallback
nativeCurlOptions
```

---

## 84. 高级：框架与基础设施层使用

```text
HttpRequestData
IHttpTransport
CurlTransport
MockTransport
IAuthProvider
IHttpMiddleware
Interceptor
RateLimiter
CircuitBreaker
IMetricsCollector
AsyncHttpClient
HttpAwaitable
```

普通 GET 不需要理解这些类型。

---

# 第十九部分：版本同步规则

## 85. v4.2.7 文件必须配套

V4.2.7 延续你当前工程的源码命名：

```text
curl_ex.h
curl_ex.cpp
curl_ex_v4.2.7.md
```

由于 `.h/.cpp` 文件名本身不带版本号，**不要只根据文件名判断新旧**。推荐每次升级后同时检查：

```cpp
ytpp::curl_ex::kCurlExVersion == "4.2.7"
```

以及 `curl_ex.cpp` 顶部的：

```text
当前源码版本：4.2.7
```

只要其中任何一处仍是 4.2.6，就说明混入了旧文件。本次交付的三个文件应作为同一套版本保存。

---

## 86. 升级版本时的推荐验证

每次修改封装后至少执行：

```text
1. C++17 编译
2. C++20 编译
3. 高警告级别编译
4. URL/Header/Cookie/Mock 等无网络单元测试
5. GET / POST / 二进制 Body 网络回归
6. Cookie Session 回归
7. Retry 回归
8. Async 并发回归
9. C++20 coroutine 回归
10. Windows min/max 宏污染检查
```

这样可以尽早发现“头文件、CPP、文档和示例不同步”的问题。

---

# 第二十部分：最小推荐模板

## 87. 一次请求

```cpp
RequestOptions options;
options.timeout = std::chrono::seconds(20);
options.headers.SetHeader("User-Agent", "MyApp/1.0");

auto response = HttpRequest::Get(url, options);

if (!response.TransportOk()) {
    // 网络错误。
} else if (!response.Ok()) {
    // HTTP 非 2xx。
} else {
    // 使用 response.content。
}
```

---

## 88. Session Client

```cpp
RequestOptions defaults;
defaults.timeout = std::chrono::seconds(20);

HttpClientOptions clientOptions;
clientOptions.authProvider =
    std::make_shared<BearerAuthProvider>(token);

HttpClient client(defaults, clientOptions);

auto r1 = client.Get(url1);
auto r2 = client.PostJson(url2, json);
```

---

## 89. 并发 Client

```cpp
AsyncHttpClient client;

auto f1 = client.GetAsync(url1);
auto f2 = client.GetAsync(url2);
auto f3 = client.GetAsync(url3);

auto r1 = f1.get();
auto r2 = f2.get();
auto r3 = f3.get();
```

---

# 90. 结论

如果只记住三个入口：

```text
单次同步请求      HttpRequest
连续 Session 请求 HttpClient
真正并发请求      AsyncHttpClient
```

如果只记住两个配置对象：

```text
单次请求配置      RequestOptions
跨请求长期策略    HttpClientOptions
```

如果只记住一个成功判断规则：

```text
TransportOk() -> 传输层是否成功
Ok()          -> 传输成功并且 HTTP 为 2xx
```

v4.2.7 继续保持这一设计目标：高频 API 尽量简单，高级能力全部存在，但只有真正需要时才进入对应层级。

---

# 附录 A：V4.2.7 公共字段与 API 查询索引

这一附录用于“查字段/查函数”而不是替代前面的设计说明。字段默认值与语义以本次 `curl_ex.h` 为准；下面列出**所有公开数据结构字段**。类的私有实现字段（例如 `impl_`）属于 ABI/实现细节，不是调用方配置面，因此不列入公共字段表。

## A.1 `UrlParamItem` 全字段速查

| 字段 | 声明/默认 | 作用 |
|---|---|---|
| `key` | `std::string key` | 参数名称。 |
| `value` | `std::string value` | 参数值。 |
| `hasEquals` | `bool hasEquals = true` | 是否保留 key= 中的等号。 |

## A.2 `HttpHeaderItem` 全字段速查

| 字段 | 声明/默认 | 作用 |
|---|---|---|
| `name` | `std::string name` | Header 名称。 |
| `value` | `std::string value` | Header 值。 |

## A.3 `Cookie` 全字段速查

| 字段 | 声明/默认 | 作用 |
|---|---|---|
| `name` | `std::string name` | Cookie 名称。 |
| `value` | `std::string value` | Cookie 值。 |
| `path` | `std::optional<std::string> path` | 可选 Path。 |
| `domain` | `std::optional<std::string> domain` | 可选 Domain。 |
| `expires` | `std::optional<std::string> expires` | 可选 Expires 文本。 |
| `maxAge` | `std::optional<std::int64_t> maxAge` | 可选 Max-Age。 |
| `secure` | `bool secure = false` | 是否包含 Secure。 |
| `httpOnly` | `bool httpOnly = false` | 是否包含 HttpOnly。 |
| `sameSite` | `std::optional<std::string> sameSite` | 可选 SameSite 属性。 |

## A.4 `ProxyOptions` 全字段速查

| 字段 | 声明/默认 | 作用 |
|---|---|---|
| `url` | `std::string url` | 代理地址。 |
| `type` | `std::optional<ProxyType> type` | 可选代理类型；为空时由 libcurl 判断。 |
| `username` | `std::string username` | 代理用户名。 |
| `password` | `std::string password` | 代理密码。 |
| `noProxy` | `std::string noProxy` | 不经过代理的主机列表。 |
| `auth` | `unsigned long auth = CURLAUTH_ANY` | 代理认证方式位掩码。 |

## A.5 `TlsOptions` 全字段速查

| 字段 | 声明/默认 | 作用 |
|---|---|---|
| `verifyPeer` | `bool verifyPeer = true` | 是否验证证书链。 |
| `verifyHost` | `bool verifyHost = true` | 是否验证证书主机名。 |
| `verifyStatus` | `bool verifyStatus = false` | 是否启用 OCSP Stapling 验证。 |
| `revocationPolicy` | `CertificateRevocationPolicy revocationPolicy = CertificateRevocationPolicy::BestEffort` | Schannel 证书吊销检查策略。 |
| `minVersion` | `TlsVersion minVersion = TlsVersion::Tls1_2` | 最低 TLS 版本；生产默认强制 TLS 1.2，旧协议必须显式降级。 |
| `maxVersion` | `TlsVersion maxVersion = TlsVersion::Default` | 最高 TLS 版本。 |
| `caFile` | `std::string caFile` | 自定义 CA 文件。 |
| `caPath` | `std::string caPath` | 自定义 CA 目录。 |
| `clientCertificate` | `std::string clientCertificate` | 客户端证书路径。 |
| `clientCertificateType` | `std::string clientCertificateType` | 客户端证书类型。 |
| `clientKey` | `std::string clientKey` | 客户端私钥路径。 |
| `clientKeyType` | `std::string clientKeyType` | 客户端私钥类型。 |
| `clientKeyPassword` | `std::string clientKeyPassword` | 客户端私钥密码。 |
| `pinnedPublicKey` | `std::string pinnedPublicKey` | 公钥 Pinning 值或文件路径。 |
| `cipherList` | `std::string cipherList` | TLS 1.2 及以下 Cipher 列表。 |
| `tls13CipherList` | `std::string tls13CipherList` | TLS 1.3 Cipher 列表。 |
| `crlFile` | `std::string crlFile` | CRL 文件路径。 |
| `useNativeCa` | `Windows: bool useNativeCa = true; non-Windows: bool useNativeCa = false` | Windows 默认请求使用系统原生 CA Store；非 Windows 默认不改变既有 CA Trust Store 策略；显式 caFile/caPath 时不额外叠加 Native CA bit。 |

## A.6 `RetryPolicy` 全字段速查

| 字段 | 声明/默认 | 作用 |
|---|---|---|
| `maxRetries` | `int maxRetries = 3` | 最大重试次数，不包含首次请求。 |
| `baseDelay` | `std::chrono::milliseconds baseDelay{500}` | 基础重试延迟。 |
| `maxDelay` | `std::chrono::milliseconds maxDelay{8000}` | 最大重试延迟。 |
| `exponentialBackoff` | `bool exponentialBackoff = true` | 是否指数退避。 |
| `jitter` | `bool jitter = true` | 是否加入随机抖动。 |
| `respectRetryAfter` | `bool respectRetryAfter = true` | 是否遵循 Retry-After。 |
| `retryNonIdempotent` | `bool retryNonIdempotent = false` | 是否允许重试非幂等请求。 |
| `retryCurlCodes` | `std::vector<CURLcode> retryCurlCodes` | 可重试 CURLcode 列表。 |
| `retryHttpStatusCodes` | `std::vector<long> retryHttpStatusCodes` | 可重试 HTTP 状态码列表。 |
| `customShouldRetry` | `std::function<std::optional<bool>(CURLcode, long, int)> customShouldRetry` | 自定义重试判定。 |

## A.7 `RedirectOptions` 全字段速查

| 字段 | 声明/默认 | 作用 |
|---|---|---|
| `follow` | `bool follow = true` | 是否自动跟随重定向。 |
| `maxRedirects` | `long maxRedirects = 10` | 最大重定向次数。 |
| `autoReferer` | `bool autoReferer = false` | 是否自动更新 Referer，默认关闭以避免无意泄露完整来源 URL。 |
| `allowHttpsToHttp` | `bool allowHttpsToHttp = false` | 是否允许自动重定向目标使用 HTTP；生产默认关闭，重定向目标仅允许 HTTPS。需要兼容 HTTP 重定向时必须显式开启。 |
| `forwardAuthToOtherHosts` | `bool forwardAuthToOtherHosts = false` | 是否向其他主机转发 libcurl 标准认证信息。 |
| `forwardSensitiveHeadersToOtherHosts` | `bool forwardSensitiveHeadersToOtherHosts = false` | 是否允许自定义敏感 Header 进入自动重定向链；默认关闭时首跳仍正常发送，真实出现重定向后会在下一跳前 fail-closed，避免潜在跨主机泄露。 |
| `allowExplicitCookiesOnRedirects` | `bool allowExplicitCookiesOnRedirects = false` | 是否允许 RequestOptions::cookies 进入自动重定向链；默认关闭时首跳仍正常发送，真实出现重定向后会在下一跳前 fail-closed。 |
| `allowUnpinnedRedirects` | `bool allowUnpinnedRedirects = false` | 配置公钥 Pinning 时是否允许进入自动重定向链；默认关闭时首跳仍执行 Pin 校验，真实出现重定向后在下一跳前 fail-closed。 |

## A.8 `DebugOptions` 全字段速查

| 字段 | 声明/默认 | 作用 |
|---|---|---|
| `enabled` | `bool enabled = false` | 是否启用封装层日志。 |
| `logRequestHeaders` | `bool logRequestHeaders = true` | 是否记录请求 Header。 |
| `logRequestBody` | `bool logRequestBody = false` | 是否记录请求 Body。 |
| `logResponseHeaders` | `bool logResponseHeaders = true` | 是否记录响应 Header。 |
| `logResponseBody` | `bool logResponseBody = false` | 是否记录响应 Body。 |
| `hideSensitiveHeaders` | `bool hideSensitiveHeaders = true` | 是否自动脱敏敏感 Header。 |
| `hideSensitiveUrlData` | `bool hideSensitiveUrlData = true` | 是否自动脱敏 URL 用户信息和常见敏感查询参数。 |
| `curlVerbose` | `bool curlVerbose = false` | 是否启用经封装层脱敏过滤的 libcurl verbose。 |
| `maxBodyLogBytes` | `std::size_t maxBodyLogBytes = 4096` | 单次最多记录的 Body 字节数。 |
| `sensitiveHeaders` | `std::vector<std::string> sensitiveHeaders` | 额外敏感 Header 名称；同时用于 Debug 脱敏和跨主机重定向保护。 |
| `sensitiveQueryParameters` | `std::vector<std::string> sensitiveQueryParameters` | 额外敏感 URL 查询参数名称。 |
| `logger` | `std::function<void(const std::string&)> logger` | 自定义日志输出回调。 |

## A.9 `DnsResolveEntry` 全字段速查

| 字段 | 声明/默认 | 作用 |
|---|---|---|
| `host` | `std::string host` | 域名。 |
| `port` | `std::uint16_t port=0` | 端口。 |
| `address` | `std::string address` | 强制解析 IP。 |

## A.10 `NetworkOptions` 全字段速查

| 字段 | 声明/默认 | 作用 |
|---|---|---|
| `resolve` | `std::vector<DnsResolveEntry> resolve` | 静态 DNS 解析项。 |
| `interfaceName` | `std::string interfaceName` | 指定出口网卡或本地地址。 |
| `localPort` | `long localPort = 0` | 本地源端口，0 表示自动。 |
| `localPortRange` | `long localPortRange = 1` | 本地端口尝试范围。 |

## A.11 `TransferSpeedOptions` 全字段速查

| 字段 | 声明/默认 | 作用 |
|---|---|---|
| `maxDownloadBytesPerSecond` | `curl_off_t maxDownloadBytesPerSecond = 0` | 最大下载速度，0 表示不限。 |
| `maxUploadBytesPerSecond` | `curl_off_t maxUploadBytesPerSecond = 0` | 最大上传速度，0 表示不限。 |

## A.12 `MultipartPart` 全字段速查

| 字段 | 声明/默认 | 作用 |
|---|---|---|
| `name` | `std::string name` | 字段名称。 |
| `data` | `std::string data` | 普通字段数据。 |
| `filePath` | `std::string filePath` | 文件字段本地路径。 |
| `fileName` | `std::string fileName` | 可选上传文件名。 |
| `contentType` | `std::string contentType` | 可选内容类型。 |

## A.13 `RequestOptions` 全字段速查

| 字段 | 声明/默认 | 作用 |
|---|---|---|
| `headers` | `HttpHeadersWrapper headers` | 请求头部。 |
| `rawHeaderLines` | `std::vector<std::string> rawHeaderLines` | 原生 HTTP 头部行。 |
| `cookies` | `HttpCookiesWrapper cookies` | 显式请求 Cookie。 |
| `proxy` | `std::optional<ProxyOptions> proxy` | 可选代理配置。 |
| `tls` | `TlsOptions tls` | TLS 配置。 |
| `retry` | `RetryPolicy retry` | 重试策略。 |
| `redirect` | `RedirectOptions redirect` | 重定向策略。 |
| `debug` | `DebugOptions debug` | 调试配置。 |
| `network` | `NetworkOptions network` | 网络层配置。 |
| `speed` | `TransferSpeedOptions speed` | 传输限速配置。 |
| `httpVersion` | `HttpVersion httpVersion = HttpVersion::Auto` | HTTP 版本偏好。 |
| `connectTimeout` | `std::chrono::milliseconds connectTimeout{6000}` | 连接超时。 |
| `timeout` | `std::chrono::milliseconds timeout{0}` | 单次传输尝试总超时，0 表示不限制；需要时建议业务按场景显式设置。 |
| `lowSpeedLimitBytesPerSecond` | `long lowSpeedLimitBytesPerSecond = 0` | 低速判定阈值，0 表示关闭低速中止。 |
| `lowSpeedTime` | `std::chrono::seconds lowSpeedTime{0}` | 低速持续时间，0 表示关闭低速中止。 |
| `autoDecompress` | `bool autoDecompress = true` | 是否自动解压响应。 |
| `acceptEncoding` | `std::string acceptEncoding` | 自定义 Accept-Encoding。 |
| `failOnHttpError` | `bool failOnHttpError = false` | 是否把 HTTP >=400 转为 CURL 错误。 |
| `tcpKeepAlive` | `bool tcpKeepAlive = true` | 是否启用 TCP KeepAlive。 |
| `tcpKeepIdleSeconds` | `long tcpKeepIdleSeconds = 30` | KeepAlive 空闲时间。 |
| `tcpKeepIntervalSeconds` | `long tcpKeepIntervalSeconds = 10` | KeepAlive 探测间隔。 |
| `disallowUsernameInUrl` | `bool disallowUsernameInUrl = false` | 是否禁止 URL 携带用户名密码，默认允许以保持常规兼容性。 |
| `enableCookieEngine` | `bool enableCookieEngine = true` | 是否启用 libcurl Cookie Engine。 |
| `newCookieSession` | `bool newCookieSession = false` | 是否开始新的 Cookie Session。 |
| `cookieFile` | `std::string cookieFile` | Cookie 读取文件。 |
| `cookieJar` | `std::string cookieJar` | Cookie 持久化文件。 |
| `userAgent` | `std::string userAgent` | 请求使用的 User-Agent 字符串。 |
| `username` | `std::string username` | 原始服务器认证用户名。 |
| `password` | `std::string password` | 原始服务器认证密码。 |
| `httpAuth` | `unsigned long httpAuth = CURLAUTH_ANY` | HTTP 认证方式位掩码。 |
| `bearerToken` | `std::string bearerToken` | 直接设置 Bearer Token。 |
| `maxResponseSize` | `std::size_t maxResponseSize = 0` | 整个响应 Body 最大字节数，0 表示不限；流式或文件下载可按业务显式设置。 |
| `maxInMemoryResponseSize` | `std::size_t maxInMemoryResponseSize = 128U * 1024U * 1024U` | content 内存缓存上限，默认 128 MiB，0 表示不限；不影响纯流式/文件下载。 |
| `maxResponseHeaderSize` | `std::size_t maxResponseHeaderSize = 2U * 1024U * 1024U` | 单次尝试累计响应 Header/Trailer 上限，默认 2 MiB，0 表示不限。 |
| `storeResponseBody` | `bool storeResponseBody = true` | 是否保存响应 Body 到 content。 |
| `responseChunkCallback` | `ResponseChunkCallback responseChunkCallback` | 响应数据块回调。 |
| `responseStream` | `std::ostream* responseStream = nullptr` | 可选响应输出流，生命周期由调用方负责。 |
| `responseRetryResetCallback` | `ResponseRetryResetCallback responseRetryResetCallback` | 已向外部接收器写入数据后，自动重试前用于回滚接收状态；为空时为避免重复数据不会重试。 |
| `cancelFlag` | `std::atomic_bool* cancelFlag = nullptr` | 可选取消标记，生命周期由调用方负责。 |
| `progressCallback` | `ProgressCallback progressCallback` | 传输进度回调。 |
| `multipart` | `std::vector<MultipartPart> multipart` | 多部分表单字段列表。 |
| `uploadSource` | `std::shared_ptr<IUploadSource> uploadSource` | 可选流式上传源。 |
| `uploadSize` | `std::optional<curl_off_t> uploadSize` | 可选显式上传大小。 |
| `range` | `std::string range` | Range 范围，例如 0-1023。 |
| `resumeFrom` | `std::optional<curl_off_t> resumeFrom` | 可选下载断点续传偏移；Download() 会校验本地文件大小后追加写入。 |
| `nativeCurlOptions` | `CurlOptionsCallback nativeCurlOptions` | 最后执行的原生 CURL 配置接口。 |

## A.14 `HttpRequestData` 全字段速查

| 字段 | 声明/默认 | 作用 |
|---|---|---|
| `method` | `std::string method = "GET"` | HTTP Method 文本。 |
| `url` | `std::string url` | 请求 URL。 |
| `body` | `std::string body` | 内存请求 Body。 |
| `requestId` | `std::string requestId` | 请求追踪 ID，为空时自动生成。 |
| `tags` | `std::unordered_map<std::string, std::string> tags` | 用户自定义元数据。 |

## A.15 `HttpHeaderBlock` 全字段速查

| 字段 | 声明/默认 | 作用 |
|---|---|---|
| `statusCode` | `long statusCode = 0` | Header 块状态码。 |
| `statusLine` | `std::string statusLine` | HTTP 状态行。 |
| `rawHeaders` | `std::string rawHeaders` | 原始 Header 文本。 |
| `headers` | `HttpHeadersWrapper headers` | 结构化 Header。 |

## A.16 `HttpResponse` 全字段速查

| 字段 | 声明/默认 | 作用 |
|---|---|---|
| `success` | `bool success = false` | 传输层是否成功完成。 |
| `error` | `std::string error` | 错误描述。 |
| `content` | `std::string content` | 响应体。 |
| `code` | `long code = 0` | 最终 HTTP 状态码。 |
| `org_headers` | `std::string org_headers` | 全部原始 Header 块。 |
| `headers` | `HttpHeadersWrapper headers` | 最终响应 Header，Trailer 也会合并到此处便于统一查询。 |
| `date` | `std::optional<std::chrono::system_clock::time_point> date` | 最终响应头中的Date。 |
| `trailers` | `HttpHeadersWrapper trailers` | HTTP Trailer 头部。 |
| `rawTrailers` | `std::string rawTrailers` | 原始 HTTP Trailer 文本。 |
| `cookies` | `HttpCookiesWrapper cookies` | 最终响应 Set-Cookie。 |
| `curl_code` | `CURLcode curl_code = CURLE_OK` | 原始 CURLcode。 |
| `errorCategory` | `HttpErrorCategory errorCategory = HttpErrorCategory::None` | 封装层错误分类。 |
| `requestId` | `std::string requestId` | 请求追踪 ID。 |
| `final_raw_headers` | `std::string final_raw_headers` | 最终响应原始 Header。 |
| `headerHistory` | `std::vector<HttpHeaderBlock> headerHistory` | 重定向 Header 历史。 |
| `effectiveUrl` | `std::string effectiveUrl` | 最终有效 URL。 |
| `contentType` | `std::string contentType` | 响应 Content-Type。 |
| `primaryIp` | `std::string primaryIp` | 实际远端 IP。 |
| `negotiatedHttpVersion` | `std::string negotiatedHttpVersion` | 实际 HTTP 版本。 |
| `redirectCount` | `long redirectCount = 0` | 重定向次数。 |
| `proxyConnectCode` | `long proxyConnectCode = 0` | HTTP 代理 CONNECT 隧道响应码。 |
| `attempts` | `int attempts = 0` | 请求尝试次数。 |
| `downloadedBytes` | `curl_off_t downloadedBytes = 0` | 下载字节数。 |
| `uploadedBytes` | `curl_off_t uploadedBytes = 0` | 上传字节数。 |
| `totalTime` | `std::chrono::microseconds totalTime{0}` | 总耗时。 |
| `nameLookupTime` | `std::chrono::microseconds nameLookupTime{0}` | DNS 耗时。 |
| `connectTime` | `std::chrono::microseconds connectTime{0}` | TCP 连接耗时。 |
| `tlsHandshakeTime` | `std::chrono::microseconds tlsHandshakeTime{0}` | TLS 握手时间点。 |
| `firstByteTime` | `std::chrono::microseconds firstByteTime{0}` | 首字节时间。 |

## A.17 `RateLimitPolicy` 全字段速查

| 字段 | 声明/默认 | 作用 |
|---|---|---|
| `requestsPerSecond` | `double requestsPerSecond = 0.0` | 每秒最大请求数，0 表示不限。 |
| `burst` | `std::size_t burst = 1` | Token Bucket 突发容量。 |
| `maxConcurrent` | `std::size_t maxConcurrent = 0` | 最大并发请求数，0 表示不限。 |

## A.18 `CircuitBreakerPolicy` 全字段速查

| 字段 | 声明/默认 | 作用 |
|---|---|---|
| `failureThreshold` | `std::size_t failureThreshold = 0` | 连续失败多少次后熔断，0 表示关闭。 |
| `openDuration` | `std::chrono::milliseconds openDuration{30000}` | 熔断保持时间。 |
| `halfOpenMaxRequests` | `std::size_t halfOpenMaxRequests = 1` | 半开状态探测请求数。 |
| `countHttp5xx` | `bool countHttp5xx = true` | 是否把 HTTP 5xx 计为失败。 |

## A.19 `HttpMetricsEvent` 全字段速查

| 字段 | 声明/默认 | 作用 |
|---|---|---|
| `method` | `std::string method` | HTTP 请求方法。 |
| `url` | `std::string url` | 请求 URL。 |
| `statusCode` | `long statusCode = 0` | HTTP 状态码。 |
| `curlCode` | `CURLcode curlCode = CURLE_OK` | libcurl 错误码。 |
| `transportOk` | `bool transportOk = false` | 传输是否成功。 |
| `attempts` | `int attempts = 0` | 请求尝试次数。 |
| `downloadedBytes` | `curl_off_t downloadedBytes = 0` | 下载字节数。 |
| `uploadedBytes` | `curl_off_t uploadedBytes = 0` | 上传字节数。 |
| `totalTime` | `std::chrono::microseconds totalTime{0}` | 总耗时。 |

## A.20 `HttpMetricsSnapshot` 全字段速查

| 字段 | 声明/默认 | 作用 |
|---|---|---|
| `totalRequests` | `std::uint64_t totalRequests = 0` | 总请求数。 |
| `transportSuccess` | `std::uint64_t transportSuccess = 0` | 传输成功数。 |
| `http2xx` | `std::uint64_t http2xx = 0` | 2xx 数量。 |
| `http3xx` | `std::uint64_t http3xx = 0` | 3xx 数量。 |
| `http4xx` | `std::uint64_t http4xx = 0` | 4xx 数量。 |
| `http5xx` | `std::uint64_t http5xx = 0` | 5xx 数量。 |
| `failedRequests` | `std::uint64_t failedRequests = 0` | 失败请求数。 |
| `totalRetries` | `std::uint64_t totalRetries = 0` | 总重试次数。 |
| `downloadedBytes` | `std::uint64_t downloadedBytes = 0` | 总下载字节数。 |
| `uploadedBytes` | `std::uint64_t uploadedBytes = 0` | 总上传字节数。 |

## A.21 `HttpClientOptions` 全字段速查

| 字段 | 声明/默认 | 作用 |
|---|---|---|
| `authProvider` | `std::shared_ptr<IAuthProvider> authProvider` | 可选认证提供器。 |
| `middleware` | `std::vector<std::shared_ptr<IHttpMiddleware>> middleware` | Middleware 列表。 |
| `requestInterceptors` | `std::vector<RequestInterceptor> requestInterceptors` | 请求前拦截器。 |
| `responseInterceptors` | `std::vector<ResponseInterceptor> responseInterceptors` | 响应后拦截器。 |
| `rateLimit` | `RateLimitPolicy rateLimit` | 客户端限流策略。 |
| `circuitBreaker` | `CircuitBreakerPolicy circuitBreaker` | 客户端熔断策略。 |
| `metricsCollector` | `std::shared_ptr<IMetricsCollector> metricsCollector` | 可选 Metrics 收集器。 |
| `refreshAuthOnUnauthorized` | `bool refreshAuthOnUnauthorized = true` | 401 时是否自动刷新认证并重放一次。 |
| `maxTotalConnections` | `long maxTotalConnections = 0` | curl_multi 最大总连接数。 |
| `maxHostConnections` | `long maxHostConnections = 0` | curl_multi 单主机最大连接数。 |
| `maxConcurrentStreams` | `long maxConcurrentStreams = 0` | HTTP/2/3 最大并发流建议值。 |

## A.API 公共函数/方法速查

下面按类型列出公开函数族。重载只在参数/默认配置来源上不同，核心作用合并说明；具体使用示例见正文对应章节。

### 全局函数

| 函数 | 作用 |
|---|---|
| `Initialize()` | 显式触发/查询 libcurl 全局初始化结果。MultiSSL 如需 `curl_global_sslset()`，必须在它之前调用。 |
| `UrlEncode()` | URL 百分号编码，超出 libcurl `int` 长度接口上限时抛 `std::length_error`。 |
| `UrlDecode()` | URL 百分号解码，同样保护长度上限。 |

### `UrlParams`

| 函数 | 作用 |
|---|---|
| 构造函数 / `Parse()` | 从 query 字符串解析并保序保存参数，允许重复名称。 |
| `ToString()` | 序列化 query，可选前导 `?`。 |
| `Get()` / `GetAll()` | 读取第一个/全部同名参数。 |
| `Set()` | 覆盖全部同名参数。 |
| `Add()` | 追加参数，可控制是否保留 `=`。 |
| `Remove()` / `Has()` | 删除/判断参数。 |
| `Size()` / `Empty()` / `Clear()` | 容器状态操作。 |
| `GetAllParams()` / `GetAllParamNames()` | 获取文本项/去重名称列表。 |
| `operator[]` | 获取或创建指定名称的第一个值。 |
| `begin/end/cbegin/cend` | 直接遍历底层保序参数列表。 |

### `HttpHeadersWrapper`

| 函数 | 作用 |
|---|---|
| 构造函数 / `ParseHeaders()` | 解析原始多行 Header。 |
| `AddHeader()` | 追加同名 Header，不覆盖。 |
| `SetHeader()` | 覆盖全部同名 Header。 |
| `SetDefaultHeader()` | 仅不存在时写入。 |
| `AppendHeader()` | 向第一个同名字段追加值。 |
| `EraseHeader()` / `IsExist()` | 删除/判断字段。 |
| `GetHeaderValue()` / `GetHeaderValues()` | 读取第一个/全部同名值。 |
| `GetKeys()` / `GetAllHeaders()` | 获取名称列表/序列化全部 Header。 |
| `Items()` / `begin/end` | 直接访问或遍历结构化 Header。 |
| `Clear()` / `Empty()` / `Size()` | 容器状态操作。 |

### `HttpCookiesWrapper`

| 函数 | 作用 |
|---|---|
| `Cookie::ToSetCookieString()` | 将单个 `Cookie` 对象序列化为 `Set-Cookie` 值。 |
| 构造函数 | 从 `Set-Cookie` 列表或请求 `Cookie` 字符串创建容器。 |
| `Merge()` / `MergedWith()` | 原地合并 / 返回合并后的新 Cookie 集合，可控制是否覆盖同身份 Cookie。 |
| `ParseFromSetCookieHeaders()` | 解析响应 `Set-Cookie`。 |
| `ParseFromCookieString()` | 解析请求 `Cookie`。 |
| `ExtractSetCookieHeaders()` | 从原始 Header 中抽取 `Set-Cookie` 行。 |
| `SetCookie()` | 写入 Cookie；字符串重载只设置 name/value。 |
| `EraseCookie()` / `IsExist()` | 按名称或 Name/Domain/Path 身份删除 / 判断同名 Cookie。 |
| `RemoveEmptyCookies()` | 删除空名称等无效项；V4.2.6 起合法空值 `name=` 不会被误删。 |
| `GetCookieValue()` / `GetCookies()` | 读取第一个同名 Cookie 的值 / 获取全部同名 Cookie 对象。 |
| `GetAllKeys()` | 获取 Cookie 名称列表，可选择忽略空值。 |
| `ToRequestCookieString()` | 序列化为请求 `Cookie` Header 的值。 |
| `ToSetCookieHeaders()` | 序列化为多条响应式 `Set-Cookie` 值。 |
| `GetAllCookies()` | 返回全部 Cookie 对象，可选择忽略空值。 |
| `Size()` / `Empty()` / `Clear()` | 容器状态操作。 |

### 上传源与 Multipart

| 类型/函数 | 作用 |
|---|---|
| `MultipartPart::Field()` | 创建普通 Multipart 文本字段。 |
| `MultipartPart::File()` | 创建文件字段，可指定 Content-Type/文件名。 |
| `MultipartPart::IsFile()` | 根据 `filePath` 判断是否文件字段。 |
| `IUploadSource::Read()` | 流式读取上传数据。 |
| `IUploadSource::Rewind()` | 自动重试/认证重放前回退数据源。 |
| `IUploadSource::Size()` | 返回可选上传总大小。 |
| `MemoryUploadSource` | 用内存字符串实现 `IUploadSource`。 |
| `CallbackUploadSource` | 用业务回调实现 `IUploadSource`。 |
| `FileUploadSource` | 用本地文件实现 `IUploadSource`；`Valid()` 检查打开状态，`Path()` 返回路径。 |

### Transport / Mock

| 类型/函数 | 作用 |
|---|---|
| `IHttpTransport::Send()` | 传输层统一抽象。 |
| `CurlTransport::Send()` | 使用可复用 libcurl easy handle 执行真实网络请求。 |
| `CurlTransport::Valid()` | 检查 easy handle 是否有效。 |
| `CurlTransport::NativeHandle()` | 获取底层 `CURL*`；不得与 `Send()` 并发操作。 |
| `GetCookieList()` / `ClearCookies()` / `ClearSessionCookies()` / `FlushCookies()` | 操作 libcurl Cookie Engine。 |
| `MockTransport::AddRule()` | 增加 predicate + responder 动态规则。 |
| `MockTransport::AddStaticResponse()` | 增加 Method + URL 精确匹配固定响应。 |
| `MockTransport::Clear()` | 清空规则。 |
| `MockTransport::Send()` | 根据规则返回 Mock 响应，本身不执行 CurlTransport 的自动重试循环。 |

### 认证 / Middleware / Metrics

| 类型/函数 | 作用 |
|---|---|
| `IAuthProvider::Apply()` | 发送前向请求/配置写入认证信息。 |
| `CanRefresh()` | 判断当前响应是否应刷新认证。 |
| `Refresh()` | 刷新认证材料；默认实现可返回 false。 |
| `Version()` | 返回认证版本，用于并发刷新协调。 |
| `BasicAuthProvider` | 写入 username/password/httpAuth。 |
| `BearerAuthProvider` | 写入 Bearer token；支持刷新回调；`SetToken()` 主动更新 Token，`GetToken()` 返回当前 Token 副本，`Version()` 用于并发刷新协调。 |
| `ApiKeyAuthProvider` | 将 API Key 写入 Header 或 Query，并同步登记 Debug 敏感字段。 |
| `IHttpMiddleware::Before()` | 请求发送前修改/短路。 |
| `IHttpMiddleware::After()` | 响应完成后观察/修改。 |
| `IMetricsCollector::OnRequestCompleted()` | 接收单次请求 Metrics 事件。 |
| `BasicMetricsCollector::OnRequestCompleted()` | 线程安全地累积一次请求指标。 |
| `BasicMetricsCollector::Snapshot()` | 返回累计指标快照。 |
| `BasicMetricsCollector::Reset()` | 清零累计指标。 |

### `HttpResponse` 成功判断

| 函数 | 作用 |
|---|---|
| `TransportOk()` | `success && curl_code == CURLE_OK`，只判断传输层。 |
| `Ok()` | `TransportOk()` 且最终 HTTP 状态为 2xx。 |

### `HttpRequest`

`HttpRequest` 是无 Session 的静态快捷入口，每次调用创建独立 `CurlTransport`。公开函数族：

| 函数族 | 作用 |
|---|---|
| `Send()` | 发送完整 `HttpRequestData`。 |
| `Request(HttpMethod/字符串 method, ...)` | 发送任意 HTTP Method。 |
| `Get/Head/Post/Put/Delete/Patch/Options/Trace/Connect` | 常用 Method 快捷入口。 |
| `RequestJson/PostJson/PutJson/PatchJson` | JSON 快捷入口，自动补默认 JSON Header。 |
| `PostForm()` | `application/x-www-form-urlencoded` 表单。 |
| `UploadMultipart()` | Multipart 上传。 |
| `UploadFile()` | 流式文件上传。 |
| `Download()` | 文件下载；非 Resume 默认使用同目录临时文件后原子/安全提交。 |

### `HttpClient`

| 函数族 | 作用 |
|---|---|
| 构造函数 | 创建可复用 Session Client，可传默认 `RequestOptions`、`HttpClientOptions` 与可选 Transport。 |
| 移动构造 / 移动赋值 | 转移 Client 所有权；复制构造/复制赋值明确禁用。 |
| `SetDefaultOptions()` | 替换默认请求配置。 |
| `DefaultOptions()` | 读取默认请求配置。 |
| `SetClientOptions()` / `ClientOptions()` | 设置/读取长期客户端策略。 |
| `SetTransport()` / `Transport()` | 替换/获取传输层，可注入 Mock。 |
| `AddMiddleware()` | 追加 Middleware。 |
| `AddRequestInterceptor()` / `AddResponseInterceptor()` | 追加请求/响应拦截器。 |
| `Send()` / `Request()` | 发送完整请求或任意 Method。 |
| `Get/Head/Post/Put/Delete/Patch/Options` | Session 版本常用 Method。 |
| `RequestJson/PostJson/PutJson/PatchJson/PostForm/UploadMultipart/UploadFile/Download` | Session 版本高级快捷入口。 |
| `GetCookieList/ClearCookies/ClearSessionCookies/FlushCookies` | 管理当前 CurlTransport Cookie Engine。 |

### `AsyncHttpClient`

| 函数族 | 作用 |
|---|---|
| 构造/析构 | 创建 curl_multi 事件循环；析构停止并完成资源清理；复制构造/复制赋值明确禁用。 |
| `SendAsync()` | 返回 `std::future<HttpResponse>`。 |
| `SendWithCallback()` | 完成后调用业务 callback。 |
| `GetAsync/HeadAsync/PostAsync/PutAsync/DeleteAsync/PatchAsync/OptionsAsync` | 常用异步 Method。 |
| `PostJsonAsync/PutJsonAsync/PatchJsonAsync` | 异步 JSON 快捷入口。 |
| `RequestAsync()` | 异步任意 Method。 |
| `PendingCount()` | 近实时等待/执行数量，仅用于观测，不是同步原语。 |

### C++20 Coroutine

| 函数/类型 | 作用 |
|---|---|
| `HttpAwaitable::await_ready()` | 检查结果是否已完成。 |
| `await_suspend()` | 注册 coroutine 恢复句柄。 |
| `await_resume()` | 取得 `HttpResponse`。 |
| `GetAwaitable()` | 创建 GET awaitable。 |
| `PostAwaitable()` | 创建 POST awaitable。 |

### 最后执行的逃生口：`nativeCurlOptions`

`RequestOptions::nativeCurlOptions(CURL*)` 不是普通便捷函数，而是封装最后执行的原生 option 回调。它可以覆盖前面设置的 TLS、重定向、代理、callback 等安全配置；一旦使用，相关行为应以回调最终写入的 libcurl option 为准。V4.2.7 的 TLS Debug 信息在它之前生成，因此如果这里覆盖 CA/SSL_OPTIONS，日志只能表示封装层原始决策。

---

# V4.2.7 发布与验证说明

正式交付文件：

```text
curl_ex.h
curl_ex.cpp
curl_ex_v4.2.7.md
```

## V4.2.7 根因、修复逻辑与回归重点

### 根因

Windows MultiSSL 当前激活 OpenSSL 时，`CURLINFO_CAINFO == null`、`CURLINFO_CAPATH == null`，而 `verifyPeer=true`。OpenSSL 没有可靠 Trust Store，就无法从服务器证书链构建到可信根，最终返回 `CURLE_PEER_FAILED_VERIFICATION`，典型 OpenSSL verify result 为 20：`unable to get local issuer certificate`。

同时旧版 `IsSchannelBackend()` 对 `OpenSSL/3.6.1 (Schannel)` 只做子串搜索，误把“括号中的未激活 Backend”当成当前 Backend。这虽然不是 error 20 的直接根因，但会让 Schannel 专属吊销 bit 错误参与 OpenSSL 的 `SSL_OPTIONS` 计算，必须在同一修复中纠正。

### 修复后的配置流程

```text
RequestOptions::tls
        │
        ├─ verifyPeer / verifyHost
        │      └─ 继续严格证书链 + hostname 验证
        │
        ├─ caFile / caPath 是否显式设置？
        │      ├─ 是 -> 设置 CAINFO/CAPATH，不叠加 Native CA bit
        │      └─ 否
        │          └─ Windows + 当前 Backend=OpenSSL + useNativeCa=true + libcurl >= 7.71
        │                -> SSL_OPTIONS |= CURLSSLOPT_NATIVE_CA
        │
        ├─ 当前激活 Backend 是 Schannel？
        │      └─ 是 -> 按 revocationPolicy 合并 Schannel 吊销 bit
        │
        └─ pinnedPublicKey 非空？
               └─ 独立设置 CURLOPT_PINNEDPUBLICKEY

最终：
sslOptions 一次性写入 CURLOPT_SSL_OPTIONS
HTTPS Proxy 时同样写入 CURLOPT_PROXY_SSL_OPTIONS
```

### 为什么只对 Windows + OpenSSL 注入 Native CA bit

本次缺陷发生在 Windows 下当前激活的 OpenSSL Backend。Schannel 本身已经以 Windows Certificate Store 作为证书信任来源，因此无需 `CURLSSLOPT_NATIVE_CA`；其他 TLS Backend 在 V4.2.7 中继续保持 V4.2.6 行为，避免把一次 OpenSSL Trust Store 修复扩张成跨 Backend 的语义修改。

### 为什么没有改写 TLS 失败的 `response.error`

需求中建议过针对 error 20 增加更明确的自动诊断文本。V4.2.7 选择**不改写现有错误字符串语义**：原始 libcurl Error Buffer 继续原样保留在 `response.error`，详细 Trust Store 状态通过 `DebugOptions` 输出。这样既能定位 `CAINFO/CAPATH/Native CA`，又不会让已有依赖错误文本的调用方在小版本升级后发生行为变化。

### 为什么不直接切换成 Schannel

你已经验证过显式选择 Schannel 能恢复请求，但 V4.2.7 没有把“强制 Schannel”作为默认修复，因为那会改变当前 MultiSSL/OpenSSL 的 TLS Backend 选择语义，也可能影响 Cipher、客户端证书、OpenSSL 特有能力和部署预期。V4.2.7 选择的是更小的修复：**保留当前 OpenSSL Backend，只为它补上 Windows 系统信任根来源**。

### 为什么显式 CA 时关闭 Native CA bit

libcurl 官方定义中 `CURLSSLOPT_NATIVE_CA` 与 `CAINFO/CAPATH` 是追加关系。如果默认 Native CA 始终开启，那么：

```text
用户设置 private-ca.pem
+
Windows 所有系统可信根
```

会一起参与信任，调用方无法通过 private CA 真正收窄信任范围。因此 V4.2.7 把“显式 CA”视为更高优先级：只要 `caFile/caPath` 任意一个非空，就不额外设置 Native CA bit。

### 为什么 Pinning 不受影响

Pinning 仍然在普通 CA/hostname 验证之后作为独立约束存在：

```text
系统/自定义 CA 链验证
        +
hostname 验证
        +
CURLOPT_PINNEDPUBLICKEY
```

因此即使 Windows 系统信任库中被加入了新的企业根证书或本地抓包根证书，只要公钥 Pin 不匹配，Pinning 仍会失败。

### 实际建议的 Windows 生产默认值

```cpp
RequestOptions options;
options.tls.verifyPeer = true;
options.tls.verifyHost = true;
options.tls.useNativeCa = true;
options.tls.minVersion = TlsVersion::Tls1_2;
```

标准公网 HTTPS 不需要再携带 `cacert.pem`。

### 显式选择 MultiSSL Backend

如果业务确实需要固定 OpenSSL：

```cpp
const CURLsslset sslSet =
    curl_global_sslset(CURLSSLBACKEND_OPENSSL, nullptr, nullptr);

if (sslSet != CURLSSLSET_OK) {
    // 处理 UNKNOWN_BACKEND / TOO_LATE 等情况。
}

const CURLcode init = ytpp::curl_ex::Initialize();
```

固定 Schannel：

```cpp
const CURLsslset sslSet =
    curl_global_sslset(CURLSSLBACKEND_SCHANNEL, nullptr, nullptr);

const CURLcode init = ytpp::curl_ex::Initialize();
```

**调用顺序必须是 `curl_global_sslset()` 在前，`Initialize()` / 任何 curl_ex 请求在后。** 一旦 libcurl 全局初始化或 TLS Backend 已经被选定，再切换可能返回 `CURLSSLSET_TOO_LATE`。

### V4.2.7 本次实际验证记录

本次交付完成后执行了以下检查：

1. **源码差异审计**：与压缩包内原始 V4.2.6 对比，公开头文件除版本号外只在 `TlsOptions` 末尾新增 `useNativeCa`；实现层行为改动集中在 MultiSSL Backend 识别、Native CA/吊销 bitmask 合并和 TLS Debug 诊断，没有重构请求/重试/Session/Async 架构。
2. **定向静态回归**：确认 `verifyPeer/verifyHost` 仍保留、`CURLOPT_PINNEDPUBLICKEY` 未删除、自定义 `caFile/caPath` 会抑制 Native CA bit、目标 TLS 和 HTTPS Proxy 各只写一次合并后的 SSL options。
3. **MultiSSL 解析单元测试**：验证 `OpenSSL/3.6.1 (Schannel)` -> `OpenSSL/3.6.1`、`(OpenSSL/3.6.1) Schannel` -> `Schannel`，并覆盖多候选 Backend/首尾空白。
4. **C++17 完整翻译单元编译**：在当前验证环境中完成 `curl_ex.cpp` 全量编译；由于基线源码的 `ParseHttpDate()` 本来使用 Windows `_mkgmtime64`，非 Windows 验证时仅对该既有函数使用兼容映射，不改变交付源码。
5. **文档-头文件字段一致性检查**：逐项比对 21 个公开数据结构，包含 `RequestOptions` 46 个字段、`HttpResponse` 29 个字段，确认查询表没有漏项；`HttpResponse::date` 已正式纳入说明。

当前验证环境不是你的 Windows/MSVC + libcurl 8.21.0 MultiSSL 运行环境，因此**没有在这里伪造“百度/Apple 已实机通过”的结论**。Windows Native CA 分支最终仍应在你的实际构建上执行下面的网络回归矩阵。

### 建议回归矩阵

在你的 Windows/MSVC + libcurl 8.21.0 MultiSSL 环境中，发布前建议至少验证：

```text
1. OpenSSL + useNativeCa=true + https://www.baidu.com      -> CURLE_OK
2. OpenSSL + useNativeCa=true + https://account.apple.com -> 证书链通过
3. hostname mismatch                                      -> 必须失败
4. 自签名且未加入信任                                     -> 必须失败
5. 正确 pinnedPublicKey                                   -> 成功
6. 错误 pinnedPublicKey                                   -> CURLE_SSL_PINNEDPUBKEYNOTMATCH
7. 显式 Schannel                                           -> 正常
8. 显式 OpenSSL                                            -> 正常
9. caFile 自定义私有 CA                                    -> 按自定义 CA 行为验证
10. debug.enabled=true                                     -> 检查 active Backend / Native CA / CAINFO/CAPATH 日志
```

本次交付环境无法替代你的 Windows 实机证书库，因此“Windows Native CA 真实握手通过”仍应在你的项目环境完成最终集成测试；源码侧已经对 bitmask 合并、MultiSSL 激活 Backend 解析、版本条件和文档一致性做定向检查。

---

# V4.2.6 发布与验证说明（历史基线，V4.2.7 继续保留）

正式源码文件为：

```text
curl_ex_v4.2.6.h
curl_ex_v4.2.6.cpp
curl_ex_v4.2.6.md
```

建议项目首次使用时至少执行：C++17/C++20 编译、基础 HTTP smoke test、目标 TLS backend 的 HTTPS 测试，以及下载/代理/认证等与你业务相关的集成测试。Windows 项目建议使用 `/W4 /permissive- /utf-8`；工程层可以定义 `NOMINMAX`，但本公共头文件自身也已经防御 `Windows.h` 的 `min/max` 宏污染。

---

## V4.2.6 本次生产审计验证记录

本节记录本次 V4.2.6 交付前实际执行过的验证，目的是区分“源码设计目标”和“已经在当前审计环境真实跑过的测试”。测试通过不等于对所有操作系统、TLS Backend、代理产品和服务端实现作绝对零缺陷承诺；生产项目仍应保留自身 CI、目标平台编译和业务集成测试。

### 审计环境

```text
Linux x86_64
GCC 14.2.0
Clang 17.0.0
C++17 / C++20
libcurl 8.10.1 + OpenSSL 3.5.5
libcurl 8.14.1 + OpenSSL 3.5.5
libcurl 8.14.1 + GnuTLS 3.8.9
```

Windows 专用提交路径（`ReplaceFileW` / `MoveFileExW`）和 Schannel 在本次 Linux 容器中无法进行真实运行测试；这些部分按公开 API 契约和条件编译路径审计，最终 Windows 项目仍应使用实际 MSVC/libcurl/Schannel 组合执行 CI。OpenSSL 与 GnuTLS 则都已经进行真实 TLS 运行回归。

### 已执行的编译与静态边界验证

- GCC × C++17：严格 warning + `-Werror` 编译通过。
- GCC × C++20：严格 warning + `-Werror` 编译通过。
- Clang × C++17：严格 warning + `-Werror` 编译通过。
- Clang × C++20：严格 warning + `-Werror` 编译通过。
- GCC 最终 warning 集包含 conversion、sign-conversion、shadow、format、null-dereference、duplicated-cond/branch、logical-op 等；Clang 对应启用 conversion、sign-conversion、shadow、format、null-dereference。Linux/LP64 下的 `-Wuseless-cast` 没有作为跨平台交付门槛，因为其中若干显式转换用于 Windows/LLP64 类型边界，删除它们反而会弱化可移植意图。
- 对最低版本 `LIBCURL_VERSION_NUM=0x073800`（7.56.0）条件分支执行过模拟编译；由于本次环境没有真实 7.56.0 SDK/运行库，该项仅用于预处理/条件路径检查，不替代真实旧版 CI。
- 公共头文件执行过 `min/max` 宏预先存在场景的独立包含测试，包含后宏可恢复，标准库 `(Type::max)()` 风格调用不受污染。
- 最终源码使用 Clang ASAN + UBSAN 重新编译并运行同步/异步/C++20 coroutine 回归，`FAILURES=0`，没有 sanitizer 报告。
- 对所有主要 libcurl C callback/扩展回调异常边界进行源码扫描：Write/Header/Upload Read/Seek/Progress、Debug logger、`nativeCurlOptions`、Retry reset、Middleware/Interceptor/Mock 等均有 C++ 异常兜底，不让异常穿过 libcurl C ABI。
- 尝试对整个 5700+ 行实现执行 GCC `-fanalyzer` 路径分析时，分析进程因当前容器资源限制被系统终止，因此 **不把 `-fanalyzer` 记为通过项**；最终结论依赖双编译器高告警、Sanitizer、针对性源码审计和真实协议回归，而不是虚报该工具结果。

### V4.2.6 新增缺陷的定向 Red/Green 回归

以下问题均有可复现测试，先在缺陷语义上失败，再在 V4.2.6 当前实现上通过：

```text
安全重定向默认值：HTTP 目标默认拒绝
默认最低 TLS：Tls1_2
常见认证 Header：9 个内置敏感名称全部脱敏
自定义 sensitiveQueryParameters：首尾空白不再导致匹配失效
显式空值 Cookie：保留并发送 name=
TLS maxVersion=Default：不注入无必要的 MAX_DEFAULT
多 URI Link Header：hideSensitiveUrlData=true 时整体脱敏
```

### 已执行的真实 HTTP/异步状态机回归

当前最终源码重新编译后，本地 HTTP 服务验证：

- `Initialize()`、同步 GET、HEAD、自定义 PATCH。
- 二进制 POST Body（包含 `\0`）按明确长度发送。
- 默认 HTTP 重定向目标拒绝；显式 `allowHttpsToHttp=true` 后可正常跟随。
- 显式空值 Cookie、Session Cookie Engine。
- 内存 Body 上限、Header 上限。
- 503 重试与 attempt 计数。
- Debug 常见凭据 Header 脱敏。
- 同步 Bearer 401 refresh + replay。
- 24 路 Async multi 并发与 `PendingCount()` 完成边界。
- 取消请求。
- 普通下载同目录临时文件提交与最终大小验证。
- Resume 下载。
- CRLF 原始 Header 注入拒绝。
- C++20 异步认证刷新、completion callback、coroutine awaitable。

上述最终普通运行回归为 `FAILURES=0`；同一组核心同步/异步/coroutine 路径在 ASAN + UBSAN 下重新执行也为 `FAILURES=0`。

本次审计过程中还执行过 JSON/Form、PUT/DELETE/OPTIONS、gzip、Cookie Engine 隔离恢复、敏感凭据重定向 fail-closed、旧 CUSTOMREQUEST 重定向语义、下载回滚失败提升、重定向中间 Body 等更细分回归；最后两项源码修订仅涉及 TLS 版本值组合与 Debug `Link` 脱敏，并分别由上述定向测试及最终核心回归再次覆盖。

### 已执行的真实 TLS / Backend 矩阵

使用本地 CA、带 `subjectAltName=IP:127.0.0.1` 的测试服务器证书、TLS 1.3-only 服务以及真实 Public Key Pin，对 **三套实际 libcurl 运行时** 分别重新编译并执行相同测试：

```text
libcurl 8.10.1 / OpenSSL 3.5.5   FAILURES=0
libcurl 8.14.1 / OpenSSL 3.5.5   FAILURES=0
libcurl 8.14.1 / GnuTLS 3.8.9    FAILURES=0
```

每套均验证：

- 未信任 CA 被拒绝。
- 自定义 CA + SAN 主机验证成功。
- 正确 Public Key Pin 成功。
- 错误 Pin 返回 `CURLE_SSL_PINNEDPUBKEYNOTMATCH`。
- 默认 TLS 1.2 最低版本可以协商 TLS 1.3-only 服务。
- 显式最高 TLS 1.2 会拒绝 TLS 1.3-only 服务。
- HTTPS → HTTP 重定向默认拒绝。
- 显式放宽 HTTP 重定向后可正常跟随。

### V4.2.6 本轮重新审计新确认并修复的问题

```text
RedirectOptions::allowHttpsToHttp 原默认 true，与企业安全默认及部分手册描述冲突
TlsOptions 默认最低版本依赖旧 libcurl/TLS Backend，无法统一保证 TLS 1.2 基线
Debug 敏感 Header 表与重定向敏感 Header 表漂移，X-Auth-Token 等可进入日志
自定义 sensitiveQueryParameters 未 Trim，配置首尾空白会造成脱敏失效
RequestOptions::cookies 序列化时忽略空值，合法 name= Cookie 被静默丢弃
maxVersion=Default 仍显式 OR MAX_DEFAULT，产生无必要的旧 backend 兼容面
Link Header 多 URI 场景只按单 URL 脱敏，后续 URI 的 token/query 可能漏入 Debug 日志
```

V4.2.5 已经修复并由 V4.2.6 继续保留的旧问题，例如 raw curl verbose 绕过脱敏、敏感凭据自动重定向 fail-closed、旧 CUSTOMREQUEST 重定向保护、外部 sink 重试副作用、异步完成资源清理、Schannel BestEffort 兼容、Windows ReplaceFileW 标志、Resume 回滚错误等，不重复冒充为 V4.2.6 新发现项。

生产上线前仍建议在你的实际 Windows/MSVC/vcpkg triplet 上至少跑一次：Debug/Release、x86/x64（如果都交付）、实际 TLS Backend、实际代理、证书 Pin、下载目录权限/杀毒软件占用、以及真实业务服务端的集成回归。
