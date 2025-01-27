#pragma once

#include <zf4.h>

enum e_sprite_index {
    ek_sprite_index_pixel,
    ek_sprite_index_player,
    ek_sprite_index_red_enemy,
    ek_sprite_index_purple_enemy,
    ek_sprite_index_bullet,
    ek_sprite_index_cursor,

    eks_sprite_cnt
};

struct s_sprites {
    zf4::s_static_array<zf4::s_rect_i, eks_sprite_cnt> src_rects;
};

void InitSprites(s_sprites& sprites);
