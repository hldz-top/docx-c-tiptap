import type { Editor, JSONContent } from '@tiptap/core'
import { EditorContent, useEditor } from '@tiptap/react'
import { useEffect } from 'react'

import { buildEditorExtensions } from '@/extensions/editor-extensions'

export type DocChromeAttr = 'header' | 'footer'

function isEmptySingleParagraphBlock(blocks: JSONContent[]): boolean {
  if (blocks.length !== 1) return false
  const n = blocks[0]
  if (n?.type !== 'paragraph') return false
  if (!n.content || n.content.length === 0) return true
  return n.content.every((c) => c.type === 'text' && !(c.text ?? '').trim())
}

/** 与 docx2tiptap 对齐：无实质内容时 attrs 用 null */
export function normalizeChromeBlocks(content: JSONContent[] | undefined): JSONContent[] | null {
  if (!content || content.length === 0) return null
  if (isEmptySingleParagraphBlock(content)) return null
  return content
}

function attrsToDocContent(blocks: JSONContent[] | null | undefined): JSONContent {
  if (blocks && blocks.length > 0 && !isEmptySingleParagraphBlock(blocks)) {
    return { type: 'doc', content: blocks }
  }
  return { type: 'doc', content: [{ type: 'paragraph' }] }
}

export function DocChromeEditor({
  mainEditor,
  docAttr,
  importTick,
  className,
  placeholder,
}: {
  mainEditor: Editor
  docAttr: DocChromeAttr
  importTick: number
  className?: string
  placeholder?: string
}) {
  const chromeEditor = useEditor({
    extensions: buildEditorExtensions({ mode: 'fragment', placeholder }),
    content: { type: 'doc', content: [{ type: 'paragraph' }] },
    editorProps: {
      attributes: {
        class: 'tiptap ProseMirror doc-chrome-inner',
      },
    },
    onUpdate: ({ editor }) => {
      const json = editor.getJSON() as JSONContent
      const normalized = normalizeChromeBlocks(json.content)
      mainEditor.commands.updateAttributes('doc', { [docAttr]: normalized })
    },
  })

  useEffect(() => {
    if (!chromeEditor || !mainEditor) return
    const raw = mainEditor.state.doc.attrs[docAttr] as JSONContent[] | null | undefined
    const doc = attrsToDocContent(raw ?? null)
    chromeEditor.commands.setContent(doc, { emitUpdate: false })
  }, [importTick, mainEditor, chromeEditor, docAttr])

  if (!chromeEditor) {
    return <div className={className ? `${className} min-h-[2.25rem]` : 'min-h-[2.25rem]'} />
  }

  return (
    <div className={className}>
      <EditorContent editor={chromeEditor} />
    </div>
  )
}
