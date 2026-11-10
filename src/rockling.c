#pragma bank 255
#include "rockling.h"
#include "actor_internal.h"
#include "desert.h"
#include "descent.h"
#include "world.h"
#include "level.h"
#include "run.h"
#include "player.h"
#include "camera.h"
#include "health.h"
#include "gfx.h"
#include "oam.h"

uint8_t rockling_on;
// Only Dustwake uses these tiles: its six creatures replace the slime art.
#define BODY 16
#define ROCK 28
#define MARK 30
#define DUST 32
#define REST 0
#define WINDUP 1
#define FLIGHT 2
#define IMPACT 3
extern uint8_t oam_next, draw_rotation;

void rockling_load(void) BANKED {
    rockling_on = play_mode == MODE_DESCENT && world_id == WORLD_DESERT && level_map == DESERT_WAKE;
    if (!rockling_on) return;
    // Small broad-shouldered relatives of Cragback: lowered arms, arms overhead,
    // and crouched recovery. Pixel art is generated only with the display off.
    uint8_t data[32];
    for (uint8_t pose=0; pose<3; pose++) for (uint8_t part=0; part<2; part++) {
        for (uint8_t y=0; y<16; y++) {
            uint8_t lo=0, hi=0;
            for (uint8_t col=0; col<8; col++) {
                uint8_t x=part*8+col, c=0, yy=pose==2 ? (y>2?y-2:0) : y;
                if (yy>=3 && yy<=12 && x>=3 && x<=12) c=(x==3||x==12||yy==3||yy==12)?3:2;
                if (yy>=6 && yy<=11 && (x==1||x==2||x==13||x==14)) c=3;
                if (yy==5 && (x==5||x==10)) c=1;
                if (yy>=8 && yy<=10 && x>=6 && x<=9) c=1;
                if (yy>=13 && yy<=14 && ((x>=3&&x<=6)||(x>=9&&x<=12))) c=3;
                if (pose==1 && y<7 && (x==1||x==2||x==13||x==14)) c=2;
                lo=(lo<<1)|(c&1); hi=(hi<<1)|(c>>1);
            }
            data[y*2]=lo; data[y*2+1]=hi;
        }
        set_sprite_data(BODY+pose*4+part*2,2,data);
    }
    for (uint8_t k=0;k<3;k++) {
        for (uint8_t y=0;y<16;y++) {
            uint8_t lo=0,hi=0;
            for (uint8_t x=0;x<8;x++) {
                uint8_t c=0;
                uint8_t left=y==3||y==10?3:y==4||y==9?2:1;
                uint8_t right=y==3||y==10?4:y==9?5:6;
                if (!k && y>=3&&y<=10&&x>=left&&x<=right) c=(y==3||y==10||x==left||x==right)?3:2;
                if (!k && y==5&&x>=2&&x<=4) c=1;
                if (k==1 && y>=6&&y<=10 && ((y==6||y==10)?(x>=2&&x<=5):(x==1||x==6))) c=3;
                if (k==2 && y>=3&&y<=12 && ((x+y)&3)==0) c=1;
                lo=(lo<<1)|(c&1);hi=(hi<<1)|(c>>1);
            }
            data[y*2]=lo;data[y*2+1]=hi;
        }
        set_sprite_data(ROCK+k*2,2,data);
    }
}

void rockling_update(void) BANKED {
    for (uint8_t i=0;i<ACTOR_MAX;i++) {
        if (!actor_type[i]) continue;
        if (actor_scripted[i]==ACTOR_WARPING) {
            if (warp_left[i]<6 && (warp_left[i]&2)) {
                actor_t0[i]=BODY;actor_t1[i]=BODY+2;
                actor_p0[i]=actor_p1[i]=SPR_PAL_SHOOTER;
            }
            continue;
        }
        if (actor_scripted[i]!=13) {
            actor_scripted[i]=13; actor_state[i]=REST;
            actor_timer[i]=40+i*13; actor_hp[i]=5;
        }
        actor_flash[i]=0;
        if (actor_timer[i]) --actor_timer[i];
        if (!actor_timer[i]) {
            if (actor_state[i]==REST) {
                int16_t dx=(int16_t)player_x-actor_x[i],dy=(int16_t)player_y-actor_y[i];
                // No blind bombardment from another screen or into the safe pocket.
                if (dx>-112&&dx<112&&dy>-96&&dy<96&&player_y<280) {
                    actor_vx[i]=player_x+8;actor_vy[i]=player_y+12;
                    actor_state[i]=WINDUP;actor_timer[i]=36;
                }
            } else if (actor_state[i]==WINDUP) {
                actor_state[i]=FLIGHT;actor_timer[i]=32;bullets_fired++;
            } else if (actor_state[i]==FLIGHT) {
                actor_state[i]=IMPACT;actor_timer[i]=8;
                int16_t dx=(int16_t)player_x+8-actor_vx[i],dy=(int16_t)player_y+12-actor_vy[i];
                if (dx>-12&&dx<12&&dy>-12&&dy<12&&player_hittable()) {
                    hurt_cause=16;player_hurt(12,actor_vx[i],actor_vy[i]);
                }
            } else {actor_state[i]=REST;actor_timer[i]=88+i*9;}
        }
        uint8_t pose=actor_state[i]==WINDUP?1:actor_state[i]==REST?0:2;
        actor_t0[i]=BODY+pose*4;actor_t1[i]=BODY+pose*4+2;
        actor_p0[i]=actor_p1[i]=SPR_PAL_SHOOTER;
    }
}

static void sprite(int16_t x,int16_t y,uint8_t tile) {
    x=x-(int16_t)cam_x+8;y=y-(int16_t)cam_y+16;
    if (oam_next>=40||x<=0||x>=168||y<=0||y>=160) return;
    shadow_OAM[oam_next].x=x;shadow_OAM[oam_next].y=y;
    shadow_OAM[oam_next].tile=tile;shadow_OAM[oam_next++].prop=SPR_PAL_UI;
}

void rockling_emit(void) BANKED {
    for (uint8_t k=0;k<ACTOR_MAX;k++) {
        uint8_t i=(k+draw_rotation)&7;
        if (!actor_type[i]||actor_scripted[i]!=13||actor_state[i]==REST) continue;
        int16_t tx=actor_vx[i],ty=actor_vy[i];
        if (actor_state[i]==IMPACT) {sprite(tx-4,ty-8,DUST);continue;}
        sprite(tx-4,ty-8,MARK); // locked ground warning persists throughout flight
        int16_t x=actor_x[i]+4,y=actor_y[i]-8;
        if (actor_state[i]==FLIGHT) {
            uint8_t t=32-actor_timer[i];
            x+=((tx-4-x)*t)>>5;
            y+=((ty-8-y)*t)>>5;
            y-=t<16?t*2:(32-t)*2;
        }
        sprite(x,y,ROCK);
    }
}
