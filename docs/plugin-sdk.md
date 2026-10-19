# 插件 SDK

插件作者 SDK、构建工具、文档与官方插件由 [EchoMusicNextPlugins](https://github.com/hoowhoami/EchoMusicNextPlugins) 仓库维护。开发包为 `@echomusicnext/plugin-sdk`，类型声明维护在该仓库的 `packages/sdk/index.d.ts`；参见[SDK 文档](https://github.com/hoowhoami/EchoMusicNextPlugins/blob/main/docs/sdk.md)及[开发指南](https://github.com/hoowhoami/EchoMusicNextPlugins/blob/main/docs/plugin-development.md)。

宿主提供 `EchoMusic.PluginSdk 1.0` QML 模块、`echo:plugin-sdk` 适配器及运行时生命周期管理。应用独立构建，不要求同级插件仓库存在。SDK 协议版本为 1.0.0，与应用版本和 QML 导入版本分别管理；安装包见[插件包与依赖](plugin-packages.md)。

## 契约导出

可执行文件的 `--export-plugin-sdk` 从 Qt 静态元对象导出公开类型、属性、信号、方法、枚举及宿主/SDK 版本、feature、capability 和 context 清单。导出在业务初始化之前退出，不加载插件或读取账号。

在插件仓库运行 `npm run sdk:check -- --host /absolute/executable` 检查作者绑定；`sdk:sync` 验证后更新宿主快照。宿主普通构建不会修改插件仓库。

## 信任与生命周期

插件是可信的进程内 QML / JS 扩展，不是安全沙盒。插件应仅通过声明的宿主接口访问业务能力，不持有另一份可写播放器或账号状态。

注册、订阅、任务和界面实例由插件 scope 管理；停用、重载、更新和退出时撤销并排空。异步结果受取消及账号世代约束，旧会话结果不能继续更新当前状态。原生 DSP 另受平台、架构和 C ABI 兼容性检查。

## 界面与频谱

现有接口支持页面、设置、标题栏及播放栏贡献，以及播放命令、私有存储、网络、任务、依赖和插件间服务。任意宿主 UI 替换、完整主题与歌词皮肤注册、部分高级文件和网络扩展尚未全部提供，具体可用性以宿主导出契约为准。

`ctx.ui.setState` 发布最多 1 MiB 的 JSON 对象，供同一插件的设置和绘制组件共享只读界面状态。`registerSurface(id, target, packageRelativeQml)` 支持 `playerbar.background`、`mini.background` 和 `lyrics.aboveControls`。宿主隐藏、最小化或插件停用时销毁挂载实例，恢复显示时按需创建。

声明 `audio.spectrum` feature 和 `audioSpectrum` capability 后，可订阅播放器实际输出的 PCM 频谱。订阅提供帧、状态及暂停/释放操作；`dispose` 后句柄失效。多个订阅共享分析，暂停或没有可见订阅时停止采集与 FFT 请求。

支持 fps 1–60、binCount 8–512、fftSize 256–8192 的指定二次幂、smoothing 0–0.95、log / linear / mel 频率映射及可选波形。帧包含 source、state、时间、采样率、FFT 大小、频段、peak / rms 等资料。当前通过有界 JSON 传输，不提供零复制二进制 buffer。HDR 在 `graphics.hdr` 不可用时保持禁用。

## 依赖与协作

通过 `ctx.plugins` 查询依赖、注册服务，使用 `pluginService<Contract>()` 调用服务。对应的能力声明为 `plugins.dependencies` 和 `plugins.services`。
