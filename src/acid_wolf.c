#pragma bank 255
#include "acid_wolf.h"
#include <gb/cgb.h>
#include "actor_internal.h"
#include "player.h"
#include "aim.h"
#include "gfx.h"
#include "camera.h"
#include "state.h"
#include "oam.h"

#define PROWL 0
#define CROUCH 1
#define LEAP 2
#define LAND 3
#define WINDUP 4
#define RECOVER 5
uint8_t wolf_slot, wolf_state, wolf_timer, wolf_cooldown, wolf_z, wolf_aim;
int8_t wolf_side, wolf_leap_x, wolf_leap_y;
uint16_t wolf_jumps, wolf_spits;

// Side silhouette: ears, pointed muzzle, acid throat, bushy tail. Frames are
// running stride A/B, tucked leap/crouch, and open-mouth spit.
static const char art[4][16][17]={
 {"0000000000000000","0000000030030000","0000000032330000","0000000032233000",
  "0300000332133300","0323333222231233","0032222222233330","0003222222232300",
  "0003222222233000","0000322222330000","0000332333230000","0000323000323000",
  "0003230000032300","0003300000033300","0000000000000000","0000000000000000"},
 {"0000000000000000","0000000000000000","0000000030030000","0000000032330000",
  "0000000032233000","0333000332133300","0032333222231233","0003222222233330",
  "0003222222232300","0000322222333000","0000332333230000","0003230000032300",
  "0032300000003230","0033300000003330","0000000000000000","0000000000000000"},
 {"0000000000000000","0000000000000000","0000000000000000","0000000030030000",
  "0000000032330000","0330000332133300","0033333222231233","0003222222233330",
  "0000322222232300","0000032222333000","0000033233230000","0000003333300000",
  "0000000000000000","0000000000000000","0000000000000000","0000000000000000"},
 {"0000000030030000","0000000032330000","0000000032233000","0000000332133300",
  "0300003222231233","0323332222233330","0032222222230000","0003222222211330",
  "0003222222233300","0000322222330000","0000332333230000","0000323000323000",
  "0003230000032300","0003330000033300","0000000000000000","0000000000000000"}};
static const uint8_t heights[13]={0,2,4,6,7,8,8,8,7,6,4,2,0};
static const uint8_t shadow[32]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,60,60,126,126,0,0};
static const uint8_t glob[32]={0,0,0,0,0,0,0,0,0,0,24,24,60,36,126,66,126,66,60,60,24,24,0,0,0,0,0,0,0,0};

static uint8_t direction(int16_t dx,int16_t dy) {
    uint16_t ax=dx<0?-dx:dx,ay=dy<0?-dy:dy;
    uint8_t q=ay*5<ax?0:ay*3<ax*2?1:ay*2<ax*3?2:ay<ax*5?3:4;
    return (dx<0?(dy<0?8+q:8-q):(dy<0?16-q:q))&15;
}
static uint8_t move(int8_t dx,int8_t dy) {
    uint16_t x=actor_x[wolf_slot]+dx,y=actor_y[wolf_slot]+dy;
    if(!enemy_box_free(x,y))return 0;
    actor_x[wolf_slot]=x;actor_y[wolf_slot]=y;return 1;
}
static void pose(void) {
    uint8_t frame=wolf_state==CROUCH || wolf_state==LEAP?2:
        wolf_state==WINDUP?3:wolf_state==PROWL?((uint8_t)frame_count>>2)&1:0;
    uint8_t left=wolf_state==WINDUP || wolf_state==RECOVER?
        wolf_aim>4 && wolf_aim<12:player_x<actor_x[wolf_slot];
    uint8_t flip=left?S_FLIPX:0,tile=96+frame*4;
    actor_t0[wolf_slot]=tile+(flip?2:0);actor_t1[wolf_slot]=tile+(flip?0:2);
    actor_p0[wolf_slot]=actor_p1[wolf_slot]=SPR_PAL_SHOOTER|flip;
    actor_flash[wolf_slot]=actor_hurt[wolf_slot] || (wolf_state==WINDUP && (wolf_timer&4));
}
void acid_wolf_start(void) BANKED {
    uint8_t data[32];
    for(uint8_t f=0;f<4;f++)for(uint8_t half=0;half<2;half++) {
        for(uint8_t y=0;y<16;y++) {
            uint8_t lo=0,hi=0;
            for(uint8_t x=0;x<8;x++) {
                uint8_t c=art[f][y][half*8+x]-'0';
                lo=(lo<<1)|(c&1);hi=(hi<<1)|(c>>1);
            }
            data[y*2]=lo;data[y*2+1]=hi;
        }
        set_sprite_data(96+f*4+half*2,2,data);
    }
    set_sprite_data(112,2,shadow);set_sprite_data(bullet_vram,2,glob);
    pea_frames[0]=pea_frames[1]=pea_frames[4]=pea_frames[5]=0;
    pal_base_obj[SPR_PAL_SHOOTER*4+1]=RGB(23,31,6);
    pal_base_obj[SPR_PAL_SHOOTER*4+2]=RGB(17,12,24);
    pal_base_obj[SPR_PAL_SHOOTER*4+3]=RGB(3,3,7);
    pal_base_obj[SPR_PAL_BULLET*4+1]=RGB(27,31,10);
    pal_base_obj[SPR_PAL_BULLET*4+2]=RGB(9,25,3);
    pal_base_obj[SPR_PAL_BULLET*4+3]=RGB(2,8,2);
    wolf_slot=actor_spawn(ACTOR_PEASHOOTER,72,28);
    actor_scripted[wolf_slot]=10;actor_hp[wolf_slot]=30;
    enemy_bullet_damage=18;enemy_bullet_cause=13;
    wolf_state=PROWL;wolf_timer=40;wolf_cooldown=20;wolf_side=1;
    wolf_z=wolf_aim=wolf_leap_x=wolf_leap_y=0;wolf_jumps=wolf_spits=0;pose();
}
// React to trajectories, not buttons. The dominant axis must be approaching
// and its cross-track corridor narrow enough to threaten the wolf.
static uint8_t dodge(void) {
    for(uint8_t n=0;n<SHOT_MAX;n++)if(shots[n].alive) {
        int16_t dx=(int16_t)actor_x[wolf_slot]+4-shots[n].x;
        int16_t dy=(int16_t)actor_y[wolf_slot]-shots[n].y;
        int16_t vx=shots[n].vx,vy=shots[n].vy;
        uint16_t ax=vx<0?-vx:vx,ay=vy<0?-vy:vy;
        if(dx<=-40 || dx>=40 || dy<=-40 || dy>=40)continue;
        if(ax>ay) {
            if(dy<=-18 || dy>=18 || !((dx>0 && vx>0)||(dx<0 && vx<0)))continue;
            wolf_leap_x=0;wolf_leap_y=wolf_side;
        } else {
            if(dx<=-18 || dx>=18 || !((dy>0 && vy>0)||(dy<0 && vy<0)))continue;
            wolf_leap_x=wolf_side;wolf_leap_y=0;
        }
        if(!enemy_box_free(actor_x[wolf_slot]+wolf_leap_x*30,actor_y[wolf_slot]+wolf_leap_y*30)) {
            wolf_leap_x=-wolf_leap_x;wolf_leap_y=-wolf_leap_y;
            if(!enemy_box_free(actor_x[wolf_slot]+wolf_leap_x*30,actor_y[wolf_slot]+wolf_leap_y*30))return 0;
        }
        wolf_state=CROUCH;wolf_timer=4;wolf_cooldown=84;return 1;
    }
    return 0;
}
static void windup(void) {
    int16_t dx=(int16_t)player_x-actor_x[wolf_slot],dy=(int16_t)player_y+2-actor_y[wolf_slot];
    wolf_aim=direction(dx,dy);
    uint16_t ax=dx<0?-dx:dx,ay=dy<0?-dy:dy;
    uint16_t dist=(ax>ay?ax+(ay>>1):ay+(ax>>1))*8;
    int16_t vx=dx*8,vy=dy*8;
    // Preserve the exact target slope without division or 32-bit arithmetic.
    // Scale to roughly 1.5--2.25 px/frame; quantized aim would miss a stationary
    // player between the sixteen visual facing angles.
    if(!dist){vy=512;dist=512;}
    while(dist>512){dist>>=1;vx>>=1;vy>>=1;}
    while(dist<256){dist<<=1;vx*=2;vy*=2;}
    if(dist<384){vx+=vx>>1;vy+=vy>>1;}
    actor_vx[wolf_slot]=vx;actor_vy[wolf_slot]=vy;
    wolf_state=WINDUP;wolf_timer=18;
}
static void spit(void) {
    uint8_t count=0;
    for(uint8_t n=0;n<BULLET_MAX && count<3;n++)if(!bullets[n].alive) {
        int16_t vx=actor_vx[wolf_slot],vy=actor_vy[wolf_slot];
        if(count==0){vx-=actor_vy[wolf_slot]>>2;vy+=actor_vx[wolf_slot]>>2;}
        else if(count==2){vx+=actor_vy[wolf_slot]>>2;vy-=actor_vx[wolf_slot]>>2;}
        Bullet *b=&bullets[n];b->alive=1;b->xs=b->ys=0;
        b->x=actor_x[wolf_slot]+4;b->y=actor_y[wolf_slot];
        bullet_set_velocity(b,vx,vy);
        bullet_count++;bullets_fired++;count++;
    }
    wolf_spits++;wolf_state=RECOVER;wolf_timer=22;
}
void acid_wolf_update(void) BANKED {
    if(!actor_type[wolf_slot] || !actor_hp[wolf_slot]) {
        for(uint8_t n=0;n<BULLET_MAX;n++)bullets[n].alive=0;
        bullet_count=wolf_z=0;return;
    }
    if(wolf_timer)wolf_timer--;
    if(wolf_cooldown)wolf_cooldown--;
    if(wolf_state==PROWL) {
        if(!wolf_cooldown && !((uint8_t)frame_count&3) && dodge()){pose();return;}
        int16_t dx=(int16_t)player_x-actor_x[wolf_slot],dy=(int16_t)player_y-actor_y[wolf_slot];
        uint16_t ax=dx<0?-dx:dx,ay=dy<0?-dy:dy;
        int8_t sx=dx>0?1:dx<0?-1:0,sy=dy>0?1:dy<0?-1:0,mx,my;
        if(ax>48 || ay>48){mx=sx;my=sy;}
        else if(ax<25 && ay<25){mx=-sx;my=-sy;}
        else {mx=ax>ay?0:-sy*wolf_side;my=ax>ay?sx*wolf_side:0;}
        if(!move(mx,my)){wolf_side=-wolf_side;move(-mx,-my);}
        if(!wolf_timer)windup();
    } else if(wolf_state==CROUCH && !wolf_timer) {
        wolf_state=LEAP;wolf_timer=12;wolf_jumps++;
    } else if(wolf_state==LEAP) {
        for(uint8_t n=0;n<3;n++)if(!move(wolf_leap_x,wolf_leap_y)){wolf_timer=0;break;}
        wolf_z=heights[wolf_timer];
        if(!wolf_timer){wolf_state=LAND;wolf_timer=14;wolf_side=-wolf_side;}
    } else if(wolf_state==LAND && !wolf_timer)windup();
    else if(wolf_state==WINDUP && !wolf_timer)spit();
    else if(wolf_state==RECOVER && !wolf_timer){wolf_state=PROWL;wolf_timer=32;}
    pose();
}
void acid_wolf_draw(void) BANKED {
    // This single-actor trial uses the normal dynamic pool. Only the shadow
    // owns an overlay slot; never reserve away projectile or player sprites.
    for(uint8_t i=OAM_PROP;i<OAM_FIRST_ACTOR;i++)shadow_OAM[i].y=0;
    if(!actor_type[wolf_slot] || !actor_hp[wolf_slot])return;
    shadow_OAM[OAM_PROP].x=actor_x[wolf_slot]+12-cam_x;
    shadow_OAM[OAM_PROP].y=actor_y[wolf_slot]+18-cam_y;
    shadow_OAM[OAM_PROP].tile=112;shadow_OAM[OAM_PROP].prop=SPR_PAL_UI;
    // With exactly one actor it emits first, before every projectile.
    shadow_OAM[OAM_FIRST_ACTOR].y-=wolf_z;
    shadow_OAM[OAM_FIRST_ACTOR+1].y-=wolf_z;
}
