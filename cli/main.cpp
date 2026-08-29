
#include <windows.h>
#include <cstdio>
#include <clocale>
#include <cstring>
#include <string>

int CmdInstall(int argc, wchar_t** argv);
int CmdUninstall(int argc, wchar_t** argv);
int CmdLaunch(int argc, wchar_t** argv);
int CmdInject(int argc, wchar_t** argv);
int CmdList(int argc, wchar_t** argv);
int CmdRemove(int argc, wchar_t** argv);
int CmdStatus(int argc, wchar_t** argv);
int CmdDoctor(int argc, wchar_t** argv);
int CmdSetHost(int argc, wchar_t** argv);

static void Usage() {
    printf(
        "n0va_plugin — N0vaDesktop 壁纸注入插件 (v0.1)\n"
        "\n"
        "用法:\n"
        "  n0va_plugin install                  部署 DLL + 打 DLL 注入补丁（双击宿主自动加载）\n"
        "  n0va_plugin uninstall                 还原 exe + 移除 DLL\n"
        "  n0va_plugin launch [exe路径]          开发模式: Detours 注入启动 N0vaDesktop\n"
        "  n0va_plugin set-host [目录]            设置/查看人工桌面安装目录\n"
        "  n0va_plugin status                    补丁/DLL/宿主/管道状态\n"
        "  n0va_plugin inject <文件> [选项]      导入壁纸\n"
        "      --name <壁纸名>        (默认取文件名)\n"
        "      --game-name <游戏名>   (默认 原神; 任意游戏名均可)\n"
        "      --author <作者名>      (默认同游戏名)\n"
        "      --format static|dynamic (默认按文件内容自动判断)\n"
        "  n0va_plugin list                     列出已导入壁纸\n"
        "  n0va_plugin remove <vid>             删除注入壁纸\n"
        "  n0va_plugin doctor [--fix-safe|--fix-force]  一致性检查\n");
}

int wmain(int argc, wchar_t** argv) {
    SetConsoleOutputCP(CP_UTF8);
    
    
    
    setlocale(LC_ALL, ".UTF-8");
    if (argc < 2) { Usage(); return 1; }
    std::wstring cmd = argv[1];
    if (cmd == L"install")   return CmdInstall(argc, argv);
    if (cmd == L"uninstall") return CmdUninstall(argc, argv);
    if (cmd == L"launch")    return CmdLaunch(argc, argv);
    if (cmd == L"inject")    return CmdInject(argc, argv);
    if (cmd == L"list")      return CmdList(argc, argv);
    if (cmd == L"remove")    return CmdRemove(argc, argv);
    if (cmd == L"status")    return CmdStatus(argc, argv);
    if (cmd == L"doctor")    return CmdDoctor(argc, argv);
    if (cmd == L"set-host")  return CmdSetHost(argc, argv);
    if (cmd == L"help" || cmd == L"--help" || cmd == L"-h") { Usage(); return 0; }
    printf("未知命令: %ls\n", argv[1]);
    Usage();
    return 1;
}
