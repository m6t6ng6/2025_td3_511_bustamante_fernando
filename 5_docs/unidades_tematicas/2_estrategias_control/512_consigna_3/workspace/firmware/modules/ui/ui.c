#include "ui.h"

static const uint8_t lux_x = 1, lux_y = 30;
static const uint8_t number_w = 65;
static const uint8_t time_x = number_w, time_y = lux_y;
static const uint8_t sp_x = lux_x + 5, sp_y = 4 + lux_y + 10;
static const uint8_t width_hour=30, height_hour=22, origin_hour_x=10, origin_hour_y=0;

bar_t bar = DEFAULT_BAR;

/**
 * @brief Inicializa la interfaz de usuario
 * 
 * @param p_oled 
 * @param i2c 
 * @param p_user 
 */
void ui_init(ssd1306_t *p_oled, i2c_inst_t *i2c, user_t *p_user){
    ssd1306_init(
            p_oled,
            128,
            64,
            OLED_ADDR,
            i2c
    );
    ssd1306_clear(p_oled);
    ui_update(p_oled, p_user);
}

/**
 * @brief Actualiza la interfaz de usuario
 * 
 * @param p_oled 
 * @param i2c 
 * @param p_user 
 */
void ui_update(ssd1306_t *p_oled, user_t *p_user){
    
    switch (p_user->menu)
    {
        case params_menu:
            ui_update_params(p_oled, p_user);
            ui_update_bar(p_oled, &bar, p_user);
            ui_update_user_sp(p_oled,&bar, p_user->sp);
        break;
        
        case config_menu:
            ui_user_config(p_oled, p_user);
        break;

        case log_menu:
            ui_logs(p_oled, p_user);
        break;

        default:
            break;
    }
    
    ssd1306_show(p_oled);
}

void ui_logs(ssd1306_t *p_oled, user_t *p_user){
    char log[25], value[5];

    ssd1306_clear(p_oled);

    ssd1306_draw_string(
        p_oled,
        origin_hour_x, origin_hour_y + 10,
        2,
        "Send data"
    );

    ssd1306_draw_string(
        p_oled,
        origin_hour_x-5, origin_hour_y + 30,
        2,
        "Erase data"
    );

    if(!p_user->change_value_mode){
        switch (p_user->select)
        {
        case 1:
            ssd1306_invert_square(p_oled, 0, origin_hour_y + 8, 124, height_hour);
            break;
        case 2:
            break;
        default:
            ssd1306_invert_square(p_oled, 0, origin_hour_y + 30 - 3, 124, height_hour);
            break;
        }
    }
    else{
        switch (p_user->select)
        {
        case 1:
            ssd1306_draw_square(p_oled, origin_hour_x-5, origin_hour_y + 10, 113, height_hour);
            break;
        case 2:
            break;
        default:
            ssd1306_draw_square(p_oled, origin_hour_x-5, origin_hour_y + 30, 113, height_hour);
            break;
        }
    }

    // for(int i=0; i<3; i++){
    //     sprintf(log, "%02d:%02d:%02d - %02d/%02d/%02d", 0, 36, 3, 5, 8, 25);
    //     sprintf(value, "lux:%d", 5);

    //     ssd1306_draw_string(
    //         p_oled,
    //         5, 5 + i*25,
    //         1,
    //         log
    //     );

    //     ssd1306_draw_string(
    //         p_oled,
    //         5, 5 + i*25 + 10,
    //         1,
    //         value
    //     );
    // }

}


/**
 * @brief Dibuja la configuracion de usuario
 * 
 * @param p_oled 
 * @param p_user 
 */
void ui_user_config(ssd1306_t *p_oled, user_t *p_user){

    char time[10], min[10], max[10], date[10];

    sprintf(time, "%02d:%02d:%02d", p_user->hour, p_user->minute, p_user->sencond);
    sprintf(date, "%02d/%02d/%02d", p_user->day, p_user->month, p_user->year);
    sprintf(min, "min:%d", p_user->min);
    sprintf(max, "max:%d", p_user->max);

    ssd1306_clear(p_oled);

    ssd1306_draw_string(
        p_oled,
        15, 5,
        2,
        time
    );

    ssd1306_draw_string(
        p_oled,
        sp_x, sp_y,
        1,
        min
    );

    ssd1306_draw_string(
        p_oled,
        40, time_y-5,
        1,
        date
    );

    ssd1306_draw_string(
        p_oled,
        time_x, sp_y,
        1,
        max
    );
    
    if(p_user->change_value_mode){
        switch (p_user->select)
        {
        case hour_config:
            ssd1306_invert_square(p_oled, origin_hour_x, origin_hour_y, width_hour, height_hour);
            break;
        case minute_config:
            ssd1306_invert_square(p_oled, origin_hour_x+width_hour+6, origin_hour_y, width_hour, height_hour);
            break;
        case second_config:
            ssd1306_invert_square(p_oled, origin_hour_x+width_hour*2+13, origin_hour_y, width_hour, height_hour);
            break;
        case day_config:
            ssd1306_invert_square(p_oled, 38, 22, 13, 10);
            break;
        case month_config:
            ssd1306_invert_square(p_oled, 38+18, 22, 13, 10);
            break;
        case year_config:
            ssd1306_invert_square(p_oled, 38*2 - 1, 22, 13, 10);
            break;      
        case min_config:
            ssd1306_invert_square(p_oled, sp_x-6, sp_y-2, number_w, 10);
            break;  
        case max_config:
            ssd1306_invert_square(p_oled, time_x-1, sp_y-2, number_w-2, 10);
            break;  
        default:
            break;
        }
    }else{
        switch (p_user->select)
        {
        case hour_config:
            ssd1306_draw_empty_square(p_oled, origin_hour_x, origin_hour_y, width_hour, height_hour);
            break;
        case minute_config:
            ssd1306_draw_empty_square(p_oled, origin_hour_x+width_hour+6, origin_hour_y, width_hour, height_hour);
            break;
        case second_config:
            ssd1306_draw_empty_square(p_oled, origin_hour_x+width_hour*2+13, origin_hour_y, width_hour, height_hour);
            break;
        case day_config:
            ssd1306_draw_empty_square(p_oled, 38, 22, 13, 10);
            break;
        case month_config:
            ssd1306_draw_empty_square(p_oled, 38+18, 22, 13, 10);
            break;
        case year_config:
            ssd1306_draw_empty_square(p_oled, 38*2 - 1, 22, 13, 10);
            break;      
        case min_config:
            ssd1306_draw_empty_square(p_oled, sp_x-6, sp_y-2, number_w, 10);
            break;  
        case max_config:
            ssd1306_draw_empty_square(p_oled, time_x-1, sp_y-2, number_w-2, 10);
            break;    
        default:
            break;
        }
    }

}

/**
 * @brief Dibuja los parametros
 * 
 * @param p_oled 
 * @param p_user 
 * @param lux 
 */
void ui_update_params(ssd1306_t *p_oled, user_t *p_user){

    char sp_ui[15], lux_ui[15];

    sprintf(sp_ui, "SP:%d", p_user->sp);
    sprintf(lux_ui, "LUX:%d", p_user->lux);

    ssd1306_clear_square(p_oled, lux_x-1, lux_y-3, number_w*2, 50);

    ssd1306_draw_string(
        p_oled,
        lux_x, lux_y,
        1,
        lux_ui
    );

    ssd1306_draw_string(
        p_oled,
        sp_x, sp_y,
        1,
        sp_ui
    );

    if(p_user->mode){

        char time_ui[15], sp_f_ui[15];

        sprintf(time_ui, "TIME:%d", p_user->rise_time_ms);
        sprintf(sp_f_ui, "SP_F:%d", p_user->sp_f);

        ssd1306_draw_string(
        p_oled,
        time_x, time_y,
        1,
        time_ui
        );

        ssd1306_draw_string(
        p_oled,
        time_x, sp_y,
        1,
        sp_f_ui
    );
    }
    
    if(p_user->change_value_mode){
        switch (p_user->select)
        {
        case set_sp:
            ssd1306_invert_square(p_oled, sp_x-6, sp_y-2, number_w, 10);
            break;
        case set_lux:
            ssd1306_invert_square(p_oled, lux_x-1, lux_y-2, number_w, 10);
            break;
        case set_time:
            ssd1306_invert_square(p_oled, time_x-1, time_y-2, number_w-2, 10);
            break;
        case set_sp_f:
            ssd1306_invert_square(p_oled, time_x-1, sp_y-2, number_w-2, 10);
            break;
        default:
            break;
        }
    }else{
        switch (p_user->select)
        {
        case set_sp:
            ssd1306_draw_empty_square(p_oled, sp_x-6, sp_y-2, number_w, 10);
            break;
        case set_lux:
            ssd1306_draw_empty_square(p_oled, lux_x-1, lux_y-2, number_w, 10);
            break;
        case set_time:
            ssd1306_draw_empty_square(p_oled, time_x-1, time_y-2, number_w-2, 10);
            break;
        case set_sp_f:
            ssd1306_draw_empty_square(p_oled, time_x-1, sp_y-2, number_w-2, 10);
            break;
        default:
            break;
        }
    }

}

/**
 * @brief Dibuja la barra con el set point
 * 
 * @param p_oled 
 * @param p_bar 
 * @param sp 
 */
void ui_update_user_sp(ssd1306_t *p_oled, bar_t *p_bar, uint32_t sp){

    uint8_t triangle_size = 5;

    ssd1306_clear_square(p_oled, p_bar->x, p_bar->y - 5, p_bar->w, triangle_size);
    ssd1306_clear_square(p_oled, p_bar->x, p_bar->y + p_bar->h+1, p_bar->w, triangle_size);

    ssd1306_draw_filled_triangle(
        p_oled,
        p_bar->w * sp/MAX_LUX, p_bar->y - p_bar->h/2,
        triangle_size,
        true
    );
    
    ssd1306_draw_filled_triangle(
        p_oled,
        p_bar->w * sp/MAX_LUX, p_bar->y + p_bar->h+1,
        triangle_size,
        false
    );
}   

/**
 * @brief Dibuja la barra indicadora
 * 
 * @param p_oled 
 * @param p_bar 
 */
void ui_update_bar(ssd1306_t *p_oled, bar_t *p_bar, user_t *p_user){

    ssd1306_clear_square(p_oled, p_bar->x, 0, p_bar->w, p_bar->h+20);

    ssd1306_draw_empty_square(
        p_oled,
        p_bar->x, p_bar->y,
        p_bar->w-1,
        p_bar->h
    );
    
    ssd1306_draw_square(p_oled, p_bar->x + 1, p_bar->y, p_bar->w * p_user->lux / MAX_LUX, p_bar->h);
}