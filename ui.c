// Fortnite U - dibujo: la TV muestra la escena 3D en tercera persona,
// el GamePad muestra mapa, inventario, materiales, piezas de construccion y emotes.
#include "game.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PI_F 3.14159265f
#define FOG_END 105.0f

static const uint32_t SKY_TOP = RGB(70, 130, 220), SKY_HOR = RGB(185, 220, 245), FOG = RGB(185, 220, 245);
static const uint32_t SKIN_TONE = RGB(240, 200, 160);

static float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }

// ---------------------------------------------------------------- personajes

static V3 loc(V3 base, float yaw, float lx, float ly, float lz) {
    float s = sinf(yaw), c = cosf(yaw);
    return v3(base.x + lx * c + lz * s, base.y + ly, base.z - lx * s + lz * c);
}

static void draw_gun(V3 hand, float yaw, float pitch, int type, int rar) {
    uint32_t dark = RGB(45, 45, 52), acc = RAR_COL[rar];
    switch (type) {
    case W_PISTOL: r_box_xf(hand, yaw, pitch, v3(0, -0.75f, 0), v3(0.05f, 0.14f, 0.07f), dark); break;
    case W_AR:
        r_box_xf(hand, yaw, pitch, v3(0, -0.85f, 0), v3(0.05f, 0.34f, 0.07f), dark);
        r_box_xf(hand, yaw, pitch, v3(0, -0.75f, 0.0f), v3(0.06f, 0.06f, 0.09f), acc);
        break;
    case W_SHOTGUN:
        r_box_xf(hand, yaw, pitch, v3(0, -0.9f, 0), v3(0.06f, 0.38f, 0.07f), RGB(110, 80, 50));
        r_box_xf(hand, yaw, pitch, v3(0, -0.9f, 0.05f), v3(0.04f, 0.38f, 0.03f), acc);
        break;
    case W_SNIPER:
        r_box_xf(hand, yaw, pitch, v3(0, -1.0f, 0), v3(0.045f, 0.5f, 0.06f), dark);
        r_box_xf(hand, yaw, pitch, v3(0, -0.8f, 0.08f), v3(0.04f, 0.1f, 0.04f), acc);
        break;
    case W_PICK:
        r_box_xf(hand, yaw, pitch, v3(0, -0.5f, 0), v3(0.03f, 0.4f, 0.03f), RGB(120, 85, 50));
        r_box_xf(hand, yaw, pitch, v3(0, -0.88f, 0), v3(0.22f, 0.05f, 0.05f), RGB(170, 175, 185));
        break;
    }
}

static void draw_player(const Player *p, float tnow) {
    float yaw = p->yaw;
    V3 pos = p->pos;
    float legL = 0, legR = 0, armL = 0, armR = 0, bob = 0;
    float sw = sinf(p->walkPh) * 0.9f * (p->spd > 0.5f ? clampf(p->spd / 6.0f, 0, 1) : 0);
    legL = sw; legR = -sw; armL = -sw * 0.8f; armR = sw * 0.8f;
    int gun = p->inv[p->slot].type >= W_PISTOL;
    int hold = p->inv[p->slot].type != W_NONE && p->emote == 0;
    if (hold) {
        armR = gun ? -1.45f : -0.3f - sinf(tnow * 0.0f) * 0;
        if (gun) armL = -1.2f;
        if (p->inv[p->slot].type == W_PICK && p->swing > 0) armR = -1.2f - sinf(p->swing * 3.1f) * 1.2f;
    }
    float t = p->emoteT;
    float extraYaw = 0;
    if (p->emote == 1) { // baile
        bob = fabsf(sinf(t * 8)) * 0.12f;
        armL = -2.5f + sinf(t * 8) * 0.9f;
        armR = -2.5f - sinf(t * 8) * 0.9f;
        legL = sinf(t * 8) * 0.6f;
        legR = -sinf(t * 8) * 0.6f;
        extraYaw = sinf(t * 4) * 0.5f;
    } else if (p->emote == 2) { // saludo
        armR = -2.9f + sinf(t * 9) * 0.35f;
        armL = 0.05f;
    } else if (p->emote == 3) { // giro
        extraYaw = t * 7.0f;
        armL = armR = -1.57f;
        bob = fabsf(sinf(t * 3.5f)) * 0.25f;
    } else if (p->emote == 4) { // victoria
        bob = fabsf(sinf(t * 5)) * 0.35f;
        armL = armR = -3.0f + sinf(t * 10) * 0.2f;
        legL = sinf(t * 10) * 0.4f;
        legR = -legL;
    }
    if (p->phase == PH_FALL) { armL = armR = -2.3f; legL = legR = 0.5f; }
    if (p->phase == PH_GLIDE) { armL = armR = -3.0f; legL = legR = 0.1f; }
    yaw += extraYaw;
    pos.y += bob;
    uint32_t pant = RGB(45, 55, 85);
    V3 hipL = loc(pos, yaw, -0.13f, 0.85f, 0), hipR = loc(pos, yaw, 0.13f, 0.85f, 0);
    r_box_xf(hipL, yaw, legL, v3(0, -0.4f, 0), v3(0.1f, 0.42f, 0.11f), pant);
    r_box_xf(hipR, yaw, legR, v3(0, -0.4f, 0), v3(0.1f, 0.42f, 0.11f), pant);
    r_box(loc(pos, yaw, 0, 1.18f, 0), v3(0.0f, 0.0f, 0.0f), 0); // (sin efecto, ancla)
    r_box_xf(loc(pos, yaw, 0, 1.18f, 0), yaw, 0, v3(0, 0, 0), v3(0.26f, 0.36f, 0.15f), p->col);
    r_box_xf(loc(pos, yaw, 0, 1.2f, -0.2f), yaw, 0, v3(0, 0, 0), v3(0.17f, 0.24f, 0.08f), p->col2);
    r_box_xf(loc(pos, yaw, 0, 1.78f, 0), yaw, 0, v3(0, 0, 0), v3(0.19f, 0.2f, 0.19f), SKIN_TONE);
    r_box_xf(loc(pos, yaw, 0, 1.98f, 0), yaw, 0, v3(0, 0, 0), v3(0.21f, 0.07f, 0.21f), p->col2);
    r_box_xf(loc(pos, yaw, 0, 1.78f, 0.19f), yaw, 0, v3(0, 0, 0), v3(0.12f, 0.03f, 0.01f), RGB(40, 40, 50));
    V3 shL = loc(pos, yaw, -0.36f, 1.5f, 0), shR = loc(pos, yaw, 0.36f, 1.5f, 0);
    r_box_xf(shL, yaw, armL, v3(0, -0.3f, 0), v3(0.08f, 0.32f, 0.09f), p->col);
    r_box_xf(shR, yaw, armR, v3(0, -0.3f, 0), v3(0.08f, 0.32f, 0.09f), p->col);
    if (hold) draw_gun(shR, yaw, armR, p->inv[p->slot].type, p->inv[p->slot].rar);
    if (p->phase == PH_GLIDE) {
        r_pyramid(v3(pos.x, pos.y + 2.4f, pos.z), 1.9f, 1.9f, 0.7f, p->col2);
        r_box(v3(pos.x, pos.y + 2.38f, pos.z), v3(1.9f, 0.03f, 1.9f), mixcol(p->col2, 0, 0.3f));
    }
}

// ---------------------------------------------------------------- escena

static void draw_prop(const Prop *pr) {
    switch (pr->kind) {
    case PR_BOX: r_box(v3(pr->x, pr->y, pr->z), v3(pr->a, pr->b, pr->c), pr->col); break;
    case PR_TREE:
        r_box(v3(pr->x, pr->b * 0.22f, pr->z), v3(0.28f, pr->b * 0.22f, 0.28f), RGB(110, 78, 48));
        r_cone(v3(pr->x, pr->b * 0.28f, pr->z), pr->a, pr->b * 0.6f, 6, pr->col);
        r_cone(v3(pr->x, pr->b * 0.55f, pr->z), pr->a * 0.7f, pr->b * 0.45f, 6, mixcol(pr->col, RGB(120, 220, 120), 0.18f));
        break;
    case PR_ROCK: r_cone(v3(pr->x, 0, pr->z), pr->a, pr->b, 6, pr->col); break;
    case PR_ROOF: r_pyramid(v3(pr->x, pr->y, pr->z), pr->a, pr->c, pr->b, pr->col); break;
    }
}

static int visible(float x, float z, float margin) {
    V3 f = r_cam_forward();
    float dx = x - G.camPos.x, dz = z - G.camPos.z;
    float d2 = dx * dx + dz * dz;
    if (d2 > (FOG_END + 15) * (FOG_END + 15)) return 0;
    if (d2 < margin * margin) return 1;
    return dx * f.x + dz * f.z > -margin * 0.5f * sqrtf(f.x * f.x + f.z * f.z + 1e-6f);
}

static void draw_loot_item(const Loot *l, float t) {
    float bob = 0.35f + sinf(t * 2.5f + l->x) * 0.08f;
    V3 c = v3(l->x, l->y + bob, l->z);
    if (l->kind == L_WEAPON) {
        r_box_xf(c, t * 1.5f, 0, v3(0, 0, 0), v3(0.07f, 0.1f, 0.38f), RGB(50, 50, 58));
        r_box_xf(c, t * 1.5f, 0, v3(0, 0.12f, 0), v3(0.05f, 0.03f, 0.2f), RAR_COL[l->rar]);
        r_box(v3(l->x, l->y + 1.0f, l->z), v3(0.03f, 0.9f, 0.03f), RAR_COL[l->rar]);
    } else if (l->kind == L_AMMO) {
        r_box_xf(c, t * 1.5f, 0, v3(0, 0, 0), v3(0.16f, 0.1f, 0.12f), RGB(230, 150, 50));
    } else if (l->kind == L_MED) {
        r_box_xf(c, t * 1.5f, 0, v3(0, 0, 0), v3(0.17f, 0.12f, 0.12f), RGB(245, 245, 245));
        r_box_xf(c, t * 1.5f, 0, v3(0, 0, 0), v3(0.18f, 0.03f, 0.04f), RGB(220, 40, 50));
    } else {
        r_box_xf(c, t * 1.5f, 0, v3(0, 0, 0), v3(0.1f, 0.15f, 0.1f), RGB(70, 140, 255));
        r_box(v3(l->x, l->y + 1.0f, l->z), v3(0.03f, 0.9f, 0.03f), RGB(70, 140, 255));
    }
}

static void draw_world(V3 cam, float yaw, float pitch, float fov, float tnow, int lobby) {
    r_camera(cam, yaw, pitch, fov);
    r_sky_ground(SKY_TOP, SKY_HOR, FOG, FOG_END, GROUND_PAL, 6, ground_idx);
    for (int i = 0; i < nProps; i++)
        if (visible(props[i].x, props[i].z, 14)) draw_prop(&props[i]);
    if (lobby) {
        r_box(v3(120, 0.15f, 12), v3(1.7f, 0.15f, 1.7f), RGB(60, 70, 110));
        r_box(v3(120, 0.35f, 12), v3(1.45f, 0.05f, 1.45f), RGB(255, 210, 70));
        return;
    }
    for (int i = 0; i < MAXPIECE; i++)
        if (pcs[i].on && visible(pcs[i].gx * 4.0f, pcs[i].gz * 4.0f, 10)) piece_visual(&pcs[i], 0);
    for (int i = 0; i < nChests; i++) {
        if (!visible(chests[i].x, chests[i].z, 8)) continue;
        V3 c = v3(chests[i].x, chests[i].y, chests[i].z);
        uint32_t body = chests[i].open ? RGB(90, 70, 40) : RGB(235, 180, 40);
        r_box(v3(c.x, c.y + 0.3f, c.z), v3(0.5f, 0.3f, 0.35f), body);
        r_box_xf(v3(c.x, c.y + 0.6f, c.z - 0.35f), 0, chests[i].open ? -1.1f : 0, v3(0, 0.08f, 0.35f), v3(0.5f, 0.08f, 0.35f), mixcol(body, RGB(255, 240, 150), 0.25f));
        if (!chests[i].open) r_box(v3(c.x, c.y + 1.2f, c.z), v3(0.04f, 0.6f, 0.04f), RGB(255, 220, 80));
    }
    for (int i = 0; i < MAXLOOT; i++)
        if (loot[i].on && visible(loot[i].x, loot[i].z, 6)) draw_loot_item(&loot[i], tnow);
    for (int i = 0; i < NP; i++) {
        const Player *p = &P[i];
        if (!p->alive || p->phase == PH_BUS) continue;
        if (i == 0 && G.camPos.x == 0) continue;
        if (visible(p->pos.x, p->pos.z, 6)) draw_player(p, tnow);
    }
    // bus
    if (G.busActive) {
        V3 b = G.busPos;
        r_box_xf(b, G.busYaw, 0, v3(0, 0, 0), v3(2.0f, 1.6f, 5.0f), RGB(60, 120, 230));
        r_box_xf(b, G.busYaw, 0, v3(0, 1.2f, 0), v3(1.8f, 0.2f, 4.6f), RGB(250, 215, 90));
        r_box_xf(v3(b.x, b.y + 4.5f, b.z), G.busYaw, 0, v3(0, 0, 0), v3(2.6f, 2.6f, 2.6f), RGB(255, 110, 60));
        r_box_xf(b, G.busYaw, 0, v3(0, 2.5f, 0), v3(0.15f, 1.0f, 0.15f), RGB(90, 90, 100));
    }
    // tracers
    for (int i = 0; i < MAXTRACER; i++)
        if (tracers[i].t > 0) r_line3(tracers[i].a, tracers[i].b, RGB(255, 240, 150));
    // pared de la tormenta
    r_stipple(1);
    int segs = 56;
    for (int i = 0; i < segs; i++) {
        float a0 = 2 * PI_F * i / segs, a1 = 2 * PI_F * (i + 1) / segs;
        float x0 = G.zx + cosf(a0) * G.zr, z0 = G.zz + sinf(a0) * G.zr, x1 = G.zx + cosf(a1) * G.zr, z1 = G.zz + sinf(a1) * G.zr;
        float dx = x0 - G.camPos.x, dz = z0 - G.camPos.z;
        if (dx * dx + dz * dz > 120.0f * 120.0f) continue;
        V3 n = v3(-cosf((a0 + a1) / 2), 0, -sinf((a0 + a1) / 2));
        r_quad(v3(x0, 0, z0), v3(x1, 0, z1), v3(x1, 40, z1), v3(x0, 40, z0), n, RGB(150, 70, 220));
        r_quad(v3(x1, 0, z1), v3(x0, 0, z0), v3(x0, 40, z0), v3(x1, 40, z1), v3(-n.x, 0, -n.z), RGB(150, 70, 220));
    }
    r_stipple(0);
}

// ---------------------------------------------------------------- HUD de la TV

static void bar(Surface *s, int x, int y, int w, int h, float v, uint32_t col) {
    s_fill(s, x - 1, y - 1, w + 2, h + 2, RGB(0, 0, 0));
    s_fill(s, x, y, w, h, RGB(40, 40, 50));
    s_fill(s, x, y, (int)(w * clampf(v, 0, 1)), h, col);
}

static void tv_hud(Surface *s) {
    char b[80];
    const Player *me = &P[0];
    int cx = RW / 2, cy = RH / 2;
    // mira
    uint32_t cc = G.ads ? RGB(255, 80, 80) : RGB(255, 255, 255);
    if (me->phase == PH_GROUND && !G.building) {
        s_fill(s, cx - 7, cy, 5, 1, cc); s_fill(s, cx + 3, cy, 5, 1, cc);
        s_fill(s, cx, cy - 7, 1, 5, cc); s_fill(s, cx, cy + 3, 1, 5, cc);
    }
    snprintf(b, sizeof(b), "VIVOS %d", G.alive);
    s_textsh(s, 8, 8, 2, RGB(255, 255, 255), b);
    snprintf(b, sizeof(b), "BAJAS %d", me->kills);
    s_textsh(s, 8, 26, 2, RGB(255, 230, 120), b);
    if (G.zPhase >= 6) snprintf(b, sizeof(b), "ZONA FINAL");
    else if (G.zShrink) snprintf(b, sizeof(b), "TORMENTA CERRANDO %d", (int)G.zTimer + 1);
    else snprintf(b, sizeof(b), "TORMENTA EN %d", (int)G.zTimer + 1);
    s_textc(s, cx, 8, 2, RGB(210, 170, 255), b);
    for (int i = 0; i < 3; i++)
        if (G.feedT[i] > 0) s_textsh(s, RW - 8 - s_textw(G.feed[i], 1), 8 + i * 10, 1, RGB(255, 255, 255), G.feed[i]);
    if (G.msgT > 0) s_textc(s, cx, 50, 2, RGB(255, 240, 150), G.msg);
    if (G.prompt[0]) s_textc(s, cx, RH - 70, 2, RGB(255, 255, 255), G.prompt);
    if (G.matchT < 12.0f && me->phase == PH_BUS) s_textc(s, cx, RH - 60, 2, RGB(255, 255, 255), "A: SALTAR DEL BUS");
    if (me->phase == PH_FALL) s_textc(s, cx, RH - 60, 2, RGB(255, 255, 255), "A: ABRIR PLANEADOR");
    // vida y escudo
    bar(s, cx - 90, RH - 34, 180, 8, me->shield / 100.0f, RGB(70, 150, 255));
    bar(s, cx - 90, RH - 22, 180, 8, me->hp / 100.0f, me->hp > 35 ? RGB(90, 220, 100) : RGB(235, 70, 70));
    // arma
    const Item *it = &me->inv[me->slot];
    if (G.building) {
        snprintf(b, sizeof(b), "CONSTRUIR %s", piece_name(G.selPiece));
        s_textsh(s, RW - 170, RH - 40, 2, RGB(120, 190, 255), b);
        snprintf(b, sizeof(b), "%s %d", G.selMat == 0 ? "MADERA" : G.selMat == 1 ? "LADRILLO" : "METAL", me->mats[G.selMat]);
        s_textsh(s, RW - 170, RH - 22, 2, RGB(255, 255, 255), b);
    } else if (it->type != W_NONE) {
        s_textsh(s, RW - 170, RH - 40, 2, RAR_COL[it->rar], WD[it->type].name);
        if (it->type >= W_PISTOL) {
            if (me->reloadT > 0) snprintf(b, sizeof(b), "RECARGANDO");
            else snprintf(b, sizeof(b), "%d / %d", it->mag, me->ammo[WD[it->type].ammoType]);
            s_textsh(s, RW - 170, RH - 22, 2, RGB(255, 255, 255), b);
        }
    }
    // materiales
    for (int m = 0; m < 3; m++) {
        static const uint32_t mc[3] = {RGB(168, 120, 66), RGB(184, 86, 70), RGB(142, 154, 170)};
        s_fill(s, 8, RH - 52 + m * 16, 10, 10, mc[m]);
        snprintf(b, sizeof(b), "%d", me->mats[m]);
        s_textsh(s, 22, RH - 52 + m * 16, 1, RGB(255, 255, 255), b);
    }
    // efectos
    if (G.flash > 0) s_blend(s, 0, 0, RW, 6, RGB(255, 0, 0), (int)(G.flash * 500));
    if (me->alive && me->phase != PH_BUS) {
        float dx = me->pos.x - G.zx, dz = me->pos.z - G.zz;
        if (dx * dx + dz * dz > G.zr * G.zr) {
            s_blend(s, 0, 0, RW, RH, RGB(150, 60, 220), 60);
            s_textc(s, cx, 70, 2, RGB(255, 200, 255), "ESTAS EN LA TORMENTA");
        }
    }
    if (G.victory) {
        s_blend(s, 0, 0, RW, RH, RGB(255, 200, 0), 70);
        s_textc(s, cx, 110, 8, RGB(255, 235, 90), "VICTORIA");
        s_textc(s, cx, 170, 4, RGB(255, 255, 255), "#1 EN LA ISLA");
        if (G.endT > 1.5f) s_textc(s, cx, 220, 2, RGB(255, 255, 255), "PULSA A PARA VOLVER AL LOBBY");
    } else if (G.humanDead) {
        s_blend(s, 0, 0, RW, RH, RGB(160, 0, 0), 60);
        s_textc(s, cx, 100, 6, RGB(255, 130, 130), "ELIMINADO");
        snprintf(b, sizeof(b), "PUESTO #%d", me->place);
        s_textc(s, cx, 150, 3, RGB(255, 255, 255), b);
        if (G.endT > 1.5f) s_textc(s, cx, 190, 2, RGB(255, 255, 255), "PULSA A PARA VOLVER AL LOBBY");
    }
}

static void tv_lobby(Surface *s) {
    int cx = RW / 2;
    s_textc(s, cx, 14, 8, RGB(255, 225, 70), "FORTNITE U");
    s_textc(s, cx, 62, 2, RGB(255, 255, 255), "BATTLE ROYALE PARA WII U");
    char b[64];
    snprintf(b, sizeof(b), "MODO: %s", G.buildAllowed ? "CON CONSTRUCCION" : "SIN CONSTRUCCION");
    s_textc(s, cx, RH - 62, 2, G.buildAllowed ? RGB(120, 200, 255) : RGB(255, 190, 120), b);
    s_textc(s, cx, RH - 42, 2, RGB(255, 255, 255), "A: JUGAR   L/R: MODO");
    s_textc(s, cx, RH - 24, 1, RGB(230, 230, 230), "D-PAD IZQ/DER: ATUENDO   ARRIBA/ABAJO: EMOTES");
}

void game_render_tv(void) {
    Surface *s = &g_view;
    float tnow = G.lobbyT;
    if (G.state == ST_LOBBY) {
        draw_world(v3(120.0f + 0.9f, 1.55f, 7.0f), 0, -0.04f, 50, tnow, 1);
        draw_player(&P[0], tnow);
        tv_lobby(s);
        return;
    }
    draw_world(G.camPos, G.camYaw, G.camPitch, G.fov, tnow, 0);
    // vista previa de construccion
    if (G.building && !G.humanDead && P[0].phase == PH_GROUND) {
        Piece t;
        piece_target(&P[0], &t);
        piece_visual(&t, 1);
    }
    // barras de vida sobre los bots cercanos
    for (int i = 1; i < NP; i++) {
        const Player *p = &P[i];
        if (!p->alive || p->phase == PH_BUS) continue;
        float dx = p->pos.x - G.camPos.x, dz = p->pos.z - G.camPos.z;
        if (dx * dx + dz * dz > 45 * 45) continue;
        float sx, sy;
        if (r_project(v3(p->pos.x, p->pos.y + 2.3f, p->pos.z), &sx, &sy)) {
            s_fill(s, (int)sx - 12, (int)sy, 24, 3, RGB(0, 0, 0));
            s_fill(s, (int)sx - 11, (int)sy + 1, (int)(22 * p->hp / 100.0f), 1, RGB(90, 230, 100));
            if (p->shield > 0) s_fill(s, (int)sx - 11, (int)sy - 1, (int)(22 * p->shield / 100.0f), 1, RGB(80, 150, 255));
        }
    }
    tv_hud(s);
}

// ---------------------------------------------------------------- GamePad

static const Pad *gpad;

static int btn(Surface *s, int x, int y, int w, int h, const char *label, int sel, uint32_t col) {
    s_fill(s, x, y, w, h, sel ? col : mixcol(col, RGB(20, 22, 30), 0.65f));
    s_rect(s, x, y, w, h, sel ? RGB(255, 255, 255) : mixcol(col, RGB(0, 0, 0), 0.4f));
    if (label) s_textc(s, x + w / 2, y + h / 2 - 5, 2, RGB(255, 255, 255), label);
    return gpad && gpad->touchPressed && gpad->tx >= x && gpad->tx < x + w && gpad->ty >= y && gpad->ty < y + h;
}

static void drc_map(Surface *s, int x0, int y0, int size) {
    float k = (float)size / MAPSZ;
    int bl = 4;
    for (int j = 0; j < size; j += bl)
        for (int i = 0; i < size; i += bl) {
            float wx = (i + bl * 0.5f) / k, wz = (j + bl * 0.5f) / k;
            uint32_t c = GROUND_PAL[ground_idx(wx, wz)];
            if (G.state == ST_MATCH) {
                float dx = wx - G.zx, dz = wz - G.zz;
                if (dx * dx + dz * dz > G.zr * G.zr) c = mixcol(c, RGB(150, 60, 220), 0.55f);
            }
            s_fill(s, x0 + i, y0 + j, bl, bl, c);
        }
    for (int i = 0; i < nProps; i++)
        if (props[i].kind == PR_ROOF) s_fill(s, x0 + (int)((props[i].x - 3) * k), y0 + (int)((props[i].z - 2.5f) * k), 6, 5, RGB(190, 72, 60));
    for (int c = 0; c < 4; c++) s_textc(s, x0 + (int)(CITY[c][0] * k), y0 + (int)((CITY[c][1] - 29) * k), 1, RGB(255, 255, 255), CITY_NAME[c]);
    s_textc(s, x0 + (int)(120 * k), y0 + (int)(116 * k), 1, RGB(255, 255, 255), "LAGO");
    if (G.state != ST_MATCH) return;
    for (int i = 0; i < nChests; i++)
        if (!chests[i].open) s_fill(s, x0 + (int)(chests[i].x * k) - 1, y0 + (int)(chests[i].z * k) - 1, 3, 3, RGB(255, 220, 60));
    for (int i = 0; i < MAXPIECE; i++)
        if (pcs[i].on) s_fill(s, x0 + (int)(pcs[i].gx * 4 * k) - 1, y0 + (int)(pcs[i].gz * 4 * k) - 1, 3, 3, RGB(120, 190, 255));
    s_ring(s, x0 + (int)(G.zx * k), y0 + (int)(G.zz * k), (int)(G.zr * k), RGB(255, 255, 255), 120);
    if (G.zPhase < 6) s_ring(s, x0 + (int)(G.ztx * k), y0 + (int)(G.ztz * k), (int)(G.ztr * k), RGB(120, 200, 255), 80);
    if (G.busActive) {
        s_line(s, x0 + (int)(G.busA.x * k), y0 + (int)(G.busA.z * k), x0 + (int)(G.busB.x * k), y0 + (int)(G.busB.z * k), RGB(255, 255, 120));
        s_circle(s, x0 + (int)(G.busPos.x * k), y0 + (int)(G.busPos.z * k), 4, RGB(255, 140, 60));
    }
    const Player *me = &P[0];
    if (me->alive) {
        int px = x0 + (int)(me->pos.x * k), py = y0 + (int)(me->pos.z * k);
        float yy = me->phase == PH_BUS ? G.busYaw : G.camYaw;
        s_circle(s, px, py, 4, RGB(255, 255, 255));
        s_circle(s, px, py, 2, RGB(60, 120, 255));
        s_line(s, px, py, px + (int)(sinf(yy) * 14), py + (int)(cosf(yy) * 14), RGB(255, 255, 255));
    }
    s_rect(s, x0 - 1, y0 - 1, size + 2, size + 2, RGB(255, 255, 255));
}

static void drc_lobby(Surface *s) {
    s_fill(s, 0, 0, DRC_W, DRC_H, RGB(24, 26, 44));
    s_textc(s, DRC_W / 2, 14, 6, RGB(255, 225, 70), "LOBBY");
    s_textc(s, DRC_W / 2, 64, 2, RGB(200, 210, 255), "ELIGE TU MODO");
    if (btn(s, 60, 95, 340, 70, "CON CONSTRUCCION", G.buildAllowed, RGB(60, 130, 230))) G.buildAllowed = 1;
    if (btn(s, 454, 95, 340, 70, "SIN CONSTRUCCION", !G.buildAllowed, RGB(230, 140, 50))) G.buildAllowed = 0;
    s_textc(s, DRC_W / 2, 185, 2, RGB(200, 210, 255), "ATUENDO");
    for (int i = 0; i < 8; i++) {
        int x = 62 + i * 92;
        s_fill(s, x, 210, 80, 60, SKIN_COL[i][0]);
        s_fill(s, x, 210, 80, 14, SKIN_COL[i][1]);
        if (G.skin == i) s_rect(s, x - 3, 207, 86, 66, RGB(255, 255, 255));
        if (gpad && gpad->touchPressed && gpad->tx >= x && gpad->tx < x + 80 && gpad->ty >= 210 && gpad->ty < 270) G.skin = i;
    }
    s_textc(s, DRC_W / 2, 288, 2, RGB(200, 210, 255), "EMOTES (TOCA PARA VER EN LA TV)");
    static const char *en[4] = {"BAILE", "SALUDO", "GIRO", "VICTORIA"};
    for (int i = 0; i < 4; i++)
        if (btn(s, 40 + i * 195, 315, 180, 50, en[i], G.lobbyEmote == i + 1 && P[0].emote, RGB(150, 90, 220))) {
            G.lobbyEmote = i + 1;
            P[0].emote = i + 1;
            P[0].emoteT = 0;
        }
    if (btn(s, 230, 395, 394, 65, "JUGAR", 1, RGB(60, 190, 90))) match_start();
}

static void drc_match(Surface *s) {
    s_fill(s, 0, 0, DRC_W, DRC_H, RGB(22, 24, 38));
    drc_map(s, 10, 10, 460);
    s_fill(s, 474, 0, DRC_W - 474, DRC_H, RGB(22, 24, 38));
    char b[64];
    const Player *me = &P[0];
    int x = 484, w = 360;
    snprintf(b, sizeof(b), "VIVOS %d   BAJAS %d", G.alive, me->kills);
    s_textsh(s, x, 10, 2, RGB(255, 255, 255), b);
    if (G.zPhase >= 6) snprintf(b, sizeof(b), "ZONA FINAL");
    else snprintf(b, sizeof(b), "TORMENTA %s %d S", G.zShrink ? "CERRANDO" : "EN", (int)G.zTimer + 1);
    s_textsh(s, x, 30, 2, RGB(210, 170, 255), b);
    bar(s, x, 52, w, 10, me->shield / 100.0f, RGB(70, 150, 255));
    bar(s, x, 66, w, 10, me->hp / 100.0f, me->hp > 35 ? RGB(90, 220, 100) : RGB(235, 70, 70));
    // materiales (toca para elegir)
    static const char *mn[3] = {"MADERA", "LADRILLO", "METAL"};
    static const uint32_t mc[3] = {RGB(168, 120, 66), RGB(184, 86, 70), RGB(142, 154, 170)};
    for (int m = 0; m < 3; m++) {
        int bx = x + m * 122;
        if (btn(s, bx, 86, 116, 40, NULL, G.selMat == m, mc[m])) G.selMat = m;
        s_text(s, bx + 5, 91, 1, RGB(255, 255, 255), mn[m]);
        snprintf(b, sizeof(b), "%d", me->mats[m]);
        s_textsh(s, bx + 5, 106, 2, RGB(255, 255, 255), b);
    }
    // piezas
    if (G.buildAllowed) {
        for (int i = 0; i < PC_COUNT; i++) {
            int bx = x + i * 91;
            if (btn(s, bx, 134, 86, 38, piece_name(i), G.building && G.selPiece == i, RGB(60, 130, 230))) { G.selPiece = i; G.building = 1; }
        }
        if (btn(s, x, 178, w, 28, G.building ? "MODO CONSTRUIR (Y)" : "ACTIVAR CONSTRUCCION (Y)", G.building, RGB(60, 190, 90))) G.building ^= 1;
    } else {
        s_fill(s, x, 134, w, 72, RGB(40, 36, 30));
        s_textc(s, x + w / 2, 160, 2, RGB(255, 190, 120), "SIN CONSTRUCCION");
    }
    // inventario
    for (int i = 0; i < 5; i++) {
        const Item *it = &me->inv[i];
        int by = 214 + i * 38;
        uint32_t col = it->type ? RAR_COL[it->rar] : RGB(70, 70, 80);
        if (btn(s, x, by, w, 34, NULL, !G.building && me->slot == i, col) && it->type) select_slot(0, i);
        if (it->type) {
            s_textsh(s, x + 8, by + 10, 2, RGB(255, 255, 255), WD[it->type].name);
            if (it->type >= W_PISTOL) {
                snprintf(b, sizeof(b), "%d/%d", it->mag, me->ammo[WD[it->type].ammoType]);
                s_textsh(s, x + w - 8 - s_textw(b, 2), by + 10, 2, RGB(255, 255, 255), b);
            }
        } else s_text(s, x + 8, by + 12, 1, RGB(130, 130, 140), "VACIO");
    }
    // curar y emotes
    snprintf(b, sizeof(b), "CURAR  VIDA %d  ESCUDO %d", me->meds, me->shields);
    if (btn(s, x, 408, w, 28, b, 0, RGB(220, 70, 90))) use_heal(0);
    static const char *en[4] = {"BAILE", "SALUDO", "GIRO", "VICT."};
    for (int i = 0; i < 4; i++)
        if (btn(s, x + i * 91, 442, 86, 30, en[i], me->emote == i + 1, RGB(150, 90, 220))) play_emote(0, i + 1);
}

void game_render_drc(Surface *s, const Pad *pad) {
    gpad = pad;
    if (G.state == ST_LOBBY) drc_lobby(s);
    else drc_match(s);
}
