(TFT plugin resume test: air moves only, spindle stays off)
(Set work zero in the air: jog to the middle of the bed, Zero XY, then)
(raise Z at least 20 mm above anything and Zero Z. Absolute moves:)
(five 30 mm squares at Z-2, lifting to Z5 between them, about 1 minute.)
(Stop it partway, then Files, this file, From line.)
G21 G90 G94 G17 G54
M5
G0 Z5
G0 X0 Y0
(square 1)
G1 Z-2 F300
G1 X30 F600
G1 Y30
G1 X0
G1 Y0
G0 Z5
(square 2)
G0 X5 Y5
G1 Z-2 F300
G1 X35 F600
G1 Y35
G1 X5
G1 Y5
G0 Z5
(square 3)
G0 X10 Y10
G1 Z-2 F300
G1 X40 F600
G1 Y40
G1 X10
G1 Y10
G0 Z5
(square 4)
G0 X15 Y15
G1 Z-2 F300
G1 X45 F600
G1 Y45
G1 X15
G1 Y15
G0 Z5
(square 5)
G0 X20 Y20
G1 Z-2 F300
G1 X50 F600
G1 Y50
G1 X20
G1 Y20
G0 Z5
G0 X0 Y0
M30
