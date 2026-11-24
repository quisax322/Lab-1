; Set up the Lab 04 example. The byte at 0x808 is outside the search range.
LOADI A, 0x42
STORE [0x800], A
STORE [0x801], A
STORE [0x803], A
STORE [0x804], A
STORE [0x805], A
STORE [0x807], A
LOADI A, 0x41
STORE [0x808], A
LOADI A, 0x41 ; in-range matches (change to 0x42 to test the miss)
STORE [0x802], A
STORE [0x806], A

; Search [0x800, 0x808) for 0x41. Print the first index, or 255 if absent.
search: LOADH H, 0x800
loop:   LOADI B, 0x41
        LOAD A, [H]
        CMP A, B
        JZ found
        INCH
        HLOW
        LOADI B, 8
        CMP A, B
        JZ missing
        JMP loop
found:  HLOW
        OUTN
        HALT
missing: LOADI A, 0xFF
         OUTN
         HALT
