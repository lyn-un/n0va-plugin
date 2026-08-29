# n0va-wallpaper-plugin

把本地壁纸（PNG/JPG/MP4）注入人工桌面（N0vaDesktop），与官方壁纸同等表现：
列表显示、可设置到桌面、可管理、重启保留。

> 仅支持人工桌面（N0vaDesktop）2.2.1.4 版本，其他版本请勿使用。

## 构建

要求：VS2022 + CMake 3.20+

依赖 Microsoft Detours（MIT，不入库）。构建前放到 `third_party/Detours/`：

```
git clone https://github.com/microsoft/Detours.git third_party/Detours
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

产物：`build/bin/Release/` 下 `n0va_plugin.dll` + `n0va_plugin.exe`。



## 使用

```
n0va_plugin.exe install                  部署 DLL + 打 DLL 注入补丁（需程序关闭）
n0va_plugin.exe uninstall                 还原 exe + 移除 DLL（三方 hash 防回退旧版）
n0va_plugin.exe launch [exe路径]          开发模式: Detours 注入启动 N0vaDesktop（不修改 exe）
n0va_plugin.exe set-host [目录]            设置/查看人工桌面安装目录
n0va_plugin.exe status                    补丁/DLL/宿主/管道状态
n0va_plugin.exe inject <文件> [选项]      导入壁纸
    --name <壁纸名>        （默认取文件名）
    --game-name <游戏中文名>（默认 原神; 见下方对照表）
    --author <作者名>      （默认同游戏中文名）
    --format static|dynamic（默认按 magic 自动判断）

n0va_plugin.exe list                     列出已导入壁纸
n0va_plugin.exe remove <vid>             删除注入壁纸
n0va_plugin.exe doctor [--fix-safe]      一致性检查（--fix-force 破坏性修复暂未实现）
```

### 使用方式（release 包，推荐）

1. 解压 release 包（含 `n0va_plugin.exe` + `n0va_plugin.dll`，GUI 版另有 `n0va-wallpaper-gui.exe` + `resources.neu`），**所有操作都在解压目录进行**
2. 首次使用：`n0va_plugin.exe set-host "D:\Program Files\N0vaDesktop"`（或直接运行任意命令，自动检测并记录人工桌面位置）
3. 关闭 N0vaDesktop，运行 `n0va_plugin.exe install`（部署 DLL + 打补丁一步到位）
4. 正常启动 N0vaDesktop（双击/自启/快捷方式均可，自动加载插件）
5. `n0va_plugin.exe inject "D:\壁纸\xxx.jpg" --name "我的壁纸" --game-name 原神`
6. 打开"我的壁纸" → 点击卡片"设置桌面"

插件数据（`wallpapers.json` 壁纸记录、`n0va_plugin.ini` 宿主路径、`n0va_plugin.log` 运行日志）
均存放在解压目录。人工桌面安装目录只会被部署 `n0va_plugin.dll` 和打补丁。

**注意**：install 后 DLL 是宿主启动的硬依赖，卸载必须走 `uninstall`，
不能手动删除宿主目录里的 DLL（否则 N0vaDesktop 无法启动）。
`launch` 仅作开发备用（不修改 exe，每次启动需手动执行）。

## 支持格式

v1 仅 PNG / JPG / MP4（magic 检测）。PNG/JPG 原样改名 .ndf；MP4 加 `00 00` 两字节前缀后改名 .ndf（与官方动态壁纸文件格式一致）。WebP/AVIF 等请先自行转换。


## 目录结构

```
n0va-wallpaper-plugin/
├── CMakeLists.txt        # 根构建（含 Detours 静态编译）
├── core/                 # core.lib: dll_inject / pipe_client / wallpaper_db / fs_helper
├── dll/                  # n0va_plugin.dll: dllmain / bootstrap / qt_bridge / injector / pipe_server
├── cli/                  # n0va_plugin.exe: 各子命令
├── test/                 # 离线单元测试（BUILD_TESTS=ON）
└── third_party/Detours/  # Microsoft Detours 4.0.1 (MIT, 不入库, 自行 clone)
```