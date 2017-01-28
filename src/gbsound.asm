SECTION "Ch2", BSS[$C000]
SongPtr:	DS 2
SeqPtr:		DS 2
EndOfPat:	DS 1
SongTimer:	DS 1
SongRate:	DS 1
Ch2InstrPtr:	DS 2
Ch2InstrBase:	DS 2
Ch2Octave:	DS 1
Ch2InstrMarker:	DS 2
Ch2PitchAdj:	DS 1
Ch2RealEnv:	DS 1

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
		CALL InitSeqPtr
		RET

InitSongCtrlCh:	LD A, [HLI]			; volume config
		LDH [$24], A
		LD A, [HLI]
		LD [SongRate], A
		RET

InitCh2:	LD A, [HLI]			; default duty
		LDH [$16], A
		LD A, [HLI]			; default envelope
		LD [Ch2RealEnv], A
		LD A, [HLI]
		LD [Ch2InstrBase], A
		LD A, [HLI]
		LD [Ch2InstrBase+1], A
		XOR A
		LD [Ch2Octave], A
		LD [Ch2PitchAdj], A
		RET

InitSeqPtr:	LD A, [HLI]
		LD [SeqPtr], A
		LD A, [HL]
		LD [SeqPtr+1], A
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
UpdateSndFrame:	LD HL, EndOfPat
		LD A, [HL]
		AND A
		JR Z, .chkTick
		XOR A
		LD [HL], A
		CALL NextPat
.chkTick:	LD A, [SongRate]
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

NextPat:	LD HL, SeqPtr
		LD A, [HLI]
		LD H, [HL]
		LD A, [HLI]
		LD HL, SeqPtr
		INC [HL]
		JR NZ, .nz
		INC L
		INC [HL]
.nz:		LD H, PatternTable >> 8
		LD L, A
		LD A, [HLI]
		LD [SongPtr], A
		LD A, [HL]
		LD [SongPtr+1], A
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
Ch2NOP:		LD HL, Ch2InstrPtr
		LD A, [HLI]
		LD H, [HL]
		LD L, A
		LD A, [HLI]
	;; 0 indicates the end of the instrument - don't do anything more (ever)
		AND A
		RET Z
		LD B, A
		LD A, L
		LD [Ch2InstrPtr], A
		LD A, H
		LD [Ch2InstrPtr+1], A
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

Ch2RstInstr:	LD HL, Ch2InstrBase
		LD A, [HLI]
		LD [Ch2InstrPtr], A
		LD A, [HL]
		LD [Ch2InstrPtr+1], A
		LD A, [Ch2RealEnv]
		LD [$17], A
		RET

;;; assumes A = Note
Ch2Note:	CALL Ch2RstInstr
		LD HL, Ch2Octave
		ADD [HL]
		LD H, FreqTable >> 8
		LD L, A
		LD A, [HLI]
		LDH [$18], A
		LD A, [HL]
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


NullInstr:	DB 0

Ch2KeyOff:	XOR A
		LDH [$17], A
		LD HL, Ch2InstrPtr
		LD A, NullInstr & $00FF
		LD [HLI], A
		LD A, NullInstr >> 8
		LD [HL], A
		RET

Ch2DutyCmd:	LD C, $16
		LD A, [C]
		AND $3F
		OR B
		LD [C], A
		RET

Ch2SetDutyLo:	LD B, 0
		JR Ch2DutyCmd

Ch2SetDuty25:   LD B, $40
		JR Ch2DutyCmd

Ch2SetDuty50:	LD B, $80
		JR Ch2DutyCmd

Ch2SetDuty75:	LD B, $C0
		JR Ch2DutyCmd

Ch2SetSndLen:	CALL PopOpcode
		LD B, A
		LD C, $16
		LD A, [C]
		AND $C0
		OR B
		LD [C], A
		RET

Ch2SetEnvCmd:	LD C, $17
		LD A, [C]
		AND $07
		OR B
		LD [C], A
		LD [Ch2RealEnv], A
		RET

Ch2SetEnv0:	LD B, $00
		JR Ch2SetEnvCmd

Ch2SetEnv1:	LD B, $01
		JR Ch2SetEnvCmd

Ch2SetEnv2:	LD B, $02
		JR Ch2SetEnvCmd

Ch2SetEnv3:	LD B, $03
		JR Ch2SetEnvCmd

Ch2SetEnv4:	LD B, $04
		JR Ch2SetEnvCmd

Ch2SetEnv5:	LD B, $05
		JR Ch2SetEnvCmd

Ch2SetEnv6:	LD B, $06
		JR Ch2SetEnvCmd

Ch2SetEnv7:	LD B, $07
		JR Ch2SetEnvCmd

Ch2SetEnvDec:	LD HL, $FF17
		RES 3,[HL]
		RET

Ch2SetEnvInc:	LD HL, $FF17
		SET 3,[HL]
		LD HL, Ch2RealEnv
		SET 3,[HL]
		RET

Ch2SetEnvVol:	CALL PopOpcode
		LD B, A
		LD C, $17
		LD A, [C]
		AND $0F
		OR B
		LD [C], A
		LD HL, Ch2RealEnv
		LD A, [HL]
		AND $0F
		OR B
		LD [HL], A
		RET

Ch2VolInstr:	LD HL, Ch2InstrPtr
		CALL PopInstr
		LD [$17], A
		RET

Ch2InstrMark:	LD HL, Ch2InstrPtr
		LD A, [HLI]
		LD [Ch2InstrMarker], A
		LD A, [HL]
		LD [Ch2InstrMarker+1], A
		RET

Ch2InstrLoop:	LD HL, Ch2InstrMarker
		LD A, [HLI]
		LD [Ch2InstrPtr], A
		LD A, [HL]
		LD [Ch2InstrPtr+1], A
		JP Ch2NOP

Ch2OctaveCmd:	LD HL, Ch2Octave
		LD A, [HL]
		ADD B
		LD [HL], A
		RET

Ch2OctaveUp:	LD B, 12
		JR Ch2OctaveCmd

Ch2OctaveDown:	LD B, -12
		JR Ch2OctaveCmd

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
		LD HL, SongPtr
		INC [HL]
		RET NZ
		INC L
		INC [HL]
		RET

SongStop:	XOR A
		LD [SongRate], A
		RET

SongEndOfPat:	LD A, 1
		LD [EndOfPat], A
		RET

SongSetRate:	CALL PopOpcode
		LD [SongRate], A
		RET

SECTION "Song", HOME[$6000]
PatternTable:	DW Pattern1
Song:		DB $77		; master volume config
		DB $40		; rate
		DB $80		; ch2 duty/sound len (50%/no sound length specified)
		DB $F0		; ch2 envelope (max volume/decrease/disabled)
		DW Instr1	; instrument
		DW Sequence
Sequence:	DB 0
Pattern1:
		DB 3, 0		; stop, keyoff
	;; instruments
Instr1:	DB 0		; no effect

SECTION "FreqTable", HOME[$7A00]
FreqTable:	DW 44, 156, 262, 363, 457, 547, 631, 710, 786, 854, 923, 986
		DW 1046, 1102, 1155, 1205, 1253, 1297, 1339, 1379, 1417, 1452, 1486, 1517
		DW 1546, 1575, 1602, 1627, 1650, 1673, 1694, 1714, 1732, 1750, 1767, 1783
		DW 1798, 1812, 1825, 1837, 1849, 1860, 1871, 1881, 1890, 1899, 1907, 1915
		DW 1923, 1930, 1936, 1943, 1949, 1954, 1959, 1964, 1969, 1974, 1978, 1982
		DW 1985, 1988, 1992, 1995, 1998, 2001, 2004, 2006, 2009, 2011, 2013, 2015

SECTION "InstrTable2", HOME[$7900]
InstrTblCh2:	DW Ch2VolInstr
		DW Ch2InstrMark
		DW Ch2InstrLoop

SECTION "CmdTable2", HOME[$7D00]
CmdTblCh2:	DW Ch2KeyOff
		DW Ch2SetDutyLo
		DW Ch2SetDuty25
		DW Ch2SetDuty50
		DW Ch2SetDuty75
		DW Ch2SetSndLen
		DW Ch2SetEnvVol
		DW Ch2SetEnvInc
		DW Ch2SetEnvDec
		DW Ch2SetEnv0
		DW Ch2SetEnv1
		DW Ch2SetEnv2
		DW Ch2SetEnv3
		DW Ch2SetEnv4
		DW Ch2SetEnv5
		DW Ch2SetEnv6
		DW Ch2SetEnv7
		DW Ch2OctaveUp
		DW Ch2OctaveDown

SECTION "CmdTblSongCtrl", HOME[$7700]
CmdTblSongCtrl:	DW SongSetRate
		DW SongStop
		DW SongEndOfPat
