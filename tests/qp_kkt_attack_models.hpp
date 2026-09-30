#pragma once

// QP-01 attack-suite models (docs/contracts/convex-qp.md §6): frozen MPS
// fixtures shared by tests/qp_kkt_attack_test.cpp.

// min ½(x1²+x2²) s.t. x1+x2 = 2 -> x = (1,1), objective 1.
inline const char* const kEqualityMps = R"(NAME EQB
ROWS
 N O
 E C1
COLUMNS
    X1        O                   0  C1                1
    X2        O                   0  C1                1
RHS
    RHS1      C1                  2
QUADOBJ
    X1        X1                 1
    X2        X2                 1
ENDATA
)";

// min -x subject to 0 <= x <= 1 (no quadratic term): zero Hessian LP.
inline const char* const kBoxMps = R"(NAME ZH
ROWS
 N O
 L R
COLUMNS
    X1        O                  -1  R                 1
RHS
    RHS1      R                   1
ENDATA
)";

// min -x with x free and no other constraint: recession direction exists.
inline const char* const kUnboundedMps = R"(NAME ZHU
ROWS
 N O
COLUMNS
    X1        O                  -1
BOUNDS
 FR BND1      X1
ENDATA
)";

// Rank-1 PSD Hessian ½(x1+x2)² with linear term -(x1+x2): flat optimal face.
inline const char* const kSingularPsdMps = R"(NAME SPSD
ROWS
 N O
COLUMNS
    X1        O                  -1
    X2        O                  -1
QUADOBJ
    X1        X1                 1
    X1        X2                 1
    X2        X2                 1
BOUNDS
 FR BND1      X1
 FR BND1      X2
ENDATA
)";

// A constraint row that no column references (empty row).
inline const char* const kEmptyRowMps = R"(NAME ERW
ROWS
 N O
 L EMPTY
 L R
COLUMNS
    X1        O                   1  R                 1
RHS
    RHS1      R                   1
ENDATA
)";

// Conflicting bounds: x1+x2 >= 5 and x1+x2 <= 2.
inline const char* const kInfeasibleMps = R"(NAME INFEAS
ROWS
 N OBJ
 G C1
 L C2
COLUMNS
    X1        OBJ                1  C1                1
    X1        C2                 1
    X2        OBJ                1  C1                1
    X2        C2                 1
RHS
    RHS1      C1                  5
    RHS1      C2                  2
QUADOBJ
    X1        X1                 2
    X2        X2                 2
ENDATA
)";

// Non-convex curvature: P = -1 on a bounded variable.
inline const char* const kNonConvexMps = R"(NAME NONCVX
ROWS
 N O
 L R
COLUMNS
    X1        O                  -1  R                 1
RHS
    RHS1      R                   1
QUADOBJ
    X1        X1                 -1
ENDATA
)";

// Same structure as each other, different P values (cache test).
inline const char* const kCacheAMps = R"(NAME CACHEA
ROWS
 N O
 L R
COLUMNS
    X1        O                  -1  R                 1
RHS
    RHS1      R                   1
QUADOBJ
    X1        X1                 1
ENDATA
)";
inline const char* const kCacheBMps = R"(NAME CACHEB
ROWS
 N O
 L R
COLUMNS
    X1        O                  -1  R                 1
RHS
    RHS1      R                   1
QUADOBJ
    X1        X1                 4
ENDATA
)";
