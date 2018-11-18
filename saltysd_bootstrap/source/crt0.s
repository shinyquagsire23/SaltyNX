.section ".crt0","ax"
.global _start

_start:
    b startup
    .word 0

.org _start+0x8
startup:

    // save lr
    mov  x27, x30

    // get aslr base
    bl   +4
    sub  x28, x30, #0x88

    // context ptr and main thread handle
    mov  x25, x0
    mov  x26, x1

    // clear .bss
    ldr x0, =__bss_start__
    ldr x1, =__bss_end__
    sub  x1, x1, x0  // calculate size
    add  x1, x1, #7  // round up to 8
    bic  x1, x1, #7

bss_loop: 
    str  xzr, [x0], #8
    subs x1, x1, #8
    bne  bss_loop

    // store stack pointer
    mov  x1, sp
    ldr x0, =__stack_top
    str  x1, [x0]

    // initialize system
    mov  x0, x25
    mov  x1, x26
    mov  x2, x27
    bl   __rel_init

    // call entrypoint
    ldr x0, =__system_argc // argc
    ldr  w0, [x0]
    ldr x1, =__system_argv // argv
    ldr  x1, [x1]
    ldr x30, =__rel_exit
    b    main

.global __nx_exit
.type   __nx_exit, %function
__nx_exit:
    // restore stack pointer
    ldr x8, =__stack_top
    ldr  x8, [x8]
    mov  sp, x8

    // jump back to loader
    br   x1

.pool
