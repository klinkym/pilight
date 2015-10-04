/*
	Copyright (C) 2015 CurlyMo , wo_rasp & meloen

	This file is part of pilight.

	pilight is free software: you can redistribute it and/or modify it under the
	terms of the GNU General Public License as published by the Free Software
	Foundation, either version 3 of the License, or (at your option) any later
	version.

	pilight is distributed in the hope that it will be useful, but WITHOUT ANY
	WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
	A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with pilight. If not, see	<http://www.gnu.org/licenses/>
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "../../core/pilight.h"
#include "../../core/common.h"
#include "../../core/dso.h"
#include "../../core/log.h"
#include "../protocol.h"
#include "../../core/binary.h"
#include "../../core/gc.h"
#include "ledrf_remote.h"

#define PULSE_LEDRF_REMOTE_SHORT	380
#define PULSE_LEDRF_REMOTE_LONG	1140
#define PULSE_LEDRF_REMOTE_FOOTER	12920
#define PULSE_LEDRF_REMOTE_50	PULSE_LEDRF_REMOTE_SHORT+(PULSE_LEDRF_REMOTE_LONG-PULSE_LEDRF_REMOTE_SHORT)/2

#define LEARN_REPEATS	4
#define NORMAL_REPEATS	4
#define PULSE_MULTIPLIER	3
#define AVG_PULSE_LENGTH	PULSE_LEDRF_REMOTE_SHORT
#define MIN_PULSE_LENGTH	AVG_PULSE_LENGTH-80
#define MAX_PULSE_LENGTH	AVG_PULSE_LENGTH+380
#define RAW_LENGTH	50

static int validate(void) {
	if(ledrf_remote->rawlen == RAW_LENGTH) {
		if(ledrf_remote->raw[ledrf_remote->rawlen-1] >= (int)(PULSE_LEDRF_REMOTE_FOOTER*0.9) &&
			 ledrf_remote->raw[ledrf_remote->rawlen-1] <= (int)(PULSE_LEDRF_REMOTE_FOOTER*1.1) &&
			 ledrf_remote->raw[0] >= MIN_PULSE_LENGTH &&
			 ledrf_remote->raw[0] <= MAX_PULSE_LENGTH) {
		return 0;
		}
	}
	return -1;
}

static void createMessage(int unit, int state) {
	ledrf_remote->message = json_mkobject();
	json_append_member(ledrf_remote->message, "unit", json_mknumber(unit, 0));
	
	if(state == 1) {
		json_append_member(ledrf_remote->message, "state", json_mkstring("on"));
	} else {
		json_append_member(ledrf_remote->message, "state", json_mkstring("off"));
	}
}

static void parseCode(void) {
	int binary[RAW_LENGTH/2], x = 0;
	
	for(x=0; x<ledrf_remote->rawlen-1; x+=2) {
		if(ledrf_remote->raw[x+1] > PULSE_LEDRF_REMOTE_50) {
			binary[x/2] = 1;
		} else {
			binary[x/2] = 0;
		}
	}

	int unit = binToDecRev(binary, 0, 19);
	int state = binToDecRev(binary, 20, 23);
	
	createMessage(unit, state);
}

static void createOne(int s, int e) {
	int i;
	for(i=s;i<=e;i+=2) {
		ledrf_remote->raw[i] = PULSE_LEDRF_REMOTE_SHORT;
		ledrf_remote->raw[i+1] = PULSE_LEDRF_REMOTE_LONG;
	}
}

static void createZero(int s, int e) {
	int i;
	for(i=s;i<=e;i+=2) {
		ledrf_remote->raw[i] = PULSE_LEDRF_REMOTE_LONG;
		ledrf_remote->raw[i+1] = PULSE_LEDRF_REMOTE_SHORT;
	}
}

static void createFooter(void) {
	ledrf_remote->raw[ledrf_remote->rawlen-1] = PULSE_LEDRF_REMOTE_FOOTER;
}

static void clearCode(void) {
	createZero(1, ledrf_remote->rawlen-2);
}

static void createUnit(int unit) {
	switch (unit) {
		case 0:
			createZero(25, 30);	// 1st row
		break;
		case 1:
			createOne(25, 26);	// 2nd row
			createOne(37, 38);	// needs to be set
		break;
		case 2:
			createOne(25, 28);	// 3rd row
			createOne(37, 38);	// needs to be set
		break;
		case 3:
			createOne(27, 28);	// 4th row
		break;
		case 4:
			createOne(25, 30);	// 6th row MASTER (all)
		break;
		default:
		break;
	}
}

static void createState(int state) {
	if(state == 1) {
		createOne(20, 22);
		createZero(23);		//on
	}
}

static int createCode(JsonNode *code) {
	double itmp = -1;
	int unit = -1, state = -1;

	if(json_find_number(code, "unit", &itmp) == 0)
		unit = (int)round(itmp);
	if(json_find_number(code, "off", &itmp) == 0)
		state=0;
	else if(json_find_number(code, "on", &itmp) == 0)
		state=1;

	if(unit==-1 || state==-1) {
		logprintf(LOG_ERR, "ledrf_remote: insufficient number of arguments");
		return EXIT_FAILURE;
	} else if(unit > 3 || unit < 0) {
		logprintf(LOG_ERR, "ledrf_remote: invalid button code unit range");
		return EXIT_FAILURE;
	} else {	
		ledrf_remote->rawlen = RAW_LENGTH;
		createMessage(unit, state);
		clearCode();
		createUnit(unit);
		createState(state);
		createFooter();
	}
	return EXIT_SUCCESS;
}

static void printHelp(void) {
	printf("\t -u --unit=unit\t\t\tcontrol the device unit with this code\n");
	printf("\t -t --on\t\t\tsend an on signal to device\n");
	printf("\t -f --off\t\t\tsend an off signal to device\n");
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void ledrfremoteInit(void) {

	protocol_register(&ledrf_remote);
	protocol_set_id(ledrf_remote, "ledrf_remote");
	protocol_device_add(ledrf_remote, "ledrf_remote", "11 Key RF LED Controller");
	ledrf_remote->devtype = SWITCH;
	ledrf_remote->hwtype = RF433;
	ledrf_remote->txrpt = NORMAL_REPEATS;			 // SHORT: GT-FSI-04a range: 620... 960
	ledrf_remote->minrawlen = RAW_LENGTH;
	ledrf_remote->maxrawlen = RAW_LENGTH;
	ledrf_remote->maxgaplen = (int)(PULSE_LEDRF_REMOTE_FOOTER*0.9);
	ledrf_remote->mingaplen = (int)(PULSE_LEDRF_REMOTE_FOOTER*1.1);

	options_add(&ledrf_remote->options, 't', "on", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&ledrf_remote->options, 'f', "off", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
	options_add(&ledrf_remote->options, 'u', "unit", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^([0-3])$");

	options_add(&ledrf_remote->options, 0, "readonly", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");
	options_add(&ledrf_remote->options, 0, "confirm", OPTION_HAS_VALUE, GUI_SETTING, JSON_NUMBER, (void *)0, "^[10]{1}$");

	ledrf_remote->parseCode=&parseCode;
	ledrf_remote->createCode=&createCode;
	ledrf_remote->printHelp=&printHelp;
	ledrf_remote->validate=&validate;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "ledrf_remote";
	module->version = "0.2";
	module->reqversion = "6.0";
	module->reqcommit = "84";
}

void init(void) {
	ledrfremoteInit();
}
#endif
