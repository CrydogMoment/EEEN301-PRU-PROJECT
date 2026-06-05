	.cdecls "main.c"
	.clink
	.global START
	.asg 32, PRU0_R31_VEC_VALID 		; allows notification of program completion
	.asg 3, PRU_EVTOUT_0 			; the event number that is sent back
	.asg 2000, TRIGGER_COUNT ;
	.asg 100000, SAMPLE_DELAY_1MS
	.asg 50, DEBOUNCE
	.asg 2508, MULTI_CONST

; Using register 0 for temporary storage
START:
	SET r30, r30.t7                ; set pru output gpio for whole program
	LDI32 r0, 0x00000000           ; Load the number of samples
	LBBO &r1, r0, 0, 4
	LDI32 r0, 0x00000004           ; Load the sample delay
	LBBO &r2, r0, 0, 4

MAINLOOP:
	LDI32 r0, TRIGGER_COUNT        ; store length of the trigger pulse delay
	SET r30, r30.t5                ; set the trigger high

TRIGGERING:                        ; delay for 10us
	SUB r0, r0, 1                  ; decrement loop counter
	QBNE TRIGGERING, r0, 0         ; repeat loop unless zero
	CLR r30, r30.t5                ; 10us over, set trigger low
	; clear the counter and wait until the echo goes high
	LDI32 r3, 0 				   ; r3 will store the echo pulse width
    WBS r31, 3                     ; wait until the echo goes high

COUNTING:
	LDI r9, DEBOUNCE               ; reset debounce counter
	ADD r3, r3, 1                  ; increment the echo counter by 1
	QBBS COUNTING, r31, 3          ; loop if the echo is still high

; "debounce", make sure we only count the echo as finished if it stays low
COUNT_DEBOUNCE:
SUB r9, r9, 1                  ; debounce countdown
	ADD r3, r3, 1                  ; still increase echo counter in debounce mode
	QBBS COUNTING, r31, 3          ; go back to normal counting if echo is high again
	QBNE COUNT_DEBOUNCE, r9, 0     ; continue loop until debounce is over

	LDI r9, MULTI_CONST 	; set up multiplication registers
	LDI r8, 0		;bit logic register
	LDI r7, 0		;where output will go

MULTI_LOOP:
	QBEQ DONE_MULTI, r3, 0
	AND r8, r3, 1
	QBEQ SKIP_ADD, r8, 0
	ADD r7, r7, r9

SKIP_ADD:
	LSR r3,r3,1
	LSL r9,r9,1
	QBA MULTI_LOOP

DONE_MULTI:
	LDI r3, 0
	ADD r3, r3, r7

	; Write the value to shared memory
	LDI32 r0, 0
	LSL r0, r1, 2                  ; multiply iteration by 4 for array pos
	ADD r0, r0, 4                  ; constant offset (don't overwrite trigger count & sample delay)
	SBBO &r3, r0, 0, 4             ; store echo count at this address

	; one more sample iteration has taken place
	SUB r1, r1, 1                  ; take 1 away from the number of iterations
	MOV r0, r2                     ; load delay between samples

SAMPLEDELAY: 				       ; do this loop r2 times (1ms delay each time)
	SUB r0, r0, 1                  ; decrement counter by 1
	LDI32 r4, SAMPLE_DELAY_1MS     ; load 1ms delay into r4

DELAY1MS:
	SUB r4, r4, 1
	QBNE DELAY1MS, r4, 0 			; keep going until 1ms has elapsed
	QBNE SAMPLEDELAY, r0, 0 		; repeat loop unless zero
	QBNE MAINLOOP, r1, 0            ; loop if the number of iterations has not passed

END:
    CLR r30, r30.t7                 ; clear pru output pin, use this as a GPIO interrupt
	HALT                            ; halt the pru program
