.include "m8515def.inc"

.cseg
reset: rjmp init ;used as an entry point for the reset

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

main:
  rcall ln1
  ldi ZH, HIGH(bootmsg<<1)
	ldi ZL, LOW(bootmsg<<1)
  rcall lcd_puts

  rjmp main

; -------
bootmsg: .db 2, "VisionVroom", 1, "Areg Nakashian" 

.include "functions.inc"

.exit
