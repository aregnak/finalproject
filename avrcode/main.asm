; My final project, VisionVroom, AVR assembly code
; this program is a monitorin system of inputs received from the esp32 car controller
; also displays a battery's level from a range of 9.7v to 7.2v (100%-0%)


; remove the line below if assembling on microchip studio
.include "m8515def.inc"

.equ BATMAX = 255; ADC max voltage cutoff for percentage
.equ BATMIN = 230; ADC min cutoff

.dseg
.org $60
adclsb:   .byte 1 ; you can tell        at $60
adcmsb:   .byte 1 ; you can tell        at $61
bat0:     .byte 1 ; Battery msb         at $62
bat1:     .byte 1 ; Battery middle      at $63
bat2:     .byte 1 ; Battery lsb         at $64
batupdel: .byte 1 ; Battery update del  at $65
batval:   .byte 1 ; Battery value now   at $66
batbuff:  .byte 1 ; Battery value buf   at $67

.cseg
reset: rjmp init     ;used as an entry point for the reset

.org $16

init:
  ldi r16, LOW(RAMEND)
  out SPL, r16
  ldi r16, HIGH(RAMEND)
  out SPH, r16

  ; Init lcd and bus
  rcall init_bus
  rcall init_lcd

  ; Initialize portb as input
  ldi r16, $00
  out DDRB, r16
  
  ; Initialize memory addresses to 0
  sts adclsb, r16 
  sts adcmsb, r16 
  sts bat0, r16 
  sts bat1, r16 
  sts bat2, r16 
  sts batval, r16 
  sts batbuff, r16 
       
  ; Initialize battery update delay loop
  ldi r16, $1
  sts batupdel, r16

bootmessage:
  ldi ZH, HIGH(bootmsg<<1)
	ldi ZL, LOW(bootmsg<<1)
  rcall lcd_puts
  ldi r23, $4               ; a delay to properly see welcome message
  ldi XH, $8f
  ldi XL, $ff
  rcall delay_bigger
  rcall lcd_clear

wificmp:
  rcall getb          ; get portb input
  cpi r16, $04
  breq wifigood
  rcall ln1
  ldi ZH, HIGH(wifimsg<<1)
  ldi ZL, LOW(wifimsg<<1)
  rcall lcd_puts
  rcall wait
  rcall lcd_clear
  rjmp wificmp  ; loop connecting message until connected

wifigood:
  rcall ln1
  rcall lcd_clear
  ldi ZH, HIGH(wifigoodmsg<<1)
  ldi ZL, LOW(wifigoodmsg<<1)
  rcall lcd_puts
  rcall wait

init_bat_buff:
  sts $6000, r1

init_bat_buff_loop:
  ; check for adc input
  in r20, PINB
  andi r20, $8 ; check for 00010000 on portb
  cpi r20, $1
  breq init_bat_buff_loop     ; loop while nothing received
  lds r16, $6000
  sts batbuff, r16

main:
  ; run adc
  sts $6000, r1

loop:
  ; check for adc input
  in r20, PINB
  andi r20, $8
  cpi r20, $1
  breq loop     ; loop while nothing received

  lds r16, $6000 ; store adc value into r20
  sts batval, r16

; uncomment if you want the raw adc hex input converted to ascii
;  rcall hex2asc
;  sts test, r16
;  mov r16, r17
;  sts test2, r16

  lds r16, batval
  lds r17, batbuff
  cp r16, r17
  brge no_swap ; If r16 >= r17, no need to swap

  ; Swap for absolute value
  mov  r18, r16 ; r18 acts as r16's buffer
  mov  r16, r17
  mov  r17, r18

no_swap:
  sub r16, r17 ; r16 = |r16 - r17|
  cpi r16, $6 ; Compare |difference| with 6
  brsh do_percent  ; skip averaging if the difference is less than 10
  
do_avg:
  rcall checkavg

do_percent:
  ; call percent calculation subroutine
  ; after a small delay
  lds r21, batupdel
  subi r21, 1
  brne loop_continue
  
  lds r16, batval   ; store bat value in hex for %
  sts batbuff, r16 

  rcall percent

  ; convert hex value (on r16) to decimal
  rcall hex2dec
  sts bat0, r0 ; store for later use
  sts bat1, r1
  sts bat2, r2
  
  ; re-initialize delay
  ldi r21, $15

loop_continue:
  sts batupdel, r21 

  ; get directional message
  rcall getb
  cpi r16, $02
  breq msgfore

  cpi r16, $03
  breq msgback 
  
  cpi r16, $04
  breq msgleft

  cpi r16, $05
  breq msgright

  cpi r16, $06
  breq msgstop

  ; Default to NULL message if no command
msgnull:
  ldi ZH, HIGH(nullmsg<<1)
  ldi ZL, LOW(nullmsg<<1)
  rcall print
  rjmp main

msgfore:
  ldi ZH, HIGH(foremsg<<1)
  ldi ZL, LOW(foremsg<<1)
  rcall print
  rjmp main

msgback:
  ldi ZH, HIGH(backmsg<<1)
  ldi ZL, LOW(backmsg<<1)
  rcall print
  rjmp main

msgleft:
  ldi ZH, HIGH(leftmsg<<1)
  ldi ZL, LOW(leftmsg<<1)
  rcall print
  rjmp main

msgright:
  ldi ZH, HIGH(rightmsg<<1)
  ldi ZL, LOW(rightmsg<<1)
  rcall print
  rjmp main

msgstop:
  ldi ZH, HIGH(stopmsg<<1)
  ldi ZL, LOW(stopmsg<<1)
  rcall print
  rjmp main

msgauto:
  ldi ZH, HIGH(automsg<<1)
  ldi ZL, LOW(automsg<<1)
  rcall print
  rjmp main

failsafeloop:
  rjmp failsafeloop
  ; Very failsafe, in case something catastrophic happens
  ; and it gets to this point in code

;-----------------------------
; subroutines
;
; get the input on pinb
getb:
  in r16, PINB
  andi r16, $07 ; mask the first 3 bits of r16
  ret

; small delay sbr ~300ms
wait:
  ldi XH, $ff
  ldi XL, $ff
  rcall delay_big
  ret

; little sbr that contains all printing to lcd in one place
print:
  rcall lcd_clear
  rcall ln1 ; print command
  rcall lcd_puts

  ; will display:
  ; BAT0: xxx%
  rcall ln2 ; print battery level
  ldi ZH, HIGH(batmsg<<1)
  ldi ZL, LOW(batmsg<<1)
  rcall lcd_puts

  lds r16, bat2
  subi r16, -'0'
  rcall lcd_putch

  lds r16, bat1
  subi r16, -'0'
  rcall lcd_putch

  lds r16, bat0
  subi r16, -'0'
  rcall lcd_putch

  ldi r16, '%'
  rcall lcd_putch

  rcall wait
  ret


; calculate battery percentage of a 9v battery 
; input: r16 should have adc level in hex
; output: r16 with the percentage in hex

; percentage = ((adc value - MIN) * 100) / (MAX - MIN)
; The *100 needs to be done before the division as it will give a fraction every time
percent:
  subi r16, BATMIN ; read level - MIN 
  brcs zero ; if less than 0, display 0%
  breq zero ; if equal to 0, display 0%

  ldi r17, 100 ; multiply by 100
  mul r16, r17

  clr r3 ; clear divisor MSB
  ldi r17, BATMAX ; MAX-MIN subtraction
  subi r17, BATMIN
  mov r2, r17
  rcall div16 ; call division

store:
  mov r16, r0  ; save percentage into r16
  ret

zero:
  ldi r16, 0 ; zero out the output
  ret
  
; Check battery value and average with the
; previous one if difference is too high
; for more even and accurate battery display
checkavg:
  lds r16, batval
  lds r17, batbuff
  subi r16, $80 ; subtract 128 to not use 16 bit registers
  subi r17, $80 

  add r16, r17
  lsr r16
  
  subi r16, -$80 ; add back the 128
  
  sts batval, r16 ; Store averaged bat val
  ret



; R0 / R17 = R0,  Remainder R4
div8:
    clr     r4
    ldi     r16, 9

div8loop:
    rol     r0
    dec     r16
    brne    div8cont
    ret

div8cont:
    rol     r4
    sub     r4, r17
    brcc    div8skip
    add     r4, r17
    clc
    rjmp    div8loop

div8skip:
    sec
    rjmp    div8loop

; input:
; R1:R0 (dividend)
; R3:R2 (divisor)
; output:
; R1:R0 (Quotient)
; R5:R4 (remainder)
;
; (R1:R0 / R3:R2) = (R1:R0), (R5:R4)
div16:
  clr r4
  clr r5
  ldi r16, 17

divloop:
  rol r0
  rol r1
  dec r16
  brne divcont

  ret

divcont:
  rol r4
  rol r5
  sub r4, r2
  sbc r5, r3

  brcc divskip
  add r4, r2
  adc r5, r3
  clc
  rjmp divloop

divskip:
  sec
  rjmp divloop


; -------------------------------------
; data bytes
bootmsg:     .db "VisionVroom", 1, "Areg Nakashian", 0
wifimsg:     .db "Connecting to", 1, "WiFi", 0
wifigoodmsg: .db "WiFi Connected ", 1, "Successfully!", 0
foremsg:     .db "ESP32: Foreward", 0
backmsg:     .db "ESP32: Reverse", 0
rightmsg:    .db "ESP32: Right", 0
leftmsg:     .db "ESP32: Left", 0
stopmsg:     .db "ESP32: Stop", 0
nullmsg:     .db "ESP32: NULL", 0
automsg:     .db "ESP32: auto mode ", 0
batmsg:      .db "BAT0: ", 0

; includes
.include "functions.inc"

.exit
