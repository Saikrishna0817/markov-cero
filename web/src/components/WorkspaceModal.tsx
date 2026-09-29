import { useState, type ChangeEvent, type FormEvent } from 'react'
import { restoreSession, type AuthSession } from '../lib/auth'

type Result = Record<string, string | number | boolean | null>
const api = import.meta.env.VITE_SOLVER_API_URL?.replace(/\/$/, '')

export default function WorkspaceModal({ session, onClose, onSignOut }: { session: AuthSession; onClose: () => void; onSignOut: () => void }) {
  const [model, setModel] = useState('')
  const [filename, setFilename] = useState('model.mps')
  const [busy, setBusy] = useState(false)
  const [message, setMessage] = useState('')
  const [result, setResult] = useState<Result | null>(null)

  const loadSample = async () => {
    const response = await fetch('/assets/blend.mps')
    setModel(await response.text())
    setFilename('blend.mps')
    setResult(null)
    setMessage('Sample model loaded. Results come only from a live solver run.')
  }
  const loadFile = async (event: ChangeEvent<HTMLInputElement>) => {
    const file = event.target.files?.[0]
    if (!file) return
    if (!/\.(mps|lp)$/i.test(file.name) || file.size > 1_000_000) {
      setMessage('Choose an .mps or .lp file under 1 MB.')
      return
    }
    setFilename(file.name)
    setModel(await file.text())
    setResult(null)
    setMessage(`${file.name} is ready.`)
  }
  const solve = async (event: FormEvent) => {
    event.preventDefault()
    if (!api || !model.trim()) return
    setBusy(true)
    setMessage('')
    setResult(null)
    try {
      const current = await restoreSession()
      if (!current) throw new Error('Your session expired. Sign in again to solve.')
      const response = await fetch(`${api}/solve`, { method: 'POST', headers: { 'Content-Type': 'application/json', Authorization: `Bearer ${current.access_token}` }, body: JSON.stringify({ model, filename }) })
      const data = await response.json()
      if (!response.ok) throw new Error(data.error || 'The solve request failed.')
      setResult(data)
    } catch (error) {
      setMessage(error instanceof Error ? error.message : 'The solve request failed.')
    } finally { setBusy(false) }
  }

  return <div className="modal-backdrop" onMouseDown={event => { if (event.target === event.currentTarget) onClose() }}>
    <section className="workspace-panel" role="dialog" aria-modal="true" aria-labelledby="workspace-heading">
      <button className="icon-button close-button" onClick={onClose} aria-label="Close workspace">×</button>
      <div className="eyebrow"><span className="signal-dot" /> AUTHENTICATED WORKSPACE</div>
      <h2 id="workspace-heading">Explore the solver.</h2>
      <p>Signed in as <strong>{session.user.email}</strong>. Submit an MPS or LP model when the solver API is available.</p>
      <div className="workspace-columns">
        <form onSubmit={solve}>
          <div className="workspace-field-head"><label htmlFor="model-input">MODEL INPUT / {filename}</label><button type="button" onClick={loadSample}>Load sample ↗</button></div>
          <textarea id="model-input" spellCheck={false} value={model} onChange={event => { setModel(event.target.value); setResult(null) }} placeholder="Paste an MPS or LP model here, or choose a file." />
          <div className="workspace-controls"><label className="button button-secondary file-button">Choose file<input type="file" accept=".mps,.lp,text/plain" onChange={loadFile} /></label><button className="button button-primary" type="submit" disabled={!api || !model.trim() || busy}>{busy ? 'Solving…' : 'Run solver →'}</button></div>
          {!api && <p className="workspace-warning">Solver API deployment is pending. You can inspect the sample model and the source now.</p>}
        </form>
        <div className="workspace-output"><span>RESULT / VERIFIED OUTPUT</span>{result ? <dl>{Object.entries(result).map(([key, value]) => <div key={key}><dt>{key.replaceAll('_', ' ')}</dt><dd>{String(value)}</dd></div>)}</dl> : <div className="empty-result"><span className="empty-node" /><strong>No result yet.</strong><p>The solver returns status, objective, and verification details here.</p></div>}</div>
      </div>
      {message && <p className="form-message" role="status">{message}</p>}
      <div className="workspace-footer"><a href="https://github.com/Saikrishna0817/markov-zip1/tree/main/docs" target="_blank" rel="noreferrer">Documentation ↗</a><button onClick={onSignOut}>Sign out</button></div>
    </section>
  </div>
}
