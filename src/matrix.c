#include "doomgeneric.h"
#include "led-matrix-c.h"
#include "doomkeys.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
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
char text_buffer[80];


/*
dimensions for the drawing surface which may be
less than the matrix dimensions to account for aspect ratio
*/
int surfaceWidth, surfaceHeight;
int matrixWidth, matrixHeight;

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

int main(int argc, char **argv)
{
  struct RGBLedMatrixOptions options;

  memset(&options, 0, sizeof(options));

  meteosource_init();

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
    doomgeneric_Tick();
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

  uint32_t *pix = surface->pixels;
  for (int y = 0; y < matrixHeight; ++y)
  {
    for (int x = 0; x < matrixWidth; ++x)
    {
      if (x < surfaceWidth && y < surfaceHeight) {
        uint8_t r = *pix >> 16;
        uint8_t g = *pix >> 8;
        uint8_t b = *pix;
        led_canvas_set_pixel(offscreen_canvas, x, y, r, g, b);
        pix++;
      } else {
        led_canvas_set_pixel(offscreen_canvas, x, y, 0, 0, 0);
      }
    }
  }
  // TODO if --time passed
  time(&rawtime);
  info = localtime(&rawtime);
  strftime(text_buffer, 80, "%H:%M", info);
  draw_text(offscreen_canvas, font, 0, 64, 255, 255, 0, text_buffer, 1);
  // TODO if meteo-source-key is passed
  // TODO schedule meteo to be queries once an hour


  offscreen_canvas = led_matrix_swap_on_vsync(matrix, offscreen_canvas);
  handleKeyInput();
}
