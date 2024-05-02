TARGET_EXEC := doom_matrix

BUILD_DIR := ./build
SRC_DIRS := ./src

DOOMGENERIC_DIR := ./libs/doomgeneric/doomgeneric
SRCS_DOOM = dummy.c am_map.c doomdef.c doomstat.c dstrings.c d_event.c d_items.c d_iwad.c d_loop.c d_main.c d_mode.c d_net.c f_finale.c f_wipe.c g_game.c hu_lib.c hu_stuff.c info.c i_cdmus.c i_endoom.c i_joystick.c i_scale.c i_sound.c i_system.c i_timer.c memio.c m_argv.c m_bbox.c m_cheat.c m_config.c m_controls.c m_fixed.c m_menu.c m_misc.c m_random.c p_ceilng.c p_doors.c p_enemy.c p_floor.c p_inter.c p_lights.c p_map.c p_maputl.c p_mobj.c p_plats.c p_pspr.c p_saveg.c p_setup.c p_sight.c p_spec.c p_switch.c p_telept.c p_tick.c p_user.c r_bsp.c r_data.c r_draw.c r_main.c r_plane.c r_segs.c r_sky.c r_things.c sha1.c sounds.c statdump.c st_lib.c st_stuff.c s_sound.c tables.c v_video.c wi_stuff.c w_checksum.c w_file.c w_main.c w_wad.c z_zone.c w_file_stdc.c i_input.c i_video.c mus2mid.c doomgeneric.c i_sdlmusic.c i_sdlsound.c
SRCS_DOOM := $(addprefix $(DOOMGENERIC_DIR)/,$(SRCS_DOOM))
OBJS := $(SRCS_DOOM:%=$(BUILD_DIR)/%.o)

RGB_LIB_DISTRIBUTION=./libs/rpi-rgb-led-matrix
RGB_LIBDIR=$(RGB_LIB_DISTRIBUTION)/lib
RGB_LIBRARY_NAME=rgbmatrix
RGB_LIBRARY=$(RGB_LIBDIR)/lib$(RGB_LIBRARY_NAME).a
RGB_LIBRARY_INCLUDE=$(RGB_LIB_DISTRIBUTION)/include

SDL_LIBS = `sdl2-config --cflags --libs` -lSDL2_mixer
LDFLAGS+=-L$(RGB_LIBDIR) -l$(RGB_LIBRARY_NAME) -lrt -lm -lc -lpthread $(SDL_LIBS)

METEOSOURCE_DIR := ./libs/meteosource
METEOSOURCE_INCLUDE := $(METEOSOURCE_DIR)/src
METEOSOURCE_ARTIFACT := $(METEOSOURCE_INCLUDE)/Meteosource.o
METEOSOURCE_LIBS = -lcurl -ljsoncpp

SRCS := $(shell find $(SRC_DIRS) -name '*.cpp' -or -name '*.c')
OBJS += $(SRCS:%=$(BUILD_DIR)/%.o)

INC_DIRS := $(shell find $(SRC_DIRS) -type d) $(DOOMGENERIC_DIR) $(RGB_LIBRARY_INCLUDE) $(METEOSOURCE_INCLUDE)
INC_FLAGS := $(addprefix -I,$(INC_DIRS)) $(SDL_LIBS)

CC = gcc
CXX = g++
CFLAGS = -DFEATURE_SOUND $(SDL_CFLAGS) -Wall
CXXFLAGS = -Wall -std=c++11  $(INC_FLAGS)
CPPFLAGS := $(INC_FLAGS) -MMD -MP

$(TARGET_EXEC): $(OBJS) $(RGB_LIBRARY) $(METEOSOURCE_ARTIFACT)
	$(CXX) $(shell find $(BUILD_DIR) -name '*.o') $(shell find $(METEOSOURCE_INCLUDE) -name '*.o') -o $@ $(LDFLAGS) $(METEOSOURCE_LIBS)

$(RGB_LIBRARY): FORCE
	$(MAKE) -C $(RGB_LIBDIR)

$(METEOSOURCE_ARTIFACT): FORCE
	$(MAKE) -C $(METEOSOURCE_DIR)

$(BUILD_DIR)/%.c.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@ $(LIBS)

$(BUILD_DIR)/%.cpp.o: %.cpp
	mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)
	rm -f $(TARGET_EXEC)
	$(MAKE) -C $(RGB_LIBDIR) clean

FORCE:
.PHONY: FORCE

