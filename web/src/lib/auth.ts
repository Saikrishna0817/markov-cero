export type AuthSession = { access_token: string; refresh_token: string; expires_at: number; user: { id: string; email?: string } }

const url = import.meta.env.VITE_SUPABASE_URL?.replace(/\/$/, '')
const key = import.meta.env.VITE_SUPABASE_PUBLISHABLE_KEY
const storageKey = 'markov-cero-auth'

export const authConfigured = Boolean(url && key)

async function request(path: string, body: object, accessToken?: string) {
  if (!authConfigured) throw new Error('Authentication is not configured for this deployment.')
  const response = await fetch(`${url}/auth/v1/${path}`, {
    method: 'POST',
    headers: {
      apikey: key,
      'Content-Type': 'application/json',
      ...(accessToken ? { Authorization: `Bearer ${accessToken}` } : {}),
    },
    body: JSON.stringify(body),
  })
  const result = await response.json().catch(() => ({}))
  if (!response.ok) throw new Error(result.msg || result.error_description || result.message || 'Authentication request failed.')
  return result
}

export async function signUp(email: string, password: string) {
  return request(`signup?redirect_to=${encodeURIComponent(window.location.origin)}`, { email, password })
}

function saveSession(result: { access_token: string; refresh_token: string; expires_in: number; user: AuthSession['user'] }): AuthSession {
  const session = { access_token: result.access_token, refresh_token: result.refresh_token, expires_at: Math.floor(Date.now() / 1000) + result.expires_in, user: result.user }
  sessionStorage.setItem(storageKey, JSON.stringify(session))
  return session
}

export async function signIn(email: string, password: string): Promise<AuthSession> {
  return saveSession(await request('token?grant_type=password', { email, password }))
}

export function getSession(): AuthSession | null {
  try {
    const raw = sessionStorage.getItem(storageKey)
    if (!raw) return null
    const session = JSON.parse(raw) as AuthSession
    return session.expires_at > Math.floor(Date.now() / 1000) ? session : null
  } catch { return null }
}

async function sessionFromConfirmationLink(): Promise<AuthSession | null> {
  const fragment = new URLSearchParams(window.location.hash.slice(1))
  const accessToken = fragment.get('access_token')
  const refreshToken = fragment.get('refresh_token')
  const expiresIn = Number(fragment.get('expires_in'))
  if (!accessToken || !refreshToken || !Number.isFinite(expiresIn) || expiresIn <= 0 || !authConfigured) return null
  window.history.replaceState(null, '', window.location.pathname + window.location.search)
  const response = await fetch(`${url}/auth/v1/user`, {
    headers: { apikey: key, Authorization: `Bearer ${accessToken}` },
  })
  if (!response.ok) throw new Error('Confirmation link could not be verified.')
  const user = await response.json() as AuthSession['user']
  if (!user.id) throw new Error('Confirmation link did not include an account.')
  return saveSession({ access_token: accessToken, refresh_token: refreshToken, expires_in: expiresIn, user })
}

export async function restoreSession(): Promise<AuthSession | null> {
  try {
    const confirmed = await sessionFromConfirmationLink()
    if (confirmed) return confirmed
    const raw = sessionStorage.getItem(storageKey)
    if (!raw) return null
    const session = JSON.parse(raw) as AuthSession
    if (session.expires_at > Math.floor(Date.now() / 1000) + 30) return session
    if (!session.refresh_token) return null
    return saveSession(await request('token?grant_type=refresh_token', { refresh_token: session.refresh_token }))
  } catch {
    sessionStorage.removeItem(storageKey)
    return null
  }
}

export async function signOut() {
  const session = getSession()
  sessionStorage.removeItem(storageKey)
  if (session && authConfigured) {
    try { await request('logout', {}, session.access_token) } catch { /* local session is cleared */ }
  }
}
