# 构建指南

## 开发环境

- Qt 6.12.0，C++20，CMake 3.28+，Ninja。
- Rust 1.99.0，工具链由 `rust-toolchain.toml` 固定，依赖由 `Cargo.lock` 固定。
- Python 3.12+；格式工具另需 Node.js / npm。
- Windows：MSVC 2022 开发环境；macOS：Xcode Command Line Tools。
- Linux：编译器、OpenGL/EGL、X11/XCB、Wayland、字体、音频和桌面集成开发库，完整软件包列表见 [CI 配置](../.github/workflows/build.yml)。

目标系统基线为 Windows 10 1809+ x64 / Windows 11 x64、ARM64，macOS 14.4+ Intel、Apple Silicon，以及 Ubuntu 22.04+ 或等价 glibc 2.35+ 的 Linux x64、ARM64（X11 / Wayland）。这些是构建与适配目标，不代表全部组合已完成实机验证。

## 安装 Qt SDK

在仓库根目录执行：

```sh
python3.12 scripts/install-qt.py
python3.12 scripts/install-qt.py --check
```

Windows 使用对应版本的 `python`。Windows/macOS 安装官方预编译 SDK；Linux 为匹配 glibc 2.35 基线独立构建源码 SDK。安装工具及其依赖固定在 `tools/qt/requirements.txt`，仅用于开发。

默认 SDK 位于 `build/qt-official/6.12.0/<kit>` 或 `build/qt-sdk/6.12.0`。也可通过 Qt 官方安装器安装相同版本和所需模块，随后传入 `--qt-prefix /path/to/Qt/6.12.0/kit`。脚本会检查 SDK 版本及 Core、Gui、Widgets、Qml、Quick、QuickControls2、QuickDialogs2、Multimedia、Svg、Sql、ShaderTools 等所需模块。

需要源码 SDK 时显式运行 `python3.12 scripts/install-qt.py --source`。日常构建和启动不会下载或重新构建 Qt。

## 构建和启动

```sh
python3.12 scripts/build.py
python3.12 scripts/start.py
python3.12 scripts/start.py --no-build
python3.12 scripts/start.py --no-build --log-level debug
```

默认构建 Release；切换 SDK 时脚本会刷新 CMake 中的 Qt 路径。日志默认写入 `build/logs/echomusicnext-<PID>.log`，可通过 `--log-dir` 指定目录。直接打开应用时使用标准应用数据目录下的 `logs`。

仅在需要 Debug 调试或断言时显式执行 `python3.12 scripts/build.py --preset debug`。平时只保留 Release；Debug 任务结束后保留必要日志与测试结果，确认没有进程使用，再清理 `build/debug/`。`--log-level debug` 只调整日志级别，不要求 Debug 构建。

Windows、macOS 和 Linux 均通过 Crashpad 在独立进程采集原生崩溃，现场写入同一日志目录的 `crashes/database`。报告仅在本机保存，上传关闭且未配置接收地址；未接入 Sentry 服务。`qFatal` 的脱敏原因另行同步写入会话 `.jsonl`，避免异步日志队列在退出前来不及排空。再次启动会将新 dump 的异常码、线程、故障地址、模块及偏移、构建标识写入运行日志，并生成 `.summary.json`。崩溃定位请保留整个 `logs` 目录；dump 包含线程栈和进程内存片段，分享前应检查其中的私人数据。

Release 同样生成调试资料；Windows 文件版本和日志包含真实产品版本，另有构建时间、源 revision 和工作树状态标识。六平台发布构建在安装包之外保存匹配的符号工件：Windows 验证 PE/PDB GUID 与 age，macOS 验证二进制/dSYM UUID，Linux 保留含 DWARF 的原生二进制。CI 的 `symbols-*` 工件保存 90 天，维护者应下载归档需要长期维护的版本；这些符号不进入自动更新清单或发行附件。必须使用对应构建的符号，重新编译的同版本文件不能替代。

配置阶段通过 SHA-256 固定的归档获取 Crashpad 及子依赖，需要联网或预先填充 CMake FetchContent 缓存。`crashpad_handler` 必须随三端应用部署，Windows 还需 `crashpad_wer.dll`；macOS 位于应用包的 `Contents/Helpers`，其余平台与主程序同目录。正常运行不初始化第二个 Qt 界面。系统强杀、断电、内核/OOM 终止或损坏严重到无法采集的情形不能保证生成 dump；下次启动仍通过会话记录标记未正常退出，不将其误判为已定位的崩溃。

也可直接使用 CMake：

```sh
cmake --preset release -DCMAKE_PREFIX_PATH=/path/to/Qt/6.12.0/kit
cmake --build --preset release
ctest --preset release
```

应用目标为 `echomusicnext`。Release 产物分别为：

- macOS：`build/release/app/echomusicnext.app`。
- Windows：`build/release/app/echomusicnext.exe`。
- Linux：`build/release/app/echomusicnext`。

并发构建使用各自的 CMake 构建目录。默认 Cargo 目录相互独立；需要复用时显式配置 `ECHO_CARGO_DIR`，并在链接完成后运行依赖该产物的应用或测试。

### 开发产物管理

日常只构建和保留 `build/release`，复用已经安装的 SDK；不自动补建或长期保留 Debug。Debug 按具体任务需要显式创建，结束后清理其完整编译树。Release 的崩溃诊断符号仍按原要求保留。需要隔离构建时，源码副本排除所有层级的构建目录、`target`、`node_modules`、`.git`、SDK 和运行数据；隔离验证可设置 `CARGO_INCREMENTAL=0`。完成后保留复现命令、配置、日志、测试结果及必要截图，清理该任务的编译树和临时部署副本。

根 `target/`、隔离目录中的 Cargo/CMake 产物、弃用 SDK 的源码构建和下载缓存均可重建。清理前确认没有构建、链接或运行进程使用它们，并核对活动 CMake 配置引用的 Qt 与 Rust 路径。不要直接删除整个 `build/`：其中也可能包含 SDK、工具环境、崩溃现场、发行符号和唯一源码快照。匹配发行版本的符号及崩溃现场按诊断需要另行保留，不能用重新构建的二进制替代。

大构建前检查磁盘可用空间，任务结束检查目录占用。一次性验证结束即清理可重建产物；重复创建完整构建树会同时放大 Rust 依赖、增量缓存和 Qt/C++ 中间文件。

## 平台依赖

HTTP 代理认证使用 libcurl：macOS 链接系统库，Linux 需要 `libcurl4-openssl-dev`（最低 7.81，需 NTLM 支持），Windows 由 `cmake/Curl.cmake` 下载固定哈希的 curl 8.23.0 并构建静态库，使用 Schannel / SSPI。

Linux 还需要 `libproxy-dev` 和 `libglib2.0-dev`。系统代理与 PAC 功能依赖桌面环境提供的 libproxy 配置和 JavaScript 插件，CI 的安装列表包含所需软件包。源码 Qt Multimedia SDK 需要 FFmpeg 及相应音频开发库。

验证码使用系统默认浏览器，应用不部署 Qt WebEngine。正式部署仍需包含实际使用的 Qt 插件、Multimedia 后端和第三方库，并附带相应许可材料。

## 检查

```sh
python3.12 scripts/format.py --install
python3.12 scripts/format.py --check
python3.12 -m unittest discover -s tests/tooling -v
cargo clippy --locked --workspace --all-targets -- -D warnings
cargo test --locked --workspace
cargo run --locked -p ffi-header -- --check
ctest --preset release
cmake --build --preset lint
```

格式规则见[代码格式与检查](formatting.md)。ABI 修改后使用 `cargo run --locked -p ffi-header -- --write` 生成头文件，再检查差异并重新构建。普通构建只校验生成头，不修改源码。

默认自动检查离线运行；显式在线测试依赖网络和服务状态。自动检查通过不替代原生窗口、音频设备、最低系统及安装包验证。

`lint` 预设使用 Debug 配置；仅需 QML lint 时先运行 `python3.12 scripts/build.py --preset debug --configure-only`，再执行对应 lint 命令，无需编译完整 Debug 应用。检查结束后同样清理 `build/debug/`。

## 分发构建

```sh
python3.12 scripts/build.py --preset release --distribution
```

此选项启用 `ECHO_DISTRIBUTION_BUILD`，包括诊断重启与应用内安装更新。Release 优化级别本身不启用分发模式。

完成构建后，准备部署材料并打包：

```sh
node tools/release/prepare.mjs
node tools/release/package.mjs --qt=/path/to/Qt/6.12.0/kit
```

输出位于 `build/packages/`。使用其它构建目录时可传 `--build=build/your-build`；输出目录可用 `--out=build/your-packages` 指定。目录均需位于项目 `build/` 下。Node.js 仅用于开发、打包与检查，不随应用运行。

- macOS：通过 `macdeployqt` 部署 Qt，生成临时签名的 DMG 和 ZIP；当前没有 Developer ID 签名和公证。
- Windows：需要 NSIS 3.11，默认读取 `Program Files (x86)/NSIS/makensis.exe`，也可传 `--nsis=...`；通过 `windeployqt` 部署后生成当前用户安装程序，包含协议与 `.echomod` 关联；缺少运行库时安装已验证 Microsoft 签名的官方 VC14 Redistributable，可能请求系统授权。
- Linux：在 Ubuntu 22.04 构建。需要 Ruby/FPM 1.17.0、rpm、zstd、libarchive-tools、libfuse2；准备步骤下载并校验锁定的 linuxdeploy / Qt 插件。传 `--linuxdeploy=build/release-tools/linuxdeploy-x86_64.AppImage`（ARM64 使用 `aarch64`）生成 AppImage、DEB、RPM、Arch 包和 tar.gz。tar.gz 解压后运行 `echomusicnext/AppRun`。

安装包保留项目许可、第三方许可、Qt SPDX 或源码通知，以及锁定的 Rust 依赖清单。应用运行时不部署 Node.js、Python 或 Qt WebEngine。

### CI 与版本发布

[桌面工作流](../.github/workflows/build.yml) 只接受 `v*` 标签推送和手动运行。手动运行只上传保留 7 天的构建产物；标签发布必须完成六个平台的构建、测试和打包，再统一校验文件名、版本、平台、大小及 SHA-512，上传到草稿 Release，完整后才公开。任一平台失败均不发布不完整版本。分享页面使用独立的手动工作流。

根 CMake 与 Cargo 的版本须保持一致；例如声明 `0.1.0` 时允许标签 `v0.1.0` 或 `v0.1.0-beta.1`，拒绝不匹配的标签。完整版本传入应用和更新清单，预发布标签同时设置 GitHub prerelease。每次发布须在 `CHANGELOG.md` 中有对应版本记录，发布前移除该版本的“未发布”标记。

Rust 检查只覆盖项目自有 workspace 包，不整理 vendor：格式检查及 correctness / suspicious 类 Clippy 检查阻断发布，其余诊断仍保留在日志。离线 API 回归不包含本地接口样本。

### 应用内更新

应用从本仓库 GitHub Release 检查版本，使用完整 SemVer 比较并遵守预发布开关。每个目标的 `update-{platform}-{arch}.json` 与安装包一起发布；下载按设置使用 GitHub 加速，网络失败回源。下载写入临时文件，大小与 SHA-512 校验通过后才能安装。关闭更新窗口仍可从设置或任务中心恢复进度；取消和失败不会安装残缺文件。

Windows 使用 NSIS 更新，静默选项控制安装向导；AppImage 在应用退出后替换并重新启动，无法创建新进程时恢复原文件；DEB/RPM/Arch 包经系统授权交给相应包管理器。独立安装助手等待原进程正常退出，不强制终止播放与存储服务。系统拒绝授权或安装失败时保留原安装并提示重新检查更新。macOS 的临时签名发行版与 tar.gz 使用手动下载替换。

本机检查不代表 Windows、Linux、最低系统或全部架构已验收；发行验证应使用对应平台的真实安装、更新与卸载流程。
