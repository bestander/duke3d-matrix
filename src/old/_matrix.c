#include "doomgeneric.h"
#include "led-matrix-c.h"
#include "doomkeys.h"
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <SDL.h>
#include <time.h>
#include "meteosource_c.h"

SDL_Surface *surface = NULL;
SDL_Renderer *renderer = NULL;
SDL_Texture *texture = NULL;
SDL_Window *window = NULL;

struct RGBLedMatrix *matrix;
struct LedFont *font;
struct LedCanvas *offscreen_canvas;
time_t rawtime;
struct tm *info;

Meteosource *meteosource;
MinMaxTemperature temperature_forecast;
char *metsource_key = NULL;
char *metsource_place_id = NULL;

/*
dimensions for the drawing surface which may be
less than the matrix dimensions to account for aspect ratio
*/
int surface_width, surface_height;
int matrix_width, matrix_height;
bool is_game_sleeping = false;
int sleep_timeout = 0;
int refresh_weather_timeout = 0;
clock_t last_activity_time;


void getWeather(char *metsource_key, char *metsource_place_id)
{
  if (meteosource)
  {
    char *sections = "hourly";
    char *timezone = "UTC";
    char *language = "en";
    char *units = "metric";
    temperature_forecast = get_min_max_temparature_forecast(meteosource, metsource_place_id, sections, timezone, language, units);
  }
}

static void handleKeyInput()
{
  SDL_Event e;

  while (SDL_PollEvent(&e))
  {
    if (e.type == SDL_QUIT)
    {
      puts("Quit requested");
      atexit(SDL_Quit);
      exit(1);
    }
    is_game_sleeping = false;
    S_SetMusicVolume(127);
    last_activity_time = clock();
  }
}


void drawFrame()
{

  if (!temperature_forecast.isError)
  {
    char temp_message[20];
    sprintf(temp_message, "%.1f° - %.1f°", temperature_forecast.min, temperature_forecast.max);
    draw_text(offscreen_canvas, font, 0, 64, 255, 255, 0, temp_message, 1);
  }

  offscreen_canvas = led_matrix_swap_on_vsync(matrix, offscreen_canvas);
  handleKeyInput();
}

void *refreshWeatherAndRestartDoom(void *arg)
{
  while (true)
  {
    printf("Timer thread: calling refreshWeatherAndRestartDoom\n");
    getWeather(metsource_key, metsource_place_id);
    is_game_sleeping = false;
    S_SetMusicVolume(127);
    sleep(refresh_weather_timeout);
  }
  return NULL;
}

void *putDoomToSleep(void *arg)
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
        S_SetMusicVolume(0);
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

int main(int argc, char **argv)
{
  struct RGBLedMatrixOptions options;

  memset(&options, 0, sizeof(options));

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
  if (metsource_key && metsource_place_id)
  {
    meteosource = meteosource_init(metsource_key, "free");
  }

  pthread_t refresh_thread_id;
  if (refresh_weather_timeout)
  {
    int err = pthread_create(&refresh_thread_id, NULL, refreshWeatherAndRestartDoom, NULL);
    if (err != 0)
    {
      printf("Error creating timer thread: %d\n", err);
      return 1;
    }
  }
  pthread_t sleep_thread_id;
  if (sleep_timeout)
  {
    int err = pthread_create(&sleep_thread_id, NULL, putDoomToSleep, NULL);
    if (err != 0)
    {
      printf("Error creating timer thread: %d\n", err);
      return 1;
    }
  }

  temperature_forecast.isError = true;

  last_activity_time = clock();

  matrix = led_matrix_create_from_options(&options, &argc, &argv);
  if (matrix == NULL)
    return 1;

  font = load_font("libs/rpi-rgb-led-matrix/fonts/4x6.bdf");
  offscreen_canvas = led_matrix_create_offscreen_canvas(matrix);
  led_canvas_get_size(offscreen_canvas, &matrix_width, &matrix_height);
  fprintf(stderr, "Size: %dx%d. Hardware gpio mapping: %s\n",
          matrix_width, matrix_height, options.hardware_mapping);

  doomgeneric_Create(argc, argv);
  signal(SIGINT, catchInt);
  while (true)
  {
    if (!is_game_sleeping)
    {
      doomgeneric_Tick();
    }
    else
    {
      drawFrame();
    }
  }

  return 0;
}
