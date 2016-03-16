/* 
 * apuled - show network traffic on apu LEDs
 * Copyright (c) 2013 Christian Herzog <daduke@daduke.org>
 * based on ifled by Mattias Wadman <napolium@sudac.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *                                         
 * Last modified: 20140109
 * changed LED control from mmap'd memory poking to /sys/class/led/ writes
 * in order to comply with newer kernels with CONFIG_STRICT_DEVMEM set
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <linux/kd.h>
#include <sys/utsname.h>
#include <signal.h>
#include <unistd.h>
#include <sys/io.h>
#include <sys/stat.h>
#include <sys/mman.h>


const char *banner = 
		"apuled v1.0 - (c) 2013 Christian Herzog <daduke@daduke.org>\n"
		"based on ifled by Mattias Wadman <napolium@sudac.org>\n"
		"This program is distributed under the terms of GPL.\n";
const char *help = 
		"%sUsage: %s interface [options]\n"
		"\tinterface\tInterface to monitor, for example: eth0\n"
		"Options:\n"
		"\t-c crt\t\tLED config 3 chars, num, caps and scroll-lock.\n"
		"\t\t\tr = Receive          t = Transmit\n"
		"\t\t\tu = Drop (receive)   i = Drop (transmit)\n"
		"\t\t\tj = Error (receive)  k = Error (transmit)\n"
		"\t\t\tReceive or transmit:\n"
		"\t\t\te = Error            d = Drop\n"
		"\t\t\ta = Activity         c = Collision\n"
		"\t\t\tn = None (will not touch this LED)\n"
		"\t\t\tDefault: crt\n"
		"\t-d delay\tLED update delay in ms. Default: 50\n"
		"\t-f\t\tFork program into background.\n\n";
volatile unsigned char *ptr;


#define TRUE	1
#define FALSE	0

#define IF_RX		0
#define IF_TX		1
#define IF_COLL		2
#define IF_DROP_RX 	4
#define IF_DROP_TX	5
#define IF_ERR_TX	6
#define IF_ERR_RX	7

#define IF_RXTX		8
#define IF_DROP		9
#define IF_ERR		10
#define IF_NONE		11

#define OPT_FORK	1
#define OPT_KERNEL_2_0	2

int ledfd[3];
unsigned long int if_info[8]; // current interface values.
unsigned long int l_if_info[8]; // last interface values.
unsigned char led_config[3] = {IF_COLL,IF_RX,IF_TX};
char options = 0;


void freakout(char *why) {
	fprintf(stderr,"Error: %s\n",why);
	exit(1);
}

void set_led(char mode,char led) {
	if (mode) {
		write(ledfd[led], "1", 1);
	} else {
		write(ledfd[led], "0", 1);
	}
}

void update_netproc(char *interface) {
	char b[255];
	char dummy;
	FILE *procfd;
	if ((procfd = fopen("/proc/net/dev","r")) == NULL)
		freakout("Unable to open /proc/net/dev.");
	while (fgets(b,sizeof(b),procfd) !=  NULL) {
		char *bp = b;
		
		while (*bp == ' ')
			*bp++;
		
		if (strncmp(bp,interface,strlen(interface)) == 0 && *(bp+strlen(interface)) == ':' ) {
			bp = bp+strlen(interface)+1;
			if (options & OPT_KERNEL_2_0)
				sscanf(bp,"%lu %lu %lu %lu %lu %lu %lu %lu %lu %lu",
						&if_info[IF_RX],&if_info[IF_ERR_RX],&if_info[IF_DROP_RX],
						&dummy,&dummy,&if_info[IF_TX],&if_info[IF_ERR_TX],
						&if_info[IF_DROP_TX],&dummy,&if_info[IF_COLL]);
			else
				sscanf(bp,"%lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu",
						&if_info[IF_RX],&dummy,&if_info[IF_ERR_RX],&if_info[IF_DROP_RX],
						&dummy,&dummy,&dummy,&dummy,&if_info[IF_TX],&dummy,
						&if_info[IF_ERR_TX],&if_info[IF_DROP_TX],&dummy,&if_info[IF_COLL]);	
			fclose(procfd);
			return;
		}
	}
	fclose(procfd);
	freakout("Unable to find interface.");
}

char is_changed(char temp) {
	switch(temp) {
		case IF_RXTX:
			return (((if_info[IF_TX] != l_if_info[IF_TX]) || (if_info[IF_RX] != l_if_info[IF_RX])) ? TRUE : FALSE);
		case IF_DROP:
			return (((if_info[IF_DROP_TX] != l_if_info[IF_DROP_TX]) || (if_info[IF_DROP_RX] != l_if_info[IF_DROP_RX])) ? TRUE : FALSE);
		case IF_ERR:
			return (((if_info[IF_ERR_TX] != l_if_info[IF_ERR_TX]) || (if_info[IF_ERR_RX] != l_if_info[IF_ERR_RX])) ? TRUE : FALSE);
		default:
			return (if_info[temp] != l_if_info[temp] ? TRUE : FALSE);
	}
}

void update_leds() {
	char led;
	for (led=0;led < 3;led++) {
		if(led_config[led] == IF_NONE)
			continue;
		if(is_changed(led_config[led]))
			set_led(TRUE,led);
		else
			set_led(FALSE,led);
	}
	memcpy(&l_if_info,&if_info,sizeof(if_info));
}

char select_mode(char mode) {
	switch(mode) {
		case 'r': return IF_RX;
		case 't': return IF_TX;
		case 'e': return IF_ERR;
		case 'c': return IF_COLL;
		case 'd': return IF_DROP;
		case 'a': return IF_RXTX;
		case 'u': return IF_DROP_RX;
		case 'i': return IF_DROP_TX;
		case 'j': return IF_ERR_RX;
		case 'k': return IF_ERR_TX;
		default: return IF_NONE;
	}
}

void signal_handler(int signal) {
	exit(0);
}

void fork_program() {
	pid_t program_pid;
	program_pid = fork();
	if (program_pid == -1)
		freakout("Unable to fork program.");
	if (program_pid != 0)
		exit(0);
}

int main(int argc, char *argv[]) {
	FILE *procfd;
	int delay = 50;
	struct utsname uname_dummy;
	char arg_dummy;
	
	if (argc < 3) {
		printf(help,banner,argv[0]);	
		exit(0);
	}
	for (arg_dummy=2;arg_dummy < argc;arg_dummy++) {
		if(argv[arg_dummy][0] != '-') {
			printf("Error: option: %s\n",argv[arg_dummy]);
			exit(1);
		}
		switch (argv[arg_dummy][1]) {
			case 'c':
				if(argv[arg_dummy+1] == NULL || strlen(argv[arg_dummy+1]) != 3)
					freakout("-c option needs 3 chars");
				led_config[0] = select_mode(argv[arg_dummy+1][0]);
				led_config[1] = select_mode(argv[arg_dummy+1][1]);
				led_config[2] = select_mode(argv[arg_dummy+1][2]);
				arg_dummy++;
				break;
			case 'd':
				if(argv[arg_dummy+1] == NULL)
					freakout("-d option needs an integer");
				delay = atol(argv[arg_dummy+1]);
				arg_dummy++;
				break;
			case 'f':
				options |= OPT_FORK;
				break;
			default:
				printf("Error: option: %s\n",argv[arg_dummy]);
				exit(1);
				break;
		}
	}
	if (options & OPT_FORK)
		fork_program();
	else
		printf(banner);
	signal(SIGINT,signal_handler);
	signal(SIGTERM,signal_handler);  
	update_netproc(argv[1]);
	memcpy(&l_if_info,&if_info,sizeof(if_info));


	char fd[255];
	int led;
	for (led=0;led < 3;led++) {
		sprintf(fd, "/sys/class/leds/apu:%d/brightness", led+1);
		ledfd[led] = open(fd, O_WRONLY);
	}


	while(1) {
		#if DEBUG
		printf("tx:%d rx:%d coll:%d tx_drop:%d rx_drop:%d err_tx:%d err_rx:%d\n",
				if_info[IF_TX],if_info[IF_RX],if_info[IF_COLL],if_info[IF_DROP_TX],
				if_info[IF_DROP_RX],if_info[IF_ERR_TX],if_info[IF_ERR_RX]);
		#endif
		update_netproc(argv[1]);
		update_leds();
		usleep(delay*1000);
	}
	for (led=0;led < 3;led++) {
		close(ledfd[led]);
	}
	return 0;
}
