#include <cmath>
#include "mesh.h"

Mesh::Mesh() : vertices(), vao(0), vbo(0), vertex_count(0) {}

Mesh::~Mesh()
{
    if(vbo) glDeleteBuffers(1, &vbo);
    if(vao) glDeleteVertexArrays(1, &vao);
}

void Mesh::upload()
{
    if(vao == 0)
    {
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(0));
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(3 * sizeof(float)));
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(6 * sizeof(float)));
        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
        glEnableVertexAttribArray(2);
    }
    else
    {
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
    }
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
    vertex_count = vertices.size() / 9;
    glBindVertexArray(0);
}

void Mesh::clear()
{
    vertices.clear();
    vertex_count = 0;
}

static void push_v(std::vector<float>& v, const glm::vec3& p, const glm::vec3& n, const glm::vec3& c)
{
    v.push_back(p.x); v.push_back(p.y); v.push_back(p.z);
    v.push_back(n.x); v.push_back(n.y); v.push_back(n.z);
    v.push_back(c.r); v.push_back(c.g); v.push_back(c.b);
}

static void push_tri(std::vector<float>& v,
                     const glm::vec3& a, const glm::vec3& b, const glm::vec3& c,
                     const glm::vec3& n, const glm::vec3& col)
{
    push_v(v, a, n, col);
    push_v(v, b, n, col);
    push_v(v, c, n, col);
}

static void push_quad(std::vector<float>& v,
                      const glm::vec3& a, const glm::vec3& b,
                      const glm::vec3& c, const glm::vec3& d,
                      const glm::vec3& n, const glm::vec3& col)
{
    push_tri(v, a, b, c, n, col);
    push_tri(v, a, c, d, n, col);
}

void add_box(Mesh& m, const glm::vec3& center, const glm::vec3& size, const glm::vec3& color)
{
    glm::vec3 h = size * 0.5f;
    glm::vec3 v000 = center + glm::vec3(-h.x, -h.y, -h.z);
    glm::vec3 v100 = center + glm::vec3( h.x, -h.y, -h.z);
    glm::vec3 v010 = center + glm::vec3(-h.x,  h.y, -h.z);
    glm::vec3 v110 = center + glm::vec3( h.x,  h.y, -h.z);
    glm::vec3 v001 = center + glm::vec3(-h.x, -h.y,  h.z);
    glm::vec3 v101 = center + glm::vec3( h.x, -h.y,  h.z);
    glm::vec3 v011 = center + glm::vec3(-h.x,  h.y,  h.z);
    glm::vec3 v111 = center + glm::vec3( h.x,  h.y,  h.z);

    push_quad(m.vertices, v100, v101, v111, v110, glm::vec3( 1, 0, 0), color); //+X
    push_quad(m.vertices, v001, v000, v010, v011, glm::vec3(-1, 0, 0), color); //-X
    push_quad(m.vertices, v010, v110, v111, v011, glm::vec3( 0, 1, 0), color); //+Y
    push_quad(m.vertices, v000, v001, v101, v100, glm::vec3( 0,-1, 0), color); //-Y
    push_quad(m.vertices, v001, v011, v111, v101, glm::vec3( 0, 0, 1), color); //+Z
    push_quad(m.vertices, v000, v100, v110, v010, glm::vec3( 0, 0,-1), color); //-Z
}

void add_sphere(Mesh& m, const glm::vec3& center, float radius, const glm::vec3& color, int segments)
{
    int stacks = segments < 3 ? 3 : segments;
    int slices = stacks * 2;

    for(int i = 0; i < stacks; i++)
    {
        float th1 = (float)M_PI * (float)i / stacks - (float)M_PI * 0.5f;
        float th2 = (float)M_PI * (float)(i + 1) / stacks - (float)M_PI * 0.5f;
        float y1 = sinf(th1), y2 = sinf(th2);
        float r1 = cosf(th1), r2 = cosf(th2);
        for(int j = 0; j < slices; j++)
        {
            float ph1 = 2.0f * (float)M_PI * (float)j / slices;
            float ph2 = 2.0f * (float)M_PI * (float)(j + 1) / slices;
            float c1 = cosf(ph1), s1 = sinf(ph1);
            float c2 = cosf(ph2), s2 = sinf(ph2);

            glm::vec3 d1(r1 * c1, y1, r1 * s1);
            glm::vec3 d2(r2 * c1, y2, r2 * s1);
            glm::vec3 d3(r2 * c2, y2, r2 * s2);
            glm::vec3 d4(r1 * c2, y1, r1 * s2);

            glm::vec3 p1 = center + radius * d1;
            glm::vec3 p2 = center + radius * d2;
            glm::vec3 p3 = center + radius * d3;
            glm::vec3 p4 = center + radius * d4;

            push_v(m.vertices, p1, d1, color);
            push_v(m.vertices, p2, d2, color);
            push_v(m.vertices, p3, d3, color);

            push_v(m.vertices, p1, d1, color);
            push_v(m.vertices, p3, d3, color);
            push_v(m.vertices, p4, d4, color);
        }
    }
}

static void add_hand(Mesh& m, float hand_x, float hand_y, const glm::vec3& color)
{
    //small rounded hand (no fingers - skipped per request, looked off at low poly)
    add_box(m, glm::vec3(hand_x, hand_y, 0.0f), glm::vec3(0.08f, 0.07f, 0.05f), color);
}

static const glm::vec3 SKIN(0.55f, 0.60f, 0.55f);
static const glm::vec3 EYE (0.02f, 0.02f, 0.02f);

void build_alien_body(Mesh& m, float leg_phase)
{
    m.clear();

    //torso
    add_box(m, glm::vec3(0.0f, 0.50f, 0.0f), glm::vec3(0.18f, 0.26f, 0.11f), SKIN);

    //shoulders (smooth the arm-torso junction)
    add_sphere(m, glm::vec3(-0.10f, 0.61f, 0.0f), 0.05f, SKIN, 6);
    add_sphere(m, glm::vec3( 0.10f, 0.61f, 0.0f), 0.05f, SKIN, 6);

    //arms (long, hanging)
    add_box(m, glm::vec3(-0.13f, 0.43f, 0.0f), glm::vec3(0.055f, 0.36f, 0.055f), SKIN);
    add_box(m, glm::vec3( 0.13f, 0.43f, 0.0f), glm::vec3(0.055f, 0.36f, 0.055f), SKIN);

    //hands
    add_hand(m, -0.135f, 0.235f, SKIN);
    add_hand(m,  0.135f, 0.235f, SKIN);

    //hips
    add_box(m, glm::vec3(0.0f, 0.36f, 0.0f), glm::vec3(0.17f, 0.06f, 0.10f), SKIN);

    //legs - alternate forward/back along Z based on leg_phase
    float swing = 0.10f * cosf(leg_phase * (float)M_PI);
    add_box(m, glm::vec3(-0.06f, 0.20f, -swing), glm::vec3(0.075f, 0.32f, 0.075f), SKIN);
    add_box(m, glm::vec3( 0.06f, 0.20f,  swing), glm::vec3(0.075f, 0.32f, 0.075f), SKIN);
}

void build_alien_head(Mesh& m)
{
    m.clear();

    //neck connector - short stub so the head can twist over the body
    add_box(m, glm::vec3(0.0f, 0.665f, 0.0f), glm::vec3(0.055f, 0.06f, 0.055f), SKIN);

    //head sphere
    add_sphere(m, glm::vec3(0.0f, 0.80f, 0.0f), 0.135f, SKIN, 8);

    //eyes - dark almonds, slight forward offset
    add_sphere(m, glm::vec3(-0.05f, 0.80f, 0.11f), 0.038f, EYE, 6);
    add_sphere(m, glm::vec3( 0.05f, 0.80f, 0.11f), 0.038f, EYE, 6);
}

//--- world props ---

void build_tree(Mesh& m)
{
    m.clear();
    const glm::vec3 BARK(0.32f, 0.22f, 0.14f);
    const glm::vec3 LEAF(0.10f, 0.30f, 0.13f);

    //trunk - tall so the tree towers over the player
    add_box(m, glm::vec3(0.0f, 1.20f, 0.0f), glm::vec3(0.32f, 2.40f, 0.32f), BARK);
    //foliage - 3 overlapping spheres on top
    add_sphere(m, glm::vec3( 0.0f,  2.85f, 0.0f),  0.85f, LEAF, 8);
    add_sphere(m, glm::vec3( 0.40f, 3.05f, 0.15f), 0.60f, LEAF, 6);
    add_sphere(m, glm::vec3(-0.35f, 3.00f,-0.20f), 0.60f, LEAF, 6);
}

void build_cow(Mesh& m)
{
    m.clear();
    const glm::vec3 WHITE(0.92f, 0.92f, 0.88f);
    const glm::vec3 SPOT (0.10f, 0.08f, 0.06f);
    const glm::vec3 LEG  (0.20f, 0.18f, 0.15f);
    const glm::vec3 SNOUT(0.85f, 0.70f, 0.70f);

    //body - elongated like a real cow (longer than tall)
    add_box(m, glm::vec3(0.0f, 0.65f, 0.0f), glm::vec3(0.62f, 0.50f, 1.25f), WHITE);

    //head - forward of body, slightly higher
    add_box(m, glm::vec3(0.0f, 0.80f, 0.80f), glm::vec3(0.36f, 0.38f, 0.34f), WHITE);
    //snout
    add_box(m, glm::vec3(0.0f, 0.72f, 1.02f), glm::vec3(0.22f, 0.20f, 0.12f), SNOUT);

    //black spots scattered on the body
    add_box(m, glm::vec3( 0.22f, 0.90f,  0.25f), glm::vec3(0.24f, 0.05f, 0.26f), SPOT);
    add_box(m, glm::vec3(-0.20f, 0.90f, -0.30f), glm::vec3(0.26f, 0.05f, 0.28f), SPOT);
    add_box(m, glm::vec3( 0.32f, 0.65f, -0.20f), glm::vec3(0.04f, 0.30f, 0.30f), SPOT);

    //4 legs - taller, slightly thicker
    add_box(m, glm::vec3( 0.22f, 0.20f,  0.45f), glm::vec3(0.11f, 0.40f, 0.11f), LEG);
    add_box(m, glm::vec3(-0.22f, 0.20f,  0.45f), glm::vec3(0.11f, 0.40f, 0.11f), LEG);
    add_box(m, glm::vec3( 0.22f, 0.20f, -0.45f), glm::vec3(0.11f, 0.40f, 0.11f), LEG);
    add_box(m, glm::vec3(-0.22f, 0.20f, -0.45f), glm::vec3(0.11f, 0.40f, 0.11f), LEG);
}

void build_barn(Mesh& m)
{
    m.clear();
    const glm::vec3 RED  (0.60f, 0.12f, 0.10f);
    const glm::vec3 ROOF (0.20f, 0.18f, 0.16f);
    const glm::vec3 DOOR (0.18f, 0.10f, 0.06f);
    const glm::vec3 TRIM (0.95f, 0.90f, 0.80f);

    //main barn body - tall and elongated like a real barn (7 wide x 5 tall x 9 deep)
    add_box(m, glm::vec3(0.0f, 2.50f, 0.0f), glm::vec3(7.0f, 5.0f, 9.0f), RED);

    //tiered gambrel-style roof (3 stacked layers tapering up)
    add_box(m, glm::vec3(0.0f, 5.15f, 0.0f), glm::vec3(7.30f, 0.30f, 9.30f), ROOF);
    add_box(m, glm::vec3(0.0f, 5.55f, 0.0f), glm::vec3(6.30f, 0.40f, 8.20f), ROOF);
    add_box(m, glm::vec3(0.0f, 6.00f, 0.0f), glm::vec3(4.80f, 0.40f, 6.40f), ROOF);

    //big front double-door (faces +Z, the long side has the entrance)
    add_box(m, glm::vec3(0.0f, 1.80f, 4.51f), glm::vec3(2.60f, 3.60f, 0.04f), DOOR);

    //white trim band along the top of long sides
    add_box(m, glm::vec3(0.0f, 4.92f, 4.51f), glm::vec3(7.05f, 0.18f, 0.04f), TRIM);
    add_box(m, glm::vec3(0.0f, 4.92f,-4.51f), glm::vec3(7.05f, 0.18f, 0.04f), TRIM);
}

void build_fence_section(Mesh& m)
{
    m.clear();
    const glm::vec3 WOOD(0.45f, 0.30f, 0.18f);

    //two vertical posts (1m apart, ~0.95m tall)
    add_box(m, glm::vec3(-0.50f, 0.475f, 0.0f), glm::vec3(0.10f, 0.95f, 0.10f), WOOD);
    add_box(m, glm::vec3( 0.50f, 0.475f, 0.0f), glm::vec3(0.10f, 0.95f, 0.10f), WOOD);
    //two horizontal rails
    add_box(m, glm::vec3(0.0f, 0.75f, 0.0f), glm::vec3(1.00f, 0.08f, 0.05f), WOOD);
    add_box(m, glm::vec3(0.0f, 0.35f, 0.0f), glm::vec3(1.00f, 0.08f, 0.05f), WOOD);
}

void build_grass_tuft(Mesh& m)
{
    m.clear();
    const glm::vec3 GRASS(0.18f, 0.30f, 0.10f);

    //two thin crossed blades so it reads as grass from any angle
    add_box(m, glm::vec3(0.0f, 0.18f, 0.0f), glm::vec3(0.05f, 0.36f, 0.02f), GRASS);
    add_box(m, glm::vec3(0.0f, 0.18f, 0.0f), glm::vec3(0.02f, 0.36f, 0.05f), GRASS);
}

void build_light_beam(Mesh& m)
{
    m.clear();
    const glm::vec3 BEAM_BRIGHT(0.45f, 1.00f, 0.55f); //saturated alien green
    const glm::vec3 BEAM_CORE  (0.85f, 1.00f, 0.85f); //inner near-white glow

    //tall vertical pillar reaching well above the cow into the sky
    add_box(m, glm::vec3(0.0f, 2.20f, 0.0f), glm::vec3(0.55f, 4.40f, 0.55f), BEAM_BRIGHT);
    //inner brighter core
    add_box(m, glm::vec3(0.0f, 2.20f, 0.0f), glm::vec3(0.20f, 4.40f, 0.20f), BEAM_CORE);
}

//--- shotgun (first-person), built with +Z pointing down the barrel ---
void build_shotgun(Mesh& m)
{
    m.clear();
    const glm::vec3 METAL (0.18f, 0.18f, 0.20f); //dark gunmetal
    const glm::vec3 WOOD  (0.30f, 0.18f, 0.08f); //walnut
    const glm::vec3 SIGHT (0.85f, 0.85f, 0.85f); //bright front sight
    const glm::vec3 SKIN  (0.84f, 0.63f, 0.49f); //tanned farmer skin
    const glm::vec3 SHIRT (0.48f, 0.14f, 0.12f); //red flannel work shirt sleeve

    //barrel (long, slim, +Z direction)
    add_box(m, glm::vec3(0.0f, 0.05f, 0.30f), glm::vec3(0.045f, 0.045f, 0.55f), METAL);
    //receiver (center of gun)
    add_box(m, glm::vec3(0.0f, 0.04f, 0.0f), glm::vec3(0.065f, 0.10f, 0.16f), METAL);
    //pump / fore-end grip below the barrel
    add_box(m, glm::vec3(0.0f, -0.03f, 0.16f), glm::vec3(0.075f, 0.07f, 0.18f), WOOD);
    //trigger guard
    add_box(m, glm::vec3(0.0f, -0.05f, -0.04f), glm::vec3(0.045f, 0.03f, 0.04f), METAL);
    //pistol grip (angled down behind receiver)
    add_box(m, glm::vec3(0.0f, -0.13f, -0.06f), glm::vec3(0.05f, 0.13f, 0.07f), WOOD);
    //stock (extends back)
    add_box(m, glm::vec3(0.0f, -0.06f, -0.24f), glm::vec3(0.05f, 0.085f, 0.22f), WOOD);
    //front sight (tiny bump on barrel tip)
    add_box(m, glm::vec3(0.0f, 0.085f, 0.555f), glm::vec3(0.012f, 0.025f, 0.012f), SIGHT);

    //--- farmer's left arm reaching across to grip the barrel ---
    //hand wraps the barrel mid-length (skin)
    add_box(m, glm::vec3(-0.02f, 0.015f, 0.36f), glm::vec3(0.090f, 0.085f, 0.15f), SKIN);
    //wrist coming in from the left, slightly below the barrel
    add_box(m, glm::vec3(-0.11f, -0.03f, 0.26f), glm::vec3(0.080f, 0.080f, 0.10f), SKIN);
    //forearm extending diagonally back-left toward the player's shoulder (still skin - sleeve is rolled up)
    add_box(m, glm::vec3(-0.18f, -0.09f, 0.13f), glm::vec3(0.085f, 0.085f, 0.20f), SKIN);
    //tip of the red flannel sleeve where it disappears behind the camera
    add_box(m, glm::vec3(-0.22f, -0.14f, -0.02f), glm::vec3(0.095f, 0.095f, 0.10f), SHIRT);
}

//--- muzzle flash (rendered for one frame at the gun muzzle when firing) ---
void build_muzzle_flash(Mesh& m)
{
    m.clear();
    const glm::vec3 CORE  (1.00f, 0.95f, 0.70f); //bright near-white
    const glm::vec3 OUTER (1.00f, 0.55f, 0.10f); //hot orange
    const glm::vec3 EMBER (1.00f, 0.30f, 0.05f); //red-orange tips

    add_sphere(m, glm::vec3(0.0f, 0.0f, 0.0f), 0.09f, OUTER, 6);
    add_sphere(m, glm::vec3(0.0f, 0.0f, 0.0f), 0.06f, CORE,  6);
    //small spiky outer specks for an irregular flash silhouette
    add_box(m, glm::vec3( 0.10f, 0.0f, 0.04f), glm::vec3(0.06f, 0.025f, 0.025f), EMBER);
    add_box(m, glm::vec3(-0.10f, 0.0f, 0.04f), glm::vec3(0.06f, 0.025f, 0.025f), EMBER);
    add_box(m, glm::vec3( 0.0f, 0.10f, 0.04f), glm::vec3(0.025f, 0.06f, 0.025f), EMBER);
    add_box(m, glm::vec3( 0.0f,-0.10f, 0.04f), glm::vec3(0.025f, 0.06f, 0.025f), EMBER);
}

//altitude at which the saucer hovers in world space (mesh-relative)
const float UFO_ALTITUDE = 13.0f;

//flying saucer hovering above the cow pen. classic disc with green underside lights
void build_ufo(Mesh& m)
{
    m.clear();
    const glm::vec3 HULL  (0.16f, 0.16f, 0.18f); //dark metal
    const glm::vec3 BEVEL (0.28f, 0.28f, 0.32f); //lighter accent
    const glm::vec3 DOME  (0.55f, 0.65f, 0.78f); //cool silver-blue
    const glm::vec3 LIGHT (0.45f, 1.00f, 0.50f); //bright alien green - matches the beam

    const float Y = UFO_ALTITUDE;

    //main saucer disc - wide flat box
    add_box(m, glm::vec3(0.0f, Y,        0.0f), glm::vec3(3.6f, 0.45f, 3.6f), HULL);
    //chamfered upper bevel
    add_box(m, glm::vec3(0.0f, Y + 0.35f, 0.0f), glm::vec3(2.8f, 0.30f, 2.8f), BEVEL);
    //dome base
    add_box(m, glm::vec3(0.0f, Y + 0.65f, 0.0f), glm::vec3(1.5f, 0.20f, 1.5f), BEVEL);
    //dome (cockpit)
    add_sphere(m, glm::vec3(0.0f, Y + 1.10f, 0.0f), 0.80f, DOME, 10);
    //lower bevel for the rim
    add_box(m, glm::vec3(0.0f, Y - 0.20f, 0.0f), glm::vec3(3.0f, 0.20f, 3.0f), BEVEL);
    //underside light ring (8 glowing nodes)
    for(int i = 0; i < 8; i++)
    {
        float ang = (float)i * 2.0f * (float)M_PI / 8.0f;
        float r = 1.55f;
        add_sphere(m, glm::vec3(cosf(ang) * r, Y - 0.35f, sinf(ang) * r), 0.18f, LIGHT, 5);
    }
}

//--- alien gore explosion: bursts of green chunks around a smoky core ---
//drawn for a Temporary sprite; the sprite's growing `size` field scales the whole burst
void build_alien_explosion(Mesh& m)
{
    m.clear();
    const glm::vec3 GORE_BRIGHT(0.40f, 0.95f, 0.25f); //alien blood
    const glm::vec3 GORE_DARK  (0.18f, 0.50f, 0.12f); //deeper green
    const glm::vec3 SMOKE      (0.10f, 0.10f, 0.10f);

    //central smoke puff
    add_sphere(m, glm::vec3(0.0f, 0.0f, 0.0f), 0.18f, SMOKE, 6);
    //6 chunks radiating outward (alien chest height assumed baked in by the call site)
    add_sphere(m, glm::vec3( 0.22f, 0.0f, 0.0f),  0.08f, GORE_BRIGHT, 5);
    add_sphere(m, glm::vec3(-0.22f, 0.0f, 0.0f),  0.08f, GORE_BRIGHT, 5);
    add_sphere(m, glm::vec3( 0.0f, 0.0f,  0.22f), 0.08f, GORE_BRIGHT, 5);
    add_sphere(m, glm::vec3( 0.0f, 0.0f, -0.22f), 0.08f, GORE_BRIGHT, 5);
    add_sphere(m, glm::vec3( 0.0f, 0.20f, 0.0f),  0.07f, GORE_BRIGHT, 5);
    add_sphere(m, glm::vec3( 0.0f,-0.18f, 0.0f),  0.07f, GORE_DARK,   5);
    //diagonal speckles
    add_sphere(m, glm::vec3( 0.15f, 0.12f, 0.10f), 0.05f, GORE_DARK, 4);
    add_sphere(m, glm::vec3(-0.14f, 0.10f,-0.13f), 0.05f, GORE_DARK, 4);
}
