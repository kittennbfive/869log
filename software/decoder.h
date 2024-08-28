#ifndef __DECODER_H__
#define __DECODER_H__
#include <stdint.h>
#include <stdbool.h>

/*
This file is part of 869log (c) 2024 by kittennbfive - https://github.com/kittennbfive

AGPLv3+ and NO WARRANTY!

Please read the fine manual.
*/

typedef enum
{
	PREFIX_UNKNOWN=0,
	PREFIX_NANO,
	PREFIX_MICRO,
	PREFIX_MILLI,
	PREFIX_NONE,
	PREFIX_KILO,
	PREFIX_MEGA
} value_prefix_t;

typedef enum
{
	QUANTITY_UNKNOWN=0,
	QUANTITY_VOLTAGE,
	QUANTITY_POWER,
	QUANTITY_CURRENT,
	QUANTITY_RESISTANCE,
	QUANTITY_CONDUCTIVITY,
	QUANTITY_TEMPERATURE,
	QUANTITY_FREQUENCY,
	QUANTITY_DUTY_CYCLE,
	QUANTITY_CAPACITY
} measured_quantity_t;

typedef enum
{
	VOLT_CURR_UNKNOWN=0,
	VOLT_CURR_AC,
	VOLT_CURR_DC,
	VOLT_CURR_ACDC
} voltage_current_type_t;

typedef enum
{
	MODE_TEMPERATURE_UNKNOWN=0,
	MODE_TEMPERATURE_T1,
	MODE_TEMPERATURE_T2,
	MODE_TEMPERATURE_DIFF_T1_T2
} temperature_mode_t;

typedef struct
{
	bool is_autoscale;
	bool is_hold_active;
	bool is_relative_mode;
	bool is_batt_low;
	bool is_continuity_beeper;
	bool is_vfd_mode;
	bool is_diode_test_mode;
	bool is_secondary_4_20mA;
	
	value_prefix_t primary_value_prefix;
	measured_quantity_t primary_meas_quantity;
	voltage_current_type_t primary_ac_dc;
	temperature_mode_t primary_temperature_mode;
	bool is_primary_negative;
	uint_fast8_t primary_nb_digits;
	char primary_digits[6];
	uint_fast8_t primary_dot_pos;
	
	bool is_secondary_active;
	value_prefix_t secondary_value_prefix;
	measured_quantity_t secondary_meas_quantity;
	temperature_mode_t secondary_temperature_mode;
	bool is_secondary_ac_mode;
	bool is_secondary_negative;
	uint_fast8_t secondary_nb_digits;
	char secondary_digits[4];
	uint_fast8_t secondary_dot_pos;
} lcd_t;

bool decoder(struct timespec * const timestamp, uint8_t const * const data, config_t const * const config, char * const decoded);

#endif
