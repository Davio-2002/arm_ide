    .global _start

    .data
message:
    .ascii "Hello, AArch64!\n"
    .set length, . - message

    .text
_start:
    mov     x0, #1
    ldr     x1, =message
    mov     x2, #length
    mov     x8, #64
    svc     #0

    mov     x0, #0
    mov     x8, #93
    svc     #0
