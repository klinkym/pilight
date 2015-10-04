/*
 * File:   bhc9938d.c
 * Author: Michael Hegenbarth (carschrotter) <mnh@mn-hegenbarth.de>
 *
 * Created on 23. Januar 2015, 13:19
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
 
#include "../../core/pilight.h"
#include "../../core/common.h"
#include "../../core/dso.h"
#include "../../core/log.h"
#include "../protocol.h"
#include "../../core/binary.h"
#include "../../core/gc.h" 
#include "bhc9938d.h"

#ifndef MODULE
 
__attribute__((weak))
#endif
    
const char * indent = "\t\t\t\t\t";
 
typedef enum { OFF, ON, UNKNOWN=-1 } switchState;
 
/*
 * code all codes for on and off state from piligt debug
 */
const char * codes[2][3] = {
    {
    "380 1140 380 1140 380 1140 380 1140 380 1140 380 1140 380 1140 1140 380 1140 380 1140 380 1140 380 380 1140 380 1140 380 1140 1140 380 380 1140 380 1140 380 1140 380 1520 380 1140 380 1140 380 1140 380 1140 1140 380 380 12920",
    "380 1140 380 1140 380 1140 380 1140 380 1140 380 1140 380 1140 1140 380 1140 380 1140 380 1140 380 380 1140 380 1140 380 1140 1140 380 380 1140 380 1140 380 1140 380 1520 380 1140 380 1140 380 1140 380 1140 1140 380 380 12920",
    "380 1140 380 1140 380 1140 380 1140 380 1140 380 1140 380 1140 1140 380 1140 380 1140 380 1140 380 380 1140 380 1140 380 1140 1140 380 380 1140 380 1140 380 1140 380 1520 380 1140 380 1140 380 1140 380 1140 1140 380 380 12920"
    }, //off codes (index 0)
    {
    "380 1140 380 1140 380 1140 380 1140 380 1140 380 1140 380 1140 1140 380 1140 380 1140 380 1140 380 380 1140 380 1140 380 1140 1140 380 380 1140 380 1140 380 1140 380 1520 380 1140 380 1140 380 1140 380 1140 1140 380 380 12920",
    "380 1140 380 1140 380 1140 380 1140 380 1140 380 1140 380 1140 1140 380 1140 380 1140 380 1140 380 380 1140 380 1140 380 1140 1140 380 380 1140 380 1140 380 1140 380 1520 380 1140 380 1140 380 1140 380 1140 1140 380 380 12920",
    "380 1140 380 1140 380 1140 380 1140 380 1140 380 1140 380 1140 1140 380 1140 380 1140 380 1140 380 380 1140 380 1140 380 1140 1140 380 380 1140 380 1140 380 1140 380 1520 380 1140 380 1140 380 1140 380 1140 1140 380 380 12920"
    } //On codes (index 1)
};
bool strIsInt(const char * str, int * result){
    char * rest = NULL;
    int tmp = (int) strtol(str, &rest, 10);
    result = &tmp;
    return (bool) !(rest != NULL);
}
 
 
static void bhc9938dPrintHelp(void) {
    printf("\t -u --unit=unitnumber\t\tcontrol a device with this unitcode\t (1|2|3)\n");
    printf("\t -t --on\t\t\tsend an on signal\n");
    printf("\t -f --off\t\t\tsend an off signal\n");
/*
    printf("\t -d --device=\"raw\"\t\traw code to tur on device \n");
    printf("\t -f -rawOff=\"raw\"\t\traw code to tur off device \n");
    printf("%sraw code is devided by spaces\n", indent);
    printf("%s(just like the output of debug)\n", indent);
*/
}
 
static int bhc9938dCreateCode(JsonNode * code) {
    int unit = -1;
    switchState sState = UNKNOWN;
    double itmp = 0;
    
    int codeIndex = 0;
    
    char * rcode = NULL;
    char * pch = NULL;
    int i = 0;
 
    if(json_find_number(code, "unit", &itmp) == 0){
        unit = (int)round(itmp);
    }
    
    if(json_find_number(code, "off", &itmp) == 0){
        sState=OFF;
    } else if(json_find_number(code, "on", &itmp) == 0){
        sState=ON;
    }
    if(unit == -1 || sState == -1) {
        logprintf(LOG_ERR, "bhc9938d: insufficient number of arguments");
        return EXIT_FAILURE;
    } else if(unit > 4 || unit < 0) {
        logprintf(LOG_ERR, "bhc9938d: invalid unit range");
        return EXIT_FAILURE;
    } else {
    codeIndex = unit -1;
    rcode = strdup(codes[sState][codeIndex]);
    
    printf("\nindex:\t%d\nsState:\t%d\ncode:%s\n", codeIndex, sState, rcode);
    
    pch = strtok(rcode, " ");
 
    while (pch != NULL) {
        
        bhc9938d -> raw[i] = atoi(pch);
        pch = strtok(NULL, " ");
 
        i++;
    }
 
    sfree((void *) &rcode);
 
    bhc9938d -> rawlen = i;
    }
    return EXIT_SUCCESS;
}
 
/*
// BHC9938D bhc9938d
*/
void bhc9938dInit(void) {
    protocol_register(&bhc9938d);
    protocol_set_id(bhc9938d, "bhc9938d");
    protocol_device_add(bhc9938d, "bhc9938d", "BHC9938D protocol");
    
    options_add(&bhc9938d->options, 'u', "unit", OPTION_HAS_VALUE, DEVICES_ID, JSON_NUMBER, NULL, "^(3[012]?|[012][0-9]|[0-9]{1})$");
    options_add(&bhc9938d->options, 't', "on", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
    options_add(&bhc9938d->options, 'f', "off", OPTION_NO_VALUE, DEVICES_STATE, JSON_STRING, NULL, NULL);
    
    bhc9938d -> devtype = RAW;
    bhc9938d -> printHelp = &bhc9938dPrintHelp;
    bhc9938d -> createCode = &bhc9938dCreateCode;
}
 
#ifdef MODULE
 
void compatibility(struct module_t *module) {
    module -> name = "bhc9938d";
    module -> version = "0.1";
    module -> reqversion = "5.0";
    module -> reqcommit = NULL;
}
 
void init(void) {
    bhc9938dInit();
}
#endif
