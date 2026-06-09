	.cdecls "main.c"
	.clink
	.global START
	.asg 32, PRU0_R31_VEC_VALID 		; allows notification of program completion
	.asg 3, PRU_EVTOUT_0 			; the event number that is sent back
	.asg 1000, TRIGGER_COUNT ;
	.asg 100000, SAMPLE_DELAY_1MS
	.asg 50, DEBOUNCE
	.asg 2508, MULTI_CONST
	.asg 8, MEMORY_START_OFFSET
	.asg 1, SENSOR_MASK_1
	.asg 3, SENSOR_MASK_2
	.asg 7, SENSOR_MASK_3

; Using register 0 for temporary storage
START:
	SET r30, r30.t7                ; set pru output gpio for whole program
	LDI32 r0, 0x00000000           ; Load the number of samples
	LBBO &r1, r0, 0, 4			
	LDI32 r0, 0x00000004           ; Load the sample delay
	LBBO &r2, r0, 0, 4
	LDI32 r0, 0x00000008			;load the number of sensors
	LBBO &r3, r0, 0, 4

	QBEQ TWO_SENSOR, r3, 2
	QBEQ THREE_SENSOR, r3, 3
	LDI r4, SENSOR_MASK_1
	QBA MAINLOOP

TWO_SENSOR:
	LDI r4, SENSOR_MASK_2
	QBA MAINLOOP

THREE_SENSOR:
	LDI r4, SENSOR_MASK_3
	QBA MAINLOOP

MAINLOOP:
	LDI32 r0, TRIGGER_COUNT
	SET r30, r30.t5

TRIGGERING:                        ; delay for 10us
	SUB r0, r0, 1                  ; decrement loop counter
	QBNE TRIGGERING, r0, 0         ; repeat loop unless zero
	CLR r30, r30.t5                ; 10us over, set trigger low
	; clear the counter and wait until the echo goes high
	LDI32 r13, 0 				   ; r13 will store the echo pulse width
    WBS r31, 3                     ; wait until the echo goes high
	MOV r6, r4

RESET_DEBOUNCE:
	LDI r19, DEBOUNCE               ; reset debounce counter
COUNTING:
	ADD r13, r13, 1                 ; increment the echo counter by 1
	NOT r0, r31
	AND r5, r6, r0					; bitmask the echo pins
	QBNE COUNTING, r5, 1          ; loop if the echo is still high


	LMBD r16, r5, 1

; "debounce", make sure we only count the echo as finished if it stays low
COUNT_DEBOUNCE:
    SUB r19, r19, 1                  ; debounce countdown
	ADD r13, r13, 1                  ; still increase echo counter in debounce mode
	
	QBBS RESET_DEBOUNCE, r31, r16       ; go back to normal counting if echo is high again
	QBNE COUNT_DEBOUNCE, r19, 0      ; continue loop until debounce is over

	XOR r6, r5, r6
	QBNE RESET_DEBOUNCE, r6, 0

	; multiply time by 2508, resulting in distance (micrometers)
	LDI r19, MULTI_CONST            ; set up multiplication registers
	LDI r18, 0                      ; bit logic register
	LDI r17, 0                      ; where output will go

MULTI_LOOP:
	QBEQ DONE_MULTI, r13, 0
	AND r18, r13, 1
	QBEQ SKIP_ADD, r18, 0
	ADD r17, r17, r19

SKIP_ADD:
	LSR r13, r13, 1
	LSL r19, r19, 1
	QBA MULTI_LOOP

DONE_MULTI:
	LDI r13, 0
	ADD r13, r13, r17

	; Write the value to shared memory
	MOV r0, r3
	LSL r0, r1, 2                  ; multiply iteration by 4 for array pos
	ADD r0, r0, 4                  ; constant offset (don't overwrite trigger count & sample delay)
	SBBO &r13, r0, 0, 4             ; store echo count at this address

	; one more sample iteration has taken place
	SUB r1, r1, 1                  ; take 1 away from the number of iterations
	MOV r0, r2                     ; load delay between samples

SAMPLEDELAY: 				       ; do this loop r2 times (1ms delay each time)
	SUB r0, r0, 1                  ; decrement counter by 1
	LDI32 r14, SAMPLE_DELAY_1MS     ; load 1ms delay into r14

DELAY1MS:
	SUB r14, r14, 1
	QBNE DELAY1MS, r14, 0 			; keep going until 1ms has elapsed
	QBNE SAMPLEDELAY, r0, 0 		; repeat loop unless zero
	QBNE MAINLOOP, r1, 0            ; loop if the number of iterations has not passed

END:
    CLR r30, r30.t7                 ; clear pru output pin, use this as a GPIO interrupt
	HALT                            ; halt the pru program
