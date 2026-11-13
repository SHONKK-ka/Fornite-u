#ifndef PLATFORM_H
#define PLATFORM_H
#include <stdint.h>

// Botones (independientes de la plataforma)
enum {
    BTN_A = 1 << 0, BTN_B = 1 << 1, BTN_X = 1 << 2, BTN_Y = 1 << 3,
    BTN_L = 1 << 4, BTN_R = 1 << 5, BTN_ZL = 1 << 6, BTN_ZR = 1 << 7,
    BTN_UP = 1 << 8, BTN_DOWN = 1 << 9, BTN_LEFT = 1 << 10, BTN_RIGHT = 1 << 11,
    BTN_PLUS = 1 << 12, BTN_MINUS = 1 << 13, BTN_L3 = 1 << 14, BTN_R3 = 1 << 15
};

typedef struct {
    float lx, ly, rx, ry;       // sticks, -1..1 (arriba = positivo)
    uint32_t hold, pressed;     // botones
    int touching, touchPressed; // pantalla tactil del GamePad
    int tx, ty;                 // coordenadas tactiles en pixeles del GamePad (854x480)
} Pad;

typedef struct {
    uint32_t *px;   // pixeles 0xRRGGBB00
    int w, h, pitch; // pitch en pixeles
} Surface;

#define DRC_W 854
#define DRC_H 480

int plat_init(void);
int plat_running(void);
void plat_poll(Pad *pad);
double plat_time(void);                                  // segundos
void plat_present_tv(const uint32_t *src, int w, int h); // sube la imagen 3D a la TV (escala entera)
Surface plat_begin_drc(void);                            // superficie 854x480 del GamePad
void plat_flip(void);                                    // muestra ambas pantallas
void plat_shutdown(void);

#endif
