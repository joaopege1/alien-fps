#ifndef _PLAYER_H_
#define _PLAYER_H_

#include <SDL2/SDL.h>
#include "map.h"
#include "button.h"
#include "menu.h"

const float speed = 30;
const float  turn_accel = 0.18;
const float turn_max = 0.08;
const float mouse_sensitivity = 0.003;
const float mouse_pitch_speed = 0.003; //radians per pixel of vertical mouse movement
const float max_pitch = 1.4;           //~80 degrees up/down

class Player
{
	public:
        Player(Map* ma, Menu* me);
        void handle_events(float dt);
        bool display_flash = false;
        int health = 100;
        ushort key_count = 0;
        bool turkey_destruct, wall_destruct, hurt_sound, key_sound;

        float get_x();
        float get_y();
        float get_angle();
        float get_pitch();

        ~Player();

        //forbids copy constructor to avoid warning in c++11
        Player(const Player& p) = delete;
        Player& operator=(const Player& p) = delete;

    private:
        float x, y, angle; //player position and rotation
        float pitch; //vertical look angle in radians
        float turn, walk_x, walk_y; // player input
        bool* pressed_keys;

        void update_key(SDL_Keycode key, bool state);
        void Fire();

        Map* map;
        Menu* menu;
};

#endif