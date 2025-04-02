.include "m8515def.inc"

.cseg
reset: rjmp init     ;used as an entry point for the reset
isr0:  rjmp isr_int0 ; int0 entry

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

firstmessage:
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
  rcall ln1
  ldi ZH, HIGH(movemsg<<1)
  ldi ZL, LOW(movemsg<<1)
  rcall lcd_puts
  rcall ln2

  clr r23
  clr r20
loop:
  sts $6000, r1
  
l1:
  in r20, PINB
  andi r20, $8
  cpi r20, $1
  breq l1 

  lds r20, $6000 

  mov r16, r23
  rcall hex2asc
  mov r16, r17
  rcall lcd_putch
  inc r23

  mov r16, r20 

  rcall hex2asc

print:
  rcall lcd_putch
  mov r16, r17
  rcall lcd_putch

  rcall wait
  rcall lcd_clear
  rcall ln2
  rjmp loop

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
  rcall lcd_puts
  rcall wait
  rcall lcd_clear
  rjmp movementcmp
 
msgfore:
  ldi ZH, HIGH(foremsg<<1)
  ldi ZL, LOW(foremsg<<1)
  rcall lcd_puts
  rcall wait
  rcall lcd_clear
  rjmp movementcmp

msgback:
  ldi ZH, HIGH(backmsg<<1)
  ldi ZL, LOW(backmsg<<1)
  rcall lcd_puts
  rcall wait
  rcall lcd_clear
  rjmp movementcmp

msgleft:
  ldi ZH, HIGH(leftmsg<<1)
  ldi ZL, LOW(leftmsg<<1)
  rcall lcd_puts
  rcall wait
  rcall lcd_clear
  rjmp movementcmp

msgright:
  ldi ZH, HIGH(rightmsg<<1)
  ldi ZL, LOW(rightmsg<<1)
  rcall lcd_puts
  rcall wait
  rcall lcd_clear
  rjmp movementcmp

msgstop:
  ldi ZH, HIGH(stopmsg<<1)
  ldi ZL, LOW(stopmsg<<1)
  rcall lcd_puts
  rcall wait
  rcall lcd_clear
  rjmp movementcmp

pt1:
  rjmp pt1 


isr_int0:
  lds r20, $6000 

  mov r16, r23
  rcall hex2asc
  mov r16, r17
  rcall lcd_putch
  inc r23
  ;sts $70, r17

  reti

; subroutines
;
; get the input on pinb
getb:
  in r16, PINB
  andi r16, $07 ; mask the first 3 bits of r16
  ret

wait:
  ldi XH, $ff
  ldi XL, $ff
  rcall delay_big
  ret

; -------
; data bytes
bootmsg:     .db "VisionVroom", 1, "Areg Nakashian", 0, 0 
wifimsg:     .db "Connecting to", 1, "WiFi", 0, 0
wifigoodmsg: .db "WiFi Connected ", 1, "Successfully!", 0
movemsg:     .db "Current CMD: " 0, 0
foremsg:     .db "Foreward", 0, 0
backmsg:     .db "Reverse", 0
rightmsg:    .db "Right", 0
leftmsg:     .db "Left", 0, 0
stopmsg:     .db "Stop", 0, 0
nullmsg:     .db "NULL", 0, 0
automsg:     .db "auto mode ", 0

; includes
.include "functions.inc"
.include "numio.inc"

.exit
