* Units: production, inventory, demand and capacity synthetic items per period;
* setup variables counts; objective synthetic currency per horizon.
NAME          POCHET_LOT_SIZING
ROWS
 N  objCOST
 L  cap_0
 L  cap_1
 L  cap_2
 L  cap_3
 L  cap_4
 L  cap_5
 L  cap_6
 L  cap_7
 G  dem_0_0
 G  dem_0_1
 G  dem_0_2
 G  dem_0_3
 G  dem_0_4
 G  dem_0_5
 G  dem_0_6
 G  dem_0_7
 G  dem_1_0
 G  dem_1_1
 G  dem_1_2
 G  dem_1_3
 G  dem_1_4
 G  dem_1_5
 G  dem_1_6
 G  dem_1_7
 G  dem_2_0
 G  dem_2_1
 G  dem_2_2
 G  dem_2_3
 G  dem_2_4
 G  dem_2_5
 G  dem_2_6
 G  dem_2_7
 G  dem_3_0
 G  dem_3_1
 G  dem_3_2
 G  dem_3_3
 G  dem_3_4
 G  dem_3_5
 G  dem_3_6
 G  dem_3_7
COLUMNS
    x0_0          objCOST                      1.5  cap_0                          1
    x0_0          dem_0_0                        1
    MARKER000                 'MARKER'                 INTORG
    y0_0          objCOST                       60  cap_0                         30
    MARKER001                 'MARKER'                 INTEND
    inv0_0        objCOST                      0.8
    x0_1          objCOST                      1.5  cap_1                          1
    x0_1          dem_0_1                        1
    MARKER002                 'MARKER'                 INTORG
    y0_1          objCOST                       60  cap_1                         30
    MARKER003                 'MARKER'                 INTEND
    inv0_1        objCOST                      0.8
    x0_2          objCOST                      1.5  cap_2                          1
    x0_2          dem_0_2                        1
    MARKER004                 'MARKER'                 INTORG
    y0_2          objCOST                       60  cap_2                         30
    MARKER005                 'MARKER'                 INTEND
    inv0_2        objCOST                      0.8
    x0_3          objCOST                      1.5  cap_3                          1
    x0_3          dem_0_3                        1
    MARKER006                 'MARKER'                 INTORG
    y0_3          objCOST                       60  cap_3                         30
    MARKER007                 'MARKER'                 INTEND
    inv0_3        objCOST                      0.8
    x0_4          objCOST                      1.5  cap_4                          1
    x0_4          dem_0_4                        1
    MARKER008                 'MARKER'                 INTORG
    y0_4          objCOST                       60  cap_4                         30
    MARKER009                 'MARKER'                 INTEND
    inv0_4        objCOST                      0.8
    x0_5          objCOST                      1.5  cap_5                          1
    x0_5          dem_0_5                        1
    MARKER010                 'MARKER'                 INTORG
    y0_5          objCOST                       60  cap_5                         30
    MARKER011                 'MARKER'                 INTEND
    inv0_5        objCOST                      0.8
    x0_6          objCOST                      1.5  cap_6                          1
    x0_6          dem_0_6                        1
    MARKER012                 'MARKER'                 INTORG
    y0_6          objCOST                       60  cap_6                         30
    MARKER013                 'MARKER'                 INTEND
    inv0_6        objCOST                      0.8
    x0_7          objCOST                      1.5  cap_7                          1
    x0_7          dem_0_7                        1
    MARKER014                 'MARKER'                 INTORG
    y0_7          objCOST                       60  cap_7                         30
    MARKER015                 'MARKER'                 INTEND
    inv0_7        objCOST                      0.8
    x1_0          objCOST                      1.7  cap_0                          1
    x1_0          dem_1_0                        1
    MARKER016                 'MARKER'                 INTORG
    y1_0          objCOST                       68  cap_0                         30
    MARKER017                 'MARKER'                 INTEND
    inv1_0        objCOST                      0.8
    x1_1          objCOST                      1.7  cap_1                          1
    x1_1          dem_1_1                        1
    MARKER018                 'MARKER'                 INTORG
    y1_1          objCOST                       68  cap_1                         30
    MARKER019                 'MARKER'                 INTEND
    inv1_1        objCOST                      0.8
    x1_2          objCOST                      1.7  cap_2                          1
    x1_2          dem_1_2                        1
    MARKER020                 'MARKER'                 INTORG
    y1_2          objCOST                       68  cap_2                         30
    MARKER021                 'MARKER'                 INTEND
    inv1_2        objCOST                      0.8
    x1_3          objCOST                      1.7  cap_3                          1
    x1_3          dem_1_3                        1
    MARKER022                 'MARKER'                 INTORG
    y1_3          objCOST                       68  cap_3                         30
    MARKER023                 'MARKER'                 INTEND
    inv1_3        objCOST                      0.8
    x1_4          objCOST                      1.7  cap_4                          1
    x1_4          dem_1_4                        1
    MARKER024                 'MARKER'                 INTORG
    y1_4          objCOST                       68  cap_4                         30
    MARKER025                 'MARKER'                 INTEND
    inv1_4        objCOST                      0.8
    x1_5          objCOST                      1.7  cap_5                          1
    x1_5          dem_1_5                        1
    MARKER026                 'MARKER'                 INTORG
    y1_5          objCOST                       68  cap_5                         30
    MARKER027                 'MARKER'                 INTEND
    inv1_5        objCOST                      0.8
    x1_6          objCOST                      1.7  cap_6                          1
    x1_6          dem_1_6                        1
    MARKER028                 'MARKER'                 INTORG
    y1_6          objCOST                       68  cap_6                         30
    MARKER029                 'MARKER'                 INTEND
    inv1_6        objCOST                      0.8
    x1_7          objCOST                      1.7  cap_7                          1
    x1_7          dem_1_7                        1
    MARKER030                 'MARKER'                 INTORG
    y1_7          objCOST                       68  cap_7                         30
    MARKER031                 'MARKER'                 INTEND
    inv1_7        objCOST                      0.8
    x2_0          objCOST                      1.9  cap_0                          1
    x2_0          dem_2_0                        1
    MARKER032                 'MARKER'                 INTORG
    y2_0          objCOST                       76  cap_0                         30
    MARKER033                 'MARKER'                 INTEND
    inv2_0        objCOST                      0.8
    x2_1          objCOST                      1.9  cap_1                          1
    x2_1          dem_2_1                        1
    MARKER034                 'MARKER'                 INTORG
    y2_1          objCOST                       76  cap_1                         30
    MARKER035                 'MARKER'                 INTEND
    inv2_1        objCOST                      0.8
    x2_2          objCOST                      1.9  cap_2                          1
    x2_2          dem_2_2                        1
    MARKER036                 'MARKER'                 INTORG
    y2_2          objCOST                       76  cap_2                         30
    MARKER037                 'MARKER'                 INTEND
    inv2_2        objCOST                      0.8
    x2_3          objCOST                      1.9  cap_3                          1
    x2_3          dem_2_3                        1
    MARKER038                 'MARKER'                 INTORG
    y2_3          objCOST                       76  cap_3                         30
    MARKER039                 'MARKER'                 INTEND
    inv2_3        objCOST                      0.8
    x2_4          objCOST                      1.9  cap_4                          1
    x2_4          dem_2_4                        1
    MARKER040                 'MARKER'                 INTORG
    y2_4          objCOST                       76  cap_4                         30
    MARKER041                 'MARKER'                 INTEND
    inv2_4        objCOST                      0.8
    x2_5          objCOST                      1.9  cap_5                          1
    x2_5          dem_2_5                        1
    MARKER042                 'MARKER'                 INTORG
    y2_5          objCOST                       76  cap_5                         30
    MARKER043                 'MARKER'                 INTEND
    inv2_5        objCOST                      0.8
    x2_6          objCOST                      1.9  cap_6                          1
    x2_6          dem_2_6                        1
    MARKER044                 'MARKER'                 INTORG
    y2_6          objCOST                       76  cap_6                         30
    MARKER045                 'MARKER'                 INTEND
    inv2_6        objCOST                      0.8
    x2_7          objCOST                      1.9  cap_7                          1
    x2_7          dem_2_7                        1
    MARKER046                 'MARKER'                 INTORG
    y2_7          objCOST                       76  cap_7                         30
    MARKER047                 'MARKER'                 INTEND
    inv2_7        objCOST                      0.8
    x3_0          objCOST                      2.1  cap_0                          1
    x3_0          dem_3_0                        1
    MARKER048                 'MARKER'                 INTORG
    y3_0          objCOST                       84  cap_0                         30
    MARKER049                 'MARKER'                 INTEND
    inv3_0        objCOST                      0.8
    x3_1          objCOST                      2.1  cap_1                          1
    x3_1          dem_3_1                        1
    MARKER050                 'MARKER'                 INTORG
    y3_1          objCOST                       84  cap_1                         30
    MARKER051                 'MARKER'                 INTEND
    inv3_1        objCOST                      0.8
    x3_2          objCOST                      2.1  cap_2                          1
    x3_2          dem_3_2                        1
    MARKER052                 'MARKER'                 INTORG
    y3_2          objCOST                       84  cap_2                         30
    MARKER053                 'MARKER'                 INTEND
    inv3_2        objCOST                      0.8
    x3_3          objCOST                      2.1  cap_3                          1
    x3_3          dem_3_3                        1
    MARKER054                 'MARKER'                 INTORG
    y3_3          objCOST                       84  cap_3                         30
    MARKER055                 'MARKER'                 INTEND
    inv3_3        objCOST                      0.8
    x3_4          objCOST                      2.1  cap_4                          1
    x3_4          dem_3_4                        1
    MARKER056                 'MARKER'                 INTORG
    y3_4          objCOST                       84  cap_4                         30
    MARKER057                 'MARKER'                 INTEND
    inv3_4        objCOST                      0.8
    x3_5          objCOST                      2.1  cap_5                          1
    x3_5          dem_3_5                        1
    MARKER058                 'MARKER'                 INTORG
    y3_5          objCOST                       84  cap_5                         30
    MARKER059                 'MARKER'                 INTEND
    inv3_5        objCOST                      0.8
    x3_6          objCOST                      2.1  cap_6                          1
    x3_6          dem_3_6                        1
    MARKER060                 'MARKER'                 INTORG
    y3_6          objCOST                       84  cap_6                         30
    MARKER061                 'MARKER'                 INTEND
    inv3_6        objCOST                      0.8
    x3_7          objCOST                      2.1  cap_7                          1
    x3_7          dem_3_7                        1
    MARKER062                 'MARKER'                 INTORG
    y3_7          objCOST                       84  cap_7                         30
    MARKER063                 'MARKER'                 INTEND
    inv3_7        objCOST                      0.8
RHS
    RHS       cap_0                        100
    RHS       cap_1                        100
    RHS       cap_2                        100
    RHS       cap_3                        100
    RHS       cap_4                        100
    RHS       cap_5                        100
    RHS       cap_6                        100
    RHS       cap_7                        100
    RHS       dem_0_0                       10
    RHS       dem_0_1                       14
    RHS       dem_0_2                       18
    RHS       dem_0_3                       22
    RHS       dem_0_4                       26
    RHS       dem_0_5                       10
    RHS       dem_0_6                       14
    RHS       dem_0_7                       18
    RHS       dem_1_0                       14
    RHS       dem_1_1                       18
    RHS       dem_1_2                       22
    RHS       dem_1_3                       26
    RHS       dem_1_4                       10
    RHS       dem_1_5                       14
    RHS       dem_1_6                       18
    RHS       dem_1_7                       22
    RHS       dem_2_0                       18
    RHS       dem_2_1                       22
    RHS       dem_2_2                       26
    RHS       dem_2_3                       10
    RHS       dem_2_4                       14
    RHS       dem_2_5                       18
    RHS       dem_2_6                       22
    RHS       dem_2_7                       26
    RHS       dem_3_0                       22
    RHS       dem_3_1                       26
    RHS       dem_3_2                       10
    RHS       dem_3_3                       14
    RHS       dem_3_4                       18
    RHS       dem_3_5                       22
    RHS       dem_3_6                       26
    RHS       dem_3_7                       10
BOUNDS
 UP BND       y0_0                           1
 UP BND       y0_1                           1
 UP BND       y0_2                           1
 UP BND       y0_3                           1
 UP BND       y0_4                           1
 UP BND       y0_5                           1
 UP BND       y0_6                           1
 UP BND       y0_7                           1
 UP BND       y1_0                           1
 UP BND       y1_1                           1
 UP BND       y1_2                           1
 UP BND       y1_3                           1
 UP BND       y1_4                           1
 UP BND       y1_5                           1
 UP BND       y1_6                           1
 UP BND       y1_7                           1
 UP BND       y2_0                           1
 UP BND       y2_1                           1
 UP BND       y2_2                           1
 UP BND       y2_3                           1
 UP BND       y2_4                           1
 UP BND       y2_5                           1
 UP BND       y2_6                           1
 UP BND       y2_7                           1
 UP BND       y3_0                           1
 UP BND       y3_1                           1
 UP BND       y3_2                           1
 UP BND       y3_3                           1
 UP BND       y3_4                           1
 UP BND       y3_5                           1
 UP BND       y3_6                           1
 UP BND       y3_7                           1
ENDATA
