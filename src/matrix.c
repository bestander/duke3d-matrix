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
MinMaxTemperature temperatureForecast;
char *metsource_key = NULL;
char *metsource_place_id = NULL;

/*
dimensions for the drawing surface which may be
less than the matrix dimensions to account for aspect ratio
*/
int surfaceWidth, surfaceHeight;
int matrixWidth, matrixHeight;
bool is_game_sleeping = false;
int sleep_timeout = 0;
int refresh_weather_timeout = 0;
clock_t last_activity_time;

#define KEYQUEUE_SIZE 16

static unsigned short s_KeyQueue[KEYQUEUE_SIZE];
static unsigned int s_KeyQueueWriteIndex = 0;
static unsigned int s_KeyQueueReadIndex = 0;

static unsigned char convertToDoomKey(unsigned int key)
{
  switch (key)
  {
  case SDLK_RETURN:
    key = KEY_ENTER;
    break;
  case SDLK_ESCAPE:
    key = KEY_ESCAPE;
    break;
  case SDLK_LEFT:
    key = KEY_LEFTARROW;
    break;
  case SDLK_RIGHT:
    key = KEY_RIGHTARROW;
    break;
  case SDLK_UP:
    key = KEY_UPARROW;
    break;
  case SDLK_DOWN:
    key = KEY_DOWNARROW;
    break;
  case SDLK_LCTRL:
  case SDLK_RCTRL:
    key = KEY_FIRE;
    break;
  case SDLK_SPACE:
    key = KEY_USE;
    break;
  case SDLK_LSHIFT:
  case SDLK_RSHIFT:
    key = KEY_RSHIFT;
    break;
  case SDLK_LALT:
  case SDLK_RALT:
    key = KEY_LALT;
    break;
  case SDLK_F2:
    key = KEY_F2;
    break;
  case SDLK_F3:
    key = KEY_F3;
    break;
  case SDLK_F4:
    key = KEY_F4;
    break;
  case SDLK_F5:
    key = KEY_F5;
    break;
  case SDLK_F6:
    key = KEY_F6;
    break;
  case SDLK_F7:
    key = KEY_F7;
    break;
  case SDLK_F8:
    key = KEY_F8;
    break;
  case SDLK_F9:
    key = KEY_F9;
    break;
  case SDLK_F10:
    key = KEY_F10;
    break;
  case SDLK_F11:
    key = KEY_F11;
    break;
  case SDLK_EQUALS:
  case SDLK_PLUS:
    key = KEY_EQUALS;
    break;
  case SDLK_MINUS:
    key = KEY_MINUS;
    break;
  default:
    key = tolower(key);
    break;
  }

  return key;
}

static unsigned char convertToDoomJoyKey(unsigned int key)
{
  switch (key)
  {
  case 9: // start
    key = KEY_ENTER;
    break;
  case 8: // select
    key = KEY_ESCAPE;
    break;
  case SDLK_LEFT:
    key = KEY_LEFTARROW;
    break;
  case SDLK_RIGHT:
    key = KEY_RIGHTARROW;
    break;
  case SDLK_UP:
    key = KEY_UPARROW;
    break;
  case SDLK_DOWN:
    key = KEY_DOWNARROW;
    break;
  case 1: // B
    key = KEY_FIRE;
    break;
  case 3: // Y
    key = KEY_USE;
    break;
  case 4: // run
    key = KEY_RSHIFT;
    break;
  case 5: // ALT = strafe
    key = KEY_LALT;
    break;
  case 2: // plus / x
    key = KEY_EQUALS;
    break;
  case 0: // minus // a
    key = KEY_MINUS;
    break;
  }

  return key;
}

static void addKeyToQueue(int pressed, bool isJoystick, unsigned int keyCode)
{
  unsigned char key = isJoystick ? convertToDoomJoyKey(keyCode) : convertToDoomKey(keyCode);

  unsigned short keyData = (pressed << 8) | key;

  s_KeyQueue[s_KeyQueueWriteIndex] = keyData;
  s_KeyQueueWriteIndex++;
  s_KeyQueueWriteIndex %= KEYQUEUE_SIZE;
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
    if (e.type == SDL_JOYAXISMOTION)
    {
      if (e.jaxis.axis == 0)
      {
        if (e.jaxis.value > 32000)
        {
          addKeyToQueue(1, true, SDLK_RIGHT);
        }
        else if (e.jaxis.value < -32000)
        {
          addKeyToQueue(1, true, SDLK_LEFT);
        }
        else
        {
          addKeyToQueue(0, true, SDLK_RIGHT);
          addKeyToQueue(0, true, SDLK_LEFT);
        }
      }
      else
      {
        if (e.jaxis.value > 32000)
        {
          addKeyToQueue(1, true, SDLK_DOWN);
        }
        else if (e.jaxis.value < -32000)
        {
          addKeyToQueue(1, true, SDLK_UP);
        }
        else
        {
          addKeyToQueue(0, true, SDLK_UP);
          addKeyToQueue(0, true, SDLK_DOWN);
        }
      }
    }
    if (e.type == SDL_JOYBUTTONDOWN)
    {
      addKeyToQueue(1, true, e.jbutton.button);
    }
    if (e.type == SDL_JOYBUTTONUP)
    {
      addKeyToQueue(0, true, e.jbutton.button);
    }
    if (e.type == SDL_KEYDOWN)
    {
      addKeyToQueue(1, false, e.key.keysym.sym);
    }
    else if (e.type == SDL_KEYUP)
    {
      addKeyToQueue(0, false, e.key.keysym.sym);
    }
  }
}

int DG_GetKey(int *pressed, unsigned char *doomKey)
{
  if (s_KeyQueueReadIndex == s_KeyQueueWriteIndex)
  {
    // key queue is empty
    return 0;
  }
  else
  {
    unsigned short keyData = s_KeyQueue[s_KeyQueueReadIndex];
    s_KeyQueueReadIndex++;
    s_KeyQueueReadIndex %= KEYQUEUE_SIZE;

    *pressed = keyData >> 8;
    *doomKey = keyData & 0xFF;

    return 1;
  }

  return 0;
}

void catch_int(int sig_num)
{
  led_matrix_delete(matrix);
  exit(0);
}

void getWeather(char *metsource_key, char *metsource_place_id)
{
  if (meteosource)
  {
    char *sections = "hourly";
    char *timezone = "UTC";
    char *language = "en";
    char *units = "metric";
    temperatureForecast = get_min_max_temparature_forecast(meteosource, metsource_place_id, sections, timezone, language, units);
  }
}

void draw_frame()
{
  uint32_t *pix = surface->pixels;
  for (int y = 0; y < matrixHeight; ++y)
  {
    for (int x = 0; x < matrixWidth; ++x)
    {
      if (x < surfaceWidth && y < surfaceHeight)
      {
        if (!is_game_sleeping) {
          uint8_t r = *pix >> 16;
          uint8_t g = *pix >> 8;
          uint8_t b = *pix;
          led_canvas_set_pixel(offscreen_canvas, x, y, r, g, b);
        } else {
          led_canvas_set_pixel(offscreen_canvas, x, y, 0, 0, 0);
        }
        pix++;
      }
      else
      {
        led_canvas_set_pixel(offscreen_canvas, x, y, 0, 0, 0);
      }
    }
  }
  time(&rawtime);
  info = localtime(&rawtime);
  char time_buffer[20];
  strftime(time_buffer, 20, "%H:%M", info);
  draw_text(offscreen_canvas, font, 0, 56, 255, 255, 0, time_buffer, 1);

  if (!temperatureForecast.isError)
  {
    char temp_message[20];
    sprintf(temp_message, "%.1f° - %.1f°", temperatureForecast.min, temperatureForecast.max);
    draw_text(offscreen_canvas, font, 0, 64, 255, 255, 0, temp_message, 1);
  }

  offscreen_canvas = led_matrix_swap_on_vsync(matrix, offscreen_canvas);
  handleKeyInput();
}

void *refresh_weather_and_restart_doom(void *arg)
{
  while (true)
  {
    printf("Timer thread: calling refresh_weather_and_restart_doom\n");
    getWeather(metsource_key, metsource_place_id);
    is_game_sleeping = false;
    S_SetMusicVolume(127);
    sleep(refresh_weather_timeout);
  }
  return NULL;
}

void *put_doom_to_sleep(void *arg)
{
  while (true)
  {
    printf("Timer thread: calling put_doom_to_sleep\n");
    if (!is_game_sleeping) {
      clock_t now_time = clock();
      int elapsed_sec = (int)(now_time - last_activity_time) / CLOCKS_PER_SEC;
      if (elapsed_sec >= sleep_timeout) {
        is_game_sleeping = true;
        S_SetMusicVolume(0);
        sleep(sleep_timeout);
      } else {
        sleep(sleep_timeout - elapsed_sec);
      }
    } else {
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
  if (refresh_weather_timeout) {
    int err = pthread_create(&refresh_thread_id, NULL, refresh_weather_and_restart_doom, NULL);
    if (err != 0)
    {
      printf("Error creating timer thread: %d\n", err);
      return 1;
    }
  } 
  pthread_t sleep_thread_id;
  if (sleep_timeout) {
    int err = pthread_create(&sleep_thread_id, NULL, put_doom_to_sleep, NULL);
    if (err != 0)
    {
      printf("Error creating timer thread: %d\n", err);
      return 1;
    }
  } 

  temperatureForecast.isError = true;

  last_activity_time = clock();

  matrix = led_matrix_create_from_options(&options, &argc, &argv);
  if (matrix == NULL)
    return 1;

  font = load_font("libs/rpi-rgb-led-matrix/fonts/4x6.bdf");
  offscreen_canvas = led_matrix_create_offscreen_canvas(matrix);
  led_canvas_get_size(offscreen_canvas, &matrixWidth, &matrixHeight);
  fprintf(stderr, "Size: %dx%d. Hardware gpio mapping: %s\n",
          matrixWidth, matrixHeight, options.hardware_mapping);

  doomgeneric_Create(argc, argv);
  signal(SIGINT, catch_int);
  while (true)
  {
    if (!is_game_sleeping) {
      doomgeneric_Tick();
    } else {
      draw_frame();
    }
  }

  return 0;
}

void DG_Init()
{
  surfaceWidth = matrixWidth;
  surfaceHeight = matrixWidth * 0.75;

  if (surfaceHeight > matrixHeight)
  {
    surfaceHeight = matrixHeight;
    surfaceWidth = surfaceHeight / 0.75;
  }
  SDL_InitSubSystem(SDL_INIT_HAPTIC | SDL_INIT_JOYSTICK);
  SDL_Joystick *joy;
  if (SDL_NumJoysticks() > 0)
  {
    joy = SDL_JoystickOpen(0);
    if (joy)
    {
      printf("Opened Joystick 0\n");
      printf("Name: %s\n", SDL_JoystickName(0));
      printf("Number of Axes: %d\n", SDL_JoystickNumAxes(joy));
      printf("Number of Buttons: %d\n", SDL_JoystickNumButtons(joy));
      printf("Number of Balls: %d\n", SDL_JoystickNumBalls(joy));
    }
    else
    {
      printf("Couldn't open Joystick 0\n");
    }
  }
  S_SetMusicVolume(127);

  window = SDL_CreateWindow("DOOM",
                            SDL_WINDOWPOS_UNDEFINED,
                            SDL_WINDOWPOS_UNDEFINED,
                            DOOMGENERIC_RESX,
                            DOOMGENERIC_RESY,
                            SDL_WINDOW_FULLSCREEN_DESKTOP);
  surface = SDL_CreateRGBSurface(0, surfaceWidth, surfaceHeight, 32, 0, 0, 0, 0);
  renderer = SDL_CreateSoftwareRenderer(surface);
  texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB888, SDL_TEXTUREACCESS_TARGET, DOOMGENERIC_RESX, DOOMGENERIC_RESY);
}

void DG_SleepMs(uint32_t ms)
{
  SDL_Delay(ms);
}

uint32_t DG_GetTicksMs()
{
  return SDL_GetTicks();
}

void DG_SetWindowTitle(const char *title)
{
}

void DG_DrawFrame()
{
  SDL_UpdateTexture(texture, NULL, DG_ScreenBuffer, DOOMGENERIC_RESX * sizeof(uint32_t));
  SDL_RenderClear(renderer);
  SDL_RenderCopy(renderer, texture, NULL, NULL);
  SDL_RenderPresent(renderer);
  draw_frame();
}
