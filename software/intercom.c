#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <err.h>
#include <unistd.h>

#include "intercom.h"

/*
This file is part of 869log (c) 2024 by kittennbfive - https://github.com/kittennbfive

AGPLv3+ and NO WARRANTY!

Please read the fine manual.
*/

void send_message_ev_quit(int pipe_write)
{
	message_t msg;
	memset(&msg, 0, sizeof(msg));
	msg.magic=MSG_MAGIC;
	msg.msg_type=MSG_EVENT_QUIT;
	if(write(pipe_write, &msg, sizeof(msg))<0)
		err(1, "write() failed");
}

void send_message_do_resync(int pipe_write)
{
	message_t msg;
	memset(&msg, 0, sizeof(msg));
	msg.magic=MSG_MAGIC;
	msg.msg_type=MSG_DO_RESYNC;
	if(write(pipe_write, &msg, sizeof(msg))<0)
		err(1, "write() failed");
}

void send_message_lcd_data(int pipe_write, uint8_t const * const data, struct timespec const * const timestamp, struct timespec const * const start_time)
{
	message_t msg;
	memset(&msg, 0, sizeof(msg));
	msg.magic=MSG_MAGIC;
	msg.msg_type=MSG_LCD_DATA;
	memcpy(&msg.timestamp, timestamp, sizeof(struct timespec));
	memcpy(&msg.start_time, start_time, sizeof(struct timespec));
	memcpy(msg.lcd_data, data, 20*sizeof(uint8_t));
	if(write(pipe_write, &msg, sizeof(msg))<0)
		err(1, "write() failed");
}
