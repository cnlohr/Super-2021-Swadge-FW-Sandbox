#ifndef _SWADGEEMU_H
#define _SWADGEEMU_H

#include <stdlib.h>
#include "c_types.h"
#include "display/oled.h"

//Configuration
#define WS_HEIGHT 10
#define BTN_HEIGHT 30

#define INIT_PX_SCALE 4
#define HEADER_PIXELS ((3 * WS_HEIGHT) + 1)
#define FOOTER_PIXELS BTN_HEIGHT
#define NR_WS2812 6

extern int px_scale;
extern uint32_t * rawvidmem;
extern short screenx, screeny;
extern uint32_t footerpix[FOOTER_PIXELS*OLED_WIDTH];
extern uint32_t ws2812s[NR_WS2812];
extern double boottime;
extern uint8_t gpio_status;




//which_display = 0 -> mainscreen, = 1 -> footer
void emuCheckFooterMouse( int x, int y, int finger, int bDown );
void emuSendOLEDData( int which_display, uint8_t * currentFb );
void emuHeader();
void emuFooter();
void emuCheckResize();


#endif
 
