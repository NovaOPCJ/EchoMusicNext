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
python3.12 scripts/build.py --preset debug
```

默认构建 Release；切换 SDK 时脚本会刷新 CMake 中的 Qt 路径。日志默认写入 `build/logs/echomusicnext-<PID>.log`，可通过 `--log-dir` 指定目录。直接打开应用时使用标准应用数据目录下的 `logs`。

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

## 分发构建

```sh
python3.12 scripts/build.py --preset release --distribution
```

此选项启用 `ECHO_DISTRIBUTION_BUILD`，包括设置中的诊断重启流程。Release 优化级别本身不启用分发模式。打包还需完成 Qt 及第三方依赖部署、文件关联、签名、公证和平台安装验证；当前本地构建不自动完成这些步骤。

[三端 CI](../.github/workflows/build.yml) 定义 x64 / ARM64 构建任务。正式支持状态以对应平台的实际构建和设备验证为准。
