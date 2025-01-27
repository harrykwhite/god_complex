#pragma once

#include <zf4.h>
#include "sprites.h"

constexpr int g_enemy_limit = 256;
constexpr int g_projectile_limit = 1024;

struct s_player {
    zf4::s_vec_2d pos;
    zf4::s_vec_2d vel;
    float rot;

    int shoot_cooldown;
};

enum e_enemy_type {
    ek_enemy_type_red,
    ek_enemy_type_purple,

    eks_enemy_type_cnt
};

struct s_enemy {
    zf4::s_vec_2d pos;
    zf4::s_vec_2d vel;

    float rot;

    int hp;

    e_enemy_type type;
};

struct s_projectile {
    zf4::s_vec_2d pos;
    zf4::s_vec_2d vel;
    bool enemy;
};

struct s_level {
    s_player player;

    zf4::s_static_list<s_enemy, g_enemy_limit> enemies;
    int enemy_spawn_time;

    zf4::s_static_list<s_projectile, g_projectile_limit> projectiles;

    zf4::s_vec_2d cam_pos;
};

bool InitLevel(s_level& level, zf4::s_mem_arena& mem_arena);
bool UpdateLevel(s_level& level, const zf4::s_window& window, const s_sprites& sprites, zf4::s_mem_arena& scratch_space);
bool DrawLevel(s_level& level, zf4::s_draw_phase_state& draw_phase_state, const zf4::s_renderer& renderer, const zf4::s_window& window, const s_sprites& sprites, zf4::s_mem_arena& scratch_space);
