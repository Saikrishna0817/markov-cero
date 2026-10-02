import { useState } from 'react'
import { stages } from '../data'
import type { AuthSession } from '../lib/auth'
import PageMenu from './PageMenu'

type Props = { active: number; session: AuthSession | null; onAuth: () => void; onSignOut: () => void }

const links = [
  ['Explore', 'problem'], ['Solver', 'solver'], ['Research', 'gpu'],
] as const

export default function Header({ active, session, onAuth, onSignOut }: Props) {
  const [open, setOpen] = useState(false)
  const go = (id: string) => {
    document.getElementById(id)?.scrollIntoView({ behavior: 'smooth' })
    setOpen(false)
  }
  return <header className={`site-header ${active > 0 ? 'scrolled' : ''}`}>
    <a className="brand" href="#entry" aria-label="Markov-cero, back to top" onClick={() => setOpen(false)}>
      <span className="brand-icon" aria-hidden="true" />
      <span>markov<span className="brand-hyphen">-</span><b>cero</b></span>
    </a>
    <nav className={`primary-nav ${open ? 'open' : ''}`} aria-label="Main navigation">
      {links.map(([label, id]) => <button key={id} className={stages[active]?.id === id ? 'nav-current' : ''} onClick={() => go(id)}>{label}</button>)}
      <a href="?page=verification">Evidence</a>
      <PageMenu />
      <div className="mobile-action"><button className="button button-primary" onClick={() => { setOpen(false); onAuth() }}>{session ? 'Workspace' : 'Launch console'} →</button></div>
    </nav>
    <div className="header-actions">
      {session && <button className="signout-link" onClick={onSignOut}>Sign out</button>}
      <button className="button button-primary header-cta" onClick={onAuth}>{session ? 'Workspace' : 'Launch console'} <span>↗</span></button>
      <button className="menu-toggle" aria-label={open ? 'Close menu' : 'Open menu'} aria-expanded={open} onClick={() => setOpen(value => !value)}>{open ? '×' : '☰'}</button>
    </div>
  </header>
}
