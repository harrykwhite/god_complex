#pragma once

#include <zf4.h>
#include "sprites.h"
#include "level.h"

enum e_font {
    ek_font_eb_garamond_18,
    ek_font_eb_garamond_28,
    ek_font_eb_garamond_36,
    ek_font_eb_garamond_72
};

enum e_shader_prog {
    ek_shader_prog_blend,
    ek_shader_prog_lighting
};

enum e_render_surface {
    ek_render_surface_level,
    ek_render_surface_blend,

    eks_render_surface_cnt
};

struct s_game {
    s_sprites sprites;
    s_level level;
};

bool InitGame(const zf4::s_game_ptrs& const game_ptrs);
bool GameTick(const zf4::s_game_ptrs& const game_ptrs, const double fps);
bool DrawGame(zf4::s_draw_phase_state& const draw_phase_state, const zf4::s_game_ptrs& const game_ptrs, const double fps);
