#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <err.h>
#include <time.h>

#include "main.h"
#include "decoder.h"

/*
This file is part of 869log (c) 2024 by kittennbfive - https://github.com/kittennbfive

AGPLv3+ and NO WARRANTY!

Please read the fine manual.

NOTE: This is the actual decoder. It is a bit of a mess, incomplete and almost untested! You may need to add stuff depending on your use case. Consider output with caution!

BUG: dot is at the wrong place for QUANTITY_DUTY_CYCLE
BUG: missing dot for QUANTITY_RESISTANCE and open input ("OL")
TODO: in resistance mode the meter returns 0xFF instead of 0x00 as first byte?! bug in firmware or undocumented behaviour? needs investigation
*/

void decode_byte1(lcd_t * const lcd, const uint8_t byte)
{
	if(byte&(1<<0))
		lcd->is_autoscale=true;
	
	if(byte&(1<<3))
		lcd->is_hold_active=true;
	
	if(byte&(1<<4))
		lcd->primary_ac_dc=VOLT_CURR_DC;
}

void decode_byte2(lcd_t * const lcd, const uint8_t byte)
{
	if(byte&(1<<0))
	{
		if(lcd->primary_ac_dc==VOLT_CURR_UNKNOWN)
			lcd->primary_ac_dc=VOLT_CURR_AC;
		else
			lcd->primary_ac_dc=VOLT_CURR_ACDC;
	}
	
	if(byte&(1<<1))
	{
		lcd->primary_meas_quantity=QUANTITY_TEMPERATURE;
		lcd->primary_temperature_mode=MODE_TEMPERATURE_T1;
	}
	
	if(byte&(1<<3))
	{
		lcd->primary_meas_quantity=QUANTITY_TEMPERATURE;
		if(lcd->primary_temperature_mode==MODE_TEMPERATURE_UNKNOWN)
			lcd->primary_temperature_mode=MODE_TEMPERATURE_T2;
		else
			lcd->primary_temperature_mode=MODE_TEMPERATURE_DIFF_T1_T2;
	}
		
	if(byte&(1<<6))
		lcd->is_vfd_mode=true;
	
	if(byte&(1<<7))
		lcd->is_primary_negative=true;
}

void decode_byte9(lcd_t * const lcd, const uint8_t byte)
{
	if(byte&(1<<0))
		lcd->secondary_value_prefix=PREFIX_MICRO;
	
	if(byte&(1<<1))
		lcd->secondary_value_prefix=PREFIX_MILLI;
	
	if(byte&(1<<2))
		lcd->secondary_meas_quantity=QUANTITY_CURRENT;
	
	if(byte&(1<<3))
		lcd->is_secondary_4_20mA=true;
	
	if(byte&(1<<4))
		lcd->is_secondary_negative=true;
	
	if(byte&(1<<5))
		lcd->is_secondary_ac_mode=true;
	
	if(byte&(1<<6))
	{
		lcd->secondary_meas_quantity=QUANTITY_TEMPERATURE;
		lcd->secondary_temperature_mode=MODE_TEMPERATURE_T2;
	}
	
	if(byte&(1<<7))
		lcd->is_batt_low=true;
}

void decode_byte14(lcd_t * const lcd, const uint8_t byte)
{
	if(byte&(1<<0))
		lcd->secondary_value_prefix=PREFIX_MEGA;
	
	if(byte&(1<<1))
		lcd->secondary_value_prefix=PREFIX_KILO;
	
	if(byte&(1<<2))
		lcd->secondary_meas_quantity=QUANTITY_FREQUENCY;
	
	if(byte&(1<<3))
		lcd->secondary_meas_quantity=QUANTITY_VOLTAGE;
	
	if(byte&(1<<4))
		lcd->primary_meas_quantity=QUANTITY_CONDUCTIVITY;
	
	if(byte&(1<<5))
		lcd->primary_meas_quantity=QUANTITY_CAPACITY;
	
	if(byte&(1<<6))
		lcd->primary_value_prefix=PREFIX_NANO;
	
	if(byte&(1<<7))
		lcd->primary_meas_quantity=QUANTITY_CURRENT;
}
void decode_byte15(lcd_t * const lcd, const uint8_t byte)
{
	if(byte&(1<<0))
		lcd->primary_meas_quantity=QUANTITY_FREQUENCY;
	
	if(byte&(1<<1))
		lcd->primary_meas_quantity=QUANTITY_POWER;
	
	if(byte&(1<<2))
		lcd->primary_value_prefix=PREFIX_MILLI;
	
	if(byte&(1<<3))
		lcd->primary_value_prefix=PREFIX_MICRO;
	
	if(byte&(1<<4))
		lcd->primary_meas_quantity=QUANTITY_RESISTANCE;
	
	if(byte&(1<<5))
		lcd->primary_value_prefix=PREFIX_MEGA;
	
	if(byte&(1<<6))
		lcd->primary_value_prefix=PREFIX_KILO;
	
	if(byte&(1<<7))
		lcd->primary_meas_quantity=QUANTITY_DUTY_CYCLE;
}

typedef struct
{
	uint8_t byte;
	char symbol;
} digit_t;

bool decode_digit(lcd_t * const lcd, const uint_fast8_t nb, uint8_t byte)
{
	bool dot=byte&(1<<0);
	byte&=~(1<<0);
	
	if(nb>6 && !lcd->is_secondary_active)
		return false;
	
	if(byte==0x00)
	{
		if(nb>6)
			lcd->is_secondary_active=false;
		return false;
	}
	
	const digit_t digit_data[]=
	{
		{ 0xBE, '0' },
		{ 0xA0, '1' },
		{ 0xDA, '2' },
		{ 0xF8, '3' },
		{ 0xE4, '4' },
		{ 0x7C, '5' },
		{ 0x7E, '6' },
		{ 0xA8, '7' },
		{ 0xFE, '8' },
		{ 0xFC, '9' },
		{ 0x1E, 'C' },
		{ 0x4E, 'F' },
		{ 0x40, '-' },
		{ 0x16, 'L' },
		{ 0xF2, 'd' },
		{ 0x20, 'i' },
		{ 0x72, 'o' }
	};
	
	uint_fast8_t i;
	for(i=0; i<sizeof(digit_data)/sizeof(digit_t); i++)
	{
		if(byte==digit_data[i].byte)
		{
			if(dot)
			{
				if(nb==1)
					lcd->is_relative_mode=true;
				else if(nb==7)
					lcd->is_continuity_beeper=true;
				else if(nb<7)
					lcd->primary_dot_pos=nb-1;
				else
					lcd->secondary_dot_pos=nb-6-1;
			}
			
			if(nb<7)
				lcd->primary_digits[lcd->primary_nb_digits++]=digit_data[i].symbol;
			else
				lcd->secondary_digits[lcd->secondary_nb_digits++]=digit_data[i].symbol;
			
			if(byte==0xF2)
				lcd->is_diode_test_mode=true;
			
			return false;
		}
	}
	
	return true;
}

void decode_screen(lcd_t const * const lcd, char * const decoded)
{
	const char * value_prefix_str[]={"<ERROR: UNDEFINED VALUE PREFIX>", "n", "u", "m", "", "k", "M"};
	const char * meas_quantity_unit_str[]={"<ERROR: UNDEFINED MEAS QUANTITY/UNIT>", "V", "dBm", "A", "R", "S", "" /*unit C/F shown as part of digit*/, "Hz", "D%", "F" /*as in Farad*/};
	const char * voltage_current_type_str[]={"<ERROR: UNDEFINED VOLTAGE/CURRENT TYPE>", "AC", "DC", "ACDC"};
	const char * temperature_mode_str[]={"<ERROR: UNDEFINED TEMPERATURE MEAS MODE>", "(T1)", "(T2)", "(T1-T2)"};
	
	decoded[0]='\0';
	
	char buf[100];
	
	if(lcd->is_hold_active)
		strcat(decoded, "HOLD! ");

	if(lcd->is_relative_mode)
		strcat(decoded, "REL! ");
	
	if(lcd->is_batt_low)
		strcat(decoded, "!LOW BATT! ");
	
	if(lcd->is_vfd_mode)
		strcat(decoded, "VFD ");
	
	if(lcd->is_primary_negative)
		strcat(decoded, "-");
	
	uint_fast8_t i;
	for(i=0; i<lcd->primary_nb_digits; i++)
	{
		if(lcd->primary_dot_pos==i)
			strcat(decoded, ".");
		buf[0]=lcd->primary_digits[i];
		buf[1]='\0';
		strcat(decoded, buf);
	}
	
	if(lcd->primary_meas_quantity!=QUANTITY_POWER)
		strcat(decoded, value_prefix_str[(uint)lcd->primary_value_prefix]);
	
	if(lcd->is_diode_test_mode)
		strcat(decoded, " ");
	
	strcat(decoded, meas_quantity_unit_str[(uint)lcd->primary_meas_quantity]);
	strcat(decoded, " ");
	
	if(lcd->primary_meas_quantity==QUANTITY_TEMPERATURE)
		strcat(decoded, temperature_mode_str[(uint)lcd->primary_temperature_mode]);
	
	if((lcd->primary_meas_quantity==QUANTITY_VOLTAGE || lcd->primary_meas_quantity==QUANTITY_CURRENT) && !lcd->is_diode_test_mode)
	{
		strcat(decoded, "(");
		strcat(decoded, voltage_current_type_str[(uint)lcd->primary_ac_dc]);
		strcat(decoded, ") ");
	}
	
	if(!lcd->is_secondary_active)
		return;
	
	strcat(decoded, " ");
	
	if(lcd->is_secondary_negative)
		strcat(decoded, "-");
	
	for(i=0; i<lcd->secondary_nb_digits; i++)
	{
		if(lcd->secondary_dot_pos==i)
			strcat(decoded, ".");
		buf[0]=lcd->secondary_digits[i];
		buf[1]='\0';
		strcat(decoded, buf);
	}
	
	if(!lcd->is_diode_test_mode)
	{
		strcat(decoded, value_prefix_str[(uint)lcd->secondary_value_prefix]);
		
		if(lcd->is_secondary_4_20mA)
			strcat(decoded, "%(4-20mA)");
		else
			strcat(decoded, meas_quantity_unit_str[(uint)lcd->secondary_meas_quantity]);
		
		strcat(decoded, " ");
	}
	
	if(lcd->secondary_meas_quantity==QUANTITY_TEMPERATURE)
		strcat(decoded, temperature_mode_str[(uint)lcd->secondary_temperature_mode]);
	
	if(lcd->is_secondary_ac_mode)
		strcat(decoded, "(AC)");
}

bool decoder(struct timespec * const timestamp, uint8_t const * const data, config_t const * const config, char * const decoded)
{
	lcd_t lcd;
	memset(&lcd, 0, sizeof(lcd_t));
	//set what seems to be assumed defaults?
	lcd.primary_value_prefix=PREFIX_NONE;
	lcd.primary_meas_quantity=QUANTITY_VOLTAGE;
	lcd.primary_dot_pos=0xFF; //no dot
	lcd.is_secondary_active=true;
	lcd.secondary_value_prefix=PREFIX_NONE;
	lcd.secondary_meas_quantity=QUANTITY_VOLTAGE;
	lcd.secondary_dot_pos=0xFF; //no dot
	
	//not sure about the 0xFF that is returned in resistance mode - is this a bug in the firmware or something undocumented? TODO investigate
	if(data[0]!=0x00 && data[0]!=0xFF) //sync
		goto is_error;
	
	if(data[19]!=0x86) //model ID
		goto is_error;
	
	decode_byte1(&lcd, data[1]);
	decode_byte2(&lcd, data[2]);
	
	if(decode_digit(&lcd, 1, data[3]))
		goto is_error;
	if(decode_digit(&lcd, 2, data[4]))
		goto is_error;
	if(decode_digit(&lcd, 3, data[5]))
		goto is_error;
	if(decode_digit(&lcd, 4, data[6]))
		goto is_error;
	if(decode_digit(&lcd, 5, data[7]))
		goto is_error;
	if(decode_digit(&lcd, 6, data[8]))
		goto is_error;
	
	decode_byte9(&lcd, data[9]);
	
	if(decode_digit(&lcd, 7, data[10]))
		goto is_error;
	if(decode_digit(&lcd, 8, data[11]))
		goto is_error;
	if(decode_digit(&lcd, 9, data[12]))
		goto is_error;
	if(decode_digit(&lcd, 10, data[13]))
		goto is_error;
	
	decode_byte14(&lcd, data[14]);
	decode_byte15(&lcd, data[15]);

	struct tm * loctime;
	loctime=localtime(&timestamp->tv_sec);
	
	char datetime[SZ_DATE_TIME_MAX+1];
	char measurement[SZ_MEASUREMENT_MAX+1];
		
	if(!config->relative_time)
	{
		snprintf(datetime, SZ_DATE_TIME_MAX, "%02d.%02d.%04d %02d:%02d:%02d.%03ld: ", loctime->tm_mday, loctime->tm_mon+1, loctime->tm_year+1900, loctime->tm_hour, loctime->tm_min, loctime->tm_sec, timestamp->tv_nsec/1000000);
		datetime[SZ_DATE_TIME_MAX]='\0';
	}
	else
	{
		snprintf(datetime, SZ_DATE_TIME_MAX, "%02d:%02d:%02d.%03ld: ", loctime->tm_hour-1, loctime->tm_min, loctime->tm_sec, timestamp->tv_nsec/1000000);
		datetime[SZ_DATE_TIME_MAX]='\0';
	}
		
	decode_screen(&lcd, measurement);
	
	snprintf(decoded, SZ_DECODED_MAX, "%s%s", datetime, measurement);
	
	return false;
	
is_error:
	return true;
}
