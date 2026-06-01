#include <iostream>
#include <cmath>
#include "menu.h"
 
Menu::Menu() : wants_to_quit(false), current(Main), mouse_down(false), difficulty(Normal), sound(LowSound), timer(), leaderboard(),
    buttons{
        Button(550, 350, 140, 55), //0 play
        Button(380, 420, 600, 55), //1 difficulty
        Button(550, 620, 140, 55), //2 menu quit
        Button(500, 300, 220, 55), //3 resume
        Button(530, 370, 140, 55), //4 pause quit
        Button(550, 560, 140, 55), //5 menu help
        Button(550, 600, 140, 55), //6 help back
        Button(550, 600, 140, 55), //7 win quit
        Button(460, 490, 300, 55), //8 sound
        Button(380, 250, 520, 50), //9  shop: repair fences
        Button(380, 320, 520, 50), //10 shop: heal
        Button(380, 390, 520, 50), //11 shop: buy cow
        Button(380, 460, 520, 50), //12 shop: upgrade pen
        Button(380, 560, 520, 50)  //13 shop: close
    },
    shop_repair_pressed(false), shop_heal_pressed(false),
    shop_buy_cow_pressed(false), shop_upgrade_pen_pressed(false)
{

}

bool Menu::check_hover(ushort button_id)
{
    int x, y;
    SDL_GetMouseState(&x, &y);
    bool hover = buttons[button_id].is_mouse_over(ushort(x), ushort(y));
    if(hover && mouse_down)
    {
        std::cout << "CLICK button_id=" << button_id << " at cursor (" << x << "," << y << ")" << std::endl;
        handle_click(button_id);
    }
    return hover;
}

void Menu::handle_click(ushort button_id)
{
    if(button_id == 0) //play
    {
        current = None;
        timer.start();
    }
    else if(button_id == 1) //difficulty
        difficulty = (Difficulty)(difficulty != 2 ? difficulty + 1 : 0);
    else if(button_id == 2 || button_id == 4 || button_id == 7) //quit
        wants_to_quit = true;
    else if(button_id == 3) //resume
        current = None;
    else if(button_id == 5) //menu help
        current = Help;
    else if(button_id == 6) //help back
        current = Main;
    else if(button_id == 8) //sound
        sound = (SoundVolume)(sound != 2 ? sound + 1 : 0);
    else if(button_id == 9)        //shop: repair fences
        shop_repair_pressed = true;
    else if(button_id == 10)       //shop: heal
        shop_heal_pressed = true;
    else if(button_id == 11)       //shop: buy cow
        shop_buy_cow_pressed = true;
    else if(button_id == 12)       //shop: upgrade pen
        shop_upgrade_pen_pressed = true;
    else if(button_id == 13)       //shop: close
        current = None;
}

Menu::~Menu()
{
    std::cout<<"Menu deleted"<<std::endl;
}