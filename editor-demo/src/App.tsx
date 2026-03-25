import type { JSONContent } from '@tiptap/core'
import { EditorContent, useEditor } from '@tiptap/react'
import { useCallback, useEffect, useState } from 'react'

import { FormatToolbar } from '@/components/FormatToolbar'
import { buildExtensions } from '@/extensions/editor-extensions'
import { applyPageTemplate } from '@/utils/page-template'
import { sanitizeDocChromeHtml } from '@/utils/sanitize-doc-html'
import { liftBlockNotesFromInlines } from '@/utils/sanitize-docx-json'

const emptyDoc: JSONContent = {
  type: 'doc',
  attrs: {
    warnings: [],
    pageSetup: null,
    header: null,
    footer: null,
    headerHtml: null,
    footerHtml: null,
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
  const [localHeader, setLocalHeader] = useState('')
  const [localFooter, setLocalFooter] = useState('')

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

  useEffect(() => {
    if (!editor) return
    const h = editor.state.doc.attrs.headerHtml
    const f = editor.state.doc.attrs.footerHtml
    setLocalHeader(typeof h === 'string' ? h : '')
    setLocalFooter(typeof f === 'string' ? f : '')
  }, [editor, importTick])

  useEffect(() => {
    if (!editor) return
    const t = window.setTimeout(() => {
      editor.commands.updateAttributes('doc', {
        headerHtml: localHeader.trim() ? localHeader : null,
        footerHtml: localFooter.trim() ? localFooter : null,
      })
    }, 400)
    return () => window.clearTimeout(t)
  }, [localHeader, localFooter, editor])

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

  const headerRendered = sanitizeDocChromeHtml(applyPageTemplate(localHeader))
  const footerRendered = sanitizeDocChromeHtml(applyPageTemplate(localFooter))
  const showHeader = localHeader.trim().length > 0
  const showFooter = localFooter.trim().length > 0

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
            <p className="mb-2 text-xs text-zinc-500">
              支持简易 HTML（经 DOMPurify 过滤）。可用占位符 <code className="rounded bg-zinc-100 px-0.5">{'{page}'}</code>、
              <code className="rounded bg-zinc-100 px-0.5">{'{total}'}</code>（当前固定为 1）。
            </p>
            <label className="block text-xs font-medium text-zinc-600">页眉 HTML</label>
            <textarea
              className="mt-1 w-full rounded border border-zinc-200 p-2 font-mono text-xs"
              rows={4}
              value={localHeader}
              onChange={(e) => setLocalHeader(e.target.value)}
              spellCheck={false}
            />
            <label className="mt-2 block text-xs font-medium text-zinc-600">页脚 HTML</label>
            <textarea
              className="mt-1 w-full rounded border border-zinc-200 p-2 font-mono text-xs"
              rows={4}
              value={localFooter}
              onChange={(e) => setLocalFooter(e.target.value)}
              spellCheck={false}
            />
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
            {showHeader ? (
              <div
                className="doc-chrome-band header-band"
                dangerouslySetInnerHTML={{ __html: headerRendered }}
              />
            ) : null}
            <div
              className={
                showHeader || showFooter ? 'simple-editor-content' : 'simple-editor-content is-single-chrome'
              }
            >
              <EditorContent editor={editor} />
            </div>
            {showFooter ? (
              <div
                className="doc-chrome-band footer-band"
                dangerouslySetInnerHTML={{ __html: footerRendered }}
              />
            ) : null}
          </div>
        </main>
      </div>
    </div>
  )
}
