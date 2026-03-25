# DOCX → Tiptap JSON：C 解析库选型与架构设计

> 目标：用 C 语言实现一个将 `.docx` 文件解析并转换为 Tiptap JSON 的库
> 定位：纯库，无副作用，支持编译为 CLI / `.so` / `.wasm` 多端复用

---

## 目录

1. [设计原则](#1-设计原则)
2. [整体架构分层](#2-整体架构分层)
3. [技术选型](#3-技术选型)
4. [各模块详细设计](#4-各模块详细设计)
5. [OOXML → Tiptap 节点映射表](#5-ooxml--tiptap-节点映射表)
6. [关键跨模块流程](#6-关键跨模块流程)
7. [目录结构](#7-目录结构)
8. [依赖汇总与 vendoring 策略](#8-依赖汇总与-vendoring-策略)
9. [开发顺序建议](#9-开发顺序建议)

---

## 1. 设计原则

三个核心约束，所有后续决策均从这里推导：

1. **单向转换**：DOCX → Tiptap JSON，不做反向
2. **无副作用的纯库**：不做文件 I/O，不持有全局状态，调用方传入字节流，返回 JSON 字符串
3. **可移植**：同一份代码能编译成 CLI / `.so` / `.wasm`，不依赖平台特性

> **关键认知**：DOCX 本质是一个 ZIP 包，核心是 `word/document.xml`。你需要解析的不是"任意 XML"，而是 **OOXML（Office Open XML）的子集**。不需要做通用 XML 解析器，而是做一个 **OOXML → Tiptap JSON 的专用转换器**。

---

## 2. 整体架构分层

```
.docx 字节流（uint8_t* buf, size_t len）
        │
        ▼
┌─────────────────────────────────────┐
│  公开 API 层  docx2tiptap.h          │
│  docx_parse() · docx_free()          │
│  docx_error_str()                    │
│  对外只暴露这三个符号                 │
└──────────────┬──────────────────────┘
               │
               ▼
┌─────────────────────────────────────┐
│  模块 1 — ZIP 解包  zip_reader        │
│  miniz（单头文件）                    │
│  提取五个目标文件到内存缓冲，不写磁盘  │
└──────┬──────────┬──────────┬────────┘
       │          │          │
       ▼          ▼          ▼
┌──────────┐ ┌──────────┐ ┌──────────────┐
│模块 2a   │ │模块 2b   │ │模块 2c       │
│styles_   │ │numbering_│ │rels_reader   │
│reader    │ │reader    │ │              │
│styles.xml│ │numbering │ │.rels →       │
│→StyleMap │ │.xml →    │ │RelMap        │
│          │ │NumMap    │ │(rId→target)  │
└────┬─────┘ └────┬─────┘ └──────┬───────┘
     └────────────┴──────────────┘
                  │
                  ▼
┌─────────────────────────────────────┐
│  模块 3 — 解析上下文  ParseContext    │
│  StyleMap + NumMap + RelMap          │
│  + Arena 分配器 + Options            │
└──────────────┬──────────────────────┘
               │
               ▼
┌─────────────────────────────────────┐
│  模块 4 — 正文解析  doc_reader        │
│  document.xml DOM 遍历               │
│  产出 OoxmlDoc 内部 IR 树             │
│  两阶段：先收集段落，再归组列表        │
└──────┬──────┬──────┬──────┬─────────┘
       │      │      │      │  按节点类型分发
       ▼      ▼      ▼      ▼
┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────────┐
│模块 5a   │ │模块 5b   │ │模块 5c   │ │模块 5d       │
│block_    │ │inline_   │ │table_    │ │media_mapper  │
│mapper    │ │mapper    │ │mapper    │ │              │
│段落/标题 │ │run/mark/ │ │表格/行/  │ │图片/脚注/    │
│区块      │ │链接      │ │单元格    │ │页面设置      │
└────┬─────┘ └────┬─────┘ └────┬─────┘ └──────┬───────┘
     └────────────┴────────────┴──────────────┘
                  │
                  ▼
┌─────────────────────────────────────┐
│  模块 6 — JSON 序列化  json_writer    │
│  yyjson                              │
│  TiptapNode 树 → JSON 字符串         │
│  输出到调用方提供的缓冲或 malloc 堆  │
└─────────────────────────────────────┘
```

---

## 3. 技术选型

### 3.1 XML 解析器

**选型：`libxml2` DOM 模式**

两种流派对比：

| 方式 | 代表库 | 优点 | 缺点 |
|---|---|---|---|
| SAX 流式 | `expat` | 内存占用极低，大文档友好 | 需自维护状态机（栈追踪当前层级） |
| DOM 全量 | `libxml2` | 随机访问任意节点，开发直观 | 内存占用稍大（DOCX 通常几 MB，可接受） |

**选择 `libxml2` DOM 的理由**：OOXML 中 `w:rPr`（run properties）需要频繁向上查找父节点或引用 `styles.xml`，DOM 的随机访问优势明显。DOCX 文档体积通常在几 MB 以内，DOM 全量加载完全可接受。

> 如果 WASM 体积是硬约束（< 500KB），可换成 `pugixml`（C++，极轻量）或手写简化 XML 解析器（OOXML 的 XML 结构规则，不需要完整规范支持）。

### 3.2 ZIP 解包

**选型：`miniz`**

- 单个 `.c` + `.h` 文件，MIT 协议
- 直接 vendor 进仓库，零外部依赖
- Emscripten 编译 WASM 无障碍

### 3.3 JSON 序列化

**选型：`yyjson`**

- 纯 C 实现，零依赖
- 读写性能是现有 C JSON 库最快之一
- API 设计清晰（builder 模式），单头文件可 vendor

### 3.4 内存管理

**选型：Arena 分配器**

整个解析过程是"一次性消费"——输入进来，中间态构建，输出 JSON，然后整体销毁。

Arena 的做法是预先申请一大块内存（如 4MB），所有内部分配都从这里线性分配，释放时只需 `free` 这一块。

**优势**：
- 零碎内存管理 bug 消失
- 缓存局部性好
- 销毁时间 O(1)

---

## 4. 各模块详细设计

### 模块 1 — ZIP 解包（zip_reader）

需要按依赖顺序提取的五个目标文件：

| 文件 | 用途 | 必须 |
|---|---|---|
| `word/_rels/document.xml.rels` | 建立 rId → 媒体路径映射 | 是 |
| `word/styles.xml` | 样式名 → 格式属性 | 是 |
| `word/numbering.xml` | 列表抽象定义 | 有列表时 |
| `word/document.xml` | 正文 | 是 |
| `word/media/*` | 图片字节流 | 有图时 |

设计为"按需提取"接口而非"全量解压"，避免大文件浪费内存。

---

### 模块 2a — styles_reader

**输出**：`StyleMap`（哈希表，键为样式 ID 字符串）

需从 `styles.xml` 提取的信息：

- 段落样式名（`w:name val`）到语义类型的映射，例如 `"heading 1"` → `HEADING_1`，`"摘要"` → `ABSTRACT`
- 字符样式的格式属性（加粗、斜体、字体名、字号）
- 样式继承链（`w:basedOn`），最多追三级，避免无限递归
- 文档默认格式（`w:docDefaults`），作为所有样式的最低优先级底座

**样式优先级（高→低）**：直接格式 > 字符样式 > 段落样式 > 文档默认值

---

### 模块 2b — numbering_reader

**输出**：`NumMap`

OOXML 列表是两级结构：
- `abstractNum`：抽象定义，描述每级的格式
- `num`：具体实例，引用 abstractNum

`w:p` 里的 `w:numPr` 指向 `numId + ilvl`（实例 ID + 缩进级别）。

需提取：
- `numId → abstractNumId` 的映射
- 每个 `abstractNumId` 每级的 `numFmt`（`bullet` / `decimal` / `lowerLetter` 等）

---

### 模块 2c — rels_reader

**输出**：`RelMap`（哈希表，键为 `rId`）

每条记录三个字段：`Id`、`Type`（imageRelType / hyperlinkRelType 等）、`Target`（相对路径或 URL）。

---

### 模块 3 — 解析上下文（ParseContext）

只读共享的数据包，聚合前置模块输出，传给所有后续模块：

```
ParseContext {
    StyleMap    styles
    NumMap      numbering
    RelMap      rels
    Arena*      arena        // 统一内存分配器
    Options     opts         // 调用方配置项
}
```

**Options 应包含**：
- 图片处理策略：`base64_inline` / `skip` / `extract_to_path`
- 未知样式的回退行为：映射到 paragraph 还是报 warning
- 是否保留页面设置元数据

---

### 模块 4 — 正文解析（doc_reader）

**核心：两阶段处理**

**第一阶段：线性遍历 `w:body` 直接子节点**

对每个 `w:p` 生成 `OoxmlPara` 中间对象，记录：
- 段落样式 ID（查 StyleMap 得语义类型）
- `numId + ilvl`（有则为列表项，无则为 -1）
- `run` 列表（每个 run 持有文字 + 已合并的格式属性）
- 特殊内容标志：是否含图、是否含脚注引用、是否含域代码

**第二阶段：归组处理**

线性序列里，相邻且具有相同 `numId + ilvl` 的段落需收拢进同一个 `bulletList` 或 `orderedList` 容器。图题段落（紧跟图片段落之后的特定样式段落）需和图片节点配对合并为 `figure` 复合节点。

**内部 IR（OoxmlDoc）结构**：

```
OoxmlDoc
  └─ children[]
       ├─ OoxmlPara       // 普通段落 / 标题 / 各区块
       ├─ OoxmlList       // 归组后的列表容器
       │    └─ items[]
       │         └─ OoxmlPara
       ├─ OoxmlTable
       │    └─ rows[] → cells[] → OoxmlPara[]
       └─ OoxmlFigure     // 图 + 图题配对
```

---

### 模块 5a — block_mapper

根据 `OoxmlPara.styleType` 分发到 Tiptap node：

| OoxmlPara.styleType | Tiptap node type | attrs |
|---|---|---|
| `HEADING_1..4` | `heading` | `{ level: 1..4 }` |
| `ABSTRACT_ZH / ABSTRACT_EN` | `abstractBlock` | `{ lang: "zh"/"en" }` |
| `KEYWORDS_ZH / KEYWORDS_EN` | `keywordsBlock` | — |
| `BODY_TEXT` | `paragraph` | — |
| `REFERENCE_ITEM` | `referenceItem` | — |
| `CAPTION_FIGURE / CAPTION_TABLE` | 归入上层 figure/table | — |
| `UNKNOWN` | `paragraph` + warning | — |

---

### 模块 5b — inline_mapper

每个 `OoxmlRun` 转成一个 `text` node，附带 marks 数组。

需处理的特殊 run 类型：
- **超链接**（`w:hyperlink`）：含 `rId`，查 RelMap 拿 href
- **脚注引用**（`w:footnoteReference`）：需关联 `footnotes.xml` 里的内容
- **域代码 run**（`w:fldChar` + `w:instrText`）：大部分忽略，页码域需识别并跳过

---

### 模块 5c — table_mapper

递归结构：`table → tableRow → tableCell → paragraph[]`

单元格内容复用 `block_mapper` 和 `inline_mapper`。

额外提取的属性：
- 列宽（`w:tblGrid`）
- 水平合并（`w:gridSpan`）
- 垂直合并（`w:vMerge`）

合并信息作为 `attrs` 透传给 Tiptap `tableCell` node。

---

### 模块 5d — media_mapper

**图片处理流程**：
1. 从 `w:drawing → wp:inline/wp:anchor → a:blip` 路径拿到 `r:embed`（rId）
2. 查 RelMap 拿到媒体路径
3. 从 ZIP 里读字节流
4. 按 `opts.image_strategy` 决定 base64 内联或其他策略
5. 记录布局方式：`wp:inline`（嵌入式）/ `wp:anchor`（浮动），浮动标记 `layout: "float"`

**脚注处理**：`footnotes.xml` 是独立文件，结构与 `document.xml` 类似，用相同的 `doc_reader` 递归处理，输出的 TiptapNode 树嵌入到引用处的 `footnote` node 里。

**页面设置**：从 `w:sectPr` 提取页边距（`w:pgMar`）、纸张尺寸（`w:pgSz`）、方向。不映射为 Tiptap node，而是作为 `doc.attrs.pageSetup` 元数据挂在顶层节点上。

---

### 模块 6 — JSON 序列化（json_writer）

输出严格遵循 Tiptap `JSONContent` 格式：

```json
{
  "type": "doc",
  "attrs": {
    "pageSetup": {
      "marginTop": 2540,
      "marginBottom": 2540,
      "marginLeft": 3175,
      "marginRight": 3175,
      "pageWidth": 21000,
      "pageHeight": 29700
    },
    "warnings": []
  },
  "content": [
    {
      "type": "heading",
      "attrs": { "level": 1 },
      "content": [{ "type": "text", "text": "第一章 绪论" }]
    },
    {
      "type": "paragraph",
      "content": [
        {
          "type": "text",
          "text": "这是正文内容",
          "marks": [{ "type": "bold" }]
        }
      ]
    }
  ]
}
```

此层不做任何业务逻辑，只是 IR 树的忠实序列化。warning 作为顶层 `attrs.warnings[]` 数组输出，不影响主体内容。

---

## 5. OOXML → Tiptap 节点映射表

| OOXML 元素 | Tiptap node / mark | 难度 | 备注 |
|---|---|---|---|
| `w:p` + `w:r` + `w:t` | `paragraph` → `text` | 易 | 基础路径 |
| `w:pStyle` → styles.xml 查找 | `heading` / `abstractBlock` 等 | 中 | 需样式继承链解析 |
| `w:numPr` → numbering.xml | `bulletList` / `orderedList` + `listItem` | **难** | 需两阶段处理 |
| `w:tbl` / `w:tr` / `w:tc` | `table` → `tableRow` → `tableCell` | 中 | 需处理合并单元格 |
| `w:hyperlink` + `r:id` rels | `link mark { href }` | 中 | 需查 RelMap |
| `w:drawing` / `a:blip`（图片）| `image { src }` | **难** | 需先解析 .rels 建映射表 |
| `w:footnote` / `w:endnote` | 自定义 `footnote` node | **难** | 需关联 footnotes.xml |
| `w:b` / `w:i` / `w:u` / `w:strike` | `bold` / `italic` / `underline` / `strike` marks | 易 | 直接映射 |
| `w:sectPr`（页面设置）| `doc.attrs.pageSetup`（元数据）| 中 | 不作为 node，挂顶层 attrs |
| `wp:anchor`（浮动图片）| `image { layout: "float" }` | 中 | 检测"非嵌入式"错误的依据 |
| 手写编号段落（无 `w:numPr`）| `paragraph` + warning | 中 | 正文匹配 `/^\d+\./` 模式检测 |

> **最大陷阱**：列表（`w:numPr`）需先扫描全文所有段落，收集同 `numId+ilvl` 的连续段落后才能构建列表容器节点，这需要两阶段处理，是架构设计的核心难点。图片同理：需先解析 `.rels` 文件建立 `r:id → media` 路径的映射表，再处理正文。

---

## 6. 关键跨模块流程

### 6.1 列表归组流程

```
第一阶段：线性扫描 w:body
    ↓
每个 w:p 记录 numId + ilvl（无则为 -1）
    ↓
第二阶段：归组扫描
    ↓
连续相同 numId 段落 → 归入同一 OoxmlList
    ↓
查 NumMap 判断类型：bullet / ordered / unknown
    ↓
手写编号检测：无 numPr 但文字匹配 /^\d+\./
    ↓
输出 TiptapList node
```

### 6.2 图片处理流程

```
前置：解析 .rels → RelMap（rId → target 路径）
    ↓
正文遇到 w:drawing
    ↓
判断 wp:inline（嵌入式）/ wp:anchor（浮动）→ 记录 layout 属性
    ↓
提取 a:blip r:embed（rId）→ 查 RelMap → 拿到 media 路径
    ↓
按 opts.image_strategy 分支：
    ├─ base64_inline → 从 ZIP 读字节流 → base64 编码 → data URI
    └─ skip / url → 占位符
    ↓
扫描后续段落：caption 样式 → 配对为图题
    ↓
输出 TiptapFigure node { image, caption }
```

---

## 7. 目录结构

```
docx2tiptap/
├── include/
│   └── docx2tiptap.h           // 唯一对外头文件，只暴露三个符号
├── src/
│   ├── api.c                   // 公开 API 实现，串联所有模块
│   ├── arena.c / arena.h       // Arena 分配器
│   ├── zip_reader.c / .h
│   ├── styles_reader.c / .h
│   ├── numbering_reader.c / .h
│   ├── rels_reader.c / .h
│   ├── parse_context.h         // 纯结构定义，无实现
│   ├── doc_reader.c / .h
│   ├── ooxml_ir.h              // OoxmlDoc / OoxmlPara 等结构定义
│   ├── tiptap_ir.h             // TiptapNode 结构定义
│   ├── block_mapper.c / .h
│   ├── inline_mapper.c / .h
│   ├── table_mapper.c / .h
│   ├── media_mapper.c / .h
│   └── json_writer.c / .h
├── vendor/
│   ├── miniz.c / miniz.h       // vendored，不修改
│   └── yyjson.c / yyjson.h     // vendored，不修改
├── cli/
│   └── main.c                  // CLI 入口，薄壳，< 100 行
├── bindings/
│   ├── wasm/                   // Emscripten 胶水代码
│   └── python/                 // ctypes 封装
└── tests/
    ├── fixtures/               // 真实 .docx 测试文件（5~10 个）
    └── test_*.c
```

**公开 API 设计原则**：对外只暴露三个符号：

```c
// 解析：传入字节流，返回 JSON 字符串（调用方负责调用 docx_free 释放）
char* docx_parse(const uint8_t* buf, size_t len, const DocxOptions* opts);

// 释放：释放 docx_parse 返回的字符串
void docx_free(char* json);

// 错误描述：返回最近一次错误的可读字符串
const char* docx_error_str(void);
```

---

## 8. 依赖汇总与 vendoring 策略

| 依赖 | 用途 | 引入方式 | WASM 兼容 | 协议 |
|---|---|---|---|---|
| `miniz` | ZIP 解包 | vendor 单文件 | ✅ | MIT |
| `libxml2` | XML DOM 解析 | 系统库 / 静态链接 | ⚠️ 体积大，需裁剪 | MIT |
| `yyjson` | JSON 序列化 | vendor 单文件 | ✅ | MIT |

**WASM 体积优化**：如果目标 `.wasm` 需控制在 500KB 以内，`libxml2` 可替换为：
- `pugixml`（C++，极轻量，~200KB）
- 手写简化 XML 解析器（OOXML 的 XML 结构非常规则，实现成本可控）

CLI 和 `.so` 场景无此顾虑，保持 `libxml2`。

---

## 9. 开发顺序建议

按此顺序每一步结束后都有可运行的产出，避免大量代码写完才能第一次跑通：

| 步骤 | 内容 | 验收标准 |
|---|---|---|
| 1 | Arena 分配器 | 单元测试通过，valgrind 无泄漏 |
| 2 | zip_reader | 能解包 DOCX，提取 document.xml 打印到 stdout |
| 3 | styles_reader + rels_reader | 能打印 StyleMap 和 RelMap 内容 |
| 4 | doc_reader 第一阶段 | 能线性输出段落列表（忽略样式） |
| 5 | block_mapper + json_writer | 能输出最基本的 paragraph/heading JSON，接入 Tiptap 前端验证 |
| 6 | inline_mapper | 加粗、斜体、链接 mark 正确输出 |
| 7 | numbering_reader + 列表归组 | **最难，单独攻关**，用含多级列表的真实文档验证 |
| 8 | table_mapper | 包含合并单元格的表格正确输出 |
| 9 | media_mapper | 图片 base64 内联，脚注嵌入，页面设置元数据 |
| 10 | CLI 包装 | `docx2tiptap input.docx` 输出 JSON 到 stdout |
| 11 | WASM / FFI binding | 按需选择：浏览器端用 Emscripten，Python 后端用 ctypes |

**工程建议**：
- 从一开始就准备 5~10 个有代表性的真实论文 DOCX（含标题、列表、表格、脚注、公式），作为固定测试 fixtures
- 明确定义你的 OOXML 子集范围：`supported`（转换）/ `ignored`（丢弃）/ `preserved as opaque`（保留原始 XML 不转换但不丢弃）
- 先读 ECMA-376 Part 1 的 WordprocessingML 章节前 10 页，理解 `w:document → w:body → w:p → w:r → w:t` 这条主干，再动手

---

*文档生成时间：2026-03-25*
