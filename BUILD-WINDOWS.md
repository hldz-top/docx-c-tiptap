# Windows 构建（vcpkg + MSVC x64）

## 前置条件

- [CMake](https://cmake.org/download/)（建议 **4.2+**；若只装 **Visual Studio 2026**，需支持 **`Visual Studio 18 2026`** 生成器）
- [Visual Studio 2022](https://visualstudio.microsoft.com/) 或 **Build Tools 2022**，并安装 **使用 C++ 的桌面开发**（含 MSVC、Windows SDK）
- [vcpkg](https://github.com/microsoft/vcpkg)，并已安装 `libxml2:x64-windows`

## 推荐：一键配置并编译

在 **PowerShell** 中（仓库根目录 `c-tiptap`）：

```powershell
$env:VCPKG_ROOT = 'D:\你的\vcpkg'   # 若省略，脚本会尝试 D:\app\vcpkg-2026.03.18、D:\vcpkg 等常见路径
.\scripts\configure-windows.ps1
```

- 仅配置、不编译：`.\scripts\configure-windows.ps1 -NoBuild`
- 已装好 libxml2、跳过 vcpkg：`.\scripts\configure-windows.ps1 -SkipVcpkgInstall`

脚本会通过 **vswhere** 列出安装实例，优先选择 **`isComplete` 且存在 `Hostx64\x64\cl.exe` 的版本**（避免 VS 2026 不匹配旧版 `-requires` 组件 ID 导致误判「未找到 MSVC」），按 `installationVersion` 自动选用 **`Visual Studio 18 2026`** 或 **`Visual Studio 17 2022`**，并传入 `CMAKE_GENERATOR_INSTANCE`。

手动指定生成器：`.\scripts\configure-windows.ps1 -CMakeGenerator 'Visual Studio 17 2022'`

成功后 CLI 一般在：`build\Release\docx2tiptap_cli.exe`

## 可选：CMake Preset（完整 VS 安装时）

先设置环境变量 `VCPKG_ROOT`，再：

```powershell
cmake --preset windows-vcpkg-x64
cmake --build build --config Release
```

若仍报找不到 Visual Studio，请改用上面的 `configure-windows.ps1`。

## DOCX 转 JSON

```powershell
.\build\Release\docx2tiptap_cli.exe path\to\file.docx | Out-File -Encoding utf8 out.json
```

验收前端见 [editor-demo/README.md](editor-demo/README.md)。
