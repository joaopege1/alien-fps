#include <iostream>
#include <cmath>
#include <vector>
#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "renderer.h"
#include "map.h"
#include "menu.h"
#include "timer.h"
#include "leaderboard.h"

//=== Shaders ===

static const char* wall_vertex_src = R"(
#version 330 core
layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec2 a_uv;
uniform mat4 u_view;
uniform mat4 u_proj;
out vec2 v_uv;
out vec3 v_world_pos;
void main() {
    gl_Position = u_proj * u_view * vec4(a_pos, 1.0);
    v_uv = a_uv;
    v_world_pos = a_pos;
}
)";

static const char* wall_fragment_src = R"(
#version 330 core
in vec2 v_uv;
in vec3 v_world_pos;
uniform sampler2D u_tex;
uniform vec3 u_cam_pos;
uniform vec3 u_fog_color;
uniform float u_fog_density;
out vec4 frag_color;
void main() {
    vec3 c = texture(u_tex, v_uv).rgb;
    float d = length(v_world_pos - u_cam_pos);
    float fog = 1.0 - exp(-d * u_fog_density);
    c = mix(c, u_fog_color, clamp(fog, 0.0, 1.0));
    frag_color = vec4(c, 1.0);
}
)";

static const char* sprite3d_vertex_src = R"(
#version 330 core
layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec2 a_uv;
uniform mat4 u_view;
uniform mat4 u_proj;
out vec2 v_uv;
out vec3 v_world_pos;
void main() {
    gl_Position = u_proj * u_view * vec4(a_pos, 1.0);
    v_uv = a_uv;
    v_world_pos = a_pos;
}
)";

static const char* sprite3d_fragment_src = R"(
#version 330 core
in vec2 v_uv;
in vec3 v_world_pos;
uniform sampler2D u_tex;
uniform vec3 u_cam_pos;
uniform vec3 u_fog_color;
uniform float u_fog_density;
out vec4 frag_color;
void main() {
    vec4 c = texture(u_tex, v_uv);
    if(c.r < 0.05 && c.g > 0.9 && c.b > 0.9) discard;
    float d = length(v_world_pos - u_cam_pos);
    float fog = 1.0 - exp(-d * u_fog_density);
    vec3 col = mix(c.rgb, u_fog_color, clamp(fog, 0.0, 1.0));
    frag_color = vec4(col, c.a);
}
)";

static const char* solid3d_vertex_src = R"(
#version 330 core
layout(location = 0) in vec3 a_pos;
uniform mat4 u_view;
uniform mat4 u_proj;
out vec3 v_world_pos;
void main() {
    gl_Position = u_proj * u_view * vec4(a_pos, 1.0);
    v_world_pos = a_pos;
}
)";

static const char* solid3d_fragment_src = R"(
#version 330 core
in vec3 v_world_pos;
uniform vec4 u_color;
uniform vec3 u_cam_pos;
uniform vec3 u_fog_color;
uniform float u_fog_density;
uniform float u_day_factor;
out vec4 frag_color;

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float noise2d(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

void main() {
    //multi-scale noise to fake a grass field
    float n_large = noise2d(v_world_pos.xz * 5.0);
    float n_fine  = noise2d(v_world_pos.xz * 22.0);
    float n = n_large * 0.65 + n_fine * 0.35;

    //3-stop palette so the ground also fades through a dusky sunset
    vec3 grass_dark_n  = vec3(0.05, 0.08, 0.04);
    vec3 grass_dark_s  = vec3(0.18, 0.15, 0.08);
    vec3 grass_dark_d  = vec3(0.20, 0.35, 0.15);
    vec3 grass_light_n = vec3(0.13, 0.19, 0.09);
    vec3 grass_light_s = vec3(0.40, 0.30, 0.18);
    vec3 grass_light_d = vec3(0.42, 0.58, 0.22);

    vec3 grass_dark  = (u_day_factor > 0.5) ? mix(grass_dark_s,  grass_dark_d,  (u_day_factor - 0.5) * 2.0)
                                            : mix(grass_dark_n,  grass_dark_s,  u_day_factor * 2.0);
    vec3 grass_light = (u_day_factor > 0.5) ? mix(grass_light_s, grass_light_d, (u_day_factor - 0.5) * 2.0)
                                            : mix(grass_light_n, grass_light_s, u_day_factor * 2.0);
    vec3 c = mix(grass_dark, grass_light, n);

    //occasional dirt patches
    if(n < 0.18) {
        vec3 dirt_n = vec3(0.07, 0.06, 0.04);
        vec3 dirt_s = vec3(0.30, 0.20, 0.10);
        vec3 dirt_d = vec3(0.40, 0.30, 0.18);
        vec3 dirt = (u_day_factor > 0.5) ? mix(dirt_s, dirt_d, (u_day_factor - 0.5) * 2.0)
                                         : mix(dirt_n, dirt_s, u_day_factor * 2.0);
        c = mix(dirt, c, n / 0.18);
    }

    //distance fog
    float d = length(v_world_pos - u_cam_pos);
    float fog = 1.0 - exp(-d * u_fog_density);
    c = mix(c, u_fog_color, clamp(fog, 0.0, 1.0));

    frag_color = vec4(c, 1.0);
}
)";

static const char* sky_vertex_src = R"(
#version 330 core
layout(location = 0) in vec2 a_pos;
out vec2 v_uv;
void main() {
    gl_Position = vec4(a_pos, 0.9999, 1.0);
    v_uv = a_pos * 0.5 + 0.5;
}
)";

static const char* sky_fragment_src = R"(
#version 330 core
in vec2 v_uv;
uniform float u_pitch_shift;
uniform float u_day_factor; //0 = full night, 0.5 = sunset, 1 = full day
out vec4 frag_color;

vec3 lerp3(vec3 a, vec3 b, vec3 c, float t) {
    return (t > 0.5) ? mix(b, c, (t - 0.5) * 2.0)
                     : mix(a, b, t * 2.0);
}

void main() {
    float t = clamp(v_uv.y + u_pitch_shift, 0.0, 1.0);

    //3-stop palette through sunset for a smooth transition
    vec3 horizon = lerp3(vec3(0.10, 0.13, 0.22),
                         vec3(0.95, 0.50, 0.20),
                         vec3(0.85, 0.90, 0.95), u_day_factor);
    vec3 mid     = lerp3(vec3(0.05, 0.07, 0.16),
                         vec3(0.70, 0.35, 0.30),
                         vec3(0.55, 0.75, 0.95), u_day_factor);
    vec3 zenith  = lerp3(vec3(0.01, 0.02, 0.07),
                         vec3(0.30, 0.18, 0.30),
                         vec3(0.30, 0.55, 0.85), u_day_factor);

    vec3 sky = (t < 0.5) ? mix(horizon, mid, t * 2.0)
                         : mix(mid, zenith, (t - 0.5) * 2.0);

    //horizon glow: moonlight blue -> warm sunset -> bright sun
    float glow = exp(-abs(v_uv.y - (0.5 - u_pitch_shift)) * 5.0);
    vec3 glow_col = lerp3(vec3(0.35, 0.40, 0.55),
                          vec3(1.00, 0.55, 0.25),
                          vec3(1.00, 0.95, 0.80), u_day_factor);
    sky = mix(sky, glow_col, glow * 0.22);

    frag_color = vec4(sky, 1.0);
}
)";

//--- alien (vertex-colored 3D mesh with simple directional lighting) ---

static const char* alien_vertex_src = R"(
#version 330 core
layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec3 a_color;
uniform mat4 u_model;
uniform mat4 u_view;
uniform mat4 u_proj;
out vec3 v_normal;
out vec3 v_color;
out vec3 v_world_pos;
void main() {
    vec4 world = u_model * vec4(a_pos, 1.0);
    gl_Position = u_proj * u_view * world;
    v_normal = mat3(u_model) * a_normal;
    v_color = a_color;
    v_world_pos = world.xyz;
}
)";

static const char* alien_fragment_src = R"(
#version 330 core
in vec3 v_normal;
in vec3 v_color;
in vec3 v_world_pos;
uniform vec3 u_light_dir; //direction light travels (not "to light")
uniform vec3 u_cam_pos;
uniform vec3 u_fog_color;
uniform float u_fog_density;
out vec4 frag_color;
void main() {
    vec3 n = normalize(v_normal);
    float ndl = max(dot(n, -normalize(u_light_dir)), 0.0);
    float light = 0.32 + ndl * 0.68;
    vec3 moonlight_tint = vec3(0.78, 0.86, 1.08);
    vec3 col = v_color * light * moonlight_tint;
    //distance fog
    float d = length(v_world_pos - u_cam_pos);
    float fog = 1.0 - exp(-d * u_fog_density);
    col = mix(col, u_fog_color, clamp(fog, 0.0, 1.0));
    frag_color = vec4(col, 1.0);
}
)";

static const char* sprite_vertex_src = R"(
#version 330 core
layout(location = 0) in vec2 a_pos;
uniform mat4 u_model;
uniform mat4 u_proj;
uniform vec4 u_uvrect;
out vec2 v_uv;
void main() {
    gl_Position = u_proj * u_model * vec4(a_pos, 0.0, 1.0);
    v_uv = mix(u_uvrect.xy, u_uvrect.zw, a_pos);
}
)";

static const char* sprite_fragment_src = R"(
#version 330 core
in vec2 v_uv;
uniform sampler2D u_tex;
uniform int u_chroma;
uniform vec4 u_tint;
out vec4 frag_color;
void main() {
    vec4 c = texture(u_tex, v_uv);
    if(u_chroma == 1) {
        if(c.r < 0.05 && c.g > 0.9 && c.b > 0.9) discard;
    }
    frag_color = c * u_tint;
}
)";

static const char* solid_vertex_src = R"(
#version 330 core
layout(location = 0) in vec2 a_pos;
uniform mat4 u_model;
uniform mat4 u_proj;
void main() {
    gl_Position = u_proj * u_model * vec4(a_pos, 0.0, 1.0);
}
)";

static const char* solid_fragment_src = R"(
#version 330 core
uniform vec4 u_color;
out vec4 frag_color;
void main() {
    frag_color = u_color;
}
)";

//=== Renderer ===

Renderer::Renderer(Player* p, Map* ma, Menu* me)
    : window(NULL), gl_context(NULL), screen_w(0), screen_h(0),
      font_big(NULL), font_medium(NULL), font_hud(NULL),
      wall_program(0), wall_vao(0), wall_vbo(0), wall_vertex_count(0),
      wall_texture(0), wall_tile_count(0), u_view_loc(-1), u_proj_loc(-1),
      u_wall_light_pos(-1), u_wall_light_dir(-1), u_wall_ambient(-1),
      u_wall_cone_cos(-1), u_wall_light_range(-1),
      u_wall_cam_pos(-1), u_wall_fog_color(-1), u_wall_fog_density(-1),
      solid3d_program(0), floor_vao(0), floor_vbo(0), floor_vertex_count(0),
      u_solid3d_view(-1), u_solid3d_proj(-1), u_solid3d_color(-1),
      u_floor_light_pos(-1), u_floor_light_dir(-1), u_floor_ambient(-1),
      u_floor_cone_cos(-1), u_floor_light_range(-1),
      u_floor_cam_pos(-1), u_floor_fog_color(-1), u_floor_fog_density(-1),
      sprite3d_program(0), sprite3d_vao(0), sprite3d_vbo(0),
      u_sprite3d_view(-1), u_sprite3d_proj(-1),
      u_sprite_light_pos(-1), u_sprite_light_dir(-1), u_sprite_ambient(-1),
      u_sprite_cone_cos(-1), u_sprite_light_range(-1),
      u_sprite3d_cam_pos(-1), u_sprite3d_fog_color(-1), u_sprite3d_fog_density(-1),
      sky_program(0), sky_vao(0), sky_vbo(0), u_sky_pitch(-1), u_sky_day(-1), u_floor_day(-1),
      alien_program(0), u_alien_model(-1), u_alien_view(-1),
      u_alien_proj(-1), u_alien_light_dir(-1),
      u_alien_cam_pos(-1), u_alien_fog_color(-1), u_alien_fog_density(-1),
      alien_body_mesh_a(), alien_body_mesh_b(), alien_head_mesh(),
      tree_mesh(), cow_mesh(), barn_mesh(), fence_mesh(), grass_mesh(), beam_mesh(),
      shotgun_mesh(), muzzle_flash_mesh(), explosion_mesh(), ufo_mesh(), shop_mesh(),
      sprite_program(0), solid_program(0), quad_vao(0), quad_vbo(0),
      sprites_texture(0), sprites_tile_count(0),
      u_sprite_model(-1), u_sprite_proj(-1), u_sprite_uvrect(-1),
      u_sprite_chroma(-1), u_sprite_tint(-1),
      u_solid_model(-1), u_solid_proj(-1), u_solid_color(-1),
      player(p), map(ma), menu(me)
{
}

bool Renderer::init_sdl(const char* title, ushort width, ushort height)
{
    if(SDL_Init(SDL_INIT_EVERYTHING))
    {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
        return false;
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    window = SDL_CreateWindow(title, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                              width, height, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if(!window) { std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << std::endl; return false; }

    gl_context = SDL_GL_CreateContext(window);
    if(!gl_context) { std::cerr << "SDL_GL_CreateContext failed: " << SDL_GetError() << std::endl; return false; }
    SDL_GL_SetSwapInterval(1);

    glewExperimental = GL_TRUE;
    GLenum err = glewInit();
    if(err != GLEW_OK && err != GLEW_ERROR_NO_GLX_DISPLAY)
    {
        std::cerr << "glewInit failed: " << glewGetErrorString(err) << std::endl;
        return false;
    }
    glGetError();

    std::cout << "GL version: " << glGetString(GL_VERSION) << std::endl;
    std::cout << "GLSL version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;

    screen_w = width;
    screen_h = height;
    glViewport(0, 0, width, height);
    glEnable(GL_DEPTH_TEST);

    TTF_Init();
    font_big    = TTF_OpenFont("pixelz.ttf", 100);
    font_medium = TTF_OpenFont("pixelz.ttf", 60);
    font_hud    = TTF_OpenFont("pixelz.ttf", 54); //10% smaller than font_medium for in-game HUD
    if(!font_big || !font_medium || !font_hud) { std::cerr << "Couldn't load ttf: " << SDL_GetError() << std::endl; return false; }

    return init_gl_resources();
}

bool Renderer::init_gl_resources()
{
    //--- wall program ---
    {
        GLuint vs = compile_shader(GL_VERTEX_SHADER, wall_vertex_src);
        GLuint fs = compile_shader(GL_FRAGMENT_SHADER, wall_fragment_src);
        if(!vs || !fs) return false;
        wall_program = link_program(vs, fs);
        glDeleteShader(vs); glDeleteShader(fs);
        if(!wall_program) return false;
        u_view_loc = glGetUniformLocation(wall_program, "u_view");
        u_proj_loc = glGetUniformLocation(wall_program, "u_proj");
        u_wall_light_pos   = glGetUniformLocation(wall_program, "u_light_pos");
        u_wall_light_dir   = glGetUniformLocation(wall_program, "u_light_dir");
        u_wall_ambient     = glGetUniformLocation(wall_program, "u_ambient");
        u_wall_cone_cos    = glGetUniformLocation(wall_program, "u_cone_cos");
        u_wall_light_range = glGetUniformLocation(wall_program, "u_light_range");
        u_wall_cam_pos     = glGetUniformLocation(wall_program, "u_cam_pos");
        u_wall_fog_color   = glGetUniformLocation(wall_program, "u_fog_color");
        u_wall_fog_density = glGetUniformLocation(wall_program, "u_fog_density");
    }

    wall_texture = load_bmp_texture("walltext.bmp", &wall_tile_count);
    if(!wall_texture) return false;
    std::cout << "wall atlas tiles: " << wall_tile_count << std::endl;
    build_wall_mesh();

    if(!init_2d_resources()) return false;

    sprites_texture = load_bmp_texture("sprites.bmp", &sprites_tile_count);
    if(!sprites_texture) return false;
    std::cout << "sprite atlas tiles: " << sprites_tile_count << std::endl;

    if(!init_3d_extras()) return false;
    if(!init_alien_resources()) return false;
    return true;
}

bool Renderer::init_alien_resources()
{
    GLuint vs = compile_shader(GL_VERTEX_SHADER, alien_vertex_src);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, alien_fragment_src);
    if(!vs || !fs) return false;
    alien_program = link_program(vs, fs);
    glDeleteShader(vs); glDeleteShader(fs);
    if(!alien_program) return false;
    u_alien_model     = glGetUniformLocation(alien_program, "u_model");
    u_alien_view      = glGetUniformLocation(alien_program, "u_view");
    u_alien_proj      = glGetUniformLocation(alien_program, "u_proj");
    u_alien_light_dir = glGetUniformLocation(alien_program, "u_light_dir");
    u_alien_cam_pos   = glGetUniformLocation(alien_program, "u_cam_pos");
    u_alien_fog_color = glGetUniformLocation(alien_program, "u_fog_color");
    u_alien_fog_density = glGetUniformLocation(alien_program, "u_fog_density");

    //body has two walking poses; head is a single mesh that twists independently
    build_alien_body(alien_body_mesh_a, 0.0f);
    alien_body_mesh_a.upload();
    build_alien_body(alien_body_mesh_b, 1.0f);
    alien_body_mesh_b.upload();
    build_alien_head(alien_head_mesh);
    alien_head_mesh.upload();

    //world prop meshes (each built once, then instanced via per-draw model matrix)
    build_tree(tree_mesh);          tree_mesh.upload();
    build_cow(cow_mesh);            cow_mesh.upload();
    build_barn(barn_mesh);          barn_mesh.upload();
    build_fence_section(fence_mesh);fence_mesh.upload();
    build_grass_tuft(grass_mesh);   grass_mesh.upload();
    build_light_beam(beam_mesh);    beam_mesh.upload();
    build_shotgun(shotgun_mesh);              shotgun_mesh.upload();
    build_muzzle_flash(muzzle_flash_mesh);    muzzle_flash_mesh.upload();
    build_alien_explosion(explosion_mesh);    explosion_mesh.upload();
    build_ufo(ufo_mesh);                      ufo_mesh.upload();
    build_shop(shop_mesh);                    shop_mesh.upload();

    std::cout << "alien body verts: " << alien_body_mesh_a.vertex_count
              << ", head: " << alien_head_mesh.vertex_count
              << ", tree: " << tree_mesh.vertex_count
              << ", cow: " << cow_mesh.vertex_count
              << ", barn: " << barn_mesh.vertex_count << std::endl;
    return true;
}

bool Renderer::init_3d_extras()
{
    //--- solid3d (floor) ---
    {
        GLuint vs = compile_shader(GL_VERTEX_SHADER, solid3d_vertex_src);
        GLuint fs = compile_shader(GL_FRAGMENT_SHADER, solid3d_fragment_src);
        if(!vs || !fs) return false;
        solid3d_program = link_program(vs, fs);
        glDeleteShader(vs); glDeleteShader(fs);
        if(!solid3d_program) return false;
        u_solid3d_view  = glGetUniformLocation(solid3d_program, "u_view");
        u_solid3d_proj  = glGetUniformLocation(solid3d_program, "u_proj");
        u_solid3d_color = glGetUniformLocation(solid3d_program, "u_color");
        u_floor_light_pos   = glGetUniformLocation(solid3d_program, "u_light_pos");
        u_floor_light_dir   = glGetUniformLocation(solid3d_program, "u_light_dir");
        u_floor_ambient     = glGetUniformLocation(solid3d_program, "u_ambient");
        u_floor_cone_cos    = glGetUniformLocation(solid3d_program, "u_cone_cos");
        u_floor_light_range = glGetUniformLocation(solid3d_program, "u_light_range");
        u_floor_cam_pos     = glGetUniformLocation(solid3d_program, "u_cam_pos");
        u_floor_fog_color   = glGetUniformLocation(solid3d_program, "u_fog_color");
        u_floor_fog_density = glGetUniformLocation(solid3d_program, "u_fog_density");
        u_floor_day         = glGetUniformLocation(solid3d_program, "u_day_factor");
    }
    build_floor_mesh();

    //--- sprite3d (billboard) ---
    {
        GLuint vs = compile_shader(GL_VERTEX_SHADER, sprite3d_vertex_src);
        GLuint fs = compile_shader(GL_FRAGMENT_SHADER, sprite3d_fragment_src);
        if(!vs || !fs) return false;
        sprite3d_program = link_program(vs, fs);
        glDeleteShader(vs); glDeleteShader(fs);
        if(!sprite3d_program) return false;
        u_sprite3d_view = glGetUniformLocation(sprite3d_program, "u_view");
        u_sprite3d_proj = glGetUniformLocation(sprite3d_program, "u_proj");
        u_sprite_light_pos   = glGetUniformLocation(sprite3d_program, "u_light_pos");
        u_sprite_light_dir   = glGetUniformLocation(sprite3d_program, "u_light_dir");
        u_sprite_ambient     = glGetUniformLocation(sprite3d_program, "u_ambient");
        u_sprite_cone_cos    = glGetUniformLocation(sprite3d_program, "u_cone_cos");
        u_sprite_light_range = glGetUniformLocation(sprite3d_program, "u_light_range");
        u_sprite3d_cam_pos   = glGetUniformLocation(sprite3d_program, "u_cam_pos");
        u_sprite3d_fog_color = glGetUniformLocation(sprite3d_program, "u_fog_color");
        u_sprite3d_fog_density = glGetUniformLocation(sprite3d_program, "u_fog_density");
    }
    glGenVertexArrays(1, &sprite3d_vao);
    glGenBuffers(1, &sprite3d_vbo);
    glBindVertexArray(sprite3d_vao);
    glBindBuffer(GL_ARRAY_BUFFER, sprite3d_vbo);
    glBufferData(GL_ARRAY_BUFFER, 0, NULL, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    //--- sky ---
    {
        GLuint vs = compile_shader(GL_VERTEX_SHADER, sky_vertex_src);
        GLuint fs = compile_shader(GL_FRAGMENT_SHADER, sky_fragment_src);
        if(!vs || !fs) return false;
        sky_program = link_program(vs, fs);
        glDeleteShader(vs); glDeleteShader(fs);
        if(!sky_program) return false;
        u_sky_pitch = glGetUniformLocation(sky_program, "u_pitch_shift");
        u_sky_day   = glGetUniformLocation(sky_program, "u_day_factor");
    }
    float sky_quad[] = {
        -1.f, -1.f,
         1.f, -1.f,
         1.f,  1.f,
        -1.f, -1.f,
         1.f,  1.f,
        -1.f,  1.f
    };
    glGenVertexArrays(1, &sky_vao);
    glGenBuffers(1, &sky_vbo);
    glBindVertexArray(sky_vao);
    glBindBuffer(GL_ARRAY_BUFFER, sky_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(sky_quad), sky_quad, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    return true;
}

bool Renderer::init_2d_resources()
{
    {
        GLuint vs = compile_shader(GL_VERTEX_SHADER, sprite_vertex_src);
        GLuint fs = compile_shader(GL_FRAGMENT_SHADER, sprite_fragment_src);
        if(!vs || !fs) return false;
        sprite_program = link_program(vs, fs);
        glDeleteShader(vs); glDeleteShader(fs);
        if(!sprite_program) return false;
        u_sprite_model  = glGetUniformLocation(sprite_program, "u_model");
        u_sprite_proj   = glGetUniformLocation(sprite_program, "u_proj");
        u_sprite_uvrect = glGetUniformLocation(sprite_program, "u_uvrect");
        u_sprite_chroma = glGetUniformLocation(sprite_program, "u_chroma");
        u_sprite_tint   = glGetUniformLocation(sprite_program, "u_tint");
    }
    {
        GLuint vs = compile_shader(GL_VERTEX_SHADER, solid_vertex_src);
        GLuint fs = compile_shader(GL_FRAGMENT_SHADER, solid_fragment_src);
        if(!vs || !fs) return false;
        solid_program = link_program(vs, fs);
        glDeleteShader(vs); glDeleteShader(fs);
        if(!solid_program) return false;
        u_solid_model = glGetUniformLocation(solid_program, "u_model");
        u_solid_proj  = glGetUniformLocation(solid_program, "u_proj");
        u_solid_color = glGetUniformLocation(solid_program, "u_color");
    }
    float quad[] = {
        0.f, 0.f, 1.f, 0.f, 1.f, 1.f,
        0.f, 0.f, 1.f, 1.f, 0.f, 1.f
    };
    glGenVertexArrays(1, &quad_vao);
    glGenBuffers(1, &quad_vbo);
    glBindVertexArray(quad_vao);
    glBindBuffer(GL_ARRAY_BUFFER, quad_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
    return true;
}

GLuint Renderer::compile_shader(GLenum type, const char* src)
{
    GLuint sh = glCreateShader(type);
    glShaderSource(sh, 1, &src, NULL);
    glCompileShader(sh);
    GLint ok = 0;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if(!ok)
    {
        char log[1024];
        glGetShaderInfoLog(sh, 1024, NULL, log);
        std::cerr << "shader compile error: " << log << std::endl;
        glDeleteShader(sh);
        return 0;
    }
    return sh;
}

GLuint Renderer::link_program(GLuint vs, GLuint fs)
{
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if(!ok)
    {
        char log[1024];
        glGetProgramInfoLog(prog, 1024, NULL, log);
        std::cerr << "program link error: " << log << std::endl;
        glDeleteProgram(prog);
        return 0;
    }
    return prog;
}

GLuint Renderer::load_bmp_texture(const char* path, int* out_tile_count)
{
    SDL_Surface* surf = SDL_LoadBMP(path);
    if(!surf) { std::cerr << "SDL_LoadBMP " << path << " failed: " << SDL_GetError() << std::endl; return 0; }
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    GLenum src_format = (surf->format->BytesPerPixel == 4) ? GL_BGRA : GL_BGR;
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, surf->w, surf->h, 0, src_format, GL_UNSIGNED_BYTE, surf->pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    if(out_tile_count) *out_tile_count = surf->w / surf->h;
    SDL_FreeSurface(surf);
    return tex;
}

//--- wall mesh ---

static void push_vertex(std::vector<float>& v, float x, float y, float z, float u, float w)
{
    v.push_back(x); v.push_back(y); v.push_back(z);
    v.push_back(u); v.push_back(w);
}

static void push_quad(std::vector<float>& v,
    float ax, float ay, float az, float au, float aw,
    float bx, float by, float bz, float bu, float bw,
    float cx, float cy, float cz, float cu, float cw,
    float dx, float dy, float dz, float du, float dw)
{
    push_vertex(v, ax, ay, az, au, aw);
    push_vertex(v, bx, by, bz, bu, bw);
    push_vertex(v, cx, cy, cz, cu, cw);
    push_vertex(v, ax, ay, az, au, aw);
    push_vertex(v, cx, cy, cz, cu, cw);
    push_vertex(v, dx, dy, dz, du, dw);
}

void Renderer::build_wall_mesh()
{
    std::vector<float> verts;
    int mw = map->w;
    int mh = map->h;
    float tc = (float)wall_tile_count;
    for(int my = 0; my < mh; my++)
    for(int mx = 0; mx < mw; mx++)
    {
        char t = map->get_tile(mx, my);
        if(t == ' ') continue;
        int tile_id = t - '0';
        float u0 = tile_id / tc;
        float u1 = (tile_id + 1) / tc;
        float x0 = mx, x1 = mx + 1;
        float z0 = my, z1 = my + 1;
        float y0 = 0, y1 = 1;
        bool ow = (mx == 0)      || map->get_tile(mx - 1, my) == ' ';
        bool oe = (mx == mw - 1) || map->get_tile(mx + 1, my) == ' ';
        bool on = (my == 0)      || map->get_tile(mx, my - 1) == ' ';
        bool os = (my == mh - 1) || map->get_tile(mx, my + 1) == ' ';
        if(ow) push_quad(verts, x0,y0,z0,u0,1, x0,y1,z0,u0,0, x0,y1,z1,u1,0, x0,y0,z1,u1,1);
        if(oe) push_quad(verts, x1,y0,z1,u0,1, x1,y1,z1,u0,0, x1,y1,z0,u1,0, x1,y0,z0,u1,1);
        if(on) push_quad(verts, x1,y0,z0,u0,1, x1,y1,z0,u0,0, x0,y1,z0,u1,0, x0,y0,z0,u1,1);
        if(os) push_quad(verts, x0,y0,z1,u0,1, x0,y1,z1,u0,0, x1,y1,z1,u1,0, x1,y0,z1,u1,1);
    }
    wall_vertex_count = verts.size() / 5;
    if(wall_vao == 0)
    {
        glGenVertexArrays(1, &wall_vao);
        glGenBuffers(1, &wall_vbo);
        glBindVertexArray(wall_vao);
        glBindBuffer(GL_ARRAY_BUFFER, wall_vbo);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
    }
    else
    {
        glBindVertexArray(wall_vao);
        glBindBuffer(GL_ARRAY_BUFFER, wall_vbo);
    }
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_DYNAMIC_DRAW);
    glBindVertexArray(0);
}

//--- floor mesh (huge quad at y=0) ---

void Renderer::build_floor_mesh()
{
    float a = -200.0f, b = 200.0f;
    float verts[] = {
        a, 0, a,
        b, 0, a,
        b, 0, b,
        a, 0, a,
        b, 0, b,
        a, 0, b
    };
    floor_vertex_count = 6;
    glGenVertexArrays(1, &floor_vao);
    glGenBuffers(1, &floor_vbo);
    glBindVertexArray(floor_vao);
    glBindBuffer(GL_ARRAY_BUFFER, floor_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

//--- sky ---

void Renderer::draw_sky(float pitch_rad)
{
    //pitch shifts the gradient: looking up brings horizon (orange) down, more zenith on screen
    float shift = pitch_rad * 0.35f;
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    //match the same day_factor curve used for fog/floor
    float sky_day;
    if(!map->is_day)                sky_day = 0.0f;
    else if(map->day_timer > 30.0f) sky_day = 1.0f;
    else                            sky_day = map->day_timer / 30.0f;

    glUseProgram(sky_program);
    glUniform1f(u_sky_pitch, shift);
    glUniform1f(u_sky_day,   sky_day);
    glBindVertexArray(sky_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
}

//--- 3D sprite billboards ---

void Renderer::draw_sprites_3d(const glm::mat4& view, const glm::mat4& proj, float yaw)
{
    const std::vector<Sprite>& sprites = map->get_sprites();
    if(sprites.empty()) return;

    //billboard right & up vectors (Y-axis billboard: only yaw matters)
    glm::vec3 right(-sin(yaw), 0.0f, cos(yaw));
    glm::vec3 up(0.0f, 1.0f, 0.0f);

    std::vector<float> verts;
    verts.reserve(sprites.size() * 6 * 5);
    float tc = (float)sprites_tile_count;

    for(unsigned int i = 0; i < sprites.size(); i++)
    {
        const Sprite& s = sprites[i];
        if(s.type == Enemy) continue;     //enemies render as 3D alien meshes
        if(s.type == Temporary) continue; //explosions render as 3D meshes in draw_explosions_3d
        float world_size = s.size / 600.0f;
        if(world_size <= 0) continue;

        //anchor sprite bottom slightly below floor so cyan-padded asset bottoms still touch the ground
        glm::vec3 base(s.x, -world_size * 0.15f, s.y);
        glm::vec3 r = right * (world_size * 0.5f);
        glm::vec3 u = up * world_size;

        glm::vec3 bl = base - r;             //bottom-left
        glm::vec3 br = base + r;             //bottom-right
        glm::vec3 tr = base + r + u;         //top-right
        glm::vec3 tl = base - r + u;         //top-left

        float u0 = s.itex / tc;
        float u1 = (s.itex + 1) / tc;

        push_vertex(verts, bl.x, bl.y, bl.z, u0, 1);
        push_vertex(verts, br.x, br.y, br.z, u1, 1);
        push_vertex(verts, tr.x, tr.y, tr.z, u1, 0);
        push_vertex(verts, bl.x, bl.y, bl.z, u0, 1);
        push_vertex(verts, tr.x, tr.y, tr.z, u1, 0);
        push_vertex(verts, tl.x, tl.y, tl.z, u0, 0);
    }
    if(verts.empty()) return;

    glUseProgram(sprite3d_program);
    glUniformMatrix4fv(u_sprite3d_view, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(u_sprite3d_proj, 1, GL_FALSE, glm::value_ptr(proj));
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sprites_texture);

    glBindVertexArray(sprite3d_vao);
    glBindBuffer(GL_ARRAY_BUFFER, sprite3d_vbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, verts.size() / 5);
    glBindVertexArray(0);
}

void Renderer::draw_aliens_3d(const glm::mat4& view, const glm::mat4& proj)
{
    const std::vector<Sprite>& sprites = map->get_sprites();
    if(sprites.empty()) return;

    glUseProgram(alien_program);
    glUniformMatrix4fv(u_alien_view, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(u_alien_proj, 1, GL_FALSE, glm::value_ptr(proj));
    //moonlight from upper-front-left (matches sky direction roughly)
    glUniform3f(u_alien_light_dir, -0.35f, -0.85f, -0.40f);

    float px = player->get_x();
    float py = player->get_y();

    for(unsigned int i = 0; i < sprites.size(); i++)
    {
        const Sprite& s = sprites[i];
        if(s.type != Enemy) continue;

        //--- HEAD: turn toward whatever the alien is currently pursuing ---
        //fallback to player if target hasn't been set yet (e.g. very first frame)
        float dx_t, dz_t;
        if(s.target_init)
        {
            dx_t = s.target_x - s.x;
            dz_t = s.target_y - s.y;
        }
        else
        {
            dx_t = px - s.x;
            dz_t = py - s.y;
        }

        if(dx_t * dx_t + dz_t * dz_t > 0.0001f)
        {
            float head_target = atan2f(dx_t, dz_t);
            if(!s.head_yaw_init)
            {
                s.head_yaw = head_target;
                s.head_yaw_init = true;
            }
            else
            {
                float diff = head_target - s.head_yaw;
                while(diff >  (float)M_PI) diff -= 2.0f * (float)M_PI;
                while(diff < -(float)M_PI) diff += 2.0f * (float)M_PI;
                s.head_yaw += diff * 0.18f; //head reacts faster than body
            }
        }

        //body_yaw is driven by Map::update_sprites (it gates movement on alignment)
        //fallback: if we somehow get here before that ran, snap to head direction
        if(!s.body_yaw_init)
        {
            s.body_yaw = s.head_yaw;
            s.body_yaw_init = true;
        }

        glm::vec3 pos(s.x, 0.0f, s.y);
        const float alien_scale = 1.55f; //scarier - towers slightly above the player

        //draw body
        glm::mat4 body_model = glm::translate(glm::mat4(1.0f), pos);
        body_model = glm::rotate(body_model, s.body_yaw, glm::vec3(0.0f, 1.0f, 0.0f));
        body_model = glm::scale(body_model, glm::vec3(alien_scale));
        glUniformMatrix4fv(u_alien_model, 1, GL_FALSE, glm::value_ptr(body_model));
        const Mesh& body = (s.itex == 1) ? alien_body_mesh_a : alien_body_mesh_b;
        glBindVertexArray(body.vao);
        glDrawArrays(GL_TRIANGLES, 0, body.vertex_count);

        //draw head with independent yaw (rotation around Y leaves head position unchanged)
        glm::mat4 head_model = glm::translate(glm::mat4(1.0f), pos);
        head_model = glm::rotate(head_model, s.head_yaw, glm::vec3(0.0f, 1.0f, 0.0f));
        head_model = glm::scale(head_model, glm::vec3(alien_scale));
        glUniformMatrix4fv(u_alien_model, 1, GL_FALSE, glm::value_ptr(head_model));
        glBindVertexArray(alien_head_mesh.vao);
        glDrawArrays(GL_TRIANGLES, 0, alien_head_mesh.vertex_count);
    }
    glBindVertexArray(0);
}

void Renderer::draw_decorations(const glm::mat4& view, const glm::mat4& proj)
{
    const std::vector<Prop>& props = map->get_props();
    if(props.empty()) return;

    glUseProgram(alien_program);
    glUniformMatrix4fv(u_alien_view, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(u_alien_proj, 1, GL_FALSE, glm::value_ptr(proj));
    glUniform3f(u_alien_light_dir, -0.35f, -0.85f, -0.40f);

    for(unsigned int i = 0; i < props.size(); i++)
    {
        const Prop& p = props[i];
        if(!p.active) continue;
        if(p.type == PropUFO && map->is_day) continue; //UFO only shows up at night

        glm::mat4 model;

        if(p.type == PropBeam)
        {
            //slant the abduction beam so it connects the cow on the ground to the UFO above
            glm::vec3 base(p.x, 0.0f, p.z);
            glm::vec3 ufo(map->ufo_x, UFO_ALTITUDE, map->ufo_z);
            glm::vec3 dir = ufo - base;
            float len = glm::length(dir);
            if(len < 0.001f) continue;
            glm::vec3 dirn = dir / len;

            //rotate mesh local +Y to align with dirn (using axis-angle from cross product)
            glm::mat4 rot(1.0f);
            float cs = glm::clamp(dirn.y, -1.0f, 1.0f); //dot of (0,1,0) and dirn
            if(cs < 0.9999f)
            {
                glm::vec3 axis = glm::normalize(glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), dirn));
                rot = glm::rotate(glm::mat4(1.0f), acosf(cs), axis);
            }
            //stretch Y so the 4.4-unit-tall beam reaches exactly the UFO
            glm::mat4 scl = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, len / 4.4f, 1.0f));
            model = glm::translate(glm::mat4(1.0f), base) * rot * scl;
        }
        else
        {
            model = glm::translate(glm::mat4(1.0f), glm::vec3(p.x, 0.0f, p.z));
            if(p.yaw != 0.0f)
                model = glm::rotate(model, p.yaw, glm::vec3(0.0f, 1.0f, 0.0f));
        }
        glUniformMatrix4fv(u_alien_model, 1, GL_FALSE, glm::value_ptr(model));

        const Mesh* m = NULL;
        switch(p.type)
        {
            case PropBarn:  m = &barn_mesh;  break;
            case PropCow:   m = &cow_mesh;   break;
            case PropFence: m = &fence_mesh; break;
            case PropTree:  m = &tree_mesh;  break;
            case PropGrass: m = &grass_mesh; break;
            case PropBeam:  m = &beam_mesh;  break;
            case PropUFO:   m = &ufo_mesh;   break;
            case PropShop:  m = &shop_mesh;  break;
        }
        if(!m) continue;
        glBindVertexArray(m->vao);
        glDrawArrays(GL_TRIANGLES, 0, m->vertex_count);
    }
    glBindVertexArray(0);
}

void Renderer::on_window_resize(int new_w, int new_h)
{
    if(new_w <= 0 || new_h <= 0) return;
    screen_w = (ushort)new_w;
    screen_h = (ushort)new_h;
    glViewport(0, 0, screen_w, screen_h);
}

void Renderer::draw_player_weapon(const glm::mat4& view, const glm::mat4& proj,
                                  const glm::vec3& cam_pos, float yaw, float pitch)
{
    //build a camera-attached basis so the gun rides with the camera (yaw + pitch)
    glm::vec3 forward(cosf(pitch) * cosf(yaw), sinf(pitch), cosf(pitch) * sinf(yaw));
    glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
    glm::vec3 up    = glm::cross(right, forward);

    //offset the gun down-right-forward from the camera (hip-fire pose)
    glm::vec3 gun_pos = cam_pos + forward * 0.40f + right * 0.22f - up * 0.22f;

    //model matrix whose local +X=right, +Y=up, +Z=forward axes match the camera basis
    glm::mat4 gun_model(1.0f);
    gun_model[0] = glm::vec4(right,   0.0f);
    gun_model[1] = glm::vec4(up,      0.0f);
    gun_model[2] = glm::vec4(forward, 0.0f);
    gun_model[3] = glm::vec4(gun_pos, 1.0f);

    glUseProgram(alien_program);
    glUniformMatrix4fv(u_alien_view, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(u_alien_proj, 1, GL_FALSE, glm::value_ptr(proj));
    glUniform3f(u_alien_light_dir, -0.35f, -0.85f, -0.40f);
    glUniformMatrix4fv(u_alien_model, 1, GL_FALSE, glm::value_ptr(gun_model));
    glBindVertexArray(shotgun_mesh.vao);
    glDrawArrays(GL_TRIANGLES, 0, shotgun_mesh.vertex_count);

    //muzzle flash: only on the frame the player just fired
    if(player->display_flash)
    {
        //flash sits at the barrel tip (gun local (0, 0.05, 0.58))
        glm::vec4 muzzle_local(0.0f, 0.05f, 0.58f, 1.0f);
        glm::vec4 muzzle_world = gun_model * muzzle_local;
        glm::mat4 flash_model = gun_model;
        flash_model[3] = muzzle_world;
        glUniformMatrix4fv(u_alien_model, 1, GL_FALSE, glm::value_ptr(flash_model));
        glBindVertexArray(muzzle_flash_mesh.vao);
        glDrawArrays(GL_TRIANGLES, 0, muzzle_flash_mesh.vertex_count);
    }
    glBindVertexArray(0);
}

void Renderer::draw_explosions_3d(const glm::mat4& view, const glm::mat4& proj)
{
    const std::vector<Sprite>& sprites = map->get_sprites();
    if(sprites.empty()) return;

    glUseProgram(alien_program);
    glUniformMatrix4fv(u_alien_view, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(u_alien_proj, 1, GL_FALSE, glm::value_ptr(proj));
    glUniform3f(u_alien_light_dir, -0.35f, -0.85f, -0.40f);

    for(unsigned int i = 0; i < sprites.size(); i++)
    {
        const Sprite& s = sprites[i];
        if(s.type != Temporary) continue;
        //sprite.size grows ~15 per frame from 400 - drive the explosion scale from it
        float scale = (float)s.size / 600.0f;
        glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(s.x, 0.6f, s.y));
        model = glm::scale(model, glm::vec3(scale));
        glUniformMatrix4fv(u_alien_model, 1, GL_FALSE, glm::value_ptr(model));
        glBindVertexArray(explosion_mesh.vao);
        glDrawArrays(GL_TRIANGLES, 0, explosion_mesh.vertex_count);
    }
    glBindVertexArray(0);
}

//--- 2D draw helpers ---

void Renderer::draw_textured_quad(GLuint tex, float x, float y, float w, float h,
                                  float u0, float v0, float u1, float v1,
                                  bool chroma_key, float r, float g, float b, float a)
{
    glm::mat4 proj = glm::ortho(0.0f, (float)screen_w, (float)screen_h, 0.0f);
    glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(x, y, 0.0f))
                    * glm::scale(glm::mat4(1.0f), glm::vec3(w, h, 1.0f));
    glUseProgram(sprite_program);
    glUniformMatrix4fv(u_sprite_proj, 1, GL_FALSE, glm::value_ptr(proj));
    glUniformMatrix4fv(u_sprite_model, 1, GL_FALSE, glm::value_ptr(model));
    glUniform4f(u_sprite_uvrect, u0, v0, u1, v1);
    glUniform1i(u_sprite_chroma, chroma_key ? 1 : 0);
    glUniform4f(u_sprite_tint, r, g, b, a);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    glBindVertexArray(quad_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

void Renderer::draw_solid_quad(float x, float y, float w, float h,
                               float r, float g, float b, float a)
{
    glm::mat4 proj = glm::ortho(0.0f, (float)screen_w, (float)screen_h, 0.0f);
    glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(x, y, 0.0f))
                    * glm::scale(glm::mat4(1.0f), glm::vec3(w, h, 1.0f));
    glUseProgram(solid_program);
    glUniformMatrix4fv(u_solid_proj, 1, GL_FALSE, glm::value_ptr(proj));
    glUniformMatrix4fv(u_solid_model, 1, GL_FALSE, glm::value_ptr(model));
    glUniform4f(u_solid_color, r, g, b, a);
    glBindVertexArray(quad_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

void Renderer::draw_sprite_tile(GLuint atlas, int tile_count, int tile_id,
                                float x, float y, float size, bool chroma_key)
{
    float u0 = tile_id / (float)tile_count;
    float u1 = (tile_id + 1) / (float)tile_count;
    draw_textured_quad(atlas, x, y, size, size, u0, 0, u1, 1, chroma_key);
}

void Renderer::draw_text(float x, float y, const std::string& text,
                         TTF_Font* font, SDL_Color color)
{
    if(text.empty()) return;
    SDL_Surface* raw = TTF_RenderText_Blended(font, text.c_str(), color);
    if(!raw) return;
    SDL_Surface* surf = SDL_ConvertSurfaceFormat(raw, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(raw);
    if(!surf) return;
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, surf->w, surf->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, surf->pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    draw_textured_quad(tex, x, y, surf->w, surf->h, 0, 0, 1, 1, false);
    glDeleteTextures(1, &tex);
    SDL_FreeSurface(surf);
}

//--- UI per screen ---

void Renderer::draw_in_game_hud()
{
    int cx = screen_w / 2;
    int cy = screen_h / 2;

    //crosshair
    float ch_thickness = 2, ch_len = 10;
    draw_solid_quad(cx - ch_len, cy - ch_thickness/2, ch_len*2, ch_thickness, 0, 1, 1, 1);
    draw_solid_quad(cx - ch_thickness/2, cy - ch_len, ch_thickness, ch_len*2, 0, 1, 1, 1);

    //health bar
    float bar_x = 8, bar_y = screen_h - 58, bar_w = 256, bar_h = 50;
    draw_solid_quad(bar_x, bar_y, bar_w, bar_h, 30/255.f, 0, 0, 1);
    float hp_w = bar_w * (player->health / 100.0f);
    if(hp_w < 0) hp_w = 0;
    draw_solid_quad(bar_x, bar_y, hp_w, bar_h, 200/255.f, 30/255.f, 30/255.f, 1);

    //"Press E to enter shop" prompt when day and near shop
    if(map->is_day)
    {
        float dx = player->get_x() - map->shop_x;
        float dz = player->get_y() - map->shop_z;
        if(dx * dx + dz * dz < 16.0f)
            draw_text(cx - 200, screen_h - 220, "Press E to enter the shop", font_medium, ttf_color_banana);
    }
}

void Renderer::draw_shop_menu()
{
    //dim overlay
    draw_solid_quad(0, 0, screen_w, screen_h, 0, 0, 0, 0.55f);

    draw_text(440, 80, "SHOP", font_big, ttf_color_banana);

    char buf[64];
    snprintf(buf, sizeof(buf), "$ %d in your pocket", map->coins);
    draw_text(390, 180, buf, font_medium, ttf_color_white);

    //affordability decides label color
    auto color_for = [&](int cost) {
        return (map->coins >= cost) ? ttf_color_banana : ttf_color_red;
    };

    snprintf(buf, sizeof(buf), "Repair All Fences  - $15");
    draw_text(380, 255, buf, font_medium, menu->check_hover(9)  ? color_for(15) : ttf_color_white);

    snprintf(buf, sizeof(buf), "Restore Health     - $20");
    draw_text(380, 325, buf, font_medium, menu->check_hover(10) ? color_for(20) : ttf_color_white);

    snprintf(buf, sizeof(buf), "Buy a Cow (%d/%d)   - $50", map->get_cow_count(), map->max_cows);
    draw_text(380, 395, buf, font_medium, menu->check_hover(11) ? color_for(50) : ttf_color_white);

    snprintf(buf, sizeof(buf), "Upgrade Pen (+2)   - $200");
    draw_text(380, 465, buf, font_medium, menu->check_hover(12) ? color_for(200) : ttf_color_white);

    draw_text(380, 565, "Close (Esc)", font_medium,
              menu->check_hover(13) ? ttf_color_banana : ttf_color_white);
}

void Renderer::draw_main_menu()
{
    draw_text(100, 50, "Defend your Cows", font_big, ttf_color_white);
    draw_text(550, 350, "PLAY", font_medium,
              menu->check_hover(0) ? ttf_color_banana : ttf_color_white);
    std::string diff = std::string("DIFFICULTY:") +
        (menu->difficulty == 0 ? "EASY" : (menu->difficulty == 1 ? "NORMAL" : "HARD"));
    draw_text(380, 420, diff, font_medium,
              menu->check_hover(1) ? ttf_color_banana : ttf_color_white);
    std::string snd = std::string("SOUND:") +
        (menu->sound == 0 ? "NONE" : (menu->sound == 1 ? "LOW" : "NORMAL"));
    draw_text(460, 490, snd, font_medium,
              menu->check_hover(8) ? ttf_color_banana : ttf_color_white);
    draw_text(550, 560, "HELP", font_medium,
              menu->check_hover(5) ? ttf_color_banana : ttf_color_white);
    draw_text(550, 620, "QUIT", font_medium,
              menu->check_hover(2) ? ttf_color_banana : ttf_color_white);
}

void Renderer::draw_pause_menu()
{
    draw_text(470, 100, "PAUSE", font_big, ttf_color_white);
    draw_text(520, 300, "RESUME", font_medium,
              menu->check_hover(3) ? ttf_color_banana : ttf_color_white);
    draw_text(550, 370, "QUIT", font_medium,
              menu->check_hover(4) ? ttf_color_banana : ttf_color_white);
}

void Renderer::draw_help_menu()
{
    draw_text(500, 50, "HELP", font_big, ttf_color_white);
    draw_text(20, 200, "Move : WASD", font_medium, ttf_color_white);
    draw_text(20, 270, "Camera : Mouse", font_medium, ttf_color_white);
    draw_text(20, 340, "Fire : Left click", font_medium, ttf_color_white);
    draw_text(20, 450, "Kill every alien before", font_medium, ttf_color_white);
    draw_text(20, 520, "they get your cows !", font_medium, ttf_color_white);
    draw_text(530, 600, "BACK", font_medium,
              menu->check_hover(6) ? ttf_color_banana : ttf_color_white);
}

void Renderer::draw_game_over()
{
    draw_text(380, 80, "GAME OVER", font_big, ttf_color_red);

    char buf[80];
    snprintf(buf, sizeof(buf), "Day %d reached", map->day_number);
    draw_text(460, 220, buf, font_medium, ttf_color_white);

    snprintf(buf, sizeof(buf), "%d aliens killed", map->total_aliens_killed);
    draw_text(460, 280, buf, font_medium, ttf_color_white);

    snprintf(buf, sizeof(buf), "%d cows in the pen", map->get_cow_count());
    draw_text(460, 340, buf, font_medium, ttf_color_banana);

    snprintf(buf, sizeof(buf), "%d pen upgrades", map->pen_upgrades);
    draw_text(460, 400, buf, font_medium, ttf_color_white);

    snprintf(buf, sizeof(buf), "$ %d total earned", map->total_coins_earned);
    draw_text(460, 460, buf, font_medium, ttf_color_banana);

    draw_text(380, 580, "Press SPACE to quit", font_medium, ttf_color_white);
}

void Renderer::draw_win_menu()
{
    draw_text(380, 50, "You won !", font_big, ttf_color_white);
    draw_text(250, 170, "Your time : " + menu->timer.get_time_string(), font_medium, ttf_color_white);
    for(int i = 0; i < 5; i++)
    {
        std::string t = std::to_string(i+1) + " - " +
                        menu->timer.get_time_string(menu->leaderboard.scores[i]);
        draw_text(300, 250 + i * 70, t, font_medium, ttf_color_white);
    }
    draw_text(550, 600, "QUIT", font_medium,
              menu->check_hover(7) ? ttf_color_banana : ttf_color_white);
}

//--- main draw ---

void Renderer::draw(uint fps)
{
    //track window size each frame so resize / fullscreen toggles just work
    int cur_w = 0, cur_h = 0;
    SDL_GetWindowSize(window, &cur_w, &cur_h);
    if(cur_w != screen_w || cur_h != screen_h)
        on_window_resize(cur_w, cur_h);

    //rebuild wall mesh if any tile changed (doors opened, rocks shot)
    if(map->dirty)
    {
        build_wall_mesh();
        map->dirty = false;
    }

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_BLEND);

    //=== sky (no depth) ===
    float yaw = player->get_angle();
    float pitch_rad = player->get_pitch();
    draw_sky(pitch_rad);

    //=== 3D pass ===
    glEnable(GL_DEPTH_TEST);
    //camera height: eye-level so the player feels taller than aliens/cows/fences,
    //but still well below trees and the barn
    glm::vec3 cam_pos(player->get_x(), 1.2f, player->get_y());
    glm::vec3 forward(
        cos(pitch_rad) * cos(yaw),
        sin(pitch_rad),
        cos(pitch_rad) * sin(yaw)
    );
    glm::vec3 up_vec(0.0f, 1.0f, 0.0f);
    glm::mat4 view = glm::lookAt(cam_pos, cam_pos + forward, up_vec);
    glm::mat4 proj = glm::perspective(fov, (float)screen_w / (float)screen_h, 0.05f, 200.0f);

    //day -> sunset -> night phase, smooth over the last 30s of each day
    float day_factor;
    if(!map->is_day)                    day_factor = 0.0f;
    else if(map->day_timer > 30.0f)     day_factor = 1.0f;
    else                                day_factor = map->day_timer / 30.0f;

    //fog goes through a warm dusk color too (3-stop lerp)
    const glm::vec3 night_fog (0.08f, 0.10f, 0.16f);
    const glm::vec3 sunset_fog(0.55f, 0.32f, 0.24f);
    const glm::vec3 day_fog   (0.78f, 0.83f, 0.88f);
    glm::vec3 fog_color = (day_factor > 0.5f)
        ? sunset_fog + (day_fog    - sunset_fog) * ((day_factor - 0.5f) * 2.0f)
        : night_fog  + (sunset_fog - night_fog ) * (day_factor * 2.0f);
    const float fog_density = 0.035f - 0.022f * day_factor; //~0.013 in daytime

    //floor
    glUseProgram(solid3d_program);
    glUniformMatrix4fv(u_solid3d_view, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(u_solid3d_proj, 1, GL_FALSE, glm::value_ptr(proj));
    glUniform4f(u_solid3d_color, 0.10f, 0.13f, 0.09f, 1.0f); //dark night grass
    glUniform3f(u_floor_cam_pos, cam_pos.x, cam_pos.y, cam_pos.z);
    glUniform3f(u_floor_fog_color, fog_color.r, fog_color.g, fog_color.b);
    glUniform1f(u_floor_fog_density, fog_density);
    glUniform1f(u_floor_day, day_factor);
    glBindVertexArray(floor_vao);
    glDrawArrays(GL_TRIANGLES, 0, floor_vertex_count);
    glBindVertexArray(0);

    //walls
    glUseProgram(wall_program);
    glUniformMatrix4fv(u_view_loc, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(u_proj_loc, 1, GL_FALSE, glm::value_ptr(proj));
    glUniform3f(u_wall_cam_pos, cam_pos.x, cam_pos.y, cam_pos.z);
    glUniform3f(u_wall_fog_color, fog_color.r, fog_color.g, fog_color.b);
    glUniform1f(u_wall_fog_density, fog_density);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, wall_texture);
    glBindVertexArray(wall_vao);
    glDrawArrays(GL_TRIANGLES, 0, wall_vertex_count);
    glBindVertexArray(0);

    //prep fog uniforms on sprite3d/alien programs once so the per-prop helpers don't need to know
    glUseProgram(sprite3d_program);
    glUniform3f(u_sprite3d_cam_pos, cam_pos.x, cam_pos.y, cam_pos.z);
    glUniform3f(u_sprite3d_fog_color, fog_color.r, fog_color.g, fog_color.b);
    glUniform1f(u_sprite3d_fog_density, fog_density);

    glUseProgram(alien_program);
    glUniform3f(u_alien_cam_pos, cam_pos.x, cam_pos.y, cam_pos.z);
    glUniform3f(u_alien_fog_color, fog_color.r, fog_color.g, fog_color.b);
    glUniform1f(u_alien_fog_density, fog_density);

    //world props (trees, cows, barn, fence, grass)
    draw_decorations(view, proj);

    //sprites (billboards) - flowers, key, weapon FX, etc
    draw_sprites_3d(view, proj, yaw);

    //3D aliens - enemies rendered as procedural mesh
    draw_aliens_3d(view, proj);

    //3D explosions for dead aliens (Temporary sprites with growing size)
    draw_explosions_3d(view, proj);

    //first-person weapon - only while actively playing
    if(menu->current == None)
        draw_player_weapon(view, proj, cam_pos, yaw, pitch_rad);

    //=== 2D UI ===
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if(menu->current == None || menu->current == Shop)
    {
        draw_in_game_hud();
        std::string fps_text = std::to_string(fps) + " FPS";

        std::string phase_text;
        if(map->is_day)
        {
            int sec = (int)map->day_timer;
            int mm = sec / 60;
            int ss = sec % 60;
            char buf[64];
            snprintf(buf, sizeof(buf), "Day %d - %d:%02d left", map->day_number, mm, ss);
            phase_text = buf;
        }
        else
        {
            phase_text = "Night " + std::to_string(map->day_number)
                       + " - " + std::to_string(map->enemy_count) + " aliens left";
        }

        std::string cow_text   = std::to_string(map->get_cow_count())
                                + "/" + std::to_string(map->max_cows) + " cows";
        std::string coins_text = "$ " + std::to_string(map->coins);

        draw_text(10, 10,  phase_text, font_hud, map->is_day ? ttf_color_banana : ttf_color_white);
        draw_text(10, 55,  fps_text,   font_hud, ttf_color_white);
        draw_text(10, 100, cow_text,   font_hud, ttf_color_banana);
        draw_text(10, 145, coins_text, font_hud, ttf_color_banana);
        draw_text(10, screen_h - 130, std::to_string(player->health), font_hud, ttf_color_white);

        if(menu->current == Shop) draw_shop_menu();
    }
    else if(menu->current == Main)     draw_main_menu();
    else if(menu->current == Pause)    draw_pause_menu();
    else if(menu->current == Help)     draw_help_menu();
    else if(menu->current == GameOver) draw_game_over();
    else if(menu->current == Win)      draw_win_menu();

    SDL_GL_SwapWindow(window);
}

Renderer::~Renderer()
{
    if(wall_vbo) glDeleteBuffers(1, &wall_vbo);
    if(wall_vao) glDeleteVertexArrays(1, &wall_vao);
    if(quad_vbo) glDeleteBuffers(1, &quad_vbo);
    if(quad_vao) glDeleteVertexArrays(1, &quad_vao);
    if(floor_vbo) glDeleteBuffers(1, &floor_vbo);
    if(floor_vao) glDeleteVertexArrays(1, &floor_vao);
    if(sprite3d_vbo) glDeleteBuffers(1, &sprite3d_vbo);
    if(sprite3d_vao) glDeleteVertexArrays(1, &sprite3d_vao);
    if(sky_vbo) glDeleteBuffers(1, &sky_vbo);
    if(sky_vao) glDeleteVertexArrays(1, &sky_vao);
    if(wall_texture) glDeleteTextures(1, &wall_texture);
    if(sprites_texture) glDeleteTextures(1, &sprites_texture);
    if(wall_program) glDeleteProgram(wall_program);
    if(sprite_program) glDeleteProgram(sprite_program);
    if(solid_program) glDeleteProgram(solid_program);
    if(solid3d_program) glDeleteProgram(solid3d_program);
    if(sprite3d_program) glDeleteProgram(sprite3d_program);
    if(sky_program) glDeleteProgram(sky_program);

    if(font_big) TTF_CloseFont(font_big);
    if(font_medium) TTF_CloseFont(font_medium);
    TTF_Quit();

    if(gl_context) SDL_GL_DeleteContext(gl_context);
    if(window) SDL_DestroyWindow(window);
    SDL_Quit();

    std::cout << "Renderer deleted" << std::endl;
}
