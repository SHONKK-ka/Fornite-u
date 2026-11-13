// Fortnite U - logica del juego (mundo, fisica, armas, construccion, bots, tormenta, bus)
#include "game.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PI_F 3.14159265f
#define PR_R 0.4f
#define PH_H 1.8f
#define BUS_SPEED 22.0f
#define BUS_ALT 110.0f

Game G;
Player P[NP];
Prop props[MAXPROP];
int nProps;
Solid solids[MAXSOLID];
int nSolids, nStatic;
Piece pcs[MAXPIECE];
Loot loot[MAXLOOT];
Chest chests[MAXCHEST];
int nChests;
Tracer tracers[MAXTRACER];

const WDef WD[W_COUNT] = {
    {"NADA", 0, 0, 0, 0, 0, 0, 0, 0},
    {"PICO", 0, 20, 1.6f, 0, 0, 1, 0, 3.4f},
    {"PISTOLA", 0, 24, 3.6f, 15, 1.4f, 1, 0.012f, 100},
    {"FUSIL", 0, 19, 7.0f, 30, 2.2f, 1, 0.022f, 120},
    {"ESCOPETA", 1, 11, 0.9f, 6, 2.6f, 8, 0.075f, 38},
    {"FRANCOTIRADOR", 2, 95, 0.65f, 1, 2.8f, 1, 0.002f, 220},
};
const float RAR_MUL[5] = {1.0f, 1.08f, 1.17f, 1.28f, 1.4f};
const uint32_t RAR_COL[5] = {RGB(170, 170, 175), RGB(90, 210, 90), RGB(70, 150, 255), RGB(185, 90, 240), RGB(255, 195, 50)};
const char *RAR_NAME[5] = {"COMUN", "POCO COMUN", "RARO", "EPICO", "LEGENDARIO"};
const uint32_t SKIN_COL[8][2] = {
    {RGB(60, 120, 255), RGB(255, 220, 60)}, {RGB(230, 60, 60), RGB(40, 40, 50)},
    {RGB(70, 200, 90), RGB(250, 250, 250)}, {RGB(170, 80, 230), RGB(250, 200, 60)},
    {RGB(255, 150, 40), RGB(60, 60, 70)},   {RGB(240, 90, 170), RGB(255, 255, 255)},
    {RGB(50, 50, 60), RGB(230, 60, 60)},    {RGB(240, 240, 240), RGB(70, 120, 255)}};
const char *CITY_NAME[4] = {"PUEBLO PINO", "CIUDAD TOMATE", "TORRES TORCIDAS", "VILLA SALCHICHA"};
const float CITY[4][2] = {{50, 60}, {175, 55}, {60, 178}, {172, 172}};
const uint32_t GROUND_PAL[6] = {RGB(84, 168, 74), RGB(74, 154, 66), RGB(112, 114, 120), RGB(58, 120, 205),
                                RGB(224, 206, 150), RGB(48, 110, 60)};
const float ZONE_DMG[6] = {1, 2, 3, 5, 8, 10};
static const float ZONE_TARGET[6] = {105, 72, 46, 28, 14, 5};
static const float ZONE_WAIT[6] = {55, 30, 25, 20, 15, 10};
static const float ZONE_SHRINK[6] = {30, 24, 20, 15, 12, 10};

static float houseX[80], houseZ[80];
static int nHouses;
static int nextPiece;

// ---------------------------------------------------------------- utilidades

static float frand(float a, float b) { return a + (b - a) * ((float)rand() / (float)RAND_MAX); }
static float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
static float angdiff(float a, float b) {
    float d = b - a;
    while (d > PI_F) d -= 2 * PI_F;
    while (d < -PI_F) d += 2 * PI_F;
    return d;
}
static V3 v3norm(V3 a) {
    float l = sqrtf(v3dot(a, a));
    return l > 1e-6f ? v3mul(a, 1.0f / l) : v3(0, 1, 0);
}

void set_msg(const char *m) {
    snprintf(G.msg, sizeof(G.msg), "%s", m);
    G.msgT = 2.2f;
}
static void push_feed(const char *m) {
    for (int i = 0; i < 2; i++) {
        memcpy(G.feed[i], G.feed[i + 1], sizeof(G.feed[0]));
        G.feedT[i] = G.feedT[i + 1];
    }
    snprintf(G.feed[2], sizeof(G.feed[2]), "%s", m);
    G.feedT[2] = 5.0f;
}
static const char *pname(int i) {
    static char buf[4][16];
    static int k;
    k = (k + 1) & 3;
    if (i < 0) snprintf(buf[k], 16, "TORMENTA");
    else if (i == 0) snprintf(buf[k], 16, "TU");
    else snprintf(buf[k], 16, "BOT %d", i);
    return buf[k];
}

// ---------------------------------------------------------------- terreno

int ground_idx(float wx, float wz) {
    if (wx < 0 || wz < 0 || wx > MAPSZ || wz > MAPSZ) return 3;
    if (wx < 5 || wz < 5 || wx > MAPSZ - 5 || wz > MAPSZ - 5) return 4;
    float dx = wx - 120, dz = wz - 120;
    if (dx * dx + dz * dz < 22 * 22) return 3;
    if (dx * dx + dz * dz < 25 * 25) return 4;
    for (int c = 0; c < 4; c++) {
        float rx = fabsf(wx - CITY[c][0]), rz = fabsf(wz - CITY[c][1]);
        if (rx < 30 && rz < 25) {
            if (fabsf(rz - 7.5f) < 1.5f || fabsf(rx - 8.5f) < 1.5f) return 2;
            if (rx < 26 && rz < 22) return 0;
        }
    }
    return (((int)floorf(wx / 6.0f)) + ((int)floorf(wz / 6.0f))) & 1;
}

// ---------------------------------------------------------------- mundo

static void add_solid(float x0, float y0, float z0, float x1, float y1, float z1, int piece, int mat) {
    if (nSolids >= MAXSOLID) return;
    Solid s = {x0, y0, z0, x1, y1, z1, piece, mat};
    solids[nSolids++] = s;
}
static void add_prop(int kind, float x, float y, float z, float a, float b, float c, uint32_t col) {
    if (nProps >= MAXPROP) return;
    Prop p = {kind, x, y, z, a, b, c, col};
    props[nProps++] = p;
}
static void wall_box(float cx, float cy, float cz, float hx, float hy, float hz, uint32_t col, int mat) {
    add_prop(PR_BOX, cx, cy, cz, hx, hy, hz, col);
    add_solid(cx - hx, cy - hy, cz - hz, cx + hx, cy + hy, cz + hz, -1, mat);
}

static void make_house(float cx, float cz, int doorSide) {
    static const uint32_t WALLC[5] = {RGB(235, 225, 200), RGB(200, 220, 235), RGB(235, 200, 200), RGB(205, 235, 205),
                                      RGB(240, 230, 170)};
    const float W = 10, D = 8, H = 3.2f, T = 0.3f;
    uint32_t wc = WALLC[rand() % 5];
    float bz = doorSide ? cz - D / 2 : cz + D / 2;
    float fz = doorSide ? cz + D / 2 : cz - D / 2;
    add_prop(PR_BOX, cx, 0.05f, cz, W / 2, 0.05f, D / 2, RGB(150, 112, 72));
    wall_box(cx, H / 2, bz, W / 2, H / 2, T / 2, wc, 2);
    wall_box(cx - W / 2, H / 2, cz, T / 2, H / 2, D / 2, wc, 2);
    wall_box(cx + W / 2, H / 2, cz, T / 2, H / 2, D / 2, wc, 2);
    float gap = 1.3f, seg = (W / 2 - gap) / 2;
    wall_box(cx - gap - seg, H / 2, fz, seg, H / 2, T / 2, wc, 2);
    wall_box(cx + gap + seg, H / 2, fz, seg, H / 2, T / 2, wc, 2);
    wall_box(cx, H - 0.35f, fz, gap, 0.35f, T / 2, wc, 2);
    add_prop(PR_ROOF, cx, H, cz, W / 2 + 0.6f, 2.4f, D / 2 + 0.6f, RGB(190, 72, 60));
    if (rand() % 100 < 40) wall_box(cx, 0.45f, cz, 0.7f, 0.45f, 0.45f, RGB(130, 90, 55), -1);
    if (nHouses < 80) {
        houseX[nHouses] = cx;
        houseZ[nHouses] = cz;
        nHouses++;
    }
}

static void world_gen(void) {
    nProps = nSolids = nHouses = 0;
    for (int c = 0; c < 4; c++)
        for (int i = -1; i <= 1; i++)
            for (int j = -1; j <= 1; j++) {
                int door = j < 0 ? 1 : (j > 0 ? 0 : rand() & 1);
                make_house(CITY[c][0] + i * 17.0f, CITY[c][1] + j * 15.0f, door);
            }
    // arboles
    for (int n = 0, tries = 0; n < 230 && tries < 2000; tries++) {
        float x = frand(8, MAPSZ - 8), z = frand(8, MAPSZ - 8);
        int bad = 0;
        for (int c = 0; c < 4; c++)
            if (fabsf(x - CITY[c][0]) < 34 && fabsf(z - CITY[c][1]) < 29) bad = 1;
        float dx = x - 120, dz = z - 120;
        if (dx * dx + dz * dz < 28 * 28) bad = 1;
        if (bad) continue;
        float h = frand(4.5f, 7.0f), r = frand(1.5f, 2.3f);
        uint32_t leaf = rand() & 1 ? RGB(40, 125, 55) : RGB(52, 140, 62);
        add_prop(PR_TREE, x, 0, z, r, h, 0, leaf);
        add_solid(x - 0.3f, 0, z - 0.3f, x + 0.3f, 3.0f, z + 0.3f, -1, 0);
        n++;
    }
    // rocas
    for (int n = 0, tries = 0; n < 45 && tries < 800; tries++) {
        float x = frand(10, MAPSZ - 10), z = frand(10, MAPSZ - 10);
        if (ground_idx(x, z) != 0 && ground_idx(x, z) != 1) continue;
        float r = frand(0.9f, 1.8f);
        add_prop(PR_ROCK, x, 0, z, r, r * 1.1f, 0, RGB(120, 124, 132));
        add_solid(x - r * 0.8f, 0, z - r * 0.8f, x + r * 0.8f, r * 0.9f, z + r * 0.8f, -1, 1);
        n++;
    }
    nStatic = nSolids;
}

// ---------------------------------------------------------------- loot y cofres

static int roll_rar(void) {
    int r = rand() % 100;
    return r < 45 ? 0 : r < 73 ? 1 : r < 89 ? 2 : r < 97 ? 3 : 4;
}
static int roll_wtype(void) {
    int r = rand() % 100;
    return r < 30 ? W_PISTOL : r < 60 ? W_AR : r < 85 ? W_SHOTGUN : W_SNIPER;
}
static void spawn_loot(float x, float y, float z, int kind, int wtype, int rar, int amt) {
    for (int i = 0; i < MAXLOOT; i++)
        if (!loot[i].on) {
            Loot l = {1, kind, wtype, rar, amt, x, y, z};
            loot[i] = l;
            return;
        }
}
static int ammo_amt(int t) { return t == 0 ? 30 : t == 1 ? 10 : 5; }
static void rand_loot(float x, float z) {
    int r = rand() % 100;
    if (r < 45) spawn_loot(x, 0, z, L_WEAPON, roll_wtype(), roll_rar(), 0);
    else if (r < 70) {
        int t = rand() % 3;
        spawn_loot(x, 0, z, L_AMMO, t, 0, ammo_amt(t));
    } else if (r < 85) spawn_loot(x, 0, z, L_MED, 0, 0, 1);
    else spawn_loot(x, 0, z, L_SHIELD, 0, 0, 1);
}
static void populate(void) {
    memset(loot, 0, sizeof(loot));
    nChests = 0;
    for (int h = 0; h < nHouses; h++) {
        float cx = houseX[h], cz = houseZ[h];
        if (rand() % 100 < 55 && nChests < MAXCHEST) {
            Chest c = {cx + (rand() & 1 ? 3.6f : -3.6f), 0, cz + (rand() & 1 ? 2.8f : -2.8f), 0};
            chests[nChests++] = c;
        }
        if (rand() % 100 < 65) rand_loot(cx + frand(-3, 3), cz + frand(-2, 2));
        if (rand() % 100 < 30) rand_loot(cx + frand(-3, 3), cz + frand(-2, 2));
    }
    for (int i = 0; i < 10 && nChests < MAXCHEST; i++) {
        float x = frand(15, MAPSZ - 15), z = frand(15, MAPSZ - 15);
        if (ground_idx(x, z) > 1) continue;
        Chest c = {x, 0, z, 0};
        chests[nChests++] = c;
    }
    for (int i = 0; i < 40; i++) {
        float x = frand(12, MAPSZ - 12), z = frand(12, MAPSZ - 12);
        if (ground_idx(x, z) > 1) continue;
        rand_loot(x, z);
    }
}
static void open_chest(int ci) {
    Chest *c = &chests[ci];
    c->open = 1;
    int wt = roll_wtype(), rar = roll_rar();
    if (rand() % 100 < 30 && rar < 4) rar++;
    spawn_loot(c->x + 0.8f, c->y, c->z, L_WEAPON, wt, rar, 0);
    int at = WD[wt].ammoType;
    spawn_loot(c->x - 0.8f, c->y, c->z, L_AMMO, at, 0, ammo_amt(at));
    if (rand() & 1) spawn_loot(c->x, c->y, c->z + 0.9f, L_SHIELD, 0, 0, 1);
    else spawn_loot(c->x, c->y, c->z + 0.9f, L_MED, 0, 0, 1);
}

// ---------------------------------------------------------------- piezas de construccion

const char *piece_name(int t) {
    static const char *n[PC_COUNT] = {"PARED", "SUELO", "RAMPA", "TECHO"};
    return n[t];
}
static void rot_local(int rot, float lx, float lz, float *wx, float *wz) {
    switch (rot & 3) {
    case 0: *wx = lx; *wz = lz; break;
    case 1: *wx = lz; *wz = -lx; break;
    case 2: *wx = -lx; *wz = -lz; break;
    default: *wx = -lz; *wz = lx; break;
    }
}
static void piece_aabb(const Piece *pc, float lx0, float lz0, float lx1, float lz1, float y0, float y1, float o[6]) {
    float cx = pc->gx * 4.0f, cz = pc->gz * 4.0f, ax, az, bx, bz;
    rot_local(pc->rot, lx0, lz0, &ax, &az);
    rot_local(pc->rot, lx1, lz1, &bx, &bz);
    o[0] = cx + fminf(ax, bx); o[1] = y0; o[2] = cz + fminf(az, bz);
    o[3] = cx + fmaxf(ax, bx); o[4] = y1; o[5] = cz + fmaxf(az, bz);
}
static void piece_solids(const Piece *pc, int idx) {
    float y0 = pc->lvl * 3.0f, o[6];
    switch (pc->type) {
    case PC_WALL: piece_aabb(pc, -2, 1.9f, 2, 2.1f, y0, y0 + 3, o); add_solid(o[0], o[1], o[2], o[3], o[4], o[5], idx, -1); break;
    case PC_FLOOR: piece_aabb(pc, -2, -2, 2, 2, y0, y0 + 0.2f, o); add_solid(o[0], o[1], o[2], o[3], o[4], o[5], idx, -1); break;
    case PC_ROOF: piece_aabb(pc, -2, -2, 2, 2, y0 + 2.8f, y0 + 3.0f, o); add_solid(o[0], o[1], o[2], o[3], o[4], o[5], idx, -1); break;
    default:
        for (int i = 0; i < 6; i++) {
            piece_aabb(pc, -2, -2 + i * (4.0f / 6.0f), 2, 2, y0, y0 + (i + 1) * 0.5f, o);
            add_solid(o[0], o[1], o[2], o[3], o[4], o[5], idx, -1);
        }
    }
}
static void solids_rebuild(void) {
    nSolids = nStatic;
    for (int i = 0; i < MAXPIECE; i++)
        if (pcs[i].on) piece_solids(&pcs[i], i);
}
static uint32_t mat_col(int m) {
    return m == 0 ? RGB(168, 120, 66) : m == 1 ? RGB(184, 86, 70) : RGB(142, 154, 170);
}
void piece_visual(const Piece *pc, int ghost) {
    uint32_t col = ghost ? RGB(90, 170, 255) : mat_col(pc->mat);
    float y0 = pc->lvl * 3.0f, o[6];
    r_stipple(ghost);
    if (pc->type == PC_RAMP) {
        float cx = pc->gx * 4.0f, cz = pc->gz * 4.0f, ax, az, bx, bz;
        rot_local(pc->rot, -2, -2, &ax, &az);
        rot_local(pc->rot, 2, -2, &bx, &bz);
        V3 l0 = v3(cx + ax, y0, cz + az), r0 = v3(cx + bx, y0, cz + bz);
        rot_local(pc->rot, 2, 2, &ax, &az);
        rot_local(pc->rot, -2, 2, &bx, &bz);
        V3 r1 = v3(cx + ax, y0 + 3, cz + az), l1 = v3(cx + bx, y0 + 3, cz + bz);
        float nx, nz;
        rot_local(pc->rot, 0, -0.6f, &nx, &nz);
        r_quad(l0, r0, r1, l1, v3(nx, 0.8f, nz), col);
        V3 l0b = v3(l0.x, y0 + 3, l0.z), r0b = v3(r0.x, y0 + 3, r0.z);
        (void)l0b; (void)r0b;
        V3 l1d = v3(l1.x, y0, l1.z), r1d = v3(r1.x, y0, r1.z);
        rot_local(pc->rot, 1, 0, &nx, &nz);
        r_tri(l0, l1d, l1, v3(-nx, 0, -nz), mixcol(col, 0, 0.25f));
        r_tri(r0, r1d, r1, v3(nx, 0, nz), mixcol(col, 0, 0.25f));
        rot_local(pc->rot, 0, 1, &nx, &nz);
        r_quad(l1d, r1d, r1, l1, v3(nx, 0, nz), mixcol(col, 0, 0.15f));
    } else {
        if (pc->type == PC_WALL) piece_aabb(pc, -2, 1.9f, 2, 2.1f, y0, y0 + 3, o);
        else if (pc->type == PC_FLOOR) piece_aabb(pc, -2, -2, 2, 2, y0, y0 + 0.2f, o);
        else piece_aabb(pc, -2, -2, 2, 2, y0 + 2.8f, y0 + 3.0f, o);
        r_box(v3((o[0] + o[3]) * 0.5f, (o[1] + o[4]) * 0.5f, (o[2] + o[5]) * 0.5f),
              v3((o[3] - o[0]) * 0.5f, (o[4] - o[1]) * 0.5f, (o[5] - o[2]) * 0.5f), col);
        if (pc->type == PC_ROOF)
            r_pyramid(v3((o[0] + o[3]) * 0.5f, y0 + 3.0f, (o[2] + o[5]) * 0.5f), 2.1f, 2.1f, 1.3f,
                      mixcol(col, RGB(190, 72, 60), 0.5f));
    }
    r_stipple(0);
}
int piece_target(const Player *p, Piece *out) {
    float fx = sinf(G.camYaw), fz = cosf(G.camYaw);
    int rot, dx = 0, dz = 0;
    if (fabsf(fz) >= fabsf(fx)) { rot = fz > 0 ? 0 : 2; dz = fz > 0 ? 1 : -1; }
    else { rot = fx > 0 ? 1 : 3; dx = fx > 0 ? 1 : -1; }
    int gx = (int)floorf(p->pos.x / 4.0f + 0.5f), gz = (int)floorf(p->pos.z / 4.0f + 0.5f);
    out->on = 1;
    out->type = G.selPiece;
    out->rot = rot;
    out->mat = G.selMat;
    out->lvl = (int)floorf((p->pos.y + 0.5f) / 3.0f);
    out->gx = G.selPiece == PC_WALL ? gx : gx + dx;
    out->gz = G.selPiece == PC_WALL ? gz : gz + dz;
    out->hp = G.selMat == 0 ? 150.0f : G.selMat == 1 ? 250.0f : 350.0f;
    return 1;
}

// ---------------------------------------------------------------- rayos

static float ray_box(V3 o, V3 d, float x0, float y0, float z0, float x1, float y1, float z1) {
    float tmin = 0, tmax = 1e9f;
    float oo[3] = {o.x, o.y, o.z}, dd[3] = {d.x, d.y, d.z}, lo[3] = {x0, y0, z0}, hi[3] = {x1, y1, z1};
    for (int i = 0; i < 3; i++) {
        if (fabsf(dd[i]) < 1e-6f) {
            if (oo[i] < lo[i] || oo[i] > hi[i]) return 1e9f;
        } else {
            float inv = 1.0f / dd[i], t1 = (lo[i] - oo[i]) * inv, t2 = (hi[i] - oo[i]) * inv;
            if (t1 > t2) { float t = t1; t1 = t2; t2 = t; }
            if (t1 > tmin) tmin = t1;
            if (t2 < tmax) tmax = t2;
            if (tmin > tmax) return 1e9f;
        }
    }
    return tmin;
}
static float ray_solids(V3 o, V3 d, float maxT, int *which) {
    float best = 1e9f;
    int bi = -1;
    for (int i = 0; i < nSolids; i++) {
        Solid *s = &solids[i];
        float t = ray_box(o, d, s->x0, s->y0, s->z0, s->x1, s->y1, s->z1);
        if (t < best && t <= maxT) { best = t; bi = i; }
    }
    if (which) *which = bi;
    return bi >= 0 ? best : 1e9f;
}
static float ray_players(V3 o, V3 d, float maxT, int ignore, int *which, int *head) {
    float best = 1e9f;
    int bi = -1;
    for (int i = 0; i < NP; i++) {
        Player *p = &P[i];
        if (i == ignore || !p->alive || p->phase == PH_BUS) continue;
        float t = ray_box(o, d, p->pos.x - 0.4f, p->pos.y, p->pos.z - 0.4f, p->pos.x + 0.4f, p->pos.y + PH_H, p->pos.z + 0.4f);
        if (t < best && t <= maxT) { best = t; bi = i; }
    }
    if (which) *which = bi;
    if (head && bi >= 0) *head = (o.y + d.y * best) > P[bi].pos.y + 1.45f;
    return bi >= 0 ? best : 1e9f;
}

// ---------------------------------------------------------------- fisica

static int overlap(const Player *p, const Solid *s) {
    return p->pos.x + PR_R > s->x0 && p->pos.x - PR_R < s->x1 && p->pos.z + PR_R > s->z0 && p->pos.z - PR_R < s->z1 &&
           p->pos.y + PH_H > s->y0 && p->pos.y < s->y1;
}
static void phys_step(Player *p, float vx, float vz, float dt, float grav) {
    p->pos.x += vx * dt;
    for (int i = 0; i < nSolids; i++) {
        Solid *s = &solids[i];
        if (!overlap(p, s)) continue;
        if (s->y1 - p->pos.y <= 0.55f && s->y1 > p->pos.y + 0.001f) p->pos.y = s->y1;
        else if (vx > 0) p->pos.x = s->x0 - PR_R - 0.001f;
        else p->pos.x = s->x1 + PR_R + 0.001f;
    }
    p->pos.z += vz * dt;
    for (int i = 0; i < nSolids; i++) {
        Solid *s = &solids[i];
        if (!overlap(p, s)) continue;
        if (s->y1 - p->pos.y <= 0.55f && s->y1 > p->pos.y + 0.001f) p->pos.y = s->y1;
        else if (vz > 0) p->pos.z = s->z0 - PR_R - 0.001f;
        else p->pos.z = s->z1 + PR_R + 0.001f;
    }
    p->pos.x = clampf(p->pos.x, 1, MAPSZ - 1);
    p->pos.z = clampf(p->pos.z, 1, MAPSZ - 1);
    p->vel.y += grav * dt;
    p->pos.y += p->vel.y * dt;
    p->onGround = 0;
    for (int i = 0; i < nSolids; i++) {
        Solid *s = &solids[i];
        if (!overlap(p, s)) continue;
        if (p->vel.y <= 0) { p->pos.y = s->y1; p->vel.y = 0; p->onGround = 1; }
        else { p->pos.y = s->y0 - PH_H - 0.001f; p->vel.y = 0; }
    }
    if (p->pos.y <= 0) {
        p->pos.y = 0;
        if (p->vel.y < 0) p->vel.y = 0;
        p->onGround = 1;
    }
}
static void phys_move(Player *p, float vx, float vz, float dt, float grav) {
    float ox = p->pos.x, oz = p->pos.z;
    int n = 1 + (int)(fabsf(p->vel.y * dt) / 0.7f);
    if (n > 4) n = 4;
    for (int i = 0; i < n; i++) phys_step(p, vx, vz, dt / n, grav);
    p->spd = dt > 0 ? sqrtf((p->pos.x - ox) * (p->pos.x - ox) + (p->pos.z - oz) * (p->pos.z - oz)) / dt : 0;
}

// ---------------------------------------------------------------- combate

static int count_alive(void) {
    int n = 0;
    for (int i = 0; i < NP; i++)
        if (P[i].alive) n++;
    return n;
}
static void eliminate(int v, int att) {
    Player *p = &P[v];
    p->alive = 0;
    p->hp = 0;
    G.alive = count_alive();
    p->place = G.alive + 1;
    if (att >= 0 && att != v) P[att].kills++;
    for (int s = 1; s < 5; s++)
        if (p->inv[s].type != W_NONE) spawn_loot(p->pos.x + frand(-1, 1), p->pos.y, p->pos.z + frand(-1, 1), L_WEAPON, p->inv[s].type, p->inv[s].rar, 0);
    for (int t = 0; t < 3; t++)
        if (p->ammo[t] > 0) spawn_loot(p->pos.x + frand(-1, 1), p->pos.y, p->pos.z + frand(-1, 1), L_AMMO, t, 0, p->ammo[t] > 40 ? 40 : p->ammo[t]);
    char b[48];
    snprintf(b, sizeof(b), "%s ELIMINO A %s", pname(att), pname(v));
    push_feed(b);
    if (v == 0) {
        G.humanDead = 1;
        G.endT = 0;
        G.building = 0;
        G.spec = att > 0 ? att : 1;
        for (int i = 1; i < NP; i++)
            if (!P[G.spec].alive && P[i].alive) G.spec = i;
    }
    if (P[0].alive && G.alive <= 1) {
        G.victory = 1;
        G.endT = 0;
    }
}
static void hurt(int v, float dmg, int att) {
    Player *p = &P[v];
    if (!p->alive || p->phase == PH_BUS) return;
    if (v == 0) G.flash = 0.35f;
    float s = fminf(p->shield, dmg);
    p->shield -= s;
    dmg -= s;
    p->hp -= dmg;
    if (p->hp <= 0) eliminate(v, att);
}
static void damage_piece(int idx, float dmg) {
    Piece *pc = &pcs[idx];
    pc->hp -= dmg;
    if (pc->hp <= 0) {
        pc->on = 0;
        solids_rebuild();
    }
}
static void gather(int att, int mat) {
    Player *p = &P[att];
    p->mats[mat] = p->mats[mat] + 6 > 999 ? 999 : p->mats[mat] + 6;
    if (att == 0) {
        static const char *n[3] = {"+6 MADERA", "+6 LADRILLO", "+6 METAL"};
        set_msg(n[mat]);
    }
}
static void add_tracer(V3 a, V3 b) {
    for (int i = 0; i < MAXTRACER; i++)
        if (tracers[i].t <= 0) {
            tracers[i].a = a;
            tracers[i].b = b;
            tracers[i].t = 0.1f;
            return;
        }
}
static void hit_ray(int att, V3 o, V3 d, float range, float dmg, int melee) {
    int si = -1, pi = -1, head = 0;
    float ts = ray_solids(o, d, range, &si);
    float tp = ray_players(o, d, range, att, &pi, &head);
    float t = fminf(ts, tp);
    if (t > 1e8f) t = range;
    if (pi >= 0 && tp < ts) hurt(pi, dmg * (head && !melee ? 1.5f : 1.0f), att);
    else if (si >= 0 && ts < 1e8f) {
        Solid *s = &solids[si];
        if (s->piece >= 0) damage_piece(s->piece, dmg);
        else if (melee && s->mat >= 0 && G.buildAllowed) gather(att, s->mat);
    }
    if (!melee) add_tracer(v3add(o, v3mul(d, 1.2f)), v3add(o, v3mul(d, t)));
}
static void start_reload(int idx) {
    Player *p = &P[idx];
    Item *it = &p->inv[p->slot];
    if (it->type < W_PISTOL || p->reloadT > 0) return;
    const WDef *w = &WD[it->type];
    if (it->mag >= w->mag || p->ammo[w->ammoType] <= 0) return;
    p->reloadT = w->reload;
    p->reloadSlot = p->slot;
}
static void shoot(int idx, V3 aim) {
    Player *p = &P[idx];
    Item *it = &p->inv[p->slot];
    if (it->type == W_NONE || p->fireCd > 0 || p->reloadT > 0) return;
    const WDef *w = &WD[it->type];
    V3 o = v3(p->pos.x, p->pos.y + 1.45f, p->pos.z);
    V3 d = v3norm(v3sub(aim, o));
    if (it->type == W_PICK) {
        p->fireCd = 1.0f / w->rate;
        hit_ray(idx, o, d, w->range, w->dmg, 1);
        p->swing = 1.0f;
        return;
    }
    if (it->mag <= 0) {
        start_reload(idx);
        return;
    }
    it->mag--;
    p->fireCd = 1.0f / w->rate;
    float dmg = w->dmg * RAR_MUL[it->rar];
    for (int k = 0; k < w->pellets; k++) {
        V3 dd = v3norm(v3add(d, v3(frand(-w->spread, w->spread), frand(-w->spread, w->spread), frand(-w->spread, w->spread))));
        hit_ray(idx, o, dd, w->range, dmg, 0);
    }
}
void use_heal(int idx) {
    Player *p = &P[idx];
    if (!p->alive) return;
    if (p->shield < 100 && p->shields > 0) { p->shield = fminf(100, p->shield + 50); p->shields--; if (idx == 0) set_msg("+50 ESCUDO"); }
    else if (p->hp < 100 && p->meds > 0) { p->hp = fminf(100, p->hp + 30); p->meds--; if (idx == 0) set_msg("+30 VIDA"); }
}
void select_slot(int idx, int slot) {
    Player *p = &P[idx];
    if (slot < 0 || slot > 4 || p->inv[slot].type == W_NONE) return;
    if (p->slot != slot) { p->slot = slot; p->reloadT = 0; }
    if (idx == 0) G.building = 0;
}
static void cycle_slot(Player *p, int idx, int dir) {
    for (int k = 1; k <= 5; k++) {
        int s = (p->slot + dir * k + 10) % 5;
        if (p->inv[s].type != W_NONE) { select_slot(idx, s); return; }
    }
}
void play_emote(int idx, int e) {
    Player *p = &P[idx];
    if (!p->alive || p->phase != PH_GROUND || !p->onGround) return;
    p->emote = e;
    p->emoteT = 0;
    if (idx == 0) G.building = 0;
}

static void pickup_weapon(int idx, int li) {
    Player *p = &P[idx];
    Loot *l = &loot[li];
    int slot = -1;
    for (int s = 1; s < 5; s++)
        if (p->inv[s].type == W_NONE) { slot = s; break; }
    if (slot < 0) {
        slot = p->slot >= 1 ? p->slot : 1;
        Item old = p->inv[slot];
        spawn_loot(p->pos.x + 0.7f, p->pos.y, p->pos.z, L_WEAPON, old.type, old.rar, 0);
    }
    Item it = {l->wtype, l->rar, WD[l->wtype].mag};
    p->inv[slot] = it;
    p->ammo[WD[l->wtype].ammoType] += ammo_amt(WD[l->wtype].ammoType);
    l->on = 0;
    p->slot = slot;
    p->reloadT = 0;
    if (idx == 0) G.building = 0;
}
static void interact(int idx) {
    Player *p = &P[idx];
    int bc = -1;
    float bd = 2.8f * 2.8f;
    for (int i = 0; i < nChests; i++) {
        if (chests[i].open) continue;
        float dx = chests[i].x - p->pos.x, dz = chests[i].z - p->pos.z;
        if (dx * dx + dz * dz < bd && fabsf(chests[i].y - p->pos.y) < 2) { bd = dx * dx + dz * dz; bc = i; }
    }
    if (bc >= 0) { open_chest(bc); return; }
    int bl = -1;
    bd = 2.4f * 2.4f;
    for (int i = 0; i < MAXLOOT; i++) {
        if (!loot[i].on || loot[i].kind != L_WEAPON) continue;
        float dx = loot[i].x - p->pos.x, dz = loot[i].z - p->pos.z;
        if (dx * dx + dz * dz < bd && fabsf(loot[i].y - p->pos.y) < 2) { bd = dx * dx + dz * dz; bl = i; }
    }
    if (bl >= 0) pickup_weapon(idx, bl);
}
static void place_piece(int idx) {
    Player *p = &P[idx];
    if (!G.buildAllowed) return;
    Piece t;
    piece_target(p, &t);
    if (p->mats[t.mat] < 10) { set_msg("SIN MATERIALES"); return; }
    for (int i = 0; i < MAXPIECE; i++) {
        Piece *q = &pcs[i];
        if (q->on && q->type == t.type && q->gx == t.gx && q->gz == t.gz && q->lvl == t.lvl && (t.type != PC_WALL || q->rot == t.rot)) {
            set_msg("YA HAY UNA PIEZA");
            return;
        }
    }
    int k = -1;
    for (int i = 0; i < MAXPIECE; i++)
        if (!pcs[i].on) { k = i; break; }
    if (k < 0) { k = nextPiece; nextPiece = (nextPiece + 1) % MAXPIECE; }
    pcs[k] = t;
    solids_rebuild();
    for (int i = 0; i < NP; i++) {
        if (!P[i].alive || P[i].phase == PH_BUS) continue;
        for (int s = nStatic; s < nSolids; s++)
            if (solids[s].piece == k && overlap(&P[i], &solids[s])) {
                pcs[k].on = 0;
                solids_rebuild();
                set_msg("OCUPADO");
                return;
            }
    }
    p->mats[t.mat] -= 10;
}

// ---------------------------------------------------------------- zona y bus

static void zone_next_target(void) {
    G.zsx = G.zx; G.zsz = G.zz; G.zsr = G.zr;
    G.ztr = ZONE_TARGET[G.zPhase];
    float maxoff = G.zsr - G.ztr;
    if (maxoff < 0) maxoff = 0;
    float a = frand(0, 2 * PI_F), d = frand(0, maxoff * 0.8f);
    G.ztx = clampf(G.zx + cosf(a) * d, 20, MAPSZ - 20);
    G.ztz = clampf(G.zz + sinf(a) * d, 20, MAPSZ - 20);
    G.zTimer = ZONE_WAIT[G.zPhase];
    G.zShrink = 0;
}
static void zone_update(float dt) {
    if (G.zPhase < 6) {
        G.zTimer -= dt;
        if (!G.zShrink) {
            if (G.zTimer <= 0) { G.zShrink = 1; G.zTimer = ZONE_SHRINK[G.zPhase]; }
        } else {
            float t = 1.0f - G.zTimer / ZONE_SHRINK[G.zPhase];
            if (t > 1) t = 1;
            G.zx = G.zsx + (G.ztx - G.zsx) * t;
            G.zz = G.zsz + (G.ztz - G.zsz) * t;
            G.zr = G.zsr + (G.ztr - G.zsr) * t;
            if (G.zTimer <= 0) {
                G.zx = G.ztx; G.zz = G.ztz; G.zr = G.ztr;
                G.zPhase++;
                if (G.zPhase < 6) zone_next_target();
            }
        }
    }
    float dmg = ZONE_DMG[G.zPhase < 6 ? G.zPhase : 5];
    for (int i = 0; i < NP; i++) {
        Player *p = &P[i];
        if (!p->alive || p->phase == PH_BUS) continue;
        float dx = p->pos.x - G.zx, dz = p->pos.z - G.zz;
        if (dx * dx + dz * dz > G.zr * G.zr) hurt(i, dmg * dt, -1);
    }
}
static void drop_player(int i) {
    Player *p = &P[i];
    p->phase = PH_FALL;
    p->pos = v3(G.busPos.x + frand(-2, 2), BUS_ALT - 3, G.busPos.z + frand(-2, 2));
    p->vel = v3(0, -20, 0);
    p->onGround = 0;
    if (i > 0) {
        int cand[4], n = 0;
        for (int c = 0; c < 4; c++) {
            float dx = CITY[c][0] - p->pos.x, dz = CITY[c][1] - p->pos.z;
            if (dx * dx + dz * dz < 95 * 95) cand[n++] = c;
        }
        if (n > 0 && rand() % 100 < 75) {
            int c = cand[rand() % n];
            p->tx = CITY[c][0] + frand(-22, 22);
            p->tz = CITY[c][1] + frand(-18, 18);
        } else {
            float a = frand(0, 2 * PI_F), d = frand(25, 65);
            p->tx = clampf(p->pos.x + cosf(a) * d, 10, MAPSZ - 10);
            p->tz = clampf(p->pos.z + sinf(a) * d, 10, MAPSZ - 10);
        }
    }
}
static void bus_update(float dt) {
    (void)dt;
    float len = sqrtf((G.busB.x - G.busA.x) * (G.busB.x - G.busA.x) + (G.busB.z - G.busA.z) * (G.busB.z - G.busA.z));
    float t = G.matchT * BUS_SPEED / len;
    G.busActive = t < 1.0f;
    if (t > 1) t = 1;
    G.busPos = v3(G.busA.x + (G.busB.x - G.busA.x) * t, BUS_ALT, G.busA.z + (G.busB.z - G.busA.z) * t);
    G.busYaw = atan2f(G.busB.x - G.busA.x, G.busB.z - G.busA.z);
    if (!G.busActive)
        for (int i = 0; i < NP; i++)
            if (P[i].alive && P[i].phase == PH_BUS) drop_player(i);
}

// ---------------------------------------------------------------- partida

void lobby_start(void) {
    G.state = ST_LOBBY;
    G.lobbyT = 0;
    memset(&P[0], 0, sizeof(Player));
    P[0].alive = 1;
    P[0].phase = PH_GROUND;
    P[0].onGround = 1;
    P[0].pos = v3(120, 0, 12);
    P[0].inv[1].type = W_AR;
    P[0].inv[1].rar = 3;
    P[0].inv[1].mag = 30;
    P[0].slot = 1;
    G.humanDead = G.victory = 0;
    G.building = 0;
}

void match_start(void) {
    memset(P, 0, sizeof(P));
    memset(pcs, 0, sizeof(pcs));
    memset(tracers, 0, sizeof(tracers));
    solids_rebuild();
    populate();
    G.state = ST_MATCH;
    G.matchT = G.endT = G.flash = 0;
    G.humanDead = G.victory = 0;
    G.building = 0;
    G.ads = 0;
    G.selPiece = PC_WALL;
    G.selMat = 0;
    G.camPitch = -0.25f;
    G.msgT = 0;
    G.feedT[0] = G.feedT[1] = G.feedT[2] = 0;
    for (int i = 0; i < NP; i++) {
        Player *p = &P[i];
        p->alive = 1;
        p->bot = i != 0;
        p->hp = 100;
        p->phase = PH_BUS;
        p->inv[0].type = W_PICK;
        p->slot = 0;
        p->skin = i == 0 ? G.skin : rand() % 8;
        p->col = SKIN_COL[p->skin][0];
        p->col2 = SKIN_COL[p->skin][1];
        p->dropT = frand(1.5f, 16.0f);
        p->tx = p->tz = 120;
        p->mats[0] = G.buildAllowed ? 80 : 0;
        p->mats[1] = G.buildAllowed ? 30 : 0;
    }
    G.alive = NP;
    float a = frand(0, 2 * PI_F);
    V3 c = v3(120 + frand(-30, 30), 0, 120 + frand(-30, 30));
    G.busA = v3(c.x - cosf(a) * 190, BUS_ALT, c.z - sinf(a) * 190);
    G.busB = v3(c.x + cosf(a) * 190, BUS_ALT, c.z + sinf(a) * 190);
    G.busPos = G.busA;
    G.busActive = 1;
    G.zx = G.zz = MAPSZ / 2;
    G.zr = 190;
    G.zPhase = 0;
    zone_next_target();
    G.camYaw = atan2f(G.busB.x - G.busA.x, G.busB.z - G.busA.z);
    set_msg("PULSA A PARA SALTAR DEL BUS");
}

// ---------------------------------------------------------------- actualizacion comun

static void update_common(int i, float dt) {
    Player *p = &P[i];
    if (p->fireCd > 0) p->fireCd -= dt;
    if (p->swing > 0) p->swing -= dt * 4;
    if (p->reloadT > 0) {
        p->reloadT -= dt;
        if (p->reloadT <= 0) {
            Item *it = &p->inv[p->reloadSlot];
            if (it->type >= W_PISTOL) {
                const WDef *w = &WD[it->type];
                int need = w->mag - it->mag, have = p->ammo[w->ammoType];
                int take = need < have ? need : have;
                it->mag += take;
                p->ammo[w->ammoType] -= take;
            }
        }
    }
    p->walkPh += p->spd * dt * 1.4f;
    if (p->emote) {
        p->emoteT += dt;
        if (p->spd > 1.0f || !p->onGround) p->emote = 0;
    }
    if (p->phase != PH_GROUND) return;
    for (int k = 0; k < MAXLOOT; k++) {
        Loot *l = &loot[k];
        if (!l->on) continue;
        float dx = l->x - p->pos.x, dz = l->z - p->pos.z;
        if (dx * dx + dz * dz > 2.2f * 2.2f || fabsf(l->y - p->pos.y) > 2.0f) continue;
        if (l->kind == L_AMMO) { p->ammo[l->wtype] += l->amt; if (p->ammo[l->wtype] > 300) p->ammo[l->wtype] = 300; l->on = 0; }
        else if (l->kind == L_MED && p->meds < 5) { p->meds++; l->on = 0; }
        else if (l->kind == L_SHIELD && p->shields < 5) { p->shields++; l->on = 0; }
        else if (l->kind == L_WEAPON && i > 0) {
            int free = 0, worst = 99;
            for (int s = 1; s < 5; s++) {
                if (p->inv[s].type == W_NONE) free = 1;
                else if (p->inv[s].rar < worst) worst = p->inv[s].rar;
            }
            if (free || l->rar > worst) {
                if (!free) {
                    for (int s = 1; s < 5; s++)
                        if (p->inv[s].rar == worst) p->inv[s].type = W_NONE;
                }
                pickup_weapon(i, k);
            }
        }
    }
}

// ---------------------------------------------------------------- jugador humano

static void dz_stick(float *x, float *y) {
    float m = sqrtf(*x * *x + *y * *y);
    if (m < 0.16f) { *x = *y = 0; return; }
    float s = (m - 0.16f) / (0.84f * m);
    *x *= s;
    *y *= s;
}

static V3 aim_point(void) {
    float sy = sinf(G.camYaw), cy = cosf(G.camYaw), sp = sinf(G.camPitch), cp = cosf(G.camPitch);
    V3 f = v3(sy * cp, sp, cy * cp), o = G.camPos;
    int pi = -1;
    float t = fminf(ray_solids(o, f, 150, NULL), ray_players(o, f, 150, 0, &pi, NULL));
    if (t > 1e8f) t = 150;
    return v3add(o, v3mul(f, t));
}

static void update_human(const Pad *pad, float dt) {
    Player *p = &P[0];
    float sens = G.ads ? 0.55f : 1.0f;
    G.camYaw += pad->rx * 2.8f * dt * sens;
    G.camPitch = clampf(G.camPitch + pad->ry * 1.9f * dt * sens, -1.0f, 1.2f);
    if (p->phase == PH_BUS) {
        p->pos = G.busPos;
        G.ads = 0;
        if ((pad->pressed & BTN_A) && G.matchT > 1.0f) drop_player(0);
        return;
    }
    float sy = sinf(G.camYaw), cy = cosf(G.camYaw);
    float lx = pad->lx, ly = pad->ly, m = sqrtf(lx * lx + ly * ly);
    if (m > 1) { lx /= m; ly /= m; m = 1; }
    float wx = cy * lx + sy * ly, wz = -sy * lx + cy * ly;
    if (p->phase == PH_FALL || p->phase == PH_GLIDE) {
        if (p->phase == PH_FALL && ((pad->pressed & BTN_A) || p->pos.y < 48)) p->phase = PH_GLIDE;
        float spd = p->phase == PH_FALL ? 20.0f : 13.0f;
        p->vel.y = p->phase == PH_FALL ? -48.0f : -8.5f;
        phys_move(p, wx * spd, wz * spd, dt, 0);
        p->yaw = G.camYaw;
        G.ads = 0;
        if (p->onGround) { p->phase = PH_GROUND; p->vel.y = 0; set_msg("ATERRIZASTE"); }
        return;
    }
    int gun = p->inv[p->slot].type >= W_PISTOL;
    if ((pad->pressed & BTN_Y) && G.buildAllowed) { G.building ^= 1; p->emote = 0; }
    if (!G.buildAllowed) G.building = 0;
    if (pad->pressed & BTN_UP) play_emote(0, 1);
    if (pad->pressed & BTN_RIGHT) play_emote(0, 2);
    if (pad->pressed & BTN_DOWN) play_emote(0, 3);
    if (pad->pressed & BTN_LEFT) play_emote(0, 4);
    if (pad->pressed & BTN_R) { if (G.building) G.selPiece = (G.selPiece + 1) % PC_COUNT; else cycle_slot(p, 0, 1); }
    if (pad->pressed & BTN_L) { if (G.building) G.selPiece = (G.selPiece + PC_COUNT - 1) % PC_COUNT; else cycle_slot(p, 0, -1); }
    if (G.building && (pad->pressed & BTN_ZL)) G.selMat = (G.selMat + 1) % 3;
    G.ads = !G.building && gun && (pad->hold & BTN_ZL);
    if ((pad->pressed & BTN_X) && !G.building) start_reload(0);
    if (pad->pressed & BTN_R3) use_heal(0);
    if (pad->pressed & BTN_B) interact(0);
    float speed = 6.2f;
    if (pad->hold & BTN_L3) speed *= 1.4f;
    if (G.ads) speed *= 0.55f;
    if (G.building) speed *= 0.9f;
    if ((pad->pressed & BTN_A) && p->onGround) { p->vel.y = 8.4f; p->emote = 0; }
    phys_move(p, wx * speed, wz * speed, dt, -26.0f);
    int firing = (pad->hold & BTN_ZR) || G.ads;
    float target = p->yaw;
    if (firing || G.building) target = G.camYaw;
    else if (m > 0.1f) target = atan2f(wx, wz);
    if (!p->emote || m > 0.1f) p->yaw += angdiff(p->yaw, target) * clampf(dt * 14.0f, 0, 1);
    if (G.building) {
        if (pad->pressed & BTN_ZR) place_piece(0);
    } else if (pad->hold & BTN_ZR) {
        shoot(0, aim_point());
        p->emote = 0;
    }
}

// ---------------------------------------------------------------- bots

static int has_gun(Player *p) {
    for (int s = 1; s < 5; s++)
        if (p->inv[s].type >= W_PISTOL && (p->inv[s].mag > 0 || p->ammo[WD[p->inv[s].type].ammoType] > 0)) return 1;
    return 0;
}
static void bot_choose_weapon(int i, float dist) {
    Player *p = &P[i];
    int best = p->slot, bs = -1;
    float bsc = -1;
    for (int s = 1; s < 5; s++) {
        Item *it = &p->inv[s];
        if (it->type < W_PISTOL) continue;
        if (it->mag <= 0 && p->ammo[WD[it->type].ammoType] <= 0) continue;
        float sc = it->type == W_SHOTGUN ? (dist < 14 ? 3.0f : 0.3f) : it->type == W_AR ? 2.0f : it->type == W_PISTOL ? 1.0f : (dist > 30 ? 2.5f : 0.7f);
        sc *= 1.0f + it->rar * 0.12f;
        if (sc > bsc) { bsc = sc; bs = s; }
    }
    if (bs >= 0) best = bs;
    if (best != p->slot) select_slot(i, best);
}
static void bot_goal(int i) {
    Player *p = &P[i];
    int gun = has_gun(p);
    float bd = (gun ? 30.0f : 90.0f), bx = 0, bz = 0;
    int found = 0;
    bd *= bd;
    for (int c = 0; c < nChests; c++) {
        if (chests[c].open || gun) continue;
        float dx = chests[c].x - p->pos.x, dz = chests[c].z - p->pos.z, d = dx * dx + dz * dz;
        if (d < bd) { bd = d; bx = chests[c].x; bz = chests[c].z; found = 1; }
    }
    for (int k = 0; k < MAXLOOT; k++) {
        if (!loot[k].on || (gun && loot[k].kind == L_WEAPON && rand() % 3)) continue;
        if (!gun && loot[k].kind != L_WEAPON) continue;
        float dx = loot[k].x - p->pos.x, dz = loot[k].z - p->pos.z, d = dx * dx + dz * dz;
        if (d < bd) { bd = d; bx = loot[k].x; bz = loot[k].z; found = 1; }
    }
    if (found) { p->tx = bx; p->tz = bz; }
    else {
        float a = frand(0, 2 * PI_F), r = frand(0, G.zr * 0.7f);
        p->tx = clampf(G.zx + cosf(a) * r, 8, MAPSZ - 8);
        p->tz = clampf(G.zz + sinf(a) * r, 8, MAPSZ - 8);
    }
    p->retarget = frand(2.5f, 6.0f);
}

static void update_bot(int i, float dt) {
    Player *p = &P[i];
    if (p->phase == PH_BUS) {
        p->pos = G.busPos;
        p->dropT -= dt;
        if (p->dropT <= 0) drop_player(i);
        return;
    }
    if (p->phase == PH_FALL || p->phase == PH_GLIDE) {
        float dx = p->tx - p->pos.x, dz = p->tz - p->pos.z, d = sqrtf(dx * dx + dz * dz) + 0.001f;
        if (p->phase == PH_FALL && p->pos.y < 40 + (i % 5) * 6) p->phase = PH_GLIDE;
        float spd = (p->phase == PH_FALL ? 20.0f : 13.0f) * (d > 4 ? 1.0f : 0.25f);
        p->vel.y = p->phase == PH_FALL ? -48.0f : -8.5f;
        phys_move(p, dx / d * spd, dz / d * spd, dt, 0);
        p->yaw = atan2f(dx, dz);
        if (p->onGround) { p->phase = PH_GROUND; p->vel.y = 0; p->retarget = 0; }
        return;
    }
    p->aiT -= dt;
    p->retarget -= dt;
    if (p->react > 0) p->react -= dt;
    float rx = G.zx, rz = G.zz, rr = G.zr;
    if (G.zPhase < 6 && (G.zShrink || G.zTimer < 8)) { rx = G.ztx; rz = G.ztz; rr = G.ztr; }
    float zdx = rx - p->pos.x, zdz = rz - p->pos.z, zd = sqrtf(zdx * zdx + zdz * zdz);
    int outside = zd > rr - 6;
    if (p->aiT <= 0) {
        p->aiT = 0.3f + frand(0, 0.2f);
        int old = p->target;
        p->target = -1;
        float bd = 55.0f * 55.0f;
        V3 eye = v3(p->pos.x, p->pos.y + 1.5f, p->pos.z);
        for (int j = 0; j < NP; j++) {
            if (j == i || !P[j].alive || P[j].phase != PH_GROUND) continue;
            float dx = P[j].pos.x - p->pos.x, dz = P[j].pos.z - p->pos.z, d2 = dx * dx + dz * dz;
            if (d2 >= bd) continue;
            V3 tp = v3(P[j].pos.x, P[j].pos.y + 1.3f, P[j].pos.z);
            V3 dir = v3norm(v3sub(tp, eye));
            float dist = sqrtf(v3dot(v3sub(tp, eye), v3sub(tp, eye)));
            if (ray_solids(eye, dir, dist - 0.5f, NULL) > 1e8f) { bd = d2; p->target = j; }
        }
        if (p->target >= 0 && p->target != old) p->react = frand(0.3f, 0.7f);
        if (p->hp < 55 && p->meds > 0 && p->target < 0) use_heal(i);
        if (p->shield < 50 && p->shields > 0 && p->target < 0) use_heal(i);
    }
    int tgt = p->target;
    if (tgt >= 0 && (!P[tgt].alive || P[tgt].phase != PH_GROUND)) tgt = p->target = -1;
    float mvx = 0, mvz = 0, speed = 5.2f;
    if (outside) {
        mvx = zdx / (zd + 0.001f);
        mvz = zdz / (zd + 0.001f);
        speed = 7.5f;
        p->yaw += angdiff(p->yaw, atan2f(mvx, mvz)) * clampf(dt * 10, 0, 1);
    } else if (tgt >= 0 && has_gun(p)) {
        float dx = P[tgt].pos.x - p->pos.x, dz = P[tgt].pos.z - p->pos.z, d = sqrtf(dx * dx + dz * dz) + 0.001f;
        float ux = dx / d, uz = dz / d;
        bot_choose_weapon(i, d);
        if (p->retarget <= 0) { p->strafe = (rand() & 1) ? 1.0f : -1.0f; p->retarget = frand(0.8f, 2.0f); }
        float fwd = d > 24 ? 1.0f : d < 9 ? -1.0f : 0.0f;
        mvx = ux * fwd + -uz * p->strafe * 0.8f;
        mvz = uz * fwd + ux * p->strafe * 0.8f;
        speed = 5.5f;
        p->yaw += angdiff(p->yaw, atan2f(ux, uz)) * clampf(dt * 12, 0, 1);
        if (p->react <= 0) {
            float err = 0.3f + d * 0.03f;
            V3 aim = v3(P[tgt].pos.x + frand(-err, err), P[tgt].pos.y + 1.1f + frand(-err, err), P[tgt].pos.z + frand(-err, err));
            shoot(i, aim);
        }
    } else {
        if (p->retarget <= 0) bot_goal(i);
        float dx = p->tx - p->pos.x, dz = p->tz - p->pos.z, d = sqrtf(dx * dx + dz * dz);
        if (d < 1.5f) { p->retarget = 0; }
        else { mvx = dx / d; mvz = dz / d; p->yaw += angdiff(p->yaw, atan2f(mvx, mvz)) * clampf(dt * 8, 0, 1); }
    }
    // abrir cofres cercanos
    for (int c = 0; c < nChests; c++) {
        if (chests[c].open) continue;
        float dx = chests[c].x - p->pos.x, dz = chests[c].z - p->pos.z;
        if (dx * dx + dz * dz < 2.2f * 2.2f) { open_chest(c); break; }
    }
    float vm = sqrtf(mvx * mvx + mvz * mvz);
    if (vm > 1) { mvx /= vm; mvz /= vm; }
    phys_move(p, mvx * speed, mvz * speed, dt, -26.0f);
    if (vm > 0.3f && p->spd < 1.0f) {
        p->stuckT += dt;
        if (p->stuckT > 0.6f) {
            p->stuckT = 0;
            float a = frand(0, 2 * PI_F);
            p->tx = clampf(p->pos.x + cosf(a) * 14, 8, MAPSZ - 8);
            p->tz = clampf(p->pos.z + sinf(a) * 14, 8, MAPSZ - 8);
            p->retarget = 2.0f;
            if (p->onGround) p->vel.y = 8.0f;
        }
    } else p->stuckT = 0;
    if (p->inv[p->slot].type >= W_PISTOL && p->inv[p->slot].mag <= 0) start_reload(i);
    else if (tgt < 0 && p->inv[p->slot].type == W_PICK && has_gun(p)) bot_choose_weapon(i, 20);
}

// ---------------------------------------------------------------- camara

static void compute_camera(void) {
    Player *f = &P[G.humanDead ? G.spec : 0];
    float sy = sinf(G.camYaw), cy = cosf(G.camYaw), sp = sinf(G.camPitch), cp = cosf(G.camPitch);
    V3 fwd = v3(sy * cp, sp, cy * cp), right = v3(cy, 0, -sy);
    V3 tgt;
    float dist = G.ads ? 2.2f : 3.6f, side = G.ads ? 0.55f : 0.75f;
    if (f->phase == PH_BUS) { tgt = G.busPos; dist = 16; side = 0; }
    else {
        tgt = v3(f->pos.x, f->pos.y + 1.55f, f->pos.z);
        if (f->phase != PH_GROUND) dist = 5.5f;
    }
    V3 origin = v3add(tgt, v3mul(right, side));
    V3 back = v3mul(fwd, -1.0f);
    float len = dist;
    float t = ray_solids(origin, back, dist, NULL);
    if (t < len) len = fmaxf(0.5f, t - 0.3f);
    G.camPos = v3add(origin, v3mul(back, len));
    if (G.camPos.y < 0.35f) G.camPos.y = 0.35f;
    G.fov = G.ads ? 40.0f : 62.0f;
}

// ---------------------------------------------------------------- prompt

static void update_prompt(void) {
    G.prompt[0] = 0;
    if (G.humanDead || P[0].phase != PH_GROUND) return;
    Player *p = &P[0];
    for (int i = 0; i < nChests; i++) {
        if (chests[i].open) continue;
        float dx = chests[i].x - p->pos.x, dz = chests[i].z - p->pos.z;
        if (dx * dx + dz * dz < 2.8f * 2.8f) { snprintf(G.prompt, sizeof(G.prompt), "B: ABRIR COFRE"); return; }
    }
    for (int i = 0; i < MAXLOOT; i++) {
        if (!loot[i].on || loot[i].kind != L_WEAPON) continue;
        float dx = loot[i].x - p->pos.x, dz = loot[i].z - p->pos.z;
        if (dx * dx + dz * dz < 2.4f * 2.4f) {
            snprintf(G.prompt, sizeof(G.prompt), "B: %s %s", WD[loot[i].wtype].name, RAR_NAME[loot[i].rar]);
            return;
        }
    }
}

// ---------------------------------------------------------------- bucle

void game_init(void) {
    memset(&G, 0, sizeof(G));
    world_gen();
    G.buildAllowed = 1;
    G.skin = 0;
    lobby_start();
}

void game_update(const Pad *padIn, float dt) {
    Pad pad = *padIn;
    dz_stick(&pad.lx, &pad.ly);
    dz_stick(&pad.rx, &pad.ry);
    G.lobbyT += dt;
    if (G.msgT > 0) G.msgT -= dt;
    if (G.flash > 0) G.flash -= dt;
    for (int i = 0; i < 3; i++)
        if (G.feedT[i] > 0) G.feedT[i] -= dt;
    for (int i = 0; i < MAXTRACER; i++)
        if (tracers[i].t > 0) tracers[i].t -= dt;

    if (G.state == ST_LOBBY) {
        Player *p = &P[0];
        if (pad.pressed & BTN_A) { match_start(); return; }
        if (pad.pressed & (BTN_L | BTN_R)) G.buildAllowed ^= 1;
        if (pad.pressed & BTN_LEFT) G.skin = (G.skin + 7) % 8;
        if (pad.pressed & BTN_RIGHT) G.skin = (G.skin + 1) % 8;
        if (pad.pressed & BTN_UP) { G.lobbyEmote = G.lobbyEmote % 4 + 1; p->emote = G.lobbyEmote; p->emoteT = 0; }
        if (pad.pressed & BTN_DOWN) { G.lobbyEmote = G.lobbyEmote <= 1 ? 4 : G.lobbyEmote - 1; p->emote = G.lobbyEmote; p->emoteT = 0; }
        p->col = SKIN_COL[G.skin][0];
        p->col2 = SKIN_COL[G.skin][1];
        p->yaw = PI_F + sinf(G.lobbyT * 0.6f) * 0.5f;
        p->emoteT += dt;
        if (p->emote && p->emoteT > 6.0f) p->emote = 0;
        p->walkPh = 0;
        p->spd = 0;
        return;
    }

    G.matchT += dt;
    if (G.humanDead || G.victory) {
        G.endT += dt;
        if (G.endT > 1.5f && (pad.pressed & BTN_A)) { lobby_start(); return; }
        G.camYaw += pad.rx * 2.8f * dt;
        G.camPitch = clampf(G.camPitch + pad.ry * 1.9f * dt, -1.0f, 1.2f);
    }
    if (pad.pressed & BTN_PLUS) { lobby_start(); return; }
    bus_update(dt);
    zone_update(dt);
    if (P[0].alive && !G.victory) update_human(&pad, dt);
    else if (P[0].alive) { /* victoria: se queda quieto */ phys_move(&P[0], 0, 0, dt, -26.0f); }
    for (int i = 1; i < NP; i++)
        if (P[i].alive) update_bot(i, dt);
    for (int i = 0; i < NP; i++)
        if (P[i].alive) update_common(i, dt);
    if (G.humanDead && !P[G.spec].alive)
        for (int i = 1; i < NP; i++)
            if (P[i].alive) { G.spec = i; break; }
    if (P[0].alive && G.alive <= 1 && !G.victory) { G.victory = 1; G.endT = 0; }
    compute_camera();
    update_prompt();
}
