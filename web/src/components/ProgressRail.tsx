import { stages } from '../data'

export default function ProgressRail({ active, progress }: { active: number; progress: number }) {
  return <nav className="progress-rail" aria-label="Journey stages">
    <span className="rail-top">SEARCH</span>
    <div className="rail-track"><span className="rail-fill" style={{ height: `${Math.max(0, Math.min(100, (progress / (stages.length - 1)) * 100))}%` }} /></div>
    <div className="rail-nodes">
      {stages.map((stage, i) => <a key={stage.id} href={`#${stage.id}`} className={active === i ? 'active' : i < active ? 'visited' : ''} aria-label={`${stage.index}: ${stage.label}`} title={stage.label}><span className="rail-circle" /><span className="rail-label">{stage.index}</span></a>)}
    </div>
    <span className="rail-bottom">CONVERGE</span>
  </nav>
}
