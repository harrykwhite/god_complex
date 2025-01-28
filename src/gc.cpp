#include <cstdio>
#include <zf4.h>

static constexpr zf4::s_vec_4d i_bg_color = {0.63f, 0.63f, 0.49f, 1.0f};

static constexpr float i_vel_lerp = 0.2f;

static constexpr float i_player_move_spd = 3.0f;

static constexpr int i_enemy_limit = 256;
static constexpr int i_enemy_spawn_interval = 90;
static constexpr int i_enemy_spawn_limit = 10;
static_assert(i_enemy_spawn_limit <= i_enemy_limit);

static constexpr int i_projectile_limit = 1024;

static constexpr float i_camera_scale = 2.0f;
static constexpr float i_camera_pos_lerp = 0.25f;

static constexpr int i_tile_size = 16;
static constexpr zf4::s_vec_2d_i i_tilemap_size = {40, 40};
static constexpr int i_tilemap_tile_cnt = i_tilemap_size.x * i_tilemap_size.y;

static constexpr zf4::s_vec_2d_i i_level_size = i_tilemap_size * i_tile_size;

enum e_sprite_index {
    ek_sprite_index_pixel,
    ek_sprite_index_player,
    ek_sprite_index_red_enemy,
    ek_sprite_index_purple_enemy,
    ek_sprite_index_bullet,
    ek_sprite_index_tile,
    ek_sprite_index_cursor,

    eks_sprite_cnt
};

static constexpr zf4::s_static_array<zf4::s_rect_i, eks_sprite_cnt> i_sprite_src_rects = {
    .elems_raw = {
        {0, 0, 1, 1},
        {10, 2, 28, 28},
        {0, 40, 24, 24},
        {24, 40, 16, 16},
        {42, 10, 4, 4},
        {40, 16, 16, 16},
        {0, 8, 8, 8}
    }
};

struct s_player {
    zf4::s_vec_2d pos;
    zf4::s_vec_2d vel;

    float rot;

    int hp;
    int inv_cooldown;

    int shoot_cooldown;
};

enum e_enemy_type {
    ek_enemy_type_red,
    ek_enemy_type_purple,

    eks_enemy_type_cnt
};

static constexpr zf4::s_static_array<e_sprite_index, eks_enemy_type_cnt> i_enemy_type_sprite_indexes = {
    ek_sprite_index_red_enemy,
    ek_sprite_index_purple_enemy
};

static constexpr zf4::s_static_array<int, eks_enemy_type_cnt> i_enemy_type_hps = {
    8,
    5
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

struct s_tilemap {
    zf4::s_static_array<zf4::a_byte, zf4::BitsToBytes(i_tilemap_tile_cnt)> activity;
};

struct s_game {
    s_player player;
    bool player_active;

    zf4::s_static_list<s_enemy, i_enemy_limit> enemies;
    int enemy_spawn_time;

    zf4::s_static_list<s_projectile, i_projectile_limit> projectiles;

    zf4::s_vec_2d cam_pos;

    s_tilemap tilemap;
};

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

static zf4::s_rect& LoadColliderFromSprite(const zf4::s_vec_2d pos, const e_sprite_index sprite_index) {
    assert(sprite_index >= 0 && sprite_index < eks_sprite_cnt);

    zf4::s_rect collider = {
        .width = (float)i_sprite_src_rects[sprite_index].width,
        .height = (float)i_sprite_src_rects[sprite_index].height
    };

    // NOTE: At the moment we assume a centered origin.
    collider.x = pos.x - (collider.width / 2.0f);
    collider.y = pos.y - (collider.height / 2.0f);

    return collider;
}

static zf4::s_vec_2d CalcKnockback(const zf4::s_vec_2d pos, const zf4::s_vec_2d inflictor_pos, const float strength) {
    // TODO: Handle different knockback types.

    if (pos == inflictor_pos) {
        return {};
    }

    const zf4::s_vec_2d kb_dir = Normal(pos - inflictor_pos);
    return kb_dir * strength;
}

static void HurtPlayer(s_player& player, const int dmg, const zf4::s_vec_2d knockback) {
    assert(player.inv_cooldown == 0);
    assert(dmg > 0);

    player.vel += knockback;
    player.hp -= dmg;
    player.inv_cooldown = 20;
}

static zf4::s_rect GenEnemyCollider(const zf4::s_vec_2d pos, const e_enemy_type type) {
    assert(type >= 0 && type < eks_enemy_type_cnt);
    return LoadColliderFromSprite(pos, i_enemy_type_sprite_indexes[type]);
}

static zf4::s_static_list<zf4::s_rect, i_enemy_limit> LoadEnemyColliders(const zf4::s_static_list<s_enemy, i_enemy_limit>& enemies) {
    zf4::s_static_list<zf4::s_rect, i_enemy_limit> colliders = {
        .len = enemies.len
    };

    for (int i = 0; i < enemies.len; ++i) {
        colliders[i] = LoadColliderFromSprite(enemies[i].pos, i_enemy_type_sprite_indexes[enemies[i].type]);
    }

    return colliders;
}

static s_projectile* SpawnProjectile(const zf4::s_vec_2d pos, const float spd, const float dir, zf4::s_static_list<s_projectile, i_projectile_limit>& projectiles) {
    if (projectiles.len == i_projectile_limit) {
        return nullptr;
    }

    const int index = projectiles.len;
    ++projectiles.len;

    s_projectile& proj = projectiles[index];
    assert(zf4::IsStructZero(proj));
    proj.pos = pos;
    proj.vel = zf4::Dir(dir) * spd;

    return &proj;
}

static inline zf4::s_vec_2d CameraSize(const zf4::s_vec_2d_i window_size) {
    return {(float)window_size.x / i_camera_scale, (float)window_size.y / i_camera_scale};
}

static inline zf4::s_vec_2d CameraTopLeft(const zf4::s_vec_2d cam_pos, const zf4::s_vec_2d_i window_size) {
    const zf4::s_vec_2d size = CameraSize(window_size);
    return cam_pos - (size / 2.0f);
}

static inline zf4::s_vec_2d CameraToScreenPos(const zf4::s_vec_2d pos, const zf4::s_vec_2d cam_pos, const zf4::s_vec_2d_i window_size) {
    const zf4::s_vec_2d top_left = CameraTopLeft(cam_pos, window_size);
    return (pos - top_left) * i_camera_scale;
}

static inline zf4::s_vec_2d ScreenToCameraPos(const zf4::s_vec_2d pos, const zf4::s_vec_2d cam_pos, const zf4::s_vec_2d_i window_size) {
    const zf4::s_vec_2d top_left = CameraTopLeft(cam_pos, window_size);
    return top_left + (pos * (1.0f / i_camera_scale));
}

static zf4::s_matrix_4x4 LoadCameraViewMatrix4x4(const zf4::s_vec_2d cam_pos, const zf4::s_vec_2d_i window_size) {
    zf4::s_matrix_4x4 mat = {};

    mat.elems[0][0] = i_camera_scale;
    mat.elems[1][1] = i_camera_scale;
    mat.elems[3][3] = 1.0f;
    mat.elems[3][0] = (-cam_pos.x * i_camera_scale) + (window_size.x / 2.0f);
    mat.elems[3][1] = (-cam_pos.y * i_camera_scale) + (window_size.y / 2.0f);

    return mat;
}

static inline bool IsTilePosWithinBounds(const int x, const int y) {
    return x >= 0 && y >= 0 && x < i_tilemap_size.x && y < i_tilemap_size.y;
}

static inline zf4::s_vec_2d TileToLevelPos(const int tx, const int ty) {
    assert(IsTilePosWithinBounds(tx, ty));
    return {(float)(tx * i_tile_size), (float)(ty * i_tile_size)};
}

static inline int TileIndex(const int x, const int y) {
    return (y * i_tilemap_size.x) + x;
}

static inline void ActivateTile(const int x, const int y, s_tilemap& tilemap) {
    assert(IsTilePosWithinBounds(x, y));
    zf4::ActivateBit(TileIndex(x, y), zf4::StaticArrayToArray(tilemap.activity), i_tilemap_tile_cnt);
}

static inline bool IsTileActive(const int x, const int y, const s_tilemap& tilemap) {
    assert(IsTilePosWithinBounds(x, y));
    return zf4::IsBitActive(TileIndex(x, y), zf4::StaticArrayToArray(tilemap.activity), i_tilemap_tile_cnt);
}

static zf4::s_rect LoadTileCollider(const int tx, const int ty) {
    const zf4::s_vec_2d level_pos = TileToLevelPos(tx, ty);
    return {level_pos.x, level_pos.y, level_pos.x + i_tile_size, level_pos.y + i_tile_size};
}

static bool TileCollisionCheck(const zf4::s_rect collider, const s_tilemap& tilemap) {
    const int tx_begin = zf4::Clamp((int)floorf(collider.x / i_tile_size), 0, i_tilemap_size.x - 1);
    const int ty_begin = zf4::Clamp((int)floorf(collider.y / i_tile_size), 0, i_tilemap_size.y - 1);

    const int tx_end = zf4::Clamp((int)ceilf(RectRight(collider) / i_tile_size), 0, i_tilemap_size.x);
    const int ty_end = zf4::Clamp((int)ceilf(RectBottom(collider) / i_tile_size), 0, i_tilemap_size.y);

    for (int ty = ty_begin; ty < ty_end; ++ty) {
        for (int tx = tx_begin; tx < tx_end; ++tx) {
            if (IsTileActive(tx, ty, tilemap)) {
                return true;
            }
        }
    }

    return false;
}

static bool ProcTileCollisions(zf4::s_vec_2d& vel, const zf4::s_rect collider, const s_tilemap& tilemap) {
    const zf4::s_rect hor_collider = RectTranslated(collider, {vel.x, 0.0f});

    if (TileCollisionCheck(hor_collider, tilemap)) {
        vel.x = 0.0f;
    }

    const zf4::s_rect ver_collider = RectTranslated(collider, {0.0f, vel.y});

    if (TileCollisionCheck(ver_collider, tilemap)) {
        vel.y = 0.0f;
    }

    // TODO: Handle diagonal collisions.

    return true;
}

static bool InitGame(const zf4::s_game_ptrs& game_ptrs) {
    const auto game = static_cast<s_game*>(game_ptrs.custom_data);

    if (!InitRenderSurfaces(eks_render_surface_cnt, game_ptrs.renderer.surfs, game_ptrs.window.size_cache)) {
        return false;
    }

    game->player.pos = i_level_size / 2.0f;
    game->player.hp = 100;
    game->player_active = true;

    game->cam_pos = game->player.pos;

    for (int x = 0; x < i_tilemap_size.x; ++x) {
        ActivateTile(x, 0, game->tilemap);
        ActivateTile(x, i_tilemap_size.y - 1, game->tilemap);
    }

    for (int y = 0; y < i_tilemap_size.y; ++y) {
        ActivateTile(0, y, game->tilemap);
        ActivateTile(i_tilemap_size.x - 1, y, game->tilemap);
    }

    return true;
}

static bool GameTick(const zf4::s_game_ptrs& game_ptrs, const double fps) {
    const auto game = static_cast<s_game*>(game_ptrs.custom_data);

    //
    // Player Movement and Invincibility
    //
    if (game->player_active) {
        const zf4::s_vec_2d move_axis = {
            static_cast<float>(zf4::KeyDown(zf4::ek_key_code_d, game_ptrs.window.input_state) - zf4::KeyDown(zf4::ek_key_code_a, game_ptrs.window.input_state)),
            static_cast<float>(zf4::KeyDown(zf4::ek_key_code_s, game_ptrs.window.input_state) - zf4::KeyDown(zf4::ek_key_code_w, game_ptrs.window.input_state))
        };
        const zf4::s_vec_2d vel_lerp_targ = move_axis * i_player_move_spd;
        game->player.vel = zf4::Lerp(game->player.vel, vel_lerp_targ, i_vel_lerp);
        ProcTileCollisions(game->player.vel, LoadColliderFromSprite(game->player.pos, ek_sprite_index_player), game->tilemap);
        game->player.pos += game->player.vel;

        const zf4::s_vec_2d mouse_cam_pos = ScreenToCameraPos(game_ptrs.window.input_state.mouse_pos, game->cam_pos, game_ptrs.window.size_cache);
        game->player.rot = zf4::Dir(game->player.pos, mouse_cam_pos);

        if (game->player.inv_cooldown > 0) {
            --game->player.inv_cooldown; // NOTE: A cooldown timer of 1 actually corresponds to 0 frames of invincibility - fix?
        }
    }

    //
    // Enemy Movement
    //
    for (int i = 0; i < game->enemies.len; ++i) {
        s_enemy& enemy = game->enemies[i];
        enemy.vel = zf4::Lerp(enemy.vel, {}, i_vel_lerp);
        ProcTileCollisions(enemy.vel, LoadColliderFromSprite(enemy.pos, i_enemy_type_sprite_indexes[enemy.type]), game->tilemap);
        enemy.pos += enemy.vel;
    }

    //
    // Projectile Movement
    //
    for (int i = 0; i < game->projectiles.len; ++i) {
        s_projectile& projectile = game->projectiles[i];
        projectile.pos += projectile.vel;
    }

    //
    // Player Shooting
    //
    if (game->player_active) {
        if (game->player.shoot_cooldown > 0) {
            --game->player.shoot_cooldown;
        } else {
            if (zf4::MouseButtonDown(zf4::ek_mouse_button_code_left, game_ptrs.window.input_state)) {
                SpawnProjectile(game->player.pos, 12.0f, game->player.rot, game->projectiles);
                game->player.shoot_cooldown = 10;
            }
        }
    }

    //
    // Enemy Spawning
    //
    if (game->enemy_spawn_time < i_enemy_spawn_interval) {
        ++game->enemy_spawn_time;
    } else {
        if (game->enemies.len < i_enemy_spawn_limit) {
            const int enemy_index = game->enemies.len;
            ++game->enemies.len;

            s_enemy& enemy = game->enemies[enemy_index];
            assert(zf4::IsStructZero(enemy));

            enemy.type = zf4::RandPerc() < 0.7f ? ek_enemy_type_red : ek_enemy_type_purple;

            do {
                enemy.pos = {
                    zf4::RandFloat(0.0f, i_level_size.x),
                    zf4::RandFloat(0.0f, i_level_size.y)
                };
            } while (TileCollisionCheck(GenEnemyCollider(enemy.pos, enemy.type), game->tilemap));

            enemy.hp = i_enemy_type_hps[enemy.type];
        }

        game->enemy_spawn_time = 0;
    }

    //
    // Collision Processing
    //
    {
        const zf4::s_rect player_collider = LoadColliderFromSprite(game->player.pos, ek_sprite_index_player);
        const auto enemy_colliders = LoadEnemyColliders(game->enemies);

        // Handle the player colliding with enemies.
        if (game->player.inv_cooldown == 0) {
            for (int i = 0; i < enemy_colliders.len; ++i) {
                if (zf4::DoRectsIntersect(player_collider, enemy_colliders[i])) {
                    const zf4::s_vec_2d kb = CalcKnockback(game->player.pos, game->enemies[i].pos, 8.0f);
                    HurtPlayer(game->player, 1, kb);
                    break;
                }
            }
        }

        // Handle projectiles colliding with the player or enemies.
        {
            int proj_index = 0;

            while (proj_index < game->projectiles.len) {
                s_projectile& proj = game->projectiles[proj_index];
                const zf4::s_rect proj_collider = LoadColliderFromSprite(proj.pos, ek_sprite_index_bullet);
                const zf4::s_vec_2d proj_knockback = proj.vel * 0.6f;

                bool destroy = false;

                if (proj.enemy) {
                    if (game->player.inv_cooldown == 0 && zf4::DoRectsIntersect(proj_collider, player_collider)) {
                        HurtPlayer(game->player, 1, proj_knockback);
                        destroy = true;
                    }
                } else {
                    for (int i = 0; i < enemy_colliders.len; ++i) {
                        if (zf4::DoRectsIntersect(proj_collider, enemy_colliders[i])) {
                            s_enemy& enemy = game->enemies[i];
                            enemy.vel += proj_knockback;
                            --enemy.hp;

                            destroy = true;

                            break;
                        }
                    }
                }

                if (TileCollisionCheck(proj_collider, game->tilemap)) {
                    destroy = true;
                }

                if (destroy) {
                    s_projectile& end_proj = game->projectiles[game->projectiles.len - 1];
                    proj = end_proj;
                    zf4::ZeroOutStruct(end_proj);
                    --game->projectiles.len;
                } else {
                    ++proj_index;
                }
            }
        }
    }

    //
    // Process Player Death
    //
    if (game->player.hp <= 0) {
        game->player_active = false;
    }

    //
    // Processing Enemy Deaths
    //
    {
        int enemy_index = 0;

        while (enemy_index < game->enemies.len) {
            const s_enemy& enemy = game->enemies[enemy_index];

            if (enemy.hp <= 0) {
                s_enemy& end_enemy = game->enemies[game->enemies.len - 1];
                game->enemies[enemy_index] = end_enemy;
                zf4::ZeroOutStruct(end_enemy);
                --game->enemies.len;
            } else {
                ++enemy_index;
            }
        }
    }

    //
    // Camera
    //
    {
        const zf4::s_vec_2d dest = game->player.pos; // We do this even if the player is inactive.
        game->cam_pos = Lerp(game->cam_pos, dest, i_camera_pos_lerp);
    }

    return true;
}

static bool DrawGame(zf4::s_draw_phase_state& draw_phase_state, const zf4::s_game_ptrs& game_ptrs, const double fps) {
    const auto game = static_cast<s_game*>(game_ptrs.custom_data);

    zf4::RenderClear(i_bg_color);

    //
    // Level
    //
    zf4::ZeroOutStruct(draw_phase_state.view_mat);
    draw_phase_state.view_mat = LoadCameraViewMatrix4x4(game->cam_pos, game_ptrs.window.size_cache);

    // Draw enemies.
    for (int i = 0; i < game->enemies.len; ++i) {
        const s_enemy& enemy = game->enemies[i];
        zf4::SubmitTextureToRenderBatch(0, i_sprite_src_rects[i_enemy_type_sprite_indexes[enemy.type]], enemy.pos, draw_phase_state, game_ptrs.renderer, {0.5f, 0.5f}, {1.0f, 1.0f}, enemy.rot);
    }

    // Draw the player.
    if (game->player_active) {
        zf4::SubmitTextureToRenderBatch(0, i_sprite_src_rects[ek_sprite_index_player], game->player.pos, draw_phase_state, game_ptrs.renderer, {0.5f, 0.5f}, {1.0f, 1.0f}, game->player.rot);
    }

    // Draw projectiles.
    for (int i = 0; i < game->projectiles.len; ++i) {
        const s_projectile& proj = game->projectiles[i];
        zf4::SubmitTextureToRenderBatch(0, i_sprite_src_rects[ek_sprite_index_bullet], proj.pos, draw_phase_state, game_ptrs.renderer);
    }

    // Draw tiles.
    for (int y = 0; y < i_tilemap_size.y; ++y) {
        for (int x = 0; x < i_tilemap_size.x; ++x) {
            if (IsTileActive(x, y, game->tilemap)) {
                const zf4::s_vec_2d pos = TileToLevelPos(x, y);
                zf4::SubmitTextureToRenderBatch(0, i_sprite_src_rects[ek_sprite_index_tile], pos, draw_phase_state, game_ptrs.renderer, {});
            }
        }
    }

    zf4::FlushTextureBatch(draw_phase_state, game_ptrs.renderer);

    //
    // UI
    //
    zf4::ZeroOutStruct(draw_phase_state.view_mat);
    zf4::InitIdentityMatrix4x4(draw_phase_state.view_mat);

    char fps_str[20] = {};
    std::snprintf(fps_str, sizeof(fps_str), "FPS: %.2f", fps);
    SubmitStrToRenderBatch(fps_str, 0, {10.0f, 10.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, zf4::ek_str_hor_align_left, zf4::ek_str_ver_align_top, draw_phase_state, game_ptrs.renderer);

    zf4::SubmitTextureToRenderBatch(0, i_sprite_src_rects[ek_sprite_index_cursor], game_ptrs.window.input_state.mouse_pos, draw_phase_state, game_ptrs.renderer, {0.5f, 0.5f}, {2.0f, 2.0f});

    zf4::FlushTextureBatch(draw_phase_state, game_ptrs.renderer);

    return true;
}

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
