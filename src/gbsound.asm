SECTION "MusicVars", BSS[$C000]
;;; pointer into the opcode stream
SongPtr:	DS 2	;; musn't cross a page
;;; the song is described as "sequence", a list of 8-bit numbers.
;;; Each entry in the sequence is called a "frame".
;;; Each frame contains an 8-bit pattern number
;;; Pattern numbers are used to look up an entry in the pattern table
;;; The pattern table contains pointers into the opcode stream
;; TODO: make this only an offset into the sequence, as implied by SongJmpFrame
SeqPtr:		DS 2	;; musn't cross a page
;;; Boolean flag to indicate if we've hit the end of a pattern and need to load the next
EndOfPat:	DS 1
;;; Each frame, SongTimer is incremented by SongRate; if it overflows, we run a music tick
SongRate:	DS 1
SongTimer:	DS 1 ;; must follow SongRate
;;; Used to keep track of the current channel being updated
ChNum:		DS 1
;;; Used to keep track of the register base of the current channel being updated
;;; On the GB, we can look at each channel being controlled by 5 registers
;;; Channel 1 - FF10 - FF14
;;; Channel 2 - FF15 - FF19
;;; Channel 3 - FF1A - FF1E
;;; Channel 4 - FF1F - FF23
;;; While they don't perfectly map onto each other (e.g. FF15 and FF1F are unused,
;;; volume envelopes work differently for each etc), they're similarly enough that
;;; we can treat them uniformly.
ChRegBase:	DS 1

;;; An instrument is a stream of special opcodes that update a channel's output
;;; parameters on a per note basis
SECTION "ChInstrBases", BSS[$C100]
;;; Pointer to the beginning of each channels instrument;
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

SECTION "ChRealDuties", BSS[$C800]
;;; instruments can change the duty cycle; this stores the duty cycle that
;;; we should reset to on each note
ChRealDuties:	DS 4
;;; MUST BE TOGETHER
SECTION "ChRealEnvs", BSS[$C900]
;;; instruments can change the volume (envelope); this stores the volume that
;;; we should reset to on each ntoe
ChRealEnvs:	DS 4

;;; when a VBlank interrupt is fired, the CPU immediately jumps to $0040
;;; but this area is too packed (with other interrupt handlers)
;;; to write our code here, so just jump to the real handler
SECTION "VBlank", HOME[$40]
		JP VBlank

;;; when the firmware hands control over to our program, the PC is at $0100
;;; but the header comes immediately after (at $0104), so there's no room for
;;; code here; we just immediately jump to the real main program.
;;; It's unclear why you NOP first, but tradition is that you do.
SECTION "Boot", HOME[$0100]
		NOP
		JP Start

;;; We leave the header blank in the code; the rgbfix program - as part of the build
;;; procedure - fills this in.
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
	;; the vblank interrupt is where all the action happens, so just HALT to wait for it
.loop:		HALT
		JR .loop
	
InitInterrupts:	LD A, 1				; enable vblank
		LDH [$FF], A
		XOR A				; clear pending IRQs
		LDH [$0F], A
		EI				; enable interrupts
		RET

;;; Song initialization:
;;; HL is mutated across function calls and consistently points
;;; to the location in the song data we're working through
InitSndEngine:	XOR A
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

InitCh:		LD D, ChRealDuties >> 8
		LD A, [ChNum]
		LD E, A
		LD A, [HLI]	; default duty
		LD [DE], A
		INC D		; envelopes
		LD A, [HLI]	; default envelope
		LD [DE], A
		LD A, [HLI]	; initial instrument
		CALL ChSetInstr
		XOR A
		LD D, ChOctaves >> 8
		LD A, [ChNum]	
		LD E, A
		LD [DE], A
		INC D		; pitch adjusts
		LD [DE], A
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
UpdateSndFrame:	LD HL, EndOfPat	; test the current pattern over flag
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
		RET NC
;;; fall thru to...
RunSndFrame:	CALL TickSongCtrl
	;; TODO: make me a loop
		LD A, 0
		LD [ChNum], A
		LD A, $10
		CALL TickCh
		LD A, 1
		LD A, $15
		LD [ChNum], A
		CALL TickCh
		;; CALL TickCh3
		;; CALL TickCh4
		RET

;;; TODO: if we limit ourselves to 256 frames, we can make SeqPtr an 8-bit offset
NextPat:	LD HL, SeqPtr
		LD A, [HLI]
		LD H, [HL]
		LD A, [HLI]
		LD HL, SeqPtr
		INC [HL]
		JR NZ, LoadPat
		INC L
		INC [HL]
	;; fall thru to
		
;;; A = Pattern to load
LoadPat:	LD H, PatternTable >> 8
		LD L, A
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
		JP Z, ChNOP
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

;;; If no event occurred, just apply the current instrument
;;; The frequency register still needs to be rewritten, though
;;; TODO: if the registers aren't "dirty", don't restart the note
ChNOP:		CALL ApplyInstrCh
TriggerCh:	LD H, ChFreqs >> 8
		LD A, [ChNum]
		ADD A
		LD L, A
		LD A, [ChRegBase]
		ADD 3		; freq reg 1
		LD C, A
		LD A, [HLI]
		LD [C], A
		INC C		; freq reg 2
		LD A, [HL]
		SET 7,A		; TODO: uh now that we have to set this every time we write maybe do this once on write?
		LD [C], A
		RET

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

;;; Called at the end of a note to reset the instrument
ChRstInstr:	LD H, ChInstrBases >> 8
		LD D, ChInstrPtrs >> 8
		LD A, [ChNum]
		LD B, A
		ADD A
		LD L, A
		LD E, A
		LD A, [HLI]
		LD [DE], A
		LD A, [HL]
		INC E
		LD [DE], A
		LD H, ChRealDuties >> 8
		LD L, B
		LD A, [ChRegBase]
		LD C, A
		INC C		; duty reg
		LD A, [HL]
		LD [C], A
		INC H		; vol envelope
		LD A, [HL]
		INC C		; vol env reg
		LD [C], A
		RET

ChNote:		CALL ChRstInstr
		LD H, ChCurNotes >> 8
		LD A, [ChNum]
		LD B, A
		LD L, A
		LD A, [HL]
		INC H		; octaves
		ADD [HL]
		LD H, FreqTable >> 8
		LD L, A
		LD A, [HLI]
		LD C, [HL]
		LD H, ChFreqs >> 8
		LD L, B
		SLA L
		LD [HLI], A
		LD [HL], C
		CALL ApplyInstrCh
		JP TriggerCh

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

ChDutyCmd:	LD A, [ChRegBase]
		ADD 1		; duty reg
		LD C, A
		LD A, [C]
		AND $3F
		OR B
		LD [C], A
		RET

ChSetDutyLo:	LD B, 0
		JR ChDutyCmd

ChSetDuty25:	LD B, $40
		JR ChDutyCmd

ChSetDuty50:	LD B, $80
		JR ChDutyCmd

ChSetDuty75:	LD B, $C0
		JR ChDutyCmd

ChSetSndLen:	CALL PopOpcode
		LD B, A
		LD A, [ChRegBase]
		ADD 1		; sound length reg
		LD C, A
		LD A, [C]
		AND $C0
		OR B
		LD [C], A
		RET

ChSetEnvCmd:	LD H, ChRealEnvs >> 8
		LD A, [ChNum]
		LD L, A
		LD A, [ChRegBase]
		ADD 2		; volume envelope reg
		LD C, A
		LD A, [C]
		AND $07
		OR B
		LD [C], A
		LD [HL], A
		RET

ChSetEnv0:	LD B, $00
		JR ChSetEnvCmd

ChSetEnv1:	LD B, $01
		JR ChSetEnvCmd

ChSetEnv2:	LD B, $02
		JR ChSetEnvCmd

ChSetEnv3:	LD B, $03
		JR ChSetEnvCmd

ChSetEnv4:	LD B, $04
		JR ChSetEnvCmd

ChSetEnv5:	LD B, $05
		JR ChSetEnvCmd

ChSetEnv6:	LD B, $06
		JR ChSetEnvCmd

ChSetEnv7:	LD B, $07
		JR ChSetEnvCmd

ChSetEnvDec:	LD H, $FF
		LD A, [ChRegBase]
		ADD 2		; volume env reg
		LD L, A
		RES 3,[HL]
		RET

ChSetEnvInc:	LD H, $FF
		LD A, [ChRegBase]
		ADD 2		; volume env reg
		LD L, A
		SET 3,[HL]
		LD H, ChRealEnvs >> 8
		LD A, [ChNum]
		LD L, A
		SET 3,[HL]
		RET

ChSetEnvVol:	CALL PopOpcode
		LD B, A
		LD A, [ChRegBase]
		ADD 2		; volume envelope reg
		LD C, A
		LD A, [C]
		AND $0F
		OR B
		LD [C], A
		LD H, ChRealEnvs >> 8
		LD A, [ChNum]
		LD L, A
		LD A, [HL]
		AND $0F
		OR B
		LD [HL], A
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

ChDutyInstr:	LD A, [ChRegBase]
		ADD 1		; duty reg
		LD C, A
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

ChInstrLoop:	LD H, ChInstrMarkers >> 8
		LD A, [ChNum]
		ADD A
		LD L, A
		LD A, [HLI]
		LD B, [HL]
		DEC H		; ChInstrPtrs
		DEC L
		LD [HLI], A
		LD [HL], B
		JP ChNOP

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
		JP RunSndFrame

SongSetRate:	CALL PopOpcode
		LD [SongRate], A
		RET

SECTION "Song", HOME[$6000]
PatternTable:	DW Pattern1
Song:		DB $77		; master volume config
		DB $FF		; all channels on in both speakers
		DB $40		; rate
		DB $80		; ch2 duty/sound len (50%/no sound length specified)
		DB $F0		; ch2 envelope (max volume/decrease/disabled)
		DB 0		; instrument
		DW Sequence
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

SECTION "Instruments", HOME[$6100]
Instruments:	DW .instr1
.instr1:	DB 2, $F0, 1
		DB 2, $00, 0

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
		DW ChSetDutyLo
		DW ChSetDuty25
		DW ChSetDuty50
		DW ChSetDuty75
		DW ChSetSndLen
		DW ChSetEnvVol
		DW ChSetEnvInc
		DW ChSetEnvDec
		DW ChSetEnv0
		DW ChSetEnv1
		DW ChSetEnv2
		DW ChSetEnv3
		DW ChSetEnv4
		DW ChSetEnv5
		DW ChSetEnv6
		DW ChSetEnv7
		DW ChOctaveUp
		DW ChOctaveDown
		DW ChSetInstrCmd

SECTION "CmdTblSongCtrl", HOME[$7700]
CmdTblSongCtrl:	DW SongSetRate
		DW SongStop
		DW SongEndOfPat
		DW SongJmpFrame
