/**
 * 与 docx2tiptap 块级 attrs 对齐（paragraph / heading 共用）
 */
export function docxBlockStyleAttributes() {
  return {
    firstLineIndent: {
      default: null as string | null,
      parseHTML: (element: HTMLElement) =>
        element.getAttribute('data-first-line-indent') || element.style?.textIndent || null,
      renderHTML: (attrs: Record<string, unknown>) => {
        const v = attrs.firstLineIndent as string | null | undefined
        if (!v) return {}
        return { style: `text-indent: ${v}`, 'data-first-line-indent': v }
      },
    },
    marginTop: {
      default: null as string | null,
      parseHTML: (element: HTMLElement) => element.style?.marginTop || null,
      renderHTML: (attrs: Record<string, unknown>) => {
        const v = attrs.marginTop as string | null | undefined
        if (!v) return {}
        return { style: `margin-top: ${v}` }
      },
    },
    marginBottom: {
      default: null as string | null,
      parseHTML: (element: HTMLElement) => element.style?.marginBottom || null,
      renderHTML: (attrs: Record<string, unknown>) => {
        const v = attrs.marginBottom as string | null | undefined
        if (!v) return {}
        return { style: `margin-bottom: ${v}` }
      },
    },
    lineHeight: {
      default: null as string | null,
      parseHTML: (element: HTMLElement) => element.style?.lineHeight || null,
      renderHTML: (attrs: Record<string, unknown>) => {
        const v = attrs.lineHeight as string | null | undefined
        if (!v) return {}
        return { style: `line-height: ${v}` }
      },
    },
  }
}
