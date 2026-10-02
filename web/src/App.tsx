import { useEffect, useState } from 'react'
import Header from './components/Header'
import ProgressRail from './components/ProgressRail'
import NetworkJourney from './components/NetworkJourney'
import AuthModal from './components/AuthModal'
import WorkspaceModal from './components/WorkspaceModal'
import { Hero, Problem, Idea, Solver, Algorithms, Flow, Applications, Principles, GpuResearch, FAQ, Convergence, Footer } from './components/Sections'
import { stages } from './data'
import { getSession, restoreSession, signOut, type AuthSession } from './lib/auth'
import Pages from './components/Pages'

function useJourney() {
  const [progress, setProgress] = useState(0)
  useEffect(() => {
    let frame = 0
    const update = () => {
      frame = 0
      const position = window.scrollY + window.innerHeight * 0.52
      const tops = stages.map(stage => document.getElementById(stage.id)?.offsetTop ?? 0)
      let value = 0
      for (let i = 0; i < tops.length - 1; i++) {
        if (position >= tops[i]) value = i + Math.min(1, Math.max(0, (position - tops[i]) / Math.max(1, tops[i + 1] - tops[i])))
      }
      if (position >= tops[tops.length - 1]) value = stages.length - 1
      setProgress(value)
    }
    const schedule = () => { if (!frame) frame = requestAnimationFrame(update) }
    update()
    window.addEventListener('scroll', schedule, { passive: true })
    window.addEventListener('resize', schedule)
    return () => { window.removeEventListener('scroll', schedule); window.removeEventListener('resize', schedule); cancelAnimationFrame(frame) }
  }, [])
  return progress
}

function JourneyApp() {
  const progress = useJourney()
  const active = Math.max(0, Math.min(stages.length - 1, Math.floor(progress + 0.12)))
  const [authOpen, setAuthOpen] = useState(false)
  const [workspaceOpen, setWorkspaceOpen] = useState(false)
  const [session, setSession] = useState<AuthSession | null>(() => getSession())
  const onAuth = () => session ? setWorkspaceOpen(true) : setAuthOpen(true)
  const onSignedIn = (value: AuthSession) => { setSession(value); setWorkspaceOpen(true) }
  const onSignOut = async () => { await signOut(); setSession(null); setWorkspaceOpen(false) }

  useEffect(() => { void restoreSession().then(setSession) }, [])

  useEffect(() => {
    const id = decodeURIComponent(window.location.hash.slice(1))
    if (id) document.getElementById(id)?.scrollIntoView({ behavior: 'instant' as ScrollBehavior })
  }, [])

  useEffect(() => {
    document.body.style.overflow = authOpen || workspaceOpen ? 'hidden' : ''
    return () => { document.body.style.overflow = '' }
  }, [authOpen, workspaceOpen])

  return <>
    <a className="skip-link" href="#problem">Skip to content</a>
    <div className="background-grid" aria-hidden="true" />
    <NetworkJourney progress={progress} />
    <Header active={active} session={session} onAuth={onAuth} onSignOut={onSignOut} />
    <ProgressRail active={active} progress={progress} />
    <main>
      <Hero onAuth={onAuth} />
      <Problem />
      <Idea />
      <Solver />
      <Algorithms />
      <Flow />
      <Applications />
      <Principles />
      <GpuResearch />
      <FAQ />
      <Convergence onAuth={onAuth} />
    </main>
    <Footer />
    {authOpen && <AuthModal onClose={() => setAuthOpen(false)} onSignedIn={onSignedIn} />}
    {workspaceOpen && session && <WorkspaceModal session={session} onClose={() => setWorkspaceOpen(false)} onSignOut={onSignOut} />}
  </>
}

export default function App() {
  const page = new URLSearchParams(window.location.search).get('page')
  return page ? <Pages page={page} /> : <JourneyApp />
}
