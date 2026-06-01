#include <ctime>
#include <iostream>
#include "renderer.h"
#include "player.h"
#include "sound.h"

int main()
{
    uint lastTime = 0;
    uint lastFPSDisplay = 0;
    uint fps = 60;

    Map map;
    if(map.w == 0) return 0;
    Menu menu;
	Player player(&map, &menu);
    Renderer renderer(&player, &map, &menu);
    Sound sound(&menu, &player);
    sound.init_sounds();

    if(!renderer.init_sdl("Defend Your Cows!", 1280, 720))
        return 0;
    
    //game loop
    while (!menu.wants_to_quit)
    {
        float dt = (SDL_GetTicks() - lastTime) / 1000.0;
        lastTime = SDL_GetTicks();
        if(dt == 0) dt = 1; //prevent division by zero

        //called five times per second
        if(lastFPSDisplay + 200 < lastTime)
        {
            lastFPSDisplay = lastTime;
            fps = 1 / dt;

            //sort sprites only 5 times per seconds
            map.sort_sprites(player.get_x(), player.get_y());
            map.animate_sprites();

            if(map.pickup_keys())
            {
                player.key_count++;
                player.key_sound = true;
            }

            if(menu.current == None)
            {
                int damage = map.damage_player();
                player.health -= damage;
                if(damage != 0)
                    player.hurt_sound = true;
            }
        }
        
        if(menu.current == None) //no menu is displayed, the player is currently playing
        {
            if(player.key_count > 0 && map.update_doors(player.get_x(), player.get_y(), dt))
                player.key_count--;

            map.update_sprites(player.get_x(), player.get_y(), dt);
        }
        else if(menu.current == Shop)
        {
            //consume shop click flags - each action has a price
            if(menu.shop_repair_pressed) {
                menu.shop_repair_pressed = false;
                if(map.coins >= 15) { map.coins -= 15; map.repair_fences(); }
            }
            if(menu.shop_heal_pressed) {
                menu.shop_heal_pressed = false;
                if(map.coins >= 20) { map.coins -= 20; player.health = 100; }
            }
            if(menu.shop_buy_cow_pressed) {
                menu.shop_buy_cow_pressed = false;
                if(map.coins >= 50 && map.try_buy_cow()) { map.coins -= 50; }
            }
            if(menu.shop_upgrade_pen_pressed) {
                menu.shop_upgrade_pen_pressed = false;
                if(map.coins >= 200) { map.coins -= 200; map.upgrade_pen(); }
            }
        }
        else if(menu.current == Main)
        {
            map.damage = menu.difficulty == 0 ? 1 : (menu.difficulty == 1 ? 3 : 6);
            map.speed = menu.difficulty == 0 ? 0.8 : (menu.difficulty == 1 ? 1.2 : 2.5);
            sound.set_volume(menu.sound == 0 ? 0 : (menu.sound == 1 ? 20 : 100));
        }

        player.handle_events(dt);
        renderer.draw(fps);
        sound.play_sounds();
    }
    return 0;
}
