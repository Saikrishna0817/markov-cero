import { useEffect, useRef, useState } from 'react'

const groups = [
  { title: 'Product', pages: [['algorithms', 'Algorithms lab'], ['docs', 'Docs and API']] },
  { title: 'Evidence', pages: [['verification', 'Verification'], ['benchmarks', 'Benchmarks'], ['refinery', 'Refinery case study']] },
  { title: 'Research', pages: [['gpu', 'GPU and research']] },
  { title: 'Project', pages: [['status', 'Status and roadmap'], ['sovereignty', 'Sovereignty and source'], ['team', 'Team'], ['security', 'Security'], ['changelog', 'Changelog'], ['glossary', 'Glossary']] },
] as const

export default function PageMenu() {
  const [open, setOpen] = useState(false)
  const root = useRef<HTMLDivElement>(null)

  useEffect(() => {
    if (!open) return
    const closeOnOutside = (event: PointerEvent) => {
      if (!root.current?.contains(event.target as Node)) setOpen(false)
    }
    const closeOnEscape = (event: KeyboardEvent) => {
      if (event.key === 'Escape') {
        setOpen(false)
        root.current?.querySelector('button')?.focus()
      }
    }
    document.addEventListener('pointerdown', closeOnOutside)
    document.addEventListener('keydown', closeOnEscape)
    return () => {
      document.removeEventListener('pointerdown', closeOnOutside)
      document.removeEventListener('keydown', closeOnEscape)
    }
  }, [open])

  return <div className="pages-menu" ref={root}>
    <button type="button" className={open ? 'nav-current' : ''} aria-expanded={open} aria-controls="page-menu-panel" onClick={() => setOpen(value => !value)}>Pages <span aria-hidden="true">⌄</span></button>
    {open && <div className="pages-menu-panel" id="page-menu-panel">
      {groups.map(group => <div className="pages-menu-group" key={group.title}>
        <strong>{group.title}</strong>
        {group.pages.map(([id, label]) => <a href={`?page=${id}`} key={id} onClick={() => setOpen(false)}>{label}</a>)}
      </div>)}
    </div>}
  </div>
}
