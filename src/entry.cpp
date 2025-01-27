#include <zf4.h>
#include "game.h"

static void LoadGameInfo(zf4::s_game_info* const info) {
    info->init_func = InitGame;
    info->tick_func = GameTick;
    info->draw_func = DrawGame;

    info->window_title = "God Complex";
    info->window_flags = (zf4::e_window_flags)(zf4::ek_window_flags_hide_cursor | zf4::ek_window_flags_resizable);

    info->custom_data_size = sizeof(s_game);
    info->custom_data_alignment = alignof(s_game);
}

int main(void) {
    return RunGame(LoadGameInfo) ? EXIT_SUCCESS : EXIT_FAILURE;
}
