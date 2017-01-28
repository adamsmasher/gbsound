SECTION "Ch2", BSS[$C000]
SongPtr:	DS 2
SongTimer:	DS 1
SongRate:	DS 1
Ch2Instr:	DS 2

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
		LD [SongTimer], A
		LD HL, Song
		CALL InitSongCtrlCh
		CALL InitCh2
		LD A, L
		LD [SongPtr], A
		LD A, H
		LD [SongPtr+1], A
		RET

InitSongCtrlCh:	LD A, [HLI]			; volume config
		LDH [$24], A
		LD A, [HLI]
		LD [SongRate], A
		RET

InitCh2:	LD A, [HLI]			; default duty
		LDH [$16], A
		LD A, [HLI]			; default envelope
		LDH [$17], A
		LD A, [HLI]
		LD [Ch2Instr], A
		LD A, [HLI]
		LD [Ch2Instr+1], A
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

;;; each frame, add a "rate" var to a timer variable
;;; if the timer wraps around, play the next note
;;; this allows tempos as fast as notes 60 per second
;;; or as slow as 1 every ~4s (256 frames at 60fps)
UpdateSndFrame:	LD A, [SongRate]
		LD HL, SongTimer
		ADD [HL]
		LD [HL], A
		RET NC
;;; fall thru to...
		CALL TickSongCtrl
		;; CALL TickCh1
		CALL TickCh2
		;; CALL TickCh3
		;; CALL TickCh4
		RET

TickSongCtrl:	CALL PopOpcode
;;; 0 is a NOP
		AND A
		RET Z
		DEC A
		LD H, CmdTblSongCtrl >> 8
		LD L, A
		LD A, [HLI]
		LD H, [HL]
		LD L, A
		JP [HL]

TickCh2:	CALL PopOpcode
;;; 0 is a NOP
		AND A
		JP Z, Ch2NOP
		DEC A
;;; all commands have their LSB set to 0 (i.e., the first is 2, then 4...)
;;; after shifting everything down by 1, that means they now have a 1 in the LSB
		BIT 0,A
		JP NZ, Ch2Cmd
		JP Ch2Note

;;; If no event occurred, just apply the current instrument
Ch2NOP:		LD HL, Ch2Instr
		LD A, [HLI]
		LD H, [HL]
		LD L, A
		LD A, [HLI]
	;; 0 indicates the end of the instrument - don't do anything more (ever)
		AND A
		RET Z
		LD B, A
		LD A, L
		LD [Ch2Instr], A
		LD A, H
		LD [Ch2Instr+1], A
	;; 1 indicates the end of a frame - end the loop
		DEC B
		RET Z
	;; Push Ch2NOP onto the stack as a return address, so when the effect proc ends,
	;; we run the loop again
		LD HL, Ch2NOP
		PUSH HL
	;; invoke the effect proc
		LD H, InstrTblCh2 >> 8
		LD L, B
		LD A, [HLI]
		LD H, [HL]
		LD L, A
		JP [HL]

;;; assumes A = Note
Ch2Note:	LD H, FreqTable >> 8
		LD L, A
		LD A, [HLI]
		LDH [$18], A
		LD A, [HL]
	;; TODO: you could probably set this bit on each entry in the freq table (hmm, probably not - once you add instrument pitch...)
		SET 7,A		; restart sound
		LDH [$19], A
		RET

;;; Assumes A = Cmd + 1
Ch2Cmd:		DEC A
		LD H, CmdTblCh2 >> 8
		LD L, A
		LD A, [HLI]
		LD H, [HL]
		LD L, A
		JP [HL]

Ch2KeyOff:	XOR A
		LDH [$17], A
		RET

Ch2DutyCmdPre:	LD C, $16
		LD A, [C]
		AND $3F
		RET

Ch2SetDutyLo:	CALL Ch2DutyCmdPre
		LD [C], A
		RET

Ch2SetDuty25:	CALL Ch2DutyCmdPre
		OR $40
		LD [C], A
		RET

Ch2SetDuty50:	CALL Ch2DutyCmdPre
		LD A, [C]
		AND $3F

Ch2SetDuty75:	LD C, $16
		LD A, [C]
		OR $C0
		LD [C], A
		RET

Ch2SetSndLen:	CALL PopOpcode
		LD B, A
		LD C, $16
		LD A, [C]
		AND $C0
		OR B
		LD [C], A
		RET

Ch2VolInstr:	LD HL, Ch2Instr
		CALL PopInstr
		LD [$17], A
		RET

;;; assume HL = Instrument pointer
;;; returns the next instrument byte in A and increments the instrument pointer
PopInstr:	LD D, H
		LD E, L
		LD A, [HLI]
		LD H, [HL]
		LD L, A
		LD B, A
		LD A, L
		LD [DE], A
		INC E
		LD A, H
		LD [DE], A
		LD A, B
		RET

;;; returns the next opcode byte in A and increments the song pointer
PopOpcode:	LD HL, SongPtr
		LD A, [HLI]
		LD H, [HL]
		LD L, A
		LD A, [HLI]
		LD B, A
		LD A, L
		LD [SongPtr], A
		LD A, H
		LD [SongPtr+1], A
		LD A, B
		RET

SongStop:	XOR A
		LD [SongRate], A
		RET

SongSetRate:	CALL PopOpcode
		LD [SongRate], A
		RET

SECTION "Song", HOME
Song:		DB $77		; master volume config
		DB $40		; rate
		DB $80		; ch2 duty/sound len (50%/no sound length specified)
		DB $F0		; ch2 envelope (max volume/decrease/disabled)
		DW .instr1	; instrument
	;; song starts
		DB 3, 0		; stop, keyoff
	;; instruments
.instr1:	DB 0		; no effect

SECTION "FreqTable", HOME[$7A00]
FreqTable:	DW 44, 156, 262, 363, 457, 547, 631, 710, 786, 854, 923, 986
		DW 1046, 1102, 1155, 1205, 1253, 1297, 1339, 1379, 1417, 1452, 1486, 1517
		DW 1546, 1575, 1602, 1627, 1650, 1673, 1694, 1714, 1732, 1750, 1767, 1783
		DW 1798, 1812, 1825, 1837, 1849, 1860, 1871, 1881, 1890, 1899, 1907, 1915
		DW 1923, 1930, 1936, 1943, 1949, 1954, 1959, 1964, 1969, 1974, 1978, 1982
		DW 1985, 1988, 1992, 1995, 1998, 2001, 2004, 2006, 2009, 2011, 2013, 2015

SECTION "InstrTable2", HOME[$7900]
InstrTblCh2:	DW Ch2VolInstr

SECTION "CmdTable2", HOME[$7D00]
CmdTblCh2:	DW Ch2KeyOff
		DW Ch2SetDutyLo
		DW Ch2SetDuty25
		DW Ch2SetDuty50
		DW Ch2SetDuty75
		DW Ch2SetSndLen

CmdTblSongCtrl:	DW SongSetRate
		DW SongStop
