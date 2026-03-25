import type { JSONContent } from '@tiptap/core'
import { Document } from '@tiptap/extension-document'

function encodeAttr(s: string): string {
  try {
    return encodeURIComponent(s)
  } catch {
    return ''
  }
}

function decodeAttr(s: string): string {
  try {
    return decodeURIComponent(s)
  } catch {
    return s
  }
}

/**
 * 与 docx2tiptap JSON 对齐：pageSetup、warnings、header/footer 块数组
 */
export const DocWithMeta = Document.extend({
  name: 'doc',

  addAttributes() {
    return {
      ...this.parent?.(),
      pageSetup: {
        default: null as Record<string, unknown> | null,
        parseHTML: (element) => {
          const raw = element.getAttribute('data-page-setup')
          if (!raw) return null
          try {
            return JSON.parse(raw) as Record<string, unknown>
          } catch {
            return null
          }
        },
        renderHTML: (attributes) => {
          const v = attributes.pageSetup as Record<string, unknown> | null | undefined
          if (!v || typeof v !== 'object') return {}
          return { 'data-page-setup': JSON.stringify(v) }
        },
      },
      warnings: {
        default: [] as string[],
        parseHTML: (element) => {
          const raw = element.getAttribute('data-warnings')
          if (!raw) return []
          try {
            const p = JSON.parse(raw) as unknown
            return Array.isArray(p) ? (p as string[]) : []
          } catch {
            return []
          }
        },
        renderHTML: (attributes) => {
          const w = attributes.warnings as string[] | undefined
          if (!w || !Array.isArray(w) || w.length === 0) return {}
          return { 'data-warnings': JSON.stringify(w) }
        },
      },
      /** docx2tiptap doc.attrs.header：块级节点 JSON 数组 */
      header: {
        default: null as JSONContent[] | null,
        parseHTML: (element) => {
          const raw = element.getAttribute('data-header-json')
          if (!raw) return null
          try {
            const p = JSON.parse(decodeAttr(raw)) as unknown
            return Array.isArray(p) ? (p as JSONContent[]) : null
          } catch {
            return null
          }
        },
        renderHTML: (attributes) => {
          const v = attributes.header as JSONContent[] | null | undefined
          if (!v || !Array.isArray(v) || v.length === 0) return {}
          return { 'data-header-json': encodeAttr(JSON.stringify(v)) }
        },
      },
      /** docx2tiptap doc.attrs.footer */
      footer: {
        default: null as JSONContent[] | null,
        parseHTML: (element) => {
          const raw = element.getAttribute('data-footer-json')
          if (!raw) return null
          try {
            const p = JSON.parse(decodeAttr(raw)) as unknown
            return Array.isArray(p) ? (p as JSONContent[]) : null
          } catch {
            return null
          }
        },
        renderHTML: (attributes) => {
          const v = attributes.footer as JSONContent[] | null | undefined
          if (!v || !Array.isArray(v) || v.length === 0) return {}
          return { 'data-footer-json': encodeAttr(JSON.stringify(v)) }
        },
      },
    }
  },
})
