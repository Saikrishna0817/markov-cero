# Sovereign Optimization Solver — Consolidated Research Paper List

This bibliography is a dated research inventory for algorithm and evaluation
work. Inclusion means a paper was considered relevant, not that markov-cero
implements its method or has verified its results. The [research index](README.md)
explains the note archive; [status](../project/STATUS.md) records current scope.

> SIH-2026 PS SIH26119 (MRPL) — Sovereign LP/MILP/QP solver core built from scratch.
> Merged from two sources: the 14-module web research pass and the 54-entry "Research Paper
> Repository" document, deduplicated and reorganized by solver component.
> **Total: 204 entries.** ✦ = added from the repository document, ○ = from web research, ★ = priority read.
> Placeholder citations ("—") have been resolved; see §17 Errata for the corrected records.

---

## Module 0 — Orientation & Survey (read first)

| # | Paper | Authors / Year | Purpose |
|---|---|---|---|
| 1 | Computational Linear Programming: The Evolution of LP Solvers ★ | Bixby, 2002 (Operations Research) | 40-year history of LP computation; measured impact of presolve, sparse LU, dual simplex, steepest-edge |
| 2 | The Interior-Point Revolution in Optimization ★ | Wright, 2004 (SIAM Review) [link](https://doi.org/10.1090/s0273-0979-04-01040-7) | History + theory linking simplex, barrier, Karmarkar |
| 3 | Constraint Integer Programming (PhD thesis) ★ | Achterberg, 2007 (ZIB) | Best architectural blueprint: presolve, cuts, heuristics, branching, LP interconnection |
| 4 | Fifty Years of Integer Programming: A Review of the Solution Approaches ○ | Kumar, Luhandjula, Munapo & Jones, 2010 [doi](https://doi.org/10.1177/097324701000600301) | Expository overview of IP solution approaches |
| 5 | Integer and Combinatorial Optimization (book) ○ | Nemhauser & Wolsey | LP/MILP theory backbone |
| 6 | Linear Programming: Foundations and Extensions (book) ○ | Vanderbei | Standard LP text |
| 7 | Computational Optimization Techniques in Linear Programming (book) ○ | Maros | Best single book on LP numerics |

---

## Module 1 — Problem Input, Data Structures, Scaling

| # | Paper / Ref | Authors / Year | Purpose |
|---|---|---|---|
| 8 | MPS format specification (IBM/Gamst; Netlib `lp/data` docs) ○ | 1970s– | Parsing free/fixed MPS + LP formats for benchmark runs |
| 9 | Improving LP-Representations of Zero-One Linear Programs for Branch-and-Cut ○ | Hoffman & Padberg, 1991 (J. Computing) | Model tightening before solve |
| 10 | The Simplex Method of Linear Programming Using LU Decomposition / Automatic Scaling of Matrices for Gaussian Elimination ○ | Curtis & Reid, 1972 (JIMA); Oren, "Model Scaling in LP", 1980 | Row/column scaling — cheapest robustness win against ill-conditioning |
| 11 | Equilibrating Both Matrices in Linearly Dependent Problems ○ | O'Leary, 1981 | Handling rank-deficient/dependent rows |

---

## Module 2 — Sparse Linear Algebra Foundation

| # | Paper | Authors / Year | Purpose |
|---|---|---|---|
| 12 | The Simplex Method of Linear Programming Using LU Decomposition ★ | Bartels & Golub, 1969 (CACM) [doi](https://doi.org/10.1145/362946.362974) | Basis as LU, not explicit inverse |
| 13 | Updating Triangular Factors of the Basis to Maintain Sparsity ★ | Forrest & Tomlin, 1972 (Math. Prog.) | Standard basis update (vs. refactorization) |
| 14 | Sparse Partial Pivoting in Time Proportional to Arithmetic Operations ★ | Gilbert & Peierls, 1988 (SIAM JSSC) | Sparse triangular solve (FTRAN/BTRAN) proportional to touched entries |
| 15 | An Approximate Minimum Degree Ordering Algorithm ★ | Amestoy, Davis & Duff, 1996 (SIMAX) | AMD fill-reducing ordering |
| 16 | COLAMD: A Column Approximate Minimum Degree Ordering Algorithm (Algorithm 836) ★ | Davis, Gilbert, Larimore & Ng, 2004 (ACM TOMS) [link](https://dl.acm.org/doi/10.1145/1024074.1024080) | Column ordering for unsymmetric A / IPM normal equations |
| 17 | The Elimination Form of the Inverse and its Application to Linear Programming ○ | Markowitz, 1957 (Management Sci.) | Pivot selection: fill-in vs. stability |
| 18 | A Fast LU Factorization for Linear Programming Bases ○ | Suhl & Suhl, 1990 (Math. Prog.) | Threshold partial pivoting for bases |
| 19 | A Sparsity-Exploiting Variant of the Bartels-Golub Algorithm ○ | Reid, 1982 | Refactorization policy |
| 20 | Direct Methods for Sparse Matrices (book) ○ | Duff, Erisman & Reid | Complete sparse direct methods reference |
| 21 | The Role of Elimination Trees in Sparse Factorization ○ | Liu, 1990 (SIMAX) | Elimination tree theory |
| 22 | A Fast and High Quality Multilevel Scheme for Partitioning Irregular Graphs (METIS) ○ | Karypis & Kumar, 1998 | Nested-dissection ordering for very large systems |
| 23 | Design and Implementation of a Reduced-Space SQP Solver with Column Reordering for Large-Scale Process Optimization ✦ | Zhao, Liu, Jiang et al., 2025 (Algorithms) [link](https://www.mdpi.com/1999-4893/18/11/699) | Column reordering + sparse computation in a practical solver |
| 24 | Good and Fast Row-Sparse AH-Symmetric Reflexive Generalized Inverses ✦ | Ponte, Fampa, Lee & Xu, 2026 [link](https://arxiv.org/abs/2401.17540) | Sparse generalized-inverse construction (basis-handling adjacent) |
| 25 | Prestructuring Sparse Matrices with Dense Rows and Columns via Null Space Methods ✦ | Howell, 2018 (Numer. Lin. Alg. Appl.) [doi](https://doi.org/10.1002/nla.2133) | Dense row/column handling in otherwise sparse industrial models |
| 26 | Parallel Symbolic Cholesky Factorization ✦ | Ribizel & Anzt, 2023 (SC-W) [doi](https://doi.org/10.1145/3624062.3624253) | Symbolic factorization for IPM on large sparse problems (GPU/CPU) |
| 27 | Parallel Symbolic Factorization for Sparse LU with Static Pivoting ✦ | Grigori, Demmel & Li, 2007 (SIAM J. Sci. Comput.) [doi](https://doi.org/10.1137/050638102) | Memory-scalable parallel symbolic factorization, graph-partitioned |
| 28 | Sympiler: Transforming Sparse Matrix Codes by Decoupling Symbolic Analysis ✦ | Cheshmi, Kamil, Strout & Mehri Dehnavi, 2017 (SC'17) [doi](https://doi.org/10.1145/3126908.3126936) | Compiler for sparse-solver kernels via symbolic transformation |

---

## Module 3 — LP: Simplex Methods

### 3A. Primal / revised simplex

| # | Paper | Authors / Year | Purpose |
|---|---|---|---|
| 29 | Linear Programming and Extensions (book) + "Computational Algorithm of the Revised Simplex Method" (RAND RM-1266) ★ | Dantzig, 1953/1963 | Original algorithm |
| 30 | The Product Form for the Inverse in the Simplex Method ★ | Dantzig & Orchard-Hays, 1954 (MTAC) [doi](https://doi.org/10.1090/s0025-5718-1954-0061469-8) | PFI representation |
| 31 | A Practicable Steepest-Edge Simplex Algorithm ★ | Goldfarb & Reid, 1977 (Math. Prog.) | Steepest-edge pricing |
| 32 | Pivot Selection Methods of the Devex Simplex Code ★ | Harris, 1973 (Math. Prog.) | Tolerance-based ratio test |
| 33 | The Anti-Cycling Rule for the Simplex Method ★ | Bland, 1977 (Operations Research) | Finite termination guarantee |
| 34 | Implementation of the Simplex Algorithm ✦ | Mehlhorn, 2010 [pdf](https://resources.mpi-inf.mpg.de/departments/d1/teaching/ss10/Obst/Simplex.pdf) | Practical revised-simplex implementation guide; round-off, sparsity, public-domain code comparison |
| 35 | Hyper-sparsity in the Revised Simplex Method and How to Exploit It ✦ | Hall & McKinnon, 2005 (Comp. Optim. Appl.) [link](https://link.springer.com/article/10.1007/s10589-005-2404-4) | FTRAN/BTRAN/PRICE hyper-sparsity exploitation; 5.2× speedup |
| 36 | Factorization and Update of a Reduced Basis Matrix for the Revised Simplex Method ✦ | Gleixner, 2012 (ZIB) [link](https://opus4.kobv.de/opus4-zib/frontdoor/index/index/docId/1681) | Reduced-basis factorization/update improving stability + sparsity |

### 3B. Dual simplex (used at every MIP node)

| # | Paper | Authors / Year | Purpose |
|---|---|---|---|
| 37 | The Dual Simplex Method: Techniques for a Fast and Stable Implementation (PhD thesis) ★ | Koberstein, 2005 [link](http://nbn-resolving.de/urn:nbn:de:hbz:466-20050101272) | Definitive dual-simplex implementation reference: Harris tolerances, BFRT, hypersparseness, pricing loops |
| 38 | Steepest Edge Simplex Algorithms for Linear Programming ★ | Goldfarb & Forrest, 1992 (Math. Prog.) | Dual steepest edge — essential for degenerate LP relaxations |
| 39 | Solving Linear Programs by Dual Simplex ○ | Fourer, 1982; Dantzig/Cottle/Lemke (eds.) | Standard dual simplex |
| 40 | A Generalized Dual Phase-2 Simplex Algorithm ○ | Maros, 2003 (EJOR) [link](https://www.doc.ic.ac.uk/research/technicalreports/2001/DTR01-2.pdf) | Bound-flipping, multi-pivot dual iterations |
| 41 | Another Simplex-Type Method for Large Scale Linear Programming ○ | Gondzio, 1994 | Dual simplex + hypersparse solves |
| 42 | Hierarchical Solution of Large-Scale Linear Programs; Steepest-Edge Simplexing for Network-Style Problems ○ | Fourer & Maros; Fourer, 1994 (Networks) | BFRT details |
| 43 | Parallelizing the Dual Revised Simplex Method ✦ | Huangfu & Hall, 2018 (Math. Prog. Comp.) [link](https://link.springer.com/article/10.1007/s12532-017-0130-5) | Algorithmic basis of the HiGHS simplex; parallel dual simplex design |
| 44 | A Practical Anti-Cycling Procedure for Linearly Constrained Optimization (EXPAND) ○ | Gill, Murray, Saunders & Wright, 1989 (Math. Prog.) [link](https://convexoptimization.com/sol/papers/EXPAND.pdf) | Expanding working tolerance — practical anti-stalling |
| 45 | A Stabilization of the Simplex Method ○ | Bartels, 1971 (Numer. Math.) | First rigorous rounding-error analysis of LU simplex |

---

## Module 4 — LP: Interior-Point Methods

| # | Paper | Authors / Year | Purpose |
|---|---|---|---|
| 46 | A Polynomial Algorithm in Linear Programming ★ | Khachiyan, 1979 | Ellipsoid method / complexity foundation |
| 47 | A New Polynomial-Time Algorithm for Linear Programming ★ | Karmarkar, 1984 (Combinatorica) [link](https://link.springer.com/article/10.1007/BF02579150) | Origin of IPM |
| 48 | A Primal-Dual Interior-Point Algorithm for Linear Programming ★ | Kojima, Mizuno & Yoshise, 1989 | Path-following foundation |
| 49 | On the Implementation of a Primal-Dual Interior Point Algorithm ★ | Mehrotra, 1992 (SIAM J. Optim.) [link](https://epubs.siam.org/doi/10.1137/0802015) | Predictor-corrector — algorithm used by nearly all modern IPMs |
| 50 | On Implementing Mehrotra's Predictor-Corrector IPM for LP ★ | Lustig, Marsten & Shanno, 1992 (SIAM J. Optim.) | Free variables, bounds, dense-column Schur instability |
| 51 | Primal-Dual Interior-Point Methods (book) ★ | Wright, 1997 (SIAM) [link](https://epubs.siam.org/doi/book/10.1137/1.9781611971453) | Complete algorithm + sparse linear algebra + full Mehrotra spec |
| 52 | Interior Point Methods for LP: Computational State of the Art ○ | Lustig, Marsten & Shanno, 1994 (IJOC) [pdf](https://pubsonline.informs.org/doi/pdf/10.1287/ijoc.6.1.1) | OB1 implementation details (presolve, ordering, factorization) |
| 53 | On an Interior Point Algorithm for Linear Programming ○ | Ye, 1991; Monteiro & Adler, 1989 | Path-following complexity |
| 54 | Multiple Centrality Corrections in a Primal-Dual Method ○ | Gondzio, 1996 (CPAA) | Higher-order corrections |
| 55 | Symmetric Indefinite Systems for Interior Point Methods ○ | Vanderbei, 1995; Fourer & Mehrotra, 1993 | LDLᵀ vs. normal equations — conditioning tradeoff |
| 56 | Presolve Analysis of LPs Prior to Applying an Interior Point Method ○ | Gondzio, 1997 (IJOC) | Dense column splitting, implied free variables, dependent rows |
| 57 | Solving Dense Linear Systems with Semi-Normal Equations ○ | Fourer & Mehrotra | Robust normal-equation solves |
| 58 | Crossover and Interior Point Algorithms for LP ★ | Ye, 1998 (CPAA); Lustig et al., "Experimental Investigations in Combining PD IPM and Simplex", 1995 (Ann. OR) | IPM → simplex crossover (extracting a basis/vertex) |
| 59 | An Interior Point Method in Dantzig-Wolfe Decomposition ✦ | Martinson & Tind, 1999 (Comput. Optim. Appl.) [link](https://www.sciencedirect.com/science/article/pii/S0305054898001892) | IPM on block-angular (decomposed) LP structures — relevant to refinery/planning models |
| 60 | A Simple, Quadratically Convergent Interior Point Algorithm for LP and Convex QP ✦ | Tits & Zhou, 1994 (Large Scale Optimization: State of the Art, Kluwer, pp. 411–427) [doi](https://doi.org/10.1007/978-1-4613-3632-7_20) | Quadratic-convergence IPM for both LP and convex QP |
| 61 | An Implementation of a Primal-Dual Interior Point Method for Linear Programming ✦ | McShane, Monma & Shanno, 1989 (IJOC) [link](https://pubsonline.informs.org/doi/10.1287/ijoc.1.1.70) | Classic practical IPM implementation paper |

---

## Module 5 — Presolve & Model Reduction

| # | Paper | Authors / Year | Purpose |
|---|---|---|---|
| 62 | Presolving in Linear Programming ★ | Andersen & Andersen, 1995 (Math. Prog.) [doi](https://doi.org/10.1007/BF01586000) | Canonical LP presolve survey: singleton/doubleton rows & cols, implied bounds, dominated/forcing constraints, dual recovery (postsolve) |
| 63 | Preprocessing and Probing Techniques for Mixed Integer Programming Problems ★ | Savelsbergh, 1994 (ORSA J. Computing) [pdf](https://www2.isye.gatech.edu/~ms79/software/ojoc6.pdf) | MIP probing, logical implications, clique inequalities, coefficient reduction |
| 64 | Progress in Presolving for Mixed Integer Programming ★ | Gamrath, Koch, Martin, Miltenheimer & Weninger, 2015 (MPC) [pdf](https://webdoc.sub.gwdg.de/ebook/serien/ah/ZIB/ZR-13-48.pdf) | Singleton-column stuffing, dominating columns, connected components |
| 65 | Presolve Reductions in Mixed Integer Programming ★ | Achterberg, Bixby, Gu, Rothberg & Weninger, 2020 (IJOC) [doi](https://doi.org/10.1287/ijoc.2018.0857) | Complete Gurobi presolve taxonomy — most complete modern description |
| 66 | Analysis of Mathematical Programming Problems Prior to Applying the Simplex Method ○ | Brearley, Mitra & Williams, 1975 (JOTA) | The original presolve paper |
| 67 | Presolving Mixed-Integer Linear Programs ○ | Mahajan, 2011 (Enc. of OR) [doi](https://doi.org/10.1002/9780470400531.eorms0437) | Short survey + bibliography |
| 68 | Integer-Programming Software Systems ○ | Atamtürk & Savelsbergh, 2008 (Ann. OR) | How presolve integrates into branch-and-cut |
| 69 | Feasibility and Redundancy in Linear Programming ○ | Chinneck, 1992 (IJOC) | Redundancy detection heuristics |
| 70 | A Novel Linear Optimization Presolve Technique Based on Fourier-Motzkin Elimination ✦ | Zhang, Ploskas & Sahinidis, 2026 (Math. Prog. Comp.) [link](https://link.springer.com/article/10.1007/s12532-026-00278-w) | FME-based presolve with reduction-size estimation; 6–11% CPU reductions on CPLEX |
| 71 | Enhancing Presolve in Mixed Integer Programming by Combining Probing and Dual Fixing ✦ | Wang, Chen & Dai, 2026 [link](https://arxiv.org/abs/2607.10767) | Probing + dual fixing combined |
| 72 | A Combined Linear and Nonlinear Presolve for Nonlinear Optimization ✦ | Zhang & Sahinidis, 2025 (EURO J. Comput. Optim.) [doi](https://doi.org/10.1016/j.ejco.2025.100119) | Combined linear/nonlinear presolve for the future NLP/MINLP extension |
| 73 | Model Scaling / Equilibration (cross-ref entry 10) ○ | — | Referenced here for completeness |

---

## Module 6 — Quadratic Programming (QP)

| # | Paper | Authors / Year | Purpose |
|---|---|---|---|
| 74 | A Numerically Stable Dual Method for Solving Strictly Convex Quadratic Programs ★ | Goldfarb & Idnani, 1983 (Math. Prog.) [doi](https://doi.org/10.1007/BF02591962) | Reference active-set dual QP: Cholesky + QR updates, no Phase I |
| 75 | Dual and Primal-Dual Methods for Solving Strictly Convex Quadratic Programs ★ | Goldfarb & Idnani, 1984; Panton, "On the QP Algorithm of Goldfarb and Idnani", 1985 | Implementation variants + ready-to-use subroutine |
| 76 | Primal-Dual IPM (Ch. on convex QP); Monteiro, Adler & Resende, 1990 ★ | Wright, 1997 | Interior-point QP extending Mehrotra (for large-constraint QPs) |
| 77 | Sequential Quadratic Programming (Acta Numerica) ○ | Boggs & Tolle, 1995 | QP as building block for future NLP/MINLP |
| 78 | Solving Least Squares Problems; Practical Optimization (books) ○ | Lawson & Hanson, 1974; Gill, Murray & Wright, 1981 | Equality-constrained/least-squares subproblems, NNLS, Cholesky handling |
| 79 | A Dual Active-Set Algorithm for Positive Semidefinite Quadratic Programming ○ | Connell, 1999 (Math. Prog.) | Handling PSD (not strictly PD) Q |

---

## Module 7 — MILP: Branch-and-Cut Framework & Solver Architecture

| # | Paper | Authors / Year | Purpose |
|---|---|---|---|
| 80 | An Automatic Method of Solving Discrete Programming Problems ★ | Land & Doig, 1960 (Econometrica) | Original branch-and-bound |
| 81 | An Additive Algorithm for Solving LPs with Zero-One Variables ★ | Balas, 1965 (Operations Research) | Implicit enumeration |
| 82 | A Branch-and-Cut Algorithm for the Resolution of Large-Scale Symmetric TSP Problems ★ | Padberg & Rinaldi, 1991 (SIAM Review) | The branch-and-cut paper: LP relaxation + separation + combinatorial bounding |
| 83 | General Mixed Integer Programming: Computational Issues for Branch-and-Cut Algorithms ★ | Achterberg, Koch & Martin, 2005 (IJOC) | How all components interact within one node |
| 84 | Valid Inequalities for Mixed Integer Linear Sets ★ | Cornuéjols, 2008 (Math. Prog. B) [pdf](https://www.andrew.cmu.edu/user/gc0v/webpub/integerRioMPSjuly.pdf) | Split, GMI, lift-and-project, intersection cuts and their relationships |
| 85 | Strong Formulations for Mixed Integer Programming: A Survey ○ | Wolsey, 1989 | Formulation strength |
| 86 | Branch-and-Cut for Combinatorial Optimization ○ | Jünger, Reinelt & Rinaldi, 1995 (Handbook) | Algorithmic structure survey |
| 87 | Branch-and-Cut Algorithms for Combinatorial Optimization and Their Implementation in ABACUS ○ | Cornuéjols & Li, 2001 [chapter](https://link.springer.com/chapter/10.1007/3-540-45586-8_5) | Software architecture: managers, pools, callbacks |
| 88 | Branch and Bound Algorithms — Principles and Examples ○ | Clausen, 1999 | Practical B&B mechanics |
| 89 | Noncommercial Software for Mixed-Integer Linear Programming (CBC) ✦ | Linderoth & Ralphs, 2005 [link](https://optimization-online.org/2004/12/945/) | Architectural description of CBC: class hierarchy, customization |
| 90 | Compiling Mixed Integer Programming Problems: The SCIP Optimization Suite ✦ | Bixby, Hendel, Vigerske & Weninger, 2020 (MPC) / [scipopt.org](https://www.scipopt.org/) | Architecture of SCIP + SoPlex framework (LP/MILP/MINLP) |
| 91 | CBC Solver Documentation (black box vs. framework) ✦ | COIN-OR [link](https://www.coin-or.org/Cbc/) | API design and framework reference |
| 92 | The Space of Branching Rules for Branch-and-Cut ○ | Cook | Design space of branching rules |

---

## Module 8 — Cutting Planes

| # | Paper | Authors / Year | Purpose |
|---|---|---|---|
| 93 | Outline of an Algorithm for Integer Solutions to Linear Programs ★ | Gomory, 1958 (Bull. AMS) | Fractional cuts — origin |
| 94 | All-Integer Programming Algorithm / mixed-integer version ★ | Gomory, 1963 (Operations Research) | GMI (mixed-integer rounding) cuts |
| 95 | Gomory Cuts Revisited ★ | Balas, Ceria, Cornuéjols & Natraj, 1996 (OR Letters) | Revived Gomory cuts: 86% vs 55% solve rate on MIPLIB |
| 96 | A Lift-and-Project Cutting Plane Algorithm for Mixed 0/1 Programs ★ | Balas, Ceria & Cornuéjols, 1993 (Math. Prog.) [doi](https://doi.org/10.1007/BF01581273) | Lift-and-project / disjunctive cuts |
| 97 | Edmonds Polytopes and a Hierarchy of Combinatorial Problems ★ | Chvátal, 1973 (Discrete Math.) | Chvátal–Gomory cuts, Chvátal rank |
| 98 | Classical Cuts for Mixed-Integer Programming and Branch-and-Cut ★ | Padberg, 2005 (Ann. OR) [link](https://ideas.repec.org/a/spr/annopr/v139y2005i1p321-35210.1007/s10479-005-3453-y.html) | Survey of fractional/MIR cuts + validity after branching |
| 99 | The Mixed Integer Rounding Family of Inequalities / Computational Study of L&P Cuts ★ | Marchand & Wolsey, 1996; Cornuéjols & Tuintra | MIR cuts (workhorse of modern solvers) |
| 100 | Cuts of Fixed Rank in Zero-One Matrices ★ | Balas & Zemel, 1980 (Math. Prog.) | Knapsack cover cuts — most effective family for binary packing |
| 101 | Cover Inequalities for Mixed-Integer Programs ○ | Atamtürk, 2003 | Modern cover generation |
| 102 | Aggregation and Knapsack Inequalities ○ | Marchand & Wolsey, 1996 (OR Letters) | Row aggregation for cuts |
| 103 | Linear Programming Implementations of the Lift-and-Project Method ○ | Benichou, Gauthier, Girard & Sierksma, 1997 (IJOC) | L&P cut implementation in OSL |
| 104 | On the Potential of Cutting Planes in Practical MIP / Learning to Select Cutting Planes ○ | Turner, Berger & Achterberg, 2024; Papageorgiou & Trespalacios | Cut management — where most modern gains live |
| 105 | The Group Approach to Cutting Planes: Recent Developments ○ | Richard & Dey, 2010 [pdf](https://www.maths.tcd.ie/EMIS/journals/DMJDMV/vol-ismp/37_cornuejols-gerard.pdf) | Modern corner-polyhedron theory |
| 106 | Investigating the Exact Effectiveness of Cutting Planes over Branch-and-Bound in Integer Programming ✦ | Su, 2025 [JHU link](https://jscholarship.library.jhu.edu/items/) | Proof: B&C proof trees reducible to pure cutting-plane trees for 3D convex 0/1 IPs |
| 107 | Machine Learning Techniques for Branch-and-Cut Methods: The Selection of Cutting Planes ✦ | Giallombardo, Miglionico & Sammarra, 2025 (LNCS 14476, NUMTA 2023) [doi](https://doi.org/10.1007/978-3-031-81241-5_25) | SVR-based cut ranking/selection |
| 108 | Computational Study of Cutting Planes for a Lot-Sizing Problem in Branch-and-Cut Algorithm ✦ | Chung, 2015 (J. Operations Research Society of Korea) [doi](https://doi.org/10.7737/jkorms.2015.40.3.023) | Strength comparison of three cut families (application-level) |
| 109 | To Pool or Not to Pool? (row aggregation in cut generation) ○ | Rex, Gleixner et al. | Aggregation effects on cut quality |

---

## Module 9 — Primal Heuristics (feasible solutions)

| # | Paper | Authors / Year | Purpose |
|---|---|---|---|
| 110 | The Feasibility Pump ★ | Fischetti, Glover & Lodi, 2005 (Math. Prog.) [pdf](http://www.dei.unipd.it/~fisch/papers/feasibility_pump.pdf) | Fastest route to first incumbent |
| 111 | A Feasibility Pump Heuristic for General MIPs ★ | Fischetti, Bertacco & Lodi, 2006 (Discrete Optim.) [pdf](http://www.dei.unipd.it/~fisch/papers/fp_general_integer.pdf) | FP for general integers |
| 112 | Improving the Feasibility Pump ★ | Achterberg & Berthold, 2007 (Discrete Optim.) [pdf](https://webdoc.sub.gwdg.de/ebook/serien/ah/reports/ZIBreport/ZR-05-42.pdf) | Objective FP: mean gap 55% → 29.5% |
| 113 | Exploring Relaxation Induced Neighborhoods to Improve MIP Solutions (RINS) ★ | Danna, Rothberg & Le Pape, 2004 (Math. Prog.) | Fix agreeing variables, solve sub-MIP |
| 114 | Local Branching ★ | Fischetti & Lodi, 2003 (Math. Prog.) | Distance-constrained sub-MIP around incumbent |
| 115 | Heuristics of the Branch-Cut-and-Price-Framework SCIP ★ | Berthold, 2007 (ZIB) [pdf](https://webdoc.sub.gwdg.de/ebook/serien/ah/ZIB/ZR-07-30.pdf) | Taxonomy of 23 heuristics: rounding, diving (6 variants), objective diving, LNS (RENS/RINS/LB/crossover) |
| 116 | Primal Heuristics for Mixed Integer Programs (MS thesis) ○ | Berthold, 2006 (TU Berlin) | Extended version of #115 |
| 117 | RENS — Relaxation Enforced Neighborhood Search ○ | Berthold, 2007 (ZIB) | Relaxed-variable sub-MIP |
| 118 | Feasibility Jump ○ | Berthold, 2023 (IJOC) | Fast local repair heuristic |
| 119 | Efficient Heuristic Procedures for Integer LP with an Interior ○ | Hillier, 1969 (Operations Research) | Ancestor of rounding heuristics |
| 120 | Reduced Cost Fixing ○ | Bixby, 1994; Savelsbergh, 1994 | Fix variables by reduced cost given a feasible solution |
| 121 | The Traveling Salesman Problem: A Computational Study (heuristic chapters) ○ | Applegate, Bixby, Chvátal & Cook, 2006 | Deep practical heuristic coverage |
| 122 | Rounding and Propagation Heuristics for Mixed Integer Programming ✦ | Achterberg, Berthold & Hendel, 2011 (ZIB) [pdf](https://opus4.kobv.de/opus4-zib/files/1325/ZR-11-29.pdf) | Focused study of rounding + constraint-propagation heuristics |
| 123 | Presolve Heuristics in HiGHS: Implementation and Computational Study ✦ | Spoorendonk, 2026 [link](https://arxiv.org/abs/2609.22938) | Reference implementations of Feasibility Jump, fix-propagate-repair, LocalMIP, Scylla; +23.7% primal integral |
| 124 | A Frank-Wolfe-based Primal Heuristic for Quadratic Mixed-Integer Optimization ✦ | Mexi, Hendrych, Designolle, Besançon & Pokutta, 2026 (MPC) [doi](https://doi.org/10.1007/s12532-026-00267-2) | MIQP heuristic (Boscia extension); 1st place Land-Doig MIP Competition 2025 |
| 125 | Primal Heuristics for Mixed-Integer Nonlinear Programming ✦ | Berthold, 2025 [Cambridge link](https://www.cambridge.org/core/books/abs/primal-heuristics-in-integer-programming/primal-heuristics-for-mixedinteger-nonlinear-programming/) | Extends MIP heuristics to MINLP (roadmap) |
| 126 | Objective Feasibility Pump / crossover heuristics ○ | Achterberg & Berthold; Berthold | (Covered within #112/#115) |

---

## Module 10 — Branching & Node Selection

| # | Paper | Authors / Year | Purpose |
|---|---|---|---|
| 127 | Branching Rules Revisited ★ | Achterberg, Koch & Martin, 2005 (OR Letters) [pdf](https://edocs.tib.eu/files/e01fn06/49371636Xl.pdf) | Most-infeasible, pseudocost, strong branching; reliability branching (η_rel=8, λ=4) |
| 128 | The Impact of Branch-and-Bound Procedures on the Complexity of Integer Programming ★ | Linderoth & Savelsbergh, 2000 | Branching objective: node count vs. time |
| 129 | Lookahead Branching for Mixed Integer Programming ★ | Held, Savelsbergh & Woodruff, 2006 [pdf](https://optimization-online.org/wp-content/uploads/2006/10/1490.pdf) | Two-level lookahead + implications from trial branching |
| 130 | An Algorithm for the Assignment Problem (pseudocost origins) ○ | Driebeek, 1966 | Pseudocost pricing |
| 131 | Hybrid Branching ○ | Berthold, 2006 [pdf](https://www.zib.de/userpage/berthold/hybrid_branching.pdf) | Reliability (MIP) + inference (CP) + VSIDS (SAT) — SCIP default |
| 132 | Cloud Branching ○ | Berthold & Salvagnin, 2013 [pdf](https://www.dei.unipd.it/~salvagni/pdf/cloudbranching.pdf) | Exploiting alternative LP optima (degeneracy) for branching |
| 133 | Best-estimate / best-bound node selection ○ | Achterberg, 2007 (thesis, §15); Linderoth & Karypis | Node selection policy |
| 134 | Cut-Based Conflict Analysis in MIP ○ | Jabbar, Gleixner et al., 2024 (MPC) | Learning from infeasible nodes |
| 135 | How Important Are Branching Decisions: Fooling MIP Solvers ○ | Hollenbeck, Antonsen, Ralphs et al., 2014 [pdf](https://optimization-online.org/wp-content/uploads/2014/04/4324.pdf) | Why branching rules matter and how they fail |
| 136 | Selection of Variables in MIP; Sparse/tri-branching ○ | Rader & Riesselman; Turner et al. | Candidate-set screening, beyond-single-variable branching |
| 137 | Restart Strategies in MIP (knapsack-conflict restarts) ○ | Schweizer, Beringer et al. | Search restarts |
| 138 | Intelligent Branching / Large-Neighborhood Branching ○ | Turner, Salvagnin, Coucheney et al. | Advanced branching beyond single variables |
| 139 | Learning to Select Nodes in Branch and Bound with Sufficient Tree Representation ✦ | Zhang, Zeng, Li, Wu & Li, 2025 (ICLR) [link](https://iclr.cc/virtual/2025/poster/30706) | Tripartite graph + RL/GNN node selection (TRGNN) |
| 140 | Node Selection Strategies in Interval Branch and Bound Algorithms ✦ | Neveu, Trombettoni & Araya, 2016 (JGLO) [link](https://link.springer.com/article/10.1007/s10898-015-0385-1) | Node selection policies incorporating upper bounds |

---

## Module 11 — Numerical Robustness (degeneracy, ill-conditioning, certification)

*Directly targets the "degenerate models / ill-conditioned matrices / weak relaxations" requirement of the problem statement.*

| # | Paper | Authors / Year | Purpose |
|---|---|---|---|
| 141 | Optimality and Multi-Valuedness in LP (perturbation method) ★ | Charnes, 1954; Megiddo, "On the Perturbation Method for Avoiding Degeneracy", 1983 [pdf](https://theory.stanford.edu/~megiddo/pdf/degen.pdf) | Perturbing b, c to break degeneracy; polynomial ε bound |
| 142 | Methods for Modifying Matrix Factorizations ★ | Gill, Golub, Murray & Saunders, 1974 (J. Comp. Math.) | Stable LU updating (replaces naive PFI in production) |
| 143 | Practical Anti-Cycling Procedure (EXPAND) ★ | Gill, Murray, Saunders & Wright, 1989 (Math. Prog.) | Controlled infeasibility + expanding working tolerance |
| 144 | An Estimate for the Condition Number of a Matrix ★ | Cline, Moler, Stewart & Wilkinson, 1979 (SINUM) [doi](https://doi.org/10.1137/0716029) | O(n²) κ(A) estimator — detect ill-conditioning early |
| 145 | Rounding Errors in Algebraic Processes (1963); Accuracy and Stability of Numerical Algorithms (1996/2002) ★ | Wilkinson; Higham | Backward stability, componentwise error bounds |
| 146 | Rounding Errors in Algebraic Processes / Moler iterative refinement ★ | Moler, 1967 (J. ACM) [doi](https://doi.org/10.1145/321386.321394); Higham, 1997 (IMA JNA) | Refinement of basis solves |
| 147 | Iterative Refinement for Linear Programming ★ | Gleixner, Steffy & Wolter, 2015 [pdf](https://optimization-online.org/wp-content/uploads/2015/06/4948.pdf) | LP-level refinement → extended-precision/exact solutions (SoPlex) |
| 148 | Condition Numbers, the Linear Programming Problem and Sensitivity Analysis ★ | Renegar, 1994 | Rigorous definition of "ill-conditioned LP" |
| 149 | Two-Phase Algorithms / Farkas certificate extraction ★ | Gill, Murray, Saunders & Wright; Fourer, "Testing a LP for Infeasibility", 1983; Andersen & Andersen | Provable infeasibility/unboundedness certificates |
| 150 | On Numerical Stability of Simplex Algorithms ○ | Georg & Hettich, 1987/2007 [link](https://www.tandfonline.com/doi/abs/10.1080/02331930701693480) | Bartels–Golub stable **iff** tolerances are error-estimating |
| 151 | On Numerical Stability of Simplex Algorithms (rounding-error analysis) ○ | Ogryczak, 1987 [pdf](https://www.ia.pw.edu.pl/~wogrycza/publikacje/artykuly/mylinalg.pdf) | Rigorous analysis; well-behaved updates + tolerances ⇒ stability |
| 152 | Positive-Edge Pricing Rule for the Dual Simplex ○ | de Farias et al., 2019 (EJOR) [pdf](https://hal.science/hal-02099584v1/document) | Practical anti-degeneracy pricing |
| 153 | Safe Bounds in Linear and Mixed-Integer Linear Programming ○ | Neumaier & Shcherbina, 2004 (Math. Prog.) | Rigorous bound certification for ill-posed instances |
| 154 | Primal-Dual Methods for LP with Ill-Conditioning ○ | Chinneck & Drud, 1987 | Near-singular system handling |
| 155 | Exact Algorithms for Linear Programming / Projective Test ○ | Steinrücken, 2019; Cook & Koch | Exact rational fallback when floating-point fails |
| 156 | A New Degeneracy Method and Steepest-Edge-Based Conditioning for LP ○ | Maros (SIMAX) [doi](https://epubs.siam.org/doi/10.1137/S1052623494277470) | Recursive degeneracy resolution + cheap basis-condition estimates |
| 157 | Exact Arithmetic at Low Cost — A Case Study in Linear Programming ✦ | Gärtner, 1999 [link](https://link.springer.com/article/10.1007/PL00009457) | Hybrid exact/floating-point simplex when m ≪ n |
| 158 | A Revised Simplex Method with Integer Q-Matrices ✦ | Azulay & Pique [link](https://dl.acm.org/doi/10.1145/1989734.1989738) | Integer Q-matrix formulation for exact multiprecision simplex |
| 159 | A Numerical Investigation of the Simplex Method ✦ | Bartels, 1968 (PhD thesis / STAN-CS-68-104, Stanford) [doi](https://doi.org/10.21236/AD0673010) | Demonstrates numerical instability of naive basis-inverse updating |
| 160 | Verified Linear Programming via Tolerance-Aware Precision Boosting ✦ | 2026 [arXiv](https://arxiv.org/abs/2609.11721) | Conditions under which precision-boosted simplex provably matches exact-arithmetic pivots |
| 161 | A Practical Anti-Degeneracy Row Selection Technique in Network LP ○ | Maros, 1993 | Anti-degeneracy pivoting for network structures |
| 162 | A Generalized Dual Phase-2 Simplex Algorithm ○ | Maros, 2003 | (Cross-ref #40) Bound-swapping bypasses dual-degenerate vertices |

---

## Module 12 — Parallelization & GPU

| # | Paper | Authors / Year | Purpose |
|---|---|---|---|
| 163 | Control Strategies for Parallel Mixed Integer Branch and Bound ★ | Eckstein, 1994 (ACM) [doi](https://doi.org/10.1145/602783.602785) | Centralized vs. decentralized work distribution; Karp–Zhang randomization + rendezvous; incumbent broadcast |
| 164 | Parallel SCIP / UG Framework ★ | Berthold; Bussieck, Lindner, Ludwig et al., 2019 (MPC) | Malleable/distributed B&B: ramp-up, node exchange, determinism |
| 165 | On the Design of Parallel Branch-and-Bound Algorithms for MIP ★ | Zhang, Lin, Ralphs & Lübecke; Shinano et al. (ParaSCIP) | Work stealing, racing ramp-up, redundancy management |
| 166 | Improving Branch-and-Cut Performance by Random Sampling ★ | Fischetti, Lodi, Monaci, Salvagnin & Tramontani, 2015 (MPC) [pdf](http://www.dei.unipd.it/~fisch/papers/improving_branch_and_cut_performance_by_random_sampling.pdf) | Parallel sampling of optimal LP bases (adopted in CPLEX 12.5.1) |
| 167 | PIPS-PSBB: Multi-Level Parallelism for Stochastic MIP ○ | Abbasi, Ralphs et al., 2020 [pdf](https://www.osti.gov/servlets/purl/1635781) | Fine-grained decentralized node rebalancing via MPI collectives |
| 168 | Parallel Sparse Linear Algebra in MUMPS ○ | Amestoy, Duff & Gu, 2000 | Parallel sparse factorization |
| 169 | Parallelizing the Approximate Minimum Degree Ordering Algorithm ○ | 2025 [arXiv](https://arxiv.org/html/2504.17097v2) | First scalable shared-memory AMD (7.29× on 64 threads) |
| 170 | Solving Sparse Linear Systems on a Hypercube; Updating LU Factors (Hager) ○ | Anderson & Saad, 1989; Hager, 1980 | Parallel triangular solves / factor updates |
| 171 | Efficient Sparse Matrix-Vector Multiplication on CUDA ○ | Bell & Garland, 2008 (NVIDIA TR) | GPU kernel baseline |
| 172 | Deterministic Parallel MIP ○ | Schweizer et al., 2018 | Reproducible parallel runs (required for credible benchmarking) |
| 173 | Anomalies in Parallel Branch-and-Bound Algorithms ✦ | Lai & Sahni, 1984 (ACM) [link](https://dl.acm.org/doi/10.1145/358080.358071) | Parallelizing B&B can *increase* total time — architecture design constraint |
| 174 | An Asynchronous Parallel Revised Simplex Algorithm ✦ | Hall & McKinnon, 1995 [link](https://www.maths.ed.ac.uk/hall/MS-95-050/) | Parallel revised simplex on MIMD with fast interconnect |
| 175 | PGAS-based Parallel Branch-and-Bound for Ultra-Scale GPU-powered Supercomputers ✦ | Helbecque, 2025 [link](https://orbilu.uni.lu/handle/10993/63097) | PGAS vs MPI+X for exascale B&B; multi-level (GPU + inter-node) parallelism |
| 176 | PDCS: A Primal-Dual Large-Scale Conic Programming Solver with GPU Enhancements ✦ | Lin, Xiong, Ge & Ye, 2025 [link](https://arxiv.org/abs/2505.00311) | GPU primal-dual method beating commercial solvers on large LP/QP |
| 177 | cuPDLP.jl: A GPU Implementation of Restarted PDHG for LP ✦ | Lu & Yang, 2025 (Operations Research) [link](https://pubsonline.informs.org/doi/10.1287/opre.2024.1069) | Demonstrates viability of first-order GPU methods for large LP |
| 178 | An Overview of GPU-based First-Order Methods for LP and Extensions ✦ | 2025 [arXiv](https://ar5iv.labs.arxiv.org/html/2404.05878) | Survey of GPU first-order LP variants and their relationships |
| 179 | Accelerating Optimization Solvers on GPUs ✦ | — [CNRS link](https://hal.cnrs.fr/) | Second-order methods on GPU; NVIDIA cuDSS for sparse LU/Cholesky |

---

## Module 13 — Benchmarking, Evaluation, Reporting

| # | Paper / Ref | Authors / Year | Purpose |
|---|---|---|---|
| 180 | MIPLIB 2017: Data-Driven Compilation of the 6th MIP Library ★ | Gleixner, Hendel, Gamrath, Achterberg et al., 2021 (MPC) [doi](https://doi.org/10.1007/s12532-020-00194-3) | Standard benchmark: 1065 collection + 240 benchmark instances |
| 181 | MIPLIB 2003 ★ | Achterberg, Koch & Martin, 2005 (OR Letters) | Instance taxonomy, performance measurement methodology |
| 182 | Performance Variability in Mixed-Integer Programming ★ | Lodi & Tramontani, 2013 (TutORials in OR) [doi](https://doi.org/10.1287/educ.2013.0112) | Mandatory for honest benchmarking: multi-seed/permutation runs |
| 183 | Benchmarks for Optimization Software (Mittelmann) ★ | Mittelmann [plato.asu.edu](http://plato.asu.edu/bench.html) | Live LP/QP/MIP performance leaderboards |
| 184 | NETLIB LP test set + Anderson & Wright, Solving Linear Systems on Backward Stable Computers ★ | Anderssen & Klee, 1984 | 115+ classic LPs (AFIRO, E226, PILOT, DFL001…) |
| 185 | Benchmarking Optimization Software with Performance Profiles ○ | Dolan & Moré, 2002 (Math. Prog.) | The correct way to plot comparative solver results |
| 186 | A Algorithm for Testing the Adequacy of Algorithms / Decision Tree for Optimization Software ○ | Rice; Mittelmann | Benchmark tooling |
| 187 | MIPLIB 2017 — Design and Experiments + solution checker ○ | Koch et al. | Verifying returned solutions (feasibility + optimality certificates) |
| 188 | Parallel Optimization (reporting metrics) ○ | Censor & Zenios, 1997 | Strong/weak scaling metrics |
| 189 | Distributional MIPLIB (D-MIPLIB) ✦ | 2024 [link](https://arxiv.org/abs/2406.12144) | Distributional benchmark capturing instance-level hardness variation |
| 190 | Mittelmann LP/MILP Benchmark Sets ✦ | Mittelmann [ASU](https://plato.asu.edu/ftp/lpfree.html) | Netlib/Kennington test-set performance pages |
| 191 | mipfeas Benchmark ✦ | Bussieck & Dirkse, 2026 [GAMS](https://www.gams.com/mipfeas/) | 233-instance set for evaluating primal heuristics |
| 192 | Safe Bounds (cross-ref #153) ○ | Neumaier & Shcherbina, 2004 | Certifying optima on ill-posed benchmark instances |

---

## Module 14 — Domain Applications (refinery, power, logistics narrative)

| # | Paper / Ref | Purpose |
|---|---|---|
| 193 | Neiro, Ramos & Mendes, "Mathematical Modeling of Petroleum Refinery" (Comput. Chem. Eng., 2004); Samsioe linear blending LP | Refinery scheduling / crude blending |
| 194 | Kallrath, *Planning and Scheduling in Industry* (2002) | Production planning |
| 195 | Carpentier (1962) origin of economic dispatch; Cohen & Wan (1983) | Power system dispatch / unit commitment |
| 196 | Toth & Vigo, *Vehicle Routing: Problems, Methods, and Applications* (2014); Gouveia, Pires & Martin (Networks, 1998) | Transportation & logistics |
| 197 | Supply-chain arc-flow/lot-sizing literature (Chvátal; Pochet & Wolsey, *Production Planning by Mixed Integer Programming*, 2006) | Supply chain management |

---

## Module 15 — Advanced Extensions (MIQP, MIQCQP, MINLP)

| # | Paper | Authors / Year | Purpose |
|---|---|---|---|
| 198 | Compact Mixed-Integer Programming Formulations in Quadratic Optimization ✦ | Beach, Hildebrand & Huchette, 2022 (JGLO) [link](https://link.springer.com/article/10.1007/s10898-022-01184-6) | Compact MIQP formulations — MIQP extension path |
| 199 | Enhancements of Discretization Approaches for Non-Convex MIQCQP (Parts I & II) ✦ | Beach, Burlacu, Bärmann, Hager & Hildebrand, 2024 (COAP 87(3)) [Part I](https://doi.org/10.1007/s10589-023-00543-7) · [Part II](https://doi.org/10.1007/s10589-024-00554-y) | MIP relaxations for nonconvex quadratic constraints |
| 200 | 50 Years of Mixed-Integer Nonlinear and Disjunctive Programming ✦ | Kronqvist, Bernal & Grossmann, 2025 (EJOR 331(3):687–705) [doi](https://doi.org/10.1016/j.ejor.2025.07.016) | MINLP + Generalized Disjunctive Programming survey |
| 201 | Trends and Perspectives in Deterministic MINLP Optimization for Integrated Planning, Scheduling, Control and Design of Chemical Processes ✦ | Liñán & Ricardez-Sandoval, 2025 (Rev. Chem. Eng. 41(5):451–472) [doi](https://doi.org/10.1515/revce-2024-0064) | Directly matches refinery/process use cases in the problem statement |

---

## Module 16 — Machine Learning for Solver Enhancement (optional track)

| # | Paper | Authors / Year | Purpose |
|---|---|---|---|
| 202 | Apollo-MILP: An Alternating Prediction-Correction Neural Solving Framework for MILP ✦ | 2025 (ICLR) [link](https://iclr.cc/virtual/2025/poster/30706) | ML-predicted initial solutions |
| 203 | Machine Learning Algorithms for Improving Exact Classical Solvers in Mixed Integer Continuous Optimization ✦ | Kimiaei, Kungurtsev & Olimba, 2025 [arXiv](https://arxiv.org/abs/2508.06906) | ML integration points across solver levels |
| 204 | Scalable Primal Heuristics Using Graph Neural Networks for Combinatorial Optimization ✦ | Cantürk, Varol, Aydoğan & Özener, 2024 (JAIR 80:327–376) [doi](https://doi.org/10.1613/jair.1.14972) | GNN-based primal heuristics with inductive bias |

---

## Merged Priority Reading Order

| Pri | Topic | Key papers | Why |
|---|---|---|---|
| 1 | Revised/dual simplex | Mehlhorn 2010; Hall & McKinnon 2005; Koberstein 2005; Goldfarb–Forrest 1992 | Solver core foundation |
| 2 | Sparse linear algebra | Bartels–Golub 1969; Forrest–Tomlin 1972; Gilbert–Peierls 1988; AMD/COLAMD | Everything runs on this |
| 3 | Presolve | Achterberg et al. 2020; Andersen & Andersen 1995; Savelsbergh 1994 | Highest-leverage: orders-of-magnitude speedups |
| 4 | Interior point | Mehrotra 1992; Lustig et al. 1992; Wright 1997 | LP/QP second engine |
| 5 | Branch-and-cut | Padberg–Rinaldi 1991; Achterberg et al. 2005; Cornuéjols 2008 | MILP framework |
| 6 | Cutting planes | Balas et al. 1996 (Gomory revisited); Marchand–Wolsey 1996; Balas–Zemel 1980 | Root-node gap closure |
| 7 | Heuristics | Fischetti–Glover–Lodi 2005; Achterberg–Berthold 2007; Danna et al. 2004; Berthold 2007 | Incumbents prune the tree |
| 8 | Branching | Achterberg–Koch–Martin 2005 | Tree size driver |
| 9 | Numerical robustness | Charnes/Megiddo perturbation; Cline et al. 1979; Gleixner–Steffy–Wolter 2015; Higham | SIH's differentiator |
| 10 | Benchmarking | MIPLIB 2017; Lodi–Tramontani 2013; Dolan–Moré 2002 | Honest evaluation |
| 11 | Parallel/GPU | Eckstein 1994; Huangfu–Hall 2018; Fischetti et al. 2015; cuPDLP 2025; Lai–Sahni 1984 | Scaling to millions of variables |
| 12 | QP | Goldfarb–Idnani 1983 | QP requirement |

---

## Errata & Verification Notes

All placeholder ("—") citations from the original merge were resolved against primary sources:

| Entry | Correction |
|---|---|
| 4 | Correct authors are Kumar, Luhandjula, Munapo & Jones (2010), not Cornuéjols; expository survey |
| 25 | DOI is `10.1002/nla.2133` (repository doc listed `nla.2188`, which is invalid) |
| 60 | Tits & Zhou 1994 book chapter; repository doc's JGLO `s10898-005-3240-y` link points to a different paper |
| 199 | COAP 2024 Parts I & II; repository doc's JGO `s10898-021-01062-7` link points to a different paper |
| 62 | Andersen & Andersen presolve DOI is `10.1007/BF01586000` (`...6003` is invalid) |
| 65 | Achterberg et al. presolve DOI is `10.1287/ijoc.2018.0857` |
| 15 | 1996 SIMAX AMD paper is the companion/original version of Algorithm 1.4; COLAMD is entry 16 |
| — | Remaining generic publisher links (193–195 domain applications, 161 Maros) have full authors but need DOI lookups before download |
