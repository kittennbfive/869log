#ifndef __INTERCOM_H__
#define __INTERCOM_H__
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

/*
This file is part of 869log (c) 2024 by kittennbfive - https://github.com/kittennbfive

AGPLv3+ and NO WARRANTY!

Please read the fine manual.
*/

//do not change!
#define PIPE_READ_END 0
#define PIPE_WRITE_END 1

#define MSG_MAGIC 0x6d656f77

typedef enum
{
	MSG_EVENT_QUIT, //interface->uart_worker (Cleanup and terminate)
	MSG_LCD_DATA, //uart_worker->interface (LCD data to be decoded)
	MSG_DO_RESYNC, //interface->uart_worker (resync on incoming data)
} msg_type_t;

typedef struct
{
	uint32_t magic;
	msg_type_t msg_type;
	
	struct timespec timestamp;
	struct timespec start_time;
	uint8_t lcd_data[20];
} message_t;

void send_message_ev_quit(int pipe_write);
void send_message_do_resync(int pipe_write);
void send_message_lcd_data(int pipe_write, uint8_t const * const data, struct timespec const * const timestamp, struct timespec const * const start_time);

#endif
