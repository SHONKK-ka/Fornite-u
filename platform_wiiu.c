// Plataforma Wii U: dos pantallas independientes con OSScreen (TV y GamePad)
// y entrada por VPAD (GamePad). No usa SDL: asi cada pantalla muestra algo distinto.
#include "platform.h"
#include <coreinit/cache.h>
#include <coreinit/memdefaultheap.h>
#include <coreinit/screen.h>
#include <coreinit/time.h>
#include <string.h>
#include <vpad/input.h>
#include <whb/proc.h>

static uint32_t *tvBase, *drcBase;
static size_t tvBytes, drcBytes;
static size_t tvHalf, drcHalf; // palabras (uint32) por mitad
static int tvPitch = 1920, tvRows = 1080, drcPitch = 896;
static uint32_t *tvDraw, *drcDraw;
static uint32_t probeCount;
static int ready;
static uint32_t prevHold;
static int prevTouch;

// Detecta que mitad del buffer es la "trasera" (donde OSScreen dibuja ahora)
static uint32_t *probe_draw(OSScreenID scr, uint32_t *base, size_t half, uint32_t *fallback) {
    uint32_t mark = 0x10203000u | ((++probeCount) & 0xFFu);
    base[0] = 0;
    base[half] = 0;
    OSScreenPutPixelEx(scr, 0, 0, mark);
    if (base[0] == mark) return base;
    if (base[half] == mark) return base + half;
    return fallback;
}

// Busca el pitch real escribiendo un pixel en (0,1)
static int probe_pitch(OSScreenID scr, uint32_t *draw, int guess) {
    uint32_t mark = 0x0A0B0C00u | ((++probeCount) & 0xFFu);
    OSScreenPutPixelEx(scr, 0, 1, mark);
    if (draw[guess] == mark) return guess;
    for (int i = 1; i < 4096; i++)
        if (draw[i] == mark) return i;
    return guess;
}

int plat_init(void) {
    WHBProcInit();
    OSScreenInit();
    tvBytes = OSScreenGetBufferSizeEx(SCREEN_TV);
    drcBytes = OSScreenGetBufferSizeEx(SCREEN_DRC);
    tvBase = (uint32_t *)MEMAllocFromDefaultHeapEx((uint32_t)tvBytes, 0x100);
    drcBase = (uint32_t *)MEMAllocFromDefaultHeapEx((uint32_t)drcBytes, 0x100);
    if (!tvBase || !drcBase) return 0;
    OSScreenSetBufferEx(SCREEN_TV, tvBase);
    OSScreenSetBufferEx(SCREEN_DRC, drcBase);
    OSScreenEnableEx(SCREEN_TV, 1);
    OSScreenEnableEx(SCREEN_DRC, 1);
    OSScreenClearBufferEx(SCREEN_TV, 0);
    OSScreenClearBufferEx(SCREEN_DRC, 0);
    OSScreenFlipBuffersEx(SCREEN_TV);
    OSScreenFlipBuffersEx(SCREEN_DRC);
    tvHalf = tvBytes / 8;
    drcHalf = drcBytes / 8;
    tvDraw = probe_draw(SCREEN_TV, tvBase, tvHalf, tvBase + tvHalf);
    drcDraw = probe_draw(SCREEN_DRC, drcBase, drcHalf, drcBase + drcHalf);
    tvPitch = probe_pitch(SCREEN_TV, tvDraw, 1920);
    drcPitch = probe_pitch(SCREEN_DRC, drcDraw, 896);
    tvRows = (int)(tvHalf / (size_t)tvPitch);
    if (tvRows > 1080) tvRows = 1080;
    VPADInit();
    ready = 1;
    return 1;
}

int plat_running(void) { return WHBProcIsRunning(); }

double plat_time(void) { return (double)OSTicksToMilliseconds(OSGetSystemTime()) / 1000.0; }

void plat_poll(Pad *pad) {
    memset(pad, 0, sizeof(*pad));
    VPADStatus st;
    VPADReadError err;
    if (VPADRead(VPAD_CHAN_0, &st, 1, &err) < 1 || err != VPAD_READ_SUCCESS) return;
    uint32_t h = 0;
    struct { uint32_t v, b; } map[] = {
        {VPAD_BUTTON_A, BTN_A}, {VPAD_BUTTON_B, BTN_B}, {VPAD_BUTTON_X, BTN_X}, {VPAD_BUTTON_Y, BTN_Y},
        {VPAD_BUTTON_L, BTN_L}, {VPAD_BUTTON_R, BTN_R}, {VPAD_BUTTON_ZL, BTN_ZL}, {VPAD_BUTTON_ZR, BTN_ZR},
        {VPAD_BUTTON_UP, BTN_UP}, {VPAD_BUTTON_DOWN, BTN_DOWN}, {VPAD_BUTTON_LEFT, BTN_LEFT},
        {VPAD_BUTTON_RIGHT, BTN_RIGHT}, {VPAD_BUTTON_PLUS, BTN_PLUS}, {VPAD_BUTTON_MINUS, BTN_MINUS},
        {VPAD_BUTTON_STICK_L, BTN_L3}, {VPAD_BUTTON_STICK_R, BTN_R3}};
    for (unsigned i = 0; i < sizeof(map) / sizeof(map[0]); i++)
        if (st.hold & map[i].v) h |= map[i].b;
    pad->hold = h;
    pad->pressed = h & ~prevHold;
    prevHold = h;
    pad->lx = st.leftStick.x;
    pad->ly = st.leftStick.y;
    pad->rx = st.rightStick.x;
    pad->ry = st.rightStick.y;
    VPADTouchData cal;
    VPADGetTPCalibratedPoint(VPAD_CHAN_0, &cal, &st.tpNormal);
    if (cal.touched) {
        pad->touching = 1;
        pad->tx = (int)cal.x * DRC_W / 1280;
        pad->ty = (int)cal.y * DRC_H / 720;
    }
    pad->touchPressed = pad->touching && !prevTouch;
    prevTouch = pad->touching;
}

// Sube la imagen 3D (w x h) a la TV con escala entera (640x360 -> 1920x1080 = x3)
void plat_present_tv(const uint32_t *src, int w, int h) {
    if (!ready) return;
    tvDraw = probe_draw(SCREEN_TV, tvBase, tvHalf, tvDraw);
    int sc = tvPitch >= 1900 ? 3 : 2;
    int outW = w * sc, outH = h * sc;
    if (outH > tvRows) outH = tvRows;
    for (int y = 0; y < h && y * sc < outH; y++) {
        uint32_t *dst = tvDraw + (size_t)(y * sc) * tvPitch;
        const uint32_t *row = src + y * w;
        for (int x = 0; x < w; x++) {
            uint32_t c = row[x];
            for (int k = 0; k < sc; k++) dst[x * sc + k] = c;
        }
        for (int k = 1; k < sc && y * sc + k < outH; k++) memcpy(dst + (size_t)k * tvPitch, dst, (size_t)outW * 4);
    }
}

Surface plat_begin_drc(void) {
    drcDraw = probe_draw(SCREEN_DRC, drcBase, drcHalf, drcDraw);
    Surface s = {drcDraw, DRC_W, DRC_H, drcPitch};
    return s;
}

void plat_flip(void) {
    if (!ready) return;
    DCFlushRange(tvDraw, (uint32_t)(tvHalf * 4));
    DCFlushRange(drcDraw, (uint32_t)(drcHalf * 4));
    OSScreenFlipBuffersEx(SCREEN_TV);
    OSScreenFlipBuffersEx(SCREEN_DRC);
}

void plat_shutdown(void) {
    if (ready) {
        OSScreenShutdown();
        VPADShutdown();
    }
    WHBProcShutdown();
}
