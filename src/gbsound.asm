;;; TODO: implement arpeggios
;;; TODO: implement effects?
;;; TODO: document/test

SECTION "MusicVars", BSS
;;; where the song starts in ROM
SongBase:	DS 2
;;; pointer into the opcode stream
SongPtr:	DS 2	;; musn't cross a page
;;; Pattern numbers are used to look up an entry in the pattern table
;;; The pattern table contains pointers into the opcode stream
NextPattern:	DS 1
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
;;; These values are in BYTES
InstrTblLen:	DS 1
PatTblLen:	DS 1

;;; An instrument is a stream of special opcodes that update a channel's output
;;; parameters on a per note basis
SECTION "ChInstrBases", BSS[$C000]
;;; Pointer to the beginning of each channel's instrument;
;;; the corresponding ChInstrPtr is reset to this when the note changes
ChInstrBases:	DS 4 * 2
;;; MUST BE TOGETHER
SECTION "ChInstrPtrs", BSS[$C100]
ChInstrPtrs:	DS 4 * 2
;;; MUST BE TOGETHER
SECTION "ChInstrMarkers", BSS[$C200]
;;; Instruments can contain an unbounded loop;
;;; these contain pointers to each channel's instrument's loop point
ChInstrMarkers:	DS 4 * 2

SECTION "ChFreqs", BSS[$C300]
;;; Stores the current frequency being output on each channel
ChFreqs:	DS 4 * 2

SECTION "ChCurNotes", BSS[$C400]
;;; Stores the current note (offset into the note table) being output on each channel
ChCurNotes:	DS 4
;;; MUST BE TOGETHER
SECTION "ChOctaves", BSS[$C500]
;;; stores an offset that's added to note lookup; used to change keys or octaves
ChOctaves:	DS 4
;;; MUST BE TOGETHER
SECTION "ChPitchAdjs", BSS[$C600]
;;; allows for fine tuning the pitch (for slides, etc)
;;; TODO: is this used?
ChPitchAdjs:	DS 4 * 2

SECTION "PatternTable", BSS[$C700]
PatternTable:	DS 128*2

SECTION "Instruments", BSS[$C800]
InstrumentTbl:	DS 128*2

SECTION "Waves", BSS[$C900]
Waves:		DS 256

SECTION "SongData", BSS[$CA00]
SongData:	DS $600

SECTION "GbSound", ROM0

;;; Song initialization:
;;; Call with HL = Song
InitSndEngine:: 
	;; set this to $FF, so it ticks as soon as we start playing
		LD A, $FF
		LD [SongTimer], A
	;; NextPattern = 0, the first pattern
		INC A
		LD [NextPattern], A
	;; Set the EndOfPat flag to 1, so that we'll load the NextPattern (the first one) once we tick
		INC A
		LD [EndOfPat], A
		CALL LoadSong
		CALL ClearFreqs
		CALL ClearEffects
		CALL ClearSndRegs
		JP InitInstrs

;;; Call with HL = Song
;;; HL is mutated across function calls and consistently points
;;; to the location in the song data we're working through
LoadSong:	LD A, L
		LD [SongBase], A
		LD A, H
		LD [SongBase+1], A
		CALL LoadSongCtrlCh
		CALL LoadWaves
		CALL LoadPatternTbl
		CALL LoadInstrTbl
		CALL OffsetPatTbl
		JP OffsetInstrTbl

LoadSongCtrlCh:	LD A, [HLI]			; volume config
		LDH [$24], A
		LD A, [HLI]			; channel select
		LDH [$25], A
		LD A, [HLI]			; song rate
		LD [SongRate], A
		RET

LoadWaves:	LD A, [HLI]			; how many bytes of wave data to load
		LD B, A
		LD DE, Waves
.loop:		LD A, [HLI]
		LD [DE], A
		INC E
		DEC B
		JR NZ, .loop
		RET

LoadPatternTbl:	LD DE, PatternTable
		LD A, [HLI]			; how many pattern table bytes to load
		LD B, A
		LD [PatTblLen], A
.loop:		LD A, [HLI]
		LD [DE], A
		INC E
		DEC B
		JR NZ, .loop
		RET

LoadInstrTbl:	LD DE, InstrumentTbl
		LD A, [HLI]
		LD B, A
		LD [InstrTblLen], A
.loop:		LD A, [HLI]
		LD [DE], A
		INC E
		DEC B
		JR NZ, .loop
		RET

;;; B = number of BYTES to update
;;; HL - pointer to table
OffsetTbl:	SRL B
		LD A, [SongBase]
		LD D, A
		LD A, [SongBase+1]
		LD E, A
.loop:		LD A, D
		ADD [HL]
		LD [HLI], A
		JR NC, .nc
		INC [HL]
.nc:		LD A, E
		ADD [HL]
		LD [HLI], A
		DEC B
		JR NZ, .loop
		RET

OffsetPatTbl:	LD A, [PatTblLen]
		LD B, A
		LD HL, PatternTable
		JP OffsetTbl

OffsetInstrTbl:	LD A, [InstrTblLen]
		LD B, A
		LD HL, InstrumentTbl
		JP OffsetTbl

;;; The Instruments table contains a list of pointers to the start of each instrument
;;; This just copies the requested instrument number's pointer into the instrument base
;;; for the channel
;;; A - instrument number (even numbers only)
ChSetInstr:
	;; get the pointer from the table
		LD H, InstrumentTbl >> 8
		LD L, A
		LD A, [HLI]
		LD D, [HL]
		LD E, A
	;; copy the pointer into the base
		LD H, ChInstrBases >> 8
		LD A, [ChNum]
		ADD A
		LD L, A
		LD A, E
		LD [HLI], A
		LD [HL], D
		RET

ClearEffects:   LD HL, ChOctaves
		XOR A
		LD B, 4 
.loop1:		LD [HLI], A
		DEC B
		JR NZ, .loop1
		INC H		; move to pitch adjusts
		LD L, A
		LD B, 8
.loop2:		LD [HLI], A
		DEC B
		JR NZ, .loop2
		RET

ClearFreqs:	LD HL, ChFreqs
		XOR A
		LD B, 8
.loop:		LD [HLI], A
		DEC B
		JR NZ, .loop
		RET

ClearSndRegs:	XOR A
		LD C, $10
		LD B, 5 * 4	; number of sound registers
.loop:		LD [C], A
		DEC B
		JR NZ, .loop
		RET

InitNextPat:	XOR A
		LD [NextPattern], A
		RET

;;; make all the instruments just point to the dummy
;;; "NullInstr"
InitInstrs:	LD HL, ChInstrBases
		LD B, 4
		LD C, NullInstr & $00FF
		LD D, NullInstr >> 8
.loop1:		LD A, C
		LD [HLI], A
		LD A, D
		LD [HLI], A
		DEC B
		JR NZ, .loop1
	;; copy these into the pointers, too
		LD HL, ChInstrPtrs
		LD B, 4
.loop2:		LD A, C
		LD [HLI], A
		LD A, D
		LD [HLI], A
		DEC B
		JR NZ, .loop2
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
		CALL PlayNextPat
.chkTick:	LD HL, SongRate
		LD A, [HLI]
		ADD [HL]	; update SongTimer
		LD [HL], A
		CALL C, RunSndTick
	;; every frame, regardless if the engine ticks, we update the instruments
		CALL UpdateInstrs
	;; finally, we tell the hardware to refresh by writing the frequencies
		JP UpdateHardware

RunSndTick:	CALL TickSongCtrl
		LD HL, ChNum
		XOR A
		LD [HLI], A
	;; set reg base ($FF10)
		LD A, $10
		LD [HL], A
.loop:		CALL TickCh
		LD HL, ChRegBase
		LD A, [HL]
		ADD 5
		LD [HLD], A
		LD A, [HL]
		INC A
		LD [HL], A
		CP 4
		JR NZ, .loop
		RET

PlayNextPat:
	;; first, figure out what pattern to play
		LD HL, NextPattern
		LD A, [HL]
		INC [HL]	; patterns go by twos
		INC [HL]
	;; now load a pointer to that pattern, pulled from the PatternTable
		LD H, PatternTable >> 8
		LD L, A
		LD A, [HLI]
		LD H, [HL]
		LD L, A
	;; TODO: compress/decompress
	;; copy it into the song data
		LD A, [HLI]
		LD C, A
		LD A, [HLI]
		LD B, A
		LD DE, SongData
.loop:		LD A, [HLI]
		LD [DE], A
		INC DE
		DEC BC
		LD A, B
		OR C
		JR NZ, .loop
	;; finally - point the SongPtr to the beginning of the new data
		LD HL, SongPtr
		XOR A
		LD [HLI], A
		LD A, SongData >> 8
		LD [HL], A
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
		LD B, A
		LD H, ChCurNotes >> 8
		LD A, [ChNum]
		LD L, A
		LD [HL], B
		JP ChNote

UpdateInstrs:	LD HL, ChNum
		XOR A
		LD [HLI], A
		LD A, $10
		LD [HL], A
.loop:		CALL ApplyInstrCh
		LD HL, ChRegBase
		LD A, [HL]
		ADD $5
		LD [HLD], A
	;; chnum
		LD A, [HL]
		INC A
		LD [HL], A
		CP 4
		JR NZ, .loop
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
		LD A, H
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
		LD C, A
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

ChInstrSetWave:	LD H, ChInstrPtrs >> 8
		LD A, [ChNum]
		ADD A
		LD L, A
		CALL PopInstr
		JP LoadWave

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

ChSetWaveCmd:	CALL PopOpcode
		JP LoadWave

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
		LD HL, ChNum
		LD [HLI], A
		LD A, $10
		LD [ChRegBase], A
.loop:		CALL ChKeyOff
		LD HL, ChRegBase
		LD A, [HL]
		ADD 5
		LD [HLD], A
		INC [HL]
		LD A, [HL]
		CP 4
		JR NZ, .loop
		XOR A
		LD [SongRate], A
	;; a nasty bit of trickery - instead of returning and updating the sound channels
	;; scrap the return address at the top of the stack, which escapes the interrupt handler
		ADD SP, 4
		RET

SongEndOfPat:	LD A, 1
		LD [EndOfPat], A
		RET

SongJmpFrame:	CALL PopOpcode
		LD [NextPattern], A
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

;;; A - index into the wave table
LoadWave:	LD H, Waves >> 8
		LD L, A
		LD C, $30
		LD B, 16
.loop:		LD A, [HLI]
		LD [C], A
		INC C
		DEC B
		JR NZ, .loop
		RET

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
		DW ChInstrSetWave

SECTION "CmdTable", HOME[$7D00]
CmdTblCh:	DW ChKeyOff
		DW ChSetSndLen
		DW ChOctaveUp
		DW ChOctaveDown
		DW ChSetInstrCmd
		DW ChSetWaveCmd

SECTION "CmdTblSongCtrl", HOME[$7700]
CmdTblSongCtrl:	DW SongSetRate
		DW SongStop
		DW SongEndOfPat
		DW SongJmpFrame
