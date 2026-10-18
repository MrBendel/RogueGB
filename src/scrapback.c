#pragma bank 255
#include "scrapback.h"
#include <gb/cgb.h>
#include "boss.h"
#include "actor_internal.h"
#include "player.h"
#include "health.h"
#include "gfx.h"
#include "camera.h"
#include "oam.h"
#include "state.h"

// BOSS_RETREAT is used here for stalking; the remaining states are shared.
uint8_t scrap_facing, scrap_target_x, scrap_target_y;
uint16_t scrap_crashes;
static const uint8_t body_slots[6]={11,12,13,14,15,16};
static const uint8_t body_x[6]={0,8,16,0,8,16};
static const uint8_t body_y[6]={0,0,0,16,16,16};
// Right-facing armored quadruped. Rotation at load time gives four readable
// headings; movement itself uses a continuous fixed-point vector.
static const char beast[24][25]={
 "000000000000000000000000","000000000000000000000000",
 "000000003333333300000000","000000033222222330000000",
 "000000332332332233000000","000003322332332223300000",
 "003033222222222223330000","003332233333322223233000",
 "003222331111332223223300","003222311111132233222330",
 "003222311111132233213233","003222331111332233222233",
 "000322233333322233222233","000322222222222233213233",
 "000332233223322223222330","000033233223322223223300",
 "000003333333333333233000","000003223000032233330000",
 "000003223000032230000000","000033223000032233000000",
 "000033333000033333000000","000000000000000000000000",
 "000000000000000000000000","000000000000000000000000"};

static void load_art(void) {
    uint8_t data[32];
    for(uint8_t facing=0;facing<4;facing++)for(uint8_t part=0;part<6;part++) {
        for(uint8_t row=0;row<16;row++) {
            uint8_t lo=0,hi=0,y=(part/3)*16+row;
            for(uint8_t col=0;col<8;col++) {
                uint8_t x=(part%3)*8+col,c=0;
                if(y<24) {
                    uint8_t xx=x,yy=y;
                    if(facing==1){xx=y;yy=23-x;}
                    else if(facing==2){xx=23-x;yy=23-y;}
                    else if(facing==3){xx=23-y;yy=x;}
                    c=beast[yy][xx]-'0';
                }
                lo=(lo<<1)|(c&1);hi=(hi<<1)|(c>>1);
            }
            data[row*2]=lo;data[row*2+1]=hi;
        }
        set_sprite_data(96+facing*12+part*2,2,data);
    }
}
static void track(void) {
    int16_t dx=(int16_t)player_x-actor_x[boss_slot],dy=(int16_t)player_y-actor_y[boss_slot];
    uint16_t ax=dx<0?-dx:dx,ay=dy<0?-dy:dy;
    scrap_facing=ax>ay?(dx<0?2:0):(dy<0?3:1);
    scrap_target_x=player_x;scrap_target_y=player_y;
}
static uint8_t free_at(uint16_t x,uint16_t y) {
    // The full 24x24 footprint fits inside the perimeter, including the art
    // extending four pixels beyond the shared 16x16 damage/contact anchor.
    return x>=20 && x<=124 && y>=20 && y<=92 && enemy_box_free(x,y);
}
static void lock_charge(void) {
    int16_t dx=(int16_t)scrap_target_x-actor_x[boss_slot],dy=(int16_t)scrap_target_y-actor_y[boss_slot];
    uint16_t ax=dx<0?-dx:dx,ay=dy<0?-dy:dy;
    uint16_t dist=(ax>ay?ax+(ay>>1):ay+(ax>>1))*8;
    int16_t vx=dx*8,vy=dy*8;
    if(!dist){vy=256;dist=256;}
    // Normalize once at launch without division. Each of three substeps is
    // ~one pixel, preserving non-cardinal slopes and preventing wall tunneling.
    while(dist>320){dist>>=1;vx>>=1;vy>>=1;}
    while(dist<160){dist<<=1;vx*=2;vy*=2;}
    if(dist<224){vx+=vx>>1;vy+=vy>>1;}
    actor_vx[boss_slot]=vx;actor_vy[boss_slot]=vy;
    actor_xs[boss_slot]=actor_ys[boss_slot]=0;
    boss_state=BOSS_CHARGE;boss_timer=90;boss_attacks++;
}
static void crash(void) {
    boss_state=BOSS_RECOVER;boss_timer=100;boss_cycles++;scrap_crashes++;
    actor_guard[boss_slot]=0;camera_shake(SHAKE_SMALL);
}
void scrapback_start(void) BANKED {
    load_art();boss_kind=BOSS_SCRAPBACK;boss_slot=actor_spawn(ACTOR_PEASHOOTER,72,28);
    boss_hp=boss_hp_max=24;actor_hp[boss_slot]=24;actor_scripted[boss_slot]=11;
    actor_guard[boss_slot]=1;boss_state=BOSS_RETREAT;boss_timer=36;
    boss_phase=boss_reward=boss_relays=0;boss_attacks=boss_cycles=scrap_crashes=0;
    // The ordinary actor emitter reserves two entries; draw replaces those
    // plus four prop slots with the six parts of the larger body.
    actor_t0[boss_slot]=96;actor_t1[boss_slot]=98;
    actor_p0[boss_slot]=actor_p1[boss_slot]=SPR_PAL_SHOOTER;
    for(uint8_t p=SPR_PAL_SHOOTER;p<=SPR_PAL_BULLET;p++) {
        pal_base_obj[p*4+2]=RGB(19,12,5);pal_base_obj[p*4+3]=RGB(4,3,3);
    }
    pal_base_obj[SPR_PAL_SHOOTER*4+1]=RGB(8,7,5);
    pal_base_obj[SPR_PAL_BULLET*4+1]=RGB(8,31,25);
    track();health_give_shield(SHIELD_START_CAP,SHIELD_START_DELAY,SHIELD_START_RATE);
    bkg_text(1,0,"SCRAPBACK");
}
void scrapback_update(void) BANKED {
    if(boss_timer)boss_timer--;
    if(boss_state==BOSS_RETREAT) {
        track();
        if(!((uint8_t)frame_count&1)) {
            uint16_t x=actor_x[boss_slot],y=actor_y[boss_slot];
            int16_t dx=(int16_t)player_x-x,dy=(int16_t)player_y-y;
            if(dx>36 || dx< -36 || dy>36 || dy< -36) {
                x+=dx>0?1:dx<0?-1:0;y+=dy>0?1:dy<0?-1:0;
                if(free_at(x,y)){actor_x[boss_slot]=x;actor_y[boss_slot]=y;}
            }
        }
        if(!boss_timer){boss_state=BOSS_WARN;boss_timer=36;}
    } else if(boss_state==BOSS_WARN) {
        track();if(!boss_timer)lock_charge();
    } else if(boss_state==BOSS_CHARGE) {
        for(uint8_t n=0;n<3;n++) {
            int16_t fx=(int16_t)actor_xs[boss_slot]+actor_vx[boss_slot];
            int16_t fy=(int16_t)actor_ys[boss_slot]+actor_vy[boss_slot];
            uint16_t x=actor_x[boss_slot]+(fx>>8),y=actor_y[boss_slot]+(fy>>8);
            if(!free_at(x,y)){crash();break;}
            actor_x[boss_slot]=x;actor_y[boss_slot]=y;
            actor_xs[boss_slot]=(uint8_t)fx;actor_ys[boss_slot]=(uint8_t)fy;
        }
        // Covers the full visible body instead of just the smaller shared
        // actor hitbox. Apply before generic contact so a charge hits for 20.
        int16_t dx=(int16_t)player_x-actor_x[boss_slot],dy=(int16_t)player_y-actor_y[boss_slot];
        if(player_hittable() && dx>-19 && dx<19 && dy>-19 && dy<19) {
            hurt_cause=14;player_hurt(20,actor_x[boss_slot]+8,actor_y[boss_slot]+8);
        }
        if(!boss_timer && boss_state==BOSS_CHARGE)crash();
    } else if(boss_state==BOSS_RECOVER && !boss_timer) {
        actor_guard[boss_slot]=1;boss_state=BOSS_RETREAT;boss_timer=28;
    }
}
void scrapback_draw(void) BANKED {
    for(uint8_t i=OAM_PROP;i<OAM_FIRST_ACTOR;i++)shadow_OAM[i].y=0;
    if(boss_state==BOSS_WON || !actor_type[boss_slot])return;
    uint8_t pal=boss_state==BOSS_RECOVER?SPR_PAL_BULLET:SPR_PAL_SHOOTER;
    if(actor_hurt[boss_slot] || (boss_state==BOSS_WARN && (boss_timer&4)))pal=SPR_PAL_SHOT;
    uint8_t shake=boss_state==BOSS_WARN?((uint8_t)frame_count>>1)&1:0;
    uint8_t bob=boss_state==BOSS_RETREAT || boss_state==BOSS_CHARGE?((uint8_t)frame_count>>2)&1:0;
    for(uint8_t part=0;part<6;part++) {
        uint8_t i=body_slots[part];
        shadow_OAM[i].x=actor_x[boss_slot]+4+body_x[part]-cam_x+shake;
        shadow_OAM[i].y=actor_y[boss_slot]+12+body_y[part]-cam_y-bob;
        shadow_OAM[i].tile=96+scrap_facing*12+part*2;shadow_OAM[i].prop=pal;
    }
}
