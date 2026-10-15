#pragma bank 255
#include "warden.h"
#include "boss.h"
#include "player.h"
#include "health.h"
#include "gfx.h"
#include "camera.h"
#include "room.h"
#include "actor_internal.h"
#include "enemy_family.h"
#include "state.h"

// A locked floor claim: outline -> electrical stripes -> clear floor.
// Eight reserved sprites keep the warning visible even with a full shot pool.
uint8_t warden_zone_x, warden_zone_y, warden_zone_state, warden_zone_timer;
uint8_t warden_floor, warden_hud_dirty, warden_swarm_timer, warden_husks;
uint16_t warden_waves;
static uint8_t route_y[ACTOR_MAX];
static uint8_t status_hp, status_state, status_floor, status_guard;
static uint8_t terrace(uint16_t y) { return y<64?0:y<144?1:2; }

void warden_room(void) BANKED {
    room_load_tiles();
    room_w=15; room_h=14; room_px_w=240; room_px_h=224;
    for(uint16_t i=0;i<sizeof(room_map);i++) room_map[i]=BLK_SOLID;
    for(uint8_t y=1;y<13;y++) for(uint8_t x=1;x<14;x++)
        room_map[((uint16_t)y<<5)|x]=y<9?BLK_DECK:BLK_FLOOR;
    for(uint8_t x=1;x<14;x++) {
        room_map[(4<<5)|x]=(x==2 || x==3)?BLK_STAIRS:BLK_SOLID;
        room_map[(9<<5)|x]=(x==11 || x==12)?BLK_STAIRS:BLK_SOLID;
    }
    // Step treads and a clean upper-deck panel. Physical risers block movement
    // and bullets; the stair openings connect the three top-down terraces.
    uint8_t tiles[48];
    for(uint8_t y=0;y<8;y++) {
        tiles[y*2]=y&1?0:255; tiles[y*2+1]=y&1?0:255;
        tiles[16+y*2]=y==0?255:128; tiles[17+y*2]=0;
        tiles[32+y*2]=255; tiles[33+y*2]=y==0?0:255;
    }
    set_bkg_data(SCENE_TILES+10,3,tiles);
    for(uint8_t q=0;q<4;q++) {
        room_tile_lut[BLK_STAIRS*4+q]=SCENE_TILES+10;
        room_tile_lut[BLK_DECK*4+q]=SCENE_TILES+11;
        room_tile_lut[BLK_SOLID*4+q]=SCENE_TILES+12;
    }
}

static const char husk_art[16][17]={
 "0000033333000000","0000332223300000","0000321212300000","0000332223300000",
 "0000033233000000","0033332223330000","0322222222233000","0323332233323000",
 "0330032230032300","0000032230003300","0000033330000000","0000032230000000",
 "0000032033000000","0000332003300000","0000323002300000","0000333003300000"};
static void load_husks(void) {
    uint8_t data[32];
    VBK_REG=1; // sprite bank 1, below the signed-background HUD tile range
    for(uint8_t pose=0;pose<2;pose++) for(uint8_t col=0;col<2;col++) {
        for(uint8_t y=0;y<16;y++) {
            uint8_t lo=0,hi=0,yy=pose && y>=12?27-y:y;
            for(uint8_t x=0;x<8;x++) {
                uint8_t c=husk_art[yy][col*8+x]-'0';
                lo=(lo<<1)|(c&1); hi=(hi<<1)|(c>>1);
            }
            data[y*2]=lo;data[y*2+1]=hi;
        }
        set_sprite_data(pose*4+col*2,2,data);
    }
    VBK_REG=0;
}

// Each actor commits to a stair landing before selecting its next waypoint.
// This prevents turning sideways while its feet still overlap the riser.
static void route(uint8_t i,uint8_t tx,uint8_t ty) {
    uint8_t x=actor_x[i],y=actor_y[i];
    if(route_y[i]) {
        if(y==route_y[i]) route_y[i]=0;
        else actor_y[i]=y+(y<route_y[i]?1:-1);
        return;
    }
    uint8_t from=terrace(y),to=terrace(ty);
    if(from!=to) {
        uint8_t lower=from>to?from:from+1;
        tx=lower==2?184:40;
        if(x==tx) { route_y[i]=from>to?(lower==2?112:32):(lower==2?176:96); return; }
    }
    if(x!=tx) actor_x[i]=x+(x<tx?1:-1);
    else if(y!=ty) actor_y[i]=y+(y<ty?1:-1);
}
uint8_t warden_chase(void) BANKED {
    uint8_t y=warden_floor==1?112:32;
    route(boss_slot,104,y);
    return actor_x[boss_slot]==104 && actor_y[boss_slot]==y;
}
uint8_t warden_near(void) BANKED {
    int16_t dx=(int16_t)player_x-actor_x[boss_slot];
    return terrace(player_y)==terrace(actor_y[boss_slot]) && dx>-96 && dx<96;
}
static void swarm(void) {
    // Distribute swarm animation and navigation across four frames.
    for(uint8_t i=(uint8_t)frame_count&3;i<ACTOR_MAX;i+=4) {
        if(!actor_type[i] || actor_scripted[i]!=FAMILY_HUSK) continue;
        actor_t0[i]=((uint8_t)frame_count&16)?4:0;actor_t1[i]=actor_t0[i]+2;
        actor_p0[i]=actor_p1[i]=SPR_PAL_SLIME|8;
        if(actor_timer[i]) { actor_timer[i]=actor_timer[i]>4?actor_timer[i]-4:0; actor_flash[i]=(actor_timer[i]&4)!=0; }
        else {
            route(i,player_x,player_y);
            actor_flash[i]=0;
        }
    }
    if(warden_swarm_timer) { warden_swarm_timer--; return; }
    warden_husks=0;
    for(uint8_t i=0;i<ACTOR_MAX;i++)
        if(actor_type[i] && actor_hp[i] && actor_scripted[i]==FAMILY_HUSK)warden_husks++;
    warden_swarm_timer=boss_phase?150:210;
    uint8_t cap=boss_phase?4:3;
    uint8_t count=0;
    for(uint8_t n=0;n<2 && warden_husks<cap;n++) {
        uint8_t y=terrace(player_y)==2?184:terrace(player_y)==1?104:24;
        uint8_t x=player_x<112?208:24;
        uint8_t i=actor_spawn(ACTOR_PEASHOOTER,x,y+n*12);
        if(i==0xFF) break;
        actor_scripted[i]=FAMILY_HUSK; actor_hp[i]=2; actor_timer[i]=36;
        actor_t0[i]=0;actor_t1[i]=2;actor_p0[i]=actor_p1[i]=SPR_PAL_SLIME|8;
        route_y[i]=0;warden_husks++;count++;
    }
    if(count) warden_waves++;
}
void warden_status(void) BANKED {
    uint8_t guard=actor_guard[boss_slot];
    if(!warden_hud_dirty && status_hp==boss_hp && status_state==boss_state &&
       status_floor==warden_floor && status_guard==guard) return;
    status_hp=boss_hp;status_state=boss_state;status_floor=warden_floor;status_guard=guard;
    warden_hud_dirty=0;
    static uint8_t row[20];
    uint8_t tens=0,ones=boss_hp;while(ones>=10){ones-=10;tens++;}
    row[0]=FONT_TILES+'W'-' ';row[1]=FONT_TILES+'0'-' '+tens;
    row[2]=FONT_TILES+'0'-' '+ones;row[3]=FONT_TILES;
    const char *text=boss_state==BOSS_WON?"CLEAR!":boss_state==BOSS_RELAYS?"BREAK RELAYS":
        boss_state==BOSS_RETREAT?"CHASE UPSTAIRS":boss_state==BOSS_WARN?((boss_cycles&1)?"SHOTGUN - DODGE":"RINGS - DODGE"):
        guard?"FIND WARDEN":"CORE OPEN - FIRE";
    uint8_t n=4;
    for(;n<20 && *text;n++)row[n]=FONT_TILES+*text++-' ';
    for(;n<20;n++)row[n]=FONT_TILES;
    gfx_vram_copy((uint8_t*)0x9C40,row,20,0);
}
void warden_start(void) BANKED {
    load_husks();
    warden_floor=0;warden_waves=0;warden_husks=0;warden_hud_dirty=1;
    warden_swarm_timer=180;
    for(uint8_t i=0;i<ACTOR_MAX;i++)route_y[i]=0;
    uint8_t data[32];
    for (uint8_t t=0;t<8;t++) {
        for (uint8_t y=0;y<16;y++) {
            uint8_t bits=0;
            for (uint8_t x=0;x<8;x++) {
                uint8_t edge=y==0 || (t>=4 && ((x+y)&7)==0) ||
                    ((t&3)==0 && x==0) || ((t&3)==3 && x==7);
                bits=(bits<<1) | edge;
            }
            data[y*2]=bits; data[y*2+1]=bits;
        }
        set_sprite_data(112+t*2,2,data);
    }
    warden_zone_state=0; warden_zone_timer=90;
}
void warden_update(void) BANKED {
    swarm();
    if (warden_zone_timer) warden_zone_timer--;
    if (!warden_zone_timer) {
        if (!warden_zone_state) {
            // Center the claim on the current position, then never track it.
            warden_zone_x=player_x<24?16:player_x>200?192:player_x-8;
            warden_zone_y=player_y<24?16:player_y>184?176:player_y-8;
            warden_zone_state=1; warden_zone_timer=boss_phase?48:60;
        } else if (warden_zone_state==1) {
            warden_zone_state=2; warden_zone_timer=boss_phase?48:42;
            boss_attacks++;
        } else { warden_zone_state=0; warden_zone_timer=90; }
    }
    if (warden_zone_state==2 && player_hittable() &&
        player_x+PLAYER_HB_X < (uint16_t)warden_zone_x+32 &&
        player_x+PLAYER_HB_X+PLAYER_HB_W > warden_zone_x &&
        player_y+PLAYER_HB_Y < (uint16_t)warden_zone_y+32 &&
        player_y+PLAYER_HB_Y+PLAYER_HB_H > warden_zone_y) {
        hurt_cause=10;
        player_hurt(12,warden_zone_x+16,warden_zone_y+16);
    }
    cam_shake=0; // keep scrolling stair geometry stable during crowded combat
}
void warden_draw(void) BANKED {
    for (uint8_t i=15;i<23;i++) shadow_OAM[i].y=0;
    if (boss_state==BOSS_WON || !warden_zone_state) return;
    for (uint8_t t=0;t<8;t++) {
        uint8_t i=15+t;
        int16_t x=(int16_t)warden_zone_x+(t&3)*8-cam_x;
        int16_t y=(int16_t)warden_zone_y+(t>>2)*16-cam_y;
        if(x<=-8 || x>=160 || y<=-16 || y>=120) continue;
        shadow_OAM[i].x=x+8;shadow_OAM[i].y=y+16;
        shadow_OAM[i].tile=(warden_zone_state==2?120:112)+(t&3)*2;
        shadow_OAM[i].prop=(warden_zone_state==2?SPR_PAL_BULLET:SPR_PAL_UI) | (t>=4?S_FLIPY:0);
    }
}
