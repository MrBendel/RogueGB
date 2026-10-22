#pragma bank 255
#include "cragback.h"
#include <gb/cgb.h>
#include "boss.h"
#include "actor_internal.h"
#include "player.h"
#include "health.h"
#include "camera.h"
#include "gfx.h"
#include "oam.h"
#include "state.h"

extern uint8_t oam_next;
uint8_t crag_target_x,crag_target_y,crag_rock_timer,crag_rock_z,crag_dust;
int16_t crag_rock_fx,crag_rock_fy,crag_rock_vx,crag_rock_vy;
uint8_t crag_rock_x,crag_rock_y;
uint16_t crag_throws,crag_slams,crag_impacts;
uint8_t crag_hop_timer,crag_hop_z,crag_hops_left,crag_hop_side;
uint16_t crag_hops;
int16_t crag_hop_fx,crag_hop_fy,crag_hop_vx,crag_hop_vy;
static const uint8_t hop_arc[16]={2,3,5,6,7,8,8,8,8,7,6,5,3,2,1,0};
static const uint8_t arc[32]={12,15,18,21,23,25,27,28,29,30,30,30,30,29,28,27,26,24,22,20,18,16,14,12,10,8,6,4,3,2,1,0};

// Three 32x32 poses: walking, overhead wind-up, crouched digging recovery.
// Each row is generated directly as GBC two-bit tiles while the display is off.
static void load_art(void) {
 uint8_t data[32];
 for(uint8_t pose=0;pose<3;pose++)for(uint8_t part=0;part<8;part++) {
  for(uint8_t row=0;row<16;row++) {
   uint8_t lo=0,hi=0,y=(part>>2)*16+row;
   for(uint8_t col=0;col<8;col++) {
    uint8_t x=(part&3)*8+col,c=0;
    uint8_t yy=pose==2?(y>5?y-5:0):y;
    uint8_t left=8,right=23;
    if(yy>=8 && yy<=12){left=2;right=29;}
    if(yy>=13 && yy<=24){left=0;right=31;}
    if(yy>=2 && yy<=24 && x>=left && x<=right) {
     c=(x==left||x==right||yy==2||yy==24)?3:2;
     if(yy>=13 && ((x==7)||(x==24)))c=3;
     if(yy==10 && x>3&&x<28)c=1;
    }
    if(yy>=26 && yy<=30 && ((x>=6&&x<=13)||(x>=18&&x<=25)))c=3;
    if(yy>=4 && yy<=8 && x>=10 && x<=21)c=3;
    if(yy==6 && (x==12||x==19))c=1;
    if(yy>=14 && yy<=21 && x>=11 && x<=20)c=3;
    if(yy>=16 && yy<=19 && x>=13 && x<=18)c=1;
    if(pose==1 && y<17 && ((x>=1&&x<=6)||(x>=25&&x<=30)))c=(x==1||x==6||x==25||x==30||y==0)?3:2;
    if(pose==1 && y>16 && (x<8||x>23))c=0;
    lo=(lo<<1)|(c&1);hi=(hi<<1)|(c>>1);
   }
   data[row*2]=lo;data[row*2+1]=hi;
  }
  set_sprite_data(96+pose*16+part*2,2,data);
 }
 for(uint8_t kind=0;kind<3;kind++) {
  for(uint8_t y=0;y<16;y++) {
   uint8_t lo=0,hi=0;
   for(uint8_t x=0;x<8;x++) {
    uint8_t c=0;
    if(kind==0 && y>=3 && y<=11 && x>0 && x<7)c=(x==1||x==6||y==3||y==11)?3:2;
    if(kind==0 && y>=5&&y<=6&&x>=2&&x<=4)c=1;
    if(kind==1 && y>=7&&y<=9 && x>0&&x<7)c=3;
    if(kind==2 && y>=3&&y<=12 && ((x+y)&3)==0)c=1;
    lo=(lo<<1)|(c&1);hi=(hi<<1)|(c>>1);
   }
   data[y*2]=lo;data[y*2+1]=hi;
  }
  set_sprite_data(144+kind*2,2,data);
 }
}
static void hit(uint8_t x,uint8_t y,uint8_t radius,uint8_t damage) {
 int16_t dx=(int16_t)player_x+8-x,dy=(int16_t)player_y+8-y;
 if(player_hittable() && dx>-(int16_t)radius && dx<radius && dy>-(int16_t)radius && dy<radius) {
  hurt_cause=16;player_hurt(damage,x,y);
 }
}
static void fragments(void) {
 for(uint8_t n=0;n<3;n++)for(uint8_t i=0;i<BULLET_MAX;i++)if(!bullets[i].alive) {
  Bullet *b=&bullets[i];b->alive=1;b->x=crag_rock_x-4;b->y=crag_rock_y-8;b->xs=b->ys=0;
  int16_t vx=((int16_t)n-1)*320,vy=crag_rock_vy<0?-320:320;
  bullet_set_velocity(b,vx,vy);bullet_count++;bullets_fired++;break;
 }
}
static void launch(void) {
 crag_target_x=player_x+8;crag_target_y=player_y+8;
 crag_rock_x=actor_x[boss_slot]+8;crag_rock_y=actor_y[boss_slot]+8;
 crag_rock_fx=(int16_t)crag_rock_x<<7;crag_rock_fy=(int16_t)crag_rock_y<<7;
 crag_rock_vx=((int16_t)crag_target_x-crag_rock_x)*4;
 crag_rock_vy=((int16_t)crag_target_y-crag_rock_y)*4;
 crag_rock_timer=32;crag_rock_z=12;crag_throws++;boss_attacks++;
 boss_state=BOSS_CHARGE;boss_timer=32;
}
static void hop_launch(void) {
 int16_t x=actor_x[boss_slot],y=actor_y[boss_slot];
 int16_t tx=x+(player_x>x?16:-16)+(crag_hop_side?32:-32);
 int16_t ty=y+(player_y>y?24:-24)+(crag_hop_side?-12:12);
 if(tx<24)tx=24;if(tx>120)tx=120;
 if(ty<40)ty=40;if(ty>96)ty=96;
 if(tx==x && ty==y){tx=72;ty=68;}
 crag_hop_side^=1;crag_hops_left--;crag_hops++;
 crag_hop_fx=x<<4;crag_hop_fy=y<<4;
 crag_hop_vx=tx-x;crag_hop_vy=ty-y;
 crag_hop_timer=16;actor_state[boss_slot]=STATE_HOP;
}
void cragback_start(void) BANKED {
 load_art();boss_kind=BOSS_CRAGBACK;boss_slot=actor_spawn(ACTOR_PEASHOOTER,72,40);
 actor_hp[boss_slot]=boss_hp=boss_hp_max=32;actor_scripted[boss_slot]=13;
 actor_guard[boss_slot]=1;actor_t0[boss_slot]=104;actor_t1[boss_slot]=106;
 boss_state=BOSS_RETREAT;boss_timer=36;boss_phase=boss_reward=boss_relays=0;boss_cycles=boss_attacks=0;
 crag_rock_timer=crag_rock_z=crag_dust=0;crag_throws=crag_slams=crag_impacts=0;
 crag_hop_timer=crag_hop_z=crag_hops_left=crag_hop_side=0;crag_hops=0;
 bullet_vram=144;enemy_bullet_damage=8;enemy_bullet_cause=16;
 pal_base_obj[SPR_PAL_SHOOTER*4+1]=RGB(24,30,13);
 pal_base_obj[SPR_PAL_SHOOTER*4+2]=RGB(8,18,13);
 pal_base_obj[SPR_PAL_SHOOTER*4+3]=RGB(3,7,6);
 pal_base_obj[SPR_PAL_BULLET*4+1]=RGB(28,25,17);
 pal_base_obj[SPR_PAL_BULLET*4+2]=RGB(18,13,8);
 pal_base_obj[SPR_PAL_BULLET*4+3]=RGB(6,5,4);
 health_give_shield(SHIELD_START_CAP,SHIELD_START_DELAY,SHIELD_START_RATE);
 bkg_text(1,0,"CRAGBACK");
}
void cragback_update(void) BANKED {
 if(crag_dust)crag_dust--;
 if(boss_timer)boss_timer--;
 if(boss_state==BOSS_REINFORCE) {
  if(crag_hop_timer) {
   crag_hop_fx+=crag_hop_vx;crag_hop_fy+=crag_hop_vy;
   uint16_t x=crag_hop_fx>>4,y=crag_hop_fy>>4;
   if(enemy_box_free(x,y)){actor_x[boss_slot]=x;actor_y[boss_slot]=y;}
   crag_hop_z=hop_arc[16-crag_hop_timer];crag_hop_timer--;
   if(!crag_hop_timer){actor_state[boss_slot]=STATE_REST;boss_timer=8;camera_shake(SHAKE_SMALL);}
  } else if(!boss_timer) {
   if(crag_hops_left)hop_launch();
   else {boss_state=BOSS_WARN;boss_timer=40;}
  }
 } else if(boss_state==BOSS_RETREAT) {
  int16_t dx=(int16_t)player_x-actor_x[boss_slot],dy=(int16_t)player_y-actor_y[boss_slot];
  if(dx>-28&&dx<28&&dy>-28&&dy<28){boss_state=BOSS_RELAYS;boss_timer=28;}
  else {
   if(!((uint8_t)frame_count&1)) {
    uint16_t x=actor_x[boss_slot],y=actor_y[boss_slot];
    if(dx>48||dx< -48||dy>48||dy< -48) {
     x+=dx>0?1:dx<0?-1:0;y+=dy>0?1:dy<0?-1:0;
     if(x>=24&&x<=120&&y>=40&&y<=96&&enemy_box_free(x,y)){actor_x[boss_slot]=x;actor_y[boss_slot]=y;}
    }
   }
   if(!boss_timer){boss_state=BOSS_WARN;boss_timer=40;}
  }
 } else if(boss_state==BOSS_WARN) {
  if(!boss_timer)launch();
 } else if(boss_state==BOSS_CHARGE) {
  crag_rock_fx+=crag_rock_vx;crag_rock_fy+=crag_rock_vy;
  crag_rock_x=crag_rock_fx>>7;crag_rock_y=crag_rock_fy>>7;
  crag_rock_z=arc[32-crag_rock_timer];crag_rock_timer--;
  if(!crag_rock_timer) {
   crag_rock_x=crag_target_x;crag_rock_y=crag_target_y;crag_rock_z=0;
   hit(crag_rock_x,crag_rock_y,14,18);fragments();camera_shake(SHAKE_MEDIUM);
   actor_guard[boss_slot]=0;crag_dust=10;crag_impacts++;boss_state=BOSS_RECOVER;boss_timer=64;boss_cycles++;
  }
 } else if(boss_state==BOSS_RELAYS) {
  if(!boss_timer) {
   crag_rock_x=actor_x[boss_slot]+8;crag_rock_y=actor_y[boss_slot]+8;
   hit(crag_rock_x,crag_rock_y,32,16);camera_shake(SHAKE_BIG);
   actor_guard[boss_slot]=0;crag_dust=12;crag_slams++;boss_state=BOSS_RECOVER;boss_timer=56;boss_cycles++;
  }
 } else if(boss_state==BOSS_RECOVER && !boss_timer){actor_guard[boss_slot]=1;boss_state=BOSS_REINFORCE;boss_timer=10;crag_hops_left=2;}
}
static void sprite(uint8_t slot,uint8_t x,uint8_t y,uint8_t tile,uint8_t pal) {
 shadow_OAM[slot].x=x-cam_x+8;shadow_OAM[slot].y=y-cam_y+16;
 shadow_OAM[slot].tile=tile;shadow_OAM[slot].prop=pal;
}
// Called immediately after the ordinary actor emitter, BEFORE projectiles.
// Three prop sprites plus five dynamic sprites form the 32x32 body. Slot 11
// keeps the rock in front of its thrower; the shadow uses one dynamic entry.
void cragback_emit(void) BANKED {
 for(uint8_t i=11;i<15;i++)shadow_OAM[i].y=0;
 if(boss_state==BOSS_WON || !actor_type[boss_slot]){crag_rock_timer=crag_dust=crag_hop_timer=crag_hop_z=0;return;}
 uint8_t base=oam_next-2;oam_next+=3;
 uint8_t pose=(boss_state==BOSS_WARN||boss_state==BOSS_RELAYS)?1:(boss_state==BOSS_RECOVER || (boss_state==BOSS_REINFORCE && !crag_hop_timer))?2:0;
 uint8_t pal=(actor_hurt[boss_slot] || (boss_state==BOSS_RELAYS && (boss_timer&4)))?SPR_PAL_SHOT:SPR_PAL_SHOOTER;
 uint8_t bob=boss_state==BOSS_RETREAT?((uint8_t)frame_count>>3)&1:0;
 for(uint8_t p=0;p<8;p++)sprite(p<3?12+p:base+p-3,
  actor_x[boss_slot]-8+(p&3)*8,actor_y[boss_slot]-16+(p>>2)*16-bob-crag_hop_z,96+pose*16+p*2,pal);
 if(crag_hop_z) {
  sprite(11,actor_x[boss_slot],actor_y[boss_slot]+1,146,SPR_PAL_SHOOTER);
  sprite(oam_next++,actor_x[boss_slot]+8,actor_y[boss_slot]+1,146,SPR_PAL_SHOOTER);
 } else if(crag_rock_timer) {
  sprite(oam_next++,crag_rock_x-4,crag_rock_y-8,146,SPR_PAL_SHOOTER);
  sprite(11,crag_rock_x-4,crag_rock_y-8-crag_rock_z,144,SPR_PAL_BULLET);
 } else if(boss_state==BOSS_WARN) {
  sprite(11,actor_x[boss_slot]+4,actor_y[boss_slot]-24,144,SPR_PAL_BULLET);
 } else if(crag_dust) {
  sprite(11,crag_rock_x-8,crag_rock_y-8,148,SPR_PAL_BULLET);
  sprite(oam_next++,crag_rock_x+2,crag_rock_y-10,148,SPR_PAL_BULLET);
 }
}
