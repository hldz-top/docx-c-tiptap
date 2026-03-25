import DOMPurify from 'dompurify'

const CONFIG = {
  ALLOWED_TAGS: [
    'p',
    'br',
    'span',
    'b',
    'strong',
    'i',
    'em',
    'u',
    'small',
    'sub',
    'sup',
    'div',
  ],
  ALLOWED_ATTR: ['class', 'style'],
}

export function sanitizeDocChromeHtml(dirty: string): string {
  return DOMPurify.sanitize(dirty, CONFIG)
}
