# sudo setcap 'cap_sys_nice=eip' ./doom_matrix
# SDL_VIDEODRIVER=evdev ./doom_matrix -iwad doom1.wad --led-gpio-mapping=adafruit-hat --led-rows=32 --led-cols=64 --led-chain=2 --led-pixel-mapper="U-mapper" --led-slowdown-gpio=3 --led-pwm-bits=10 --led-pwm-lsb-nanoseconds=500 --led-pwm-dither-bits=2 $@
# TODO pass config with resolution and no launcher
./eduke32