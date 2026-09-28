NAME          LOGISTICS
ROWS
 N  COST
 G  DEM1
 G  DEM2
 G  DEM3
 L  CAP1
 L  CAP2
COLUMNS
    MARK0000  'MARKER'                 'INTORG'
    O1        COST      1200.0    CAP1      -150.0
    O2        COST      1500.0    CAP2      -200.0
    MARK0001  'MARKER'                 'INTEND'
    S11       COST      4.0       DEM1      1.0
    S11       CAP1      1.0
    S12       COST      8.0       DEM2      1.0
    S12       CAP1      1.0
    S13       COST      11.0      DEM3      1.0
    S13       CAP1      1.0
    S21       COST      9.0       DEM1      1.0
    S21       CAP2      1.0
    S22       COST      5.0       DEM2      1.0
    S22       CAP2      1.0
    S23       COST      6.0       DEM3      1.0
    S23       CAP2      1.0
RHS
    RHS1      DEM1      60.0      DEM2      70.0
    RHS1      DEM3      80.0
BOUNDS
 BV BND       O1
 BV BND       O2
ENDATA
