#ifndef __MAIN_H__
#define __MAIN_H__

/*
This file is part of 869log (c) 2024 by kittennbfive - https://github.com/kittennbfive

AGPLv3+ and NO WARRANTY!

Please read the fine manual.
*/

#define VERSION_STR "0.1"

#define SZ_PORT_NAME_MAX 25

#define SZ_OUT_FILE_MAX 100

#define SZ_DATE_TIME_MAX 75

#define SZ_MEASUREMENT_MAX 75

#define SZ_DECODED_MAX (SZ_DATE_TIME_MAX+1+SZ_MEASUREMENT_MAX+1)

#define RESYNC_COUNT_MAX 5

typedef enum
{
	MODE_CONTINUOUS='c',
	MODE_ONCE_A_SEC='o',
	MODE_TWICE_A_SEC='t'
} sample_mode_t;

typedef struct
{
	char portname[SZ_PORT_NAME_MAX+1];
	FILE * outfile;
	sample_mode_t sample_mode;
	bool relative_time;
	bool sampling_started;
} config_t;

#endif
