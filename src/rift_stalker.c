#pragma bank 255
#include "rift_stalker.h"
#include <gb/cgb.h>
#include "actor_internal.h"
#include "player.h"
#include "health.h"
#include "gfx.h"
#include "state.h"
#include "rift_stalker_art.inc"

#define STALK 0
#define WARN 1
#define DASH 2
#define RECOVER 3
uint8_t stalker_slot,stalker_state,stalker_timer,stalker_side,stalker_dashes;

static void pose(void) {
 uint8_t i=stalker_slot,f=((uint8_t)frame_count>>3)&1;
 if(stalker_state==STALK)f+=2;
 actor_t0[i]=96+f*4;actor_t1[i]=98+f*4;
 actor_p0[i]=actor_p1[i]=SPR_PAL_SHOOTER;
 actor_flash[i]=actor_hurt[i] || (stalker_state==WARN && (stalker_timer&4));
 actor_guard[i]=stalker_state!=RECOVER;
}
static void recover(void) {
 stalker_state=RECOVER;stalker_timer=48;
 actor_vx[stalker_slot]=actor_vy[stalker_slot]=0;
}
static void lock_dash(void) {
 uint8_t i=stalker_slot;
 int16_t dx=(int16_t)player_x-actor_x[i],dy=(int16_t)player_y-actor_y[i];
 // Aim once, before the warning. Preserve the diagonal slope and overshoot
 // slightly; movement during the warning can bait a miss.
 while(dx>56 || dx< -56 || dy>56 || dy< -56){dx>>=1;dy>>=1;}
 if(!dx && !dy)dy=24;
 actor_vx[i]=dx*20;actor_vy[i]=dy*20;
 actor_xs[i]=actor_ys[i]=0;stalker_state=WARN;stalker_timer=26;
}
void rift_stalker_start(void) BANKED {
 set_sprite_data(96,sizeof(stalker_art)/16,stalker_art);
 pal_base_obj[SPR_PAL_SHOOTER*4+1]=RGB(15,31,31);
 pal_base_obj[SPR_PAL_SHOOTER*4+2]=RGB(18,7,29);
 pal_base_obj[SPR_PAL_SHOOTER*4+3]=RGB(4,2,10);
 stalker_slot=actor_spawn(ACTOR_PEASHOOTER,40,32);
 actor_scripted[stalker_slot]=15;actor_hp[stalker_slot]=18;
 stalker_state=STALK;stalker_timer=52;stalker_side=stalker_dashes=0;pose();
}
void rift_stalker_update(void) BANKED {
 uint8_t i=stalker_slot;
 if(!actor_type[i] || !actor_hp[i])return;
 if(stalker_timer)stalker_timer--;
 if(stalker_state==STALK) {
  // A trackable shimmer moves to alternating flanks; no invisible teleport.
  int16_t tx=(int16_t)player_x+(stalker_side?32:-32),ty=(int16_t)player_y-24;
  if(tx<16)tx=16;if(tx>128)tx=128;
  if(ty<24)ty=24;if(ty>104)ty=104;
  int8_t dx=tx>(int16_t)actor_x[i]?1:tx<(int16_t)actor_x[i]?-1:0;
  int8_t dy=ty>(int16_t)actor_y[i]?1:ty<(int16_t)actor_y[i]?-1:0;
  if(enemy_box_free(actor_x[i]+dx,actor_y[i]))actor_x[i]+=dx;
  if(enemy_box_free(actor_x[i],actor_y[i]+dy))actor_y[i]+=dy;
  if(!stalker_timer)lock_dash();
 } else if(stalker_state==WARN) {
  if(!stalker_timer){stalker_state=DASH;stalker_timer=16;stalker_dashes++;}
 } else if(stalker_state==DASH) {
  int16_t fx=(int16_t)actor_xs[i]+actor_vx[i],fy=(int16_t)actor_ys[i]+actor_vy[i];
  int16_t x=actor_x[i]+(fx>>8),y=actor_y[i]+(fy>>8);
  if(enemy_box_free(x,y)){actor_x[i]=x;actor_y[i]=y;actor_xs[i]=(uint8_t)fx;actor_ys[i]=(uint8_t)fy;}
  else recover();
  // Generic contact is disabled for this enemy: only a solid dash can hurt.
  int16_t dx=(int16_t)player_x-actor_x[i],dy=(int16_t)player_y-actor_y[i];
  if(stalker_state==DASH && player_hittable() && dx> -12 && dx<12 && dy> -12 && dy<12) {
   hurt_cause=18;player_hurt(16,actor_x[i]+8,actor_y[i]+8);
  }
  if(!stalker_timer)recover();
 } else if(!stalker_timer) {
  stalker_side^=1;stalker_state=STALK;stalker_timer=44;
 }
 pose();
}
