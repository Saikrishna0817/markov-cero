import { useEffect, useState } from 'react'

const repo = 'https://github.com/Saikrishna0817/markov-zip1'
const source = (path: string) => repo + '/blob/main/' + path

const pageLinks = [
  ['status', 'Status & Roadmap'], ['verification', 'Verification'], ['refinery', 'Refinery Case Study'],
  ['sovereignty', 'Sovereignty'], ['benchmarks', 'Benchmarks'], ['algorithms', 'Algorithms Lab'],
  ['gpu', 'GPU & Research'], ['docs', 'Docs / API'], ['team', 'Team'],
  ['security', 'Security'], ['changelog', 'Changelog'], ['glossary', 'Glossary'],
] as const

const core = [
  ['MPS parsing / model', 'Implemented', 'Free-format MPS and sparse canonical model. Parser regression corrected; old benchmark data may require regeneration.'],
  ['LP: primal / dual simplex / IPM / PDLP', 'Implemented', 'Four CPU paths with different proof and fallback conditions.'],
  ['MILP branch-and-cut', 'Implemented', 'Cuts, heuristics and parallel tree search; hard-instance and resource-limit coverage remain open.'],
  ['Convex QP / MIQP', 'Implemented', 'ADMM/KKT and MIQP tree paths with model-specific verification.'],
  ['Independent checking', 'Implemented', 'Original-space primal checks, canonical LP witness checks, QP KKT residuals, and numerical tree replay where supported.'],
  ['NLP / MINLP', 'Experimental', 'Restricted convex or quadratic subsets; local NLP optimum is not a global proof.'],
  ['GPU PDLP', 'Experimental', 'RTX 2050 correctness observed; 2.3–8.3× slower end to end than CPU in repeated measured cases.'],
  ['GPU QP / ML branching', 'Experimental', 'QP device work covers only P·x; ML branch model is not promoted.'],
  ['Versioned C ABI / language bindings', 'Planned', 'CLI, C++ and Python exist; a versioned C ABI and additional bindings do not.'],
] as const

const history = [
  ['M0', 'Foundation and governance'], ['M1', 'MPS parser and model'], ['M2', 'Canonicalization and dense oracle'],
  ['M3', 'Primal revised simplex'], ['M4', 'Dual warm simplex'], ['M5', 'Sparse basis algebra'],
] as const
const roadmap = [
  ['M6', 'Warm starts and factorization reuse'], ['M7', 'PDLP, IPM and GPU decisions'],
  ['M8', 'Refinery conservation and uncertainty'], ['M9', 'Pooling and nonlinear claim boundaries'],
  ['M10', 'Multi-period decomposition'], ['M11', 'Bound propagation and valid big-M'],
  ['M12', 'Explicit cut proof and limits'], ['M13', 'Numerical conditioning and basis stability'],
  ['M14', 'Cost models and measured performance'],
] as const

function Source({ path, children }: { path: string; children?: string }) {
  return <a href={source(path)} target="_blank" rel="noreferrer">{children ?? path} ↗</a>
}

function PageShell({ title, eyebrow, lead, children }: { title: string; eyebrow: string; lead: string; children: React.ReactNode }) {
  useEffect(()=>{document.title=`${title} — markov-cero`},[title])
  return <><header className="page-header"><a className="page-brand" href="/"><span className="brand-icon" aria-hidden="true" />markov<span>-cero</span></a><nav aria-label="Page navigation"><a href="/">Explore</a><a href="?page=status">Status</a><a href="?page=verification">Verification</a><a href="?page=refinery">Case study</a><a href="?page=docs">Docs</a></nav></header>
    <main className="page-main"><div className="page-container"><div className="page-intro"><span className="page-eyebrow">{eyebrow}</span><h1>{title}</h1><p>{lead}</p></div>{children}
      <nav className="page-directory" aria-label="More pages"><h2>Continue exploring</h2><div>{pageLinks.map(([id, name]) => <a key={id} href={`?page=${id}`}>{name}<span>↗</span></a>)}</div></nav>
    </div></main><footer className="page-footer"><span>markov-cero · v0.5.2 research prototype</span><a href="/">Return to the journey ↑</a><a href="/privacy.html">Data & privacy</a></footer></>
}

function StatusPage() {
  return <PageShell eyebrow="01 / RELEASE REGISTER" title="Status & roadmap" lead="What is present in the code, what has passed named checks, and what remains an open research or release claim. Current documented release: v0.5.2.">
    <section className="page-section"><div className="page-section-head"><h2>Capability matrix</h2><p>“Implemented” means code exists. It does not imply broad solver coverage or a performance advantage.</p></div><div className="matrix-wrap"><table><thead><tr><th>Capability</th><th>State</th><th>Boundary</th></tr></thead><tbody>{core.map(([name,status,note])=><tr key={name}><th>{name}</th><td><span className={`matrix-pill ${status.toLowerCase()}`}>{status}</span></td><td>{note}</td></tr>)}</tbody></table></div><p className="source-note">Source of truth: <Source path="docs/project/STATUS.md" />. Hardware verified means device and binary provenance are recorded.</p></section>
    <section className="page-section"><div className="page-section-head"><h2>Milestone lineage</h2><p>M0–M5 are dated development milestones. M6–M14 are open work themes, not completed release milestones.</p></div><div className="milestone-grid">{history.map(([n,name])=><div key={n}><span>{n} / HISTORY</span><strong>{name}</strong></div>)}{roadmap.map(([n,name])=><div key={n}><span>{n} / ROADMAP TOPIC</span><strong>{name}</strong></div>)}</div><p className="source-note"><Source path="CHANGELOG.md" /> · <Source path="docs/project/STATUS.md" /></p></section>
    <section className="page-section callout"><h2>Release gates still open</h2><p>Independent source-trace review, full upstream benchmark coverage, aligned comparison timing, solve-wide resource accounting, and a validated refinery pilot remain open. These limits are part of the published status, not hidden behind the animation.</p></section>
  </PageShell>
}

function VerificationPage() {
  return <PageShell eyebrow="02 / TRUST THE RESULT" title="Verification" lead="A solver result is a claim backed by a witness, residual checks, and a status that reflects what the run can actually establish.">
    <section className="page-section"><h2>Independent checks</h2><div className="page-card-grid"><article><span>01 / MODEL SPACE</span><h3>Primal feasibility</h3><p>Check the original model’s rows, variable bounds and integrality after transforms are reversed.</p></article><article><span>02 / LP</span><h3>Canonical witness</h3><p>Evaluate primal residual ‖Ax−b‖∞, dual residual ‖Aᵀy+s−c‖∞, and complementarity xᵀs.</p></article><article><span>03 / QP</span><h3>KKT conditions</h3><p>Check convexity, primal and dual residuals, and gap against the QP verifier’s thresholds.</p></article><article><span>04 / MIXED INTEGER</span><h3>Tree replay</h3><p>Linear MILP and convex MIQP may export replayable numerical trees. Exhausted proof budgets remain unverified.</p></article></div><p className="source-note"><Source path="docs/project/STATUS.md" /> · <Source path="docs/project/STATUS.md" /></p></section>
    <section className="page-section"><h2>Tolerance policy</h2><div className="split-panel"><div><strong>Witness first</strong><p>Scale-aware feasibility, dual feasibility and gap conditions determine the status. The default PDLP relative KKT tolerance is 1e−4; a local NLP candidate’s 1e−6 KKT check does not prove a global optimum.</p></div><div><strong>Failure stays visible</strong><p>Time, iteration, node or proof limits are reported as limits. A feasible point without a finite dual bound remains Feasible; it is not silently called Optimal.</p></div></div></section>
    <section className="page-section"><h2>Agreement and disagreement</h2><div className="metric-row"><div><strong>20 / 20</strong><span>Preregistered Netlib/MIPLIB cases agreed with HiGHS at one and four threads</span></div><div><strong>0</strong><span>Optimal-objective disagreements in the 23-case, five-solver W9 run at relative 1e−4</span></div><div><strong>6 / 23</strong><span>Markov-cero failures or timeouts in that run; coverage remains incomplete</span></div></div><p>These comparisons support the reported cases only. The repeated 20-case geometric runtime ratios (markov-cero / HiGHS) were 15.61× and 12.76×; timing boundaries differed between APIs and subprocesses.</p><p className="source-note"><Source path="evidence/comparison/current_glpk_pinned_20260928/full_comparison_report.md" /> · <Source path="docs/project/STATUS.md" /></p></section>
    <section className="page-section"><h2>Known differences and past defects</h2><p>Zero objective disagreements does not mean identical statuses. In the W9 run, for example, markov-cero reached IterationLimit on <code>swath1</code> while GLPK and SCIP reported Optimal. Earlier internal audits found false Optimal and false Infeasible edge cases; residual-aware dual checks and strict nonzero ray validation were added. The current status still lists hard-instance and numerical coverage limits.</p><p className="source-note"><Source path="CHANGELOG.md" /> · <Source path="docs/project/STATUS.md" /></p></section>
  </PageShell>
}

function RefineryPage() {
  return <PageShell eyebrow="03 / SYNTHETIC MODEL" title="Refinery case study" lead="A small, synthetic crude blending and planning LP. It demonstrates model formulation and verification; it is not plant data or an approved operating plan.">
    <section className="page-section"><h2>Inputs and decision</h2><div className="page-card-grid"><article><span>DECISIONS</span><h3>Crude A and B</h3><p>Purchase and process tonnes of two synthetic crudes. Unit costs: 48 and 36 currency units per tonne.</p></article><article><span>CAPACITY</span><h3>100 t throughput</h3><p>A ≤ 70 t, B ≤ 80 t. Combined distillation A + B ≤ 100 t.</p></article><article><span>PRODUCT TARGETS</span><h3>Three minimum yields</h3><p>Petrol ≥ 22 t, diesel ≥ 24 t and ATF ≥ 8 t under fixed linear yields.</p></article><article><span>QUALITY PROXY</span><h3>Sulfur mass cap</h3><p>0.012A + 0.028B ≤ 2. This is a simplified linear cap, not a full blend-quality model.</p></article></div></section>
    <section className="page-section"><h2>Computed plan</h2><div className="metric-row"><div><strong>38.095 t</strong><span>Crude A</span></div><div><strong>21.429 t</strong><span>Crude B</span></div><div><strong>2,600</strong><span>Minimum purchase-cost objective</span></div></div><div className="split-panel"><div><strong>Active constraints</strong><p>Petrol minimum 22 t and ATF minimum 8 t are binding. Diesel has 0.762 t slack; throughput and sulfur caps are not binding.</p></div><div><strong>Status and runtime</strong><p>Optimal · canonical and original-space checks passed. One local CLI run on 2026-09-29 reported 0.172 ms solver runtime; this is a single illustrative measurement, not a benchmark.</p></div></div><p className="source-note">Model: <Source path="examples/refinery/refinery-feasible.mps" /> · Dictionary: <Source path="examples/refinery/data-dictionary.md" /> · Local binary SHA-256: <code>05c11f6da3a20def…e1eb8b4dee</code>.</p></section>
    <section className="page-section callout"><h2>Qualification boundary</h2><p>The broader refinery demonstration still needs feed and quality-balance validation and an engineer-owned pilot. Historical Fawley data is qualification material, not an approved plant model.</p><p><Source path="docs/project/STATUS.md" /></p></section>
  </PageShell>
}

function SovereigntyPage() {
  return <PageShell eyebrow="04 / PROVENANCE" title="Sovereignty & source" lead="The engine’s dependency boundary is documented separately from its source-independence claim. Both matter.">
    <section className="page-section"><div className="page-card-grid"><article><span>IN PROJECT</span><h3>Solver implementation</h3><p>Parser, model, presolve, LP/MILP/QP paths, checks and CLI are implemented in the project’s C++20 source tree.</p></article><article><span>EXTERNAL REFERENCES</span><h3>HiGHS, SCIP and peers</h3><p>Used for independent numerical comparison and research. They are not linked into the solver runtime.</p></article><article><span>DEPENDENCY RULE</span><h3>No external solver linkage</h3><p>Production code uses the C++20 standard library, threads and optional CUDA. The repository uses Apache-2.0 licensing.</p></article><article><span>OPEN REVIEW</span><h3>Clean-room claim</h3><p>Peer-source inspection was recorded on 2026-09-25. An independent source-trace review has not completed, so source independence is unverified.</p></article></div><p className="source-note"><Source path="docs/project/PROVENANCE.md" /> · <Source path="LICENSE" /></p></section>
  </PageShell>
}

function BenchmarksPage() {
  const rows = [['Netlib','98','51'],['MIPLIB','119','4'],['Mittelmann','20','4'],['QPLIB','18','4']]
  return <PageShell eyebrow="05 / MEASURED EVIDENCE" title="Benchmarks" lead="Pinned local evidence with explicit time caps and failure counts. This is not a claim of general competitiveness.">
    <section className="page-section"><h2>Local 15-second suite</h2><div className="matrix-wrap"><table><thead><tr><th>Collection</th><th>Checked-in rows</th><th>Verified optimal</th></tr></thead><tbody>{rows.map(([n,total,verified])=><tr key={n}><th>{n}</th><td>{total}</td><td>{verified}</td></tr>)}</tbody></table></div><p>All 255 checked-in rows were attempted with a solver-side 15-second cap; three exceeded the parent watchdog. MIPLIB also had four verified infeasibility certificates. This does not cover complete upstream datasets or the planned 300-second runs.</p><p className="source-note">Pinned binary SHA-256 <code>beaed56a7534f8c…8bc43258e7cd1</code> · <Source path="docs/project/STATUS.md" /></p></section>
    <section className="page-section"><h2>CPU versus GPU, end to end</h2><div className="split-panel"><div><strong>Physical RTX 2050</strong><p>Four generated scales passed solution verification in repeated host runs. GPU PDLP was 2.3–8.3× slower end to end than CPU PDLP in those measurements.</p></div><div><strong>Negative result matters</strong><p>No crossover threshold or GPU speedup is established. The GPU QP device path computes P·x only; its x-update remains on CPU.</p></div></div><p className="source-note"><Source path="evidence/gpu_hardware_host_access_check_20260928.json" /> · <Source path="evidence/gpu_hardware_host_access_check_20260928.json" /></p></section>
    <section className="page-section"><h2>Reference comparison</h2><p>The 23-instance run used one thread and a 15-second per-solver cap on a 12-core host. Markov-cero and HiGHS each reached Optimal on 17, GLPK on 14, CBC on 13 and SCIP on 18. No pair of reported optimal objectives disagreed beyond relative 1e−4. Runtime methods have different process boundaries.</p><div className="matrix-wrap"><table><thead><tr><th>Instance</th><th>Class</th><th>Markov-cero</th><th>HiGHS</th><th>Observation</th></tr></thead><tbody><tr><th>afiro</th><td>Netlib LP</td><td>Optimal · 3.7 ms</td><td>Optimal · 9.5 ms</td><td>Objectives agreed</td></tr><tr><th>scorpion</th><td>Netlib LP</td><td>Optimal · 526.7 ms</td><td>Optimal · 18.8 ms</td><td>Objectives agreed; runtime favored HiGHS</td></tr><tr><th>stein15</th><td>MIPLIB MILP</td><td>Optimal · 1084.6 ms</td><td>Optimal · 46.3 ms</td><td>Objectives agreed; runtime favored HiGHS</td></tr><tr><th>swath1</th><td>MIPLIB MILP</td><td>IterationLimit</td><td>Feasible</td><td>No optimal comparison for this pair</td></tr></tbody></table></div><p className="source-note"><Source path="evidence/comparison/current_glpk_pinned_20260928/full_comparison_report.md" /></p></section>
    <section className="page-section"><h2>Run provenance</h2><div className="split-panel"><div><strong>CPU comparison</strong><p>12 host cores, one solver thread per run, 15-second cap, generated 2026-09-28. Binary SHA-256: <code>beaed56a7534f8c…8bc43258e7cd1</code>. The report does not record a source commit for that binary.</p></div><div><strong>GPU study</strong><p>NVIDIA RTX 2050 (sm_86). Two repeated order-reversed host runs used pinned CUDA binary SHA-256 <code>93c96173cbb9…e0a44a3037a5a</code>, which was not rebuilt from the current worktree.</p></div></div></section>
  </PageShell>
}

const vertices = [[0,0],[4,0],[4,2],[2,4],[0,3]]
function AlgorithmDemo({ kind }: {kind: 'simplex'|'dual'|'branch'|'presolve'}) {
  const [weight,setWeight] = useState(2)
  const [limit,setLimit] = useState(3)
  const scores = vertices.map(([x,y])=>weight*x+y)
  const best = scores.indexOf(Math.max(...scores))
  const visible = vertices.map(([x,y],i)=>({x,y,i})).filter(({i})=>kind!=='presolve'||scores[i]>=scores[best]-limit)
  return <div className="demo"><svg viewBox="0 0 240 240" role="img" aria-label="Interactive two-variable feasible region"><polygon points="20,220 200,220 200,130 110,40 20,85" fill="#dcebf6" stroke="#628bad" strokeWidth="2"/>{kind==='branch'&&<><path d="M20 150H200 M110 40V220" stroke="#9ab3c9" strokeDasharray="4 4"/><path d="M20 150H110V85" stroke="#e87f24" strokeWidth="3" fill="none"/></>}{visible.map(({x,y,i})=><circle key={i} cx={20+x*45} cy={220-y*45} r={i===best?7:4} fill={i===best?'#e87f24':'#1b2a41'}/>)}</svg><div><label>Objective weight for x <strong>{weight.toFixed(1)}</strong><input type="range" min=".5" max="4" step=".1" value={weight} onChange={e=>setWeight(Number(e.target.value))}/></label>{kind==='presolve'&&<label>Toy bound window <strong>{limit}</strong><input type="range" min="1" max="5" step="1" value={limit} onChange={e=>setLimit(Number(e.target.value))}/></label>}<p>{kind==='simplex'?'The highlighted vertex maximizes this toy objective. Changing the slope changes the candidate basis.':kind==='dual'?'Move the objective and watch the supporting vertex shift. This is an intuition aid, not a dual-simplex trace.':kind==='branch'?'The orange split illustrates an integer branch; child bounds determine whether a branch survives.':`An objective-bound illustration keeps ${visible.length} of ${vertices.length} candidate vertices while preserving the best candidate. Actual presolve uses model equivalence rules.`}</p></div></div>
}
function AlgorithmsPage() {
  return <PageShell eyebrow="06 / METHODS" title="Algorithms lab" lead="Small, interactive two-variable sketches illustrate solver decisions. They are teaching models, not live output from the production engine.">
    <section className="page-section">{([['simplex','Primal simplex','Walk feasible vertices to improve the objective.'],['dual','Dual simplex','Restore primal feasibility while maintaining a dual-feasible basis.'],['branch','Branch-and-bound','Split a discrete choice and compare child bounds.'],['presolve','Presolve','Remove redundant structure before the main solve.']] as const).map(([kind,name,lead])=><article className="demo-card" key={kind}><span>METHOD / {kind.toUpperCase()}</span><h2>{name}</h2><p>{lead}</p><AlgorithmDemo kind={kind}/></article>)}</section><p className="source-note"><Source path="docs/project/STATUS.md" /> · <Source path="docs/project/STATUS.md" /></p>
  </PageShell>
}
function GpuPage() {
  return <PageShell eyebrow="07 / COMPUTE RESEARCH" title="GPU & research" lead="Parallel hardware is useful only when the entire solve benefits. The current evidence favors caution.">
    <section className="page-section"><h2>Why a hybrid path?</h2><div className="page-card-grid"><article><span>CPU</span><h3>Simplex and branch decisions</h3><p>Sparse basis updates and tree logic contain serial dependencies. CPU remains the principal path.</p></article><article><span>GPU</span><h3>PDLP / PDHG operators</h3><p>Sparse matrix-vector products and vector updates map to parallel kernels. Current RTX 2050 end-to-end runs are slower.</p></article><article><span>PARTIAL DEVICE PATH</span><h3>QP residual P·x</h3><p>The QP x-update and KKT factorization remain on CPU; this is not full GPU QP acceleration.</p></article><article><span>HYPOTHESIS</span><h3>ML branching</h3><p>A learned ranking might reduce node work, but the data split and durable model validation are open. No model is promoted.</p></article></div><p className="source-note"><Source path="evidence/gpu_hardware_host_access_check_20260928.json" /> · <Source path="docs/project/STATUS.md" /></p></section>
  </PageShell>
}
function DocsPage() {
  return <PageShell eyebrow="08 / INTERFACES" title="Docs / API" lead="Start with a model file and a bounded run. The current CLI and C++ API exist; a versioned C ABI is planned.">
    <section className="page-section"><h2>CLI quick start</h2><pre><code>markov-cero-solve MODEL.mps --engine auto --time-limit 60 --output result.json</code></pre><p>Free-format MPS supports rows, columns, RHS, bounds, ranges and integer markers. A JSON result reports status, objective, verification details, runtime and diagnostics. The frontend workspace accepts MPS or LP text only when a solver API is configured.</p><div className="page-card-grid"><article><span>RESULT STATUS</span><h3>Optimal / Infeasible / Unbounded</h3><p>Proof-backed terminal outcomes when the required witnesses pass.</p></article><article><span>LIMIT STATUS</span><h3>Resource / Iteration / Numerical</h3><p>Incomplete search or failed numerical gates remain explicit.</p></article><article><span>CURRENT API</span><h3>C++ and Python</h3><p>Native C++ API and pybind11 Python bindings are present.</p></article><article><span>PLANNED</span><h3>Versioned C ABI</h3><p>No stable C ABI or broader language-binding contract is released yet.</p></article></div><p className="source-note"><Source path="docs/project/STATUS.md" /> · <Source path="examples/refinery/README.md" /></p></section>
  </PageShell>
}
function TeamPage() {
  return <PageShell eyebrow="09 / STEWARDSHIP" title="markov-cero team" lead="The team maintains the solver through source review, numerical checks, and recorded evidence.">
    <section className="page-section"><h2>Ownership and review</h2><p>The markov-cero team prepared this project for SIH26119. Substantial implementation has used AI code-generation assistance under human direction and review. The authors retain responsibility for architecture, mathematical checks, and claims. A verified public member roster is not maintained in the source records.</p><p className="source-note"><Source path="docs/project/PROVENANCE.md" /></p></section>
  </PageShell>
}
function SecurityPage() {
  return <PageShell eyebrow="10 / SECURITY" title="Security" lead="A clear route for reporting defects and understanding the current boundaries.">
    <section className="page-section"><h2>Report a concern</h2><p>Open a minimal <a href={repo+'/issues'} target="_blank" rel="noreferrer">repository issue ↗</a> without credentials, private models, or exploit payloads. The current repository does not include a dedicated SECURITY.md disclosure channel; use a private project contact when sensitive details are involved.</p><div className="page-card-grid"><article><span>INPUT BOUNDARY</span><h3>Model parsing</h3><p>The MPS parser applies bounds checks and typed parse failures. Full memory-budget coverage is still open.</p></article><article><span>RESULT BOUNDARY</span><h3>Fail closed</h3><p>Allocation failure at exercised API boundary points maps to ResourceLimit rather than an incorrect mathematical status.</p></article></div><p className="source-note"><Source path="docs/project/STATUS.md" /> · <Source path="CHANGELOG.md" /></p></section>
  </PageShell>
}
function ChangelogPage() {
  return <PageShell eyebrow="11 / CHANGE RECORD" title="Changelog" lead="A short guide to the current release line and the work recorded since it.">
    <section className="page-section"><h2>v0.5.2 documented release</h2><p>The status register describes the current documented solver release as v0.5.2. Earlier milestones and later work are recorded in the changelog.</p><h2>Unreleased work</h2><p>Recent entries cover bounded node materialization, memory evidence, stage instrumentation, worker isolation, and allocation-failure handling. The related resource and end-to-end engine integration gates remain open.</p><p className="source-note"><Source path="CHANGELOG.md" /> · <Source path="CHANGELOG.md" /></p></section>
  </PageShell>
}
function GlossaryPage() {
  const terms=[['Feasible','Satisfies the model constraints within the declared tolerance.'],['Objective','The quantity to minimize or maximize.'],['Bound','A proven limit on the best possible solution.'],['Residual','The amount by which a mathematical condition is not met.'],['Gap','Difference between a candidate and a proven bound, with a stated scale.'],['Presolve','Transformations that simplify a model while preserving the supported outcome.'],['KKT','Optimality conditions checked for continuous constrained problems.'],['Certificate','A witness that can be checked to support a solver status.']]
  return <PageShell eyebrow="12 / TERMS" title="Glossary" lead="The words behind the decision graph, in practical terms."><section className="page-section"><div className="matrix-wrap"><table><thead><tr><th>Term</th><th>Meaning</th></tr></thead><tbody>{terms.map(([term,meaning])=><tr key={term}><th>{term}</th><td>{meaning}</td></tr>)}</tbody></table></div></section></PageShell>
}

export default function Pages({ page }: {page: string}) {
  switch(page) {
    case 'status': return <StatusPage/>
    case 'verification': return <VerificationPage/>
    case 'refinery': return <RefineryPage/>
    case 'sovereignty': return <SovereigntyPage/>
    case 'benchmarks': return <BenchmarksPage/>
    case 'algorithms': return <AlgorithmsPage/>
    case 'gpu': return <GpuPage/>
    case 'docs': return <DocsPage/>
    case 'team': return <TeamPage/>
    case 'security': return <SecurityPage/>
    case 'changelog': return <ChangelogPage/>
    case 'glossary': return <GlossaryPage/>
    default: return <StatusPage/>
  }
}
