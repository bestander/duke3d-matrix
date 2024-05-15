#include <cstdio>
#include <cstdint>
#include "led-matrix.h"
#include <time.h>
#include <string.h>

using namespace rgb_matrix;

RGBMatrix *matrix;
FrameCanvas *offscreen_canvas;
rgb_matrix::Color time_color;
rgb_matrix::Font font;

int matrix_width, matrix_height;
int surface_width, surface_height;
int surface_to_matrix_ratio;

bool is_game_sleeping = false;
int sleep_timeout = 0;
int refresh_weather_timeout = 0;
clock_t last_activity_time;

char *metsource_key = NULL;
char *metsource_place_id = NULL;

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
    time_color.r = 150;
    if (!font.LoadFont("libs/rpi-rgb-led-matrix/fonts/4x6.bdf"))
    {
        fprintf(stderr, "Couldn't load font '%s'\n", "4x6");
        return;
    }

    char *metsource_key_arg = "--metsource_key=";
    char *metsource_location_arg = "--metsource_location=";
    char *sleep_timeout_arg = "--sleep_timeout_sec=";
    char *refresh_timer_arg = "--refresh_weather_timer_sec=";
    int i;
    for (i = 1; i < argc; i++)
    {
        if (strncmp(argv[i], metsource_key_arg, strlen(metsource_key_arg)) == 0)
        {
            metsource_key = argv[i] + strlen(metsource_key_arg);
            printf("Metsource key: %s\n", metsource_key);
        }
        else if (strncmp(argv[i], metsource_location_arg, strlen(metsource_location_arg)) == 0)
        {
            metsource_place_id = argv[i] + strlen(metsource_location_arg);
            printf("Metsource place: %s\n", metsource_place_id);
        }
        else if (strncmp(argv[i], sleep_timeout_arg, strlen(sleep_timeout_arg)) == 0)
        {
            sleep_timeout = atoi(argv[i] + strlen(sleep_timeout_arg));
            printf("Sleep timoeut: %d\n", sleep_timeout);
        }
        else if (strncmp(argv[i], refresh_timer_arg, strlen(refresh_timer_arg)) == 0)
        {
            refresh_weather_timeout = atoi(argv[i] + strlen(refresh_timer_arg));
            printf("Data refresh timoeut: %d\n", refresh_weather_timeout);
        }
    }
}

void SDL_OverrideResolution(int *width, int *height)
{
    *width = surface_width;
    *height = surface_height;
}

void SDL_on_DrawFrame(uint32_t *pixels)
{
    uint32_t *pix = pixels;
    for (int y = 0; y < matrix_height; ++y)
    {
        for (int x = 0; x < matrix_width; ++x)
        {
            if (!is_game_sleeping && y < surface_height / 10)
            {
                uint8_t red = *pix >> 16;
                uint8_t green = *pix >> 8;
                uint8_t blue = *pix;
                pix = pixels + x * surface_to_matrix_ratio + (surface_to_matrix_ratio * y) * surface_width;
                offscreen_canvas->SetPixel(x, y, red, green, blue);
            }
            else
            {
                offscreen_canvas->SetPixel(x, y, 0, 0, 0);
            }
        }
    }

    char time_buffer[20];
    time_t raw_time;
    struct tm *info;
    time(&raw_time);
    info = localtime(&raw_time);
    strftime(time_buffer, 20, "%H:%M", info);
    DrawText(offscreen_canvas, font, 0, 56, time_color, NULL, time_buffer, 1);
    offscreen_canvas = matrix->SwapOnVSync(offscreen_canvas);
}
