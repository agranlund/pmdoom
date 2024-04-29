#if defined(__GNUC__) && (defined(__m68k__) && !defined(__mcoldfire__))

	.globl	_FixedMul020
	.globl	_FixedDiv2020

	.chip	68020

_FixedMul020:
	move.l 4(%sp),%d0
	move.l 8(%sp),%d1

	mulsl	%d1,%d1:%d0
	movew	%d1,%d0
	swap	%d0

	rts

_FixedDiv2020:
	move.l %d2,-(%sp)
	move.l 8(%sp),%d0

	movel	%d0,d2
	swap	%d0
	movew	%d0,d1
	extl	d1
	clrw	%d0
	divsl	12(%sp),d1:%d0

	move.l (%sp)+,%d2
	rts

#endif
