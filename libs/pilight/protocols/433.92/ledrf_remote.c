/*
	Copyright (C) 2014 CurlyMo & wo_rasp

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

#define PULSE_LEDRF_REMOTE_SHORT	700
#define PULSE_LEDRF_REMOTE_LONG	1400
#define PULSE_LEDRF_REMOTE_FOOTER	81000
#define PULSE_LEDRF_REMOTE_50	PULSE_LEDRF_REMOTE_SHORT+(PULSE_LEDRF_REMOTE_LONG-PULSE_LEDRF_REMOTE_SHORT)/2

#define LEARN_REPEATS			4
#define NORMAL_REPEATS		4
#define PULSE_MULTIPLIER	2
#define AVG_PULSE_LENGTH	PULSE_LEDRF_REMOTE_SHORT
#define MIN_PULSE_LENGTH	AVG_PULSE_LENGTH-80
#define MAX_PULSE_LENGTH	AVG_PULSE_LENGTH+260
#define RAW_LENGTH				42

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

static void createMessage(int id, int state, int unit, int all, int learn) {
	ledrf_remote->message = json_mkobject();
	json_append_member(ledrf_remote->message, "id", json_mknumber(id, 0));
	if(all == 1) {
		json_append_member(ledrf_remote->message, "all", json_mknumber(all, 0));
	} else {
		json_append_member(ledrf_remote->message, "unit", json_mknumber(unit, 0));
	}

	if(state == 1) {
		json_append_member(ledrf_remote->message, "state", json_mkstring("on"));
	} else {
		json_append_member(ledrf_remote->message, "state", json_mkstring("off"));
	}

	if(learn == 1) {
		ledrf_remote->txrpt = LEARN_REPEATS;
	} else {
		ledrf_remote->txrpt = NORMAL_REPEATS;
	}
}

static void parseCode(void) {
	int binary[RAW_LENGTH/2], x = 0, dec_unit[4] = {0, 3, 1, 2};
	int iParity=1, iParityData=-1; // init for even parity

	for(x=0; x<ledrf_remote->rawlen-1; x+=2) {
		if(ledrf_remote->raw[x+1] > PULSE_LEDRF_REMOTE_50) {
			binary[x/2] = 1;
			if((x / 2) > 11 && (x / 2) < 19) {
				iParityData = iParity;
				iParity = -iParity;
			}
		} else {
			binary[x/2] = 0;
		}
	}

	if(iParityData < 0)
		iParityData=0;

	int id = binToDecRev(binary, 0, 11);
	int unit = binToDecRev(binary, 12, 13);
	int all = binToDecRev(binary, 14, 14);
	int state = binToDecRev(binary, 15, 15);
	int dimm = binToDecRev(binary, 16, 16);
	int parity = binToDecRev(binary, 19, 19);
	int learn = 0;

	unit = dec_unit[unit];

	if((dimm == 1) && (state == 1)) {
		dimm = 2;
	}

	if (iParityData == parity && dimm < 1) {
		createMessage(id, state, unit, all, learn);
	}
}

static void createZero(int s, int e) {
	int i;
	for(i=s;i<=e;i+=2) {
		ledrf_remote->raw[i] = PULSE_LEDRF_REMOTE_SHORT;
		ledrf_remote->raw[i+1] = PULSE_LEDRF_REMOTE_LONG;
	}
}

static void createOne(int s, int e) {
	int i;
	for(i=s;i<=e;i+=2) {
		ledrf_remote->raw[i] = PULSE_LEDRF_REMOTE_LONG;
		ledrf_remote->raw[i+1] = PULSE_LEDRF_REMOTE_SHORT;
	}
}

static void createHeader(void) {
	ledrf_remote->raw[0] = PULSE_LEDRF_REMOTE_SHORT;
}

static void createFooter(void) {
	ledrf_remote->raw[ledrf_remote->rawlen-1] = PULSE_LEDRF_REMOTE_FOOTER;
}

static void clearCode(void) {
	createHeader();
	createZero(1, ledrf_remote->rawlen-3);
}

static void createId(int id) {
	int binary[16], length = 0, i = 0, x = 23;

	length = decToBin(id, binary);
	for(i=length;i>=0;i--) {
		if(binary[i] == 1) {
			createOne(x, x+1);
		}
		x = x-2;
	}
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
		createOne(31, 32); //on
	}
}

static void createParity(void) {
	int i, p = 1;		// init even parity, without system ID
	for(i=25;i<=37;i+=2) {
		if(ledrf_remote->raw[i] == PULSE_LEDRF_REMOTE_LONG) {
			p = -p;
		}
	}
	if(p == -1) {
		createOne(39, 40);
	}
}

static int createCode(JsonNode *code) {
	double itmp = -1;
	int unit = -1, id = -1, learn = -1, state = -1, all = 0;

	if(json_find_number(code, "id", &itmp) == 0)
		id = (int)round(itmp);
	if(json_find_number(code, "unit", &itmp) == 0)
		unit = (int)round(itmp);
	if(json_find_number(code, "all", &itmp) == 0)
		all = (int)round(itmp);
	if(json_find_number(code, "learn", &itmp) == 0)
		learn = (int)round(itmp);
	if(json_find_number(code, "off", &itmp) == 0)
		state=0;
	else if(json_find_number(code, "on", &itmp) == 0)
		state=1;

	if(id==-1 || (unit==-1 && all==0) || state==-1) {
		logprintf(LOG_ERR, "ledrf_remote: insufficient number of arguments");
		return EXIT_FAILURE;
	} else if(id > 4095 || id < 0) {
		logprintf(LOG_ERR, "ledrf_remote: invalid programm code id range");
		return EXIT_FAILURE;
	} else if((unit > 3 || unit < 0) && all == 0) {
		logprintf(LOG_ERR, "ledrf_remote: invalid button code unit range");
		return EXIT_FAILURE;
	} else {
		if(unit == -1 && all == 1) {
			unit = 4;
		}
		ledrf_remote->rawlen = RAW_LENGTH;
		createMessage(id, state, unit, all, learn);
		clearCode();
		createId(id);
		createUnit(unit);
		createState(state);
		createParity();
		createFooter();
	}
	return EXIT_SUCCESS;
}

static void printHelp(void) {
	printf("\t -i --id=id\t\t\tcontrol one or multiple devices with this id\n");
	printf("\t -u --unit=unit\t\t\tcontrol the device unit with this code\n");
	printf("\t -t --on\t\t\tsend an on signal to device\n");
	printf("\t -f --off\t\t\tsend an off signal to device\n");
	printf("\t -l --learn\t\t\temulate learning mode of remote control\n");
	printf("\t -a --id=all\t\t\tcommand to all devices with this id\n");
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void ledrfremoteInit(void) {

	protocol_register(&ledrf_remote);
	protocol_set_id(ledrf_remote, "ledrf_remote");
	protocol_device_add(ledrf_remote, "ledrf_remote", "11Key RF LED Controller");
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
	options_add(&ledrf_remote->options, 'i', "id", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^([1-9]|[1-9][0-9]|[1-9][0-9][0-9]|[1-3][0-9][0-9][0-9]|40[0-8][0-9]|409[0-5])$");
	options_add(&ledrf_remote->options, 'a', "all", OPTION_NO_VALUE, DEVICES_OPTIONAL, JSON_NUMBER, NULL, NULL);
	options_add(&ledrf_remote->options, 'l', "learn", OPTION_HAS_VALUE, DEVICES_OPTIONAL, JSON_NUMBER, NULL, NULL);

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
	module->version = "0.1";
	module->reqversion = "6.0";
	module->reqcommit = "84";
}

void init(void) {
	ledrfremoteInit();
}
#endif
