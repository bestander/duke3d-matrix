#include <cstdio>
#include <cstdint>
#include "led-matrix.h"
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include "Meteosource.h"

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
Meteosource *meteosource;
typedef struct
{
    bool isError;
    double min;
    double max;
} MinMaxTemperature;
MinMaxTemperature temperature_forecast;

void getWeather()
{
    if (meteosource)
    {
        const char *sections = "hourly";
        const char *timezone = "UTC";
        const char *language = "en";
        const char *units = "metric";

        temperature_forecast.isError = true;

        auto res = meteosource->get_point_forecast(metsource_place_id, sections, timezone, language, units);
        if (!res)
        {
            return;
        }

        if (res->hourly.size() > 0)
        {
            temperature_forecast.isError = false;
            temperature_forecast.min = res->hourly[0]->temperature;
            temperature_forecast.max = res->hourly[0]->temperature;
            for (int i = 0; i < 5; ++i)
            {
                printf("Hourly: %.1f \n", res->hourly[i]->temperature);
                if (res->hourly[i]->temperature > temperature_forecast.max)
                {
                    temperature_forecast.max = res->hourly[i]->temperature;
                }
                if (res->hourly[i]->temperature < temperature_forecast.min)
                {
                    temperature_forecast.min = res->hourly[i]->temperature;
                }
            }
        }
    }
}

void *refreshWeatherAndRestartDuke(void *arg)
{
    while (true)
    {
        printf("Timer thread: calling refreshWeatherAndRestartDoom\n");
        getWeather();
        is_game_sleeping = false;
//        S_PauseSounds(true); in game.cpp

        // S_SetMusicVolume(127);
        sleep(refresh_weather_timeout);
    }
    return NULL;
}

void *putDukeToSleep(void *arg)
{
    while (true)
    {
        printf("Timer thread: calling putDoomToSleep\n");
        if (!is_game_sleeping)
        {
            clock_t now_time = clock();
            int elapsed_sec = (int)(now_time - last_activity_time) / CLOCKS_PER_SEC;
            if (elapsed_sec >= sleep_timeout)
            {
                is_game_sleeping = true;
                // S_SetMusicVolume(0);
                sleep(sleep_timeout);
            }
            else
            {
                sleep(sleep_timeout - elapsed_sec);
            }
        }
        else
        {
            sleep(sleep_timeout);
        }
    }
    return NULL;
}

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

    const char *metsource_key_arg = "--metsource_key=";
    const char *metsource_location_arg = "--metsource_location=";
    const char *sleep_timeout_arg = "--sleep_timeout_sec=";
    const char *refresh_timer_arg = "--refresh_weather_timer_sec=";
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
    temperature_forecast.isError = true;
    if (metsource_key && metsource_place_id)
    {
        meteosource = new Meteosource(metsource_key, "free", "https://www.meteosource.com/api");
    }

    pthread_t refresh_thread_id;
    if (refresh_weather_timeout)
    {
        int err = pthread_create(&refresh_thread_id, NULL, refreshWeatherAndRestartDuke, NULL);
        if (err != 0)
        {
            printf("Error creating timer thread: %d\n", err);
        }
    }
    pthread_t sleep_thread_id;
    if (sleep_timeout)
    {
        int err = pthread_create(&sleep_thread_id, NULL, putDukeToSleep, NULL);
        if (err != 0)
        {
            printf("Error creating timer thread: %d\n", err);
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

    if (!temperature_forecast.isError)
    {
        char temp_message[20];
        sprintf(temp_message, "%.1f° - %.1f°", temperature_forecast.min, temperature_forecast.max);
        DrawText(offscreen_canvas, font, 0, 64, time_color, NULL, temp_message, 1);
    }

    offscreen_canvas = matrix->SwapOnVSync(offscreen_canvas);
}
