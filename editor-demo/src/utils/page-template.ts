/** 无真实分页时占位；日后可注入真实页码 */
export function applyPageTemplate(html: string | null | undefined, page = 1, total = 1): string {
  if (html == null || html === '') return ''
  return html.replaceAll('{page}', String(page)).replaceAll('{total}', String(total))
}
