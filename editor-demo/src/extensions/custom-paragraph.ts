import { Paragraph } from '@tiptap/extension-paragraph'

import { docxBlockStyleAttributes } from './docx-block-attrs'

/** 与 gezida / docx2tiptap 对齐：class、目录占位、块级缩进与行距 */
export const CustomParagraph = Paragraph.extend({
  addAttributes() {
    const parent = this.parent?.() ?? {}
    return {
      ...parent,
      ...docxBlockStyleAttributes(),
      class: {
        default: null,
        parseHTML: (element) => element.getAttribute('class'),
        renderHTML: (attributes) => {
          if (!attributes.class) return {}
          return { class: attributes.class as string }
        },
      },
      'data-toc-placeholder': {
        default: null,
        parseHTML: (element) => element.getAttribute('data-toc-placeholder'),
        renderHTML: (attributes) => {
          const v = attributes['data-toc-placeholder'] as string | null | undefined
          if (!v) return {}
          return { 'data-toc-placeholder': v }
        },
      },
    }
  },
})
