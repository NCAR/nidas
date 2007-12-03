;Copyright (c) 2005 National Center For Atmospheric Research All rights reserved;
; myusb2d_init.A51     Mike Spowart
;
;--------------------------------------------------------------
$NOMOD51		; disable predefined 8051 registers
$nolist
$include (ezregs.inc)	; EZ-USB register assignments
$list
;
NAME	        my_usb2d_init

my_prog	segment	code
public		my_usb2d_init,settas,stoptas,setpot,setled
public          putchars, put_crlf
public		neg512,progtas,busybit
public		overld0,overld1
public		potdata,ndiv,color,gstatus,hksend,busy
public		tasstop,firstpos,firstneg
EXTRN	XDATA	(debug)
;********************
; Constants
;********************

POT_ADD_WR	equ	01010100b	;L part suffix
POT_CMD_WR	equ	00010001b	;VREG
IOEXPANDER	equ	01110000b
POT_DATA_INIT	equ	100
TAS_INIT	equ	0
CR		equ	0x0D
LF		equ	0x0A

progtas		XDATA	8000H
;-------------------------------------------------------------------------------------------------
; Bit mapped registers for flags
;-------------------------------------------------------------------------------------------------
bit_seg		SEGMENT BIT
		RSEG	bit_seg
neg512:		dbit	1
busybit:	dbit	1
tasstop:	dbit	1
firstneg:	dbit 	1
firstpos:	dbit	1
;-------------------------------------------------------------------------------------------------
; Byte registers in internal RAM
;-------------------------------------------------------------------------------------------------
data_seg	SEGMENT	DATA
		RSEG	data_seg
potdata:	ds	1
ndiv:		  ds	1
color:		ds	1
gstatus:	ds	1
busy:		  ds	1
overld0:	ds	1
overld1:	ds	1
hksend:   ds  1
;house_dat:	ds	8
;-------------------------------------------------------------------------------------------------
;		ISEG AT 60H		
;stack:		ds	20
		CSEG 	AT 	3
		ljmp	neg512_isr
		CSEG 	AT	13H
		ljmp	pos512_isr
		CSEG 	AT 	23H
		CSEG	AT 33H
;-------------------------------------------------------------------------------------------------
; Start of main program.
;-------------------------------------------------------------------------------------------------
		rseg	my_prog
my_usb2d_init:
		clr	 neg512
		clr	 busybit
		clr	 firstneg
		clr	 firstpos
		call settas
;
;Initialize serial interface #0 using TIMER 1 to 9600 baud
		mov	 TMOD,#00100000B	;Mode 2
		mov	 TH1,#0D9H
		orl	 TCON,#40H
		orl	 PCON,#80H
		mov	 CKCON,#10H	;divide CLK24 by 12
		mov	 SCON0,#01010010B	;Mode 1

;Initialize TIMER 2 for overload time
		mov	 T2CON,#00H
		mov	 RCAP2L,#00H	;pre-load counter bytes
		mov	 RCAP2H,#00H

;Initialize TAS PLL divider
		mov	 a,#TAS_INIT	;initialize tas divider 
		mov	 r0,#ndiv	;tas byte
		mov	 @r0,a

;Initialize the TAS POT
		mov  a,#POT_DATA_INIT
		mov	 r0,#potdata
		mov	 @r0,a
		mov	 a,#1
		mov	 r0,#gstatus
		mov	 @r0,a
		call setpot
		call settas
		ret

;Initialize the POT
setpot:
    mov	 dptr,#I2CS	;I2C control/status reg.
		movx a,@dptr
		orl	 a,#80h
		movx @dptr,a		;set START bit

i2c_start:
    mov	 dptr,#I2DAT
		mov	 a,#POT_ADD_WR
		movx @dptr,a		;write POT WR address
		mov	 dptr,#I2CS
pot0:
    movx a,@dptr
    anl	 a,#3h
		jnb	 acc.0,pot0
    cjne a,#3h,pot3
		mov	 dptr,#I2DAT
		mov	 a,#POT_CMD_WR	;write POT command VREG
		movx @dptr,a		
		mov	 dptr,#I2CS
pot1:
    movx a,@dptr
    anl	 a,#3h
		jnb	 acc.0,pot1
    cjne a,#3h,pot3
		mov	 dptr,#I2DAT
		mov	 a,potdata	;write POT data 
		movx @dptr,a		
		mov	 dptr,#I2CS
pot2:
    movx a,@dptr
    anl	 a,#3h
		jnb	 acc.0,pot2
    cjne a,#3h,pot3
		mov	 a,#0
		jmp	 pot4
pot3:
    mov	 a,#1
	
pot4:
    mov	 r0,#gstatus
		movx @r0,a
		mov	 dptr,#I2CS
		mov	 a,#0x40
		movx @dptr,a		;send STOP bit
		call settas
		ret

;Program the IOExpander
setled:
    mov	 dptr,#I2CS	;I2C control/status reg.
		movx a,@dptr
		orl	 a,#80h
		movx @dptr,a		;set START bit
		mov	 dptr,#I2DAT
		mov	 a,#IOEXPANDER
		movx @dptr,a		;write POT WR address
		mov	 dptr,#I2CS
led0:
    movx a,@dptr
    anl	 a,#3h
		jnb	 acc.0,led0
    cjne a,#3h,led2
		mov	 dptr,#I2DAT
		mov	 a,color		;write lED color
		movx @dptr,a	
		mov	 dptr,#I2CS
led1:
    movx a,@dptr
    anl	 a,#3h
		jnb	 acc.0,led1	;wait for DONE
    cjne a,#3h,led2
		mov	 a,#0
		jmp	 led3
led2:
    mov	 a,#1
	
led3:
    mov	 r0,#gstatus
		movx @r0,a
		mov	 dptr,#I2CS
		mov	 a,#40h
		movx @dptr,a		;send STOP bit
		ret

settas:
		mov	 dptr,#progtas	;turn on TAS
		mov	 a,ndiv  	;TAS on 
		movx @dptr,a
		clr	 tasstop
		ret

stoptas:
		mov	 dptr,#progtas	;turn off TAS
		mov	 a,#3		;TAS off
		movx @dptr,a
		setb tasstop
		ret

put_crlf:	
		jnb	 TI,$
		clr	 TI
		mov	 a,#CR
		mov	 SBUF0,a
		jnb	 TI,$
		clr	 TI
		mov	 a,#LF
		mov	 SBUF0,a
		ret

;This routine outputs a single character to serial port 0.

putchars:	
		jnb	 TI,$
		clr	 TI
    mov  r0,#hksend
    mov	 a,@r0
		mov	 SBUF0,a
		ret

;Below are the ISRs
;pos512 happens when probe buffer is 1/2 full.
pos512_isr:	
		push	psw
		push	dps
		push	dpl
		push	dph
		push	dpl1
		push	dph1
		push	acc

		jnb	 firstneg,over0
		setb firstpos
		jnb	 busybit,over0
		call stoptas
		setb TR2		;start overload timer 2
		clr	 IE1

over0:
    pop	 acc
		pop	 dph1
		pop	 dpl1
		pop	 dph
		pop	 dpl
		pop	 dps
		pop	 psw
		RETI
;
;neg512 happens when probe buffer is full
neg512_isr:	
		push	psw
		push	dps
		push	dpl
		push	dph
		push	dpl1
		push	dph1
		push	acc

		setb firstneg
		jnb	 firstpos,over1
		jnb	 neg512, over2
		mov	 dptr,#debug
		movx @dptr,a
		mov	 dptr,#debug
		movx @dptr,a
		mov	 dptr,#debug
		movx @dptr,a
		mov	 dptr,#debug
		movx @dptr,a
		mov	 dptr,#debug
		movx @dptr,a
		mov	 dptr,#debug
		movx @dptr,a

over2:
    setb neg512
		clr	 IE0

over1:
    pop	 acc
		pop	 dph1
		pop	 dpl1
		pop	 dph
		pop	 dpl
		pop	 dps
		pop	 psw
		RETI
;
;
END					; end of program


