# docx2tiptap 验收编辑器

独立 Vite + React + Tiptap 3 页面，用于加载由 C 库 `docx_parse` 输出的 JSON，检查节点类型、表格合并、图、脚注等与 [docx2tiptap](../docx2tiptap/) 是否一致。

## 依赖

- Node.js 18+
- 与仓库中 `docx2tiptap` **无编译耦合**；验收时只需其生成的 `.json` 文件。

## 安装与启动

```bash
cd editor-demo
npm install
npm run dev
```

浏览器打开终端提示的地址（默认 `http://127.0.0.1:5173`）。

## 与 docx2tiptap_cli 联调

1. 在仓库根目录按 [CMakeLists.txt](../CMakeLists.txt) 构建 `docx2tiptap_cli`（需本机已安装 libxml2 开发包）。
2. 将 DOCX 转为 JSON（PowerShell 示例）：

```powershell
.\build\docx2tiptap_cli.exe ..\docx2tiptap\tests\fixtures\minimal.docx | Out-File -Encoding utf8 sample.json
```

3. 在页面点击 **选择 JSON 文件**，选中 `sample.json`。
4. 查看正文渲染与侧栏 **doc.attrs.warnings** 是否与 CLI 输出一致。

## Schema 说明

- 自定义节点与 C 侧对齐：`abstractBlock`、`keywordsBlock`、`referenceItem`、`figure`、`footnote`、`endnote`；`Document` 上保留 `pageSetup`、`warnings`。
- **`doc` 扩展属性**：`headerHtml`、`footerHtml`（字符串或 `null`），用于编辑区外的页眉/页脚壳层；展示前经 DOMPurify 过滤，支持占位符 `{page}`、`{total}`（当前无分页时固定为 `1`）。若日后 `docx2tiptap` 解析 OOXML 页眉页脚，可写入同名字段供前端直接渲染。
- `docx2tiptap` 可能把 `footnote` / `endnote` 放在 `paragraph.content` 内（非标准 PM 结构）。导入时会自动 **提升** 为与段落并列的块，以便编辑器合法加载；若需像素级与 JSON 一致，请以 CLI 原始文件为准。

## 论文向排版（与格知达 demo 对齐的能力）

- **样式表**：[src/editor-thesis.scss](src/editor-thesis.scss) — A4 纸宽 794px 容器、小四正文、中英标题字体、`宋体/黑体/仿宋/楷体` 等 `font-family` 映射。
- **扩展**：`CustomParagraph`（`class`、`data-toc-placeholder`）、`ParagraphStyle`（`textIndent`、`marginTop`、`marginBottom`、`lineHeight`）、`BlockIndent`（块级 `margin-inline-start`，命令 `increaseBlockIndent` / `decreaseBlockIndent`）；`TextStyle` + `Color`、`FontFamily`、`FontSize`、`Highlight`（多色）。
- **工具栏**：[src/components/FormatToolbar.tsx](src/components/FormatToolbar.tsx) — 字体/字号/文字色/高亮/缩进、段前段后与行距、原有粗斜体下划线、标题、列表、对齐、链接等。

## 构建静态资源

```bash
npm run build
npm run preview
```
