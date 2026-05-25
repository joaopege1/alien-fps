#include <iostream>
#include <cmath>
#include "player.h"
 
Player::Player(Map* ma, Menu* me) : display_flash(false), health(100), key_count(0), turkey_destruct(false), wall_destruct(false),
    hurt_sound(false), key_sound(false),
    //spawn 10% in from the corner regardless of map size so the player has the farm in view
    x(ma->w * 0.10f), y(ma->h * 0.10f),
    angle(0), pitch(0), turn(0), walk_x(0), walk_y(0), pressed_keys(NULL), map(ma), menu(me)
{
    pressed_keys = new bool[7];
    for(int i = 0; i < 7; i++)
    {
        pressed_keys[i] = false;
    }
}

void Player::handle_events(float dt)
{
    SDL_Event event;
    if(health < 1)
        menu->current = GameOver;
    //if every cow has been abducted before we kill all aliens, the farm is lost
    if(map->get_cow_count() == 0 && menu->current != Win)
        menu->current = GameOver;

    menu->mouse_down = false; //the menu should see the mouse button clicked for one frame only
    display_flash = false;
    turkey_destruct = false;
    wall_destruct = false;

    //capture mouse for aim while playing, release it for menu navigation
    SDL_SetRelativeMouseMode(menu->current == None ? SDL_TRUE : SDL_FALSE);

    while(SDL_PollEvent(&event))
    {
        //left mouse button: fire while playing, otherwise drives menu interaction
        if(event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT)
        {
            if(menu->current == None)
                Fire();
            else
                menu->mouse_down = true;
        }
        //mouse moved: rotate aim and pitch only when playing
        else if(event.type == SDL_MOUSEMOTION && menu->current == None)
        {
            angle += event.motion.xrel * mouse_sensitivity;
            pitch -= event.motion.yrel * mouse_pitch_speed;
            if(pitch > max_pitch) pitch = max_pitch;
            if(pitch < -max_pitch) pitch = -max_pitch;
        }
        //a key is pressed
        else if(event.type == SDL_KEYDOWN)
        {
            update_key(event.key.keysym.sym, true);
        }
        //a key is released
        else if(event.type == SDL_KEYUP)
        {
            update_key(event.key.keysym.sym, false);

            //pauses the game if the player presses Escape
            if(event.key.keysym.sym == SDLK_ESCAPE)
            {
                if(menu->current == None)
                    menu->current = Pause;
                else if(menu->current == Pause)
                    menu->current = None;
            }

            //quits the game if the player is dead and presses Space or Escape
            if(menu->current == GameOver)
            {
                if(event.key.keysym.sym == SDLK_SPACE || event.key.keysym.sym == SDLK_ESCAPE)
                    menu->wants_to_quit = true;
            }
        }
        //close the game if the user presses the close window button
        else if(!menu->wants_to_quit)
            menu->wants_to_quit = event.type == SDL_QUIT;
    }

    if(menu->current == Main || menu->current == Help)
    {
        angle += dt * 0.1;
        return;
    }

    //Forward - backward movement
    if(pressed_keys[0])
        walk_y = speed * dt;
    else if(pressed_keys[2])
        walk_y = -speed * dt;
    else
        walk_y = 0;

    //Left - right movement
    if(pressed_keys[3])
        walk_x = speed * dt * 0.5;
    else if(pressed_keys[1])
        walk_x = -speed * dt * 0.5;
    else
        walk_x = 0;

    //rotation
    if(fabs(turn) < turn_max)
    {
        if(pressed_keys[5])
            turn += turn_accel * dt;
        else if(pressed_keys[4])
            turn -= turn_accel * dt;
    }
    if((!pressed_keys[5] && turn > 0) || (!pressed_keys[4] && turn < 0))
        turn = 0;

    angle += turn;
    if(angle > M_PI)
        angle -= 2 * M_PI;
    if(angle < -M_PI)   
        angle += 2 * M_PI;

    float nx = x + (walk_y * cos(angle) + walk_x * cos(angle + M_PI / 2.0)) * 0.1;
    float ny = y + (walk_y * sin(angle) + walk_x * sin(angle + M_PI / 2.0)) * 0.1;

    if(int(nx) >= 0 && int(nx) < map->w && int(ny) >= 0 && int(ny) < map->h)
    {
        if(map->get_tile(ushort(nx), ushort(y)) == ' ' && !map->is_blocked(nx, y, 0.25f, true))
            x = nx;
        if(map->get_tile(ushort(x), ushort(ny)) == ' ' && !map->is_blocked(x, ny, 0.25f, true))
            y = ny;
    }
}

void Player::update_key(SDL_Keycode key, bool state)
{
    if (key == 'w' && menu->current != Pause) pressed_keys[0] = state;
    else if (key == 'a' && menu->current != Pause) pressed_keys[1] = state;
    else if (key == 's' && menu->current != Pause) pressed_keys[2] = state;
    else if (key == 'd' && menu->current != Pause) pressed_keys[3] = state;
    else if (key == SDLK_LEFT && menu->current != Pause) pressed_keys[4] = state;
    else if (key == SDLK_RIGHT && menu->current != Pause) pressed_keys[5] = state;
    else if (key == SDLK_SPACE && menu->current != Pause) pressed_keys[6] = state;
}

void Player::Fire()
{
    std::vector<Sprite> sprites = map->get_sprites();

    float dist = 0;
    bool hit_wall = false;

    //we don't need to calculate sin and cos for each step
    float x_offset = cos(angle);
    float y_offset = sin(angle);

    while(!hit_wall && dist < 10) //makes the ray move forward step by step
    {
        dist += 0.1;

        //pos of the tip of the ray for this iteration
        float ray_x = x + x_offset * dist;
        float ray_y = y + y_offset * dist;

        if(map->get_tile(ushort(ray_x), ushort(ray_y)) != ' ') //the current tile is not empty, we hit a wall
        {
            hit_wall = true;
            if(map->get_tile(ushort(ray_x), ushort(ray_y)) == '2')
            {
                wall_destruct = true;
                map->set_tile(ushort(ray_x), ushort(ray_y), ' ');
                map->add_temp_sprite(7, ushort(ray_x) + 0.5, ushort(ray_y) + 0.5, 1000);
                map->sort_sprites(x, y);
                map->update_dist_map(x, y);
            }
        }
        else
        {
            for(unsigned int i = 0; i < sprites.size(); i++)
            {
                if(sprites.at(i).type == Enemy)
                {
                   float sqr_dist = pow(ray_x - sprites.at(i).x, 2) + pow(ray_y - sprites.at(i).y, 2);
                    if(sqr_dist < 0.016)
                    {
                        turkey_destruct = true;
                        map->delete_sprite(i);
                        hit_wall = true;
                        map->add_temp_sprite(6, sprites.at(i).x, sprites.at(i).y, 400);
                        map->sort_sprites(x, y);

                        map->enemy_count--;
                        if(map->enemy_count < 1)
                        {
                            menu->timer.stop();
                            menu->current = Win;
                            menu->leaderboard.add_score(menu->timer.get_time());
                        }
                        break;
                    } 
                }
            }
        }
    }

    display_flash = true;
}

float Player::get_x()
{
    return x;
}

float Player::get_y()
{
    return y; 
}

float Player::get_angle()
{
    return angle;
}

float Player::get_pitch()
{
    return pitch;
}

Player::~Player()
{
    delete pressed_keys;
    std::cout<<"Player deleted"<<std::endl;
}
