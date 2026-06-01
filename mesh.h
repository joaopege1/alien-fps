#ifndef _MESH_H_
#define _MESH_H_

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <vector>

//9 floats per vertex: pos.xyz, normal.xyz, color.rgb
//layout location: 0 = pos, 1 = normal, 2 = color
struct Mesh
{
    std::vector<float> vertices;
    GLuint vao;
    GLuint vbo;
    GLsizei vertex_count;

    Mesh();
    ~Mesh();

    void upload();
    void clear();

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
};

//procedural geometry helpers - append into mesh.vertices
void add_box(Mesh& m, const glm::vec3& center, const glm::vec3& size, const glm::vec3& color);
void add_sphere(Mesh& m, const glm::vec3& center, float radius, const glm::vec3& color, int segments = 8);

//body: torso, shoulders, arms, hands, hips, legs. animates legs via leg_phase (0 or 1)
void build_alien_body(Mesh& m, float leg_phase);
//head: neck, head sphere, eyes. rendered with its own yaw so it can twist independently
void build_alien_head(Mesh& m);

//world props - all centered on origin, drawn via a model matrix per instance
void build_tree(Mesh& m);
void build_cow(Mesh& m);
void build_barn(Mesh& m);
void build_shop(Mesh& m);
void build_fence_section(Mesh& m);
void build_grass_tuft(Mesh& m);
void build_light_beam(Mesh& m);

//first-person weapon + effects, built in local space with +Z = barrel direction
void build_shotgun(Mesh& m);
void build_muzzle_flash(Mesh& m);
void build_alien_explosion(Mesh& m);

//flying saucer hovering above the cow pen. mesh is self-elevating so a Prop at
//(x, _, z) renders the disc at altitude UFO_ALTITUDE below.
void build_ufo(Mesh& m);
extern const float UFO_ALTITUDE;

#endif
