#ifndef SCRAPBACK_H
#define SCRAPBACK_H
#include <gb/gb.h>
void scrapback_start(void) BANKED;
void scrapback_update(void) BANKED;
void scrapback_draw(void) BANKED;
#endif
