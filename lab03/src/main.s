.syntax unified
.cpu cortex-m0
.fpu softvfp
.thumb

//==================================================================
// ECE 362 Lab Experiment 3
// General Purpose I/O
//==================================================================

.equ  RCC,      0x40021000
.equ  AHBENR,   0x14
.equ  GPIOCEN,  0x00080000
.equ  GPIOBEN,  0x00040000
.equ  GPIOAEN,  0x00020000
.equ  GPIOC,    0x48000800
.equ  GPIOB,    0x48000400
.equ  GPIOA,    0x48000000
.equ  MODER,    0x00
.equ  PUPDR,    0x0c
.equ  IDR,      0x10
.equ  ODR,      0x14
.equ  BSRR,     0x18
.equ  BRR,      0x28

//==========================================================
// initb: Autotest check ??
// Enable Port B in the RCC AHBENR register and configure
// the pins as described in section 2.1 of the lab
// No parameters.
// No expected return value.
.global initb
initb:
     push {r4,r5,r6,r7,lr}
     ldr r0,=RCC
     ldr r1,[r0,#AHBENR]
     ldr r2,=GPIOBEN
     orrs r1,r2
     str r1,[r0,#AHBENR]
     ldr r0,=GPIOB
     ldr r1,[r0,#MODER]
     ldr r3,=0xff0303
     bics r1,r3
     ldr r2,=0x550000
     orrs r1,r2
     str r1,[r0,#MODER]
     pop {r4,r5,r6,r7,pc}
    // End of student code

//==========================================================
// initc: Autotest check ??
// Enable Port C in the RCC AHBENR register and configure
// the pins as described in section 2.2 of the lab
// No parameters.
// No expected return value.
.global initc
initc:
    push {lr}
    // Student code goes here
    ldr r0,=RCC
    ldr r1,[r0,#AHBENR]
    ldr r2,=GPIOCEN
    orrs r1,r2
    str r1,[r0,#AHBENR]
    ldr r0,=GPIOC
    ldr r1,[r0,#MODER]
    ldr r2,=0xffff
    bics r1,r2
    ldr r3,=0x5500
    orrs r1,r3
    str r1,[r0,#MODER]
    ldr r2,[r0,#PUPDR]
    ldr r3,=0xaa
    orrs r2,r3
    str r2,[r0,#PUPDR]
    // End of student code
    pop {pc}

//==========================================================
// setn: Autotest check ??
// Set given pin in GPIOB to given value in ODR
// Param 1 - pin number
// param 2 - value [zero or non-zero]
// No expected retern value.
.global setn
setn:
    push {r4,r5,r6,r7,lr}
    // Student code goes here
    cmp r1,#0
    beq else1
    ldr r2,=GPIOB
    movs r3,#1
    lsls r3,r0
    ldr r4,[r2,#ODR]
    orrs r4,r3
    str r4,[r2,#ODR]
    b endif1
else1:
    ldr r2,=GPIOB
    movs r3,#1
    lsls r3,r0
    ldr r4,[r2,#ODR]
    bics r4,r3
    str r4,[r2,#ODR]
    b endif1
endif1:
    // End of student code
    pop {r4,r5,r6,r7,pc}

//==========================================================
// readpin: Autotest check ??
// read the pin given in param 1 from GPIOB_IDR
// Param 1 - pin to read
// No expected return value.
.global readpin
readpin:
    push {lr}
    // Student code goes here
    ldr r1,=GPIOB
    ldr r2,[r1,#IDR]
    movs r3,#1
    lsls r3,r0
    ands r2,r3
    beq else2
    movs r0,#0x1
    b endif2
else2:
    movs r0,#0x0
    // End of student code
 endif2:
    pop {pc}

//==========================================================
// buttons: Autotest check ??
// Check the pushbuttons and turn a light on or off as
// described in section 2.6 of the lab
// No parameters.
// No return value
.global buttons
buttons:
    push    {lr}
    // Student code goes here
    movs r0,#0
    bl readpin
    movs r1,r0
    movs r0,#8
    bl setn
    movs r0,#4
    bl readpin
    movs r1,r0
    movs r0,#9
    bl setn
    // End of student code
    pop     {pc}

//==========================================================
// keypad: Autotest check ??
// Cycle through columns and check rows of keypad to turn
// LEDs on or off as described in section 2.7 of the lab
// No parameters.
// No expected return value.
.global keypad
keypad:
    push {r4,r5,r6,r7,lr}
    // Student code goes here
for:
    movs r4,#8
check:
    cmp r4,#0
    ble endfor3
    ldr r0,=GPIOC
    lsls r2,r4,#4
    str r2,[r0,#ODR]
    bl mysleep
    ldr r0,=GPIOC
    ldr r3,[r0,#IDR]
    ldr r5,=0xf
    ands r3,r5
if:
    cmp r4,#8
    bne else3
    movs r0,#8
    movs r1,#1
    ands r1,r3
    bl setn
    lsrs r4,#1
    b check
else3:
    cmp r4,#4
    bne else4
    movs r0,#9
    movs r1,#2
    ands r1,r3
    bl setn
    lsrs r4,#1
    b check
else4:
    cmp r4,#2
    bne else5
    movs r0,#10
    movs r1,#4
    ands r1,r3
    bl setn
    lsrs r4,#1
    b check
else5:
    movs r0,#11
    movs r1,#8
    ands r1,r3
    bl setn
    lsrs r4,#1
    b check
endfor3:
    // End of student code
    pop {r4,r5,r6,r7,pc}

//==========================================================
// mysleep: Autotest check ??
// a do nothing loop so that row lines can be charged
// as described in section 2.7 of the lab
// No parameters.
// No expected return value.
.global mysleep
mysleep:
    push {lr}
    // Student code goes here
for1:
    movs r0,#0
    ldr r1,=1000
check1:
    cmp r0,r1
    bge endfor1
    adds r0,#1
    b check1
endfor1:
    pop {pc}
    // End of student code

//==========================================================
// The main subroutine calls everything else.
// It never returns.
.global main
main:
        push {lr}
	bl   autotest // Uncomment when most things are working
	bl   initb
        bl   initc
// uncomment one loop when ready
loop1:
	bl   buttons
        b    loop1
loop2:
	bl   keypad
	b    loop2

        wfi
        pop {pc}
