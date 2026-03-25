import type { Editor } from '@tiptap/react'

/** 更新当前选区所在最近一段 paragraph / heading 的节点属性 */
export function updateNearestBlockAttrs(editor: Editor, attrs: Record<string, unknown>): boolean {
  const { state } = editor
  const { $from } = state.selection
  for (let d = $from.depth; d > 0; d--) {
    const node = $from.node(d)
    if (node.type.name === 'paragraph' || node.type.name === 'heading') {
      const pos = $from.before(d)
      const next = { ...node.attrs, ...attrs }
      return editor
        .chain()
        .focus()
        .command(({ tr }) => {
          tr.setNodeMarkup(pos, undefined, next)
          return true
        })
        .run()
    }
  }
  return false
}
