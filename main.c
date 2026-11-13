#include "game.h"
#include "platform.h"

int main(void) {
    if (!plat_init()) return 1;
    game_init();
    double last = plat_time();
    while (plat_running()) {
        Pad pad;
        plat_poll(&pad);
        double now = plat_time();
        float dt = (float)(now - last);
        last = now;
        if (dt > 0.05f) dt = 0.05f;
        if (dt < 0.001f) dt = 0.001f;
        game_update(&pad, dt);
        game_render_tv();
        plat_present_tv(g_view.px, RW, RH);
        Surface d = plat_begin_drc();
        game_render_drc(&d, &pad);
        plat_flip();
    }
    plat_shutdown();
    return 0;
}
