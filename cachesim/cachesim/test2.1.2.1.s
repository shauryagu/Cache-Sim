    lw    0    1    z
    sw    0    2    o
b   add   1    2    1
    sw    0    3    tar
    beq   3    1    d
    beq   0    0    b
d   halt
z   .fill 0
o   .fill 1
tar .fill 5
