---
type: competitive-note
tags: [competitive, t3, sih26119]
status: verified
cloned: 2026-09-25
threat: none
---

# Placeholder and empty repos

> T3: no solver substance. Listed for completeness (the survey is full-scale) and because
> *volume of empty submissions* frames how rare a working artifact is.

| Repo | Contents (verified) |
|---|---|
| **xarjunpatil/SIH26119-…Sovereign-Alternative-to** | Pitch web app: `README.md` (75 L), `problem_statement.json`, `project/{app.py 136 L, index.html, solution.md, Dockerfile, test_app.py}`. FastAPI/Chart.js "mission control dashboard"; **template reuse visible** — badges say domain "Landslide & Slope Stability GIS" for a refinery-solver PS. Zero solver code [Observed]. |
| **shivarajhdindure-tech/indigenous-gpu-optimization-solver** | 29 scaffold files, **every single one 0 bytes** (`wc -c` on `backend/src/cli/main.cpp`, `README.md`, `CMakeLists.txt` → 0). Filename-only skeleton (qp/active_set, milp/cutting_planes, cusparse_kernels.cuh) in one commit [Observed]. |
| **Abhinav-Prabhakar/SIH2026** | 231 files, **186 .md** = scrape of all 175 SIH-2026 problem statements from sih.gov.in (`README.md:3`); our PS exists only as a copy. Only executable content = an unrelated Next.js app for a different PS [Observed]. |
| **NIVION-HUB/SIH2026-PS** | 2 files: `SIH_2026_Problem_Statements_Final.txt` (88 KB) + `README.md` (29 KB), PS text incl. SIH26119 verbatim. No code [Observed]. |
| **AryanSahu321/sih** | 2 files byte-identical (61,820 B): README + `problemSelection` scoring matrix; row `| #27 | SIH26119 | 12/500 | 35.8/40 |`. Last commit: "Update print statement to say 'Goodbye World'" [Observed]. |
| **Nandhitha-ai/indigenous-optimization-solver** | Single 32-byte `README.md`: `# indigenous-optimization-solver` [Observed]. |
| **Naresh-V-7/SIH26119-CPU-Solver-and-Data** | **Zero commits** — `git status` → "No commits yet" [Observed]. |
| **sheena9937/Indigenous-GPU-Accelerated-Optimization-Solver** | **Zero commits** (empty repo) [Observed]. |

**R10/R16**: none of the eight touches any external solver or ships any result — nothing to
compare, nothing to fear.

**Field texture for the deck:** ~8 of 27 discovered repos are empty or PS-scrapes; only 9
projects (6 distinct teams + siblings) actually build engines. Being *one of the few with
committed, re-runnable evidence* is the story [[19-competitive-landscape]] §19.5 tells.

Related: [[19-competitive-landscape]] §19.7.

## Referenced By

- [[Competitive Landscape MOC|competitive/Competitive Landscape MOC]]