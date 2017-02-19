IMPORT InitSndEngine, UpdateSndFrame

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
