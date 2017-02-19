;;; TODO: implement sweep
;;; TODO: implement wave RAM
;;; TODO: implement arpeggios
;;; TODO: implement effects?
;;; TODO: document/test

SECTION "MusicVars", BSS[$C000]
;;; pointer into the opcode stream
SongPtr:	DS 2	;; musn't cross a page
;;; the song is described as "sequence", a list of 8-bit numbers.
;;; Each entry in the sequence is called a "frame".
;;; Each frame contains an 8-bit pattern number (0, 2, 4, ...)
;;; Pattern numbers are used to look up an entry in the pattern table
;;; The pattern table contains pointers into the opcode stream
SeqPtr:		DS 1
;;; Boolean flag to indicate if we've hit the end of a pattern and need to load the next
EndOfPat:	DS 1
;;; Each frame, SongTimer is incremented by SongRate; if it overflows, we run a music tick
SongRate:	DS 1
SongTimer:	DS 1 ;; must follow SongRate
;;; Used to keep track of the current channel being updated
ChNum:		DS 1
;;; please keep this and ChRegBase together
;;; Used to keep track of the register base of the current channel being updated
;;; On the GB, we can look at each channel being controlled by 5 registers
;;; Channel 1 - FF10 - FF14
;;; Channel 2 - FF15 - FF19
;;; Channel 3 - FF1A - FF1E
;;; Channel 4 - FF1F - FF23
;;; While they don't perfectly map onto each other (e.g. FF15 and FF1F are unused,
;;; volume envelopes work differently for each etc), they're similar enough that
;;; we can treat them uniformly.
ChRegBase:	DS 1

;;; An instrument is a stream of special opcodes that update a channel's output
;;; parameters on a per note basis
SECTION "ChInstrBases", BSS[$C100]
;;; Pointer to the beginning of each channel's instrument;
;;; the corresponding ChInstrPtr is reset to this when the note changes
ChInstrBases:	DS 4 * 2
;;; MUST BE TOGETHER
SECTION "ChInstrPtrs", BSS[$C200]
ChInstrPtrs:	DS 4 * 2
;;; MUST BE TOGETHER
SECTION "ChInstrMarkers", BSS[$C300]
;;; Instruments can contain an unbounded loop;
;;; these contain pointers to each channel's instrument's loop point
ChInstrMarkers:	DS 4 * 2

SECTION "ChFreqs", BSS[$C400]
;;; Stores the current frequency being output on each channel
ChFreqs:	DS 4 * 2

SECTION "ChCurNotes", BSS[$C500]
;;; Stores the current note (offset into the note table) being output on each channel
ChCurNotes:	DS 4
;;; MUST BE TOGETHER
SECTION "ChOctaves", BSS[$C600]
;;; stores an offset that's added to note lookup; used to change keys or octaves
ChOctaves:	DS 4
;;; MUST BE TOGETHER
SECTION "ChPitchAdjs", BSS[$C700]
;;; allows for fine tuning the pitch (for slides, etc)
;;; TODO: is this used?
;;; TODO: should this be 16-bits per channel?
ChPitchAdjs:	DS 4

SECTION "GbSound", ROM0
;;; Song initialization:
;;; HL is mutated across function calls and consistently points
;;; to the location in the song data we're working through
InitSndEngine::	XOR A
		LD [SongTimer], A
		LD HL, Song
		CALL InitSongCtrlCh
		;; TODO: make this a loop
		LD A, 0
		LD [ChNum], A
		LD A, $10
		LD [ChRegBase], A
		CALL InitCh
		LD A, 1
		LD [ChNum], A
		LD A, $15
		LD [ChRegBase], A
		CALL InitCh
		CALL InitSeqPtr
		RET

InitSongCtrlCh:	LD A, [HLI]			; volume config
		LDH [$24], A
		LD A, [HLI]			; channel select
		LDH [$25], A
		LD A, [HLI]			; song rate
		LD [SongRate], A
		RET

;;; The Instruments table contains a list of pointers to the start of each instrument
;;; This just copies the requested instrument number's pointer into the instrument base
;;; for the channel
;;; A - instrument number (even numbers only)
ChSetInstr:	PUSH HL
		LD H, Instruments >> 8
		LD L, A
		LD D, ChInstrBases >> 8
		LD A, [ChNum]
		ADD A
		LD E, A
		LD A, [HLI]
		LD [DE], A
		INC E
		LD A, [HL]
		LD [DE], A
		POP HL
		RET

InitCh:		LD A, [HLI]	; initial instrument
		CALL ChSetInstr
		XOR A
		LD D, ChOctaves >> 8
		LD A, [ChNum]	
		LD E, A
		LD [DE], A
		INC D		; pitch adjusts
		LD [DE], A
		RET

InitSeqPtr:	XOR A
		LD [SeqPtr], A
		RET
		
;;; each frame, add a "rate" var to a timer variable
;;; if the timer wraps around, play the next note
;;; this allows tempos as fast as notes 60 per second
;;; or as slow as 1 every ~4s (256 frames at 60fps)
UpdateSndFrame::LD HL, EndOfPat	; test the current pattern over flag
		LD A, [HL]
		AND A
		JR Z, .chkTick
		XOR A		; if so, clear that flag and move to the next
		LD [HL], A	; and then update as normal
		CALL NextPat
.chkTick:	LD HL, SongRate
		LD A, [HLI]
		ADD [HL]	; update SongTimer
		LD [HL], A
		CALL C, RunSndTick
	;; every frame, regardless if the engine ticks, we update the instruments
		CALL UpdateInstrs
	;; finally, we tell the hardware to refresh by writing the frequencies
	;; TODO: just embed the code for UpdateHardware here
		JP UpdateHardware

RunSndTick:	CALL TickSongCtrl
		LD HL, ChNum
		XOR A
		LD [HLI], A
		LD A, $10
		LD [HL], A
.loop:		
	;; note how when the loop starts, HL MUST = ChRegNum!
		LD A, $5
		ADD [HL]
		LD [HLD], A
		INC [HL]
		CALL TickCh
		LD HL, ChNum
		LD A, [HLI]	; this makes HL = ChRegNum for the loop!
		CP 3
		JR NZ, .loop
		RET

NextPat:
	;; first, figure out what pattern to play
		LD HL, SeqPtr
		LD A, [HL]
		INC [HL]
		LD H, Sequence >> 8
		LD L, A
		LD L, [HL]
	;; now load a pointer to that pattern, pulled from the PatternTable, into SongPtr
		LD H, PatternTable >> 8
		LD A, [HLI]
		LD [SongPtr], A
		LD A, [HL]
		LD [SongPtr+1], A
		RET

TickSongCtrl:	CALL PopOpcode
	;; 0 is a NOP
		AND A
		RET Z
	;; SongOpcodes are then numbered 1, 3, ... in the stream
		DEC A
		LD H, CmdTblSongCtrl >> 8
		LD L, A
		LD A, [HLI]
		LD H, [HL]
		LD L, A
		JP [HL]

TickCh:		CALL PopOpcode
	;; 0 is a NOP
		AND A
		RET Z
		DEC A
	;; all commands have their LSB set to 0 (i.e., the first is 2, then 4...)
	;; after shifting everything down by 1, that means they now have a 1 in the LSB
		BIT 0,A
		JP NZ, ChCmd
		LD H, ChCurNotes >> 8
		LD A, [ChNum]
		LD L, A
		LD [HL], A
		JP ChNote

UpdateInstrs:	LD HL, ChNum
		XOR A
		LD [HLI], A
		LD A, $10
		LD [HL], A
.loop:
	;; note how when the loop starts, HL MUST = ChRegNum!
		LD A, $5
		ADD [HL]
		LD [HLD], A
		INC [HL]
		CALL ApplyInstrCh
		LD HL, ChNum
		LD A, [HLI]	; this makes HL = ChRegNum for the loop!
		CP 3
		JR NZ, .loop
		RET

		CALL ApplyInstrCh

ApplyInstrCh:	LD H, ChInstrPtrs >> 8
		LD A, [ChNum]
		ADD A
		LD L, A
		LD D, H
		LD E, L
		LD A, [HLI]
		LD H, [HL]
		LD L, A
		LD A, [HLI]
	;; 0 indicates the end of the instrument - don't do anything more (ever)
		AND A
		RET Z
		LD B, A
	;; write the new pointer back
		LD A, L
		LD [DE], A
		INC E
		LD [DE], A
	;; 1 indicates the end of a frame - end the loop
		DEC B
		RET Z
	;; Push ApplyInstr onto the stack as a return address, so when the effect proc ends,
	;; we run the loop again
		LD HL, ApplyInstrCh
		PUSH HL
	;; invoke the effect proc
		DEC B
		LD H, InstrTblCh >> 8
		LD L, B
		LD A, [HLI]
		LD H, [HL]
		LD L, A
		JP [HL]

UpdateHardware:
	;; TODO: if the registers aren't "dirty", don't restart the note
		LD B, 4
		LD C, $13	; freq register 1
		LD HL, ChFreqs
.loop:		LD A, [HLI]
		LD [C], A
		INC C		; freq reg 2
		LD A, [HLI]
		SET 7,A		; TODO: uh now that we have to set this every time we write maybe do this once on write?
		LD [C], A
		LD A, C
		ADD 4		; next freq register 1
		DEC B
		JR NZ, .loop
		RET

;;; Called at the end of a note to reset the instrument
ChRstInstr:	LD A, [ChNum]
		ADD A
		LD H, ChInstrBases >> 8
		LD L, A
		LD D, ChInstrPtrs >> 8
		LD E, A
		LD A, [HLI]
		LD [DE], A
		INC E
		LD A, [HL]
		LD [DE], A
		RET

ChNote:		CALL ChRstInstr
		LD A, [ChNum]
		LD B, A
		LD H, ChCurNotes >> 8
		LD L, A
		LD A, [HL]
		INC H		; octaves
		ADD [HL]
	;; look up current note in frequency table
		LD H, FreqTable >> 8
		LD L, A
		LD A, [HLI]
		LD C, [HL]
		LD H, ChFreqs >> 8
		LD L, B
		SLA L
		LD [HLI], A
		LD [HL], C
		RET

;;; Assumes A = Cmd + 1
ChCmd:		LD HL, TickCh	; put this on the stack so that we'll try to run the next op after
		PUSH HL
		DEC A
		LD H, CmdTblCh >> 8
		LD L, A
		LD A, [HLI]
		LD H, [HL]
		LD L, A
		JP [HL]

NullInstr:	DB 0

ChKeyOff:	LD A, [ChRegBase]
		ADD 2		; volume register
		LD C, A
		XOR A
		LD [C], A
		LD H, ChInstrPtrs >> 8
		LD A, [ChNum]
		ADD A
		LD L, A
		LD A, NullInstr & $00FF
		LD [HLI], A
		LD A, NullInstr >> 8
		LD [HL], A
		RET

ChSetSndLen:	CALL PopOpcode
		LD B, A
		LD A, [ChRegBase]
		LD C, A
		INC C		; sound length reg
		LD A, [C]
		AND $C0
		OR B
		LD [C], A
		RET

ChVolInstr:	LD A, [ChRegBase]
		ADD 2		; volume
		LD C, A
		LD H, ChInstrPtrs >> 8
		LD A, [ChNum]
		ADD A
		LD L, A
		CALL PopInstr
		LD [C], A
		RET

ChPitchInstr:	LD H, ChInstrPtrs >> 8
		LD A, [ChNum]
		ADD A
		LD L, A
		CALL PopInstr
		LD B, A
		LD A, [ChRegBase]
		ADD 3		; freq reg 1
		LD H, ChFreqs >> 8
		LD A, [ChNum]
		ADD A
		LD L, A
		LD A, [HL]
		ADD B
		LD [HLI], A
		LD [C], A
		RET NC
		INC C		; freq reg 2
		INC A
		LD [HL], A
		LD [C], A
		RET

ChHPitchInstr:	LD A, [ChRegBase]
		ADD 3		; freq reg 1
		LD C, A
		LD H, ChInstrPtrs >> 8
		LD A, [ChNum]
		ADD A
		LD L, A
		CALL PopInstr
		LD D, A
		SWAP A
		AND $F0
		LD B, A
		LD H, ChFreqs >> 8
		LD A, [ChNum]
		ADD A
		LD L, A
		LD A, [HL]
		BIT 7,D
		JR NZ, .neg
		ADD B
		JR .hi
.neg:		SUB B
.hi:		LD [HLI], A
		LD [C], A
		SRA D
		SRA D
		SRA D
		SRA D
		LD A, [HL]
		ADD D
		LD [HL], A
		INC C		; freq reg 2
		LD [C], A
		RET

;;; B - duty bits (OR'd onto register)
ChDutyInstr:	LD A, [ChRegBase]
		LD C, A
		INC C		; duty reg
		LD A, [C]
		AND $3F
		OR B
		LD [C], A
		RET

ChDutyInstrLo:	LD B, 0
		JR ChDutyInstr

ChDutyInstr25:	LD B, $40
		JR ChDutyInstr
	
ChDutyInstr50:	LD B, $80
		JR ChDutyInstr

ChDutyInstr75:	LD B, $C0
		JR ChDutyInstr

ChInstrMark:	LD H, ChInstrPtrs >> 8
		LD A, [ChNum]
		ADD A
		LD L, A
		LD A, [HLI]
		LD B, [HL]
		INC H		; ChInstrMarkers
		DEC L
		LD [HLI], A
		LD [HL], B
		RET

ChInstrLoop:
	;; copy the current instrument marker into the instrument pointer
		LD H, ChInstrMarkers >> 8
		LD A, [ChNum]
		ADD A
		LD L, A
		LD A, [HLI]
		LD B, [HL]
		DEC H		; ChInstrPtrs
		DEC L
		LD [HLI], A
		LD [HL], B
		RET

;;; B - amount to add to the octave
ChOctaveCmd:	LD H, ChOctaves >> 8
		LD A, [ChNum]
		LD L, A
		LD A, [HL]
		ADD B
		LD [HL], A
		RET

ChOctaveUp:	LD B, 12
		JR ChOctaveCmd

ChOctaveDown:	LD B, -12
		JR ChOctaveCmd

ChSetInstrCmd:	CALL PopOpcode
		CALL ChSetInstr
		JP ChRstInstr

;;; assume HL = Instrument pointer
;;; returns the next instrument byte in A and increments the instrument pointer
PopInstr:	LD D, H
		LD E, L
		LD A, [HLI]
		LD H, [HL]
		LD L, A
		LD A, [HLI]
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
	;; a nasty bit of trickery - instead of returning and updating the sound channels
	;; scrap the return address at the top of the stack, which escapes the interrupt handler
		ADD SP, 2
		RET

SongEndOfPat:	LD A, 1
		LD [EndOfPat], A
		RET

SongJmpFrame:	CALL PopOpcode
		LD [SeqPtr], A
	;; another, similar nasty bit of trickery - instead of returning and updating the sound channels
	;; scrap the return address and jump back to the beginning of the frame update code
	;; so that the first song control byte isn't ignored
		ADD SP, 2
		LD A, 1
		LD [EndOfPat], A
		JP RunSndTick

SongSetRate:	CALL PopOpcode
		LD [SongRate], A
		RET

;;; TODO: move/copy this into RAM
SECTION "Song", HOME[$6000]
PatternTable:	DW Pattern1
Song:		DB $77		; master volume config
		DB $FF		; all channels on in both speakers
		DB $40		; rate
		DB 0		; ch1 instrument
Pattern1:
		DB 0, 49
		DB 0, 0
		DB 0, 0
		DB 0, 0
		DB 0, 61
		DB 0, 0
		DB 0, 0
		DB 0, 0
		DB 0, 73
		DB 0, 0
		DB 0, 0
		DB 0, 0
		DB 7, 0		; goto frame 0

;;; TODO: move/copy this into RAM
SECTION "Instruments", HOME[$6100]
Instruments:	DW .instr1
.instr1:	DB 2, $F0, 1
		DB 2, $00, 0

;;; TODO: move/copy this into RAM
SECTION "Sequence", HOME[$6200]
Sequence:	DB 0

SECTION "FreqTable", HOME[$7A00]
FreqTable:	DW 44, 156, 262, 363, 457, 547, 631, 710, 786, 854, 923, 986
		DW 1046, 1102, 1155, 1205, 1253, 1297, 1339, 1379, 1417, 1452, 1486, 1517
		DW 1546, 1575, 1602, 1627, 1650, 1673, 1694, 1714, 1732, 1750, 1767, 1783
		DW 1798, 1812, 1825, 1837, 1849, 1860, 1871, 1881, 1890, 1899, 1907, 1915
		DW 1923, 1930, 1936, 1943, 1949, 1954, 1959, 1964, 1969, 1974, 1978, 1982
		DW 1985, 1988, 1992, 1995, 1998, 2001, 2004, 2006, 2009, 2011, 2013, 2015

SECTION "InstrTable", HOME[$7900]
InstrTblCh:	DW ChVolInstr
		DW ChInstrMark
		DW ChInstrLoop
		DW ChPitchInstr
		DW ChHPitchInstr
		DW ChDutyInstrLo
		DW ChDutyInstr25
		DW ChDutyInstr50
		DW ChDutyInstr75

SECTION "CmdTable", HOME[$7D00]
CmdTblCh:	DW ChKeyOff
		DW ChSetSndLen
		DW ChOctaveUp
		DW ChOctaveDown
		DW ChSetInstrCmd

SECTION "CmdTblSongCtrl", HOME[$7700]
CmdTblSongCtrl:	DW SongSetRate
		DW SongStop
		DW SongEndOfPat
		DW SongJmpFrame
