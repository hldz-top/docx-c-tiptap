import { Heading } from '@tiptap/extension-heading'

import { docxBlockStyleAttributes } from './docx-block-attrs'

/** 与 docx2tiptap 对齐：标题块上的首行缩进、行距、段前后（与 CustomParagraph 一致） */
export const CustomHeading = Heading.extend({
  addAttributes() {
    return {
      ...this.parent?.(),
      ...docxBlockStyleAttributes(),
    }
  },
})
