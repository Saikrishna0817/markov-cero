import type { ReactNode } from 'react'
import { stages } from '../data'

const docs = 'https://github.com/Saikrishna0817/markov-zip1/tree/main/docs'
const repo = 'https://github.com/Saikrishna0817/markov-zip1'

function Stage({ at, className = '', children }: { at: number; className?: string; children: ReactNode }) {
  const stage = stages[at]
  return <section id={stage.id} className={`journey-section ${className}`} aria-labelledby={`${stage.id}-heading`} data-stage={at}>
    <div className="section-inner"><div className="stage-node-label"><span className="stage-node-dot" aria-hidden="true" /><span>NODE {stage.index} / {stage.label.toUpperCase()}</span></div>{children}</div>
  </section>
}

function Kicker({ at, suffix }: { at: number; suffix?: string }) {
  return <div className="section-kicker"><span className="kicker-index">{stages[at].index}</span><span className="kicker-line" /><span>{stages[at].phase}</span>{suffix && <span className="kicker-suffix">/ {suffix}</span>}</div>
}

function SectionLead({ at, children }: { at: number; children: ReactNode }) {
  return <><Kicker at={at} /><h2 id={`${stages[at].id}-heading`} className="section-title">{stages[at].title}</h2><p className="section-lead">{children}</p></>
}

export function Hero({ onAuth }: { onAuth: () => void }) {
  return <Stage at={0} className="hero-section">
    <div className="hero-content">
      <div className="hero-label"><span className="signal-dot" /> SMART INDIA HACKATHON 2026 <span className="hero-label-divider">/</span> PS 26119</div>
      <div className="hero-rule" />
      <h1 id="entry-heading">Optimization is a journey <em>through decisions.</em></h1>
      <p>An optimization solver research project built to navigate complex decision spaces, reduce unnecessary search, and find high-quality solutions.</p>
      <div className="hero-actions"><a className="button button-primary" href="#problem">Explore the network <span>↘</span></a><button className="button button-secondary" onClick={onAuth}>Enter workspace <span>↗</span></button></div>
      <div className="hero-scroll"><span className="scroll-mark" /> SCROLL TO EXPLORE <span>01 / 10</span></div>
    </div>
    <div className="hero-graph-label"><span className="signal-dot" /> DECISION NETWORK <span>SCROLL PATH 00 → 10</span></div>
  </Stage>
}

export function Problem() {
  return <Stage at={1} className="problem-section">
    <div className="content-column"><SectionLead at={1}>Every constraint changes what is possible. As variables and choices multiply, a solver must distinguish promising states from paths that cannot improve the answer.</SectionLead>
      <div className="problem-types"><div><span>01 / CONTINUOUS</span><strong>Linear programming</strong><p>Choose values inside a feasible region.</p></div><div><span>02 / DISCRETE</span><strong>Mixed-integer programming</strong><p>Choose among combinations of yes/no and whole-number decisions.</p></div><div><span>03 / CURVED</span><strong>Convex quadratic programming</strong><p>Optimize objectives with quadratic structure.</p></div></div>
    </div>
    <div className="visual-caption"><span className="caption-line" /><span>MANY FEASIBLE STATES<br />ONE OBJECTIVE</span></div>
  </Stage>
}

export function Idea() {
  return <Stage at={2} className="idea-section">
    <div className="content-column wide"><SectionLead at={2}>Markov describes movement between states. Cero points toward reduction. Together they define the experience: transition through the network while weak paths disappear.</SectionLead>
      <div className="duality"><div><span>MARKOV</span><strong>State → transition → state</strong><p>Explore the next valid decision.</p></div><div><span>CERO</span><strong>Reduce → eliminate → converge</strong><p>Keep the routes that matter.</p></div><div><span>VISIBLE OUTCOME</span><strong>Each faded node is an eliminated possibility.</strong><p>The network narrows as the search converges.</p></div></div>
    </div>
  </Stage>
}

const pipeline = ['Model', 'Presolve', 'Solve', 'Validate', 'Solution']
export function Solver() {
  return <Stage at={3} className="solver-section">
    <div className="content-column"><SectionLead at={3}>A model enters the engine. Presolve simplifies it. Algorithms search for a result. Independent checks determine what the solver can honestly report.</SectionLead>
      <div className="pipeline" aria-label="Solver pipeline">{pipeline.map((item, i) => <div key={item}><span>{String(i + 1).padStart(2, '0')}</span><strong>{item}</strong>{i < pipeline.length - 1 && <b aria-hidden="true">→</b>}</div>)}</div>
      <div className="status-grid"><div><span className="status-chip implemented">CORE PATHS IMPLEMENTED</span><strong>LP, MILP, convex QP and MIQP — with model-specific verification and limits.</strong></div><div><span className="status-chip experimental">RESTRICTED / RESEARCH</span><strong>NLP, MINLP and GPU paths have narrower scope; GPU speedup is unproven.</strong></div></div>
      <a className="text-link" href="?page=status">See the capability matrix and roadmap ↗</a>
    </div>
  </Stage>
}

const algorithms = [
  { number: '01', name: 'Revised simplex', detail: 'Moves between feasible bases to improve a linear objective.', glyph: 'Ax = b' },
  { number: '02', name: 'Branch-and-cut', detail: 'Splits discrete choices and removes branches using bounds and cuts.', glyph: 'z ≥ z*' },
  { number: '03', name: 'Presolve', detail: 'Reduces a model before the main search begins.', glyph: 'n → n′' },
  { number: '04', name: 'Verification', detail: 'Checks feasibility and solution conditions before reporting results.', glyph: '∥r∥ ≤ ε' },
]
export function Algorithms() {
  return <Stage at={4} className="algorithms-section">
    <div className="content-column wide"><SectionLead at={4}>The network responds to mathematical rules. Each algorithm changes which state is explored next, which branch can be rejected, and when a result is credible.</SectionLead>
      <div className="algorithm-grid">{algorithms.map(item => <article key={item.number} className="algorithm-card"><span>{item.number} / METHOD</span><div className="algorithm-glyph" aria-hidden="true">{item.glyph}</div><h3>{item.name}</h3><p>{item.detail}</p></article>)}</div>
    </div>
  </Stage>
}

const flow = [
  ['01', 'Problem', 'Define the objective.'], ['02', 'Model', 'Encode constraints.'], ['03', 'Presolve', 'Reduce redundancy.'],
  ['04', 'Search', 'Explore states.'], ['05', 'Bound', 'Reject weaker paths.'], ['06', 'Validate', 'Check the result.'], ['07', 'Converge', 'Report the best supported outcome.'],
]
export function Flow() {
  return <Stage at={5} className="flow-section"><div className="content-column wide"><SectionLead at={5}>A solution is the end of a traceable process. Follow the active path from model to validated outcome.</SectionLead>
    <ol className="flow-list">{flow.map(([number, name, description]) => <li key={number}><span className="flow-number">{number}</span><strong>{name}</strong><span>{description}</span><i aria-hidden="true">↗</i></li>)}</ol>
  </div></Stage>
}

const applications = [
  ['ENERGY', 'Balance generation, demand, and operating constraints.'],
  ['LOGISTICS', 'Choose routes and allocations under capacity limits.'],
  ['MANUFACTURING', 'Schedule resources against production requirements.'],
  ['INFRASTRUCTURE', 'Evaluate trade-offs across constrained systems.'],
]
export function Applications() {
  return <Stage at={6} className="applications-section"><div className="content-column wide"><SectionLead at={6}>Different industries produce different models. The common structure is constant: decisions, constraints, and an objective.</SectionLead>
    <div className="applications-grid">{applications.map(([name, description], i) => <article key={name}><span>0{i + 1} / APPLICATION</span><h3>{name}</h3><p>{description}</p><div className="app-node" aria-hidden="true"><span /><span /><span /></div></article>)}</div>
    <p className="application-note">These are example problem domains, not claims of customer deployments.</p>
  </div></Stage>
}

export function Principles() {
  return <Stage at={7} className="principles-section"><div className="content-column wide"><SectionLead at={7}>The project treats transparency and verification as part of the engine, not a layer added after the answer.</SectionLead>
    <div className="principles-grid"><div><span>01 / TRACEABLE</span><strong>Make the search legible.</strong><p>Show where states come from and why a branch remains active.</p></div><div><span>02 / MODULAR</span><strong>Match method to model.</strong><p>Separate model handling, presolve, solving, and validation.</p></div><div><span>03 / HONEST</span><strong>Report the evidence.</strong><p>Distinguish implemented capability from experimental work.</p></div></div>
    <a className="button button-outline" href={docs} target="_blank" rel="noreferrer">Explore documentation <span>↗</span></a>
  </div></Stage>
}

const questions = [
  ['What is Markov-cero?', 'Markov-cero is an optimization solver research project. It explores mathematical models, searches possible decisions, and reports a result with validation details.'],
  ['What do the moving nodes represent?', 'Each highlighted node is a stage of the project story. The connected network moves together as you scroll, showing how a search passes from a broad set of possibilities toward a selected outcome. It is an explanation of the process, not a live trace of a particular solve.'],
  ['Which model files can I try?', 'The workspace accepts MPS and LP text files up to 1 MB. A sample MPS model is included. Running it requires a configured solver API and a signed-in account.'],
  ['Do I need an account to explore the site?', 'No. The journey, explanations, documentation links, and sample model description are public. An account is required only when submitting a model to the solver API.'],
  ['How are accounts and models handled?', 'Supabase Auth handles email accounts. The browser keeps the session in session storage. When the solver API is configured, submitted models are sent to it for a bounded solve; this frontend does not save models or results. Read the data and privacy page for details.'],
  ['Where can I check current capabilities?', 'The repository capability register and evidence index describe implemented solver paths, measured results, and known limits. The visual network here does not claim live benchmark results.'],
] as const

export function GpuResearch() {
  return <Stage at={8} className="gpu-section"><div className="content-column wide"><SectionLead at={8}>PDLP uses sparse matrix-vector products that can run in parallel. The network continues through this research stage; the visual does not imply a speedup.</SectionLead>
    <div className="split-panel"><div><strong>GPU hypothesis</strong><p>Keep sparse operators on device and measure the whole solve, including transfer and verification.</p></div><div><strong>Measured result</strong><p>On the recorded RTX 2050 cases, GPU PDLP was 2.3–8.3× slower end to end than CPU. The QP x-update remains on CPU.</p></div></div>
    <a className="text-link" href="?page=gpu">Read GPU evidence and research limits ↗</a>
  </div></Stage>
}

export function FAQ() {
  return <Stage at={9} className="faq-section"><div className="content-column wide"><SectionLead at={9}>Find the practical details behind the journey, from model formats to account access and project status.</SectionLead>
    <div className="faq-list">{questions.map(([question, answer], index) => <details key={question}><summary><span>{String(index + 1).padStart(2, '0')}</span><strong>{question}</strong><b aria-hidden="true">+</b></summary><p>{answer}</p></details>)}</div>
    <a className="text-link" href="/privacy.html">Read data and privacy details ↗</a>
  </div></Stage>
}

export function Convergence({ onAuth }: { onAuth: () => void }) {
  return <Stage at={10} className="convergence-section"><div className="convergence-content"><Kicker at={10} />
    <h2 id="convergence-heading">Converge on<br /><em>better decisions.</em></h2>
    <p>From a broad search space to a defensible result. Optimization for a Higher Yield.</p>
    <div className="convergence-actions"><button className="button button-primary" onClick={onAuth}>Explore the workspace <span>↗</span></button><a className="button button-secondary" href={docs} target="_blank" rel="noreferrer">View documentation ↗</a></div>
  </div></Stage>
}

export function Footer() {
  return <footer className="footer"><div className="footer-main">
    <div className="footer-about"><div className="footer-brand"><span className="brand-icon" aria-hidden="true" /><div><strong>markov<span>-cero</span></strong><small>OPTIMIZATION FOR A HIGHER YIELD</small></div></div><p>A research solver experience that makes the journey from problem to defensible decision easier to understand.</p><span className="footer-meta">SMART INDIA HACKATHON 2026 · PS 26119</span></div>
    <div className="footer-column"><h3>Explore</h3><a href="#problem">The problem</a><a href="#idea">The idea</a><a href="#solver">The solver</a><a href="#algorithms">Algorithms</a><a href="#applications">Applications</a><a href="?page=algorithms">Algorithms lab</a></div>
    <div className="footer-column"><h3>Evidence</h3><a href="?page=status">Status & roadmap</a><a href="?page=verification">Verification</a><a href="?page=refinery">Refinery case study</a><a href="?page=benchmarks">Benchmarks</a><a href="?page=gpu">GPU & research</a><a href="?page=docs">Docs / API</a></div>
    <div className="footer-column"><h3>Project & access</h3><a href="?page=sovereignty">Sovereignty</a><a href="?page=team">Team</a><a href="?page=security">Security</a><a href="?page=changelog">Changelog</a><a href="?page=glossary">Glossary</a><a href={repo} target="_blank" rel="noreferrer">Source repository ↗</a><a href="/privacy.html">Data & privacy</a><p>Account access uses Supabase Auth. Solver submissions require a live API.</p></div>
  </div><div className="footer-bottom"><span>© {new Date().getFullYear()} Markov-cero</span><span>Made for clearer optimization decisions.</span><a href="#entry">Return to the first node ↑</a></div></footer>
}
