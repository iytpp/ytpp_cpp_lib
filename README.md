# ytpp_cpp_lib 项目手册

`ytpp_cpp_lib` 是面向 Windows 的 C++23 静态库集合，目标是为中小型项目提供统一、易用、可复用的基础能力，减少重复封装 Windows API、网络、JSON、密码学和进程间通信的成本。

本文档是整个项目的入口手册，面向三类读者：

- **库使用者**：如何构建、安装、通过 CMake 引入并选择链接目标。
- **模块开发者**：如何理解项目结构、增加子模块、导出头文件和更新合并库。
- **维护者**：如何保持 API、依赖、文档、构建配置和二进制兼容策略一致。

各库的具体 API 和示例见对应子目录的 `README.md`。

## 1. 项目边界

- 当前仅支持 Windows。
- 使用 MSVC、CMake 和 Ninja 构建。
- 语言标准为 C++23，关闭编译器扩展。
- 产物为静态库，使用动态 MSVC 运行库：Debug 对应 `/MDd`，Release 对应 `/MD`。
- 第三方依赖通过 vcpkg manifest 管理，不在项目中复制第三方 `.lib`。
- 支持 x64/x86 与 Debug/Release 四种配置。
- 安装后导出标准 CMake Package，可通过 `find_package(ytpp_cpp_lib CONFIG REQUIRED)` 使用。

本项目不直接提供 DLL。需要 DLL 边界时，建议在业务项目中建立独立 DLL，对外暴露稳定接口并在内部链接所需的 ytpp 静态库。

## 2. 库与 CMake 目标

| 库 | CMake 目标 | 输出文件 | 主要内容 | 详细手册 |
|---|---|---|---|---|
| client-server | `ytpp::client_server` | `client-server.lib` | Windows 命名管道 RPC、签名、权限、连接池、日志 | [client-server](ytpp_librarys/client-server/README.md) |
| curl_ex | `ytpp::curl_ex` | `curl_ex.lib` | libcurl HTTP 封装、同步/异步请求、认证、重试、下载、Mock | [curl_ex](ytpp_librarys/curl_ex/README.md) |
| jsoncpp_ex | `ytpp::jsoncpp_ex` | `jsoncpp_ex.lib` | JsonCpp 序列化和解析便利接口 | [jsoncpp_ex](ytpp_librarys/jsoncpp_ex/README.md) |
| sys_core | `ytpp::sys_core` | `sys_core.lib` | Windows 系统、文件、编码、日期、环境变量、密码学等 | [sys_core](ytpp_librarys/sys_core/README.md) |
| combined | `ytpp::all` | `ytpp_cpp_lib.lib` | 将上述四个库的对象合并为一个静态库 | 本文档 |

选择原则：

- 只需要部分功能时链接具体子库，减少不必要依赖和最终链接输入。
- 需要整套能力或希望统一链接入口时使用 `ytpp::all`。
- 不要同时链接 `ytpp::all` 和其中的子库，以免重复引入相同对象。

## 3. 项目目录结构

```text
ytpp_cpp_lib/
├─ CMakeLists.txt                    # 根项目、依赖、安装和 Package 导出
├─ CMakePresets.json                 # x64/x86、Debug/Release 预设
├─ vcpkg.json                        # curl、jsoncpp、openssl 依赖清单
├─ .clang-format                     # C/C++ 格式规范
├─ cmake/
│  ├─ ytpp_compile_options.cmake     # 统一编译属性与公共使用需求
│  └─ ytpp_cpp_lib-config.cmake.in   # find_package 配置模板
├─ ytpp_librarys/
│  ├─ CMakeLists.txt                 # 注册各子项目
│  ├─ client-server/
│  │  ├─ CMakeLists.txt
│  │  ├─ README.md
│  │  └─ src/client-server/...       # 对外头文件与实现文件
│  ├─ curl_ex/
│  │  ├─ CMakeLists.txt
│  │  ├─ README.md
│  │  └─ src/curl_ex/...
│  ├─ jsoncpp_ex/
│  │  ├─ CMakeLists.txt
│  │  ├─ README.md
│  │  └─ src/jsoncpp_ex/...
│  ├─ sys_core/
│  │  ├─ CMakeLists.txt
│  │  ├─ README.md
│  │  └─ src/sys_core/...
│  └─ combined/CMakeLists.txt         # 合并静态库
├─ examples/                          # 可选示例，默认不构建
├─ tests/                             # 自动化回归测试
└─ out/
   ├─ build/<preset>/                 # 配置与构建输出
   └─ install/<preset>/               # 安装后的可消费 SDK
```

项目约定每个库的 `.h/.hpp/.cpp` 都放在该库的 `src/<公开包含目录>/` 下。安装规则只复制头文件，不复制实现文件。

## 4. 构建环境

### 4.1 必需工具

- Windows 10/11 或相应 Windows Server。
- Visual Studio 2022/2026，安装 MSVC v143 工具集和 Windows SDK。
- CMake 3.25 或更高版本。
- Ninja。
- vcpkg。

设置环境变量：

```powershell
$env:VCPKG_ROOT = "D:\path\to\vcpkg"
```

长期配置可写入用户环境变量。`VCPKG_ROOT` 必须指向包含 `scripts/buildsystems/vcpkg.cmake` 的 vcpkg 根目录。

命令行构建时，应从相应架构的 Visual Studio Developer PowerShell/Native Tools Command Prompt 运行，保证 `cl.exe` 和 Windows SDK 环境已初始化。Visual Studio 直接打开 CMake 项目时通常会自动准备工具链环境。

### 4.2 vcpkg 依赖

`vcpkg.json` 声明：

- curl
- jsoncpp
- openssl

各 preset 使用静态第三方库、动态 MSVC 运行库的 triplet：

| 架构 | triplet |
|---|---|
| x64 | `x64-windows-static-md` |
| x86 | `x86-windows-static-md` |

首次配置时 vcpkg 会按 manifest 安装依赖。`vcpkg.json` 通过 `builtin-baseline` 固定经过验证的 registry 版本；升级 baseline 时必须重新验证四种 preset、安装包和独立消费项目。

## 5. 配置、构建和安装

### 5.1 可用 preset

| 配置 preset | 构建 preset | 安装 preset |
|---|---|---|
| `x64-debug` | `x64-debug` | `x64-debug-install` |
| `x64-release` | `x64-release` | `x64-release-install` |
| `x86-debug` | `x86-debug` | `x86-debug-install` |
| `x86-release` | `x86-release` | `x86-release-install` |

### 5.2 标准流程

以 x64 Release 为例：

```powershell
cmake --preset x64-release
cmake --build --preset x64-release
cmake --build --preset x64-release-install
```

三个命令分别负责：

1. 配置生成 Ninja 构建系统，并解析 vcpkg 依赖。
2. 编译项目默认目标，但不复制安装内容。
3. 构建 `install` 目标，并把头文件、静态库、CMake Package 和使用手册写入安装目录。

只修改源码时通常重复第二条即可；修改 CMake、preset、依赖清单或切换工具链后应重新配置。

### 5.3 可选构建项

| 选项 | 默认值 | 说明 |
|---|---:|---|
| `YTPP_BUILD_TEST_APP` | `ON` | 构建 `tests/launch_for_test` |
| `YTPP_BUILD_EXAMPLES` | `OFF` | 构建示例目标 |

覆盖示例：

```powershell
cmake --preset x64-debug -DYTPP_BUILD_EXAMPLES=ON -DYTPP_BUILD_TEST_APP=ON
cmake --build --preset x64-debug
```

### 5.4 输出目录

```text
out/build/x64-release/
├─ ytpp_librarys/client-server/client-server.lib
├─ ytpp_librarys/curl_ex/curl_ex.lib
├─ ytpp_librarys/jsoncpp_ex/jsoncpp_ex.lib
├─ ytpp_librarys/sys_core/sys_core.lib
└─ ytpp_librarys/combined/ytpp_cpp_lib.lib
```

安装目录：

```text
out/install/x64-release/
├─ include/
│  ├─ client-server/
│  ├─ curl_ex/
│  ├─ jsoncpp_ex/
│  └─ sys_core/
└─ lib/
   ├─ client-server.lib
   ├─ curl_ex.lib
   ├─ jsoncpp_ex.lib
   ├─ sys_core.lib
   ├─ ytpp_cpp_lib.lib
   └─ cmake/ytpp_cpp_lib/
      ├─ ytpp_cpp_lib-config.cmake
      ├─ ytpp_cpp_lib-config-version.cmake
      └─ ytpp_cpp_lib-targets*.cmake
```

不要让业务项目直接依赖 `out/build` 内部布局；对外使用应以 `out/install/<preset>` 为 SDK 根目录。

## 6. 在其他 CMake 项目中使用

### 6.1 查找安装包

配置消费项目时把安装前缀加入 `CMAKE_PREFIX_PATH`：

```powershell
cmake -S . -B out/build `
  -DCMAKE_PREFIX_PATH="D:/path/to/ytpp_cpp_lib/out/install/x64-release"
```

消费方 `CMakeLists.txt`：

```cmake
cmake_minimum_required(VERSION 3.25)
project(example LANGUAGES CXX)

find_package(ytpp_cpp_lib CONFIG REQUIRED COMPONENTS sys_core jsoncpp_ex)

add_executable(example main.cpp)
target_link_libraries(example PRIVATE ytpp::sys_core ytpp::jsoncpp_ex)
```

未指定 `COMPONENTS` 时会加载完整依赖集合。按需指定 `client_server`、`curl_ex`、`jsoncpp_ex`、`sys_core` 或 `all`，可以避免仅使用轻量子库时仍要求安装无关第三方依赖。

使用整套库：

```cmake
target_link_libraries(example PRIVATE ytpp::all)
```

导出目标会传递：

- C++23 编译要求。
- `UNICODE`、`_UNICODE`。
- MSVC 的 UTF-8、严格一致性和 `__cplusplus` 选项。
- 所需第三方包与 Windows 系统库链接依赖。

消费项目需要能够找到所选组件的传递依赖：`curl_ex` 需要 CURL，`jsoncpp_ex` 需要 JsonCpp，`sys_core` 需要 OpenSSL，`all` 需要全部依赖；`client_server` 只依赖 Windows 系统库。推荐使用与本库一致的 vcpkg toolchain、架构和运行库配置。

### 6.2 头文件示例

```cpp
#include <curl_ex/curl_ex.h>
#include <jsoncpp_ex/jsoncpp_ex.h>
#include <sys_core/sys_core.h>
#include <client-server/RPC-named-pip/NamedPipeRpcClient.h>
```

### 6.3 配置必须匹配

- x64 应链接 x64 安装目录，x86 应链接 x86 安装目录。
- Debug/Release 最好使用对应安装配置，尤其是 MSVC 运行库和第三方依赖可能不同。
- 不要混用 `/MT` 与当前项目的 `/MD` ABI。
- 静态库不执行最终链接；缺少的系统库或第三方库错误通常在消费方生成 EXE/DLL 时暴露。

## 7. 构建设计说明

每个功能库由两个目标组成：

```cmake
add_library(ytpp_example_objects OBJECT ${example_sources})
add_library(ytpp_example STATIC $<TARGET_OBJECTS:ytpp_example_objects>)
```

OBJECT 目标只编译源码并产生对象文件，不生成独立 `.lib`；STATIC 目标把这些对象归档成子库。`ytpp::all` 再复用各 OBJECT 目标，把所有对象归档到 `ytpp_cpp_lib.lib`。

这样同一份源码既能形成独立子库，又能进入合并库，不需要维护第二份源文件列表或把若干 `.lib` 再做二次归档。

公共目标通过 `ytpp_configure_library_target()` 统一设置：

- 安装导出名称。
- 输出文件名。
- C++23 使用要求。
- 公共和安装期头文件路径。
- 公共 MSVC 编译定义与选项。

OBJECT 目标通过 `ytpp_configure_object_target()` 统一设置内部编译选项。新增模块时必须复用这些函数，避免不同子库的 ABI 和警告策略漂移。

## 8. 新增独立子库

假设新增 `image_ex`。

### 8.1 创建目录

```text
ytpp_librarys/image_ex/
├─ CMakeLists.txt
├─ README.md
└─ src/image_ex/
   ├─ image_ex.h
   └─ image_ex.cpp
```

公共头文件的包含路径应为：

```cpp
#include <image_ex/image_ex.h>
```

命名空间建议与目录一致：

```cpp
namespace ytpp::image_ex {
// Public API
}
```

### 8.2 编写子项目 CMake

```cmake
set(image_ex_source_root "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(image_ex_sources
    src/image_ex/image_ex.cpp
)

add_library(ytpp_image_ex_objects OBJECT ${image_ex_sources})
ytpp_configure_object_target(ytpp_image_ex_objects)
target_include_directories(ytpp_image_ex_objects PRIVATE "${image_ex_source_root}")

add_library(ytpp_image_ex STATIC $<TARGET_OBJECTS:ytpp_image_ex_objects>)
add_library(ytpp::image_ex ALIAS ytpp_image_ex)
ytpp_configure_library_target(
    ytpp_image_ex
    image_ex
    image_ex
    "${image_ex_source_root}"
)

install(
    DIRECTORY "${image_ex_source_root}/image_ex"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)
```

如果依赖第三方库或 Windows 系统库，同时配置 OBJECT 和 STATIC 目标：

```cmake
target_link_libraries(ytpp_image_ex_objects PRIVATE ThirdParty::Core)
target_link_libraries(ytpp_image_ex PUBLIC ThirdParty::Core windows_system_lib)
```

OBJECT 目标的 `PRIVATE` 依赖用于编译实现；STATIC 目标的 `PUBLIC` 依赖会传递给消费方完成最终链接。仅实现期使用且不出现在公共头文件/最终未解析符号中的依赖才可设为 `PRIVATE`。

### 8.3 注册子项目和安装目标

在 `ytpp_librarys/CMakeLists.txt` 中添加：

```cmake
add_subdirectory(image_ex)
```

在根 `CMakeLists.txt` 的 `install(TARGETS ...)` 中加入 `ytpp_image_ex`，否则库文件和导出目标不会进入安装包。

### 8.4 加入合并库

在 `ytpp_librarys/combined/CMakeLists.txt` 中：

```cmake
add_library(ytpp_all STATIC
    # 现有目标……
    $<TARGET_OBJECTS:ytpp_image_ex_objects>
)
```

同时把新模块的构建期 include 路径和公共链接依赖加入 `ytpp_all`。如果遗漏，独立库可能正常，但 `ytpp::all` 会缺少实现或消费依赖。

### 8.5 更新 Package 依赖

新增 vcpkg 依赖时需要同步修改：

1. `vcpkg.json`：声明包。
2. 根 `CMakeLists.txt`：使用 `find_package()` 创建依赖目标。
3. 子库 `CMakeLists.txt`：链接依赖目标。
4. `cmake/ytpp_cpp_lib-config.cmake.in`：增加 `find_dependency()`，保证安装后的消费方可恢复依赖目标。
5. `ytpp::all`：传递合并库所需依赖。

不要在 CMake 中写死 vcpkg 安装目录或直接链接某个机器上的 `.lib` 绝对路径。

### 8.6 文档与验证

新子库必须同时提供：

- 子库 `README.md`：定位、引入方式、快速开始、完整 API 分类、错误处理、线程安全、所有权和限制。
- 头文件 API 注释。
- 至少一个能编译并覆盖核心路径的测试或示例。
- 根 README 的模块表、目录树和目标列表更新。

## 9. 在现有库中新增子模块

以 `sys_core` 新增 `registry` 为例：

1. 创建 `src/sys_core/registry.h` 和 `registry.cpp`。
2. 使用 `namespace ytpp::sys_core::registry {}`。
3. 在 `sys_core/CMakeLists.txt` 的 `sys_core_sources` 中加入 `registry.cpp`。
4. 在 `sys_core/sys_core.h` 包含新头文件；如需通过总入口直接暴露名称，在 `ytpp::sys_core` 中添加对应 using 声明/命名空间导入。
5. 在 `sys_core/README.md` 增加模块定位、接口和示例。
6. 添加测试，验证独立头文件包含和总入口包含两种方式。

子模块通常不应创建新的静态库；只有当它具备独立依赖边界、独立发布价值或显著链接体积时，才升级为独立子库。

## 10. API 与源码维护规范

### 10.1 命名

- 函数：首字母大写的 PascalCase，例如 `GetSystemInformation()`。
- 类型、类、结构体、枚举：PascalCase。
- 局部变量和参数：首字母小写的 camelCase。
- 成员变量：camelCase 后加 `_`，例如 `connectTimeoutMs_`。
- 常量：`k` 开头的 camelCase，例如 `kBeijingTimeZone`。
- 宏：全大写下划线，例如 `YTPP_LOG_ERROR`。
- 命名空间：沿用现有目录命名，例如 `ytpp::sys_core::date_time`。

重命名公共接口属于破坏性变更。必须同时更新声明、定义、内部调用、测试、示例、所有 README 和版本说明；必要时提供一段迁移周期的兼容别名。

### 10.2 头文件

- 每个公共函数必须说明用途、使用方式、每个参数、返回值、异常和重要前置条件。
- 参数声明使用 `_In_`、`_Out_`、`_Inout_` 或更精确的 SAL。
- 所有标准库类型带 `std::`，禁止 `using namespace std;`。
- 尽量减少 Windows 和第三方头文件在公共接口中的暴露。
- 头文件必须能够被独立包含，不依赖其他文件碰巧先包含某个类型。
- 公共结构体字段变更可能影响源码兼容和 ABI，应谨慎处理。

### 10.3 实现文件

- CPP 不重复函数说明，只保留解释关键算法、边界、安全原因和平台限制的注释。
- Windows 全局 API 使用 `::FunctionName()`，明确与项目符号的区别。
- 使用 `.clang-format` 统一 K&R 大括号、缩进和空白。
- 不在实现文件引入全局 `using namespace`。
- 资源所有权优先使用 RAII；必须使用原生句柄时，明确创建、转移和释放路径。

### 10.4 公共接口设计

- 优先使用明确类型表达状态，避免用 `0`、空字符串或空指针同时表示多个语义。
- 输出参数应清楚标注失败时是否会被修改。
- 错误模型在同一模块内保持一致：布尔值、错误码、`std::optional` 或异常不要无理由混用。
- 跨线程对象必须说明线程安全边界和回调执行线程。
- 接收指针、句柄、回调和外部对象时必须说明所有权及生命周期。
- 安全相关接口不得默认降级证书验证、签名验证或加密强度。

## 11. CMake 维护规范

### 11.1 目标优先

所有配置通过目标表达：

- `target_sources`
- `target_include_directories`
- `target_compile_definitions`
- `target_compile_options`
- `target_link_libraries`

不要使用全局 `include_directories()`、`link_directories()` 或硬编码库文件路径。

### 11.2 PUBLIC、PRIVATE、INTERFACE

- 公共头文件需要的依赖或最终链接必须传递的库：`PUBLIC`。
- 仅实现文件使用：`PRIVATE`。
- 当前目标自身不使用、只要求消费方使用：`INTERFACE`。

静态库对依赖可见性的判断不能只看“头文件是否包含”；实现中产生的未解析外部符号也需要在最终链接阶段传递，因此通常应由导出 STATIC 目标公开链接。

### 11.3 安装与导出检查

修改 CMake 后至少验证：

1. 四个配置 preset 能完成配置和构建。
2. 四个 install preset 能安装。
3. `include/` 中没有 `.cpp`、临时文件或私有头文件。
4. 五个 `.lib` 均存在。
5. 安装目录中的 Package 能被一个独立消费项目 `find_package()`。
6. 独立子库目标和 `ytpp::all` 都能完成最终链接。

## 12. 依赖维护

- 依赖只在 `vcpkg.json` 声明，不把第三方二进制复制进源码树。
- 升级依赖前阅读变更记录，重点检查 ABI、默认安全策略、弃用 API 和最低平台要求。
- curl/OpenSSL 升级需要复核 TLS 后端、证书来源、协议版本和静态链接依赖。
- JsonCpp 升级需要复核解析/写入选项名称与默认语义。
- Debug/Release、x86/x64 必须分别验证，不能用单一配置结果推断其他配置。
- 发布版本应记录 CMake、MSVC、Windows SDK、vcpkg baseline 和依赖版本。

## 13. 测试与发布检查清单

配置并构建后运行：

```powershell
ctest --test-dir out/build/x64-release --output-on-failure
```

`ytpp_core_tests` 当前覆盖 RPC 签名完整性、协议边界、空文件覆盖写入，以及认证失败时不得发布明文等高风险回归场景。新增缺陷修复时应同步补充最小回归用例。

`tests/package_consumer` 是安装包消费验证项目，只使用安装后的头文件、库和 CMake Package，用于发现构建树正常但安装导出损坏的问题。

### 代码提交前

- [ ] 新接口符合命名、SAL 和注释规范。
- [ ] 没有 `using namespace std;`。
- [ ] Windows API 使用全局 `::` 限定。
- [ ] 头文件可独立包含。
- [ ] 新增源码已进入对应 OBJECT 目标。
- [ ] 新模块已进入安装导出和必要时的合并库。
- [ ] README 示例使用当前接口名并能编译。

### 发布前

- [ ] x64 Debug/Release 构建与安装通过。
- [ ] x86 Debug/Release 构建与安装通过。
- [ ] 独立消费项目的 `find_package()` 验证通过。
- [ ] `ytpp::client_server`、`curl_ex`、`jsoncpp_ex`、`sys_core`、`all` 均可链接。
- [ ] 测试覆盖成功、失败、边界和异常路径。
- [ ] 安全相关默认值经过复核。
- [ ] 版本号、依赖清单和文档保持一致。
- [ ] 安装目录不包含旧版本残留文件。

## 14. 常见问题

### CMake 找不到 `ytpp_cpp_lib`

确认已经执行 install preset，并把正确安装前缀加入 `CMAKE_PREFIX_PATH`。不要把 `lib/cmake/ytpp_cpp_lib` 当作 include 目录。

### CMake 找到包但找不到 CURL/OpenSSL/JsonCpp

消费项目也需要使用可解析这些依赖的 toolchain。推荐使用 vcpkg，并选择与本库一致的 triplet。

### 出现机器类型冲突

检查是否把 x86 库链接进 x64 项目，或反之。确认 `CMAKE_PREFIX_PATH` 指向正确 preset 的安装目录。

### 出现运行库冲突

本项目使用 `/MDd` 或 `/MD`。消费项目和所有静态依赖应使用兼容运行库，不要混入 `/MT` 产物。

### 修改代码后 install 目录没有更新

普通 build preset 只构建；需要再运行对应的 `*-install` preset 才会复制最新头文件和库。

### 为什么同时存在子库和 `ytpp_cpp_lib.lib`

子库支持按需依赖；合并库提供单目标使用方式。两者来自同一批 OBJECT 文件，功能实现保持一致。

## 15. 维护责任

当前项目由樱桃屁屁负责主要开发与维护。后续贡献应遵守本文的目录、接口、CMake、文档和验证约定，避免只让源码“能够编译”而破坏安装包、消费方或其他架构配置。
