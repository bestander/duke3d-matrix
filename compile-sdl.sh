cmake -S ./libs/SDL -B ./libs/SDL/build  -DSDL_DUMMYVIDEO=ON && cmake --build ./libs/SDL/build && sudo cmake --install ./libs/SDL/build --prefix /usr/local
