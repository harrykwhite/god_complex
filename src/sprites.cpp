#include "sprites.h"

void InitSprites(s_sprites& sprites) {
    assert(zf4::IsStructZero(sprites));

    sprites.src_rects[ek_sprite_index_pixel] = {0, 0, 1, 1};
    sprites.src_rects[ek_sprite_index_player] = {10, 2, 28, 28};
    sprites.src_rects[ek_sprite_index_red_enemy] = {0, 40, 24, 24};
    sprites.src_rects[ek_sprite_index_purple_enemy] = {24, 40, 16, 16};
    sprites.src_rects[ek_sprite_index_bullet] = {42, 10, 4, 4};
    sprites.src_rects[ek_sprite_index_cursor] = {0, 8, 8, 8};

    // Make sure all sprites are initialised.
    for (int i = 0; i < eks_sprite_cnt; ++i) {
        assert(!zf4::IsStructZero(sprites.src_rects[i]));
    }
}
