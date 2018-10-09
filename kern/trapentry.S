#include <inc/mmu.h>
#include <inc/memlayout.h>
#include <inc/trap.h>

.globl alltraps
alltraps:
	# Build trap frame.
	pushl %ds
	pushl %es
	pushal

	# Set up data segments.
	movw $(GD_KD), %ax
	movw %ax, %ds
	movw %ax, %es

	# Call trap(tf), where tf=%esp
	pushl %esp
	call trap
	addl $4, %esp

# Return falls through to trapret...
.globl trapret
	trapret:
	popal
	popl %gs
	popl %fs
	popl %es
	popl %ds
	addl $0x8, %esp  # trapno and errcode
	iret

# pushes extra $0 when CPU alert will not be used
.altmacro
.macro add_cpu_alert_filler count
.if ((count-8)&&(count-10)&&(count-11)&&(count-12)&&(count-13)&&(count-14)&&(count-17))
	push $0
.endif
.endm

# handlers init macro
.altmacro
.macro create_handlers count
	.if \count
		create_handlers %(count-1)
	.endif
	.globl vector\count
        .type vector\count, @function
        .align 2
        vector\count:
        add_cpu_alert_filler %(count)
	pushl $(count)
        jmp alltraps
.endm

# vectors init macro
.altmacro
.macro create_vector count
	.if \count
		create_vector %(count-1)
	.endif
	.long vector\count
.endm

.text
create_handlers 48

# vector table
.data
.globl vectors
vectors:
create_vector 48
