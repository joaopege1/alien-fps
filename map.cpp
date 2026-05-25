#include <iostream>
#include <cmath>
#include <algorithm>
#include "map.h"

Map::Map() : speed(1.2), damage(0), enemy_count(0), map(NULL), dist(NULL),
             sprites(std::vector<Sprite>()), doors(std::vector<Door>()), props(std::vector<Prop>())
{
	//Loading the map texture
	SDL_Surface* map_tex = SDL_LoadBMP("map.bmp");

	//Error handling for texture loading
    if(!map_tex)
    {
        std::cerr << "Couldn't load texture file " << SDL_GetError() << std::endl;
        return;
    }

    if(map_tex->format->BytesPerPixel != 3)
    {
        std::cerr << "Map must be using 8bpp format." << std::endl;
        return;
    }

    //sanity check on map size (square, reasonable bounds)
    if(map_tex->w != map_tex->h || map_tex->w < 16 || map_tex->w > 256)
    {
        std::cerr << "Map must be square between 16x16 and 256x256." << std::endl;
        return;
    }

    w = map_tex->w;
    h = map_tex->h;

    map = new char[w*h];
    dist = new ushort[w*h];

    std::cout<<w<<';'<<h<<"\n";

	for(ushort y = 0; y < h; y++)
	{
		for(ushort x = 0; x < w; x++)
		{
			int id = y * w + x;
			Uint32 pixel = get_pixel(map_tex, x, y);

			if(pixel == 0) //black 0,0,0 : grass
				map[id] = '0';
			else if(pixel == 65280) //green 0,255,0 : bricks
				map[id] = '1';
			else if(pixel == 255) //blue 0,0,255 : cobblestone
				map[id] = '2';
			else if(pixel == 16776960) //yellow 255,255,0 : door
			{
				map[id] = '3';

				unsigned int index = doors.size();
				doors.push_back(Door());
				doors.at(index).x = x;
				doors.at(index).y = y;
				doors.at(index).animationState = 1;
			}
			else if(pixel == 16711680) //red 255,0,0 : enemy
			{
				map[id] = ' ';
				
				unsigned int index = sprites.size();
				sprites.push_back(Sprite());
				sprites.at(index).x = x + (std::rand() % 100) / 100.0;
				sprites.at(index).y = y + (std::rand() % 100) / 100.0;
				sprites.at(index).itex = std::rand() % 2 == 1 ? 1 : 4;
				sprites.at(index).type = Enemy;
				sprites.at(index).size = 500 + ((id * 50) % 100);
				enemy_count++;
			}
			else if(pixel == 65535) //cyan 0,255,255 : flowers
			{
				map[id] = ' ';

				for(int i = 0; i < 6; i++)
				{
					unsigned int index = sprites.size();
					sprites.push_back(Sprite());
					sprites.at(index).x = x + (std::rand() % 100) / 100.0;
					sprites.at(index).y = y + (std::rand() % 100) / 100.0;
					sprites.at(index).itex = 2;
					sprites.at(index).type = Decoration;
					sprites.at(index).size = 200 + ((id * 50) % 100);
				}
			}
			else if(pixel == 16711935) //magenta 255,0,255 : key
			{
				map[id] = ' ';

				unsigned int index = sprites.size();
				sprites.push_back(Sprite());
				sprites.at(index).x = x;
				sprites.at(index).y = y;
				sprites.at(index).itex = 5;
				sprites.at(index).type = Key;
				sprites.at(index).size = 400;
			}
			else //empty
				map[id] = ' ';
			
			std::cout<<get_tile(x, y);
		}
		std::cout<<std::endl;
	}
	std::cout<<"Level loaded with "<<sprites.size()<<" sprites"<<std::endl;
	SDL_FreeSurface(map_tex);

	populate_farm();
	std::cout<<"Populated "<<props.size()<<" world props"<<std::endl;

	update_dist_map(2, 2);
}

int Map::get_cow_count() const
{
	int n = 0;
	for(unsigned int i = 0; i < props.size(); i++)
		if(props[i].type == PropCow && props[i].active) n++;
	return n;
}

void Map::spawn_alien_at_edge()
{
	//pick a random edge of the playable area; sit just inside the border so we don't
	//immediately clamp the alien on its first move. retry a few times if we'd land on
	//top of another alien.
	const float alien_min_gap_sqr = 1.0f * 1.0f;
	float sx = 0.0f, sy = 0.0f;
	bool ok = false;
	for(int tries = 0; tries < 20 && !ok; tries++)
	{
		int edge = std::rand() % 4;
		switch(edge)
		{
			case 0: sx = 1.0f + (std::rand() % (w - 2)); sy = 0.8f;          break; //N
			case 1: sx = 1.0f + (std::rand() % (w - 2)); sy = (float)h - 0.8f; break; //S
			case 2: sx = 0.8f;          sy = 1.0f + (std::rand() % (h - 2)); break; //W
			case 3: sx = (float)w - 0.8f; sy = 1.0f + (std::rand() % (h - 2)); break; //E
		}
		ok = true;
		//don't land on top of a tree, barn, fence, etc.
		if(is_blocked(sx, sy, 0.30f, false)) ok = false;
		for(unsigned int j = 0; j < sprites.size() && ok; j++)
		{
			if(sprites[j].type != Enemy) continue;
			float dx = sprites[j].x - sx;
			float dz = sprites[j].y - sy;
			if(dx * dx + dz * dz < alien_min_gap_sqr) { ok = false; break; }
		}
	}

	sprites.push_back(Sprite());
	Sprite& s = sprites.back();
	s.x = sx;
	s.y = sy;
	s.itex = (std::rand() % 2 == 1) ? 1 : 4;
	s.type = Enemy;
	s.size = 500 + (std::rand() % 100);

	//pre-seed yaws toward the center of the map so the alien doesn't visibly spin in place
	float dx = (w * 0.5f) - sx;
	float dz = (h * 0.5f) - sy;
	s.body_yaw       = atan2f(dx, dz);
	s.body_yaw_init  = true;
	s.head_yaw       = s.body_yaw;
	s.head_yaw_init  = true;
}

bool Map::is_blocked(float wx, float wz, float player_radius, bool ignore_cows) const
{
	for(unsigned int i = 0; i < props.size(); i++)
	{
		const Prop& p = props[i];
		if(!p.solid || !p.active) continue;
		if(ignore_cows && p.type == PropCow) continue;
		//treat each prop's footprint as axis-aligned; ignore yaw for simplicity in draft
		float dx = fabsf(wx - p.x);
		float dz = fabsf(wz - p.z);
		if(dx < p.half_w + player_radius && dz < p.half_d + player_radius)
			return true;
	}
	return false;
}

void Map::populate_farm()
{
	//deterministic procedural scatter so the layout is consistent across runs
	std::srand(42);

	//progressive alien spawn config (override any enemy_count set by map.bmp loader)
	enemy_count    = wave_size;
	spawned_count  = 0;
	next_spawn_in  = 3.0f;

	//--- the farm (near center of the field so the player can reach it from spawn) ---
	float fcx = w * 0.5f; //farm center x
	float fcz = h * 0.45f; //slightly north of center

	//barn footprint matches the mesh: 7 wide x 9 deep
	float barn_x = fcx, barn_z = fcz - 6.0f;
	props.push_back(Prop(barn_x, barn_z, 0.0f, PropBarn, 3.5f, 4.5f, true));

	//cow pen: a fenced rectangle south of the barn
	float pen_cx = fcx, pen_cz = fcz + 5.0f;
	float pen_w = 10.0f, pen_d = 8.0f; //~+27% area for more cow wandering room

	//remember the UFO position so the renderer can slant abduction beams up to it
	ufo_x = pen_cx;
	ufo_z = pen_cz;
	//UFO prop (mesh self-elevates to UFO_ALTITUDE, no collision)
	props.push_back(Prop(ufo_x, ufo_z, 0.0f, PropUFO, 0.0f, 0.0f, false));
	//fence sections along the 4 edges (each section is 1m wide; we tile them)
	int n_x = (int)pen_w; //~5 sections along X
	int n_z = (int)pen_d; //~4 sections along Z
	float left = pen_cx - pen_w * 0.5f;
	float right = pen_cx + pen_w * 0.5f;
	float top = pen_cz - pen_d * 0.5f;
	float bottom = pen_cz + pen_d * 0.5f;
	//top & bottom edges (run along X)
	for(int i = 0; i < n_x; i++)
	{
		float fx = left + 0.5f + i;
		props.push_back(Prop(fx, top,    0.0f,                 PropFence, 0.5f, 0.06f, true));
		props.push_back(Prop(fx, bottom, 0.0f,                 PropFence, 0.5f, 0.06f, true));
	}
	//left & right edges (run along Z, rotated 90 deg so collision is wider on Z)
	for(int i = 0; i < n_z; i++)
	{
		float fz = top + 0.5f + i;
		props.push_back(Prop(left,  fz, (float)M_PI * 0.5f, PropFence, 0.06f, 0.5f, true));
		props.push_back(Prop(right, fz, (float)M_PI * 0.5f, PropFence, 0.06f, 0.5f, true));
	}

	//6 cows inside the pen, randomly placed without overlapping each other
	const float cow_min_gap_sqr = 1.4f * 1.4f;
	for(int i = 0; i < 6; i++)
	{
		float cx = 0, cz = 0;
		bool ok = false;
		for(int tries = 0; tries < 40 && !ok; tries++)
		{
			cx = pen_cx + ((std::rand() % 100) / 100.0f - 0.5f) * (pen_w - 1.2f);
			cz = pen_cz + ((std::rand() % 100) / 100.0f - 0.5f) * (pen_d - 1.2f);
			ok = true;
			for(unsigned int j = 0; j < props.size(); j++)
			{
				if(props[j].type != PropCow) continue;
				float dx = props[j].x - cx;
				float dz = props[j].z - cz;
				if(dx * dx + dz * dz < cow_min_gap_sqr) { ok = false; break; }
			}
		}
		float yaw = (std::rand() % 360) * (float)M_PI / 180.0f;
		props.push_back(Prop(cx, cz, yaw, PropCow, 0.38f, 0.68f, true));
	}

	//--- trees scattered across the field (avoid spawn and farm zones) ---
	//density ~0.034 trees per square unit, same as the original 32x32 setup
	const int n_trees = (int)(0.034f * w * h);
	const float spawn_x = w * 0.10f;
	const float spawn_z = h * 0.10f;
	const float farm_radius_sqr  = 16.0f * 16.0f; //wider exclusion since farm is now central
	const float spawn_radius_sqr = 6.0f * 6.0f;
	for(int i = 0; i < n_trees; i++)
	{
		float tx, tz;
		int tries = 0;
		do {
			tx = (std::rand() % (w - 1)) + 0.5f;
			tz = (std::rand() % (h - 1)) + 0.5f;
			float dx_farm = tx - fcx,    dz_farm = tz - fcz;
			float dx_spawn = tx - spawn_x, dz_spawn = tz - spawn_z;
			bool near_farm  = (dx_farm  * dx_farm  + dz_farm  * dz_farm)  < farm_radius_sqr;
			bool near_spawn = (dx_spawn * dx_spawn + dz_spawn * dz_spawn) < spawn_radius_sqr;
			if(!near_farm && !near_spawn) break;
		} while(++tries < 20);
		float yaw = (std::rand() % 360) * (float)M_PI / 180.0f;
		props.push_back(Prop(tx, tz, yaw, PropTree, 0.35f, 0.35f, true));
	}

	//--- distant forest: 3 dense rings of trees beyond the playable area ---
	//non-solid because the player cannot reach them (boundary already blocks at edges)
	float ring_cx = w * 0.5f, ring_cz = h * 0.5f;
	float base_r  = w * 0.55f;
	for(int ring = 0; ring < 3; ring++)
	{
		float r_min = base_r + ring * 9.0f;
		float r_max = r_min + 8.0f;
		int count = 160 - ring * 35; //inner ring densest
		for(int i = 0; i < count; i++)
		{
			float ang = (std::rand() % 36000) / 36000.0f * 2.0f * (float)M_PI;
			float r = r_min + ((std::rand() % 100) / 100.0f) * (r_max - r_min);
			float tx = ring_cx + cosf(ang) * r;
			float tz = ring_cz + sinf(ang) * r;
			float yaw = (std::rand() % 360) * (float)M_PI / 180.0f;
			props.push_back(Prop(tx, tz, yaw, PropTree, 0.0f, 0.0f, false));
		}
	}

	//--- tall grass tufts everywhere except inside the cow pen and right at spawn ---
	const int n_grass = (int)(0.215f * w * h); //same density as 32x32
	for(int i = 0; i < n_grass; i++)
	{
		float gx = (std::rand() % (w * 100 - 100)) / 100.0f + 0.5f;
		float gz = (std::rand() % (h * 100 - 100)) / 100.0f + 0.5f;
		//skip inside cow pen
		if(gx > left && gx < right && gz > top && gz < bottom) continue;
		//skip spawn area
		float dxs = gx - spawn_x, dzs = gz - spawn_z;
		if(dxs * dxs + dzs * dzs < 4.0f) continue;
		float yaw = (std::rand() % 360) * (float)M_PI / 180.0f;
		props.push_back(Prop(gx, gz, yaw, PropGrass, 0.0f, 0.0f, false));
	}
}

char Map::get_tile(ushort x, ushort y)
{
	if(x >= w || y >= h)
		return '0';

	return map[y * w + x];
}

void Map::set_tile(ushort x, ushort y, char tile)
{
	if(x > w || y > h)
		return;

	map[y * w + x] = tile;
	dirty = true;
}

Door Map::get_door(ushort x, ushort y)
{
	unsigned int i = 0;
	for(i = 0; i < doors.size() - 1; i++)
	{
		if(doors.at(i).x == x && doors.at(i).y == y)
			break;
	}
	return doors.at(i);
}

//returns true if the player opened a door
bool Map::update_doors(float player_x, float player_y, float dt)
{
	for(unsigned int i = doors.size(); i > 0; i--)
	{
		float sqr_dist = pow(player_x - doors.at(i-1).x, 2) + pow(player_y - doors.at(i-1).y, 2);
		if(sqr_dist < 10)
		{
			doors.at(i-1).animationState -= dt;

			if(doors.at(i-1).animationState < 0.01)
			{
				set_tile(doors.at(i-1).x, doors.at(i-1).y, ' ');
				doors.erase(doors.begin() + i-1);
				update_dist_map(player_x, player_y);
				return true;
			}
		}
		else
			doors.at(i-1).animationState = 1;
	}

	return false;
}

void Map::sort_sprites(float player_x, float player_y)
{
	//update sqr_dist for ALL sprites including index 0 (otherwise the very first
	//spawned alien keeps its constructor default 0, which trips the damage check)
	for(int i = (int)sprites.size() - 1; i >= 0; i--)
	{
		sprites.at(i).sqr_dist = pow(player_x - sprites.at(i).x, 2) + pow(player_y - sprites.at(i).y, 2);

		if(sprites.at(i).type == Temporary && sprites.at(i).start_time + 500 < SDL_GetTicks())
			delete_sprite(i);
	}
	if(sprites.size() >= 2)
		std::sort(sprites.begin(), sprites.end());
}

int Map::damage_player()
{
	int amount = 0;
	for(unsigned int i = 0; i < sprites.size(); i++)
	{
		if(sprites.at(i).type == Enemy && sprites.at(i).sqr_dist < 2)
			amount += damage;
	}
	return amount;
}

bool Map::pickup_keys()
{
	if(sprites.size() == 0)
		return false;

	for(unsigned int i = sprites.size() - 1; i > 0; i--)
	{
		if(sprites.at(i).type == Key && sprites.at(i).sqr_dist < 2)
		{
			delete_sprite(i);
			return true;
		}
	}

	return false;
}

void Map::animate_sprites()
{
	for(unsigned int i = 0; i < sprites.size(); i++)
	{
		if(sprites.at(i).type == Enemy)
			sprites.at(i).itex = sprites.at(i).itex == 1 ? 4 : 1;
	}
}

std::vector<Sprite> const& Map::get_sprites()
{
	return sprites;
}

void Map::delete_sprite(ushort id)
{
	sprites.erase(sprites.begin() + id);
}

void Map::add_temp_sprite(ushort itex, float x, float y, ushort size)
{
	unsigned int index = sprites.size();
	sprites.push_back(Sprite());
	sprites.at(index).x = x;
	sprites.at(index).y = y;
	sprites.at(index).itex = itex;
	sprites.at(index).type = Temporary;
	sprites.at(index).size = size;
}

void Map::update_sprites(float player_x, float player_y, float dt)
{
	//player position
	int px = ushort(player_x);
	int py = ushort(player_y);
	if(dist[px+py*w] != 0)
		update_dist_map(px, py);

	const float align_threshold = 0.30f; //~17deg - body roughly aligned before walking
	const float turn_rate       = 0.12f;
	const float chase_dist      = 6.0f;  //alien switches to player target within this
	const float chase_dist_sqr  = chase_dist * chase_dist;
	const float stop_dist       = 0.6f;  //don't slide into the target's center

	//--- tick the spawn timer; pop new aliens at the edges over time ---
	if(spawned_count < wave_size)
	{
		next_spawn_in -= dt;
		if(next_spawn_in <= 0.0f)
		{
			spawn_alien_at_edge();
			spawned_count++;
			next_spawn_in = spawn_interval;
		}
	}

	const float abduct_dist_sqr = 1.6f * 1.6f; //alien close enough to suck the cow up with the beam (over the fence)

	//collected during the main loop, applied after so we don't mutate sprites/props mid-iter
	std::vector<unsigned int> aliens_to_remove;
	std::vector<float> abduction_x;
	std::vector<float> abduction_y;

	for(unsigned int i = 0; i < sprites.size(); i++)
	{
		if(sprites.at(i).type == Enemy)
		{
			Sprite& s = sprites.at(i);

			//--- abduction check: is this alien touching any active cow? ---
			bool abducted = false;
			for(unsigned int j = 0; j < props.size() && !abducted; j++)
			{
				const Prop& cow = props[j];
				if(cow.type != PropCow || !cow.active) continue;
				float dxc = cow.x - s.x;
				float dzc = cow.z - s.y;
				if(dxc * dxc + dzc * dzc < abduct_dist_sqr)
				{
					cow.active = false; //cow gone
					aliens_to_remove.push_back(i);
					abduction_x.push_back(cow.x);
					abduction_y.push_back(cow.z);
					abducted = true;
				}
			}
			if(abducted) continue; //skip rest of AI for this alien

			//--- pick LOOK target (head + game logic): player if close, else nearest cow ---
			float dx_p = player_x - s.x;
			float dz_p = player_y - s.y;
			float dist_p_sqr = dx_p * dx_p + dz_p * dz_p;

			float look_x, look_y;
			if(dist_p_sqr < chase_dist_sqr)
			{
				look_x = player_x;
				look_y = player_y;
			}
			else
			{
				float best = 1.0e9f;
				look_x = player_x; look_y = player_y;
				for(unsigned int j = 0; j < props.size(); j++)
				{
					if(props[j].type != PropCow) continue;
					float dx = props[j].x - s.x;
					float dz = props[j].z - s.y;
					float dd = dx * dx + dz * dz;
					if(dd < best) { best = dd; look_x = props[j].x; look_y = props[j].z; }
				}
			}

			s.target_x = look_x;
			s.target_y = look_y;
			s.target_init = true;

			//--- pick WALK target: detour override if stuck, otherwise same as look ---
			float walk_x, walk_y;
			if(s.detour_timer > 0.0f)
			{
				s.detour_timer -= dt;
				walk_x = s.detour_x;
				walk_y = s.detour_y;
			}
			else
			{
				walk_x = look_x;
				walk_y = look_y;
			}

			//--- rotate body toward walk target, step when aligned ---
			float dx = walk_x - s.x;
			float dz = walk_y - s.y;
			float d_target = sqrtf(dx * dx + dz * dz);

			float pre_x = s.x;
			float pre_y = s.y;
			bool tried_to_move = false;

			if(d_target > 0.001f)
			{
				float target_yaw = atan2f(dx, dz);

				if(!s.body_yaw_init)
				{
					s.body_yaw = target_yaw;
					s.body_yaw_init = true;
				}
				float diff = target_yaw - s.body_yaw;
				while(diff >  (float)M_PI) diff -= 2.0f * (float)M_PI;
				while(diff < -(float)M_PI) diff += 2.0f * (float)M_PI;
				s.body_yaw += diff * turn_rate;

				bool aligned = fabsf(diff) < align_threshold;
				if(aligned && d_target > stop_dist)
				{
					tried_to_move = true;
					float step = speed * dt;
					float nx = s.x + (dx / d_target) * step;
					float ny = s.y + (dz / d_target) * step;
					if(!is_blocked(nx, s.y, 0.18f)) s.x = nx;
					if(!is_blocked(s.x, ny, 0.18f)) s.y = ny;
				}
			}

			//keep aliens inside the playable area
			if(s.x < 0.5f)       s.x = 0.5f;
			if(s.x > w - 0.51f)  s.x = w - 0.51f;
			if(s.y < 0.5f)       s.y = 0.5f;
			if(s.y > h - 0.51f)  s.y = h - 0.51f;

			//--- stuck detection: if we tried to move but barely budged, accumulate
			//time; after 2s, pick a random detour for ~1.5s to slip past whatever's
			//blocking (typically a tree right between alien and cow)
			float moved_sqr = (s.x - pre_x) * (s.x - pre_x) + (s.y - pre_y) * (s.y - pre_y);
			float expected_step = speed * dt;
			if(tried_to_move && moved_sqr < expected_step * expected_step * 0.10f)
			{
				s.stuck_timer += dt;
				if(s.stuck_timer >= 2.0f && s.detour_timer <= 0.0f)
				{
					float ang = (std::rand() % 360) * (float)M_PI / 180.0f;
					float r   = 2.5f + (std::rand() % 200) / 100.0f; //2.5 - 4.5 units
					s.detour_x = s.x + cosf(ang) * r;
					s.detour_y = s.y + sinf(ang) * r;
					s.detour_timer = 1.5f;
					s.stuck_timer  = 0.0f;
				}
			}
			else
			{
				s.stuck_timer = 0.0f;
			}
		}
		else if(sprites.at(i).type == Temporary)
		{
			sprites.at(i).size += 15;
		}
	}

	//--- apply abductions after the loop so we don't shift indices mid-iteration ---
	//spawn a brief green light pillar over each abducted cow
	Uint32 beam_end = SDL_GetTicks() + 800;
	for(unsigned int k = 0; k < abduction_x.size(); k++)
	{
		Prop beam(abduction_x[k], abduction_y[k], 0.0f, PropBeam, 0.0f, 0.0f, false);
		beam.expire_at = beam_end;
		props.push_back(beam);
	}

	//erase consumed aliens in descending order so earlier indices stay valid
	for(int k = (int)aliens_to_remove.size() - 1; k >= 0; k--)
	{
		delete_sprite(aliens_to_remove[k]);
		enemy_count--;
	}

	//tick prop expirations (currently only beams have lifetimes)
	Uint32 now = SDL_GetTicks();
	for(unsigned int i = 0; i < props.size(); i++)
	{
		const Prop& p = props[i];
		if(p.active && p.expire_at != 0 && now >= p.expire_at)
			p.active = false;
	}

	//--- fence break: each fence section accumulates damage while an alien is pressing on it ---
	//compare alien against the fence's AABB (not just the center) so touching a corner counts
	const float fence_pad     = 0.30f; //how close to the AABB edge the alien needs to be
	const float fence_pad_sqr = fence_pad * fence_pad;
	const float fence_break_secs = 1.0f;
	for(unsigned int i = 0; i < props.size(); i++)
	{
		const Prop& f = props[i];
		if(f.type != PropFence || !f.active) continue;

		bool touched = false;
		for(unsigned int j = 0; j < sprites.size() && !touched; j++)
		{
			if(sprites[j].type != Enemy) continue;
			//closest point on the fence AABB to the alien
			float cx = sprites[j].x;
			float cz = sprites[j].y;
			float min_x = f.x - f.half_w, max_x = f.x + f.half_w;
			float min_z = f.z - f.half_d, max_z = f.z + f.half_d;
			if(cx < min_x) cx = min_x; else if(cx > max_x) cx = max_x;
			if(cz < min_z) cz = min_z; else if(cz > max_z) cz = max_z;
			float dx = sprites[j].x - cx;
			float dz = sprites[j].y - cz;
			if(dx * dx + dz * dz < fence_pad_sqr) touched = true;
		}

		if(touched)
		{
			f.damage_timer += dt;
			if(f.damage_timer >= fence_break_secs)
				f.active = false; //gap opens, aliens can now walk through
		}
		else
		{
			f.damage_timer = 0.0f; //must be continuous contact
		}
	}

	//--- cow wander: each cow picks a small random target and ambles toward it ---
	const float cow_speed       = 0.45f;
	const float cow_arrive_sqr  = 0.18f * 0.18f;
	const float cow_wander_rng  = 1.6f; //max offset per pick
	for(unsigned int i = 0; i < props.size(); i++)
	{
		const Prop& c = props[i];
		if(c.type != PropCow || !c.active) continue;

		float dx = c.wander_x - c.x;
		float dz = c.wander_y - c.z;
		float d_sqr = dx * dx + dz * dz;

		if(!c.wander_init || d_sqr < cow_arrive_sqr)
		{
			//new random nearby target; clamp to map so we never aim off-grid
			float ox = ((std::rand() % 200) - 100) / 100.0f * cow_wander_rng;
			float oz = ((std::rand() % 200) - 100) / 100.0f * cow_wander_rng;
			c.wander_x = c.x + ox;
			c.wander_y = c.z + oz;
			c.wander_init = true;
			dx = c.wander_x - c.x;
			dz = c.wander_y - c.z;
			d_sqr = dx * dx + dz * dz;
		}

		float d = sqrtf(d_sqr);
		if(d > 0.01f)
		{
			c.yaw = atan2f(dx, dz);
			float step = cow_speed * dt;
			float nx = c.x + (dx / d) * step;
			float nz = c.z + (dz / d) * step;

			//skip self in collision so the cow doesn't block itself
			c.active = false;
			bool blocked_x = is_blocked(nx, c.z, 0.20f);
			bool blocked_z = is_blocked(c.x, nz, 0.20f);
			c.active = true;

			if(!blocked_x) c.x = nx;
			if(!blocked_z) c.z = nz;

			//if we hit something on either axis, drop the current target so we pick a new one
			if(blocked_x || blocked_z) c.wander_init = false;
		}
	}
}

void Map::update_dist_map(ushort px, ushort py)
{
	//initialize the distance map
	for(int i = 0; i < h; i++)
	{
		for(int j = 0; j < w; j++)
		{
			dist[j * w + i] = 1000;
		}
	}

	//all for neighbors
	const int nx[4] = {0, -1, 1, 0};
	const int ny[4] = {-1, 0, 0, 1};

	dist[px+py*w] = 0; //sets player's distance to 0
	for (int iter = 0; iter < 10; iter++)
	{
		for (int y = 0; y < h; y++)
		{
			for (int x = 0; x < w; x++)
			{
				int idx = x + y * w;
				if(map[idx] == ' ')
				{
					//for each neighbor (bounds-check so an open map doesn't read past array)
					for(int i = 0; i < 4; i++)
					{
						int tx = x + nx[i];
						int ty = y + ny[i];
						if(tx < 0 || tx >= w || ty < 0 || ty >= h) continue;
						int tidx = tx + ty * w;
						if(map[tidx] == ' ' && dist[tidx] > dist[idx])
							dist[tidx] = dist[idx] + 1;
					}
				}
			}
		}
	}

	/*
	for(int i = 0; i < h; i++)
	{
		for(int j = 0; j < w; j++)
		{
			int idx = i + j * w;
			std::cout<<(char)(dist[idx] > 9 ? '-' : dist[idx] + '0');
		}
		std::cout<<std::endl;
	}
	*/
}

//Gets a pixel from the texture file
Uint32 Map::get_pixel(SDL_Surface* source, ushort x, ushort y)
{
    if(x >= h || y >= w) 
        return 0;
    
    Uint8 *p = (Uint8 *)source->pixels + y * source->pitch + x * source->format->BytesPerPixel;
    return p[0] | p[1] << 8 | p[2] << 16;
}

Map::~Map()
{
	delete dist;
	delete map;
    std::cout<<"Map deleted"<<std::endl;
}