import { Node, mergeAttributes } from '@tiptap/core'

export const AbstractBlock = Node.create({
  name: 'abstractBlock',
  group: 'block',
  content: 'inline*',
  addAttributes() {
    return {
      lang: {
        default: 'zh',
        parseHTML: (el) => el.getAttribute('data-lang') ?? 'zh',
        renderHTML: (attrs) => ({ 'data-lang': (attrs.lang as string) || 'zh' }),
      },
    }
  },
  parseHTML() {
    return [{ tag: 'div', attrs: { 'data-type': 'abstractBlock' } }]
  },
  renderHTML({ HTMLAttributes }) {
    return [
      'div',
      mergeAttributes(HTMLAttributes, { 'data-type': 'abstractBlock', class: 'abstract-block' }),
      0,
    ]
  },
})

export const KeywordsBlock = Node.create({
  name: 'keywordsBlock',
  group: 'block',
  content: 'inline*',
  parseHTML() {
    return [{ tag: 'div', attrs: { 'data-type': 'keywordsBlock' } }]
  },
  renderHTML({ HTMLAttributes }) {
    return [
      'div',
      mergeAttributes(HTMLAttributes, { 'data-type': 'keywordsBlock', class: 'keywords-block' }),
      0,
    ]
  },
})

export const ReferenceItem = Node.create({
  name: 'referenceItem',
  group: 'block',
  content: 'inline*',
  parseHTML() {
    return [{ tag: 'div', attrs: { 'data-type': 'referenceItem' } }]
  },
  renderHTML({ HTMLAttributes }) {
    return [
      'div',
      mergeAttributes(HTMLAttributes, { 'data-type': 'referenceItem', class: 'reference-item' }),
      0,
    ]
  },
})

export const Figure = Node.create({
  name: 'figure',
  group: 'block',
  content: 'image+ block*',
  parseHTML() {
    return [{ tag: 'figure', attrs: { 'data-type': 'figure' } }]
  },
  renderHTML({ HTMLAttributes }) {
    return [
      'figure',
      mergeAttributes(HTMLAttributes, { 'data-type': 'figure', class: 'docx-figure' }),
      0,
    ]
  },
})

export const Footnote = Node.create({
  name: 'footnote',
  group: 'block',
  content: 'block+',
  defining: true,
  addAttributes() {
    return {
      id: {
        default: 0,
        parseHTML: (el) => Number(el.getAttribute('data-footnote-id')) || 0,
        renderHTML: (attrs) => ({ 'data-footnote-id': String(attrs.id) }),
      },
    }
  },
  parseHTML() {
    return [{ tag: 'aside', attrs: { 'data-type': 'footnote' } }]
  },
  renderHTML({ HTMLAttributes }) {
    return [
      'aside',
      mergeAttributes(HTMLAttributes, { 'data-type': 'footnote', class: 'docx-footnote' }),
      0,
    ]
  },
})

export const Endnote = Node.create({
  name: 'endnote',
  group: 'block',
  content: 'block+',
  defining: true,
  addAttributes() {
    return {
      id: {
        default: 0,
        parseHTML: (el) => Number(el.getAttribute('data-endnote-id')) || 0,
        renderHTML: (attrs) => ({ 'data-endnote-id': String(attrs.id) }),
      },
    }
  },
  parseHTML() {
    return [{ tag: 'aside', attrs: { 'data-type': 'endnote' } }]
  },
  renderHTML({ HTMLAttributes }) {
    return [
      'aside',
      mergeAttributes(HTMLAttributes, { 'data-type': 'endnote', class: 'docx-endnote' }),
      0,
    ]
  },
})
