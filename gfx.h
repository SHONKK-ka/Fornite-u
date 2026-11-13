#ifndef GFX_H
#define GFX_H
#include <stdint.h>
#include "platform.h"

#define RW 640
#define RH 360

#define RGB(r, g, b) (((uint32_t)(r) << 24) | ((uint32_t)(g) << 16) | ((uint32_t)(b) << 8))

typedef struct { float x, y, z; } V3;
static inline V3 v3(float x, float y, float z) { V3 v = {x, y, z}; return v; }
static inline V3 v3add(V3 a, V3 b) { return v3(a.x + b.x, a.y + b.y, a.z + b.z); }
static inline V3 v3sub(V3 a, V3 b) { return v3(a.x - b.x, a.y - b.y, a.z - b.z); }
static inline V3 v3mul(V3 a, float s) { return v3(a.x * s, a.y * s, a.z * s); }
static inline float v3dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

extern Surface g_view; // 640x360, la imagen 3D de la TV

// ---- 3D ----
void r_camera(V3 pos, float yaw, float pitch, float fovDeg);
void r_sky_ground(uint32_t skyTop, uint32_t skyHorizon, uint32_t fog, float fogEnd,
                  const uint32_t *pal, int npal, int (*fn)(float, float));
void r_box(V3 c, V3 half, uint32_t col);
void r_box_xf(V3 pivot, float yaw, float pitch, V3 off, V3 half, uint32_t col);
void r_quad(V3 a, V3 b, V3 c, V3 d, V3 n, uint32_t col);
void r_tri(V3 a, V3 b, V3 c, V3 n, uint32_t col);
void r_cone(V3 base, float rad, float height, int seg, uint32_t col);
void r_pyramid(V3 base, float hx, float hz, float h, uint32_t col);
void r_line3(V3 a, V3 b, uint32_t col);
int r_project(V3 p, float *sx, float *sy); // 0 si esta detras de la camara
void r_stipple(int on);
V3 r_cam_forward(void);

// ---- 2D ----
void s_fill(Surface *s, int x, int y, int w, int h, uint32_t col);
void s_blend(Surface *s, int x, int y, int w, int h, uint32_t col, int a256);
void s_rect(Surface *s, int x, int y, int w, int h, uint32_t col);
void s_line(Surface *s, int x0, int y0, int x1, int y1, uint32_t col);
void s_circle(Surface *s, int cx, int cy, int r, uint32_t col);
void s_ring(Surface *s, int cx, int cy, int r, uint32_t col, int seg);
void s_text(Surface *s, int x, int y, int sc, uint32_t col, const char *t);
void s_textsh(Surface *s, int x, int y, int sc, uint32_t col, const char *t);
int s_textw(const char *t, int sc);
void s_textc(Surface *s, int cx, int y, int sc, uint32_t col, const char *t);
uint32_t mixcol(uint32_t a, uint32_t b, float t);

#endif
