.macro SVC_BEGIN name
	.section .text.\name, "ax", %progbits
	.global \name
	.type \name, %function
	.align 2
	.cfi_startproc
\name:
.endm

.macro SVC_END
	.cfi_endproc
.endm

SVC_BEGIN svcQueryProcessMemory
	str x1, [sp, #-16]!
	svc 0x76
	ldr x2, [sp], #16
	str w1, [x2]
	ret
SVC_END
