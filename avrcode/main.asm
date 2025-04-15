.include "m8515def.inc"

.dseg 
.org $60
adclsb: .byte 1 ; at $60
adcmsb: .byte 1 ; at $61
adchex: .byte 1 ; at $62

.cseg
reset: rjmp init     ;used as an entry point for the reset

.org $16

init:
  ldi r16, LOW(RAMEND)
  out SPL, r16
  ldi r16, HIGH(RAMEND)
  out SPH, r16

  rcall init_bus
  rcall init_lcd

  ; initialize portb as input
  ldi r16, $00
  out DDRB, r16 

bootmessage:
  ldi ZH, HIGH(bootmsg<<1)
	ldi ZL, LOW(bootmsg<<1)
  rcall lcd_puts
  ldi r23, $4
  ldi XH, $ff
  ldi XL, $ff
  rcall delay_bigger
  rcall lcd_clear

wificmp:
  rcall getb
  cpi r16, $01
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

movementcmp:
  rcall lcd_clear
  rcall ln2 ; for battery percentage

  ; clear input register
  clr r20
loop:
  ; start adc
  sts $6000, r1
  
l1:
  ; check for adc input
  in r20, PINB
  andi r20, $8
  cpi r20, $1
  breq l1 ; loop while nothing received

  lds r20, $6000 ; store adc value into r20
  ;sts adchex, r20 ; store hex value to memory

  ; convert hex adc value to dec
  ;mov r16, r20
  ;rcall hex2asc

  ;sts adclsb, r16
  ;mov r16, r17
  ;sts adcmsb, r16

  mov r16, r20

  ldi r17, $64
  mul r16, r17 ; multiply with 100(dec)

  ldi r16, $01
  mov r3, r16 ; set divisor (r3:r2)
  ldi r16, $bf
  mov r2, r16

  rcall div16

  mov r16, r0
  rcall hex2asc
  sts adclsb, r16
  mov r16, r17
  sts adcmsb, r16
  
  ; get input and print correct message
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

  ; default to NULL
msgnull:
  ldi ZH, HIGH(nullmsg<<1)
  ldi ZL, LOW(nullmsg<<1)
  rcall print
  rjmp movementcmp
 
msgfore:
  ldi ZH, HIGH(foremsg<<1)
  ldi ZL, LOW(foremsg<<1)
  rcall print
  rjmp movementcmp

msgback:
  ldi ZH, HIGH(backmsg<<1)
  ldi ZL, LOW(backmsg<<1)
  rcall print
  rjmp movementcmp

msgleft:
  ldi ZH, HIGH(leftmsg<<1)
  ldi ZL, LOW(leftmsg<<1)
  rcall print
  rjmp movementcmp

msgright:
  ldi ZH, HIGH(rightmsg<<1)
  ldi ZL, LOW(rightmsg<<1)
  rcall print
  rjmp movementcmp

msgstop:
  ldi ZH, HIGH(stopmsg<<1)
  ldi ZL, LOW(stopmsg<<1)
  rcall print
  rjmp movementcmp

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

print:
  rcall ln1 ; print command
  rcall lcd_puts

  rcall ln2 ; print battery level
  lds r16, adclsb
  rcall lcd_putch
  lds r16, adcmsb
  rcall lcd_putch

  rcall wait
  ret

percent:


; my division subroutine
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


; -------
; data bytes
; the extra 0s at the end are for padding
bootmsg:     .db "VisionVroom", 1, "Areg Nakashian", 0, 0 
wifimsg:     .db "Connecting to", 1, "WiFi", 0, 0
wifigoodmsg: .db "WiFi Connected ", 1, "Successfully!", 0
foremsg:     .db "ESP32: Foreward", 0, 0
backmsg:     .db "ESP32: Reverse", 0
rightmsg:    .db "ESP32: Right", 0
leftmsg:     .db "ESP32: Left", 0, 0
stopmsg:     .db "ESP32: Stop", 0, 0
nullmsg:     .db "ESP32: NULL", 0, 0
automsg:     .db "ESP32: auto mode ", 0, 0

; includes
.include "functions.inc"
.include "numio.inc"

.exit
