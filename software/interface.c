#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <err.h>
#include <signal.h>
#include <poll.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>

#include "main.h"
#include "interface.h"
#include "intercom.h"
#include "decoder.h"

/*
This file is part of 869log (c) 2024 by kittennbfive - https://github.com/kittennbfive

AGPLv3+ and NO WARRANTY!

Please read the fine manual.
*/

static volatile bool run=true;

static void sighandler(int sig)
{
	(void)sig;
	run=false;
}

void interface(int pipe_read, int pipe_write, config_t const * const config)
{	
	struct sigaction act;
	act.sa_handler=&sighandler;
	act.sa_flags=0;
	
	if(sigaction(SIGINT, &act, NULL)<0)
		err(1, "interface: sigaction(SIGINT) failed");
	
	//if uart_worker() dies (because of err() or other) stop decoder() too
	if(sigaction(SIGCHLD, &act, NULL)<0)
		err(1, "interface: sigaction(SIGCHLD) failed");
	
	struct pollfd fds;
	fds.fd=pipe_read;
	fds.events=POLLIN;
	
	uint_fast8_t resync_count=0;
	ssize_t nb_bytes;
	message_t msg;
	struct timespec timestamp;
	char decoded[SZ_DECODED_MAX+1];
	
	while(run)
	{
		int pollres=poll(&fds, 1, -1);
		
		if(pollres<0 && errno!=EINTR)
			err(1, "interface: poll() failed");
		
		if(fds.revents&POLLIN) //poll was possibly be interrupted by Ctrl+C or similar so there might be no data in pipe
		{
			nb_bytes=read(pipe_read, &msg, sizeof(message_t));
			if(nb_bytes<0)
				err(1, "interface: read(pipe) failed");
			if(nb_bytes==0)
				errx(1, "interface: read(pipe) didn't return any bytes, this should not happen");
			if(nb_bytes!=sizeof(message_t))
				errx(1, "interface: read(pipe) returned incomplete message");
			
			if(msg.magic!=MSG_MAGIC)
				errx(1, "interface: invalid magic");
			
			if(msg.msg_type!=MSG_LCD_DATA) //only one possible message yet
				errx(1, "invalid msg_type");
			
			memcpy(&timestamp, &msg.timestamp, sizeof(struct timespec));
			
			if(config->relative_time)
			{
				if(timestamp.tv_nsec>msg.start_time.tv_nsec)
					timestamp.tv_nsec-=msg.start_time.tv_nsec;
				else
				{
					timestamp.tv_nsec=timestamp.tv_nsec+1000000000ULL-msg.start_time.tv_nsec;
					timestamp.tv_sec--;
				}
				timestamp.tv_sec-=msg.start_time.tv_sec;
			}
			
			if(decoder(&timestamp, msg.lcd_data, config, decoded))
			{
				if(++resync_count>RESYNC_COUNT_MAX)
					errx(1, "%u resync requests in series, invalid data from DMM or decoding not implemented", RESYNC_COUNT_MAX);
				send_message_do_resync(pipe_write);
			}
			else
			{
				resync_count=0;
				printf("%s\n", decoded);
				if(config->outfile)
					fprintf(config->outfile, "%s\n", decoded);
			}
		}
	}
	
	if(config->outfile)
		fclose(config->outfile);
	
	send_message_ev_quit(pipe_write);
}
