import type { JSONContent } from '@tiptap/core'
import { EditorContent, useEditor } from '@tiptap/react'
import { useCallback, useEffect, useState } from 'react'

import { DocChromeEditor } from '@/components/DocChromeEditor'
import { FormatToolbar } from '@/components/FormatToolbar'
import { buildExtensions } from '@/extensions/editor-extensions'
import { liftBlockNotesFromInlines } from '@/utils/sanitize-docx-json'

const emptyDoc: JSONContent = {
  type: 'doc',
  attrs: {
    warnings: [],
    pageSetup: null,
    header: null,
    footer: null,
  },
  content: [{ type: 'paragraph' }],
}

function readWarnings(doc: JSONContent): string[] {
  const a = doc.attrs as { warnings?: unknown } | undefined
  const w = a?.warnings
  return Array.isArray(w) ? (w as string[]) : []
}

export default function App() {
  const [warnings, setWarnings] = useState<string[]>([])
  const [importTick, setImportTick] = useState(0)

  const editor = useEditor({
    extensions: buildExtensions(),
    content: emptyDoc,
    editorProps: {
      attributes: {
        class: 'tiptap ProseMirror simple-editor',
      },
    },
  })

  useEffect(() => {
    if (!editor) return
    const upd = () => setWarnings(readWarnings(editor.getJSON() as JSONContent))
    editor.on('update', upd)
    upd()
    return () => {
      editor.off('update', upd)
    }
  }, [editor])

  const onImportJson = useCallback(
    (e: React.ChangeEvent<HTMLInputElement>) => {
      const f = e.target.files?.[0]
      if (!f || !editor) return
      const r = new FileReader()
      r.onload = () => {
        try {
          const raw = JSON.parse(String(r.result)) as JSONContent
          const fixed = liftBlockNotesFromInlines(raw)
          editor.commands.setContent(fixed, { emitUpdate: true })
          setWarnings(readWarnings(fixed))
          setImportTick((x) => x + 1)
        } catch (err) {
          alert(`JSON 解析失败: ${err instanceof Error ? err.message : String(err)}`)
        }
      }
      r.readAsText(f, 'UTF-8')
      e.target.value = ''
    },
    [editor],
  )

  const run = useCallback(
    (fn: () => void) => {
      if (!editor) return
      fn()
      editor.chain().focus().run()
    },
    [editor],
  )

  const setLink = useCallback(() => {
    if (!editor) return
    const prev = editor.getAttributes('link').href as string | undefined
    const url = window.prompt('链接地址', prev ?? 'https://')
    if (url === null) return
    if (url === '') {
      editor.chain().focus().extendMarkRange('link').unsetLink().run()
      return
    }
    editor.chain().focus().extendMarkRange('link').setLink({ href: url }).run()
  }, [editor])

  if (!editor) {
    return <div className="p-6 text-zinc-600">正在初始化编辑器…</div>
  }

  return (
    <div className="min-h-screen bg-zinc-100 text-zinc-900">
      <header className="border-b border-zinc-200 bg-white px-4 py-3 shadow-sm">
        <h1 className="text-lg font-semibold tracking-tight">docx2tiptap 验收编辑器</h1>
        <p className="mt-1 text-sm text-zinc-500">
          导入由 <code className="rounded bg-zinc-100 px-1">docx2tiptap_cli</code> 生成的 JSON，检查结构与排版。
        </p>
      </header>

      <div className="mx-auto flex max-w-6xl flex-col gap-4 p-4 lg:flex-row">
        <aside className="lg:w-80 shrink-0 space-y-3">
          <div className="rounded-lg border border-zinc-200 bg-white p-3 shadow-sm">
            <h2 className="mb-2 text-sm font-medium text-zinc-700">导入</h2>
            <label className="flex cursor-pointer flex-col gap-2">
              <span className="rounded-md bg-blue-600 px-3 py-2 text-center text-sm font-medium text-white hover:bg-blue-700">
                选择 JSON 文件
              </span>
              <input type="file" accept=".json,application/json" className="hidden" onChange={onImportJson} />
            </label>
          </div>

          <div className="rounded-lg border border-zinc-200 bg-white p-3 text-sm shadow-sm">
            <h2 className="mb-2 font-medium text-zinc-800">页眉 / 页脚</h2>
            <p className="text-xs text-zinc-500">
              与正文相同：在编辑区上方、下方的条带内直接编辑，内容保存在{' '}
              <code className="rounded bg-zinc-100 px-0.5">doc.attrs.header</code> /{' '}
              <code className="rounded bg-zinc-100 px-0.5">footer</code>（Tiptap JSON 块数组），与 docx2tiptap 输出一致。
            </p>
          </div>

          <div className="rounded-lg border border-amber-200 bg-amber-50 p-3 text-sm">
            <h2 className="mb-2 font-medium text-amber-900">doc.attrs.warnings</h2>
            {warnings.length === 0 ? (
              <p className="text-amber-800/80">无</p>
            ) : (
              <ul className="list-inside list-disc space-y-1 text-amber-950">
                {warnings.map((w, i) => (
                  <li key={i}>{w}</li>
                ))}
              </ul>
            )}
          </div>
        </aside>

        <main className="min-w-0 flex-1 space-y-3">
          <FormatToolbar editor={editor} run={run} setLink={setLink} />
          <div className="simple-editor-wrapper rounded-lg bg-zinc-50/80 p-2 shadow-inner">
            <DocChromeEditor
              mainEditor={editor}
              docAttr="header"
              importTick={importTick}
              className="doc-chrome-band header-band"
              placeholder="页眉…"
            />
            <div className="simple-editor-content">
              <EditorContent editor={editor} />
            </div>
            <DocChromeEditor
              mainEditor={editor}
              docAttr="footer"
              importTick={importTick}
              className="doc-chrome-band footer-band"
              placeholder="页脚…"
            />
          </div>
        </main>
      </div>
    </div>
  )
}
