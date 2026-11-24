LOADI A, 6 ; n
CALL fib
OUTN
HALT

; A holds the argument and result. B is caller-saved, H is unused.
fib:   LOADI B, 0
       CMP A, B
       JZ ret_a
       LOADI B, 1
       CMP A, B
       JZ ret_a
       DEC A
       PUSH A   ; save n-1 while computing fib(n-1)
       CALL fib
       MOV B, A
       POP A
       DEC A    ; n-2
       PUSH B   ; save fib(n-1) while computing fib(n-2)
       CALL fib
       POP B
       ADD A, B
ret_a: RET
