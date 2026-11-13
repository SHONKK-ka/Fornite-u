#ifndef GAME_H
#define GAME_H
#include "gfx.h"
#include "platform.h"

#define MAPSZ 240.0f
#define NP 20
#define MAXSOLID 2400
#define MAXPROP 1100
#define MAXPIECE 300
#define MAXLOOT 320
#define MAXCHEST 64
#define MAXTRACER 20

enum { ST_LOBBY, ST_MATCH };
enum { PH_BUS, PH_FALL, PH_GLIDE, PH_GROUND };
enum { W_NONE, W_PICK, W_PISTOL, W_AR, W_SHOTGUN, W_SNIPER, W_COUNT };
enum { PC_WALL, PC_FLOOR, PC_RAMP, PC_ROOF, PC_COUNT };
enum { PR_BOX, PR_TREE, PR_ROCK, PR_ROOF };
enum { L_WEAPON, L_AMMO, L_MED, L_SHIELD };

typedef struct { int type, rar, mag; } Item;
typedef struct {
    const char *name;
    int ammoType;
    float dmg, rate;
    int mag;
    float reload;
    int pellets;
    float spread, range;
} WDef;
typedef struct { float x0, y0, z0, x1, y1, z1; int piece, mat; } Solid;
typedef struct { int kind; float x, y, z, a, b, c; uint32_t col; } Prop;
typedef struct { int on, type, gx, gz, lvl, rot, mat; float hp; } Piece;
typedef struct { int on, kind, wtype, rar, amt; float x, y, z; } Loot;
typedef struct { float x, y, z; int open; } Chest;
typedef struct { V3 a, b; float t; } Tracer;

typedef struct {
    V3 pos, vel;
    float yaw, spd;
    float hp, shield;
    int alive, bot, phase, onGround, kills, place;
    Item inv[5]; // 0 pico, 1..4 armas
    int slot, reloadSlot;
    int ammo[3];
    int mats[3];
    int meds, shields;
    float fireCd, reloadT, emoteT, walkPh, swing;
    int emote;
    int skin;
    uint32_t col, col2;
    // IA
    float aiT, dropT, tx, tz, retarget, strafe, react, stuckT;
    int target;
} Player;

typedef struct {
    int state, buildAllowed, building, selPiece, selMat, humanDead, victory, alive, ads, spec, skin, lobbyEmote;
    float matchT, endT, camYaw, camPitch, fov, lobbyT, flash, msgT;
    V3 camPos;
    V3 busA, busB, busPos;
    float busYaw;
    int busActive;
    float zx, zz, zr, zsx, zsz, zsr, ztx, ztz, ztr, zTimer;
    int zPhase, zShrink;
    char msg[64];
    char feed[3][48];
    float feedT[3];
    char prompt[64];
} Game;

extern Game G;
extern Player P[NP];
extern Prop props[MAXPROP];
extern int nProps;
extern Solid solids[MAXSOLID];
extern int nSolids, nStatic;
extern Piece pcs[MAXPIECE];
extern Loot loot[MAXLOOT];
extern Chest chests[MAXCHEST];
extern int nChests;
extern Tracer tracers[MAXTRACER];
extern const WDef WD[W_COUNT];
extern const float RAR_MUL[5];
extern const uint32_t RAR_COL[5];
extern const char *RAR_NAME[5];
extern const uint32_t SKIN_COL[8][2];
extern const char *CITY_NAME[4];
extern const float CITY[4][2];
extern const uint32_t GROUND_PAL[6];
extern const float ZONE_DMG[6];

int ground_idx(float wx, float wz);
void game_init(void);
void game_update(const Pad *pad, float dt);
void game_render_tv(void);
void game_render_drc(Surface *s, const Pad *pad);

// acciones usadas por la interfaz del GamePad
void match_start(void);
void lobby_start(void);
void use_heal(int idx);
void play_emote(int idx, int e);
void select_slot(int idx, int slot);
void set_msg(const char *m);
int piece_target(const Player *p, Piece *out);
void piece_visual(const Piece *pc, int ghost);
const char *piece_name(int t);

#endif
