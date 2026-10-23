#pragma bank 255
#include "globfather.h"
#include <gb/cgb.h>
#include "boss.h"
#include "aim.h"
#include "actor_internal.h"
#include "player.h"
#include "health.h"
#include "camera.h"
#include "gfx.h"
#include "state.h"
#include "globfather_art.inc"

extern uint8_t oam_next;
// Size 3: king, 2: medium, 1: small. A nonzero size also remembers a killed
// parent after the ordinary shot reaper frees its actor slot.
uint8_t glob_size[ACTOR_MAX],glob_state[ACTOR_MAX],glob_timer[ACTOR_MAX],glob_z[ACTOR_MAX];
uint8_t glob_attacker,glob_alive,glob_defeated;
uint16_t glob_splits,glob_hops,glob_landings;
static const uint8_t hop_z[16]={1,2,4,6,7,8,8,8,8,7,6,4,3,2,1,0};
static uint8_t bound_x(int16_t x,uint8_t size) {
 uint8_t lo=size==3?32:size==2?24:16,hi=144-lo;
 return x<lo?lo:x>hi?hi:x;
}
static uint8_t bound_y(int16_t y,uint8_t size) {
 uint8_t lo=size==3?72:size==2?48:32,hi=size==3?96:104;
 return y<lo?lo:y>hi?hi:y;
}
static void spawn(uint8_t size,int16_t x,int16_t y,uint8_t delay) {
 uint8_t i=actor_spawn(ACTOR_PEASHOOTER,bound_x(x,size),bound_y(y,size));
 if(i==0xff)return; // At most four living descendants; the pool has eight.
 glob_size[i]=size;glob_state[i]=3;glob_timer[i]=delay;glob_z[i]=0;
 actor_hp[i]=size==3?20:size==2?10:5;actor_scripted[i]=14;actor_guard[i]=1;
 actor_vx[i]=actor_vy[i]=0;
}
static void splash(uint8_t i) {
 uint8_t n=glob_size[i]==3?6:glob_size[i]==2?3:0;
 static const int8_t dx[6]={-1,0,1,1,0,-1};
 static const int8_t dy[6]={-1,-1,-1,1,1,1};
 for(uint8_t j=0;j<n && bullet_count<6;j++)for(uint8_t k=0;k<BULLET_MAX;k++)if(!bullets[k].alive) {
  Bullet *b=&bullets[k];b->alive=1;b->x=actor_x[i]+4;b->y=actor_y[i]+3;b->xs=b->ys=0;
  int16_t vx=dx[j]*288,vy=dy[j]*288;
  if(n==3 && player_y>actor_y[i])vy=-vy;
  bullet_set_velocity(b,vx,vy);bullet_count++;bullets_fired++;break;
 }
}
static void hurt_near(uint8_t i,uint8_t radius,uint8_t damage) {
 int16_t dx=(int16_t)player_x-actor_x[i],dy=(int16_t)player_y-actor_y[i];
 if(player_hittable() && dx>-(int16_t)radius && dx<radius && dy>-(int16_t)radius && dy<radius) {
  hurt_cause=17;player_hurt(damage,actor_x[i]+8,actor_y[i]+12);
 }
}
static void hop(uint8_t i) {
 uint8_t size=glob_size[i];
 int16_t dx=(int16_t)player_x-actor_x[i],dy=(int16_t)player_y-actor_y[i];
 // Cap travel to keep the threat legible, and lock this vector at takeoff.
 int16_t limit=size==3?32:size==2?40:32;
 if(dx>limit)dx=limit;if(dx< -limit)dx=-limit;
 if(dy>limit)dy=limit;if(dy< -limit)dy=-limit;
 uint8_t tx=bound_x(actor_x[i]+dx,size),ty=bound_y(actor_y[i]+dy,size);
 actor_vx[i]=((int16_t)tx-actor_x[i])*16;actor_vy[i]=((int16_t)ty-actor_y[i])*16;
 actor_xs[i]=actor_ys[i]=0;glob_state[i]=2;glob_timer[i]=16;glob_hops++;boss_attacks++;
}
void globfather_start(void) BANKED {
 set_sprite_data(96,sizeof(glob_art)/16,glob_art);
 boss_kind=BOSS_GLOBFATHER;boss_state=BOSS_RETREAT;boss_timer=0;
 boss_hp=boss_hp_max=60;boss_phase=3;boss_reward=boss_relays=0;boss_attacks=boss_cycles=0;
 glob_attacker=0xff;glob_alive=1;glob_defeated=0;glob_splits=glob_hops=glob_landings=0;
 for(uint8_t i=0;i<ACTOR_MAX;i++)glob_size[i]=glob_state[i]=glob_timer[i]=glob_z[i]=0;
 spawn(3,72,76,24);boss_slot=0;
 pal_base_obj[SPR_PAL_SHOOTER*4+1]=RGB(26,26,19);
 pal_base_obj[SPR_PAL_SHOOTER*4+2]=RGB(15,20,13);
 pal_base_obj[SPR_PAL_SHOOTER*4+3]=RGB(11,8,4);
 enemy_bullet_damage=8;enemy_bullet_cause=17;
 health_give_shield(SHIELD_START_CAP,SHIELD_START_DELAY,SHIELD_START_RATE);
 bkg_text(1,0,"GLOBFATHER");
}
void globfather_update(void) BANKED {
 // Collect ALL dead parents before spawning: their freed slots can be reused
 // by children, even when both medium slimes die on the same frame.
 uint8_t dead_size[4],dead_x[4],dead_y[4],dead=0;
 for(uint8_t i=0;i<ACTOR_MAX;i++)if(glob_size[i] && (!actor_type[i] || !actor_hp[i])) {
  dead_size[dead]=glob_size[i];dead_x[dead]=actor_x[i];dead_y[dead]=actor_y[i];dead++;
  glob_size[i]=glob_z[i]=0;glob_defeated++;
  if(actor_type[i]){actor_type[i]=0;actor_count--;enemies_killed++;}
  if(glob_attacker==i)glob_attacker=0xff;
 }
 if(dead) {
  for(uint8_t b=0;b<BULLET_MAX;b++)bullets[b].alive=0;
  bullet_count=0;
  for(uint8_t d=0;d<dead;d++)if(dead_size[d]>1) {
   uint8_t size=dead_size[d]-1;int8_t offset=size==2?22:12;
   spawn(size,(int16_t)dead_x[d]-offset,dead_y[d]-6,24);
   spawn(size,(int16_t)dead_x[d]+offset,dead_y[d]+6,40);glob_splits++;
  }
  camera_shake(SHAKE_MEDIUM);
 }
 glob_alive=0;boss_hp=0;boss_phase=0;
 for(uint8_t i=0;i<ACTOR_MAX;i++)if(glob_size[i] && actor_type[i]) {
  uint8_t size=glob_size[i];glob_alive++;
  actor_x[i]=bound_x(actor_x[i],size);actor_y[i]=bound_y(actor_y[i],size);
  boss_hp+=actor_hp[i]+(size==3?40:size==2?10:0);
  if(size>boss_phase)boss_phase=size;
  if(dead)continue; // Children get a full, harmless split telegraph.
  if(glob_timer[i])glob_timer[i]--;
  if(glob_state[i]==3) {
   if(!glob_timer[i]){glob_state[i]=0;glob_timer[i]=24;actor_guard[i]=0;}
  } else if(glob_state[i]==0) {
   hurt_near(i,size==3?22:size==2?14:9,8);
   if(!glob_timer[i] && glob_attacker==0xff) {
    glob_attacker=i;glob_state[i]=1;glob_timer[i]=size==3?32:size==2?26:22;actor_guard[i]=1;
   }
  } else if(glob_state[i]==1) {
   if(!glob_timer[i])hop(i);
  } else {
   int16_t fx=(int16_t)actor_xs[i]+actor_vx[i],fy=(int16_t)actor_ys[i]+actor_vy[i];
   actor_x[i]+=fx>>8;actor_y[i]+=fy>>8;actor_xs[i]=(uint8_t)fx;actor_ys[i]=(uint8_t)fy;
   glob_z[i]=hop_z[15-glob_timer[i]];
   if(!glob_timer[i]) {
    glob_z[i]=0;glob_landings++;boss_cycles++;
    hurt_near(i,size==3?26:size==2?18:10,size==3?20:size==2?12:8);
    splash(i);camera_shake(size==3?SHAKE_MEDIUM:SHAKE_SMALL);
    glob_state[i]=0;glob_timer[i]=size==3?64:size==2?48:40;actor_guard[i]=0;glob_attacker=0xff;
   }
  }
 }
 boss_relays=glob_alive;
 boss_state=glob_attacker==0xff?BOSS_RECOVER:glob_state[glob_attacker]==1?BOSS_WARN:BOSS_CHARGE;
 if(!glob_alive){boss_state=BOSS_WON;boss_reward=1;glob_attacker=0xff;}
}
void globfather_emit(void) BANKED {
 for(uint8_t i=11;i<15;i++)shadow_OAM[i].y=0;
 uint8_t prop=11;
 // Draw in actor slot order. All descendants together need at
 // most 17 body sprites; four prop slots leave room for six visible splashes.
 for(uint8_t i=0;i<ACTOR_MAX;i++)if(glob_size[i] && actor_type[i] && actor_hp[i]) {
  uint8_t size=glob_size[i],state=glob_state[i];
  uint8_t frame=(size-1)*2+(state==2?1:0);
  if(size==3 && state==1)frame=6;
  uint8_t pal=actor_hurt[i] || ((state==3 || state==1) && (glob_timer[i]&4))?SPR_PAL_FLASH:SPR_PAL_SHOOTER;
  uint8_t end=glob_first[frame]+glob_count[frame];
  for(uint8_t p=glob_first[frame];p<end;p++) {
   uint8_t slot=prop<15?prop++:oam_next++;
   shadow_OAM[slot].x=actor_x[i]+16+glob_parts[p][0]-cam_x;
   shadow_OAM[slot].y=actor_y[i]+32+glob_parts[p][1]-glob_z[i]-cam_y;
   shadow_OAM[slot].tile=glob_parts[p][2];shadow_OAM[slot].prop=pal;
  }
 }
}
