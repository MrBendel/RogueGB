#pragma bank 255
#include "wireweaver.h"
#include <gb/cgb.h>
#include "actor_internal.h"
#include "player.h"
#include "health.h"
#include "aim.h"
#include "input.h"
#include "gfx.h"
#include "camera.h"
#include "state.h"
#include "wireweaver_art.inc"
extern uint8_t player_jump_buffer;
// Scuttle, web warning, travelling web, recovery, lunge warning, lunge.
uint8_t weaver_slot,weaver_state,weaver_timer,weaver_side,weaver_tx,weaver_ty;
uint8_t web_x,web_y,web_life,web_trap,web_grace,weaver_spits;
static void pose(void) {
 uint8_t i=weaver_slot,t=96+(((uint8_t)frame_count>>2)&1)*4;
 actor_t0[i]=t;actor_t1[i]=t+2;actor_p0[i]=actor_p1[i]=SPR_PAL_SHOOTER;
 actor_flash[i]=actor_hurt[i] || ((weaver_state==1 || weaver_state==4)&&(weaver_timer&4));
}
void wireweaver_start(void) BANKED {
 set_sprite_data(96,sizeof(weaver_art)/16,weaver_art);
 // Web threads and a hollow warning outline, each a two-part 16x16 sprite.
 uint8_t data[32];
 for(uint8_t f=0;f<2;f++)for(uint8_t half=0;half<2;half++) {
  for(uint8_t y=0;y<16;y++) {
   uint8_t bits=0;
   for(uint8_t x=0;x<8;x++) {
    uint8_t xx=half*8+x;
    uint8_t on=f?(xx==0||xx==15||y==0||y==15):(xx==y||xx==15-y||xx==7||y==7||((xx==3||xx==12)&&y>=3&&y<=12)||((y==3||y==12)&&xx>=3&&xx<=12));
    bits=(bits<<1)|on;
   }
   data[y*2]=bits;data[y*2+1]=0;
  }
  set_sprite_data(104+f*4+half*2,2,data);
 }
 pal_base_obj[SPR_PAL_SHOOTER*4+1]=RGB(31,23,8);
 pal_base_obj[SPR_PAL_SHOOTER*4+2]=RGB(24,7,20);
 pal_base_obj[SPR_PAL_SHOOTER*4+3]=RGB(5,2,10);
 pal_base_obj[SPR_PAL_SLIME*4+1]=RGB(18,29,31);
 weaver_slot=actor_spawn(ACTOR_PEASHOOTER,72,32);actor_scripted[weaver_slot]=16;actor_hp[weaver_slot]=14;
 weaver_state=weaver_side=weaver_spits=web_life=web_trap=web_grace=0;weaver_timer=40;pose();
}
void wireweaver_pre(void) BANKED {
 if(!actor_type[weaver_slot] || !actor_hp[weaver_slot]){web_trap=web_grace=0;return;}
 if(web_grace)web_grace--;
 if(!web_trap && !web_grace && web_life && !player_air && !player_stun && !player_fall_timer) {
  int16_t dx=(int16_t)player_x-web_x,dy=(int16_t)player_y-web_y;
  if(dx> -10 && dx<14 && dy> -10 && dy<14)web_trap=36;
 }
 if(web_trap) {
  // Snared feet: aiming/firing and pause still work, but walking and jumping
  // cannot cancel the hold. Raw input edges are preserved by input.c.
  joy&=~(J_LEFT|J_RIGHT|J_UP|J_DOWN|J_B);
  joy_pressed&=~J_B;
  player_jump_buffer=0;
  player_vx=player_vy=0;player_xs=player_ys=0;
  web_trap--;
  if(!web_trap)web_grace=48;
 }
}
static void rest(void){weaver_state=3;weaver_timer=30;}
void wireweaver_update(void) BANKED {
 uint8_t i=weaver_slot;
 if(!actor_type[i] || !actor_hp[i]){web_life=web_trap=web_grace=0;return;}
 if(web_life)web_life--;
 if(weaver_timer)weaver_timer--;
 int16_t dx=(int16_t)player_x-actor_x[i],dy=(int16_t)player_y-actor_y[i];
 if(weaver_state==0) {
  int8_t mx=weaver_side?-1:1,my=dy>48?1:dy<28?-1:0;
  if(!enemy_box_free(actor_x[i]+mx,actor_y[i]))weaver_side^=1;else actor_x[i]+=mx;
  if(enemy_box_free(actor_x[i],actor_y[i]+my))actor_y[i]+=my;
  if(!weaver_timer) {
   if(dx> -32&&dx<32&&dy> -32&&dy<32) {
    actor_vx[i]=dx*24;actor_vy[i]=dy*24;actor_xs[i]=actor_ys[i]=0;
    weaver_state=4;weaver_timer=20;
   } else {
    weaver_tx=player_x<16?16:player_x>128?128:player_x;
    weaver_ty=player_y<24?24:player_y>104?104:player_y;
    weaver_state=1;weaver_timer=26;
   }
  }
 } else if(weaver_state==1&&!weaver_timer){weaver_state=2;weaver_timer=16;weaver_spits++;}
 else if(weaver_state==2&&!weaver_timer){web_x=weaver_tx;web_y=weaver_ty;web_life=210;rest();}
 else if(weaver_state==3&&!weaver_timer){weaver_state=0;weaver_timer=36;weaver_side^=1;}
 else if(weaver_state==4&&!weaver_timer){weaver_state=5;weaver_timer=12;}
 else if(weaver_state==5) {
  int16_t fx=(int16_t)actor_xs[i]+actor_vx[i],fy=(int16_t)actor_ys[i]+actor_vy[i];
  uint16_t x=actor_x[i]+(fx>>8),y=actor_y[i]+(fy>>8);
  if(enemy_box_free(x,y)){actor_x[i]=x;actor_y[i]=y;actor_xs[i]=(uint8_t)fx;actor_ys[i]=(uint8_t)fy;}else rest();
  dx=(int16_t)player_x-actor_x[i];dy=(int16_t)player_y-actor_y[i];
  if(weaver_state==5&&player_hittable()&&dx> -12&&dx<12&&dy> -12&&dy<12){hurt_cause=19;player_hurt(12,actor_x[i]+8,actor_y[i]+8);}
  if(!weaver_timer)rest();
 }
 pose();
}
static void pair(uint8_t slot,uint8_t x,uint8_t y,uint8_t tile) {
 for(uint8_t j=0;j<2;j++) {
  shadow_OAM[slot+j].x=x+8+j*8-cam_x;shadow_OAM[slot+j].y=y+16-cam_y;
  shadow_OAM[slot+j].tile=tile+j*2;shadow_OAM[slot+j].prop=SPR_PAL_SLIME;
 }
}
void wireweaver_draw(void) BANKED {
 for(uint8_t i=11;i<15;i++)shadow_OAM[i].y=0;
 if(!actor_type[weaver_slot]||!actor_hp[weaver_slot])return;
 // Four overlay sprites plus the two-part body leave four player/weapon columns.
 if(web_trap || web_life || weaver_state==1 || weaver_state==2)aim_limit_overlays(16,144,4);
 if(web_trap)pair(13,player_x,player_y,104);
 else if(web_life && (web_life>30 || (web_life&4)))pair(13,web_x,web_y,104);
 if(weaver_state==1)pair(11,weaver_tx,weaver_ty,108);
 else if(weaver_state==2) {
  uint8_t i=weaver_slot,t=16-weaver_timer;
  uint8_t x=actor_x[i]+(((int16_t)weaver_tx-actor_x[i])*t>>4),y=actor_y[i]+(((int16_t)weaver_ty-actor_y[i])*t>>4);
  pair(11,x,y,104);
 }
}
