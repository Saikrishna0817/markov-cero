# Consolidated Knowledge — What the Research Papers Tell Us

The 204 papers in `research_paper_references.md` collectively define a complete solver
architecture: sparse linear algebra and stable LU basis updates (Bartels–Golub, Gilbert–Peierls,
Forrest–Tomlin, AMD/COLAMD) form the substrate on which both engines run — the revised/dual
simplex (Dantzig, Harris, Goldfarb–Reid, Koberstein) for MIP nodes and Mehrotra-style interior
point for large LP/QP. On top of that, presolve (Andersen & Andersen, Savelsbergh, Achterberg et
al. 2020) delivers the largest single speedup, cutting planes (Gomory, Balas et al. 1996,
Marchand–Wolsey) close the root-node gap, and branch-and-cut orchestration — branching rules
(Achterberg–Koch–Martin 2005), primal heuristics (Feasibility Pump, RINS, Berthold's taxonomy),
node selection — decides whether the search finishes in seconds or never. The numerics
literature (Charnes/Megiddo degeneracy perturbation, Cline–Moler–Stewart–Wilkinson condition
estimation, Higham's rounding-error theory, Gleixner–Steffy–Wolter iterative refinement) is the
project's differentiator, since the problem statement calls out degenerate models and
ill-conditioned matrices as failure cases. Finally, the parallel/GPU and benchmarking papers
(Eckstein, Huangfu–Hall, cuPDLP, MIPLIB 2017, Lodi–Tramontani) prescribe how to scale and how to
report results honestly against HiGHS/SCIP/Gurobi, while the MIQCQP/MINLP and ML entries (Kronqvist
et al. 2025, Liñán & Ricardez-Sandoval 2025, Cantürk et al. 2024) mark the sanctioned extension
paths beyond the initial LP/MILP/QP scope.
