#include <cstdio>
#include <zf4.h>

static constexpr zf4::s_vec_4d i_bg_color = {0.92f, 0.56f, 0.38f, 1.0f};

static constexpr float i_vel_lerp = 0.2f;

static constexpr float i_player_move_spd = 3.0f;

static constexpr int i_proj_limit = 1024;

static constexpr float i_camera_scale = 2.0f;
static constexpr float i_camera_pos_lerp = 0.25f;
static constexpr float i_camera_look_dist_limit = 24.0f;
static constexpr float i_camera_look_dist_scalar_dist = i_camera_look_dist_limit * 32.0f;

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

    eks_render_surface_cnt
};

enum e_sprite_index {
    ek_sprite_index_pixel,
    ek_sprite_index_player,
    ek_sprite_index_bullet,
    ek_sprite_index_cursor,

    eks_sprite_cnt
};

static constexpr zf4::s_static_array<zf4::s_rect_i, eks_sprite_cnt> i_sprite_src_rects = {
    .elems_raw = {
        {0, 0, 1, 1},
        {8, 0, 24, 40},
        {2, 18, 4, 4},
        {0, 8, 8, 8}
    }
};

struct s_player {
    zf4::s_vec_2d pos;
    zf4::s_vec_2d vel;
};

struct s_projectile {
    zf4::s_vec_2d pos;
    zf4::s_vec_2d vel;
};

struct s_camera {
    zf4::s_vec_2d pos;
};

struct s_game {
    s_player player;
    zf4::s_static_list<s_projectile, i_proj_limit> projectiles;
    s_camera cam;
};

static inline zf4::s_vec_2d CameraSize(const zf4::s_vec_2d_i window_size) {
    return {static_cast<float>(window_size.x) / i_camera_scale, static_cast<float>(window_size.y) / i_camera_scale};
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

static bool InitGame(const zf4::s_game_ptrs& game_ptrs) {
    const auto game = static_cast<s_game*>(game_ptrs.custom_data);

    if (!InitRenderSurfaces(eks_render_surface_cnt, game_ptrs.renderer.surfs, game_ptrs.window.size_cache)) {
        return false;
    }

    return true;
}

static bool GameTick(const zf4::s_game_ptrs& game_ptrs, const double fps) {
    const auto game = static_cast<s_game*>(game_ptrs.custom_data);

    //
    // Player
    //
    {
        const zf4::s_vec_2d move_axis = {
            static_cast<float>(zf4::KeyDown(zf4::ek_key_code_d, game_ptrs.window.input_state) - zf4::KeyDown(zf4::ek_key_code_a, game_ptrs.window.input_state)),
            static_cast<float>(zf4::KeyDown(zf4::ek_key_code_s, game_ptrs.window.input_state) - zf4::KeyDown(zf4::ek_key_code_w, game_ptrs.window.input_state))
        };

        const zf4::s_vec_2d vel_lerp_targ = (move_axis != zf4::s_vec_2d() ? zf4::Normal(move_axis) : zf4::s_vec_2d()) * i_player_move_spd;
        game->player.vel = zf4::Lerp(game->player.vel, vel_lerp_targ, i_vel_lerp);

        game->player.pos += game->player.vel;
    }

    //
    // Projectiles
    //
    for (int i = 0; i < game->projectiles.len; ++i) {
        s_projectile& proj = game->projectiles[i];
        proj.pos += proj.vel;
    }

    //
    // Player Shooting
    //
    if (zf4::MouseButtonPressed(zf4::ek_mouse_button_code_left, game_ptrs.window.input_state, game_ptrs.window.input_state_saved)) {
        if (game->projectiles.len < i_proj_limit) {
            s_projectile& proj = game->projectiles[game->projectiles.len++];
            proj.pos = game->player.pos;
            proj.vel = zf4::Normal(ScreenToCameraPos(game_ptrs.window.input_state.mouse_pos, game->cam.pos, game_ptrs.window.size_cache) - game->player.pos) * 11.0f;
        }
    }

    //
    // Camera
    //
    {
        const zf4::s_vec_2d mouse_cam_pos = ScreenToCameraPos(game_ptrs.window.input_state.mouse_pos, game->cam.pos, game_ptrs.window.size_cache);
        const float player_to_mouse_cam_pos_dist = Dist(game->player.pos, mouse_cam_pos);
        const zf4::s_vec_2d player_to_mouse_cam_pos_dir = zf4::Normal(mouse_cam_pos - game->player.pos);

        const float look_dist = i_camera_look_dist_limit * zf4::Min(player_to_mouse_cam_pos_dist / i_camera_look_dist_scalar_dist, 1.0f);
        const zf4::s_vec_2d look_offs = player_to_mouse_cam_pos_dir * look_dist;

        const zf4::s_vec_2d dest = game->player.pos + look_offs;

        game->cam.pos = Lerp(game->cam.pos, dest, i_camera_pos_lerp);
    }

    return true;
}

static bool DrawGame(zf4::s_draw_phase_state& draw_phase_state, const zf4::s_game_ptrs& game_ptrs, const double fps) {
    const auto game = static_cast<s_game*>(game_ptrs.custom_data);

    //
    // Level
    //
    zf4::RenderClear(i_bg_color);

    zf4::ZeroOutStruct(draw_phase_state.view_mat);
    draw_phase_state.view_mat = LoadCameraViewMatrix4x4(game->cam.pos, game_ptrs.window.size_cache);

    zf4::SubmitTextureToRenderBatch(0, i_sprite_src_rects[ek_sprite_index_player], game->player.pos, draw_phase_state, game_ptrs.renderer);

    for (int i = 0; i < game->projectiles.len; ++i) {
        const s_projectile& proj = game->projectiles[i];
        zf4::SubmitTextureToRenderBatch(0, i_sprite_src_rects[ek_sprite_index_bullet], proj.pos, draw_phase_state, game_ptrs.renderer);
    }

    zf4::FlushTextureBatch(draw_phase_state, game_ptrs.renderer);

    //
    // UI
    //
    zf4::ZeroOutStruct(draw_phase_state.view_mat);
    zf4::InitIdentityMatrix4x4(draw_phase_state.view_mat);

    // Draw FPS.
    char fps_str[20] = {};
    std::snprintf(fps_str, sizeof(fps_str), "FPS: %.2f", fps);
    SubmitStrToRenderBatch(fps_str, 0, {10.0f, 10.0f}, zf4::colors::g_white, zf4::ek_str_hor_align_left, zf4::ek_str_ver_align_top, draw_phase_state, game_ptrs.renderer);

    // Draw the cursor.
    zf4::SubmitTextureToRenderBatch(0, i_sprite_src_rects[ek_sprite_index_cursor], game_ptrs.window.input_state.mouse_pos, draw_phase_state, game_ptrs.renderer);

    zf4::FlushTextureBatch(draw_phase_state, game_ptrs.renderer);

    return true;
}

static void LoadGameInfo(zf4::s_game_info* const info) {
    info->init_func = InitGame;
    info->tick_func = GameTick;
    info->draw_func = DrawGame;

    info->window_title = "Wasteland";
    info->window_flags = (zf4::e_window_flags)(zf4::ek_window_flags_hide_cursor | zf4::ek_window_flags_resizable);

    info->custom_data_size = sizeof(s_game);
    info->custom_data_alignment = alignof(s_game);
}

int main(void) {
    return RunGame(LoadGameInfo) ? EXIT_SUCCESS : EXIT_FAILURE;
}
