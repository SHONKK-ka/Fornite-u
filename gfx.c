// Renderizador 3D por software (triangulos planos con z-buffer) + dibujo 2D.
// Sin GPU: asi la TV y el GamePad pueden mostrar imagenes distintas.
#include "gfx.h"
#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define PI_F 3.14159265f
#define NEAR_Z 0.2f

static uint32_t viewpx[RW * RH];
static float zb[RW * RH]; // 1/z (0 = infinito)
Surface g_view = {viewpx, RW, RH, RW};

static V3 camP, cR, cU, cF;
static float camcx = RW * 0.5f, camcy = RH * 0.5f, cf = 300.0f, cYaw, cPitch;
static uint32_t fogCol = 0;
static float fogEnd = 110.0f;
static int stipple = 0;
static V3 lightDir = {0.42f, 0.80f, 0.43f};
static int lightInit = 0;

uint32_t mixcol(uint32_t a, uint32_t b, float t) {
    if (t <= 0) return a;
    if (t >= 1) return b;
    int ar = (a >> 24) & 255, ag = (a >> 16) & 255, ab = (a >> 8) & 255;
    int br = (b >> 24) & 255, bg = (b >> 16) & 255, bb = (b >> 8) & 255;
    int r = (int)(ar + (br - ar) * t), g = (int)(ag + (bg - ag) * t), bl = (int)(ab + (bb - ab) * t);
    return RGB(r, g, bl);
}

static uint32_t lit(uint32_t c, V3 n, float zv) {
    float sh = 0.50f + 0.50f * fmaxf(0.0f, v3dot(n, lightDir));
    int r = (int)(((c >> 24) & 255) * sh), g = (int)(((c >> 16) & 255) * sh), b = (int)(((c >> 8) & 255) * sh);
    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;
    uint32_t o = RGB(r, g, b);
    float k = (zv - fogEnd * 0.4f) / (fogEnd * 0.6f);
    return k > 0 ? mixcol(o, fogCol, k) : o;
}

void r_stipple(int on) { stipple = on; }
V3 r_cam_forward(void) { return cF; }

void r_camera(V3 pos, float yaw, float pitch, float fovDeg) {
    camP = pos;
    cYaw = yaw;
    cPitch = pitch;
    float sa = sinf(yaw), ca = cosf(yaw), sp = sinf(pitch), cp = cosf(pitch);
    cF = v3(sa * cp, sp, ca * cp);
    cR = v3(ca, 0, -sa);
    cU = v3(-sa * sp, cp, -ca * sp);
    cf = (RH * 0.5f) / tanf(fovDeg * 0.5f * PI_F / 180.0f);
    if (!lightInit) {
        float l = sqrtf(v3dot(lightDir, lightDir));
        lightDir = v3mul(lightDir, 1.0f / l);
        lightInit = 1;
    }
}

int r_project(V3 p, float *sx, float *sy) {
    V3 d = v3sub(p, camP);
    float z = v3dot(d, cF);
    if (z < NEAR_Z) return 0;
    *sx = camcx + cf * v3dot(d, cR) / z;
    *sy = camcy - cf * v3dot(d, cU) / z;
    return 1;
}

// ---------------------------------------------------------------- cielo y suelo

void r_sky_ground(uint32_t top, uint32_t horiz, uint32_t fog, float fogEndD, const uint32_t *pal, int npal,
                  int (*fn)(float, float)) {
    fogCol = fog;
    fogEnd = fogEndD;
    memset(zb, 0, sizeof(zb));
    float cp = cosf(cPitch), sp = sinf(cPitch);
    float hy = camcy + cf * tanf(cPitch);
    for (int y = 0; y < RH; y++) {
        float t = hy > 1.0f ? (float)y / hy : 1.0f;
        uint32_t c = mixcol(top, horiz, t > 1 ? 1 : t);
        uint32_t *row = &viewpx[y * RW];
        for (int x = 0; x < RW; x++) row[x] = c;
    }
    float h = camP.y + 1.0f; // altura de la camara sobre el suelo (+1 evita division por cero)
    int y0 = hy < 0 ? 0 : (int)hy;
    uint32_t rowpal[16];
    for (int y = y0; y < RH; y++) {
        float yn = -(((float)y + 0.5f) - camcy) / cf;
        float dy = yn * cp + sp;
        uint32_t *row = &viewpx[y * RW];
        if (dy > -1e-4f) continue;
        float t = h / (-dy);
        if (t > fogEndD * 1.5f) {
            for (int x = 0; x < RW; x++) row[x] = fog;
            continue;
        }
        float k = (t - fogEndD * 0.35f) / (fogEndD * 0.65f);
        if (k < 0) k = 0;
        if (k > 1) k = 1;
        for (int i = 0; i < npal && i < 16; i++) rowpal[i] = mixcol(pal[i], fog, k);
        float xn0 = (0.5f - camcx) / cf;
        float ax = camP.x + t * (cU.x * yn + cF.x), az = camP.z + t * (cU.z * yn + cF.z);
        float px = ax + t * cR.x * xn0, pz = az + t * cR.z * xn0;
        float sx = 2.0f * t * cR.x / cf, sz = 2.0f * t * cR.z / cf;
        for (int x = 0; x < RW; x += 2) {
            int idx = fn(px, pz);
            uint32_t c = rowpal[idx];
            row[x] = c;
            row[x + 1] = c;
            px += sx;
            pz += sz;
        }
    }
}

// ---------------------------------------------------------------- rasterizador

typedef struct { float x, y, z; } VV;

static void raster(VV a, VV b, VV c, uint32_t col) {
    float x0 = camcx + cf * a.x / a.z, y0 = camcy - cf * a.y / a.z, w0 = 1.0f / a.z;
    float x1 = camcx + cf * b.x / b.z, y1 = camcy - cf * b.y / b.z, w1 = 1.0f / b.z;
    float x2 = camcx + cf * c.x / c.z, y2 = camcy - cf * c.y / c.z, w2 = 1.0f / c.z;
    float area = (x1 - x0) * (y2 - y0) - (y1 - y0) * (x2 - x0);
    if (area == 0.0f) return;
    if (area < 0) {
        float t;
        t = x1; x1 = x2; x2 = t;
        t = y1; y1 = y2; y2 = t;
        t = w1; w1 = w2; w2 = t;
        area = -area;
    }
    float fminx = fminf(x0, fminf(x1, x2)), fmaxx = fmaxf(x0, fmaxf(x1, x2));
    float fminy = fminf(y0, fminf(y1, y2)), fmaxy = fmaxf(y0, fmaxf(y1, y2));
    if (fmaxx < 0 || fmaxy < 0 || fminx > RW || fminy > RH) return;
    int minx = fminx < 0 ? 0 : (int)fminx, maxx = fmaxx > RW - 1 ? RW - 1 : (int)fmaxx;
    int miny = fminy < 0 ? 0 : (int)fminy, maxy = fmaxy > RH - 1 ? RH - 1 : (int)fmaxy;
    if (minx > maxx || miny > maxy) return;
    float inv = 1.0f / area;
    float px = (float)minx + 0.5f, py = (float)miny + 0.5f;
    float dx0 = (y1 - y2), dy0 = (x2 - x1), e0r = (x2 - x1) * (py - y1) - (y2 - y1) * (px - x1);
    float dx1 = (y2 - y0), dy1 = (x0 - x2), e1r = (x0 - x2) * (py - y2) - (y0 - y2) * (px - x2);
    float dx2 = (y0 - y1), dy2 = (x1 - x0), e2r = (x1 - x0) * (py - y0) - (y1 - y0) * (px - x0);
    float dwdx = (dx0 * w0 + dx1 * w1 + dx2 * w2) * inv;
    float dwdy = (dy0 * w0 + dy1 * w1 + dy2 * w2) * inv;
    float wr = (e0r * w0 + e1r * w1 + e2r * w2) * inv;
    for (int y = miny; y <= maxy; y++) {
        float e0 = e0r, e1 = e1r, e2 = e2r, w = wr;
        uint32_t *cp = &viewpx[y * RW + minx];
        float *zp = &zb[y * RW + minx];
        for (int x = minx; x <= maxx; x++) {
            if (e0 >= 0 && e1 >= 0 && e2 >= 0 && w > *zp && (!stipple || ((x ^ y) & 1))) {
                *zp = w;
                *cp = col;
            }
            e0 += dx0; e1 += dx1; e2 += dx2; w += dwdx;
            cp++; zp++;
        }
        e0r += dy0; e1r += dy1; e2r += dy2; wr += dwdy;
    }
}

static VV to_view(V3 p) {
    V3 d = v3sub(p, camP);
    VV v = {v3dot(d, cR), v3dot(d, cU), v3dot(d, cF)};
    return v;
}

static void emit_tri(V3 a, V3 b, V3 c, uint32_t col) {
    VV in[3] = {to_view(a), to_view(b), to_view(c)};
    VV out[4];
    int n = 0;
    for (int i = 0; i < 3; i++) {
        VV p = in[i], q = in[(i + 1) % 3];
        int pin = p.z >= NEAR_Z, qin = q.z >= NEAR_Z;
        if (pin) out[n++] = p;
        if (pin != qin) {
            float t = (NEAR_Z - p.z) / (q.z - p.z);
            VV m = {p.x + (q.x - p.x) * t, p.y + (q.y - p.y) * t, NEAR_Z};
            out[n++] = m;
        }
    }
    if (n >= 3) raster(out[0], out[1], out[2], col);
    if (n == 4) raster(out[0], out[2], out[3], col);
}

void r_tri(V3 a, V3 b, V3 c, V3 n, uint32_t col) {
    V3 ctr = v3mul(v3add(v3add(a, b), c), 1.0f / 3.0f);
    if (v3dot(n, v3sub(camP, ctr)) <= 0) return;
    float zv = v3dot(v3sub(ctr, camP), cF);
    if (zv > fogEnd + 20) return;
    emit_tri(a, b, c, lit(col, n, zv));
}

void r_quad(V3 a, V3 b, V3 c, V3 d, V3 n, uint32_t col) {
    V3 ctr = v3mul(v3add(a, c), 0.5f);
    if (v3dot(n, v3sub(camP, ctr)) <= 0) return;
    float zv = v3dot(v3sub(ctr, camP), cF);
    if (zv > fogEnd + 20) return;
    uint32_t lc = lit(col, n, zv);
    emit_tri(a, b, c, lc);
    emit_tri(a, c, d, lc);
}

static V3 xf_pt(V3 p, float sy, float cy, float sp, float cp, V3 pivot) {
    float y1 = p.y * cp - p.z * sp, z1 = p.y * sp + p.z * cp;
    return v3(pivot.x + p.x * cy + z1 * sy, pivot.y + y1, pivot.z - p.x * sy + z1 * cy);
}
static V3 xf_dir(V3 p, float sy, float cy, float sp, float cp) {
    float y1 = p.y * cp - p.z * sp, z1 = p.y * sp + p.z * cp;
    return v3(p.x * cy + z1 * sy, y1, -p.x * sy + z1 * cy);
}

void r_box_xf(V3 pivot, float yaw, float pitch, V3 off, V3 h, uint32_t col) {
    static const int F[6][4] = {{1, 3, 7, 5}, {0, 4, 6, 2}, {2, 3, 7, 6}, {0, 1, 5, 4}, {4, 5, 7, 6}, {0, 2, 3, 1}};
    static const V3 N[6] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
    float sy = sinf(yaw), cy = cosf(yaw), sp = sinf(pitch), cp = cosf(pitch);
    V3 c[8];
    for (int i = 0; i < 8; i++) {
        V3 l = v3(off.x + ((i & 1) ? h.x : -h.x), off.y + ((i & 2) ? h.y : -h.y), off.z + ((i & 4) ? h.z : -h.z));
        c[i] = xf_pt(l, sy, cy, sp, cp, pivot);
    }
    for (int f = 0; f < 6; f++) {
        V3 n = xf_dir(N[f], sy, cy, sp, cp);
        r_quad(c[F[f][0]], c[F[f][1]], c[F[f][2]], c[F[f][3]], n, col);
    }
}

void r_box(V3 c, V3 h, uint32_t col) { r_box_xf(c, 0, 0, v3(0, 0, 0), h, col); }

void r_cone(V3 base, float rad, float height, int seg, uint32_t col) {
    V3 apex = v3(base.x, base.y + height, base.z);
    float len = sqrtf(rad * rad + height * height);
    for (int i = 0; i < seg; i++) {
        float a0 = 2 * PI_F * i / seg, a1 = 2 * PI_F * (i + 1) / seg, am = (a0 + a1) * 0.5f;
        V3 p0 = v3(base.x + cosf(a0) * rad, base.y, base.z + sinf(a0) * rad);
        V3 p1 = v3(base.x + cosf(a1) * rad, base.y, base.z + sinf(a1) * rad);
        V3 n = v3(cosf(am) * height / len, rad / len, sinf(am) * height / len);
        r_tri(apex, p0, p1, n, col);
    }
}

void r_pyramid(V3 b, float hx, float hz, float h, uint32_t col) {
    V3 apex = v3(b.x, b.y + h, b.z);
    V3 c00 = v3(b.x - hx, b.y, b.z - hz), c10 = v3(b.x + hx, b.y, b.z - hz);
    V3 c11 = v3(b.x + hx, b.y, b.z + hz), c01 = v3(b.x - hx, b.y, b.z + hz);
    float lx = sqrtf(h * h + hx * hx), lz = sqrtf(h * h + hz * hz);
    r_tri(apex, c10, c11, v3(h / lx, hx / lx, 0), col);
    r_tri(apex, c01, c00, v3(-h / lx, hx / lx, 0), col);
    r_tri(apex, c11, c01, v3(0, hz / lz, h / lz), col);
    r_tri(apex, c00, c10, v3(0, hz / lz, -h / lz), col);
}

// ---------------------------------------------------------------- 2D

static inline void put(Surface *s, int x, int y, uint32_t c) {
    if ((unsigned)x < (unsigned)s->w && (unsigned)y < (unsigned)s->h) s->px[y * s->pitch + x] = c;
}

void s_fill(Surface *s, int x, int y, int w, int h, uint32_t col) {
    int x1 = x + w, y1 = y + h;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x1 > s->w) x1 = s->w;
    if (y1 > s->h) y1 = s->h;
    for (int j = y; j < y1; j++) {
        uint32_t *row = &s->px[j * s->pitch];
        for (int i = x; i < x1; i++) row[i] = col;
    }
}

void s_blend(Surface *s, int x, int y, int w, int h, uint32_t col, int a) {
    int x1 = x + w, y1 = y + h;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x1 > s->w) x1 = s->w;
    if (y1 > s->h) y1 = s->h;
    int ia = 256 - a;
    int cr = ((col >> 24) & 255) * a, cg = ((col >> 16) & 255) * a, cb = ((col >> 8) & 255) * a;
    for (int j = y; j < y1; j++) {
        uint32_t *row = &s->px[j * s->pitch];
        for (int i = x; i < x1; i++) {
            uint32_t d = row[i];
            int r = (((d >> 24) & 255) * ia + cr) >> 8, g = (((d >> 16) & 255) * ia + cg) >> 8,
                b = (((d >> 8) & 255) * ia + cb) >> 8;
            row[i] = RGB(r, g, b);
        }
    }
}

void s_rect(Surface *s, int x, int y, int w, int h, uint32_t col) {
    s_fill(s, x, y, w, 1, col);
    s_fill(s, x, y + h - 1, w, 1, col);
    s_fill(s, x, y, 1, h, col);
    s_fill(s, x + w - 1, y, 1, h, col);
}

void s_line(Surface *s, int x0, int y0, int x1, int y1, uint32_t col) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (int guard = 0; guard < 4000; guard++) {
        put(s, x0, y0, col);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void s_circle(Surface *s, int cx, int cy, int r, uint32_t col) {
    for (int dy = -r; dy <= r; dy++) {
        int dx = (int)sqrtf((float)(r * r - dy * dy));
        s_fill(s, cx - dx, cy + dy, dx * 2 + 1, 1, col);
    }
}

void s_ring(Surface *s, int cx, int cy, int r, uint32_t col, int seg) {
    int px = cx + r, py = cy;
    for (int i = 1; i <= seg; i++) {
        float a = 2 * PI_F * i / seg;
        int x = cx + (int)(cosf(a) * r), y = cy + (int)(sinf(a) * r);
        s_line(s, px, py, x, y, col);
        px = x;
        py = y;
    }
}

void r_line3(V3 a, V3 b, uint32_t col) {
    VV va = to_view(a), vb = to_view(b);
    if (va.z < NEAR_Z && vb.z < NEAR_Z) return;
    if (va.z < NEAR_Z) {
        float t = (NEAR_Z - va.z) / (vb.z - va.z);
        va = (VV){va.x + (vb.x - va.x) * t, va.y + (vb.y - va.y) * t, NEAR_Z};
    } else if (vb.z < NEAR_Z) {
        float t = (NEAR_Z - vb.z) / (va.z - vb.z);
        vb = (VV){vb.x + (va.x - vb.x) * t, vb.y + (va.y - vb.y) * t, NEAR_Z};
    }
    int x0 = (int)(camcx + cf * va.x / va.z), y0 = (int)(camcy - cf * va.y / va.z);
    int x1 = (int)(camcx + cf * vb.x / vb.z), y1 = (int)(camcy - cf * vb.y / vb.z);
    if (abs(x0) > 4000 || abs(y0) > 4000 || abs(x1) > 4000 || abs(y1) > 4000) return;
    s_line(&g_view, x0, y0, x1, y1, col);
}

// fuente 3x5: A-Z, 0-9, # ! - : espacio . , / % + < > ( )
static const char FONT_CHARS[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789#!-: .,/%+<>()";
static const uint8_t FONT[50][5] = {
    {2, 5, 7, 5, 5}, {6, 5, 6, 5, 6}, {3, 4, 4, 4, 3}, {6, 5, 5, 5, 6}, {7, 4, 6, 4, 7},
    {7, 4, 6, 4, 4}, {3, 4, 5, 5, 3}, {5, 5, 7, 5, 5}, {7, 2, 2, 2, 7}, {1, 1, 1, 5, 2},
    {5, 5, 6, 5, 5}, {4, 4, 4, 4, 7}, {5, 7, 7, 5, 5}, {6, 5, 5, 5, 5}, {2, 5, 5, 5, 2},
    {6, 5, 6, 4, 4}, {2, 5, 5, 7, 3}, {6, 5, 6, 5, 5}, {3, 4, 2, 1, 6}, {7, 2, 2, 2, 2},
    {5, 5, 5, 5, 7}, {5, 5, 5, 5, 2}, {5, 5, 7, 7, 5}, {5, 5, 2, 5, 5}, {5, 5, 2, 2, 2},
    {7, 1, 2, 4, 7},
    {7, 5, 5, 5, 7}, {2, 6, 2, 2, 7}, {6, 1, 2, 4, 7}, {6, 1, 2, 1, 6}, {5, 5, 7, 1, 1},
    {7, 4, 6, 1, 6}, {3, 4, 7, 5, 7}, {7, 1, 2, 2, 2}, {7, 5, 7, 5, 7}, {7, 5, 7, 1, 6},
    {5, 7, 5, 7, 5}, {2, 2, 2, 0, 2}, {0, 0, 7, 0, 0}, {0, 2, 0, 2, 0}, {0, 0, 0, 0, 0},
    {0, 0, 0, 0, 2}, {0, 0, 0, 2, 4}, {1, 1, 2, 4, 4}, {5, 1, 2, 4, 5}, {0, 2, 7, 2, 0},
    {1, 2, 4, 2, 1}, {4, 2, 1, 2, 4}, {1, 2, 2, 2, 1}, {4, 2, 2, 2, 4}};

int s_textw(const char *t, int sc) {
    int n = (int)strlen(t);
    return n > 0 ? n * 4 * sc - sc : 0;
}

void s_text(Surface *s, int x, int y, int sc, uint32_t col, const char *t) {
    for (; *t; t++) {
        char c = (char)toupper((unsigned char)*t);
        const char *p = strchr(FONT_CHARS, c);
        int idx = p ? (int)(p - FONT_CHARS) : 40;
        for (int row = 0; row < 5; row++)
            for (int cl = 0; cl < 3; cl++)
                if (FONT[idx][row] & (4 >> cl)) s_fill(s, x + cl * sc, y + row * sc, sc, sc, col);
        x += 4 * sc;
    }
}

void s_textsh(Surface *s, int x, int y, int sc, uint32_t col, const char *t) {
    s_text(s, x + (sc > 1 ? sc / 2 : 1), y + (sc > 1 ? sc / 2 : 1), sc, RGB(0, 0, 0), t);
    s_text(s, x, y, sc, col, t);
}

void s_textc(Surface *s, int cx, int y, int sc, uint32_t col, const char *t) {
    s_textsh(s, cx - s_textw(t, sc) / 2, y, sc, col, t);
}
