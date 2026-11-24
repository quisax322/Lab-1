; Write a zero-terminated string into the DATA region.
LOADI A, 72
STORE [0x800], A
LOADI A, 69
STORE [0x801], A
LOADI A, 76
STORE [0x802], A
STORE [0x803], A
LOADI A, 79
STORE [0x804], A
LOADI A, 0
STORE [0x805], A
OUTS 0x800
HALT
