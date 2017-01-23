SECTION "Ch2", BSS[$C000]
Ch2Ptr:		DS 2
OpcodeCh2:	DS 1
Ch2Timer:	DS 1
Ch2Rate:	DS 1

SECTION "VBlank", HOME[$40]
		JP VBlank

SECTION "Boot", HOME[$0100]
		NOP
		JP Start

SECTION "Header", HOME[$0104]
		REPT $4C
		DB $F0
		ENDR

SECTION "Start", HOME[$0150]
Start:		DI				; disable interrupts
		LD HL, $E000			; init stack from $E000 down
		LD SP, HL
		CALL InitInterrupts
		CALL InitSndEngine
.loop:		HALT
		JR .loop
	
InitInterrupts:	LD A, 1				; enable vblank
		LDH [$FF], A
		XOR A				; clear pending IRQs
		LDH [$0F], A
		EI
		RET

InitSndEngine:	XOR A
		LD [Ch2Timer], A
		LD HL, Song
		LD A, [HLI]			; volume config
		LDH [$24], A
		LD A, [HLI]
		LD [Ch2Rate], A
		LD A, [HLI]			; default duty
		LDH [$16], A
		LD A, [HLI]			; default envelope
		LDH [$17], A
		LD A, L
		LD [Ch2Ptr], A
		LD A, H
		LD [Ch2Ptr+1], A
		RET

VBlank:		PUSH AF
		PUSH BC
		PUSH DE
		PUSH HL
		CALL UpdateSndFrame
		POP HL
		POP DE
		POP BC
		POP AF
		RETI

UpdateSndFrame:	CALL UpdateCh2Frame
		RET

;;; each frame, add a "rate" var to a timer variable
;;; if the timer wraps around, play the next note
;;; this allows tempos as fast as notes 60 per second
;;; or as slow as 1 every ~4s (256 frames at 60fps)
UpdateCh2Frame:	LD A, [Ch2Rate]
		LD HL, Ch2Timer
		ADD [HL]
		LD [HL], A
		RET NC
;;; fall thru to...
UpdateCh2Tick:	CALL PopOpcodeCh2
		JP DoCh2Opcode

PopOpcodeCh2:	LD HL, Ch2Ptr
		LD A, [HLI]
		LD H, [HL]
		LD L, A
		LD A, [HLI]
		LD [OpcodeCh2], A
		LD A, L
		LD [Ch2Ptr], A
		LD A, H
		LD [Ch2Ptr+1], A
		RET

;;; all commands have their LSB set to 1
DoCh2Opcode:	LD A, [OpcodeCh2]
		BIT 0,A
		JR NZ, .cmd
.note:		LD H, FreqTable >> 8
		LD L, A
		LD A, [HLI]
		LDH [$18], A
		LD A, [HL]
		LDH [$19], A
		RET
.cmd:		DEC A
		LD H, CmdTable2 >> 8
		LD L, A
		LD A, [HLI]
		LD H, [HL]
		LD L, A
		JP [HL]

Ch2KeyOff:	XOR A
		LDH [$17], A
		RET

Ch2Stop:	XOR A
		LD [Ch2Rate], A
		RET

Ch2SetRate:	LD HL, Ch2Ptr
		LD A, [HLI]
		LD [Ch2Rate], A
		LD A, L
		LD [Ch2Ptr], A
		LD A, H
		LD [Ch2Ptr+1], A
		RET

SECTION "Song", HOME
Song:		DB $77		; master volume config
		DB $40		; rate
		DB $80		; duty
		DB $F0		; max volume, no envelope
	;; song starts
		DB 3		; stop

SECTION "FreqTable", HOME[$7A00]
FreqTable:	DW 44, 156, 262, 363, 457, 547, 631, 710, 786, 854, 923, 986
		DW 1046, 1102, 1155, 1205, 1253, 1297, 1339, 1379, 1417, 1452, 1486, 1517
		DW 1546, 1575, 1602, 1627, 1650, 1673, 1694, 1714, 1732, 1750, 1767, 1783
		DW 1798, 1812, 1825, 1837, 1849, 1860, 1871, 1881, 1890, 1899, 1907, 1915
		DW 1923, 1930, 1936, 1943, 1949, 1954, 1959, 1964, 1969, 1974, 1978, 1982
		DW 1985, 1988, 1992, 1995, 1998, 2001, 2004, 2006, 2009, 2011, 2013, 2015

SECTION "CmdTable2", HOME[$7D00]
CmdTable2:	DW Ch2KeyOff
		DW Ch2Stop
		DW Ch2SetRate
