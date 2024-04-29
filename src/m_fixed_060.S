#if defined(__GNUC__) && (defined(__m68k__) && !defined(__mcoldfire__))

	.globl	_FixedMul060
	.globl	_FixedDiv2060

	.chip	68060

_FixedMul060:
	fmove.l	4(%sp),fp0
	fmul.l	8(%sp),fp0
	fmul.s	&0f0.0000152587890625,fp0
	fmove.l	fp0,%d0

	rts

_FixedDiv2060:
	fmove.l	4(%sp),fp0
	fmove.l	8(%sp),fp1
	fdiv.x		fp1,fp0
	fmul.s		&0f65536.0,fp0
	fmove.l	fp0,d0

	rts

#endif
