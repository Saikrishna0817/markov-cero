* Units: generation, nodal demand and line flow MW; voltage angle radians;
* objective synthetic currency with quadratic generation cost.
NAME          CAPITANESCU_DC_OPF
ROWS
 N  objCOST
 L  genmax_0
 L  genmax_1
 L  genmax_2
 E  balance_0
 E  balance_1
 E  balance_2
 E  balance_3
 E  balance_4
 E  balance_5
 E  balance_6
 E  balance_7
 E  balance_8
 L  line_0
 L  linep_0
 L  line_1
 L  linep_1
 L  line_2
 L  linep_2
 L  line_3
 L  linep_3
 L  line_4
 L  linep_4
 L  line_5
 L  linep_5
 L  line_6
 L  linep_6
 L  line_7
 L  linep_7
 L  line_8
 L  linep_8
COLUMNS
    pg0           objCOST                       10  genmax_0                       1
    pg1           objCOST                       10  genmax_1                       1
    pg1           balance_1                      1
    pg2           objCOST                       10  genmax_2                       1
    pg2           balance_2                      1
    th1           linep_0                      100  line_2                       100
    th1           balance_1                   -100  balance_1                    100
    th1           balance_1                   -100
    th2           linep_2                      100  line_3                       100
    th2           balance_2                   -100  balance_2                    100
    th2           balance_2                   -100
    th3           linep_1                      100  line_4                       100
    th3           line_6                       100  balance_3                    100
    th3           balance_3                   -100  balance_3                   -100
    th4           linep_4                      100  line_5                       100
    th4           line_7                       100  balance_4                    100
    th4           balance_4                   -100  balance_4                   -100
    th5           linep_3                      100  linep_5                      100
    th5           line_8                       100  balance_5                    100
    th5           balance_5                    100  balance_5                   -100
    th6           linep_6                      100  balance_6                    100
    th7           linep_7                      100  balance_7                    100
    th8           linep_8                      100  balance_8                    100
RHS
    RHS       genmax_0                     120
    RHS       genmax_1                     120
    RHS       genmax_2                     120
    RHS       balance_4                     60
    RHS       balance_5                     45
    RHS       balance_7                     30
    RHS       line_0                        80
    RHS       linep_0                       80
    RHS       line_1                        80
    RHS       linep_1                       80
    RHS       line_2                        80
    RHS       linep_2                       80
    RHS       line_3                        80
    RHS       linep_3                       80
    RHS       line_4                        80
    RHS       linep_4                       80
    RHS       line_5                        80
    RHS       linep_5                       80
    RHS       line_6                        80
    RHS       linep_6                       80
    RHS       line_7                        80
    RHS       linep_7                       80
    RHS       line_8                        80
    RHS       linep_8                       80
BOUNDS
 UP BND       pg0                          120
 UP BND       pg1                          120
 UP BND       pg2                          120
 UP BND       th1                          0.6
 LO BND       th1                         -0.6
 UP BND       th2                          0.6
 LO BND       th2                         -0.6
 UP BND       th3                          0.6
 LO BND       th3                         -0.6
 UP BND       th4                          0.6
 LO BND       th4                         -0.6
 UP BND       th5                          0.6
 LO BND       th5                         -0.6
 UP BND       th6                          0.6
 LO BND       th6                         -0.6
 UP BND       th7                          0.6
 LO BND       th7                         -0.6
 UP BND       th8                          0.6
 LO BND       th8                         -0.6
QUADOBJ
    pg0           pg0                         0.02
    pg1           pg1                         0.02
    pg2           pg2                         0.02
ENDATA
