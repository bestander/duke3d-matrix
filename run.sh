# sudo setcap 'cap_sys_nice=eip' ./eduke32
# SDL_VIDEODRIVER=evdev 
./eduke32 -l1 --led-gpio-mapping=adafruit-hat --led-rows=64 --led-cols=64 --led-chain=2 --led-pixel-mapper="U-mapper" --led-slowdown-gpio=3 --led-pwm-bits=10 --led-pwm-lsb-nanoseconds=500 --led-pwm-dither-bits=2 $@