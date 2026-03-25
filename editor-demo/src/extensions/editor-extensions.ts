import type { AnyExtension } from '@tiptap/core'
import Highlight from '@tiptap/extension-highlight'
import Subscript from '@tiptap/extension-subscript'
import Superscript from '@tiptap/extension-superscript'
import { Table, TableCell, TableHeader, TableRow } from '@tiptap/extension-table'
import TextAlign from '@tiptap/extension-text-align'
import { TextStyleKit } from '@tiptap/extension-text-style/text-style-kit'
import { Placeholder } from '@tiptap/extensions'
import StarterKit from '@tiptap/starter-kit'

import { BlockIndent } from './block-indent'
import { CustomHeading } from './custom-heading'
import { CustomParagraph } from './custom-paragraph'
import { DocWithMeta } from './doc-with-meta'
import { ImageWithLayout } from './image-layout'
import {
  AbstractBlock,
  Endnote,
  Figure,
  Footnote,
  KeywordsBlock,
  ReferenceItem,
} from './thesis-nodes'

const fontFamilyList = [
  { label: '宋体', value: '宋体' },
  { label: '黑体', value: '黑体' },
  { label: '仿宋', value: '仿宋' },
  { label: '楷体', value: '楷体' },
  { label: 'Times New Roman', value: 'Times New Roman' },
  { label: 'Arial', value: 'Arial' },
  { label: 'Calibri', value: 'Calibri' },
  { label: 'Cambria', value: 'Cambria' },
]

export const FONT_SIZE_OPTIONS: { label: string; value: string }[] = [
  { label: '默认', value: '' },
  { label: '初号', value: '42pt' },
  { label: '小初', value: '36pt' },
  { label: '一号', value: '26pt' },
  { label: '小一', value: '24pt' },
  { label: '二号', value: '22pt' },
  { label: '小二', value: '18pt' },
  { label: '三号', value: '16pt' },
  { label: '小三', value: '15pt' },
  { label: '四号', value: '14pt' },
  { label: '小四', value: '12pt' },
  { label: '五号', value: '10.5pt' },
  { label: '小五', value: '9pt' },
  { label: '8pt', value: '8pt' },
  { label: '9pt', value: '9pt' },
  { label: '10pt', value: '10pt' },
  { label: '11pt', value: '11pt' },
  { label: '14pt', value: '14pt' },
  { label: '16pt', value: '16pt' },
  { label: '18pt', value: '18pt' },
  { label: '20pt', value: '20pt' },
  { label: '22pt', value: '22pt' },
  { label: '24pt', value: '24pt' },
  { label: '28pt', value: '28pt' },
  { label: '36pt', value: '36pt' },
]

export { fontFamilyList }

const starterLink = {
  openOnClick: true,
  autolink: true,
  defaultProtocol: 'https' as const,
}

export type EditorExtensionMode = 'main' | 'fragment'

export function buildEditorExtensions(options: {
  mode: EditorExtensionMode
  placeholder?: string
}): AnyExtension[] {
  const { mode, placeholder } = options
  const defaultPlaceholder =
    mode === 'main'
      ? '在此编辑，或使用「导入 JSON」加载 docx2tiptap 输出…'
      : '…'
  const ph = placeholder ?? defaultPlaceholder

  const starterConfig =
    mode === 'main'
      ? {
          document: false as const,
          paragraph: false as const,
          heading: false as const,
          codeBlock: false as const,
          link: starterLink,
        }
      : {
          paragraph: false as const,
          heading: false as const,
          codeBlock: false as const,
          link: starterLink,
        }

  return [
    ...(mode === 'main' ? [DocWithMeta] : []),
    TextStyleKit.configure({
      textStyle: { mergeNestedSpanStyles: true },
      color: { types: ['textStyle'] },
      fontFamily: { types: ['textStyle'] },
      fontSize: { types: ['textStyle'] },
      /** 行距由 docx 块级 attrs.lineHeight 提供，避免与段落 line-height 重复注册为 mark */
      lineHeight: false,
      backgroundColor: false,
    }),
    Highlight.configure({ multicolor: true }),
    Subscript,
    Superscript,
    BlockIndent.configure({
      types: ['paragraph', 'heading'],
    }),
    CustomParagraph,
    CustomHeading,
    StarterKit.configure(starterConfig),
    TextAlign.configure({
      types: ['heading', 'paragraph'],
    }),
    Table.configure({
      resizable: false,
      HTMLAttributes: { class: 'docx-table' },
    }),
    TableRow,
    TableHeader,
    TableCell.extend({
      addAttributes() {
        return {
          ...this.parent?.(),
          colspan: {
            default: 1,
            parseHTML: (el) => {
              const c = el.getAttribute('colspan')
              return c ? parseInt(c, 10) : 1
            },
          },
          rowspan: {
            default: 1,
            parseHTML: (el) => {
              const c = el.getAttribute('rowspan')
              return c ? parseInt(c, 10) : 1
            },
          },
        }
      },
    }),
    ImageWithLayout.configure({
      inline: false,
      allowBase64: true,
      HTMLAttributes: { class: 'docx-image' },
    }),
    AbstractBlock,
    KeywordsBlock,
    ReferenceItem,
    Figure,
    Footnote,
    Endnote,
    Placeholder.configure({
      placeholder: ph,
    }),
  ]
}

export function buildExtensions() {
  return buildEditorExtensions({ mode: 'main' })
}
