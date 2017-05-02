SECTION "Decompress", HOME

;; assumes HL contains src
;;         DE contains dest
Decompress::
.chunk		LD A, [HLI]			; get flags
		LD B, A				; put flags in B
REPT 8
		LD A, [HLI]			; get next byte
		BIT 0, B			; is this flag hi?
		JR NZ, .literal\@		; if so, we have a literal
		AND A				; otherwise, check for EOF
		RET Z
		LD C, A				; store length to copy
		LD A, [HLI]			; get -offset
		PUSH HL				; backup read location
		ADD E				; newLoc = E + (-offset)
		LD L, A
		LD H, D
		JR C, .copy\@
		DEC H
.copy\@		LD A, [HLI]
		LD [DE], A
		INC DE
		DEC C
		JR NZ, .copy\@
		POP HL				; get pointer to compressed data
		JR .nextbyte\@
.literal\@	LD [DE], A
		INC DE
.nextbyte\@	SRL B				; move to next flag
ENDR
		JP .chunk
