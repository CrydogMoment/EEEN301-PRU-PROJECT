	.cdecls "main.c"
	.clink
	.global START
	.asg 32, PRU0_R31_VEC_VALID 		; allows notification of program completion
	.asg 3, PRU_EVTOUT_0 			; the event number that is sent back
	.asg 1000, TRIGGER_COUNT ;
	.asg 100000, SAMPLE_DELAY_1MS
	.asg 100, DEBOUNCE
	.asg 2508, MULTI_CONST
	.asg 8, MEMORY_START_OFFSET
	.asg 2, SENSOR_MASK_1
	.asg 6, SENSOR_MASK_2
	.asg 14, SENSOR_MASK_3

; Using register 0 for temporary storage
START:
	SET r30, r30.t7                ; set pru output gpio for whole program
	LDI32 r0, 0x00000000           ; Load the number of samples
	LBBO &r1, r0, 0, 4			
	LDI32 r0, 0x00000004           ; Load the sample delay
	LBBO &r2, r0, 0, 4
	LDI32 r0, 0x00000008			;load the number of sensors
	LBBO &r3, r0, 0, 4

	LDI r27, 12

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
	LDI r9, 0
	LDI r10, 0 
	LDI r11, 0 

TRIGGERING:                        ; delay for 10us
	SUB r0, r0, 1                  ; decrement loop counter
	QBNE TRIGGERING, r0, 0         ; repeat loop unless zero
	CLR r30, r30.t5                ; 10us over, set trigger low
RESET_WAIT_DEBOUNCE:
	LDI r19, DEBOUNCE
WAIT_ALL_HIGH:
	SUB r19, r19, 1
	AND r0, r31, r4
	QBNE RESET_WAIT_DEBOUNCE, r4, r0 
	QBNE WAIT_ALL_HIGH, r19, 0
	LDI r6, 0 
	ADD r6, r4, r6			;current bitmask = r6
	; clear the counter and wait until the echo goes high
	LDI32 r13, 0 				   ; r13 will store the echo pulse width

RESET_DEBOUNCE:
	LDI r19, DEBOUNCE               ; reset debounce counter
COUNTING:
	ADD r13, r13, 1                 ; increment the echo counter by 1
	AND r7, r6, r31					;temp bitmask = r31 AND current bitmask

	QBEQ COUNTING, r7, r6			;loop if all echo's still high

GET_SENSOR:
	NOT r7, r7
	AND r7, r7, r6					; turn mask into 1's where 0's 

	LMBD r8, r7, 1					; find the left most sensor that is LO 
	ADD r13, r13, 1                 ; increment the echo counter by 1 even while determining the sensor
	QBEQ DEBOUNCE_SENSOR_3, r8, 3	
	QBEQ DEBOUNCE_SENSOR_2, r8, 2

DEBOUNCE_SENSOR_1:
	SUB r19, r19, 1                  ; debounce countdown
	ADD r13, r13, 1                  ; still increase echo counter in debounce mode
	
	QBBS RESET_DEBOUNCE, r31, 1       ; go back to normal counting if echo is high again
	QBNE DEBOUNCE_SENSOR_1, r19, 0      ; continue loop until debounce is over
	;if we are here, then this sensor truly is lo. 
	XOR r6, r6, 2	;editing the current bitmask

	LDI r9, 0
	ADD r9, r9, r13	;add the count to the timer's assigned register 
	QBA SENSORS_DONE

DEBOUNCE_SENSOR_2:
	SUB r19, r19, 1                  ; debounce countdown
	ADD r13, r13, 1                  ; still increase echo counter in debounce mode
	
	QBBS RESET_DEBOUNCE, r31, 2       ; go back to normal counting if echo is high again
	QBNE DEBOUNCE_SENSOR_2, r19, 0      ; continue loop until debounce is over
	;if we are here, then this sensor truly is lo. 
	XOR r6, r6, 4	;editing the current bitmask

	LDI r10, 0
	ADD r10, r10, r13	;add the count to the timer's assigned register 
	QBA SENSORS_DONE

DEBOUNCE_SENSOR_3:
	SUB r19, r19, 1                  ; debounce countdown
	ADD r13, r13, 1                  ; still increase echo counter in debounce mode
	
	QBBS RESET_DEBOUNCE, r31, 3       ; go back to normal counting if echo is high again
	QBNE DEBOUNCE_SENSOR_3, r19, 0      ; continue loop until debounce is over
	;if we are here, then this sensor truly is lo. 
	XOR r6, r6, 8 ;editing the current bitmask

	LDI r11, 0
	ADD r11, r11, r13	;add the count to the timer's assigned register 

SENSORS_DONE:
	QBNE RESET_DEBOUNCE, r6, 0
	; set up loop to run number of times that there are sensors
	LDI r0, 0
	ADD r0, r0, r3 

MULT_SENSOR:
	; multiply time by 2508, resulting in distance (micrometers)
	LDI r19, MULTI_CONST            ; set up multiplication registers
	LDI r18, 0                      ; bit logic register
	LDI r17, 0                      ; where output will go
	LDI r13, 0						; prepare register for sensor data

	QBEQ FOR_3, r0, 3
	QBEQ FOR_2, r0, 2

	ADD r13, r13, r9
	QBA MULTI_LOOP

FOR_2:
	ADD r13, r13, r10
	QBA MULTI_LOOP
FOR_3:
	ADD r13, r13, r11
	QBA MULTI_LOOP

MULTI_LOOP:
	QBEQ DONE_MULT, r13, 0
	AND r18, r13, 1
	QBEQ SKIP_ADD, r18, 0
	ADD r17, r17, r19

SKIP_ADD:
	LSR r13, r13, 1
	LSL r19, r19, 1
	QBA MULTI_LOOP

DONE_MULT:
	; Write the value to shared memory
	SBBO &r17, r27, 0, 4	; store echo count at this address
	ADD r27, r27, 4
	
	SUB r0, r0, 1
	QBEQ RELOAD, r0, 0
	QBA MULT_SENSOR

RELOAD:
	; one more sample iteration has taken place
	SUB r1, r1, 1                  ; take 1 away from the number of iterations
	QBEQ END, r1, 0
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
