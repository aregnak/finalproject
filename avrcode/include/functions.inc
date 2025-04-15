;
; Filename: functions.inc
; 
; Under the MIT licence: https://mit-license.org/
; Copyright holder: Areg Nakashian
;
; Description: Subroutine framework for use in the project.
; The license is for funny use it however you want (license says so as well).
;
; These functions are more or less general use functions, meaning I can copy paste
; this file in other projects and have the subroutines work as they are "universal"
;
; Author: Areg Nakashian

.equ BUSON = $8A     ; for bus init
.equ BAUD = 25     ; for uart @1MHz, from Table 68 (p.160)
.equ FRAME = $86    ; select data, parity, stop bit (p.156-158)      
.equ CHAN = $18      ;Enable both Tx and Rx (UART)

; inits
init_uart:                 
    ldi R16, 0        ;always zero (mostly)
    out UBRRH, R16    
    ldi R16, BAUD    
    out UBRRL, R16    ;config. Tx baud rate w/equ value 
    ldi R16, CHAN      
    out UCSRB, R16    ;enable transmit only (see p.156)
    ldi R16, FRAME         
    out UCSRC, R16    ;config. frame elements      
    ret                 

init_bus:
	ldi R16, $C0 ; for int0 falling edge & int1 falling edge        
	out GICR,R16
    ldi R16, BUSON
	out MCUCR, R16
    ret

init_lcd:             ; 2 lines, 8-bit
	ldi XH, $4e
	ldi XL, $20
	rcall delay_big 

	ldi r18, $38
	sts $2000, R18
	ldi r19, 13 ; ~37u delay
	rcall delay	 

	ldi r18, $38
	sts $2000, R18
	ldi r19, 13 ; ~37u delay
	rcall delay	

	ldi r18, $0f
	sts $2000, R18
	ldi r19, 13 ; ~37u delay
	rcall delay
	
	ldi r18, $01
	sts $2000, R18
	ldi XH, $2     ; $2f8 makes 1.52ms delay
	ldi XL, $f8
	rcall delay_big

	ldi r18, $06
	sts $2000, R18
	ldi r19, 13		; ~37u delay
	rcall delay	
    ret

; -----------
; delays
delay:				; for numbers < $100
	subi r19, 1		; 3us per loop
	brne delay
	ret

delay_big:			; for numbers > $ff to $ffff 
	sbiw XH:XL, 1   ; 4us per loop
	brne delay_big		
	ret

delay_bigger:			; for numbers > ffff to $fffff 
	sbiw XH:XL, 1   	; 4us per loop
	brne delay_bigger
  bigger2:				
	subi r23, 1			; 3us every time it runs
	brne delay_bigger		
	ret

; -----------
; terminal stuff
cin:		; i love c++
    in r21, UDR
    ret

getch:
	in r16,UCSRA
	andi r16, $80
	breq getch		   ;poll status for key press
	in r16,UDR      ;get char from UART 
	ret

outch:
    out UDR, r16
  pollsr:
    in r16, UCSRA
	andi r16, $20
    breq pollsr
    ret

nl: 				; print new line
    ldi r16, $0d
    rcall outch
    ldi r16, $0a
    rcall outch
    ret

cout:		; i love c++ again
	lpm r16, Z+ 
	cpi r16, 0 
	breq return 
	cpi r16, 1
	breq coutnl
	rcall outch 
	rjmp cout
  coutnl:
    rcall nl
	rjmp cout

; guess what this does genius
return:
	ret

; ----------
; lcd stuff
ln1:
	push r18
	ldi r18, $80
	sts $2000, R18
	ldi r19, 19 ; ~37u delay
	rcall delay	
	pop r18
	ret

ln2:
	push r18
	ldi r18, $C0
	sts $2000, R18
	ldi r19, 19 ; ~37u delay
	rcall delay	
	pop r18
	ret

; for 2 line printing, separate messages with value 1 in array
; ex: .db "hello,", 1, "my friend.", 0
; put a 2 in front for terminal print as well
; ex: .db 2, "this will", 1, "print on the terminal", 0
lcd_puts:
	lpm r17, Z+
	cpi r17, 0		; check for terminating condition
	breq lcd_puts_ret
	cpi r17, 1		; check for second line writing
	breq nextln
	cpi r17, 2		; check for terminal printing
	breq termp_true

	cpi r18, 2
	brne lcd_puts_cont
	mov r16, r17
	rcall outch

  lcd_puts_cont:	
	sts $2100, r17
	ldi XH, $1		; for delay
	ldi XL, $f4
	rcall delay_big
	rjmp lcd_puts

  nextln:		; go to line 2 on lcd, and new line on terminal if wanted
	rcall ln2
	cpi r18, 2
	brne lcd_puts
	rcall nl
	rjmp lcd_puts

  termp_true:	; toggle terminal printing
	push r18
	mov r18, r17
	rjmp lcd_puts

  lcd_puts_ret:
	cpi r18, 2
	breq lcd_puts_ret2
	rjmp return
  lcd_puts_ret2:
	pop r18
	rjmp return

; lcd display a character
lcd_putch:
	sts $2100, r16
	ldi XH, $1		
	ldi XL, $f4
	rcall delay_big
	ret

; you don't need a comment for this
lcd_clear:
	ldi r18, $01
	sts $2000, R18
	ldi XH, $2     ; $2f8 makes 1.52ms delay
	ldi XL, $f8
	rcall delay_big
	ret

asc2bin:
    cpi r16, '0'          ; check for '0'-'9'
    brlo invalid    ; branch if less than '0'
    cpi r16, '9' + 1
    brlo numconv 

    ; check for uppercase 'A'-'F'
    cpi r16, 'A'
    brlo invalid    ; branch if less than 'A'
    cpi r16, 'F' + 1
    brlo upperconv

    ; check for lowercase 'a'-'f'
    cpi r16, 'a'
    brlo invalid    ; branch if less than 'a'
    cpi r16, 'f' + 1
    brge invalid   ; invalid input

    subi r16, 'a' - 10    ; convert 'a'-'f'
    ret

  upperconv:
    subi r16, 'A' - 10    ; convert 'A'-'F'
    ret

  numconv:
    subi r16, '0'         ; convert '0'-'9'
 
  invalid:
    ret

