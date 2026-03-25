export function ToolbarBtn({
  label,
  onClick,
  active,
  title,
}: {
  label: string
  onClick: () => void
  active?: boolean
  title?: string
}) {
  return (
    <button
      type="button"
      title={title}
      onClick={onClick}
      className={`rounded px-2 py-1 text-sm ${active ? 'bg-zinc-200 font-medium' : 'hover:bg-zinc-100'}`}
    >
      {label}
    </button>
  )
}
