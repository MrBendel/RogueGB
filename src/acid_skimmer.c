#pragma bank 255
// Standalone flying insect trial. Targeted spit leaves two persistent pools.
#include "acid_skimmer.h"
#include "actor_internal.h"
#include "player.h"
#include "health.h"
#include "gfx.h"
#include "camera.h"
#include "state.h"

uint8_t acid_slot, acid_state, acid_timer, acid_target_x, acid_target_y;
uint8_t acid_pool_x[2], acid_pool_y[2], acid_pool_life[2], acid_next_pool;
uint8_t acid_hit_timer;
uint16_t acid_spits;
int16_t acid_glob_x, acid_glob_y, acid_glob_dx, acid_glob_dy;
int8_t acid_dir;
static const char bug[2][16][17]={
 {"0330000000000330","0323300000033230","0322233003322230","0032223333222300",
  "0003323113233000","0000332112330000","0000032222300000","0000032222300000",
  "0000332332330000","0033223333223300","0322203333022230","0333000330003330",
  "0000000330000000","0000000330000000","0000000000000000","0000000000000000"},
 {"0000000000000000","0000000000000000","0000003333000000","0000333113330000",
  "0000322112230000","0333322222233330","0322232222322230","0032222332222300",
  "0003322332233000","0000333333330000","0000030330300000","0000000330000000",
  "0000000330000000","0000000330000000","0000000000000000","0000000000000000"}};

static void load_art(void) {
    uint8_t data[32];
    // 0..3 wing poses, 4..5 warning, 6..9 bubbling pools, 10 glob, 11 shadow.
    for(uint8_t s=0;s<12;s++) {
        for(uint8_t y=0;y<16;y++) {
            uint8_t lo=0,hi=0;
            for(uint8_t x=0;x<8;x++) {
                uint8_t c=0,xx=(s&1)*8+x;
                if(s<4)c=bug[s>>1][y][xx]-'0';
                else if(s<6)c=(xx==0 || xx==15 || y==0 || y==15)?3:0;
                else if(s<10) {
                    if(y>=3 && y<=13 && xx>=1 && xx<=14) {
                        uint8_t edge=(y==3 || y==13)?5:(y==4 || y==12)?3:1;
                        if(xx>=edge && xx<=15-edge) {
                            c=(y==3 || y==13 || xx==edge || xx==15-edge)?3:2;
                            if(s<8) {
                                if((xx==5 || xx==6) && (y==6 || y==7))c=1;
                                if(xx==10 && y==10)c=1;
                            } else {
                                if((xx==9 || xx==10) && (y==7 || y==8))c=1;
                                if(xx==4 && y==10)c=1;
                            }
                        }
                    }
                } else if(s==10)c=(x>=2 && x<=5 && y>=5 && y<=10)?(x==2 || y==5?3:1):0;
                else c=(y>=4 && y<=5 && x>=1 && x<=6)?3:0;
                lo=(lo<<1)|(c&1);hi=(hi<<1)|(c>>1);
            }
            data[y*2]=lo;data[y*2+1]=hi;
        }
        set_sprite_data(96+s*2,2,data);
    }
}
void acid_skimmer_start(void) BANKED {
    load_art();acid_slot=actor_spawn(ACTOR_PEASHOOTER,72,32);
    actor_scripted[acid_slot]=8;actor_hp[acid_slot]=8;
    actor_t0[acid_slot]=96;actor_t1[acid_slot]=98;
    actor_p0[acid_slot]=actor_p1[acid_slot]=SPR_PAL_SLIME;
    acid_state=0;acid_timer=36;acid_dir=1;acid_spits=0;
    acid_pool_life[0]=acid_pool_life[1]=acid_next_pool=acid_hit_timer=0;
}
static uint8_t in_pool(uint8_t i) {
    return player_x+PLAYER_HB_X<(uint16_t)acid_pool_x[i]+15 &&
        player_x+PLAYER_HB_X+PLAYER_HB_W>acid_pool_x[i]+1 &&
        player_y+PLAYER_HB_Y<(uint16_t)acid_pool_y[i]+14 &&
        player_y+PLAYER_HB_Y+PLAYER_HB_H>acid_pool_y[i]+3;
}
void acid_skimmer_update(void) BANKED {
    if(!actor_type[acid_slot] || !actor_hp[acid_slot]) {
        acid_pool_life[0]=acid_pool_life[1]=0;acid_state=0;return;
    }
    if(acid_hit_timer)acid_hit_timer--;
    for(uint8_t i=0;i<2;i++)if(acid_pool_life[i]) {
        acid_pool_life[i]--;
        if(acid_pool_life[i] && !acid_hit_timer && player_hittable() && in_pool(i)) {
            hurt_cause=11;player_hurt(6,acid_pool_x[i]+8,acid_pool_y[i]+8);acid_hit_timer=30;
        }
    }
    uint8_t tile=((uint8_t)frame_count&4)?100:96;
    actor_t0[acid_slot]=tile;actor_t1[acid_slot]=tile+2;
    actor_y[acid_slot]=32+(((uint8_t)frame_count>>4)&1);
    actor_flash[acid_slot]=acid_state==1 && (acid_timer&4);
    if(acid_timer)acid_timer--;
    if(acid_state==0) {
        if(actor_x[acid_slot]>=120)acid_dir=-1;
        if(actor_x[acid_slot]<=24)acid_dir=1;
        if(!((uint8_t)frame_count&1))actor_x[acid_slot]+=acid_dir;
        if(!acid_timer) {
            acid_target_x=player_x<16?16:player_x>128?128:player_x;
            acid_target_y=player_y<16?16:player_y>96?96:player_y;
            acid_state=1;acid_timer=36;
        }
    } else if(acid_state==1 && !acid_timer) {
        acid_state=2;acid_timer=24;acid_spits++;
        acid_glob_x=(actor_x[acid_slot]+4)*16;acid_glob_y=(actor_y[acid_slot]+4)*16;
        acid_glob_dx=((int16_t)(acid_target_x+4)*16-acid_glob_x)/24;
        acid_glob_dy=((int16_t)(acid_target_y+4)*16-acid_glob_y)/24;
    } else if(acid_state==2) {
        acid_glob_x+=acid_glob_dx;acid_glob_y+=acid_glob_dy;
        if(!acid_timer) {
            acid_pool_x[acid_next_pool]=acid_target_x;acid_pool_y[acid_next_pool]=acid_target_y;
            acid_pool_life[acid_next_pool]=180;acid_next_pool^=1;
            acid_state=0;acid_timer=60;
        }
    }
}
static uint8_t draw_oam;
static void sprite(int16_t x,int16_t y,uint8_t tile,uint8_t pal) {
    uint8_t i=draw_oam++;x-=cam_x;y-=cam_y;
    if(i>=23 || x<=-8 || x>=160 || y<=-16 || y>=128)return;
    shadow_OAM[i].x=x+8;shadow_OAM[i].y=y+16;
    shadow_OAM[i].tile=tile;shadow_OAM[i].prop=pal;
}
void acid_skimmer_draw(void) BANKED {
    for(uint8_t i=15;i<23;i++)shadow_OAM[i].y=0;
    if(!actor_type[acid_slot] || !actor_hp[acid_slot])return;
    draw_oam=15;
    sprite(actor_x[acid_slot]+4,actor_y[acid_slot]+16,118,SPR_PAL_UI);
    for(uint8_t i=0;i<2;i++)if(acid_pool_life[i]) {
        uint8_t tile=((uint8_t)frame_count&8)?112:108;
        sprite(acid_pool_x[i],acid_pool_y[i],tile,SPR_PAL_SLIME);
        sprite(acid_pool_x[i]+8,acid_pool_y[i],tile+2,SPR_PAL_SLIME);
    }
    if(acid_state==1 || acid_state==2) {
        sprite(acid_target_x,acid_target_y,104,SPR_PAL_UI);
        sprite(acid_target_x+8,acid_target_y,106,SPR_PAL_UI);
    }
    if(acid_state==2) {
        uint8_t t=24-acid_timer,arc=t<12?t:24-t;
        sprite(acid_glob_x>>4,(acid_glob_y>>4)-arc,116,SPR_PAL_SLIME);
    }
}
