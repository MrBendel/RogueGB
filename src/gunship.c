#pragma bank 255
#include "gunship.h"
#include "boss.h"
#include "actor_internal.h"
#include "player.h"
#include "health.h"
#include "aim.h"
#include "gfx.h"
#include "camera.h"

#define SHIP_TILES 96
uint8_t gunship_x, gunship_y, gunship_bomb_x, gunship_bomb_y;
uint8_t gunship_bomb_timer, gunship_bomb_live, gunship_laser_x;
uint8_t gunship_core_opening;
int8_t gunship_dir;
static uint8_t draw_oam;

// 32x32 top-down silhouette: swept wings, engines, cockpit and a belly core.
static uint8_t pixel(uint8_t x, uint8_t y) {
    if (x >= 10 && x <= 21 && y >= 2 && y <= 29) {
        if (x == 10 || x == 21 || y == 2 || y == 29) return 3;
        if (x >= 13 && x <= 18 && y >= 5 && y <= 10) return 1;
        if (x >= 12 && x <= 19 && y >= 19 && y <= 26) return (x == 12 || x == 19 || y == 19 || y == 26) ? 3 : 1;
        return 2;
    }
    if (y >= 12 && y <= 24 && x >= 1 && x <= 30) {
        uint8_t edge = x < 16 ? x : 31-x;
        if (y < 17 - (edge >> 1)) return 0;
        return y == 24 || x == 1 || x == 30 || y == 17-(edge >> 1) ? 3 : 2;
    }
    if (y >= 25 && y <= 30 && ((x >= 4 && x <= 7) || (x >= 24 && x <= 27))) return y & 1 ? 1 : 3;
    return 0;
}

static void load_art(void) {
    uint8_t data[32];
    for (uint8_t s = 0; s < 15; s++) {
        for (uint8_t y = 0; y < 16; y++) {
            uint8_t lo=0, hi=0;
            for (uint8_t x = 0; x < 8; x++) {
                uint8_t c;
                if (s < 8) c = pixel((s & 3)*8+x, (s >> 2)*16+y);
                else if (s < 10 || s >= 13) {
                    uint8_t xx=(s>=13?s-13:s-8)*8+8+x, yy=y+16;
                    c=pixel(xx,yy);
                    if (xx>=12 && xx<=19 && yy>=19 && yy<=26) {
                        // Closed slats, then panels retract to reveal a narrow
                        // glowing gap before the fully exposed core appears.
                        c=s>=13 && xx>=14 && xx<=17 ? 1 : yy&1 ? 3 : 2;
                    }
                } else if (s == 10) c=(x>=2 && x<=5) ? 1 : (x==1 || x==6) ? 3 : 0;
                else if (s == 11) c=(y==0 || y==15 || x==0 || x==7 || y==7) ? 3 : 0;
                else c=((x+y)&2) ? 1 : 3;
                lo=(lo<<1)|(c&1); hi=(hi<<1)|(c>>1);
            }
            data[y*2]=lo; data[y*2+1]=hi;
        }
        set_sprite_data(SHIP_TILES+s*2,2,data);
    }
}

static void guard(uint8_t on) {
    actor_guard[boss_slot]=on;
    gunship_core_opening=on?0:12;
    if(on) { aim_score[boss_slot]=AIM_NONE; if(aim_target==boss_slot) aim_target=AIM_NONE; }
}
static void core(void) {
    // One world-space origin for the body, warning and damaging beam.
    gunship_laser_x=gunship_x+16;
    actor_x[boss_slot]=gunship_x+8; actor_y[boss_slot]=gunship_y+16;
    actor_t0[boss_slot]=actor_guard[boss_slot] ? 112 : gunship_core_opening ? 122 : 106;
    actor_t1[boss_slot]=actor_guard[boss_slot] ? 114 : gunship_core_opening ? 124 : 108;
    actor_p0[boss_slot]=actor_p1[boss_slot]=actor_guard[boss_slot] ? SPR_PAL_WEAPON : SPR_PAL_SHOT;
}
void gunship_start(void) BANKED {
    boss_kind=BOSS_GUNSHIP; boss_phase=boss_reward=boss_relays=0;
    boss_attacks=boss_cycles=0;
    boss_hp=boss_hp_max=40;
    gunship_x=64; gunship_y=16; gunship_dir=1;
    gunship_bomb_timer=gunship_bomb_live=0; gunship_laser_x=32;
    boss_slot=actor_spawn(ACTOR_PEASHOOTER,72,32);
    actor_scripted[boss_slot]=1; actor_hp[boss_slot]=boss_hp;
    boss_state=BOSS_CRUISE; boss_timer=180;
    guard(1); load_art(); core();
    health_give_shield(SHIELD_START_CAP,SHIELD_START_DELAY,SHIELD_START_RATE);
    bkg_text(0,0,"                    "); bkg_text(0,1,"                    ");
    bkg_text(1,0,"COLLECTOR");
}

static void hurt(uint8_t damage, uint8_t x, uint8_t y) {
    if (!player_hittable()) return;
    hurt_cause=10;
    player_hurt(damage,x,y);
}
static uint8_t overlaps(uint16_t p, uint8_t size, uint8_t start, uint8_t width) {
    return p < (uint16_t)start+width && p+size > start;
}
void gunship_update(void) BANKED {
    if (boss_hp<=20) boss_phase=1;
    if (gunship_bomb_timer) {
        gunship_bomb_timer--;
        if (!gunship_bomb_timer) {
            if (!gunship_bomb_live) { gunship_bomb_live=1; gunship_bomb_timer=12; camera_shake(SHAKE_SMALL); }
            else gunship_bomb_live=0;
        }
        if (gunship_bomb_live && overlaps(player_x+PLAYER_HB_X,PLAYER_HB_W,gunship_bomb_x,16) &&
            overlaps(player_y+PLAYER_HB_Y,PLAYER_HB_H,gunship_bomb_y,16)) hurt(12,gunship_bomb_x+8,gunship_bomb_y+8);
    }
    // Give the full warning duration only after reaching the firing position.
    if (boss_timer && (boss_state!=BOSS_LASER_WARN || gunship_x==16)) boss_timer--;
    if (gunship_core_opening) gunship_core_opening--;
    actor_flash[boss_slot]=0;
    if (boss_state==BOSS_CRUISE) {
        if (gunship_x>=112) gunship_dir=-1;
        if (gunship_x<=16) gunship_dir=1;
        gunship_x+=gunship_dir;
        if (boss_timer==150 || boss_timer==100 || boss_timer==50) {
            gunship_bomb_x=player_x<16?16:player_x>128?128:player_x;
            gunship_bomb_y=player_y<56?56:player_y>96?96:player_y;
            gunship_bomb_timer=36; gunship_bomb_live=0; boss_attacks++;
        }
        if (!boss_timer) {
            boss_state=BOSS_LASER_WARN; boss_timer=boss_phase?48:60;
        }
    } else if (boss_state==BOSS_LASER_WARN) {
        if (gunship_x>16) gunship_x=gunship_x>18?gunship_x-2:16;
        if (!boss_timer) { boss_state=BOSS_LASER; boss_timer=96; guard(0); boss_attacks++; }
    } else if (boss_state==BOSS_LASER) {
        gunship_x++;
        gunship_laser_x=gunship_x+16;
        if (overlaps(player_x+PLAYER_HB_X,PLAYER_HB_W,gunship_laser_x-3,6) &&
            overlaps(player_y+PLAYER_HB_Y,PLAYER_HB_H,48,64)) hurt(10,gunship_laser_x,player_y+8);
        if (!boss_timer) { boss_state=BOSS_RECOVER; boss_timer=120; boss_cycles++; }
    } else if (boss_state==BOSS_RECOVER && !boss_timer) {
        guard(1); boss_state=BOSS_CRUISE; boss_timer=180;
    }
    core();
}

static void sprite(uint8_t x,uint8_t y,uint8_t tile,uint8_t pal) {
    if(draw_oam>=25) return;
    uint8_t i=draw_oam++;
    shadow_OAM[i].x=x+8-cam_x; shadow_OAM[i].y=y+16-cam_y;
    shadow_OAM[i].tile=tile; shadow_OAM[i].prop=pal;
}
void gunship_draw(void) BANKED {
    for(uint8_t i=15;i<25;i++) shadow_OAM[i].y=0;
    draw_oam=15;
    if(boss_state==BOSS_WON) { gunship_bomb_timer=gunship_bomb_live=0; return; }
    // The ordinary actor draws the two belly-core columns; append six body
    // columns and hazards in the reserved entries, without reserving actor slots.
    uint8_t pal=actor_flash[boss_slot]?SPR_PAL_FLASH:SPR_PAL_SHOOTER;
    for(uint8_t s=0;s<8;s++) {
        if(s==5 || s==6) continue;
        sprite(gunship_x+(s&3)*8,gunship_y+(s>>2)*16,96+s*2,pal);
    }
    if(gunship_bomb_timer) {
        uint8_t tile=gunship_bomb_live?120:118;
        sprite(gunship_bomb_x,gunship_bomb_y,tile,SPR_PAL_BULLET);
        sprite(gunship_bomb_x+8,gunship_bomb_y,tile,SPR_PAL_BULLET);
    }
    if(boss_state==BOSS_LASER || (boss_state==BOSS_LASER_WARN && gunship_x==16 && (boss_timer&8))) {
        for(uint8_t y=48;y<112;y+=16)
            sprite(gunship_laser_x-4,y,116,boss_state==BOSS_LASER?SPR_PAL_BULLET:SPR_PAL_UI);
    }
}
