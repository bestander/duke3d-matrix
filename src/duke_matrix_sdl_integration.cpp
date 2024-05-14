#include <cstdio>
#include <cstdint>
#include "led-matrix.h"

using namespace rgb_matrix;

RGBMatrix *matrix;
FrameCanvas *offscreen_canvas;
rgb_matrix::Color color;
rgb_matrix::Font font;

int matrix_width, matrix_height;
int surface_width, surface_height;
int surface_to_matrix_ratio;

void SDL_on_Init(int argc, char *argv[])
{
    RGBMatrix::Options matrix_options;
    RuntimeOptions runtime_opt;

    ParseOptionsFromFlags(&argc, &argv, &matrix_options, &runtime_opt);
    matrix_width = 64;
    surface_width = 640;
    matrix_height = 64;
    surface_height = 480;
    surface_to_matrix_ratio = surface_width / matrix_width;

    matrix = RGBMatrix::CreateFromOptions(matrix_options, runtime_opt);
    if (matrix == NULL)
    {
        printf("Failed to create RGBMatrix \n");
        return;
    }

    offscreen_canvas = matrix->CreateFrameCanvas();
    if (offscreen_canvas == NULL)
    {
        printf("Failed to CreateFrameCanvas \n");
        return;
    }
    color.r = 255;
    if (!font.LoadFont("libs/rpi-rgb-led-matrix/fonts/4x6.bdf"))
    {
        fprintf(stderr, "Couldn't load font '%s'\n", "4x6");
        return;
    }

    DrawText(offscreen_canvas, font,
             0, 60,
             color, NULL, "TEST",
             0);
}

void SDL_OverrideResolution(int *width, int *height)
{
    *width = surface_width;
    *height = surface_height;
}

bool is_game_sleeping = false;

void SDL_on_DrawFrame(uint32_t *pixels)
{
    uint32_t *pix = pixels;
    for (int y = 0; y < matrix_height; ++y)
    {
        for (int x = 0; x < matrix_width; ++x)
        {
            if (!is_game_sleeping && y < surface_height / 10)
            {
                uint16_t red = 0;
                uint16_t green = 0;
                uint16_t blue = 0;
                int ratio_square = surface_to_matrix_ratio * surface_to_matrix_ratio;

                for (int ix = 0; ix < surface_to_matrix_ratio; ++ix)
                {
                    for (int iy = 0; iy < surface_to_matrix_ratio; ++iy)
                    {
                        pix = pixels + x * surface_to_matrix_ratio + ix + (surface_to_matrix_ratio * y + iy) * surface_width;
                        red += *pix >> 16 & 0xFF;
                        green += *pix >> 8 & 0xFF;
                        blue += *pix & 0xFF;
                    }
                }
                offscreen_canvas->SetPixel(x, y, red / ratio_square, green / ratio_square, blue / ratio_square);
            }
            else
            {
                offscreen_canvas->SetPixel(x, y, 0, 0, 0);
            }
        }
    }
    DrawText(offscreen_canvas, font,
             0, 60,
             color, NULL, "TEST",
             1);
    offscreen_canvas = matrix->SwapOnVSync(offscreen_canvas);
}