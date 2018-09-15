/*
PowerMod

Copyright 2018 J. P. Morris

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors
   may be used to endorse or promote products derived from this software
   without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <linux/input.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <alsa/asoundlib.h>
#include <alsa/seq.h>

void open_client();
void close_client();
void send_mod(unsigned int value, unsigned int channel);
void quit(const char *msg, int val);
void get_params();
void help();


static char *devicepath="/dev/input/powermate";
static int midichannel = 0;	// Default to 1
static int controllertype = 1; // Default to Modwheel
static int accel = 1;

static snd_seq_t *seqhandle = NULL;
static int seqport = 0;


int main(int argc, char *argv[]) {
struct input_event ev;
FILE *fp;
size_t len;
int val = 0;
int oldval=-1;

puts("PowerMod: modwheel controller for the Griffin PowerMate");
puts("Version 1.0");
puts("(C)2018 IT-HE Software\n");

midichannel = 0; // 1

get_params(argc, argv);


open_client();

fp=fopen(devicepath, "rb");
if(!fp) {
	close_client();
	printf("Could not open device from: %s\n", devicepath);
	exit(1);
}

printf("Device opened OK, using MIDI channel %d\n", midichannel);

for(;;)	{
	len = fread(&ev, 1, sizeof(ev), fp);
	if(len > 1) {
		if(ev.type == EV_REL && ev.code == 7) {
			if(ev.value == -1) {
				val -= accel;
				if(val < 0) {
					val = 0;
				}
			}
			if(ev.value == 1) {
				val += accel;
				if(val > 127) {
					val = 127;
				}
			}

		}
		if(ev.type == EV_KEY && ev.code == 256) {
			if(ev.value == 1) {
				val=0;
			}
		}

		if(oldval != val) {
			send_mod(val, midichannel);
		}

		oldval = val;
	} else {
		usleep(100);
	}
}


}


//
//  Send a Modwheel CC message to our virtual device
//

void send_mod(unsigned int val, unsigned int channel) {
	int err;
	snd_seq_event_t ev;

	snd_seq_ev_clear(&ev);
	snd_seq_ev_set_controller(&ev, channel & 15, controllertype & 127, val & 127);
	snd_seq_ev_set_subs(&ev);
	snd_seq_ev_set_direct(&ev);

	err=snd_seq_event_output(seqhandle, &ev);
	if(err < 0) {
		printf("Output event error = %d, %s\n", err, snd_strerror(err));
	}

	err=snd_seq_drain_output(seqhandle);
	if(err < 0) {
		printf("Drain output error = %d, %s\n", err, snd_strerror(err));
	}

	err=snd_seq_sync_output_queue(seqhandle);
	if(err < 0) {
		printf("Output queue error = %d, %s\n", err, snd_strerror(err));
	}
}


//
//  Create ALSA virtual input device for the wheel
//
void open_client() {
        snd_seq_t *handle;
        int err;
        err = snd_seq_open(&handle, "default", SND_SEQ_OPEN_OUTPUT, 0);
        if (err < 0) {
		quit("Error %d opening sequencer", err);
	}
        snd_seq_set_client_name(handle, "Griffin PowerMate modwheel");
	seqhandle = handle;

	if(NULL == seqhandle) {
		quit("Failed to open sequencer", 0);
	}
        
	seqport = snd_seq_create_simple_port(handle, "PowerMate",
                        SND_SEQ_PORT_CAP_READ|SND_SEQ_PORT_CAP_SUBS_READ,
                        SND_SEQ_PORT_TYPE_MIDI_GENERIC);
	if(seqport < 0) {
		close_client();
		quit("Failed to create ALSA port", seqport);
	}

}

//
//  Tidy up after
//
void close_client() {
	if(NULL != seqhandle) {
		snd_seq_close(seqhandle);
	}
}

//
//  Error handler, really
//

void quit(const char *msg, int val) {
printf("ERROR:\n");
printf(msg, val);
exit(1);
}


//
//  Look for command-line parameters
//

void get_params(int argc, char *argv[]) {
int ctr=0;
char *cmd;
char *parm;


for(ctr=1;ctr<argc;ctr++) {
	cmd = argv[ctr];
	parm = "";
	if(ctr + 1 < argc) {
		parm = argv[ctr+1];
	}

	if(!strcmp(cmd, "-h") || !strcmp(cmd, "--help")) {
		help();
	}

	// Set MIDI channel
	if(!strcmp(cmd, "-c")) {
		midichannel = atoi(parm);
		if(midichannel < 1 || midichannel > 16) {
			puts("ERROR: -c needs a channel from 1-16, e.g. powermod -c 12");
			exit(1);
		}
		midichannel--;	// Convert to 0-base
	}

	// Set device path
	if(!strcmp(cmd, "-d")) {
		if(parm[0] == 0) {
			puts("ERROR: -d needs a device path, e.g. powermod -d /dev/input/powermate");
			exit(1);
		}
		devicepath = parm;
	}

	// Set controller type
	if(!strcmp(cmd, "-cc")) {
		if(!parm[0]) {
			puts("ERROR: -cc needs a controller number from 0-127, e.g. powermod -cc 7");
			exit(1);
		}
		controllertype = atoi(parm);
	}

	// Set acceleration
	if(!strcmp(cmd, "-a")) {
		accel = atoi(parm);
		if(accel < 1 || accel > 8) {
			puts("ERROR: -a needs a multiplier from 1-8, e.g. powermod -a 2");
			exit(1);
		}
	}

}
}

//
//  Display help and quit
//

void help() {

	puts("This program takes input from a Griffin PowerMate or compatible device and");
	puts("outputs it as a MIDI CC control for use with synthesizers.");
	puts("By default it will function as a modwheel.  The button will zero the wheel.");
	puts("");
	puts("supported options:");
	puts("    -h    Display this help");
	puts("    -c    Set MIDI channel (defaults to 1)");
	puts("    -d    Set device path (defaults to /dev/input/powermate)");
	puts("    -cc   Set a different CC control code, e.g. -cc 7 (volume)");
	puts("    -a    Change acceleration, e.g. -a 2 is twice as fast");
	puts("");
	exit(1);

}
