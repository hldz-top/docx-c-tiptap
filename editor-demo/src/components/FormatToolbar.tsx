import type { Editor } from '@tiptap/react'
import { useEditorState } from '@tiptap/react'

import { FONT_SIZE_OPTIONS, fontFamilyList } from '@/extensions/editor-extensions'
import { updateNearestBlockAttrs } from '@/utils/editor-block-attrs'

import { ToolbarBtn } from './ToolbarBtn'

const HIGHLIGHT_PRESETS = ['#fef08a', '#bbf7d0', '#fecaca', '#bfdbfe', '#e9d5ff']

function selectCls() {
  return 'max-w-[140px] rounded border border-zinc-200 bg-white px-1.5 py-1 text-xs text-zinc-800'
}

function safeHexColor(c: string | undefined): string {
  if (c && /^#[0-9A-Fa-f]{6}$/.test(c)) return c
  return '#000000'
}

export function FormatToolbar({
  editor,
  run,
  setLink,
}: {
  editor: Editor
  run: (fn: () => void) => void
  setLink: () => void
}) {
  useEditorState({
    editor,
    selector: ({ editor: e }) => e?.state.selection.anchor ?? 0,
  })

  const textStyle = useEditorState({
    editor,
    selector: ({ editor: e }) => (e ? e.getAttributes('textStyle') : {}),
  })

  const blockAttrs = useEditorState({
    editor,
    selector: ({ editor: e }) => {
      if (!e) return {}
      const { $from } = e.state.selection
      for (let d = $from.depth; d > 0; d--) {
        const node = $from.node(d)
        if (node.type.name === 'paragraph' || node.type.name === 'heading') {
          return {
            firstLineIndent: (node.attrs.firstLineIndent as string | null) ?? '',
            marginTop: (node.attrs.marginTop as string | null) ?? '',
            marginBottom: (node.attrs.marginBottom as string | null) ?? '',
            lineHeight: (node.attrs.lineHeight as string | null) ?? '',
            indent: (node.attrs.indent as number) ?? 0,
          }
        }
      }
      return {}
    },
  })

  const curFont = (textStyle?.fontFamily as string) || ''
  const curSize = (textStyle?.fontSize as string) || ''
  const curColor = safeHexColor(textStyle?.color as string | undefined)

  return (
    <div className="flex flex-wrap items-center gap-1 rounded-lg border border-zinc-200 bg-white p-2 shadow-sm">
      <ToolbarBtn onClick={() => run(() => editor.chain().undo().run())} label="撤销" />
      <ToolbarBtn onClick={() => run(() => editor.chain().redo().run())} label="重做" />
      <span className="mx-1 w-px self-stretch bg-zinc-200" />

      <select
        className={selectCls()}
        title="字体"
        value={curFont}
        onChange={(e) => {
          const v = e.target.value
          run(() => {
            if (v) void editor.chain().focus().setFontFamily(v).run()
            else void editor.chain().focus().unsetFontFamily().run()
          })
        }}
      >
        <option value="">字体默认</option>
        {fontFamilyList.map((f) => (
          <option key={f.value} value={f.value}>
            {f.label}
          </option>
        ))}
      </select>

      <select
        className={selectCls()}
        title="字号"
        value={curSize}
        onChange={(e) => {
          const v = e.target.value
          run(() => {
            if (v) void editor.chain().focus().setFontSize(v).run()
            else void editor.chain().focus().unsetFontSize().run()
          })
        }}
      >
        {FONT_SIZE_OPTIONS.map((o) => (
          <option key={o.label + o.value} value={o.value}>
            {o.label}
          </option>
        ))}
      </select>

      <label className="flex items-center gap-1 text-xs text-zinc-600">
        色
        <input
          type="color"
          className="h-7 w-8 cursor-pointer rounded border border-zinc-200 p-0"
          value={curColor}
          onChange={(e) => run(() => editor.chain().focus().setColor(e.target.value).run())}
        />
      </label>
      <ToolbarBtn
        onClick={() => run(() => editor.chain().focus().unsetColor().run())}
        label="清除颜色"
      />

      <span className="text-xs text-zinc-500">高亮</span>
      {HIGHLIGHT_PRESETS.map((c) => (
        <button
          key={c}
          type="button"
          title={c}
          className="h-6 w-6 rounded border border-zinc-300"
          style={{ backgroundColor: c }}
          onClick={() => run(() => editor.chain().focus().toggleHighlight({ color: c }).run())}
        />
      ))}
      <ToolbarBtn onClick={() => run(() => editor.chain().focus().unsetHighlight().run())} label="清除高亮" />

      <ToolbarBtn
        onClick={() => run(() => editor.chain().focus().toggleSuperscript().run())}
        label="上标"
      />
      <ToolbarBtn
        onClick={() => run(() => editor.chain().focus().toggleSubscript().run())}
        label="下标"
      />

      <span className="mx-1 w-px self-stretch bg-zinc-200" />
      <ToolbarBtn
        onClick={() => run(() => editor.chain().focus().increaseBlockIndent().run())}
        label="缩进+"
      />
      <ToolbarBtn
        onClick={() => run(() => editor.chain().focus().decreaseBlockIndent().run())}
        label="缩进−"
      />
      <span className="text-xs text-zinc-500">级{(blockAttrs.indent as number) ?? 0}</span>

      <span className="mx-1 w-px self-stretch bg-zinc-200" />
      <select
        className={selectCls()}
        title="首行缩进"
        value={(blockAttrs.firstLineIndent as string) || ''}
        onChange={(e) => {
          const v = e.target.value
          run(() => updateNearestBlockAttrs(editor, { firstLineIndent: v || null }))
        }}
      >
        <option value="">首行缩进无</option>
        <option value="2em">2em</option>
        <option value="24pt">24pt</option>
        <option value="0.74cm">0.74cm</option>
      </select>

      <select
        className={selectCls()}
        title="段前距"
        value={(blockAttrs.marginTop as string) || ''}
        onChange={(e) => {
          const v = e.target.value
          run(() => updateNearestBlockAttrs(editor, { marginTop: v || null }))
        }}
      >
        <option value="">段前默认</option>
        <option value="6pt">6pt</option>
        <option value="12pt">12pt</option>
        <option value="18pt">18pt</option>
      </select>

      <select
        className={selectCls()}
        title="段后距"
        value={(blockAttrs.marginBottom as string) || ''}
        onChange={(e) => {
          const v = e.target.value
          run(() => updateNearestBlockAttrs(editor, { marginBottom: v || null }))
        }}
      >
        <option value="">段后默认</option>
        <option value="6pt">6pt</option>
        <option value="12pt">12pt</option>
        <option value="18pt">18pt</option>
      </select>

      <select
        className={selectCls()}
        title="行距"
        value={(blockAttrs.lineHeight as string) || ''}
        onChange={(e) => {
          const v = e.target.value
          run(() => updateNearestBlockAttrs(editor, { lineHeight: v || null }))
        }}
      >
        <option value="">行距默认</option>
        <option value="1">1</option>
        <option value="1.25">1.25</option>
        <option value="1.5">1.5</option>
        <option value="2">2</option>
        <option value="12pt">12pt</option>
        <option value="15.6pt">15.6pt</option>
      </select>

      <span className="mx-1 w-px self-stretch bg-zinc-200" />
      <ToolbarBtn
        onClick={() => run(() => editor.chain().toggleBold().run())}
        label="粗体"
        active={editor.isActive('bold')}
      />
      <ToolbarBtn
        onClick={() => run(() => editor.chain().toggleItalic().run())}
        label="斜体"
        active={editor.isActive('italic')}
      />
      <ToolbarBtn
        onClick={() => run(() => editor.chain().toggleUnderline().run())}
        label="下划线"
        active={editor.isActive('underline')}
      />
      <ToolbarBtn
        onClick={() => run(() => editor.chain().toggleStrike().run())}
        label="删除线"
        active={editor.isActive('strike')}
      />
      <span className="mx-1 w-px self-stretch bg-zinc-200" />
      <ToolbarBtn
        onClick={() => run(() => editor.chain().toggleHeading({ level: 1 }).run())}
        label="H1"
        active={editor.isActive('heading', { level: 1 })}
      />
      <ToolbarBtn
        onClick={() => run(() => editor.chain().toggleHeading({ level: 2 }).run())}
        label="H2"
        active={editor.isActive('heading', { level: 2 })}
      />
      <ToolbarBtn
        onClick={() => run(() => editor.chain().toggleHeading({ level: 3 }).run())}
        label="H3"
        active={editor.isActive('heading', { level: 3 })}
      />
      <span className="mx-1 w-px self-stretch bg-zinc-200" />
      <ToolbarBtn
        onClick={() => run(() => editor.chain().toggleBulletList().run())}
        label="无序列表"
        active={editor.isActive('bulletList')}
      />
      <ToolbarBtn
        onClick={() => run(() => editor.chain().toggleOrderedList().run())}
        label="有序列表"
        active={editor.isActive('orderedList')}
      />
      <span className="mx-1 w-px self-stretch bg-zinc-200" />
      <ToolbarBtn onClick={() => setLink()} label="链接" active={editor.isActive('link')} />
      <ToolbarBtn
        onClick={() => run(() => editor.chain().setTextAlign('left').run())}
        label="左对齐"
      />
      <ToolbarBtn
        onClick={() => run(() => editor.chain().setTextAlign('center').run())}
        label="居中"
      />
      <ToolbarBtn
        onClick={() => run(() => editor.chain().setTextAlign('right').run())}
        label="右对齐"
      />
    </div>
  )
}
