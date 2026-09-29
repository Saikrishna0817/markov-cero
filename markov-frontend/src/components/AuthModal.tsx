import { useState, type FormEvent } from 'react'
import { authConfigured, signIn, signUp, type AuthSession } from '../lib/auth'

type Props = { onClose: () => void; onSignedIn: (session: AuthSession) => void }

export default function AuthModal({ onClose, onSignedIn }: Props) {
  const [mode, setMode] = useState<'signin' | 'signup'>('signin')
  const [email, setEmail] = useState('')
  const [password, setPassword] = useState('')
  const [pending, setPending] = useState(false)
  const [message, setMessage] = useState('')

  const submit = async (event: FormEvent<HTMLFormElement>) => {
    event.preventDefault()
    setMessage('')
    setPending(true)
    try {
      if (mode === 'signin') {
        onSignedIn(await signIn(email.trim(), password))
        onClose()
      } else {
        await signUp(email.trim(), password)
        setMessage('Account created. Check your email to confirm your address, then sign in.')
        setMode('signin')
        setPassword('')
      }
    } catch (error) {
      setMessage(error instanceof Error ? error.message : 'Please try again.')
    } finally {
      setPending(false)
    }
  }

  return <div className="modal-backdrop" onMouseDown={event => { if (event.target === event.currentTarget) onClose() }}>
    <section className="auth-modal" role="dialog" aria-modal="true" aria-labelledby="auth-heading">
      <button className="icon-button close-button" onClick={onClose} aria-label="Close sign in">×</button>
      <div className="eyebrow"><span className="signal-dot" /> SECURE ACCESS / MARKOV-CERO</div>
      <h2 id="auth-heading">{mode === 'signin' ? 'Enter your workspace.' : 'Create your account.'}</h2>
      <p>Access the Markov-cero project workspace with email and password.</p>
      <div className="auth-tabs" role="tablist" aria-label="Authentication mode">
        <button type="button" role="tab" aria-selected={mode === 'signin'} className={mode === 'signin' ? 'selected' : ''} onClick={() => { setMode('signin'); setMessage('') }}>Sign in</button>
        <button type="button" role="tab" aria-selected={mode === 'signup'} className={mode === 'signup' ? 'selected' : ''} onClick={() => { setMode('signup'); setMessage('') }}>Sign up</button>
      </div>
      <form onSubmit={submit}>
        <label>Email address<input type="email" autoComplete="email" required value={email} onChange={event => setEmail(event.target.value)} placeholder="you@example.com" /></label>
        <label>Password<input type="password" autoComplete={mode === 'signin' ? 'current-password' : 'new-password'} required minLength={6} value={password} onChange={event => setPassword(event.target.value)} placeholder="At least 6 characters" /></label>
        <button className="button button-primary auth-submit" type="submit" disabled={pending || !authConfigured}>{pending ? 'Working…' : mode === 'signin' ? 'Sign in →' : 'Create account →'}</button>
      </form>
      {!authConfigured && <p className="form-message" role="status">Authentication is awaiting Supabase configuration.</p>}
      {message && <p className="form-message" role="status">{message}</p>}
      <small>Accounts are handled by Supabase Auth. No solver results are stored here yet.</small>
    </section>
  </div>
}
