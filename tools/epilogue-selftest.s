| epilogue-selftest.s -- proof that tools/check_epilogues.py still knows
| the m68k-amigaos-gcc miscompilation when it sees it (GSheets,
| 2026-09-15). `gmake epilogues` assembles this and fails the build if
| the check passes it.
|
| _GoodLocal is the shape the check must NOT report: a local struct, a
| saved register, and an epilogue that pops that register and then frees
| the local -- correct, and indistinguishable by shape alone.
| _BadEpilogue is the miscompilation: arguments are pushed for the call
| and the epilogue pops THEM into d2/d3, discarding the saved values.

	.text
	.globl _GoodLocal
_GoodLocal:
	lea sp@(-16),sp
	movel d2,sp@-
	movel a0,d2
	lea sp@(4),a0
	jsr _helper
	moveal d2,a0
	movel sp@+,d2
	lea sp@(16),sp
	rts

	.globl _BadEpilogue
_BadEpilogue:
	movel d2,sp@-
	movel d3,sp@-
	movel a0@(4),sp@-
	movel a0@(8),sp@-
	jsr _helper
	movel sp@+,d2
	movel sp@+,d3
	addql #8,sp
	rts
