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
    time_t time;
    std::string type; // "H" or "L"
    double value;
};

struct TideInfo
{
    bool isError;
    std::vector<TideEvent> nextTides;
};
TideInfo tideInfo;

// Basic Catmull-Rom spline interpolation for 1D points
// p0, p1, p2, p3 are consecutive y-values, t in [0,1]
double catmullRom(double p0, double p1, double p2, double p3, double t)
{
    return 0.5 * ((2 * p1) + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t * t + (-p0 + 3 * p1 - 3 * p2 + p3) * t * t * t);
}

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
        std::string time_str = response.substr(t_pos, t_end - t_pos);
        struct tm tide_tm = {};
        strptime(time_str.c_str(), "%Y-%m-%d %H:%M", &tide_tm);
        tide_tm.tm_isdst = -1; // Let system determine DST
        time_t tide_time = mktime(&tide_tm);

        size_t v_pos = response.find("\"v\":\"", t_end);
        if (v_pos == std::string::npos)
            break;
        v_pos += 5; // Move past "v":"
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

        tides.push_back({tide_time, type, std::stod(value)});
        pos = type_end;
    }
    // Sort by time ascending
    std::sort(tides.begin(), tides.end(), [](const TideEvent &a, const TideEvent &b)
              { return a.time < b.time; });
    return tides;
}

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
    TideInfo info = {true, {}};
    std::string url = "https://api.tidesandcurrents.noaa.gov/api/prod/datagetter?product=predictions&application=duke3d-matrix&begin_date=TODAY&end_date=TODAY&datum=MLLW&station=" + stationId + "&time_zone=lst_ldt&units=metric&interval=hilo&format=json";
    char dateStr[9];
    time_t now = time(0);
    strftime(dateStr, sizeof(dateStr), "%Y%m%d", localtime(&now));
    url.replace(url.find("TODAY"), 5, dateStr);
    url.replace(url.rfind("TODAY"), 5, dateStr);

    std::string response = http_get(url);
    if (response.empty())
        return info;

    printf("Tide info response %s \n", response.c_str());
    info.nextTides = extractTidesManual(response);
    info.isError = false;
    return info;
}

void getTide()
{
    // Example NOAA station: San Francisco 9414290
    if (noaa_tides_location)
    {
        printf("Getting next high tide: %s\n", noaa_tides_location);
        tideInfo = getNextHighTide(std::string(noaa_tides_location));
        if (tideInfo.isError)
        {
            printf("Failed to get next high tide\n");
        }
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

void drawTideCurve()
{
    // Curve visualization for next 24 hours based on tide events
    if (!tideInfo.isError)
    {
        // Gather all tide events (high and low) and their times
        std::vector<std::pair<double, int>> tidePoints; // (hour offset, y value)
        time_t raw_time;
        time(&raw_time);
        struct tm *info = localtime(&raw_time);
        time_t now = mktime(info);
        int wave_y_base = 56;
        int wave_height = 8;
        int wave_x_start = 36;
        int wave_x_end = 64;

        if (!tideInfo.nextTides.empty())
        {
            printf("Parsed tide events:\n");
            for (const auto &te : tideInfo.nextTides)
            {
                char buf[32];
                strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", localtime(&te.time));
                printf("  time: %s, type: %s, value: %.3f\n", buf, te.type.c_str(), te.value);
            }
            double minHour = difftime(tideInfo.nextTides.front().time, now) / 3600.0;
            double maxHour = minHour;

            for (const auto &te : tideInfo.nextTides)
            {
                double hour = difftime(te.time, now) / 3600.0;
                double y = te.value;
                if (hour < minHour)
                    minHour = hour;
                if (hour > maxHour)
                    maxHour = hour;
            }

            // Interpolate curve between tide points using linear segments
            // Find the minimum hour offset for leftmost point
            double minHourOffset = difftime(tideInfo.nextTides.front().time, now) / 3600.0;
            double maxHourOffset = difftime(tideInfo.nextTides.back().time, now) / 3600.0;
            double hourRange = maxHourOffset - minHourOffset;
            if (hourRange == 0)
                hourRange = 1; // Prevent division by zero

            // Prepare arrays for interpolation
            std::vector<double> hours, values;
            for (const auto &tp : tideInfo.nextTides)
            {
                double hour = difftime(tp.time, now) / 3600.0;
                hours.push_back(hour);
                values.push_back(tp.value);
            }
            double minY = *std::min_element(values.begin(), values.end());
            double maxY = *std::max_element(values.begin(), values.end());
            if (maxY == minY)
                maxY = minY + 1;

            // Interpolate curve for each x using Catmull-Rom spline
            int n = (int)hours.size();
            for (int x = wave_x_start; x <= wave_x_end; ++x)
            {
                double rel = (double)(x - wave_x_start) / (wave_x_end - wave_x_start);
                double hour = minHourOffset + rel * hourRange;
                // Find segment for interpolation
                int seg = 0;
                while (seg + 1 < n && hours[seg + 1] < hour)
                    ++seg;
                // Clamp segment indices for spline
                int i0 = std::max(seg - 1, 0);
                int i1 = seg;
                int i2 = std::min(seg + 1, n - 1);
                int i3 = std::min(seg + 2, n - 1);
                double h1 = hours[i1], h2 = hours[i2];
                double t = (h2 - h1) == 0 ? 0 : (hour - h1) / (h2 - h1);
                double y_val = catmullRom(values[i0], values[i1], values[i2], values[i3], t);
                double y_norm = (y_val - minY) / (maxY - minY);
                int y = wave_y_base - (int)(y_norm * wave_height);
                offscreen_canvas->SetPixel(x, y, 255, 0, 0);
            }

            // Mark tide points as blue dots
            for (size_t i = 0; i < hours.size(); ++i)
            {
                int x = wave_x_start + (int)(((hours[i] - minHourOffset) / hourRange) * (wave_x_end - wave_x_start));
                double y_norm = (values[i] - minY) / (maxY - minY);
                int y = wave_y_base - (int)(y_norm * wave_height);
                offscreen_canvas->SetPixel(x, y, 255, 0, 0);
            }

            // Mark current time as green dot
            int x_now = wave_x_start + (int)(((0 - minHourOffset) / hourRange) * (wave_x_end - wave_x_start));
            // Interpolate y for current time
            double y_now_val;
            int seg_now = 0;
            while (seg_now + 1 < (int)hours.size() && hours[seg_now + 1] < 0)
                ++seg_now;
            if (seg_now + 1 < (int)hours.size())
            {
                double h0 = hours[seg_now], h1 = hours[seg_now + 1];
                double v0 = values[seg_now], v1 = values[seg_now + 1];
                double t = (0 - h0) / (h1 - h0);
                y_now_val = v0 + t * (v1 - v0);
            }
            else
            {
                y_now_val = values.back();
            }
            double y_now_norm = (y_now_val - minY) / (maxY - minY);
            int y_now = wave_y_base - (int)(y_now_norm * wave_height);
            offscreen_canvas->SetPixel(x_now, y_now, 0, 0, 255); // current time
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

    drawTideCurve();

    offscreen_canvas = matrix->SwapOnVSync(offscreen_canvas);
}
