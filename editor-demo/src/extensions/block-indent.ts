import type { Command, Extensions } from '@tiptap/core'
import { Extension, isList } from '@tiptap/core'
import { AllSelection, TextSelection } from '@tiptap/pm/state'

export type BlockIndentOptions = {
  types: string[]
  minLevel: number
  maxLevel: number
  stepEm: number
}

declare module '@tiptap/core' {
  interface Commands<ReturnType> {
    blockIndent: {
      increaseBlockIndent: () => ReturnType
      decreaseBlockIndent: () => ReturnType
    }
  }
}

function updateBlockIndent(
  tr: import('@tiptap/pm/state').Transaction,
  delta: number,
  types: string[],
  minLevel: number,
  maxLevel: number,
  extensions: Extensions,
): import('@tiptap/pm/state').Transaction {
  const { doc, selection } = tr
  if (!selection || !(selection instanceof TextSelection || selection instanceof AllSelection)) {
    return tr
  }
  const { from, to } = selection
  doc.nodesBetween(from, to, (node, pos) => {
    if (types.includes(node.type.name)) {
      const cur = (node.attrs.indent as number | undefined) ?? 0
      const next = Math.min(maxLevel, Math.max(minLevel, cur + delta))
      tr = tr.setNodeMarkup(pos, node.type, { ...node.attrs, indent: next }, node.marks)
      return false
    }
    if (isList(node.type.name, extensions)) return false
    return true
  })
  return tr
}

/**
 * 块级左缩进（整段 margin-inline-start），与首行缩进 textIndent 分离
 */
export const BlockIndent = Extension.create<BlockIndentOptions>({
  name: 'blockIndent',

  addOptions() {
    return {
      types: ['paragraph', 'heading'],
      minLevel: 0,
      maxLevel: 8,
      stepEm: 2,
    }
  },

  addGlobalAttributes() {
    return [
      {
        types: this.options.types,
        attributes: {
          indent: {
            default: 0,
            parseHTML: (element) => {
              const v = element.getAttribute('data-block-indent')
              return v ? parseInt(v, 10) : 0
            },
            renderHTML: (attributes) => {
              const n = (attributes.indent as number) ?? 0
              if (!n || n <= 0) return {}
              const em = this.options.stepEm * n
              return {
                'data-block-indent': String(n),
                style: `margin-inline-start: ${em}em`,
              }
            },
          },
        },
      },
    ]
  },

  addCommands() {
    return {
      increaseBlockIndent:
        (): Command =>
        ({ tr, dispatch, editor }) => {
          const next = updateBlockIndent(
            tr,
            1,
            this.options.types,
            this.options.minLevel,
            this.options.maxLevel,
            editor.extensionManager.extensions,
          )
          if (dispatch) dispatch(next)
          return true
        },
      decreaseBlockIndent:
        (): Command =>
        ({ tr, dispatch, editor }) => {
          const next = updateBlockIndent(
            tr,
            -1,
            this.options.types,
            this.options.minLevel,
            this.options.maxLevel,
            editor.extensionManager.extensions,
          )
          if (dispatch) dispatch(next)
          return true
        },
    }
  },
})
