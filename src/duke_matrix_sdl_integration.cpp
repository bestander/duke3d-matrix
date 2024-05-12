#include <cstdio>
#include <cstdint>
#include "led-matrix.h"

using namespace rgb_matrix;

RGBMatrix *matrix;
FrameCanvas *offscreen_canvas;
Color color;
Font font;

int matrix_width, matrix_height;
int surface_width, surface_height;

void SDL_on_Init(int argc, char *argv[])
{
    RGBMatrix::Options matrix_options;
    RuntimeOptions runtime_opt;

    // // TODO move some to argv
    // matrix_options.cols = matrix_width;
    // matrix_options.rows = matrix_height;
    // // matrix_options.chain_length = 2;
    // // matrix_options.pixel_mapper_config = "U-mapper";
    // matrix_options.pwm_lsb_nanoseconds = 500;
    // matrix_options.pwm_dither_bits = 2;
    // matrix_options.pwm_bits = 10;
    // matrix_options.hardware_mapping = "adafruit-hat";
    // runtime_opt.gpio_slowdown = 3;
    ParseOptionsFromFlags(&argc, &argv, &matrix_options, &runtime_opt);
    printf("?????? PARAMS col %d row %d mapping %s \n", matrix_options.cols, matrix_options.rows, matrix_options.hardware_mapping);
    surface_width = matrix_width = matrix_options.cols;
    matrix_height = matrix_options.rows;
    surface_height = matrix_width * 0.75;

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

bool is_game_sleeping = false;

void SDL_on_DrawFrame(uint32_t *pixels)
{
    // printf("drawing frame ");
    // uint32_t *pix = pixels;
    // for (int y = 0; y < matrix_height; ++y)
    // {
    //     for (int x = 0; x < matrix_width; ++x)
    //     {
    //         if (x < surface_width && y < surface_height)
    //         {
    //             if (!is_game_sleeping)
    //             {
    //                 uint8_t r = *pix >> 16;
    //                 uint8_t g = *pix >> 8;
    //                 uint8_t b = *pix;
    //                 // printf("%d %d %d %d %d \n", x, y, r, g, b);
    //                 offscreen_canvas->SetPixel(x, y, r, g, b);
    //             }
    //             else
    //             {
    //                 offscreen_canvas->SetPixel(x, y, 0, 0, 0);
    //             }
    //             pix = pix + 10;
    //         }
    //         else
    //         {
    //             offscreen_canvas->SetPixel(x, y, 0, 0, 0);
    //         }
    //     }
    // }
     DrawText(offscreen_canvas, font,
             0, 60,
             color, NULL, "TEST",
             1);
    offscreen_canvas = matrix->SwapOnVSync(offscreen_canvas);
}