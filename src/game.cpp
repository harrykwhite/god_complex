#include "game.h"

#include <cstdio>

bool InitGame(const zf4::s_game_ptrs& game_ptrs) {
    const auto game = static_cast<s_game*>(game_ptrs.custom_data);

    InitSprites(game->sprites);

    for (int i = 0; i < eks_sprite_cnt; ++i) {
        assert(!IsStructZero(game->sprites.src_rects[i]));
    }

    if (!InitRenderSurfaces(eks_render_surface_cnt, game_ptrs.renderer.surfs, game_ptrs.window.size_cache)) {
        return false;
    }

    if (!InitLevel(game->level, game_ptrs.perm_mem_arena)) {
        return false;
    }

    return true;
}

bool GameTick(const zf4::s_game_ptrs& game_ptrs, const double fps) {
    const auto game = static_cast<s_game*>(game_ptrs.custom_data);

    if (!UpdateLevel(game->level, game_ptrs.window, game->sprites, game_ptrs.temp_mem_arena)) {
        return false;
    }

    return true;
}

bool DrawGame(zf4::s_draw_phase_state& draw_phase_state, const zf4::s_game_ptrs& game_ptrs, const double fps) {
    const auto game = static_cast<s_game*>(game_ptrs.custom_data);

    zf4::RenderClear();

    if (!DrawLevel(game->level, draw_phase_state, game_ptrs.renderer, game_ptrs.window, game->sprites, game_ptrs.temp_mem_arena)) {
        return false;
    }

    zf4::ZeroOutStruct(draw_phase_state.view_mat);
    zf4::InitIdentityMatrix4x4(draw_phase_state.view_mat);

    char fps_str[20] = {};
    snprintf(fps_str, sizeof(fps_str), "FPS: %.2f", fps);
    SubmitStrToRenderBatch(fps_str, 0, {10.0f, 10.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, zf4::ek_str_hor_align_left, zf4::ek_str_ver_align_top, draw_phase_state, game_ptrs.renderer);

    zf4::SubmitTextureToRenderBatch(0, game->sprites.src_rects[ek_sprite_index_cursor], game_ptrs.window.input_state.mouse_pos, draw_phase_state, game_ptrs.renderer, {0.5f, 0.5f}, {2.0f, 2.0f});

    zf4::FlushTextureBatch(draw_phase_state, game_ptrs.renderer);

    return true;
}
