---
type: moc
tags: [moc, competitive, sih, landscape]
status: complete
date: 2026-09-25
---

# Competitive Landscape MOC

> The SIH26119 rival field, 2026-09-25: 27 GitHub repos found, cloned, and inspected at
> file:line level (local clones in `/tmp/opencode/competitors/`). Nine T1 projects actually
> build solver engines; ten are pitches or empty. Summary matrices, threat ranking, and
> counter-moves: [[19-competitive-landscape]].

## T1 — real competitors (deep notes)

| Note | Project | Threat | One-line |
|---|---|---|---|
| [[SANKHYA (thegoodengineers)]] | thegoodengineers/SANKHYA (+ Deekshith2205 mirror) | **Very high** | IPM+parallel+cuts+CI+ldd sovereignty gate |
| [[sankhya (team-vertexx)]] | team-vertexx/sankhya | High | R16 runs inside CI; "62% of HiGHS" |
| [[VX03 (VioniX37)]] | VioniX37/VX03 | High | HSD-IPM + PyTorch GPU + CI bench; PolyForm licence |
| [[Igaos-public (trijalpgunaseelan)]] | trijalpgunaseelan/Igaos-public | High | Broadest engines + CPLEX/Gurobi CSVs; no CI |
| [[refinery-optimizer (kavinR-11)]] | kavinR-11/refinery-optimizer | Medium-high | Only published HiGHS *win*; no CI, no LICENSE |
| [[IGAOS (Lothnic)]] | Lothnic/IGAOS | Medium-high | PyPI + pinned baseline; README contradicts own CSVs |
| [[PIPEPYE (Satyanshgaur)]] | Satyanshgaur/PIPEPYE | Medium | Honest per-instance ratios it mostly loses (18/32) |
| [[Firefly solver (akshayvarma121)]] | akshayvarma121/Firefly_solver | Medium | Real CUDA; "HiGHS reference" is 2 hardcoded numbers |

## T2 — partial engines

[[optimisation solver (RaghavGupta2910)]] · [[bharatopt (JosephXpanakaL)]] ·
[[sovereign-solver (AryanMotiani)]] · [[Bharat-Opt (Kouhsik33)]] ·
[[Mid-tier solver repos]] (mohitsaitummalapalli, nomos, ApexCUDA×2, hemasri, infinity390)

## T3 — pitches / empty

[[Placeholder and empty repos]] (xarjunpatil, Abhinav-Prabhakar PS scrape, NIVION-HUB,
AryanSahu321, shivarajhdindure zero-byte scaffold, three no-commit repos)

## Reference columns

[[Established solvers]] — HiGHS / SCIP / CBC / GLPK / commercial, Mittelmann standing, the
honest student-solver bar (mipx 2–40× slower than HiGHS).

[[Prior art - solver and GPU projects]] — cuPDLP-C/-x, cuOSQP, NVIDIA cuOpt, Minotaur, FOSSEE;
what a sovereign student solver can honestly claim instead.

## Navigation

Report with matrices + threat ranking + counter-moves: [[19-competitive-landscape]] ·
Our standing: [[FINAL-AUDIT-REPORT]] · [[09-research-code-alignment]] · Strategy hub: [[SIH Strategy MOC]]

## Referenced By

- [[19-competitive-landscape|audit/19-competitive-landscape]]
- [[FINAL-AUDIT-REPORT|audit/FINAL-AUDIT-REPORT]]
- [[SIH Strategy MOC|audit/SIH Strategy MOC]]