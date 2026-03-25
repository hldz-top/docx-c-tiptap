import type { JSONContent } from '@tiptap/core'

const INLINE_ONLY_PARENTS = new Set(['paragraph', 'heading', 'blockquote'])

function isBlockFootnoteLike(node: JSONContent): boolean {
  return node.type === 'footnote' || node.type === 'endnote'
}

/**
 * 将 paragraph / heading / blockquote 内非法嵌套的 footnote、endnote 提升到与宿主块并列（满足 PM schema）。
 */
function mapContent(nodes: JSONContent[]): JSONContent[] {
  return nodes.flatMap((c) => expandWalk(c))
}

function expandWalk(n: JSONContent): JSONContent[] {
  if (!n.content || n.content.length === 0) {
    return [n]
  }
  const inner = mapContent(n.content)
  if (!n.type || !INLINE_ONLY_PARENTS.has(n.type)) {
    return [{ ...n, content: inner }]
  }
  const inls: JSONContent[] = []
  const lifts: JSONContent[] = []
  for (const x of inner) {
    if (isBlockFootnoteLike(x)) lifts.push(x)
    else inls.push(x)
  }
  const next: JSONContent = { ...n }
  if (inls.length) next.content = inls
  else delete next.content
  return [next, ...lifts]
}

export function liftBlockNotesFromInlines(doc: JSONContent): JSONContent {
  const out = expandWalk(doc)
  return out.length === 1 ? out[0] : doc
}
