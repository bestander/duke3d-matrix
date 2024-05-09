#include <cstdio>
#include <cstdint>
#include "led-matrix.h"

using namespace rgb_matrix;


RGBMatrix *matrix;
FrameCanvas *offscreen_canvas;

int surface_width, surface_height;
int matrix_width, matrix_height;


void SDL_on_Init()
{
    RGBMatrix::Options matrix_options;
    // rgb_matrix::RuntimeOptions runtime_opt;
    // if (!rgb_matrix::ParseOptionsFromFlags(&argc, &argv,
    //                                        &matrix_options, &runtime_opt))
    // {
    //     return usage(argv[0]);
    // }
    // matrix = RGBMatrix::CreateFromOptions(matrix_options, runtime_opt);
    // if (matrix == NULL)
    //     return 1;

    //   offscreen_canvas = led_matrix_create_offscreen_canvas(matrix);
    //   led_canvas_get_size(offscreen_canvas, &matrix_width, &matrix_height);
    //   fprintf(stderr, "Size: %dx%d. Hardware gpio mapping: %s\n",
    //           matrix_width, matrix_height, options.hardware_mapping);
}

bool is_game_sleeping = false;

void SDL_on_DrawFrame(uint32_t *pixels)
{
    //     uint32_t *pix = pixels;
    //   for (int y = 0; y < matrix_height; ++y)
    //   {
    //     for (int x = 0; x < matrix_width; ++x)
    //     {
    //       if (x < surface_width && y < surface_height)
    //       {
    //         if (!is_game_sleeping)
    //         {
    //           uint8_t r = *pix >> 16;
    //           uint8_t g = *pix >> 8;
    //           uint8_t b = *pix;
    //           led_canvas_set_pixel(offscreen_canvas, x, y, r, g, b);
    //         }
    //         else
    //         {
    //           led_canvas_set_pixel(offscreen_canvas, x, y, 0, 0, 0);
    //         }
    //         pix++;
    //       }
    //       else
    //       {
    //         led_canvas_set_pixel(offscreen_canvas, x, y, 0, 0, 0);
    //       }
    //     }
    //   }
}