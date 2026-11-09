#pragma once
#include <stdint.h>

/* key bits (1 = pressed), same order as REG_KEYINPUT */
enum { KEY_A = 1, KEY_B = 2, KEY_SELECT = 4, KEY_START = 8, KEY_RIGHT = 16,
       KEY_LEFT = 32, KEY_UP = 64, KEY_DOWN = 128, KEY_R = 256, KEY_L = 512 };

void ui_init(void);
void ui_update(unsigned keys_held);      /* call once per frame */
int  ui_changed(void);                   /* 1 when ui_draw() needs to run again */
void ui_draw(uint16_t *map);             /* fills a 32x32 tilemap (30x20 visible) */
