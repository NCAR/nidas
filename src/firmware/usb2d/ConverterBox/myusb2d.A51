;--------------------------------------------------------------
; myusb2d.A51     Mike Spowart
;
;Copyright (c) 2005 National Center For Atmospheric Research All rights reserved;
;--------------------------------------------------------------
$NOMOD51		; disable predefined 8051 registers
$nolist
$include (ezregs.inc)	; EZ-USB register assignments
$list
;
NAME	      my_usb2d

my_prog	segment	code
public		  my_usb2d
public		  debug
EXTRN	BIT 	(neg512,busybit,tasstop,firstpos,firstneg)
EXTRN	DATA	(overld0,overld1,color,busy)
EXTRN	XDATA	(progtas)
EXTRN	CODE	(settas,setled)
;*******************
; Constants
;********************


LEDGREEN	  equ	11111101b
LEDRED		  equ	11111110b
ADC_ADD_RD	equ	11001011b
ADC_ADD_WR	equ	11001010b

unldshift	  XDATA	8001H
byteread	  XDATA	8002H
debug		    XDATA	8003H
sreset		  XDATA	8004H
;-------------------------------------------------------------------------------------------------
; Bit mapped registers for flags
;-------------------------------------------------------------------------------------------------

;-------------------------------------------------------------------------------------------------
; Byte registers in internal RAM
;-------------------------------------------------------------------------------------------------

;-------------------------------------------------------------------------------------------------
;-------------------------------------------------------------------------------------------------
; Start of main program.
;-------------------------------------------------------------------------------------------------
		rseg	my_prog
my_usb2d:
	
;Below is the main loop:		

		mov	 r0,#busy
		jb	 neg512,loop5
		mov	 a,#0
		mov	 @r0,a
		jmp	 loop4

loop5:	
		mov	 a,#1
		mov	 @r0,a
		setb busybit
		inc	 dps
		mov	 dptr,#IN2BUF	
		inc	 dps
		mov	 r2,#64		

		mov	 dptr,#unldshift	;send unldshift pulse
		movx @dptr,a		;don't care what's in acc.

first1:		
    mov	 dptr,#PINSA
		movx a,@dptr
		jnb	 acc.0,first1
		mov	 dptr,#byteread
		mov	 a,#0x55
		inc	 dps
		movx @dptr,a
		inc	 dptr
		inc	 dps

first2:		
    mov	 dptr,#PINSA
		movx a,@dptr
		jnb	 acc.0,first2
		mov	 dptr,#byteread
		mov	 a,#0xaa
		inc	 dps
		movx @dptr,a
		inc	 dptr
		inc	 dps

first3:		
    mov	 dptr,#PINSA
		movx a,@dptr
		jnb	 acc.0,first3
		mov	 dptr,#byteread
		mov	 a,overld0
		inc	 dps
		movx @dptr,a
		inc	 dptr
		inc	 dps

first4:		
    mov	 dptr,#PINSA
		movx a,@dptr
		jnb	 acc.0,first4
		mov	 dptr,#byteread
		mov	 a,overld1
		inc	 dps
		movx @dptr,a
		inc	 dptr
		inc	 dps

		mov	 r1,#15
		mov	 r7,#0x55

loop0:		
		mov	 r5,#255
		mov	 r6,#255
		mov	 r3,#255

loop1:		
    mov	 r0,#4
		mov	 dptr,#unldshift	;send unldshift pulse
		movx @dptr,a		;don't care what's in acc.
loop2:
		mov	 dptr,#PINSA
		movx a,@dptr
		jnb	 acc.0,loop2
		mov	 dptr,#byteread
		movx a,@dptr		;read byte from 2D image data
		inc	 dps
		movx @dptr,a
		inc	 dptr
		inc	 dps
		djnz r0,loop2	;4 bytes = 1 slice yet?

		djnz r1,loop1	;16 slices (one 64 byte packet) yet?
		mov	 r1,#16
loop3:		
		mov	 dptr,#IN2CS	;check if IN2 busy
		movx a,@dptr
		jnb	 acc.1,loop6	;wait for not busy
		djnz r5,loop3
		mov	 r5,#255
		djnz r6,loop3
		mov	 r5,#255
		mov	 r6,#255
		djnz r3,loop3
		mov  dptr,#sreset
		movx @dptr,a
		mov	 dptr,#IN2CS	;disarm endpoint 2
    mov	 a,#2
		movx @dptr,a
		mov	 r0,#busy
		mov	 a,#0
		mov	 @r0,a
		call settas
		clr	 firstpos
		clr	 firstneg
		jmp	 loop4
loop6:
		mov	 dptr,#IN2BC
		mov	 a,#64
		movx @dptr,a		;arm 64 byte transfer

		inc	 dps
		mov	 dptr,#IN2BUF	; get ready to do it again
		inc	 dps
		djnz r2,loop0	;jump if record not done
		
;		mov	 dptr,#OUTA	;this writes a pulse to TP1
;		mov	 a,#2
;		movx @dptr,a
;		mov	 a,#0
;		movx @dptr,a

;Stop overload timer, turn on TAS here, and jam overload time:
;		mov	 dptr,#TL2
;		movx a,@dptr
;		mov	 r0,#overld0
;		mov	 @r0,a

    mov  a,TL2
    mov  dptr,#overld0
    movx @dptr,a

;		mov	 dptr,#TH2
;		movx a,@dptr
;		mov	 r0,#overld1
;		mov	 @r0,a

    mov  a,TH2
    mov  dptr,#overld1
    movx @dptr,a

		clr	 TR2
		call settas
		mov	 RCAP2L,#00H	;pre-load counter bytes
		mov	 RCAP2H,#00H
		anl	 CKCON,#0DFH	;clear T2M
loop4:				;go wait for next neg512 2D record
		clr	 neg512
		clr	 busybit
		ret
;
END					; end of program


