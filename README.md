<p align="center"><a href="#中文">简体中文</a> | <a href="#english">English</a></p>

<a id="中文"></a>

# 吉里吉里Z（Krkrz）个人魔改版

基于吉里吉里Z 的定制分支，保留官方 TJS 与插件接口，额外提供 Steam 集成、GPU 分组合成，以及若干内置扩展 API。

## 环境要求

| 组件 | 要求 |
| --- | --- |
| Visual Studio | 2022 或更高，需安装「使用 C++ 的桌面开发」工作负载 |
| CMake | 3.21 或更高（需要 `--preset` 与 preset v3） |
| Ninja | 需在 `PATH` 中 |
| NASM | 2.10.09 或更高，`nasm.exe` 需在 `PATH` 中 |
| VC-LTL | 5.x，供 `VC-LTL helper for cmake.cmake` 自动查找（仓库根目录、上级目录或注册表） |
| YY-Thunks | 已随仓库提供 `YY_Thunks_for_Win2K.obj`，无需额外安装 |

Steam 版还需要 `extension/steamapi/sdk`（随仓库提供，内含 `steam_api.lib` 与 `steam_api.dll`），由 `extension/steamapi/steamapi.cmake` 自动链接。

## 获取源码

```bat
git clone https://github.com/cokkeijigen/krkrz mykrkrz
cd mykrkrz
git submodule update --init --recursive
```

`external/` 下的 baseclasses、freetype、libjpeg-turbo、libpng 等均为 submodule，未拉取会编译失败。

## 编译

使用仓库自带的 `build.bat` 构建。它会自动定位 Visual Studio 并初始化 x86 环境，无需预先打开「开发者命令提示符」：

```bat
build.bat
```

默认 preset 为 `windows-msvc-x86-release`，也可显式指定其他 preset：

```bat
build.bat windows-msvc-x86-debug
build.bat windows-msvc-x64-release
```

`build.bat` 内部就是 `cmake --preset <preset>` + `cmake --build --preset <preset>`，需要时也可以手动分两步执行（先在 x86 Native Tools Command Prompt 中，或普通 cmd 先 `call "...\vcvarsall.bat" x86`）：

```bat
cmake --preset windows-msvc-x86-release
cmake --build --preset windows-msvc-x86-release
```

只编译 Steam 版：

```bat
cmake --build .build\windows-msvc-x86-release --target KRKRZ_STEAM
```

### 产物

| 目标 | 输出文件 |
| --- | --- |
| `KRKRZ` | `.build\<preset>\tvpwin32\tvpwin32.exe` |
| `KRKRZ_CONSOLE` | `.build\<preset>\tvpwin32\tvpwin32_Console.exe` |
| `KRKRZ_STEAM` | `.build\<preset>\tvpwin32_Steam\tvpwin32_Steam.exe`（并复制 `steam_api.dll`） |
| `KRKRZ_STEAM_CONSOLE` | `.build\<preset>\tvpwin32_Steam\tvpwin32_Steam_Console.exe` |

### 可用 preset

```
windows-msvc-x86-{release,debug,relwithdebinfo}
windows-msvc-x64-{release,debug,relwithdebinfo}
windows-clang-x86-{release,debug,relwithdebinfo}
windows-clang-x64-{release,debug,relwithdebinfo}
```

`debug` / `relwithdebinfo` 会生成 PDB。默认目标平台为 x86。

## 扩展 API

以下 API 全部位于 `extension/`，由引擎启动时直接注册，不需要额外插件 DLL。
`SteamHelper` 仅在 `__HAS_STEAM_API__`（即 `KRKRZ_STEAM` / `KRKRZ_STEAM_CONSOLE` 目标）下存在。

### 目录

#### `Storages.selectFolder(params: object) -> bool`

> | 字段 | 类型 | 说明 |
> | --- | --- | --- |
> | `params.name` | `string` | 输入为初始目录（storage 名或本地路径）；选中后回写为结果的 storage 名 |
> | `params.title` | `string` | 对话框标题，可省略 |
> | `params.window` | `object` | 所属窗口，用于取 owner HWND，可省略 |
> | `params.rootDir` | `string` | 浏览根目录；指定后限制在该目录内，并强制使用旧式对话框，可省略 |
>
> 返回 `true` 表示选中，`false` 表示取消；取消时不会重复弹出对话框。
> Vista 及以上使用 `IFileOpenDialog`，XP 等环境自动回退 `SHBrowseForFolder`。

#### `Storages.createFolders(path: string) -> bool`

> | 参数 | 类型 | 说明 |
> | --- | --- | --- |
> | `path` | `string` | 目标目录，storage 名或本地路径均可，缺失的中间层级会逐级创建 |
>
> 等价于 `mkdir -p`。目录已存在返回 `true`；路径被同名文件占用或无写入权限返回 `false`。

### 语言与字体

#### `System.getLanguage() -> string`

> 无参数。返回界面语言标识：`schinese`、`tchinese`、`english`、`japanese`；
> 其他语言返回小写的 locale 名，取不到时为 `unknown`。

#### `System.registerPrivateFont(file: string, alias: string) -> int`

> | 参数 | 类型 | 说明 |
> | --- | --- | --- |
> | `file` | `string` | 字体文件路径，storage 名或本地路径均可 |
> | `alias` | `string` | 注册别名，可省略 |
>
> 返回字体 `fontid`，失败返回 `0`。

#### `System.unregisterPrivateFont(alias: string) -> bool` / `System.unregisterPrivateFont(fontid: int) -> bool`

> | 参数 | 类型 | 说明 |
> | --- | --- | --- |
> | `alias` | `string` | 注册时指定的别名，与 `fontid` 二选一 |
> | `fontid` | `int` | `registerPrivateFont` 的返回值，与 `alias` 二选一 |
>
> 注销成功返回 `true`。

#### `System.getPrivateFontName(alias: string) -> string` / `System.getPrivateFontName(fontid: int) -> string`

> | 参数 | 类型 | 说明 |
> | --- | --- | --- |
> | `alias` | `string` | 注册时指定的别名，与 `fontid` 二选一 |
> | `fontid` | `int` | `registerPrivateFont` 的返回值，与 `alias` 二选一 |
>
> 返回字体显示名，取不到时返回空串。

### Steam

`SteamHelper` 是全局类（`global.SteamHelper`），不可实例化，全部为静态方法。

#### `SteamHelper.init() -> bool`

> 无参数。SteamAPI 初始化成功返回 `true`。

#### `SteamHelper.setAchievement(id: string) -> bool`

> | 参数 | 类型 | 说明 |
> | --- | --- | --- |
> | `id` | `string` | Steam 成就 ID |
>
> 写入成功返回 `true`。

#### `SteamHelper.clearAchievement(id: string) -> bool`

> | 参数 | 类型 | 说明 |
> | --- | --- | --- |
> | `id` | `string` | Steam 成就 ID |
>
> 清除成功返回 `true`。

#### `SteamHelper.clearAchievements() -> int`

> 无参数。返回本次成功清除的成就数量。

#### `SteamHelper.currentAppId() -> int`

> 无参数。返回当前 AppID。

#### `SteamHelper.restartAppIfNecessary(appid: int) -> bool`

> | 参数 | 类型 | 说明 |
> | --- | --- | --- |
> | `appid` | `int` | 期望的 AppID，必须大于 `0` |
>
> 需要由 Steam 重启拉起时返回 `true`。

#### `SteamHelper.queryAchievementState(id: string, always: bool) -> int`

> | 参数 | 类型 | 说明 |
> | --- | --- | --- |
> | `id` | `string` | Steam 成就 ID |
> | `always` | `bool` | 为 `true` 时强制向 Steam 重新查询，忽略本地缓存，可省略 |
>
> 返回 `1` 已解锁、`0` 未解锁、`-1` 不可用（Steam 未就绪或不存在该成就）。

#### `SteamHelper.getAchievementInfo(id: string, always: bool) -> object`

> | 参数 | 类型 | 说明 |
> | --- | --- | --- |
> | `id` | `string` | Steam 成就 ID |
> | `always` | `bool` | 为 `true` 时强制重新查询，可省略 |
>
> 返回成就字典，字段为 `name`、`display_name`、`description`、`hidden`、`achieved`、`unlock_time`；
> 取不到时为空。

#### `SteamHelper.getAchievements(always: bool) -> array`

> | 参数 | 类型 | 说明 |
> | --- | --- | --- |
> | `always` | `bool` | 为 `true` 时强制重新查询，可省略 |
>
> 返回成就字典组成的数组，字段同上。

#### `SteamHelper.countAchievements(achievedOnly: bool, always: bool) -> int`

> | 参数 | 类型 | 说明 |
> | --- | --- | --- |
> | `achievedOnly` | `bool` | 为 `true` 时只统计已解锁的成就 |
> | `always` | `bool` | 为 `true` 时强制重新拉取缓存，可省略 |
>
> 返回成就数量，Steam 未就绪时返回 `-1`。

#### `SteamHelper.triggerScreenshot() -> bool`

> 无参数。触发 Steam 截图成功返回 `true`。

## 兼容说明

- `Storages.selectDirectory` 由插件 `fstat.dll` 提供（旧式对话框）。引擎自带的 `selectFolder` 可在 TJS 初始化阶段替换该实现；若引擎未提供，则继续沿用插件实现。
- 插件仍按原方式从 `plugin/` 目录加载，本仓库不修改任何插件。
- 本项目使用 `msvcrt.dll`（VC-LTL）与 YY-Thunks，同一份二进制可在 Windows XP 及以上运行。

---

<a id="english"></a>

# KiriKiri Z (Krkrz) Personal Custom Build

A customized fork of KiriKiri Z. The official TJS and plugin interfaces are preserved; Steam integration, GPU group compositing and several built-in extension APIs are added.

## Requirements

| Component | Requirement |
| --- | --- |
| Visual Studio | 2022 or later, with the "Desktop development with C++" workload |
| CMake | 3.21 or later (needs `--preset` and preset v3) |
| Ninja | must be on `PATH` |
| NASM | 2.10.09 or later, `nasm.exe` must be on `PATH` |
| VC-LTL | 5.x, found by `VC-LTL helper for cmake.cmake` (repository root, parent directory or registry) |
| YY-Thunks | `YY_Thunks_for_Win2K.obj` is bundled, nothing else to install |

The Steam build additionally needs `extension/steamapi/sdk` (bundled, contains `steam_api.lib` and `steam_api.dll`); it is linked automatically by `extension/steamapi/steamapi.cmake`.

## Getting the sources

```bat
git clone <repository-url> mykrkrz
cd mykrkrz
git submodule update --init --recursive
```

baseclasses, freetype, libjpeg-turbo, libpng and others under `external/` are submodules; the build fails if they are not checked out.

## Build

Use the bundled `build.bat`. It locates Visual Studio and initializes the x86 environment, so opening a developer command prompt beforehand is not required:

```bat
build.bat
```

The default preset is `windows-msvc-x86-release`; other presets can be passed explicitly:

```bat
build.bat windows-msvc-x86-debug
build.bat windows-msvc-x64-release
```

`build.bat` simply runs `cmake --preset <preset>` and `cmake --build --preset <preset>`, so the same steps can be executed manually (inside an x86 Native Tools Command Prompt, or after `call "...\vcvarsall.bat" x86`):

```bat
cmake --preset windows-msvc-x86-release
cmake --build --preset windows-msvc-x86-release
```

Steam build only:

```bat
cmake --build .build\windows-msvc-x86-release --target KRKRZ_STEAM
```

### Outputs

| Target | Output file |
| --- | --- |
| `KRKRZ` | `.build\<preset>\tvpwin32\tvpwin32.exe` |
| `KRKRZ_CONSOLE` | `.build\<preset>\tvpwin32\tvpwin32_Console.exe` |
| `KRKRZ_STEAM` | `.build\<preset>\tvpwin32_Steam\tvpwin32_Steam.exe` (`steam_api.dll` is copied as well) |
| `KRKRZ_STEAM_CONSOLE` | `.build\<preset>\tvpwin32_Steam\tvpwin32_Steam_Console.exe` |

### Available presets

```
windows-msvc-x86-{release,debug,relwithdebinfo}
windows-msvc-x64-{release,debug,relwithdebinfo}
windows-clang-x86-{release,debug,relwithdebinfo}
windows-clang-x64-{release,debug,relwithdebinfo}
```

`debug` and `relwithdebinfo` also produce PDBs. The default target platform is x86.

## Extension API

All APIs below live in `extension/` and are registered by the engine at startup; no plugin DLL is required.
`SteamHelper` exists only in `__HAS_STEAM_API__` builds (`KRKRZ_STEAM` / `KRKRZ_STEAM_CONSOLE`).

### Folders

#### `Storages.selectFolder(params: object) -> bool`

> | Field | Type | Description |
> | --- | --- | --- |
> | `params.name` | `string` | Input: initial directory (storage name or local path). Output: the selected directory as a storage name |
> | `params.title` | `string` | Dialog title, optional |
> | `params.window` | `object` | Owner window, used to obtain the owner HWND, optional |
> | `params.rootDir` | `string` | Browse root; restricts navigation to that directory and forces the legacy dialog, optional |
>
> Returns `true` when a directory was selected and `false` when cancelled; a cancelled dialog is never shown twice.
> Uses `IFileOpenDialog` on Vista and later, and falls back to `SHBrowseForFolder` on XP and other environments.

#### `Storages.createFolders(path: string) -> bool`

> | Parameter | Type | Description |
> | --- | --- | --- |
> | `path` | `string` | Target directory, storage name or local path; missing intermediate levels are created |
>
> Equivalent to `mkdir -p`. Returns `true` if the directory already exists or was created; returns `false` if the path is occupied by a file or cannot be written.

### Language and fonts

#### `System.getLanguage() -> string`

> No parameters. Returns the UI language identifier: `schinese`, `tchinese`, `english` or `japanese`;
> other languages return the lowercased locale name, or `unknown` when unavailable.

#### `System.registerPrivateFont(file: string, alias: string) -> int`

> | Parameter | Type | Description |
> | --- | --- | --- |
> | `file` | `string` | Font file path, storage name or local path |
> | `alias` | `string` | Registration alias, optional |
>
> Returns the font `fontid`, or `0` on failure.

#### `System.unregisterPrivateFont(alias: string) -> bool` / `System.unregisterPrivateFont(fontid: int) -> bool`

> | Parameter | Type | Description |
> | --- | --- | --- |
> | `alias` | `string` | Alias given at registration time, mutually exclusive with `fontid` |
> | `fontid` | `int` | Value returned by `registerPrivateFont`, mutually exclusive with `alias` |
>
> Returns `true` on success.

#### `System.getPrivateFontName(alias: string) -> string` / `System.getPrivateFontName(fontid: int) -> string`

> | Parameter | Type | Description |
> | --- | --- | --- |
> | `alias` | `string` | Alias given at registration time, mutually exclusive with `fontid` |
> | `fontid` | `int` | Value returned by `registerPrivateFont`, mutually exclusive with `alias` |
>
> Returns the font display name, or an empty string when not found.

### Steam

`SteamHelper` is a global class (`global.SteamHelper`). It cannot be instantiated; all members are static.

#### `SteamHelper.init() -> bool`

> No parameters. Returns `true` when SteamAPI initialization succeeded.

#### `SteamHelper.setAchievement(id: string) -> bool`

> | Parameter | Type | Description |
> | --- | --- | --- |
> | `id` | `string` | Steam achievement ID |
>
> Returns `true` when the achievement was stored.

#### `SteamHelper.clearAchievement(id: string) -> bool`

> | Parameter | Type | Description |
> | --- | --- | --- |
> | `id` | `string` | Steam achievement ID |
>
> Returns `true` when the achievement was cleared.

#### `SteamHelper.clearAchievements() -> int`

> No parameters. Returns the number of achievements cleared by this call.

#### `SteamHelper.currentAppId() -> int`

> No parameters. Returns the current AppID.

#### `SteamHelper.restartAppIfNecessary(appid: int) -> bool`

> | Parameter | Type | Description |
> | --- | --- | --- |
> | `appid` | `int` | Expected AppID, must be greater than `0` |
>
> Returns `true` when the application has to be restarted by Steam.

#### `SteamHelper.queryAchievementState(id: string, always: bool) -> int`

> | Parameter | Type | Description |
> | --- | --- | --- |
> | `id` | `string` | Steam achievement ID |
> | `always` | `bool` | When `true`, re-query Steam and ignore the local cache, optional |
>
> Returns `1` unlocked, `0` locked, or `-1` unavailable (Steam not ready or no such achievement).

#### `SteamHelper.getAchievementInfo(id: string, always: bool) -> object`

> | Parameter | Type | Description |
> | --- | --- | --- |
> | `id` | `string` | Steam achievement ID |
> | `always` | `bool` | When `true`, re-query Steam, optional |
>
> Returns an achievement dictionary with the fields `name`, `display_name`, `description`, `hidden`, `achieved` and `unlock_time`; empty when not found.

#### `SteamHelper.getAchievements(always: bool) -> array`

> | Parameter | Type | Description |
> | --- | --- | --- |
> | `always` | `bool` | When `true`, re-query Steam, optional |
>
> Returns an array of achievement dictionaries with the same fields as above.

#### `SteamHelper.countAchievements(achievedOnly: bool, always: bool) -> int`

> | Parameter | Type | Description |
> | --- | --- | --- |
> | `achievedOnly` | `bool` | When `true`, only unlocked achievements are counted |
> | `always` | `bool` | When `true`, refresh the local cache first, optional |
>
> Returns the number of achievements, or `-1` when Steam is not ready.

#### `SteamHelper.triggerScreenshot() -> bool`

> No parameters. Returns `true` when the Steam screenshot was triggered.

## Compatibility notes

- `Storages.selectDirectory` is provided by the `fstat.dll` plugin (legacy dialog). The engine's `selectFolder` can replace that implementation during TJS initialization; when the engine does not provide it, the plugin implementation is used as-is.
- Plugins are still loaded from `plugin/` as before; this repository does not modify any plugin.
- The project links `msvcrt.dll` (VC-LTL) and YY-Thunks, so the same binary runs on Windows XP and later.
