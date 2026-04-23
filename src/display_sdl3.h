/* display_sdl3.h - SDL3 display backend for VirtualT */

#ifndef _DISPLAY_SDL3_H_
#define _DISPLAY_SDL3_H_

#include <SDL3/SDL.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Display context structure */
typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    int width;
    int height;
    int running;
    
    /* Display state */
    uint32_t *pixels;
    int pixel_width;
    int pixel_height;
    
    /* Display configuration */
    int scale;
    uint32_t bg_color;
    uint32_t pixel_color;
    uint32_t frame_color;
} DisplayContext;

/* Public API */
extern DisplayContext g_display;

int display_init(void);
void display_cleanup(void);
void display_update(void);
void display_handle_events(void);
void display_draw_pixel(int x, int y, int color);
void display_fill_rect(int x, int y, int w, int h, int color);
void display_clear(void);
void display_set_scale(int scale);

#ifdef __cplusplus
}
#endif

#endif
