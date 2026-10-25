#pragma bank 255
#include "globfather.h"
#include <gb/cgb.h>
#include <gb/metasprites.h>
#include "boss.h"
#include "aim.h"
#include "actor_internal.h"
#include "player.h"
#include "health.h"
#include "camera.h"
#include "gfx.h"
#include "state.h"
#include "globfather_art.inc"

extern uint8_t oam_next,draw_rotation;
extern uint16_t pal_obj[PAL_COUNT*4];
extern volatile uint8_t pal_dirty;
void actors_emit(void);
// Size 3: king, 2: medium, 1: small. A nonzero size also remembers a killed
// parent after the ordinary shot reaper frees its actor slot.
uint8_t glob_size[ACTOR_MAX],glob_state[ACTOR_MAX],glob_timer[ACTOR_MAX],glob_z[ACTOR_MAX];
uint8_t glob_attacker,glob_alive,glob_defeated;
uint8_t glob_draw_top,glob_draw_bottom;
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
 uint8_t i=0;
 while(i<ACTOR_MAX && actor_type[i])i++;
 if(i==ACTOR_MAX)return; // Two mediums each release four small slimes: eight maximum.
 // Scripted slime art never uses the peashooter's frame cache or AI timers.
 // Avoid that banked setup work when eight children arrive on the same frame.
 actor_type[i]=ACTOR_PEASHOOTER;actor_count++;
 actor_x[i]=bound_x(x,size);actor_y[i]=bound_y(y,size);
 actor_xs[i]=actor_ys[i]=actor_hurt[i]=actor_flash[i]=0;
 actor_state[i]=actor_frame[i]=0;aim_score[i]=AIM_NONE;
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
 boss_hp=boss_hp_max=80;boss_phase=3;boss_reward=boss_relays=0;boss_attacks=boss_cycles=0;
 glob_attacker=0xff;glob_alive=1;glob_defeated=0;glob_splits=glob_hops=glob_landings=0;
 for(uint8_t i=0;i<ACTOR_MAX;i++)glob_size[i]=glob_state[i]=glob_timer[i]=glob_z[i]=0;
 spawn(3,72,76,24);boss_slot=0;
 pal_base_obj[SPR_PAL_SHOOTER*4+1]=RGB(31,31,20);
 pal_base_obj[SPR_PAL_SHOOTER*4+2]=RGB(22,31,3);
 pal_base_obj[SPR_PAL_SHOOTER*4+3]=RGB(7,2,12);
 // The king and mediums never coexist, so they share one palette slot.
 pal_base_obj[SPR_PAL_SLIME*4+1]=RGB(31,24,30);
 pal_base_obj[SPR_PAL_SLIME*4+2]=RGB(31,5,20);
 pal_base_obj[SPR_PAL_SLIME*4+3]=RGB(7,2,12);
 enemy_bullet_damage=8;enemy_bullet_cause=17;
 health_give_shield(SHIELD_START_CAP,SHIELD_START_DELAY,SHIELD_START_RATE);
 bkg_text(1,0,"GLOBFATHER");
}
void globfather_update(void) BANKED {
 // Collect ALL dead parents before spawning: their freed slots can be reused
 // by children, even when both medium slimes die on the same frame.
 uint8_t dead_size[ACTOR_MAX],dead_x[ACTOR_MAX],dead_y[ACTOR_MAX],dead=0;
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
   if(size==2) {
    pal_obj[SPR_PAL_SHOOTER*4+1]=pal_base_obj[SPR_PAL_SHOOTER*4+1]=RGB(23,31,30);
    pal_obj[SPR_PAL_SHOOTER*4+2]=pal_base_obj[SPR_PAL_SHOOTER*4+2]=RGB(2,27,24);
    pal_dirty=1;
   }
   spawn(size,(int16_t)dead_x[d]-offset,dead_y[d]-10,24);
   spawn(size,(int16_t)dead_x[d]+offset,dead_y[d]+10,40);
   if(size==1) {
    spawn(size,(int16_t)dead_x[d]+offset,dead_y[d]-10,32);
    spawn(size,(int16_t)dead_x[d]-offset,dead_y[d]+10,48);
   }
   glob_splits++;
  }
  camera_shake(SHAKE_MEDIUM);
 }
 glob_alive=0;boss_hp=0;boss_phase=0;
 for(uint8_t i=0;i<ACTOR_MAX;i++)if(glob_size[i] && actor_type[i]) {
  uint8_t size=glob_size[i];glob_alive++;
  // Spawn and hop targets are already bounded; only weapon knockback can
  // push a resting slime outside the arena between updates.
  if(actor_hurt[i]){actor_x[i]=bound_x(actor_x[i],size);actor_y[i]=bound_y(actor_y[i],size);}
  boss_hp+=actor_hp[i]+(size==3?60:size==2?20:0);
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
 glob_draw_top=glob_draw_bottom=0;
 for(uint8_t i=11;i<15;i++)shadow_OAM[i].y=0;
 if(boss_phase==1) {
  // Small slimes are ordinary two-part 16x16 sprites. Use the fast assembly
  // renderer and its rotating priority; collision coordinates stay on the floor.
  for(uint8_t i=0;i<ACTOR_MAX;i++)if(actor_type[i]) {
   uint8_t state=glob_state[i];
   actor_t0[i]=glob_parts[glob_first[state==2?1:0]][2];
   actor_t1[i]=actor_t0[i]+2;
   actor_p0[i]=actor_p1[i]=SPR_PAL_SLIME;
   actor_flash[i]=actor_hurt[i] || ((state==3 || state==1) && (glob_timer[i]&4));
  }
  uint8_t jumping=glob_attacker;
  if(jumping!=0xff)actor_y[jumping]-=glob_z[jumping];
  actors_emit();
  if(jumping!=0xff)actor_y[jumping]+=glob_z[jumping];
  return;
 }
 uint8_t slot=11;
 uint8_t tops[5],ends[5],widths[5],accepted=0;
 // Rotate descendants so a crowded scanline cannot keep hiding the same slime.
 // Eight small slimes still use only 16 sprites (the king uses 17).
 for(uint8_t n=0;n<ACTOR_MAX;n++) {
  uint8_t i=draw_rotation&1?ACTOR_MAX-1-n:n;
  if(!glob_size[i] || !actor_type[i] || !actor_hp[i])continue;
  uint8_t size=glob_size[i],state=glob_state[i];
  uint8_t frame=(size-1)*2+(state==2?1:0);
  if(size==3 && state==1)frame=6;
  uint8_t pal=actor_hurt[i] || ((state==3 || state==1) && (glob_timer[i]&4))?SPR_PAL_FLASH:size==1?SPR_PAL_SLIME:SPR_PAL_SHOOTER;
  uint8_t end=glob_first[frame]+glob_count[frame];
  uint8_t sx=actor_x[i]+16-cam_x,sy=actor_y[i]+32-glob_z[i]-cam_y;
  if(size>=1) {
   uint8_t top=sy+glob_parts[glob_first[frame]][1],bottom=sy+16+glob_parts[end-1][1];
   if(!glob_draw_bottom || top<glob_draw_top)glob_draw_top=top;
   if(bottom>glob_draw_bottom)glob_draw_bottom=bottom;
  }
  uint8_t fits=1;
  if(boss_phase==2) {
   uint8_t top=sy+glob_parts[glob_first[frame]][1],bottom=top+(size==2?32:16);
   uint8_t width=size==2?4:2,used=width;
   for(uint8_t j=0;j<accepted;j++)if(top<ends[j] && bottom>tops[j])used+=widths[j];
   fits=used<=6;
   if(fits){tops[accepted]=top;ends[accepted]=bottom;widths[accepted]=width;accepted++;}
  }
  uint8_t count=glob_count[frame];
  if(fits)move_metasprite_ex(glob_meta[frame],0,pal,slot,sx,sy);
  else for(uint8_t p=0;p<count;p++) {
   shadow_OAM[slot+p].y=0;shadow_OAM[slot+p].tile=glob_parts[glob_first[frame]+p][2];
  }
  slot+=count;

 }
 if(slot>15)oam_next=slot;
}
void globfather_draw(void) BANKED {
 // Reserve four columns for the player/shadow and paired weapon overlays.
 if(glob_draw_bottom)aim_limit_overlays(glob_draw_top,glob_draw_bottom,4);
}
