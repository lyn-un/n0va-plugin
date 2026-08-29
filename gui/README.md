# N0va 壁纸助手（GUI）

Neutralinojs 桌面界面，仅做两件事：选择文件 + 填写参数 → 调用 `n0va_plugin.exe` 注入；展示已导入壁纸列表（名字/类型）。不做预览、不做设置/删除。

## 目录

```
gui/
├── neutralino.config.json   # Neutralino 配置
├── run.bat                  # 开发启动（复制第三方运行时后运行，退出清理）
└── resources/
    ├── index.html
    ├── styles.css
    ├── app.js
    └── icons/appIcon.png
```

## 运行（开发模式）

要求：`n0va_plugin.exe` 与本程序（neutralino 二进制）放在**同一目录**。第三方依赖已就位：

- `third_party/neutralinojs/neutralino-win_x64.exe`（Neutralino 二进制 v6.0.0）
- `third_party/neutralinojs/neutralino.js`（客户端库 6.9.0）

`run.bat` 会先把这两个文件临时复制到位（exe → gui/，js → resources/js/），用 `--load-dir-res` 加载资源后运行，退出后清理副本。

```
gui\run.bat
```

## 打包

```
gui\build.bat
```

用官方 neu CLI 打包（npx @neutralinojs/neu build --release），产物在 `dist\n0va-wallpaper-gui\`：`n0va-wallpaper-gui.exe` + `resources.neu` 两个文件放同一目录即可分发。

## 依赖

- Neutralinojs 二进制 v6.0.0 与客户端库 6.9.0（`third_party/neutralinojs/`）
- `n0va_plugin.exe`（本仓库 CLI，构建产物）
- Node.js + npx（打包时用，运行不需要）
