#ifndef _MAP_H_
#define _MAP_H_

#include <SDL2/SDL.h>
#include <vector>

enum SpriteType {Decoration, Enemy, Key, Temporary};

struct Sprite
{
	float x, y = 0;
	unsigned short size = 600;
	unsigned short itex = 0;
	SpriteType type = Decoration;
	//squared distance from the player, it's used to sort sprites so no need to calculate the square root
	float sqr_dist = 0;
	Uint32 start_time = 0;
	//two yaws (radians) so the body and head rotate independently.
	//mutable so the renderer can update them while iterating over a const vector.
	mutable float head_yaw = 0;       //points toward whatever the alien is pursuing
	mutable bool  head_yaw_init = false;
	mutable float body_yaw = 0;       //points along direction of motion
	mutable bool  body_yaw_init = false;
	mutable float prev_x = 0, prev_y = 0; //previous position, for movement detection
	mutable bool  prev_init = false;
	//current pursuit target in world space (set by Map::update_sprites)
	mutable float target_x = 0;
	mutable float target_y = 0;
	mutable bool  target_init = false;
	//stuck detection + detour: if the alien tried to move but barely budged for 2s
	//we route it to a random nearby point for ~1.5s to dislodge it from a tree etc.
	mutable float stuck_timer  = 0;
	mutable float detour_timer = 0;
	mutable float detour_x     = 0;
	mutable float detour_y     = 0;

	bool operator < (const Sprite& s) const
	{
		//sprites further away are sorted before near sprites
		return sqr_dist > s.sqr_dist;
	}

	Sprite() : x(0), y(0), size(600), itex(0), type(Decoration), sqr_dist(0),
	           start_time(SDL_GetTicks()),
	           head_yaw(0), head_yaw_init(false),
	           body_yaw(0), body_yaw_init(false),
	           prev_x(0), prev_y(0), prev_init(false),
	           target_x(0), target_y(0), target_init(false),
	           stuck_timer(0), detour_timer(0), detour_x(0), detour_y(0) {}
};

struct Door
{
	unsigned short x, y = 0;
	float animationState = 1; //0 = fully opened, 0.5 = half opened, 1 = fully closed

	Door() : x(0), y(0), animationState(1) {}
};

//world props (3D meshes), drawn by the renderer and queried for collision
enum PropType { PropBarn, PropCow, PropFence, PropTree, PropGrass, PropBeam, PropUFO, PropShop };

struct Prop
{
	mutable float x = 0, z = 0;  //world position (mutable so cows can wander)
	mutable float yaw = 0;       //rotation around Y (mutable so cows can face their walk direction)
	PropType type = PropGrass;
	float half_w = 0;            //AABB half-extent on X
	float half_d = 0;            //AABB half-extent on Z
	bool solid = false;          //blocks player movement
	mutable bool active = true;  //flipped off by abduction / fence break / beam expiry
	mutable Uint32 expire_at = 0; //SDL ticks ms; 0 = permanent. used for short-lived beam visuals
	mutable float damage_timer = 0; //seconds an alien has been pressing on this prop (fence only)
	//cow wander state (cows only)
	mutable float wander_x = 0, wander_y = 0;
	mutable bool wander_init = false;

	Prop() {}
	Prop(float ix, float iz, float iyaw, PropType t, float hw, float hd, bool s)
		: x(ix), z(iz), yaw(iyaw), type(t), half_w(hw), half_d(hd), solid(s),
		  active(true), expire_at(0), damage_timer(0),
		  wander_x(0), wander_y(0), wander_init(false) {}
};

class Map
{
	public:
        Map();
        unsigned short w = 0;
		unsigned short h = 0;

		float speed = 0.03; //enemy's speed, changes based on difficulty
		int damage = 0; //damage inflicted by each enemy, changes based on difficulty
		int enemy_count = 0;  //aliens still needed to kill for the win
		bool dirty = false; //true when a tile changed and the wall mesh needs rebuilding
		//progressive spawn: aliens come from edges over time, not pre-placed
		int   wave_size      = 12;
		int   spawned_count  = 0;
		float next_spawn_in  = 3.0f; //seconds until next spawn
		float spawn_interval = 3.5f; //seconds between spawns

		//run stats - shown on the game over screen
		int   total_aliens_killed = 0;
		int   total_coins_earned  = 0; //lifetime, never spent down

		//economy
		int   coins        = 30;      //starting capital
		int   max_cows     = 6;       //pen capacity, grows with upgrades
		int   pen_upgrades = 0;       //counter for game over stat
		int   coins_per_cow_per_day = 10;

		//day / night cycle
		bool  is_day         = true;
		int   day_number     = 1;
		float day_timer      = 120.0f; //seconds remaining in current day
		float day_length     = 120.0f;

		//UFO hovering above the cow pen; set by populate_farm. queried by the
		//renderer so abduction beams can slant up to it.
		float ufo_x = 0;
		float ufo_z = 0;
		//shop building - queried by player for interaction range + renderer for prompt
		float shop_x = 0;
		float shop_z = 0;
		//cow pen geometry - needed to spawn new cows and to rebuild on upgrade
		float pen_cx = 0;
		float pen_cz = 0;
		float pen_w  = 10.0f;
		float pen_d  = 8.0f;
		
		char get_tile(unsigned short x, unsigned short y);
		void set_tile(unsigned short x, unsigned short y, char tile);
		const std::vector<Prop>& get_props() const { return props; }
		//ignore_cows: pass true when the player asks - cows are visual only against the
		//player so a wandering cow can't pin them in place
		bool is_blocked(float wx, float wz, float player_radius = 0.25f, bool ignore_cows = false) const;
		int  get_cow_count() const; //counts active cows (used for HUD + lose condition)
		void populate_farm(); //hardcoded barn + cow pen + procedural trees/grass scatter
		void spawn_alien_at_edge(); //pops a fresh enemy at a random map edge
		void repair_fences();       //bring every fence section back online
		bool try_buy_cow();         //add a cow inside the pen if there's room; returns success
		void upgrade_pen();         //grow pen, rebuild fences, raise max_cows
		void sort_sprites(float player_x, float player_y); //sorts sprites in the vector based on the distance from the player
		std::vector<Sprite> const& get_sprites();
		void delete_sprite(unsigned short id); //deletes sprite at specified index in the vector
		void update_sprites(float player_x, float player_y, float dt); //moves enemies so they follow the player and changes the size of temp sprites
		Door get_door(unsigned short x, unsigned short y); //tries to retrive a door at specified coordinates, return 0 if not found
		bool update_doors(float player_x, float player_y, float dt); //updates the animation state if the player is close enough
		void animate_sprites(); //swaps sprites for turkeys animation
		int damage_player(); //returns the amount of damage the player should receive based on difficulty and nearby turkeys
		bool pickup_keys(); //returns true and remove the key from the map if the player is close enough, returns false if no key is picked up
		void add_temp_sprite(ushort itex, float x, float y, ushort size); //adds a sprite to the map that is delete 500ms later (for explosions)
		void update_dist_map(unsigned short px, unsigned short py);
		~Map();

		//forbids copy constructor to avoid warning in c++11
        Map(const Map& m) = delete;
        Map& operator=(const Map& m) = delete;

	private:
		char* map;
		unsigned short* dist;
		std::vector<Sprite> sprites;
		std::vector<Door> doors;
		std::vector<Prop> props;
		Uint32 get_pixel(SDL_Surface* source, unsigned short x, unsigned short y);
};

#endif