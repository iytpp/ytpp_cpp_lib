# curl_ex v4.2.5 使用手册

> 本文档只对应以下源码：
>
> - `curl_ex_v4.2.5.h`
> - `curl_ex_v4.2.5.cpp`
>
> 不要与旧版 `curl_ex.h`、`curl_ex.cpp`、`curl_ex_v4.h` 或之前未带版本号的示例混用。

---

## 1. 项目定位

## 1.1 V4.2.5 重要变更与生产语义

V4.2.5 是在 V4.2.4 基础上继续进行完整生产级审计后的修订版。V4.2.4 已经具备 `Initialize()`、认证刷新重放、异步完成分发、Cookie 隔离、输入终检、响应资源上限和临时文件下载提交等基础生产语义；V4.2.5 **不重复把这些旧能力算作新增功能**，本次升级重点修复以下在重新审计中实际确认的问题：

- Debug 安全边界收紧：`DebugOptions::enabled` 成为真正的日志总开关；`curlVerbose=true` 不再使用 libcurl 原始 stderr verbose，而是经过封装层过滤回调，Header/URL 继续脱敏，原始 Body/TLS record 不写入日志。Debug 格式化、脱敏和 logger 异常全部按旁路能力兜底，不能逃入同步/异步请求控制流。
- `ApiKeyAuthProvider` 会自动把 Header/Query 形式的自定义凭据名称登记为敏感字段；自定义 Query API Key 不再进入 Metrics 明文。`Location`、`Content-Location`、`Referer`、`Destination`、`Link`、`Refresh` 等 URL-bearing Header 也会执行 URL 脱敏。
- 敏感凭据与自动重定向改为“首跳正常、真实出现 3xx 才 fail-closed”：自定义敏感 Header、`RequestOptions::cookies` 和 `pinnedPublicKey` 不再因为默认 `follow=true` 就预先拒绝普通请求；但一旦首跳返回可重定向目标，默认会在发送下一跳前停止。只有明确接受相应风险时才开启对应 override。
- libcurl 8.13.0 之前的 `CURLOPT_CUSTOMREQUEST + FOLLOWLOCATION` 无法可靠服从 301/302/303 的 Method 转换语义。V4.2.5 在旧运行库上允许首跳正常执行，但真实出现重定向时会在下一跳前 fail-closed，避免把 PATCH/其他自定义 Method 错误重放到目标地址；8.13.0+ 使用 `CURLFOLLOW_OBEYCODE`，并同时检查运行期 libcurl 版本，防止“新头文件 + 旧动态库”误用新模式值。
- 最低版本兼容的重定向保护补齐：libcurl 7.58.0 之前的自定义 `Authorization` Header、7.64.0 之前的自定义 `Cookie` Header，在默认不放宽跨主机认证转发时遇到重定向会 fail-closed。
- `UrlEncode()` / `UrlDecode()` 对超过 libcurl `int` 长度上限的输入显式抛出 `std::length_error`，不再静默截断。
- 非法枚举配置改为 fail-closed：非法 `HttpMethod` 不再静默退化为 GET，非法 `HttpVersion` / `TlsVersion` / `CertificateRevocationPolicy` / `ProxyType` 会在请求预检阶段返回 `InvalidArgument`，非法 `ApiKeyLocation` 在构造认证提供器时抛出 `std::invalid_argument`。
- Schannel `BestEffort` 在 libcurl 7.70.0 之前不再退化为 `NO_REVOKE`；旧版本保持 Schannel 默认严格吊销检查，避免静默降低 TLS 安全等级。
- 自动重试对外部响应 sink 的副作用判定修正：`responseChunkCallback`/`responseStream` 一旦开始接收数据，即使随后返回失败，也会视为 sink 已被触碰；没有 `responseRetryResetCallback` 时不会贸然重放并造成重复数据。
- `AsyncHttpClient::PendingCount()` 修正 future 交付竞态：请求会在 future 置为 ready 或 callback 开始前先移出计数。该值用于近实时观测，不是完成同步原语。
- 异步完成 dispatcher 的异常兜底修正：入队失败时保留原 callback/response 并在当前线程同步交付，不再因为提前 move 而丢失完成通知。
- 异步 multi fatal 清理在无法确认 easy handle 已从 multi detach 时延迟释放 easy/mime/slist，直到 multi 栈销毁后再回收，避免异常路径提前 cleanup 仍可能附着的 handle。
- Windows 普通下载提交纠正：已有目标继续使用 `ReplaceFileW`，但不再传入 Windows 文档明确不支持的 `REPLACEFILE_WRITE_THROUGH`；不存在目标时仍使用 `MoveFileExW(..., MOVEFILE_WRITE_THROUGH)`。文档不再宣称 `ReplaceFileW` 具有不存在的 write-through 保证。
- Resume 失败后的本地完整性错误不再被忽略：如果无法把文件恢复到 `resumeFrom`，结果会提升为 `CURLE_WRITE_ERROR / Download`，同时保留原请求错误文本。
- 清理了重复错误分类赋值和重复 TTFB Debug 输出等批量编辑残留，并同步修正公开注释/手册与真实实现不一致之处。

V4.2.4 已有且 V4.2.5 继续保留的重要基础语义，包括：默认 128 MiB `response.content` 缓存上限、2 MiB Header/Trailer 上限、非 Resume 下载的同目录临时文件提交、`allowHttpsToHttp=false` 的仅 HTTPS 重定向目标限制、`enableCookieEngine=false` 的 Session Cookie 隔离，以及认证重放/异步完成线程模型等。后文各专题仍给出完整说明。

### 默认资源限制的设计决定

V4.2.5 保留 `128 MiB` 内存响应缓存上限和 `2 MiB` 单 attempt Header/Trailer 上限。这两个值对常规业务足够宽松，同时可以避免失控响应无限消耗内存；需要处理超大内存响应或异常巨大的 Header 时，可显式设为更大值或 `0`。对于大文件，推荐直接使用流式 sink 或文件下载接口，而不是依赖 `response.content`。


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
#include "curl_ex_v4.2.5.h"

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

V4.2.5 的普通下载默认使用目标同目录临时文件。只有请求完整成功并通过最终检查后才提交到目标路径，因此网络中断、HTTP 失败或回调失败不会提前破坏已有文件。Resume 模式为了续写现有文件仍会直接操作目标文件。


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

推荐默认保持 `Auto`，除非目标服务存在明确兼容性要求。v4.2.5 在 `Auto` 时不会主动设置 `CURLOPT_HTTP_VERSION`，而是完全沿用当前 libcurl 的默认 HTTP 协商策略。

---

## 21. TLS

```cpp
options.tls.verifyPeer = true;
options.tls.verifyHost = true;
```

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

### TLS Backend 与 libcurl 版本兼容性

`TlsVersion::Default` 的最低/最高版本都保持 `Default` 时，v4.2.5 不会主动调用 `CURLOPT_SSLVERSION`，直接采用当前 libcurl/TLS Backend 的默认协商策略。这样可以减少对旧版 wolfSSL 等 Backend 的额外约束。

需要特别注意：

- `verifyStatus=true` 对应 OCSP Stapling；当前 libcurl 文档仅列出 OpenSSL/GnuTLS Backend 支持，默认关闭。
- `CertificateRevocationPolicy::BestEffort` 主要用于 Windows Schannel。`CURLSSLOPT_REVOKE_BEST_EFFORT` 从 libcurl 7.70.0 提供；更旧版本无法表达 BestEffort，V4.2.5 会保持 Schannel 默认严格吊销检查，而不是退化为 `NO_REVOKE`。
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
options.redirect.allowHttpsToHttp = true;
options.redirect.forwardAuthToOtherHosts = false;
options.redirect.forwardSensitiveHeadersToOtherHosts = false;
options.redirect.allowExplicitCookiesOnRedirects = false;
options.redirect.allowUnpinnedRedirects = false;
```

普通无凭据请求仍默认跟随重定向。`forwardAuthToOtherHosts=false` 对应 libcurl 标准认证信息的跨主机保护；V4.2.5 对封装无法在 FOLLOWLOCATION 内安全逐跳判断的敏感数据采用“首跳正常、下一跳前 fail-closed”：

- 自定义敏感 Header（内置常见名称或 `debug.sensitiveHeaders`）：首跳照常发送；如果服务器真实返回重定向，默认停止，不把该 Header 自动带入下一跳。明确接受自动重定向链可能跨主机转发的风险时才设置 `forwardSensitiveHeadersToOtherHosts=true`。
- `RequestOptions::cookies`：它最终使用 `CURLOPT_COOKIE`，libcurl 会把这种显式 Cookie 继续用于后续重定向。默认在真实出现重定向后停止；明确接受风险时才设置 `allowExplicitCookiesOnRedirects=true`。Session Cookie Engine 仍按 libcurl 自身的 Domain/Path/Secure 等 Cookie 规则处理，不等同于这个显式 Cookie override。
- `tls.pinnedPublicKey`：首跳仍正常执行 Pin 校验；如果出现重定向，默认停止，因为同一个 `CURLOPT_PINNEDPUBLICKEY` 不能被封装视为“已经安全覆盖所有其他 Origin”。明确接受后续跳转风险时才设置 `allowUnpinnedRedirects=true`；更安全的做法是关闭自动重定向，校验 `Location` 后为目标 Origin 建立新的请求和 Pin。
- libcurl 8.13.0 之前，自定义 Method 使用 `CURLOPT_CUSTOMREQUEST` 时，FOLLOWLOCATION 可能错误保持 Method，无法可靠服从 301/302/303 的 Method 改写规则。V4.2.5 在这类旧运行库上同样执行首跳，若真实出现重定向则在下一跳前停止；8.13.0+ 使用 `CURLFOLLOW_OBEYCODE`。

由于最低支持 libcurl 7.56.0，V4.2.5 还会保护旧版本缺失的跨主机行为：7.58.0 之前的自定义 `Authorization` Header、7.64.0 之前的自定义 `Cookie` Header，在默认不放宽认证转发时遇到重定向也会 fail-closed。

如果设置 `options.redirect.allowHttpsToHttp = false`，V4.2.5 会把允许的重定向协议收紧为仅 HTTPS。这个定义比“只检查初始 URL 是否为 HTTPS”更严格，因此 `HTTP → HTTP` 重定向也会被拒绝；这是显式安全模式的预期行为。

如果业务要求更严格，可以：

```cpp
options.redirect.allowHttpsToHttp = false;
```

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

`enableCookieEngine=false` 的 V4.2.5 语义是“本次请求不使用当前 Session Cookie 状态”。即使复用 `HttpClient` easy handle 或 `AsyncHttpClient` 的共享 Cookie Jar，本次请求也不会携带历史 Cookie；请求完成后原有 Cookie Session 仍可继续使用。


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

调用方必须保证流对象在请求结束前一直有效。

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

`resumeFrom` 在 v4.2.5 中明确用于 **GET 下载断点续传**。使用 `HttpRequest::Download()` 时，本地文件必须已经存在且大小与 `resumeFrom` 完全一致，之后才会以追加方式继续下载。

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

`hideSensitiveUrlData=true` 不只处理请求 URL，也会处理响应中的 `Location`、`Content-Location`、`Referer`、`Destination`、`Link`、`Refresh` 等可能携带 URL 的 Header。`ApiKeyAuthProvider` 使用 Header/Query 模式时会自动登记它自己的字段名为敏感字段。

`DebugOptions::enabled` 是日志总开关；`enabled=false` 时即使遗留 `curlVerbose=true` 也不会启用 verbose。`curlVerbose=true` 在 V4.2.5 中不再直接打开 libcurl 原始 stderr verbose，而是安装封装层过滤回调：URL/Header 仍按上述规则脱敏，原始 Body 和 TLS record 不记录。若同时主动关闭 Header/URL 脱敏，日志就可能包含凭据；生产环境仍应把日志输出视为敏感数据资产。

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

最重要字段：

```cpp
response.success;
response.error;
response.content;
response.code;
response.headers;
response.cookies;
response.curl_code;
response.errorCategory;
```

调试和性能字段：

```cpp
response.requestId;
response.headerHistory;
response.effectiveUrl;
response.contentType;
response.primaryIp;
response.negotiatedHttpVersion;
response.redirectCount;
response.attempts;
response.downloadedBytes;
response.uploadedBytes;
response.totalTime;
response.nameLookupTime;
response.connectTime;
response.tlsHandshakeTime;
response.firstByteTime;
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

当前 v4.2.5 中：

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

Query 模式会解析原 URL 的查询参数，再设置对应 API Key 参数。V4.2.5 会自动把 Query 参数名登记到 `debug.sensitiveQueryParameters`，Header 模式则自动登记到 `debug.sensitiveHeaders`，因此自定义 API Key 名称也会参与 Debug/Metrics 脱敏及重定向保护。

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
429 / 503 重试策略
Middleware
认证 Header
Circuit Breaker
Metrics
业务错误处理
```

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

当前 v4.2.5 默认 `CurlTransport` 内部拥有一个可复用 easy handle。

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

v4.2.5 源码在涉及标准库 `min/max` 时使用了防宏冲突形式，例如：

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

v4.2.5 的正式交付只包含以下三个互相配套的文件：

```text
curl_ex_v4.2.5.h
curl_ex_v4.2.5.cpp
curl_ex_v4.2.5.md
```

请始终让 `.h` 与 `.cpp` 使用完全相同的版本号。

---

## 69. 最小 target

如果你的工程已经配置好 libcurl：

```cmake
add_library(curl_ex STATIC
    curl_ex_v4.2.5.cpp
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

Pinning 失败属于 TLS 传输错误。V4.2.5 不会因为 `redirect.follow=true` 就预先拒绝带 Pin 的普通请求：首跳照常执行 Pin 校验；只有服务器真实返回重定向时，默认才会在下一跳前 fail-closed。生产环境优先关闭自动重定向并逐 Origin 校验；只有明确接受后续跳转风险时才设置 `options.redirect.allowUnpinnedRedirects = true`。

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

重试时封装可能要求上传源 `Rewind()`；不能回退的流式源无法安全重放。

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

## 85. v4.2.5 文件必须配套

本版本以后统一以文件名中的版本号判断配套关系。

请同时使用：

```text
curl_ex_v4.2.5.h
curl_ex_v4.2.5.cpp
```

不要使用：

```text
curl_ex.h
curl_ex.cpp
```

来代替，因为同名文件在聊天附件和本地工程中都容易被历史版本覆盖。

本次正式包只包含 `.h`、`.cpp` 和本说明文档，三者版本号必须保持一致。

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

v4.2.5 的设计目标就是：高频 API 尽量简单，高级能力全部存在，但只有真正需要时才进入对应层级。

---

# V4.2.5 发布与验证说明

正式源码文件为：

```text
curl_ex_v4.2.5.h
curl_ex_v4.2.5.cpp
curl_ex_v4.2.5.md
```

建议项目首次使用时至少执行：C++17/C++20 编译、基础 HTTP smoke test、目标 TLS backend 的 HTTPS 测试，以及下载/代理/认证等与你业务相关的集成测试。Windows 项目建议使用 `/W4 /permissive- /utf-8`；工程层可以定义 `NOMINMAX`，但本公共头文件自身也已经防御 `Windows.h` 的 `min/max` 宏污染。

---

## V4.2.5 本次生产审计验证记录

本节记录本次 V4.2.5 交付前实际执行过的验证，目的是区分“源码设计目标”和“已经在当前审计环境真实跑过的测试”。测试通过不等于对所有操作系统、TLS Backend、代理产品和服务端实现作绝对零缺陷承诺；生产项目仍应保留自身 CI、目标平台编译和业务集成测试。

### 审计环境

```text
Linux x86_64
GCC 14.2.0
Clang 17.0.0
libcurl 8.10.1
TLS Backend: OpenSSL 3.5.5
C++17 / C++20
```

Windows 专用提交路径（`ReplaceFileW` / `MoveFileExW`）、Schannel 以及其他 TLS Backend 在本次 Linux 容器中无法进行真实运行测试；这些部分按公开 API 契约和条件编译路径审计，最终 Windows 项目仍应使用实际 MSVC/libcurl/Schannel 组合执行 CI。

### 已执行的编译与静态边界验证

- GCC × C++17：严格 warning + `-Werror` 编译通过。
- GCC × C++20：严格 warning + `-Werror` 编译通过。
- Clang × C++17：严格 warning + `-Werror` 编译通过。
- Clang × C++20：严格 warning + `-Werror` 编译通过。
- 对最低版本 `LIBCURL_VERSION_NUM=0x073800`（7.56.0）条件分支执行了模拟编译，旧版本条件路径可编译；由于本次环境没有真实 7.56.0 开发头文件，这不替代“真实 7.56.0 SDK + 运行库”的 CI。
- 公共头文件执行了 `min/max` 宏预先存在场景的独立包含测试，包含后宏可恢复，标准库 `(Type::max)()` 用法不受污染。
- 最终源码执行 ASAN + UBSAN 代表性运行回归，未报告 sanitizer 错误。

### 已执行的真实网络/状态机回归

本地 HTTP/HTTPS 服务实际覆盖：

- GET / HEAD / POST / PUT / PATCH / DELETE / OPTIONS。
- 二进制 Body（包含 `\0`）按明确长度发送。
- JSON、Form、普通重定向、gzip 自动解压。
- Cookie Session、单请求 Cookie Engine 隔离及恢复。
- 503 自动重试和 attempt 计数。
- 普通文件下载和最终文件内容验证。
- Async 多请求 future 并发及 `PendingCount()` 交付边界。
- C++20 coroutine 正常恢复、挂起对象销毁及 ASAN/UBSAN 生命周期回归。
- Debug 总开关、URL/Header 脱敏、URL-bearing 响应 Header 脱敏。
- 自定义 Query API Key 的 Metrics 脱敏。
- 自定义敏感 Header 重定向 fail-closed，并验证未到达目标端。
- `RequestOptions::cookies` 显式 Cookie 重定向 fail-closed，并验证凭据未到达目标端。
- libcurl 8.13.0 之前的 `PATCH -> 303` 同步/异步路径 fail-closed，并验证错误 PATCH 未到达重定向目标。
- Resume 失败后本地文件回滚失败能够升级为 `CURLE_WRITE_ERROR / Download`，不会仅报告原始网络/回调错误而掩盖文件完整性恢复失败。
- 本地真实 TLS：自签 CA/SAN 的正常证书链验证、正确 Public Key Pin 成功、错误 Pin 返回 `CURLE_SSL_PINNEDPUBKEYNOTMATCH`、带 Pin 的重定向默认在下一跳前停止，以及显式 override 后的重定向行为。

### 本次审计明确修复的高风险边界

```text
原始 curl verbose 绕过脱敏
Debug enabled=false 仍可能打开 verbose
自定义 Header/API Key 跨重定向泄露
显式 CURLOPT_COOKIE Cookie 跨重定向泄露
Pinning 只保护初始传输却继续自动跳转
旧 libcurl CUSTOMREQUEST 在 301/302/303 上错误重放 Method
Query API Key 未进入 Metrics 敏感参数集合
URL codec 超过 int 长度时静默截断
外部 response sink 已产生副作用但重试判断未感知
future 已交付而 PendingCount 尚未递减的竞态窗口
multi remove 失败后的 easy/mime/slist 提前释放风险
异步完成 dispatcher 入队失败时 callback 被 move 空
Debug 格式化异常可能逃入异步网络线程
Schannel 旧版 BestEffort 静默退化为 NO_REVOKE
Windows ReplaceFileW 使用不受支持的 REPLACEFILE_WRITE_THROUGH 假设
Resume 失败后的文件回滚错误被忽略
非法 enum 配置静默退化为其他网络行为
```

生产上线前仍建议在你的实际 Windows/MSVC/vcpkg triplet 上至少跑一次：Debug/Release、x86/x64（如果都交付）、实际 TLS Backend、实际代理、证书 Pin、下载目录权限/杀毒软件占用、以及真实业务服务端的集成回归。
