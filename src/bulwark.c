#pragma bank 255
#include "bulwark.h"
#include <gb/cgb.h>
#include "actor_internal.h"
#include "player.h"
#include "health.h"
#include "aim.h"
#include "gfx.h"
#include "camera.h"
#include "state.h"
#include "bulwark_art.inc"
// Advance, burst warning, burst, recovery, bash warning, bash.
uint8_t bulwark_slot,bulwark_state,bulwark_timer,bulwark_face,bulwark_turn,bulwark_flash;
uint16_t bulwark_blocks,bulwark_shots,bulwark_bashes;
static uint8_t shield_up(void){return bulwark_state==0 || bulwark_state==4;}
static void face_player(void) {
 int16_t dx=(int16_t)player_x-actor_x[bulwark_slot],dy=(int16_t)player_y-actor_y[bulwark_slot];
 uint16_t ax=dx<0?-dx:dx,ay=dy<0?-dy:dy;
 bulwark_face=ax>ay?(dx<0?2:0):(dy<0?3:1);
}
uint8_t bulwark_blocks_melee(uint16_t x,uint16_t y) BANKED {
 if(!shield_up())return 0;
 int16_t dx=(int16_t)x-actor_x[bulwark_slot],dy=(int16_t)y-actor_y[bulwark_slot];
 int16_t along=bulwark_face==0?dx:bulwark_face==1?dy:bulwark_face==2?-dx:-dy;
 int16_t cross=(bulwark_face&1)?dx:dy;if(cross<0)cross=-cross;
 return along>0 && along>=cross;
}
static void block_shots(void) {
 if(!shield_up())return;
 uint8_t i=bulwark_slot;
 for(uint8_t n=0;n<SHOT_MAX;n++)if(shots[n].alive) {
  Bullet *s=&shots[n];
  int16_t dx=(int16_t)s->x-actor_x[i]-4,dy=(int16_t)s->y-actor_y[i];
  int16_t along=bulwark_face==0?dx:bulwark_face==1?dy:bulwark_face==2?-dx:-dy;
  int16_t velocity=bulwark_face==0?s->vx:bulwark_face==1?s->vy:bulwark_face==2?-s->vx:-s->vy;
  int16_t cross=(bulwark_face&1)?dx:dy,next=cross+(((bulwark_face&1)?s->vx:s->vy)>>8);
  // Sweep the incoming segment against the shield's front corridor, including
  // fast rifle/sniper shots. Rear shots and parallel side shots pass normally.
  if(along>=0 && along<=40 && velocity<0 && along+(velocity>>8)<=12 &&
     (cross>=-12 || next>=-12) && (cross<=12 || next<=12)) {
   s->alive=0;shot_count--;bulwark_blocks++;bulwark_flash=6;
  }
 }
}
static void pose(void) {
 uint8_t i=bulwark_slot,t=96+(bulwark_face==2?8:0)+(((uint8_t)frame_count>>3)&1)*4;
 actor_t0[i]=t;actor_t1[i]=t+2;actor_p0[i]=actor_p1[i]=SPR_PAL_SHOOTER;
 actor_flash[i]=actor_hurt[i] || ((bulwark_state==1 || bulwark_state==4)&&(bulwark_timer&4));
}
static void lock_vector(uint8_t bash) {
 uint8_t i=bulwark_slot;int16_t dx=(int16_t)player_x-actor_x[i],dy=(int16_t)player_y-actor_y[i];
 if(bash){actor_vx[i]=dx*24;actor_vy[i]=dy*24;actor_xs[i]=actor_ys[i]=0;return;}
 int16_t vx=dx*8,vy=dy*8;if(!vx&&!vy)vy=256;
 uint16_t ax=vx<0?-vx:vx,ay=vy<0?-vy:vy,dist=ax>ay?ax+(ay>>1):ay+(ax>>1);
 while(dist>512){dist>>=1;vx>>=1;vy>>=1;}
 while(dist<384){dist<<=1;vx*=2;vy*=2;}
 actor_vx[i]=vx;actor_vy[i]=vy;
}
static void fire(void) {
 for(uint8_t n=0;n<BULLET_MAX;n++)if(!bullets[n].alive) {
  Bullet *b=&bullets[n];b->alive=1;b->xs=b->ys=0;b->x=actor_x[bulwark_slot]+4;b->y=actor_y[bulwark_slot];
  bullet_set_velocity(b,actor_vx[bulwark_slot],actor_vy[bulwark_slot]);bullet_count++;bullets_fired++;bulwark_shots++;return;
 }
}
static void recover(void){bulwark_state=3;bulwark_timer=40;}
void bulwark_start(void) BANKED {
 set_sprite_data(96,sizeof(bulwark_art)/16,bulwark_art);
 uint8_t data[32];
 for(uint8_t p=0;p<3;p++) {
  for(uint8_t y=0;y<16;y++){uint8_t bits=p==0?(y>1&&y<14?0x3c:0):(y>=6&&y<=9?0xff:0);data[y*2]=bits;data[y*2+1]=0;}
  set_sprite_data(112+p*2,2,data);
 }
 pal_base_obj[SPR_PAL_SHOOTER*4+1]=RGB(31,25,10);pal_base_obj[SPR_PAL_SHOOTER*4+2]=RGB(24,10,3);pal_base_obj[SPR_PAL_SHOOTER*4+3]=RGB(3,4,9);
 pal_base_obj[SPR_PAL_SLIME*4+1]=RGB(8,27,31);
 bulwark_slot=actor_spawn(ACTOR_PEASHOOTER,72,32);actor_scripted[bulwark_slot]=17;actor_hp[bulwark_slot]=24;
 bulwark_state=0;bulwark_timer=60;bulwark_turn=20;bulwark_face=1;bulwark_flash=0;
 bulwark_blocks=bulwark_shots=bulwark_bashes=0;enemy_bullet_damage=10;enemy_bullet_cause=20;pose();
}
void bulwark_update(void) BANKED {
 uint8_t i=bulwark_slot;
 if(!actor_type[i] || !actor_hp[i]){for(uint8_t n=0;n<BULLET_MAX;n++)bullets[n].alive=0;bullet_count=bulwark_flash=0;return;}
 if(bulwark_timer)bulwark_timer--;if(bulwark_flash)bulwark_flash--;
 if(bulwark_state==0) {
  if(bulwark_turn)bulwark_turn--;else{face_player();bulwark_turn=24;}
  int8_t mx=bulwark_face==0?1:bulwark_face==2?-1:0,my=bulwark_face==1?1:bulwark_face==3?-1:0;
  if(!((uint8_t)frame_count&1)&&enemy_box_free(actor_x[i]+mx,actor_y[i]+my)){actor_x[i]+=mx;actor_y[i]+=my;}
  if(!bulwark_timer) {
   int16_t dx=(int16_t)player_x-actor_x[i],dy=(int16_t)player_y-actor_y[i];
   uint8_t close=dx>-28&&dx<28&&dy>-28&&dy<28;
   face_player();lock_vector(close);bulwark_state=close?4:1;bulwark_timer=close?22:28;
  }
 } else if(bulwark_state==1&&!bulwark_timer){bulwark_state=2;bulwark_timer=24;fire();}
 else if(bulwark_state==2){if(bulwark_timer==16||bulwark_timer==8)fire();if(!bulwark_timer)recover();}
 else if(bulwark_state==3&&!bulwark_timer){bulwark_state=0;bulwark_timer=54;bulwark_turn=0;}
 else if(bulwark_state==4&&!bulwark_timer){bulwark_state=5;bulwark_timer=12;bulwark_bashes++;}
 else if(bulwark_state==5) {
  int16_t fx=(int16_t)actor_xs[i]+actor_vx[i],fy=(int16_t)actor_ys[i]+actor_vy[i];uint16_t x=actor_x[i]+(fx>>8),y=actor_y[i]+(fy>>8);
  if(enemy_box_free(x,y)){actor_x[i]=x;actor_y[i]=y;actor_xs[i]=(uint8_t)fx;actor_ys[i]=(uint8_t)fy;}else recover();
  int16_t dx=(int16_t)player_x-actor_x[i],dy=(int16_t)player_y-actor_y[i];
  if(bulwark_state==5&&player_hittable()&&dx>-13&&dx<13&&dy>-13&&dy<13){hurt_cause=20;player_hurt(16,actor_x[i]+8,actor_y[i]+8);recover();}
  if(!bulwark_timer)recover();
 }
 block_shots();pose();
}
void bulwark_draw(void) BANKED {
 for(uint8_t j=11;j<15;j++)shadow_OAM[j].y=0;
 if(!actor_type[bulwark_slot]||!actor_hp[bulwark_slot]||!shield_up())return;
 uint8_t i=bulwark_slot,x=actor_x[i]+8-cam_x,y=actor_y[i]+16-cam_y;
 uint8_t pal=bulwark_flash?SPR_PAL_FLASH:SPR_PAL_SLIME;
 if(bulwark_face&1) {
  for(uint8_t j=0;j<2;j++){shadow_OAM[11+j].x=x+j*8;shadow_OAM[11+j].y=y+(bulwark_face==1?8:-8);shadow_OAM[11+j].tile=114+j*2;shadow_OAM[11+j].prop=pal;}
 } else {shadow_OAM[11].x=x+(bulwark_face==0?12:-4);shadow_OAM[11].y=y;shadow_OAM[11].tile=112;shadow_OAM[11].prop=pal;}
 aim_limit_overlays(y,y+16,6);
}
