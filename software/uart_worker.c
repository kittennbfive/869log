#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <err.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <poll.h>
#include <signal.h>
#include <time.h>

#include "uart_worker.h"
#include "intercom.h"
#include "fifo.h"

/*
This file is part of 869log (c) 2024 by kittennbfive - https://github.com/kittennbfive

AGPLv3+ and NO WARRANTY!

Please read the fine manual.
*/

#define DUMP_RX_TO_FILE 0 //for debug

void uart_worker(int pipe_read, int pipe_write, int uart, config_t * const config)
{
	//ignore SIGINT so uart_worker can be shutdown properly by message from interface()
	struct sigaction act;
	act.sa_handler=SIG_IGN;
	act.sa_flags=0;
	if(sigaction(SIGINT, &act, NULL)<0)
		err(1, "uart_worker: sigaction(SIGINT) failed");
	
	fifo_init();
	
	uint8_t buffer_uart[SIZE_BUFFER_RX];
	ssize_t nb_bytes;
	
	struct pollfd fds[2];
	fds[0].fd=uart;
	fds[0].events=POLLIN;
	fds[0].revents=0;
	
	fds[1].fd=pipe_read;
	fds[1].events=POLLIN;
	fds[1].revents=0;
	
	message_t msg;
	
	struct timespec timestamp;
	uint8_t lcd_data[20];
	
	bool run=true;
	bool resync_in_progress=false;
	uint_fast8_t resync_failed_counter=0;
	
	bool sampling_started=false;
	struct timespec start_time;

#if DUMP_RX_TO_FILE
	FILE * out=fopen("rx.bin", "wb");
#endif
	
	while(run)
	{
		int pollres=poll(fds, 2, -1);
		if(pollres<0)
			err(1, "uart_worker: poll() failed");
		
		if(fds[0].revents&POLLIN) //UART
		{
			nb_bytes=read(uart, buffer_uart, SIZE_BUFFER_RX);
			if(nb_bytes<=0)
				err(1, "uart_worker: read(uart) failed");

#if DUMP_RX_TO_FILE
			fwrite(buffer_uart, 1, nb_bytes, out);
#endif
			
			fifo_push(buffer_uart, (size_t)nb_bytes);
		}
		
		if(fds[1].revents&POLLIN) //PIPE
		{
			nb_bytes=read(pipe_read, &msg, sizeof(message_t));
			if(nb_bytes<0)
				err(1, "uart_worker: read(pipe) failed");
			if(nb_bytes==0)
				errx(1, "uart_worker: read(pipe) didn't return any bytes, this should not happen");
			if(nb_bytes!=sizeof(message_t))
				errx(1, "uart_worker: read(pipe) returned incomplete message");
			
			if(msg.magic!=MSG_MAGIC)
				errx(1, "uart_worker: invalid magic");
			
			switch(msg.msg_type)
			{
				case MSG_EVENT_QUIT:
					run=false;
					continue;
					break;
				
				case MSG_DO_RESYNC:
					resync_in_progress=true;
					continue;
					break;
				
				default: errx(1, "unknown message in pipe"); break; //avoid warnings
			}
		}
		
		if(resync_in_progress)
		{
			while(fifo_get_nb_entries()>=20)
			{
				fifo_peek(lcd_data, 20);
				//not sure about the 0xFF that is returned in resistance mode - is this a bug in the firmware or something undocumented? TODO investigate
				if((lcd_data[0]==0x00 || lcd_data[0]==0xFF) && lcd_data[19]==0x86)
				{
					resync_in_progress=false;
					resync_failed_counter=0;
					break;
				}
				else
				{
					fifo_remove(1);
					if(++resync_failed_counter>50)
						errx(1, "resync failed, invalid data from DMM?");
				}
			}
		}
		else
		{
			if(fifo_get_nb_entries()>=20)
			{
				if(config->relative_time && !sampling_started)
				{
					sampling_started=true;
					if(clock_gettime(CLOCK_REALTIME, &start_time)<0)
						err(1, "clock_gettime() for starting point failed");
				}
				
				if(clock_gettime(CLOCK_REALTIME, &timestamp)<0)
					err(1, "clock_gettime() failed");
				fifo_pop(lcd_data, 20*sizeof(uint8_t));
				send_message_lcd_data(pipe_write, lcd_data, &timestamp, &start_time);
			}
		}
	}
	
#if DUMP_RX_TO_FILE
	fclose(out);
#endif
	
}
