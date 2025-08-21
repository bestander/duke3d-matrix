#include <cstdio>
#include <curl/curl.h>
#include <string>
#include <cstdint>
#include "led-matrix.h"
#include "../libs/eduke32/source/duke3d/src/sounds.h"
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include "Meteosource.h"
#include <fstream>
#include <iostream>
#include <vector>
#include <algorithm>

using namespace rgb_matrix;

RGBMatrix *matrix;
FrameCanvas *offscreen_canvas;
rgb_matrix::Color time_color;
rgb_matrix::Font font;

struct Pixel
{
    unsigned char blue;
    unsigned char green;
    unsigned char red;
};
Pixel *background = NULL;

int matrix_width, matrix_height;
int surface_width, surface_height;
int surface_to_matrix_ratio;

bool is_game_sleeping = false;
clock_t last_activity_time;
int sleep_timeout = 0;

int refresh_weather_timeout = 0;
char *metsource_key = NULL;
char *metsource_place_id = NULL;
char *noaa_tides_location = NULL;
Meteosource *meteosource;

// Manual string parsing for next high tide from NOAA JSON response
// Extract all high and low tides from NOAA JSON response (manual parsing)
struct TideEvent
{
    std::string time;
    std::string type; // "H" or "L"
    std::string value;
};

// Manual extraction of tide events from NOAA JSON response
std::vector<TideEvent> extractTidesManual(const std::string &response)
{
    std::vector<TideEvent> tides;
    size_t pos = 0;
    while (true)
    {
        size_t t_pos = response.find("\"t\":\"", pos);
        if (t_pos == std::string::npos)
            break;
        t_pos += 5;
        size_t t_end = response.find('"', t_pos);
        if (t_end == std::string::npos)
            break;
        std::string time = response.substr(t_pos, t_end - t_pos);

        size_t v_pos = response.find("\"v\":\"", t_end);
        if (v_pos == std::string::npos)
            break;
        v_pos += 6;
        size_t v_end = response.find('"', v_pos);
        if (v_end == std::string::npos)
            break;
        std::string value = response.substr(v_pos, v_end - v_pos);

        size_t type_pos = response.find("\"type\":\"", v_end);
        if (type_pos == std::string::npos)
            break;
        type_pos += 8;
        size_t type_end = response.find('"', type_pos);
        if (type_end == std::string::npos)
            break;
        std::string type = response.substr(type_pos, type_end - type_pos);

        tides.push_back({time, type, value});
        pos = type_end;
    }
    // Sort by time ascending
    std::sort(tides.begin(), tides.end(), [](const TideEvent &a, const TideEvent &b)
              { return a.time < b.time; });
    return tides;
}

// Get next tide event of given type ("H" or "L") in the future
std::string getNextTideTime(const std::vector<TideEvent> &tides, const std::string &type)
{
    time_t now = time(0);
    for (const auto &tide : tides)
    {
        if (tide.type == type)
        {
            struct tm tide_tm = {0};
            strptime(tide.time.c_str(), "%Y-%m-%d %H:%M", &tide_tm);
            time_t tide_time = mktime(&tide_tm);
            if (difftime(tide_time, now) > 0)
            {
                return tide.time;
            }
        }
    }
    return "";
}

typedef struct
{
    bool isError;
    std::string nextHighTide;
    std::string nextLowTide;
} TideInfo;

TideInfo tideInfo;
// Helper for HTTP GET using libcurl
size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    ((std::string *)userp)->append((char *)contents, size * nmemb);
    return size * nmemb;
}

std::string http_get(const std::string &url)
{
    CURL *curl = curl_easy_init();
    std::string readBuffer;
    if (curl)
    {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
        curl_easy_perform(curl);
        curl_easy_cleanup(curl);
    }
    return readBuffer;
}

TideInfo getNextHighTide(const std::string &stationId)
{
    TideInfo tideInfo = {true, "", ""};
    std::string url = "https://api.tidesandcurrents.noaa.gov/api/prod/datagetter?product=predictions&application=duke3d-matrix&begin_date=TODAY&end_date=TODAY&datum=MLLW&station=" + stationId + "&time_zone=lst_ldt&units=metric&interval=hilo&format=json";
    char dateStr[9];
    time_t now = time(0);
    strftime(dateStr, sizeof(dateStr), "%Y%m%d", localtime(&now));
    url.replace(url.find("TODAY"), 5, dateStr);
    url.replace(url.rfind("TODAY"), 5, dateStr);

    std::string response = http_get(url);
    if (response.empty())
        return tideInfo;

    // Use manual string parsing for now
    printf("Next tide response: %s\n", response.c_str());

    std::vector<TideEvent> tides = extractTidesManual(response);
    if (!tides.empty())
    {
        tideInfo.isError = false;
        tideInfo.nextHighTide = getNextTideTime(tides, "H");
        tideInfo.nextLowTide = getNextTideTime(tides, "L");
    }
    printf("Next high tide response: %s\n", tideInfo.nextHighTide.c_str());
    return tideInfo;
}

void getTide()
{
    // Example NOAA station: San Francisco 9414290
    if (noaa_tides_location)
    {
        printf("Getting next high tide: %s\n", noaa_tides_location);
        tideInfo = getNextHighTide(std::string(noaa_tides_location));
        if (!tideInfo.isError)
        {
            printf("Next high tide: %s\n", tideInfo.nextHighTide.c_str());
        }
    }
    else
    {
        printf("Failed to get next high tide\n");
    }
}
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
        printf("Timer thread: calling refreshWeatherAndRestartDuke\n");
        getWeather();
        getTide();
        // TODO G_PlaybackDemo before sleep?
        is_game_sleeping = false;
        S_PauseSounds(false);
        S_PauseMusic(false);
        sleep(refresh_weather_timeout);
    }
    return NULL;
}

void *putDukeToSleep(void *arg)
{
    while (true)
    {
        printf("Timer thread: calling putDukeToSleep\n");
        if (!is_game_sleeping)
        {
            clock_t now_time = clock();
            int elapsed_sec = (int)(now_time - last_activity_time) / CLOCKS_PER_SEC;
            if (elapsed_sec >= sleep_timeout)
            {
                is_game_sleeping = true;
                S_PauseSounds(true);
                S_PauseMusic(true);
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

void SDL_on_InputEvent()
{
    last_activity_time = clock();
    if (is_game_sleeping)
    {
        is_game_sleeping = false;
        S_PauseSounds(false);
        S_PauseMusic(false);
    }
}

void readBackgroundImageFile(char *image_path)
{
    std::ifstream file(image_path, std::ios::binary);
    if (!file.is_open())
    {
        printf("Failed to read background image \n");
        return;
    }
    else
    {
        // Read file header (54 bytes)
        char header[54];
        file.read(header, 54);
        if (header[0] != 'B' && header[1] != 'M')
        {
            printf("Warning: Only supports BMP format in background image \n");
            return;
        }
        // Check pixel format (bits per pixel)
        short bitsPerPixel = *(short *)&header[28];
        if (bitsPerPixel != 24 && bitsPerPixel != 32)
        {
            printf("Warning: Only supports 24 bits per pixel in background image: %d \n", bitsPerPixel);
            return;
        }
        // Read pixel data
        int width = *(int *)&header[18];
        int height = *(int *)&header[14];
        int offset = *(int *)&header[10];
        if (width != matrix_width)
        {
            printf("Warning: Only supports width %d pixels width in background image, got: %d \n", matrix_width, width);
            return;
        }

        file.seekg(offset);
        // Read pixel data
        for (int y = 0; y < height && y < surface_height / surface_to_matrix_ratio; ++y)
        {
            for (int x = 0; x < width && x < matrix_width; ++x)
            {
                Pixel &pixel = background[y * width + x];
                file.read((char *)&pixel, bitsPerPixel / 8);
                // Swap BGR to RGB (if needed)
                if (bitsPerPixel == 24)
                {
                    std::swap(pixel.blue, pixel.red);
                }
            }
        }
        file.close();
    }
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
    const char *background_image_arg = "--background-image-path=";
    const char *noaa_tides_location_arg = "--noaa-tides-location=";
    char *background_image_path;
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
        else if (strncmp(argv[i], background_image_arg, strlen(background_image_arg)) == 0)
        {
            background_image_path = argv[i] + strlen(background_image_arg);
            printf("Background image path: %s\n", background_image_path);
        }
        else if (strncmp(argv[i], noaa_tides_location_arg, strlen(noaa_tides_location_arg)) == 0)
        {
            noaa_tides_location = argv[i] + strlen(noaa_tides_location_arg);
            printf("NOAA Tides location: %s\n", noaa_tides_location);
        }
    }
    temperature_forecast.isError = true;
    if (metsource_key && metsource_place_id)
    {
        meteosource = new Meteosource(metsource_key, "free", "https://www.meteosource.com/api");
    }

    background = new Pixel[matrix_height * matrix_height];
    if (background_image_path)
    {
        readBackgroundImageFile(background_image_path);
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
                offscreen_canvas->SetPixel(x, y,
                                           background[y * matrix_width + x].red,
                                           background[y * matrix_width + x].green,
                                           background[y * matrix_width + x].blue);
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

    // Sine wave visualization for next 24 hours, peak at next high tide
    if (!tideInfo.isError && !tideInfo.nextHighTide.empty())
    {
        struct tm tide_tm = {0};
        strptime(tideInfo.nextHighTide.c_str(), "%Y-%m-%d %H:%M", &tide_tm);
        time_t tide_time = mktime(&tide_tm);
        time_t now = time(0);
        double hours_to_tide = difftime(tide_time, now) / 3600.0;
        printf("Hours to next high tide: %.2f\n", hours_to_tide);
        strftime(time_buffer, 20, "%H:%M", info);

        int wave_x_start = 36;
        int wave_x_end = 64;
        int wave_y_base = 54; // Y position for wave
        int wave_height = 8;  // Amplitude
        int wave_y_min = wave_y_base - wave_height / 2;
        int wave_y_max = wave_y_base + wave_height / 2;

        // For each X pixel in the range, map to hour in next 24h
        for (int x = wave_x_start; x < wave_x_end; ++x)
        {
            double hour = (double)(x - wave_x_start) * 24.0 / (wave_x_end - wave_x_start);
            // Sine phase: peak at high tide
            double phase = M_PI * (hour - hours_to_tide) / 12.0; // 12h period
            double value = sin(phase);
            int y = wave_y_base - (int)(value * (wave_height / 2));
            // Clamp y
            if (y < wave_y_min)
                y = wave_y_min;
            if (y > wave_y_max)
                y = wave_y_max;
            offscreen_canvas->SetPixel(x, y, 255, 0, 0); // Blue wave
        }
    }

    offscreen_canvas = matrix->SwapOnVSync(offscreen_canvas);
}
