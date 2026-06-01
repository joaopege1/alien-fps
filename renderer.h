#ifndef _RENDERER_H_
#define _RENDERER_H_

#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <glm/glm.hpp>
#include <string>
#include "player.h"
#include "mesh.h"

const float fov = 60 * M_PI / 180.0;

const SDL_Color ttf_color_white = {255, 255, 255, 255};
const SDL_Color ttf_color_banana = {209, 182, 6, 255};
const SDL_Color ttf_color_red = {255, 0, 0, 255};

class Renderer
{
    public:
        Renderer(Player* p, Map* ma, Menu* me);
        bool init_sdl(const char* title, ushort width, ushort height);
        void draw(uint fps);
        ~Renderer();

        Renderer(const Renderer& r) = delete;
        Renderer& operator=(const Renderer& r) = delete;

        //called on SDL_WINDOWEVENT_SIZE_CHANGED so UI re-anchors and the viewport
        //matches the new framebuffer size
        void on_window_resize(int new_w, int new_h);

    private:
        //--- init helpers ---
        bool init_gl_resources();
        bool init_2d_resources();
        bool init_3d_extras();
        bool init_alien_resources();
        void build_wall_mesh();
        void build_floor_mesh();
        void draw_aliens_3d(const glm::mat4& view, const glm::mat4& proj);
        GLuint load_bmp_texture(const char* path, int* out_tile_count);
        GLuint compile_shader(GLenum type, const char* src);
        GLuint link_program(GLuint vs, GLuint fs);

        //--- 3D draw ---
        void draw_sky(float pitch_rad);
        void draw_sprites_3d(const glm::mat4& view, const glm::mat4& proj, float yaw);
        void draw_decorations(const glm::mat4& view, const glm::mat4& proj);
        void draw_player_weapon(const glm::mat4& view, const glm::mat4& proj,
                                const glm::vec3& cam_pos, float yaw, float pitch);
        void draw_explosions_3d(const glm::mat4& view, const glm::mat4& proj);

        //--- 2D draw helpers ---
        void draw_textured_quad(GLuint tex, float x, float y, float w, float h,
                                float u0, float v0, float u1, float v1,
                                bool chroma_key, float r = 1, float g = 1, float b = 1, float a = 1);
        void draw_solid_quad(float x, float y, float w, float h,
                             float r, float g, float b, float a = 1);
        void draw_sprite_tile(GLuint atlas, int tile_count, int tile_id,
                              float x, float y, float size, bool chroma_key);
        void draw_text(float x, float y, const std::string& text, TTF_Font* font, SDL_Color color);

        //--- menu/UI ---
        void draw_in_game_hud();
        void draw_main_menu();
        void draw_pause_menu();
        void draw_help_menu();
        void draw_game_over();
        void draw_win_menu();
        void draw_shop_menu();

        SDL_Window* window;
        SDL_GLContext gl_context;

        ushort screen_w;
        ushort screen_h;

        TTF_Font* font_big;
        TTF_Font* font_medium;
        TTF_Font* font_hud; //10% smaller than font_medium, used by in-game HUD

        //--- 3D walls ---
        GLuint wall_program;
        GLuint wall_vao;
        GLuint wall_vbo;
        GLsizei wall_vertex_count;
        GLuint wall_texture;
        int wall_tile_count;
        GLint u_view_loc;
        GLint u_proj_loc;
        GLint u_wall_light_pos;
        GLint u_wall_light_dir;
        GLint u_wall_ambient;
        GLint u_wall_cone_cos;
        GLint u_wall_light_range;
        GLint u_wall_cam_pos;
        GLint u_wall_fog_color;
        GLint u_wall_fog_density;

        //--- 3D floor (solid color) ---
        GLuint solid3d_program;
        GLuint floor_vao;
        GLuint floor_vbo;
        GLsizei floor_vertex_count;
        GLint u_solid3d_view;
        GLint u_solid3d_proj;
        GLint u_solid3d_color;
        GLint u_floor_light_pos;
        GLint u_floor_light_dir;
        GLint u_floor_ambient;
        GLint u_floor_cone_cos;
        GLint u_floor_light_range;
        GLint u_floor_cam_pos;
        GLint u_floor_fog_color;
        GLint u_floor_fog_density;

        //--- 3D sprites (billboard, chroma-keyed) ---
        GLuint sprite3d_program;
        GLuint sprite3d_vao;
        GLuint sprite3d_vbo;
        GLint u_sprite3d_view;
        GLint u_sprite3d_proj;
        GLint u_sprite_light_pos;
        GLint u_sprite_light_dir;
        GLint u_sprite_ambient;
        GLint u_sprite_cone_cos;
        GLint u_sprite_light_range;
        GLint u_sprite3d_cam_pos;
        GLint u_sprite3d_fog_color;
        GLint u_sprite3d_fog_density;

        //--- sky (full-screen gradient) ---
        GLuint sky_program;
        GLuint sky_vao;
        GLuint sky_vbo;
        GLint u_sky_pitch;
        GLint u_sky_day;
        GLint u_floor_day;

        //--- 3D aliens (vertex-colored mesh, replaces enemy billboards) ---
        GLuint alien_program;
        GLint u_alien_model;
        GLint u_alien_view;
        GLint u_alien_proj;
        GLint u_alien_light_dir;
        GLint u_alien_cam_pos;
        GLint u_alien_fog_color;
        GLint u_alien_fog_density;
        Mesh alien_body_mesh_a;  //leg phase 0 (right leg forward)
        Mesh alien_body_mesh_b;  //leg phase 1 (left leg forward)
        Mesh alien_head_mesh;    //head + neck, twists independently of body

        //--- world prop meshes (instanced per-prop via model matrix) ---
        Mesh tree_mesh;
        Mesh cow_mesh;
        Mesh barn_mesh;
        Mesh fence_mesh;
        Mesh grass_mesh;
        Mesh beam_mesh;
        Mesh shotgun_mesh;
        Mesh muzzle_flash_mesh;
        Mesh explosion_mesh;
        Mesh ufo_mesh;
        Mesh shop_mesh;

        //--- 2D pipeline ---
        GLuint sprite_program;
        GLuint solid_program;
        GLuint quad_vao;
        GLuint quad_vbo;
        GLuint sprites_texture;
        int sprites_tile_count;
        GLint u_sprite_model;
        GLint u_sprite_proj;
        GLint u_sprite_uvrect;
        GLint u_sprite_chroma;
        GLint u_sprite_tint;
        GLint u_solid_model;
        GLint u_solid_proj;
        GLint u_solid_color;

        Player* player;
        Map* map;
        Menu* menu;
};

#endif
