#pragma bank 255
#include "warden.h"
#include "boss.h"
#include "player.h"
#include "health.h"
#include "gfx.h"
#include "camera.h"

// A locked floor claim: outline -> electrical stripes -> clear floor.
// Eight reserved sprites keep the warning visible even with a full shot pool.
uint8_t warden_zone_x, warden_zone_y, warden_zone_state, warden_zone_timer;
void warden_start(void) BANKED {
    uint8_t data[32];
    for (uint8_t t=0;t<8;t++) {
        for (uint8_t y=0;y<16;y++) {
            uint8_t bits=0;
            for (uint8_t x=0;x<8;x++) {
                uint8_t edge=y==0 || (t>=4 && ((x+y)&7)==0) ||
                    ((t&3)==0 && x==0) || ((t&3)==3 && x==7);
                bits=(bits<<1) | edge;
            }
            data[y*2]=bits; data[y*2+1]=bits;
        }
        set_sprite_data(112+t*2,2,data);
    }
    warden_zone_state=0; warden_zone_timer=30;
}
void warden_update(void) BANKED {
    if (warden_zone_timer) warden_zone_timer--;
    if (!warden_zone_timer) {
        if (!warden_zone_state) {
            // Center the claim on the current position, then never track it.
            warden_zone_x=player_x<24?16:player_x>120?112:player_x-8;
            warden_zone_y=player_y<24?16:player_y>88?80:player_y-8;
            warden_zone_state=1; warden_zone_timer=boss_phase?36:48;
        } else if (warden_zone_state==1) {
            warden_zone_state=2; warden_zone_timer=boss_phase?72:60;
            boss_attacks++;
        } else { warden_zone_state=0; warden_zone_timer=30; }
    }
    if (warden_zone_state==2 && player_hittable() &&
        player_x+PLAYER_HB_X < (uint16_t)warden_zone_x+32 &&
        player_x+PLAYER_HB_X+PLAYER_HB_W > warden_zone_x &&
        player_y+PLAYER_HB_Y < (uint16_t)warden_zone_y+32 &&
        player_y+PLAYER_HB_Y+PLAYER_HB_H > warden_zone_y) {
        hurt_cause=10;
        player_hurt(24,warden_zone_x+16,warden_zone_y+16);
    }
}
void warden_draw(void) BANKED {
    for (uint8_t i=15;i<23;i++) shadow_OAM[i].y=0;
    if (boss_state==BOSS_WON || !warden_zone_state) return;
    for (uint8_t t=0;t<8;t++) {
        uint8_t i=15+t;
        shadow_OAM[i].x=warden_zone_x+(t&3)*8+8-cam_x;
        shadow_OAM[i].y=warden_zone_y+(t>>2)*16+16-cam_y;
        shadow_OAM[i].tile=(warden_zone_state==2?120:112)+(t&3)*2;
        shadow_OAM[i].prop=(warden_zone_state==2?SPR_PAL_BULLET:SPR_PAL_UI) | (t>=4?S_FLIPY:0);
    }
}
