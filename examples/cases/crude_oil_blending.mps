NAME          CRUDE_BLEND
ROWS
 N  COST
 G  REGULAR
 G  PREMIUM
 G  OCTANE
 L  SULFUR
COLUMNS
    X1        COST      45.0      REGULAR   0.4
    X1        PREMIUM   0.1       OCTANE    -3.0
    X1        SULFUR    1.0
    X2        COST      48.0      REGULAR   0.2
    X2        PREMIUM   0.2       OCTANE    2.0
    X2        SULFUR    0.5
    X3        COST      65.0      REGULAR   0.3
    X3        PREMIUM   0.4       OCTANE    8.0
    X3        SULFUR    -0.5
    X4        COST      70.0      REGULAR   0.1
    X4        PREMIUM   0.3       OCTANE    5.0
    X4        SULFUR    -0.8
RHS
    RHS1      REGULAR   40.0      PREMIUM   30.0
BOUNDS
 UP BND       X1        100.0
 UP BND       X2        100.0
 UP BND       X3        100.0
 UP BND       X4        100.0
QUADOBJ
    X3        X3        1.0
    X4        X4        1.0
ENDATA
