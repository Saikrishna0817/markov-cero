NAME          PROD_PLAN
ROWS
 N  COST
 E  BAL1
 E  BAL2
 E  BAL3
 L  CAP1
 L  CAP2
 L  CAP3
COLUMNS
    MARK0000  'MARKER'                 'INTORG'
    Y1        COST      500.0     CAP1      -150.0
    Y2        COST      500.0     CAP2      -150.0
    Y3        COST      500.0     CAP3      -150.0
    MARK0001  'MARKER'                 'INTEND'
    P1        COST      20.0      BAL1      1.0
    P1        CAP1      1.0
    P2        COST      20.0      BAL2      1.0
    P2        CAP2      1.0
    P3        COST      20.0      BAL3      1.0
    P3        CAP3      1.0
    I1        COST      3.0       BAL1      -1.0
    I1        BAL2      1.0
    I2        COST      3.0       BAL2      -1.0
    I2        BAL3      1.0
    I3        COST      3.0       BAL3      -1.0
RHS
    RHS1      BAL1      50.0      BAL2      80.0
    RHS1      BAL3      60.0
BOUNDS
 BV BND       Y1
 BV BND       Y2
 BV BND       Y3
 UP BND       I1        40.0
 UP BND       I2        40.0
 UP BND       I3        40.0
ENDATA
