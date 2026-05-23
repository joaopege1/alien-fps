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

    //TODO : Fix this issue
    //temporary workaround because map sizes other than 32x32 causes crashes
    if(map_tex->w != 32 || map_tex->h != 32)
    {
    	std::cerr << "Map must be 32x32." << std::endl;
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
	//immediately clamp the alien on its first move
	int edge = std::rand() % 4;
	float sx = 0.0f, sy = 0.0f;
	switch(edge)
	{
		case 0: sx = 1.0f + (std::rand() % (w - 2)); sy = 0.8f;          break; //N
		case 1: sx = 1.0f + (std::rand() % (w - 2)); sy = (float)h - 0.8f; break; //S
		case 2: sx = 0.8f;          sy = 1.0f + (std::rand() % (h - 2)); break; //W
		case 3: sx = (float)w - 0.8f; sy = 1.0f + (std::rand() % (h - 2)); break; //E
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

bool Map::is_blocked(float wx, float wz, float player_radius) const
{
	for(unsigned int i = 0; i < props.size(); i++)
	{
		const Prop& p = props[i];
		if(!p.solid || !p.active) continue;
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

	//--- the farm (north-east area of the 32x32 field) ---
	//barn footprint matches the mesh: 7 wide x 9 deep
	float barn_x = 22.0f, barn_z = 7.0f;
	props.push_back(Prop(barn_x, barn_z, 0.0f, PropBarn, 3.5f, 4.5f, true));

	//cow pen: a fenced rectangle south of the barn
	float pen_cx = 22.0f, pen_cz = 18.0f;
	float pen_w = 9.0f, pen_d = 7.0f;
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

	//6 cows inside the pen, randomly placed
	for(int i = 0; i < 6; i++)
	{
		float cx = pen_cx + ((std::rand() % 100) / 100.0f - 0.5f) * (pen_w - 1.2f);
		float cz = pen_cz + ((std::rand() % 100) / 100.0f - 0.5f) * (pen_d - 1.2f);
		float yaw = (std::rand() % 360) * (float)M_PI / 180.0f;
		props.push_back(Prop(cx, cz, yaw, PropCow, 0.38f, 0.68f, true));
	}

	//--- trees scattered around the perimeter (avoid spawn at (3,3) and farm) ---
	const int n_trees = 35;
	for(int i = 0; i < n_trees; i++)
	{
		//bias toward outer ring of the map
		float tx, tz;
		int tries = 0;
		do {
			tx = (std::rand() % 31) + 0.5f;
			tz = (std::rand() % 31) + 0.5f;
			//distance from farm center (barn + pen span roughly z=3..21, x=18..27)
			float dx_farm = tx - 22.0f, dz_farm = tz - 13.0f;
			float dx_spawn = tx - 3.0f, dz_spawn = tz - 3.0f;
			bool near_farm  = (dx_farm * dx_farm + dz_farm * dz_farm) < 100.0f; //r=10
			bool near_spawn = (dx_spawn * dx_spawn + dz_spawn * dz_spawn) < 12.0f;
			if(!near_farm && !near_spawn) break;
		} while(++tries < 20);
		float yaw = (std::rand() % 360) * (float)M_PI / 180.0f;
		props.push_back(Prop(tx, tz, yaw, PropTree, 0.35f, 0.35f, true));
	}

	//--- distant forest: 3 dense rings of trees beyond the playable 32x32 area ---
	//non-solid because the player cannot reach them (boundary already blocks at edges)
	for(int ring = 0; ring < 3; ring++)
	{
		float r_min = 38.0f + ring * 7.0f;
		float r_max = r_min + 6.0f;
		int count = 70 - ring * 15; //inner ring has more, outer rings fewer
		for(int i = 0; i < count; i++)
		{
			float ang = (std::rand() % 36000) / 36000.0f * 2.0f * (float)M_PI;
			float r = r_min + ((std::rand() % 100) / 100.0f) * (r_max - r_min);
			float tx = 16.0f + cosf(ang) * r;
			float tz = 16.0f + sinf(ang) * r;
			float yaw = (std::rand() % 360) * (float)M_PI / 180.0f;
			props.push_back(Prop(tx, tz, yaw, PropTree, 0.0f, 0.0f, false));
		}
	}

	//--- tall grass tufts everywhere except inside the cow pen and right at spawn ---
	const int n_grass = 220;
	for(int i = 0; i < n_grass; i++)
	{
		float gx = (std::rand() % 3100) / 100.0f + 0.5f;
		float gz = (std::rand() % 3100) / 100.0f + 0.5f;
		//skip inside cow pen
		if(gx > left && gx < right && gz > top && gz < bottom) continue;
		//skip spawn area
		float dxs = gx - 3.0f, dzs = gz - 3.0f;
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
	if(sprites.size() < 2)
		return;

	for(unsigned int i = sprites.size() - 1; i > 0; i--)
	{
		sprites.at(i).sqr_dist = pow(player_x - sprites.at(i).x, 2) + pow(player_y - sprites.at(i).y, 2);

		if(sprites.at(i).type == Temporary && sprites.at(i).start_time + 500 < SDL_GetTicks())
			delete_sprite(i);
	}
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

	const float abduct_dist_sqr = 0.55f * 0.55f; //alien close enough to "grab" a cow

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

			//--- pick target: player if close, else nearest cow (fallback: player) ---
			float dx_p = player_x - s.x;
			float dz_p = player_y - s.y;
			float dist_p_sqr = dx_p * dx_p + dz_p * dz_p;

			float tx, ty;
			if(dist_p_sqr < chase_dist_sqr)
			{
				tx = player_x;
				ty = player_y;
			}
			else
			{
				float best = 1.0e9f;
				tx = player_x; ty = player_y; //fallback if no cows
				for(unsigned int j = 0; j < props.size(); j++)
				{
					if(props[j].type != PropCow) continue;
					float dx = props[j].x - s.x;
					float dz = props[j].z - s.y;
					float dd = dx * dx + dz * dz;
					if(dd < best) { best = dd; tx = props[j].x; ty = props[j].z; }
				}
			}

			s.target_x = tx;
			s.target_y = ty;
			s.target_init = true;

			//--- rotate body to face the target, walk forward when aligned ---
			float dx = tx - s.x;
			float dz = ty - s.y;
			float d_target = sqrtf(dx * dx + dz * dz);

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
					float step = speed * dt;
					s.x += (dx / d_target) * step;
					s.y += (dz / d_target) * step;
				}
			}

			//keep aliens inside the playable area so ushort() never exceeds w/h
			if(s.x < 0.5f)       s.x = 0.5f;
			if(s.x > w - 0.51f)  s.x = w - 0.51f;
			if(s.y < 0.5f)       s.y = 0.5f;
			if(s.y > h - 0.51f)  s.y = h - 0.51f;
		}
		else if(sprites.at(i).type == Temporary)
		{
			sprites.at(i).size += 15;
		}
	}

	//--- apply abductions after the loop so we don't shift indices mid-iteration ---
	//drop a temp explosion sprite at each cow's last position
	for(unsigned int k = 0; k < abduction_x.size(); k++)
		add_temp_sprite(7, abduction_x[k], abduction_y[k], 900);

	//erase consumed aliens in descending order so earlier indices stay valid
	for(int k = (int)aliens_to_remove.size() - 1; k >= 0; k--)
	{
		delete_sprite(aliens_to_remove[k]);
		enemy_count--;
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