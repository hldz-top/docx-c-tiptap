import { Image } from '@tiptap/extension-image'

/** C 库 image.attrs.layout（如 "float"） */
export const ImageWithLayout = Image.extend({
  addAttributes() {
    return {
      ...this.parent?.(),
      layout: {
        default: null as string | null,
        parseHTML: (el) => el.getAttribute('data-layout'),
        renderHTML: (attrs) => {
          const layout = attrs.layout as string | null | undefined
          if (!layout) return {}
          return { 'data-layout': layout }
        },
      },
    }
  },
})
