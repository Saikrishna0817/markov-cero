NAME          NEIRO_REFINERY_SCHEDULING
ROWS
 N  objCOST
 L  crude_supply_0
 L  crude_supply_1
 L  crude_supply_2
 L  crude_supply_3
 L  unit_cap_0_0
 G  unit_min_0_0
 L  unit_cap_0_1
 G  unit_min_0_1
 L  unit_cap_0_2
 G  unit_min_0_2
 L  unit_cap_0_3
 G  unit_min_0_3
 L  unit_cap_1_0
 G  unit_min_1_0
 L  unit_cap_1_1
 G  unit_min_1_1
 L  unit_cap_1_2
 G  unit_min_1_2
 L  unit_cap_1_3
 G  unit_min_1_3
 G  demand_0_0
 L  market_0_0
 G  demand_0_1
 L  market_0_1
 G  demand_0_2
 L  market_0_2
 G  demand_0_3
 L  market_0_3
 G  demand_1_0
 L  market_1_0
 G  demand_1_1
 L  market_1_1
 G  demand_1_2
 L  market_1_2
 G  demand_1_3
 L  market_1_3
 G  demand_2_0
 L  market_2_0
 G  demand_2_1
 L  market_2_1
 G  demand_2_2
 L  market_2_2
 G  demand_2_3
 L  market_2_3
 G  inventory_end
COLUMNS
    x0_0          objCOST                      -60  crude_supply_0                   1
    x0_1          objCOST                      -60  crude_supply_1                   1
    x0_2          objCOST                      -60  crude_supply_2                   1
    x0_3          objCOST                      -60  crude_supply_3                   1
    x1_0          objCOST                      -65  crude_supply_0                   1
    x1_1          objCOST                      -65  crude_supply_1                   1
    x1_2          objCOST                      -65  crude_supply_2                   1
    x1_3          objCOST                      -65  crude_supply_3                   1
    x2_0          objCOST                      -70  crude_supply_0                   1
    x2_1          objCOST                      -70  crude_supply_1                   1
    x2_2          objCOST                      -70  crude_supply_2                   1
    x2_3          objCOST                      -70  crude_supply_3                   1
    MARKER000                 'MARKER'                 INTORG
    y0_0          objCOST                       30  unit_min_0_0                 -20
    y0_0          unit_cap_0_0                -200
    MARKER001                 'MARKER'                 INTEND
    f0_0          objCOST                        1  unit_cap_0_0                   1
    f0_0          unit_min_0_0                   1  demand_0_0                   0.2
    f0_0          demand_1_0                   0.2  demand_2_0                   0.2
    MARKER002                 'MARKER'                 INTORG
    z0_0_0        objCOST                        2  unit_cap_0_0                  80
    z1_0_0        objCOST                        2  unit_cap_0_0                  80
    z2_0_0        objCOST                        2  unit_cap_0_0                  80
    y0_1          objCOST                       30  unit_min_0_1                 -20
    y0_1          unit_cap_0_1                -200
    MARKER003                 'MARKER'                 INTEND
    f0_1          objCOST                        1  unit_cap_0_1                   1
    f0_1          unit_min_0_1                   1  demand_0_1                   0.2
    f0_1          demand_1_1                   0.2  demand_2_1                   0.2
    MARKER004                 'MARKER'                 INTORG
    z0_0_1        objCOST                        2  unit_cap_0_1                  80
    z1_0_1        objCOST                        2  unit_cap_0_1                  80
    z2_0_1        objCOST                        2  unit_cap_0_1                  80
    y0_2          objCOST                       30  unit_min_0_2                 -20
    y0_2          unit_cap_0_2                -200
    MARKER005                 'MARKER'                 INTEND
    f0_2          objCOST                        1  unit_cap_0_2                   1
    f0_2          unit_min_0_2                   1  demand_0_2                   0.2
    f0_2          demand_1_2                   0.2  demand_2_2                   0.2
    MARKER006                 'MARKER'                 INTORG
    z0_0_2        objCOST                        2  unit_cap_0_2                  80
    z1_0_2        objCOST                        2  unit_cap_0_2                  80
    z2_0_2        objCOST                        2  unit_cap_0_2                  80
    y0_3          objCOST                       30  unit_min_0_3                 -20
    y0_3          unit_cap_0_3                -200
    MARKER007                 'MARKER'                 INTEND
    f0_3          objCOST                        1  unit_cap_0_3                   1
    f0_3          unit_min_0_3                   1  demand_0_3                   0.2
    f0_3          demand_1_3                   0.2  demand_2_3                   0.2
    MARKER008                 'MARKER'                 INTORG
    z0_0_3        objCOST                        2  unit_cap_0_3                  80
    z1_0_3        objCOST                        2  unit_cap_0_3                  80
    z2_0_3        objCOST                        2  unit_cap_0_3                  80
    y1_0          objCOST                       30  unit_min_1_0                 -20
    y1_0          unit_cap_1_0                -200
    MARKER009                 'MARKER'                 INTEND
    f1_0          objCOST                        1  unit_cap_1_0                   1
    f1_0          unit_min_1_0                   1  demand_0_0                   0.2
    f1_0          demand_1_0                   0.2  demand_2_0                   0.2
    MARKER010                 'MARKER'                 INTORG
    z0_1_0        objCOST                        2  unit_cap_1_0                  80
    z1_1_0        objCOST                        2  unit_cap_1_0                  80
    z2_1_0        objCOST                        2  unit_cap_1_0                  80
    y1_1          objCOST                       30  unit_min_1_1                 -20
    y1_1          unit_cap_1_1                -200
    MARKER011                 'MARKER'                 INTEND
    f1_1          objCOST                        1  unit_cap_1_1                   1
    f1_1          unit_min_1_1                   1  demand_0_1                   0.2
    f1_1          demand_1_1                   0.2  demand_2_1                   0.2
    MARKER012                 'MARKER'                 INTORG
    z0_1_1        objCOST                        2  unit_cap_1_1                  80
    z1_1_1        objCOST                        2  unit_cap_1_1                  80
    z2_1_1        objCOST                        2  unit_cap_1_1                  80
    y1_2          objCOST                       30  unit_min_1_2                 -20
    y1_2          unit_cap_1_2                -200
    MARKER013                 'MARKER'                 INTEND
    f1_2          objCOST                        1  unit_cap_1_2                   1
    f1_2          unit_min_1_2                   1  demand_0_2                   0.2
    f1_2          demand_1_2                   0.2  demand_2_2                   0.2
    MARKER014                 'MARKER'                 INTORG
    z0_1_2        objCOST                        2  unit_cap_1_2                  80
    z1_1_2        objCOST                        2  unit_cap_1_2                  80
    z2_1_2        objCOST                        2  unit_cap_1_2                  80
    y1_3          objCOST                       30  unit_min_1_3                 -20
    y1_3          unit_cap_1_3                -200
    MARKER015                 'MARKER'                 INTEND
    f1_3          objCOST                        1  unit_cap_1_3                   1
    f1_3          unit_min_1_3                   1  demand_0_3                   0.2
    f1_3          demand_1_3                   0.2  demand_2_3                   0.2
    MARKER016                 'MARKER'                 INTORG
    z0_1_3        objCOST                        2  unit_cap_1_3                  80
    z1_1_3        objCOST                        2  unit_cap_1_3                  80
    z2_1_3        objCOST                        2  unit_cap_1_3                  80
    MARKER017                 'MARKER'                 INTEND
    p0_0          objCOST                     -120  demand_0_0                    -1
    p0_0          market_0_0                     1
    inv0_0        objCOST                      2.5  demand_0_0                     1
    inv0_0        demand_0_1                    -1
    p0_1          objCOST                     -120  demand_0_1                    -1
    p0_1          market_0_1                     1
    inv0_1        objCOST                      2.5  demand_0_1                     1
    inv0_1        demand_0_2                    -1
    p0_2          objCOST                     -120  demand_0_2                    -1
    p0_2          market_0_2                     1
    inv0_2        objCOST                      2.5  demand_0_2                     1
    inv0_2        demand_0_3                    -1
    p0_3          objCOST                     -120  demand_0_3                    -1
    p0_3          market_0_3                     1
    inv0_3        objCOST                      2.5  demand_0_3                     1
    inv0_3        inventory_end                   1
    p1_0          objCOST                     -128  demand_1_0                    -1
    p1_0          market_1_0                     1
    inv1_0        objCOST                      2.5  demand_1_0                     1
    inv1_0        demand_1_1                    -1
    p1_1          objCOST                     -128  demand_1_1                    -1
    p1_1          market_1_1                     1
    inv1_1        objCOST                      2.5  demand_1_1                     1
    inv1_1        demand_1_2                    -1
    p1_2          objCOST                     -128  demand_1_2                    -1
    p1_2          market_1_2                     1
    inv1_2        objCOST                      2.5  demand_1_2                     1
    inv1_2        demand_1_3                    -1
    p1_3          objCOST                     -128  demand_1_3                    -1
    p1_3          market_1_3                     1
    inv1_3        objCOST                      2.5  demand_1_3                     1
    inv1_3        inventory_end                   1
    p2_0          objCOST                     -136  demand_2_0                    -1
    p2_0          market_2_0                     1
    inv2_0        objCOST                      2.5  demand_2_0                     1
    inv2_0        demand_2_1                    -1
    p2_1          objCOST                     -136  demand_2_1                    -1
    p2_1          market_2_1                     1
    inv2_1        objCOST                      2.5  demand_2_1                     1
    inv2_1        demand_2_2                    -1
    p2_2          objCOST                     -136  demand_2_2                    -1
    p2_2          market_2_2                     1
    inv2_2        objCOST                      2.5  demand_2_2                     1
    inv2_2        demand_2_3                    -1
    p2_3          objCOST                     -136  demand_2_3                    -1
    p2_3          market_2_3                     1
    inv2_3        objCOST                      2.5  demand_2_3                     1
    inv2_3        inventory_end                   1
RHS
    RHS       crude_supply_0                 500
    RHS       crude_supply_1                 500
    RHS       crude_supply_2                 500
    RHS       crude_supply_3                 500
    RHS       unit_cap_0_0                 200
    RHS       unit_min_0_0                  20
    RHS       unit_cap_0_1                 200
    RHS       unit_min_0_1                  20
    RHS       unit_cap_0_2                 200
    RHS       unit_min_0_2                  20
    RHS       unit_cap_0_3                 200
    RHS       unit_min_0_3                  20
    RHS       unit_cap_1_0                 200
    RHS       unit_min_1_0                  20
    RHS       unit_cap_1_1                 200
    RHS       unit_min_1_1                  20
    RHS       unit_cap_1_2                 200
    RHS       unit_min_1_2                  20
    RHS       unit_cap_1_3                 200
    RHS       unit_min_1_3                  20
    RHS       demand_0_0                    40
    RHS       market_0_0                    90
    RHS       demand_0_1                    50
    RHS       market_0_1                    90
    RHS       demand_0_2                    60
    RHS       market_0_2                    90
    RHS       demand_0_3                    70
    RHS       market_0_3                    90
    RHS       demand_1_0                    40
    RHS       market_1_0                    90
    RHS       demand_1_1                    50
    RHS       market_1_1                    90
    RHS       demand_1_2                    60
    RHS       market_1_2                    90
    RHS       demand_1_3                    70
    RHS       market_1_3                    90
    RHS       demand_2_0                    40
    RHS       market_2_0                    90
    RHS       demand_2_1                    50
    RHS       market_2_1                    90
    RHS       demand_2_2                    60
    RHS       market_2_2                    90
    RHS       demand_2_3                    70
    RHS       market_2_3                    90
    RHS       inventory_end                 100
BOUNDS
 UP BND       y0_0                           1
 UP BND       z0_0_0                         1
 UP BND       z1_0_0                         1
 UP BND       z2_0_0                         1
 UP BND       y0_1                           1
 UP BND       z0_0_1                         1
 UP BND       z1_0_1                         1
 UP BND       z2_0_1                         1
 UP BND       y0_2                           1
 UP BND       z0_0_2                         1
 UP BND       z1_0_2                         1
 UP BND       z2_0_2                         1
 UP BND       y0_3                           1
 UP BND       z0_0_3                         1
 UP BND       z1_0_3                         1
 UP BND       z2_0_3                         1
 UP BND       y1_0                           1
 UP BND       z0_1_0                         1
 UP BND       z1_1_0                         1
 UP BND       z2_1_0                         1
 UP BND       y1_1                           1
 UP BND       z0_1_1                         1
 UP BND       z1_1_1                         1
 UP BND       z2_1_1                         1
 UP BND       y1_2                           1
 UP BND       z0_1_2                         1
 UP BND       z1_1_2                         1
 UP BND       z2_1_2                         1
 UP BND       y1_3                           1
 UP BND       z0_1_3                         1
 UP BND       z1_1_3                         1
 UP BND       z2_1_3                         1
ENDATA
