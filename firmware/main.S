#include <avr/io.h>

/*
This file is part of 869log (c) 2024 by kittennbfive - https://github.com/kittennbfive

Firmware for Attiny25

AGPLv3+ and NO WARRANTY!

Please read the fine manual.
*/


//REGISTER DEFINITIONS

zero_reg=0 //always 0
one_reg=1 //always 1

//sampling stuff
sample_mode=2 //'c', 'o', 't'
divider_sample=3
divider_sample_preset=4 //copied into divider_sample inside ISR
timeout_counter=5

//general purpose
temp=16
temp2=17
temp3=18
temp4=19
arg0_ret=20
arg1=21

//for status LED control
divider_status_led=23
counter_timeout_led=24

//UART
rx_data=25

//caution X-Y-Z starting at 26
//27:26 X -> wait_11ms
//29:28 Y -> unused
//31:30 Z -> load/store data


//DEFINES

//GPIOR2 is flag register
#define FLAG_RX_HAS_BYTE 0
#define FLAG_SAMPLING_DO_SAMPLE 1
#define FLAG_INHIBIT_STATUS_BLINK 2

//STATUS LED
#define STATUS_LED_DDR DDRB
#define STATUS_LED_PORT PORTB
#define STATUS_LED_PIN PINB
#define STATUS_LED PB0
#define NB_TIMEOUTS_STATUS_BLINKY 5

//UART
#define UART_DDR DDRB
#define UART_PORT PORTB
#define UART_PIN PINB
#define UART_OUT PB1
#define UART_IN PB2

//INTERFACE TO DMM
#define DMM_TX_DDR DDRB
#define DMM_TX_PORT PORTB
#define DMM_TX PB3
#define DMM_RX_DDR DDRB
#define DMM_RX_PIN PINB
#define DMM_RX PB4

//DIVIDER FOR TIMER0
#define DIVIDER_CONT_SAMPLE 1
#define DIVIDER_SAMPLE_ONCE_SEC 50
#define DIVIDER_SAMPLE_TWICE_SEC 25
#define TIMEOUT_INITIAL_VALUE 15 //300ms
#define DIVIDER_ALIVE_STATUS_LED 75

//----------------------------------------------------------------------
//-------------------------------- INT ---------------------------------

.text

//INT0
.global __vector_1

/*
This vector is used during calibration and also for regular UART-RX, so once calibration is done
GPIOR1 is set to 0xff which makes the ISR execute the "user code" part for UART-RX.
*/ 

__vector_1:
	push temp
	in temp, _SFR_IO_ADDR(SREG)
	push temp
	push temp2
	in temp2, _SFR_IO_ADDR(GPIOR1)
	cpi temp2, 0xff
	breq vector1_user_code
	out _SFR_IO_ADDR(GTCCR), zero_reg
	in temp, _SFR_IO_ADDR(GPIOR0)
	add temp, one_reg
	adc temp2, zero_reg
	out _SFR_IO_ADDR(GPIOR0), temp
	out _SFR_IO_ADDR(GPIOR1), temp2
	rjmp vector1_end
vector1_user_code:
	//UART RX
	
	//wait for middle of bit
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	
	//valid startbit?
	sbic _SFR_IO_ADDR(UART_PIN), UART_IN
	rjmp vector1_end
	
	ldi temp2, 18
1:
	dec temp2
	brne 1b
	
	clr rx_data
	
	ldi temp, 8
vector1_user_code_rx_loop:
	lsr rx_data
	sbic _SFR_IO_ADDR(UART_PIN), UART_IN
	sbr rx_data, (1<<7)
	
	ldi temp2, 19
1:
	dec temp2
	brne 1b
	
	dec temp
	brne vector1_user_code_rx_loop	
	
	//valid stopbit?
	sbic _SFR_IO_ADDR(UART_PIN), UART_IN
	sbi _SFR_IO_ADDR(GPIOR2), FLAG_RX_HAS_BYTE
	
	ldi temp2, 20
1:
	dec temp2
	brne 1b
	
vector1_end:
	//reset INT-flag, this shoule be done automatically??
	//if not done manually interrupt will trigger twice?!?
	in temp, _SFR_IO_ADDR(GIFR)
	ori temp, (1<<INTF0)
	out _SFR_IO_ADDR(GIFR), temp

	pop temp2
	pop temp
	out _SFR_IO_ADDR(SREG), temp
	pop temp
	
	reti


//TIMER0 CTC A 50Hz
.global __vector_10

__vector_10:
	push temp
	in temp, _SFR_IO_ADDR(SREG)
	push temp
	dec divider_sample
	brne 1f
	mov divider_sample, divider_sample_preset
	sbi _SFR_IO_ADDR(GPIOR2), FLAG_SAMPLING_DO_SAMPLE
1:
	tst timeout_counter
	breq 1f
	dec timeout_counter
1:
	sbic _SFR_IO_ADDR(GPIOR2), FLAG_INHIBIT_STATUS_BLINK
	rjmp 1f
	dec divider_status_led
	brne 1f
	ldi divider_status_led, DIVIDER_ALIVE_STATUS_LED
	sbi _SFR_IO_ADDR(STATUS_LED_PIN), STATUS_LED
1:
	pop temp
	out _SFR_IO_ADDR(SREG), temp
	pop temp
	reti

//----------------------------------------------------------------------
//------------------------------- MAIN ---------------------------------

.global main

main:
	clr zero_reg
	ldi temp, 1
	mov one_reg, temp
	
	sbi _SFR_IO_ADDR(STATUS_LED_DDR), STATUS_LED

	rcall calibrate_freq //call this first, will either read OSCCAL from EEPROM or make a new calibration (needs input from UART) - caution LED must be enabled (DDRx=1) for blinky-blinky messages
	
	rcall sw_uart_init
	
	rcall timer0_init
	
	sbi _SFR_IO_ADDR(DMM_TX_DDR), DMM_TX
	
	ldi temp, 'c' //by default sample data from DMM as fast/often as possible
	mov sample_mode, temp
	
	mov divider_sample_preset, one_reg
	
	mov divider_sample, divider_sample_preset
	
	ldi divider_status_led, DIVIDER_ALIVE_STATUS_LED
	
	ldi counter_timeout_led, NB_TIMEOUTS_STATUS_BLINKY
	sbi _SFR_IO_ADDR(GPIOR2), FLAG_INHIBIT_STATUS_BLINK
	
	sei
	
mainloop:
	
	sbis _SFR_IO_ADDR(GPIOR2), FLAG_RX_HAS_BYTE
	rjmp wait_sample
	
	cbi _SFR_IO_ADDR(GPIOR2), FLAG_RX_HAS_BYTE
	
	cpi rx_data, 'c'
	brne 1f
	ldi temp, DIVIDER_CONT_SAMPLE
	mov divider_sample_preset, temp
	rjmp wait_sample
1:
	cpi rx_data, 'o'
	brne 1f
	ldi temp, DIVIDER_SAMPLE_ONCE_SEC
	mov divider_sample_preset, temp
	rjmp wait_sample
1:
	cpi rx_data, 't'
	brne 1f
	ldi temp, DIVIDER_SAMPLE_TWICE_SEC
	mov divider_sample_preset, temp
1:

wait_sample:
	sbis _SFR_IO_ADDR(GPIOR2), FLAG_SAMPLING_DO_SAMPLE
	rjmp mainloop
	
	cbi _SFR_IO_ADDR(GPIOR2), FLAG_SAMPLING_DO_SAMPLE
	
	rcall do_sample
	
	rjmp mainloop


//----------------------------------------------------------------------
//-------------------------------- SUBS ---------------------------------

//actual sample / DMM-interface code

do_sample:	
	//sync / "data request"
	sbi _SFR_IO_ADDR(DMM_TX_PORT), DMM_TX
	rcall wait_11ms
	cbi _SFR_IO_ADDR(DMM_TX_PORT), DMM_TX
	
	ldi temp, TIMEOUT_INITIAL_VALUE
	mov timeout_counter, temp

1:
	sbis _SFR_IO_ADDR(DMM_RX_PIN), DMM_RX
	rjmp do_sample_inout
	
	tst timeout_counter
	breq do_sample_timeout
	
	rjmp 1b

do_sample_inout:
	ldi counter_timeout_led, NB_TIMEOUTS_STATUS_BLINKY
	sbi _SFR_IO_ADDR(GPIOR2), FLAG_INHIBIT_STATUS_BLINK
	sbi _SFR_IO_ADDR(STATUS_LED_PORT), STATUS_LED

	ldi Zl, lo8(sampled_data)
	ldi Zh, hi8(sampled_data)
	ldi temp, 20
do_sample_byte:
	clr temp2
	ldi temp3, 8
do_sample_bits:
	sbi _SFR_IO_ADDR(DMM_TX_PORT), DMM_TX
	rcall wait_250us
	lsr temp2
	sbic _SFR_IO_ADDR(DMM_RX_PIN), DMM_RX
	sbr temp2, (1<<7)
	cbi _SFR_IO_ADDR(DMM_TX_PORT), DMM_TX
	rcall wait_250us
	dec temp3
	brne do_sample_bits
	st Z+, temp2
	dec temp
	brne do_sample_byte
		
	cbi _SFR_IO_ADDR(STATUS_LED_PORT), STATUS_LED
	
	//transmit via UART
	ldi Zl, lo8(sampled_data)
	ldi Zh, hi8(sampled_data)
	ldi temp, 20
1:
	ld arg0_ret, Z+
	rcall sw_uart_putchar
	dec temp
	brne 1b
	
	rjmp do_sample_end
	
do_sample_timeout:
	dec counter_timeout_led
	brne do_sample_end
	ldi counter_timeout_led, NB_TIMEOUTS_STATUS_BLINKY
	cbi _SFR_IO_ADDR(GPIOR2), FLAG_INHIBIT_STATUS_BLINK
do_sample_end:
	ret

wait_11ms: //about 11ms - caution uses X-register
	ldi Xl, lo8(21000)
	ldi Xh, hi8(21000)
1:
	sbiw Xl, 1
	brne 1b
	ret

wait_250us: //about 250µs - quick and dirty...
	push temp
	ldi temp, 230
1:
	nop
	nop
	nop
	nop
	nop
	dec temp
	brne 1b
	pop temp
	ret

//----------------------------------------------------------------------

//EEPROM stuff, straight from datasheet, nothing fancy here

eeprom_read: //arg0=addr
	out _SFR_IO_ADDR(EEAR), arg0_ret
	sbi _SFR_IO_ADDR(EECR), EERE
	in arg0_ret, _SFR_IO_ADDR(EEDR)
	ret

eeprom_write: //arg0=addr, arg1=data
	out _SFR_IO_ADDR(EECR), zero_reg
	out _SFR_IO_ADDR(EEAR), arg0_ret
	out _SFR_IO_ADDR(EEDR), arg1
	sbi _SFR_IO_ADDR(EECR), EEMPE
	sbi _SFR_IO_ADDR(EECR), EEPE
eeprom_write_wait:
	sbis _SFR_IO_ADDR(EECR), EEPE 
	ret
	rjmp eeprom_write_wait
	
//----------------------------------------------------------------------

/*
calibration stuff

magic-signature in EEPROM is 0x55, 0xAA, <OSCCAL>
GPIOR1:GPIOR0 is count-"variable" for INT0
if(GPIOR1==0xff) -> run user code inside INT0 == vector 1
*/

//for approx. 7,3728MHz with UART 0x55==\125=='U' @115200 8N1 and Timer0 prescaler 1024
//DO NOT CHANGE THESE!
#define TARGET_TIMER_VAL 127
#define TARGET_CNT_VAL 1016

calibrate_freq:
	//check if EEPROM contains valid signature
	ldi arg0_ret, 0x00
	rcall eeprom_read
	cpi arg0_ret, 0x55
	brne calibrate_freq_do_cal
	ldi arg0_ret, 0x01
	rcall eeprom_read
	cpi arg0_ret, 0xAA
	brne calibrate_freq_do_cal
	//yes it does -> read OSCCAL and return
	ldi arg0_ret, 0x02
	rcall eeprom_read
	out _SFR_IO_ADDR(OSCCAL), arg0_ret
	rjmp calibrate_freq_end
	//no it does not -> do calibration
calibrate_freq_do_cal:
	rcall blink_do_calibration_code
	//keep timer in hold/reset
	ldi temp, (1<<TSM)|(1<<PSR0)
	out _SFR_IO_ADDR(GTCCR), temp
	//set up Timer0, free-running, prescaler 1024
	ldi temp, (1<<CS02)|(1<<CS00)
	out _SFR_IO_ADDR(TCCR0B), temp
	//INT0 rising edge
	ldi temp, (1<<ISC01)|(1<<ISC00)
	out _SFR_IO_ADDR(MCUCR), temp
	//enable INT0
	in temp, _SFR_IO_ADDR(GIMSK)
	ori temp, (1<<INT0)
	out _SFR_IO_ADDR(GIMSK), temp
	//start at middle of lowest frequency range (MSB=0)
	ldi temp, 0x7f/2
	out _SFR_IO_ADDR(OSCCAL), temp
	
	out _SFR_IO_ADDR(GPIOR0), zero_reg
	out _SFR_IO_ADDR(GPIOR1), zero_reg
	out _SFR_IO_ADDR(TCNT0), zero_reg
	
	//for compare later
	ldi temp3, lo8(TARGET_CNT_VAL)
	ldi temp4, hi8(TARGET_CNT_VAL)
	
calibrate_freq_wait_loop:
	cli //needed for atomic access
	in temp, _SFR_IO_ADDR(GPIOR0)
	in temp2, _SFR_IO_ADDR(GPIOR1)
	sei
	cp temp, temp3
	cpc temp2, temp4
	brlo calibrate_freq_wait_loop
	
	//loop exited, disable interrupts, stop timer, clear counter-regs
	cli
	out _SFR_IO_ADDR(TCCR0B), zero_reg
	out _SFR_IO_ADDR(GPIOR1), zero_reg
	out _SFR_IO_ADDR(GPIOR0), zero_reg
	//check TCNT0 value
	in temp, _SFR_IO_ADDR(TCNT0)
	cpi temp, TARGET_TIMER_VAL
	in temp, _SFR_IO_ADDR(OSCCAL) //does not change SREG
	breq calibrate_freq_found_ok //yes!
	brlo calibrate_freq_cmp_lower
	//OSCCAL--;
	tst temp
	breq calibrate_freq_failed //already 0 -> fail
	dec temp
	rjmp calibrate_freq_prepare_next
calibrate_freq_cmp_lower:
	//OSCCAL++;
	cpi temp, 0x7f
	breq calibrate_freq_failed //already max == 0x7f -> fail
	inc temp
calibrate_freq_prepare_next:
	out _SFR_IO_ADDR(OSCCAL), temp
	//prepare for next round...
	out _SFR_IO_ADDR(TCNT0), zero_reg
	ldi temp, (1<<TSM)|(1<<PSR0)
	out _SFR_IO_ADDR(GTCCR), temp
	ldi temp, (1<<CS02)|(1<<CS00)
	out _SFR_IO_ADDR(TCCR0B), temp
	rjmp calibrate_freq_wait_loop
	
calibrate_freq_found_ok:
	//disable INT0
	in temp, _SFR_IO_ADDR(GIMSK)
	andi temp, ~(1<<INT0)
	out _SFR_IO_ADDR(GIMSK), temp
	//write "magic" signature and OSCCAL value to EEPROM
	ldi arg0_ret, 0x00
	ldi arg1, 0x55
	rcall eeprom_write
	ldi arg0_ret, 0x01
	ldi arg1, 0xAA
	rcall eeprom_write
	ldi arg0_ret, 0x02
	in arg1, _SFR_IO_ADDR(OSCCAL)
	rcall eeprom_write
	rjmp calibrate_freq_end
	
calibrate_freq_failed: //while(1)
	rcall blink_error_code
	rjmp calibrate_freq_failed
	
calibrate_freq_end:
	//set GPIOR1 to 0xff to run user code in __vector_1 == INT0
	ldi temp, 0xff
	out _SFR_IO_ADDR(GPIOR1), temp
	ret
	
//----------------------------------------------------------------------

//calibration needed, blink code on LED to tell user

//blink 3 times slowly

blink_do_calibration_code:
	ldi temp, 3
1:
	sbi _SFR_IO_ADDR(STATUS_LED_PORT), STATUS_LED
	ldi arg0_ret, 20
	rcall wait_blink
	cbi _SFR_IO_ADDR(STATUS_LED_PORT), STATUS_LED
	ldi arg0_ret, 40
	rcall wait_blink
	dec temp
	brne 1b
	ret

//----------------------------------------------------------------------

//calibration failed, blink error code on LED - called inside a while(1) from calibrate-code

//blink (continously) fast(er)

blink_error_code:
	sbi _SFR_IO_ADDR(STATUS_LED_PORT), STATUS_LED
	ldi arg0_ret, 10
	rcall wait_blink
	cbi _SFR_IO_ADDR(STATUS_LED_PORT), STATUS_LED
	ldi arg0_ret, 10
	rcall wait_blink
	ret

//----------------------------------------------------------------------

//software UART stuff

sw_uart_init:
	sbi _SFR_IO_ADDR(UART_DDR), UART_OUT
	sbi _SFR_IO_ADDR(UART_PORT), UART_OUT //idle high
	
	//INT0 on falling edge
	ldi temp, (1<<ISC01)
	out _SFR_IO_ADDR(MCUCR), temp
	ldi temp, (1<<INT0)
	out _SFR_IO_ADDR(GIMSK), temp
	
	ret

sw_uart_putchar: //arg0_ret contains byte to TX
	push temp
	push temp2
	cbi _SFR_IO_ADDR(UART_PORT), UART_OUT //startbit
	ldi temp, 19 ;1
1:
	dec temp ;1
	brne 1b ;2
	
	ldi temp2, 8
sw_uart_putchar_loop:
	sbrc arg0_ret, 0
	rjmp sw_uart_putchar_set
	cbi _SFR_IO_ADDR(UART_PORT), UART_OUT
	rjmp 1f
sw_uart_putchar_set:
	sbi _SFR_IO_ADDR(UART_PORT), UART_OUT
	nop
	nop
1:
	lsr arg0_ret
	
	ldi temp, 18 ;1
1:
	dec temp ;1
	brne 1b ;2
	
	dec temp2
	brne sw_uart_putchar_loop
	sbi _SFR_IO_ADDR(UART_PORT), UART_OUT //stopbit
	
	ldi temp, 20 ;1
1:
	dec temp ;1
	brne 1b ;2
	pop temp2
	pop temp
	ret


//----------------------------------------------------------------------

//TIMER0 (8 bit!) init: prescaler 1024 CTC 144 --> INT 50x/s

timer0_init:
	ldi temp, 144
	out _SFR_IO_ADDR(OCR0A), temp
	
	ldi temp, (1<<WGM01)
	out _SFR_IO_ADDR(TCCR0A), temp
	
	ldi temp, (1<<CS02)|(1<<CS00)
	out _SFR_IO_ADDR(TCCR0B), temp
	
	ldi temp, (1<<OCIE0A)
	out _SFR_IO_ADDR(TIMSK), temp
	
	ret

//----------------------------------------------------------------------

//delay for calibration related blinky-blinky-stuff, not calibrated in any way

wait_blink: //caution uses X-register - arg0_ret contains outer counter value for variable delay (1-255)
	ser Xl
	ser Xh
1:
	sbiw Xl, 1
	brne 1b
	dec arg0_ret
	brne wait_blink
	ret


//----------------------------------------------------------------------
//-------------------------------- SRAM --------------------------------

.data

.org 0

sampled_data:
	.zero 20
