#pragma bank 255
#include "mercenary.h"
#include <gb/cgb.h>
#include "actor_internal.h"
#include "player.h"
#include "aim.h"
#include "gfx.h"
#include "camera.h"
#include "state.h"
#include "weapon_rifle.h"
#include "spr_rifle.h"
#include "mercenary_art.h"

// STRAFE -> AIM -> BURST -> RELOAD. A threatened strafe can branch into a
// signalled roll with a committed direction, followed by vulnerable recovery.
#define STRAFE 0
#define AIM 1
#define BURST 2
#define RELOAD 3
#define DODGE_WARN 4
#define ROLL 5
#define RECOVER 6
uint8_t merc_slot, merc_state, merc_timer, merc_aim, merc_locked_aim;
uint8_t merc_dodge_cooldown, merc_fire_timer, merc_rounds, merc_flash;
uint16_t merc_shots, merc_rolls;
int8_t merc_side, merc_roll_x, merc_roll_y;
int16_t merc_bullet_vx, merc_bullet_vy;
extern uint8_t rifle_vram;
static const uint8_t shadow[32]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,60,60,126,126,0,0};
static const uint8_t pellet[32]={0,0,0,0,0,0,0,0,0,0,0,0,24,24,60,0,60,0,24,24,0,0,0,0,0,0,0,0,0,0,0,0};

static uint8_t direction(int16_t dx,int16_t dy) {
    uint16_t ax=dx<0?-dx:dx, ay=dy<0?-dy:dy;
    uint8_t q=ay*5<ax?0:ay*3<ax*2?1:ay*2<ax*3?2:ay<ax*5?3:4;
    return (dx<0?(dy<0?8+q:8-q):(dy<0?16-q:q))&15;
}
static uint8_t move(int8_t dx,int8_t dy) {
    uint16_t x=actor_x[merc_slot]+dx,y=actor_y[merc_slot]+dy;
    if(!enemy_box_free(x,y))return 0;
    actor_x[merc_slot]=x;actor_y[merc_slot]=y;return 1;
}
static void pose(void) {
    uint8_t d=merc_state==ROLL?direction(merc_roll_x,merc_roll_y):merc_aim;
    uint8_t facing=d>=2&&d<=6?0:d>=10&&d<=14?1:2;
    uint8_t frame=merc_state==ROLL?(14-merc_timer)>>2:((uint8_t)frame_count>>3)&3;
    if(merc_state==AIM || merc_state==BURST || merc_state==RECOVER || merc_state==DODGE_WARN)frame=0;
    uint8_t tile=96+((merc_state==ROLL?12:0)+facing*4+(frame&3))*4;
    uint8_t flip=facing==2&&d>6&&d<10?S_FLIPX:0;
    actor_t0[merc_slot]=tile+(flip?2:0);actor_t1[merc_slot]=tile+(flip?0:2);
    actor_p0[merc_slot]=actor_p1[merc_slot]=SPR_PAL_SHOOTER|flip;
    actor_flash[merc_slot]=actor_hurt[merc_slot] ||
        ((merc_state==AIM || merc_state==DODGE_WARN) && (merc_timer&4));
}
void mercenary_start(void) BANKED {
    set_sprite_data(96,96,mercenary_tiles);
    set_sprite_data(192,2,shadow);
    for(uint8_t stage=0;stage<4;stage++) {
        uint8_t bar[32]={255,255,129,129,255,255};
        bar[2]|=(uint8_t)(0x7e<<(6-stage*2));
        set_sprite_data(194+stage*2,2,bar);
    }
    set_sprite_data(bullet_vram,2,pellet);
    pea_frames[0]=pea_frames[1]=pea_frames[4]=pea_frames[5]=0;
    // Preserve the supplied cyan armor and dark blue outline.
    pal_base_obj[SPR_PAL_SHOOTER*4+1]=RGB(31,31,28);
    pal_base_obj[SPR_PAL_SHOOTER*4+2]=RGB(3,27,31);
    pal_base_obj[SPR_PAL_SHOOTER*4+3]=RGB(2,3,9);
    merc_slot=actor_spawn(ACTOR_PEASHOOTER,72,32);
    enemy_bullet_damage=16;enemy_bullet_cause=12;
    actor_scripted[merc_slot]=9;actor_hp[merc_slot]=28;
    merc_state=STRAFE;merc_timer=42;merc_side=1;merc_aim=4;
    merc_dodge_cooldown=30;merc_fire_timer=merc_rounds=merc_flash=0;
    merc_shots=merc_rolls=0;pose();
}
static uint8_t threatened(void) {
    for(uint8_t n=0;n<SHOT_MAX;n++)if(shots[n].alive) {
        int16_t dx=(int16_t)actor_x[merc_slot]+4-shots[n].x;
        int16_t dy=(int16_t)actor_y[merc_slot]-shots[n].y;
        if(dx>-30 && dx<30 && dy>-30 && dy<30 &&
           ((dx>0 && shots[n].vx>0)||(dx<0 && shots[n].vx<0)||
            (dy>0 && shots[n].vy>0)||(dy<0 && shots[n].vy<0)))return 1;
    }
    return 0;
}
static void fire(void) {
    if(bullet_count==BULLET_MAX)return;
    uint8_t d=merc_locked_aim;
    if(merc_rounds==6 || merc_rounds==3) {
        // Exact muzzle-to-target aim twice per burst; the gun sprite uses the
        // nearest of 16 angles. Octagonal normalization fits 16-bit arithmetic.
        merc_locked_aim=d=direction((int16_t)(player_x-actor_x[merc_slot]),(int16_t)(player_y-actor_y[merc_slot]));
        int16_t dx=(int16_t)player_x+8-actor_x[merc_slot]-weapon_rifle_place[2][d];
        int16_t dy=(int16_t)player_y+10-actor_y[merc_slot]-weapon_rifle_place[3][d];
        uint16_t ax=dx<0?-dx:dx,ay=dy<0?-dy:dy;
        int16_t dist=ax>ay?ax+(ay>>1):ay+(ax>>1);
        if(!dist)dist=1;
        merc_bullet_vx=(dx*128/dist)*5;merc_bullet_vy=(dy*128/dist)*5;
    }
    merc_aim=d;
    for(uint8_t n=0;n<BULLET_MAX;n++)if(!bullets[n].alive) {
        Bullet *b=&bullets[n];b->alive=1;b->xs=b->ys=0;
        b->x=actor_x[merc_slot]+weapon_rifle_place[2][d]-4;
        b->y=actor_y[merc_slot]+weapon_rifle_place[3][d]-8;
        bullet_set_velocity(b,merc_bullet_vx,merc_bullet_vy);
        bullet_count++;bullets_fired++;merc_shots++;merc_flash=3;return;
    }
}
void mercenary_update(void) BANKED {
    if(!actor_type[merc_slot] || !actor_hp[merc_slot]) {
        for(uint8_t n=0;n<BULLET_MAX;n++)bullets[n].alive=0;
        bullet_count=0;return;
    }
    if(merc_timer)merc_timer--;
    if(merc_dodge_cooldown)merc_dodge_cooldown--;
    if(merc_flash)merc_flash--;
    int16_t dx=(int16_t)(player_x-actor_x[merc_slot]),dy=(int16_t)(player_y-actor_y[merc_slot]);
    int8_t sx=dx>0?1:dx<0?-1:0, sy=dy>0?1:dy<0?-1:0;
    uint16_t ax=dx<0?-dx:dx,ay=dy<0?-dy:dy;
    merc_aim=merc_state==BURST?merc_locked_aim:direction(dx,dy);
    if(merc_state==STRAFE || merc_state==RELOAD) {
        if(merc_state==STRAFE && !merc_dodge_cooldown && !((uint8_t)frame_count&3) && threatened()) {
            merc_roll_x=ax>ay?0:-sy*merc_side;merc_roll_y=ax>ay?sx*merc_side:0;
            merc_state=DODGE_WARN;merc_timer=8;merc_dodge_cooldown=140;
        } else {
            if(merc_state==STRAFE || !((uint8_t)frame_count&1)) {
                int8_t mx,my;
                if(ax<32 && ay<32){mx=-sx;my=-sy;}
                else if(ax>68 || ay>68){mx=sx;my=sy;}
                else {mx=ax>ay?0:-sy*merc_side;my=ax>ay?sx*merc_side:0;}
                if(!move(mx,my)){merc_side=-merc_side;move(-mx,-my);}
            }
            if(!merc_timer) {
                if(merc_state==RELOAD){merc_state=STRAFE;merc_timer=16;merc_side=-merc_side;}
                else {merc_state=AIM;merc_timer=18;}
            }
        }
    } else if(merc_state==AIM && !merc_timer) {
        merc_locked_aim=merc_aim;merc_state=BURST;merc_rounds=6;merc_fire_timer=0;
    } else if(merc_state==BURST) {
        if(merc_fire_timer)merc_fire_timer--;
        if(!merc_fire_timer) {
            fire();merc_fire_timer=7;
            if(!--merc_rounds){merc_state=RELOAD;merc_timer=30;}
        }
    } else if(merc_state==DODGE_WARN && !merc_timer) {
        merc_state=ROLL;merc_timer=14;merc_rolls++;
    } else if(merc_state==ROLL) {
        // Substeps prevent a fast roll tunnelling through a wall.
        for(uint8_t n=0;n<3;n++)if(!move(merc_roll_x,merc_roll_y)){merc_timer=0;break;}
        if(!merc_timer){merc_state=RECOVER;merc_timer=18;}
    } else if(merc_state==RECOVER && !merc_timer) {
        merc_side=-merc_side;merc_state=STRAFE;merc_timer=24;
    }
    pose();
}
static void sprite(uint8_t i,int16_t x,int16_t y,uint8_t tile,uint8_t prop) {
    x-=cam_x;y-=cam_y;
    if(x<=-8 || x>=160 || y<=-16 || y>=128)return;
    shadow_OAM[i].x=x+8;shadow_OAM[i].y=y+16;
    shadow_OAM[i].tile=tile;shadow_OAM[i].prop=prop;
}
void mercenary_draw(void) BANKED {
    for(uint8_t i=15;i<23;i++)shadow_OAM[i].y=0;
    if(!actor_type[merc_slot] || !actor_hp[merc_slot])return;
    int16_t x=actor_x[merc_slot],y=actor_y[merc_slot];
    sprite(15,x+4,y+2,192,SPR_PAL_UI);
    if(merc_state==RELOAD)sprite(19,x+4,y-4,194+(((30-merc_timer)>>3)*2),SPR_PAL_SHOT);
    if(merc_state==ROLL)return;
    uint8_t d=merc_aim,rifle_frame[4];
    // Copy only this direction, keeping persistent RAM below the room map.
    banked_memcpy(rifle_frame,spr_rifle_frames+d*4,4,BANK(spr_rifle));
    int16_t gx=x+weapon_rifle_place[0][d],gy=y+weapon_rifle_place[1][d];
    if(merc_flash){gx+=weapon_rifle_place[4][d];gy+=weapon_rifle_place[5][d];}
    sprite(16,gx,gy,rifle_vram+rifle_frame[0]*2,rifle_frame[1]|S_BANK|SPR_PAL_WEAPON);
    sprite(17,gx+8,gy,rifle_vram+rifle_frame[2]*2,rifle_frame[3]|S_BANK|SPR_PAL_WEAPON);
    if(merc_flash)sprite(18,x+weapon_rifle_place[2][d]-4,y+weapon_rifle_place[3][d]-8,
        shot_vram+shot_frames[0]*2,SPR_PAL_SHOT|shot_frames[1]);
}
