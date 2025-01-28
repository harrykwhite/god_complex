#include "level.h"

#include <algorithm>
#include "game.h"

constexpr zf4::s_vec_2d_i i_level_size = {800, 800};

constexpr zf4::s_vec_4d i_bg_color = {0.63f, 0.63f, 0.49f, 1.0f};

constexpr float i_vel_lerp = 0.2f;

constexpr float i_player_move_spd = 3.0f;

constexpr int i_enemy_spawn_interval = 90;
constexpr int i_enemy_spawn_limit = 10;
static_assert(i_enemy_spawn_limit <= g_enemy_limit);

constexpr zf4::s_static_array<e_sprite_index, eks_enemy_type_cnt> i_enemy_type_sprite_indexes = {
    ek_sprite_index_red_enemy,
    ek_sprite_index_purple_enemy
};

constexpr zf4::s_static_array<int, eks_enemy_type_cnt> i_enemy_type_hps = {
    8,
    5
};

constexpr float i_camera_scale = 2.0f;
constexpr float i_camera_pos_lerp = 0.25f;

static zf4::s_rect& LoadColliderFromSprite(const zf4::s_vec_2d pos, const e_sprite_index sprite_index, const s_sprites& sprites) {
    assert(sprite_index >= 0 && sprite_index < eks_sprite_cnt);

    zf4::s_rect collider = {
        .width = (float)sprites.src_rects[sprite_index].width,
        .height = (float)sprites.src_rects[sprite_index].height
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

static zf4::s_static_list<zf4::s_rect, g_enemy_limit> LoadEnemyColliders(const zf4::s_static_list<s_enemy, g_enemy_limit>& enemies, const s_sprites& sprites) {
    zf4::s_static_list<zf4::s_rect, g_enemy_limit> colliders = {
        .len = enemies.len
    };

    for (int i = 0; i < enemies.len; ++i) {
        colliders[i] = LoadColliderFromSprite(enemies[i].pos, i_enemy_type_sprite_indexes[enemies[i].type], sprites);
    }

    return colliders;
}

static s_projectile* SpawnProjectile(const zf4::s_vec_2d pos, const float spd, const float dir, zf4::s_static_list<s_projectile, g_projectile_limit>& projectiles) {
    if (projectiles.len == g_projectile_limit) {
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

bool InitLevel(s_level& level, zf4::s_mem_arena& mem_arena) {
    assert(zf4::IsStructZero(level));

    level.player.pos = i_level_size / 2.0f;
    level.player.hp = 100;
    level.player_active = true;

    level.cam_pos = level.player.pos;

    return true;
}

bool UpdateLevel(s_level& level, const zf4::s_window& window, const s_sprites& sprites, zf4::s_mem_arena& scratch_space) {
    //
    // Player Movement and Invincibility
    //
    if (level.player_active) {
        const zf4::s_vec_2d move_axis = {
            static_cast<float>(zf4::KeyDown(zf4::ek_key_code_d, window.input_state) - zf4::KeyDown(zf4::ek_key_code_a, window.input_state)),
            static_cast<float>(zf4::KeyDown(zf4::ek_key_code_s, window.input_state) - zf4::KeyDown(zf4::ek_key_code_w, window.input_state))
        };
        const zf4::s_vec_2d vel_lerp_targ = move_axis * i_player_move_spd;
        level.player.vel = zf4::Lerp(level.player.vel, vel_lerp_targ, i_vel_lerp);
        level.player.pos += level.player.vel;

        const zf4::s_vec_2d mouse_cam_pos = ScreenToCameraPos(window.input_state.mouse_pos, level.cam_pos, window.size_cache);
        level.player.rot = zf4::Dir(level.player.pos, mouse_cam_pos);

        if (level.player.inv_cooldown > 0) {
            --level.player.inv_cooldown; // NOTE: A cooldown timer of 1 actually corresponds to 0 frames of invincibility - fix?
        }
    }

    //
    // Enemy Movement
    //
    for (int i = 0; i < level.enemies.len; ++i) {
        s_enemy& enemy = level.enemies[i];
        enemy.vel = zf4::Lerp(enemy.vel, {}, i_vel_lerp);
        enemy.pos += enemy.vel;
    }

    //
    // Projectile Movement
    //
    for (int i = 0; i < level.projectiles.len; ++i) {
        s_projectile& projectile = level.projectiles[i];
        projectile.pos += projectile.vel;
    }

    //
    // Player Shooting
    //
    if (level.player_active) {
        if (level.player.shoot_cooldown > 0) {
            --level.player.shoot_cooldown;
        } else {
            if (zf4::MouseButtonDown(zf4::ek_mouse_button_code_left, window.input_state)) {
                SpawnProjectile(level.player.pos, 12.0f, level.player.rot, level.projectiles);
                level.player.shoot_cooldown = 10;
            }
        }
    }

    //
    // Enemy Spawning
    //
    if (level.enemy_spawn_time < i_enemy_spawn_interval) {
        ++level.enemy_spawn_time;
    } else {
        if (level.enemies.len < i_enemy_spawn_limit) {
            const int enemy_index = level.enemies.len;
            ++level.enemies.len;

            s_enemy& enemy = level.enemies[enemy_index];
            assert(zf4::IsStructZero(enemy));

            enemy.pos = {
                zf4::RandFloat(0.0f, (float)i_level_size.x),
                zf4::RandFloat(0.0f, (float)i_level_size.y)
            };

            enemy.type = zf4::RandPerc() < 0.7f ? ek_enemy_type_red : ek_enemy_type_purple;

            enemy.hp = i_enemy_type_hps[enemy.type];
        } else {
            zf4::LogError("Failed to spawn enemy as the enemy list is full!");
        }

        level.enemy_spawn_time = 0;
    }

    //
    // Collision Processing
    //
    {
        const zf4::s_rect player_collider = LoadColliderFromSprite(level.player.pos, ek_sprite_index_player, sprites);
        const auto enemy_colliders = LoadEnemyColliders(level.enemies, sprites);

        // Handle the player colliding with enemies.
        if (level.player.inv_cooldown == 0) {
            for (int i = 0; i < enemy_colliders.len; ++i) {
                if (zf4::DoRectsIntersect(player_collider, enemy_colliders[i])) {
                    const zf4::s_vec_2d kb = CalcKnockback(level.player.pos, level.enemies[i].pos, 8.0f);
                    HurtPlayer(level.player, 1, kb);
                    break;
                }
            }
        }

        // Handle projectiles colliding with the player or enemies.
        {
            int proj_index = 0;

            while (proj_index < level.projectiles.len) {
                s_projectile& proj = level.projectiles[proj_index];
                const zf4::s_rect proj_collider = LoadColliderFromSprite(proj.pos, ek_sprite_index_bullet, sprites);
                const zf4::s_vec_2d proj_knockback = proj.vel * 0.6f;

                bool destroy = false;

                if (proj.enemy) {
                    if (level.player.inv_cooldown == 0 && zf4::DoRectsIntersect(proj_collider, player_collider)) {
                        HurtPlayer(level.player, 1, proj_knockback);
                        destroy = true;
                    }
                } else {
                    for (int i = 0; i < enemy_colliders.len; ++i) {
                        if (zf4::DoRectsIntersect(proj_collider, enemy_colliders[i])) {
                            s_enemy& enemy = level.enemies[i];
                            enemy.vel += proj_knockback;
                            --enemy.hp;

                            destroy = true;

                            break;
                        }
                    }
                }

                if (destroy) {
                    s_projectile& end_proj = level.projectiles[level.projectiles.len - 1];
                    proj = end_proj;
                    zf4::ZeroOutStruct(end_proj);
                    --level.projectiles.len;
                } else {
                    ++proj_index;
                }
            }
        }
    }

    //
    // Process Player Death
    //
    if (level.player.hp <= 0) {
        level.player_active = false;
    }

    //
    // Processing Enemy Deaths
    //
    {
        int enemy_index = 0;

        while (enemy_index < level.enemies.len) {
            const s_enemy& enemy = level.enemies[enemy_index];

            if (enemy.hp <= 0) {
                s_enemy& end_enemy = level.enemies[level.enemies.len - 1];
                level.enemies[enemy_index] = end_enemy;
                zf4::ZeroOutStruct(end_enemy);
                --level.enemies.len;
            } else {
                ++enemy_index;
            }
        }
    }

    //
    // Camera
    //
    {
        const zf4::s_vec_2d dest = level.player.pos; // We do this even if the player is inactive.
        level.cam_pos = Lerp(level.cam_pos, dest, i_camera_pos_lerp);
    }

    return true;
}

bool DrawLevel(s_level& level, zf4::s_draw_phase_state& draw_phase_state, const zf4::s_renderer& renderer, const zf4::s_window& window, const s_sprites& sprites, zf4::s_mem_arena& scratch_space) {
    zf4::RenderClear(i_bg_color);

    zf4::ZeroOutStruct(draw_phase_state.view_mat);
    draw_phase_state.view_mat = LoadCameraViewMatrix4x4(level.cam_pos, window.size_cache);

    // Draw enemies.
    for (int i = 0; i < level.enemies.len; ++i) {
        const s_enemy& enemy = level.enemies[i];
        zf4::SubmitTextureToRenderBatch(0, sprites.src_rects[i_enemy_type_sprite_indexes[enemy.type]], enemy.pos, draw_phase_state, renderer, {0.5f, 0.5f}, {1.0f, 1.0f}, enemy.rot);
    }

    // Draw the player.
    if (level.player_active) {
        zf4::SubmitTextureToRenderBatch(0, sprites.src_rects[ek_sprite_index_player], level.player.pos, draw_phase_state, renderer, {0.5f, 0.5f}, {1.0f, 1.0f}, level.player.rot);
    }

    // Draw projectiles.
    for (int i = 0; i < level.projectiles.len; ++i) {
        const s_projectile& proj = level.projectiles[i];
        zf4::SubmitTextureToRenderBatch(0, sprites.src_rects[ek_sprite_index_bullet], proj.pos, draw_phase_state, renderer);
    }

    zf4::FlushTextureBatch(draw_phase_state, renderer);

    return true;
}
