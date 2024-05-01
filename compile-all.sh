echo "building SDL"
cmake -S ./libs/SDL -B ./libs/SDL/build  -DSDL_DUMMYVIDEO=ON && cmake --build ./libs/SDL/build && sudo cmake --install ./libs/SDL/build --prefix /usr/local
echo "building SDL_mixer"
cd ./libs/SDL_mixer && ./configure && make && sudo make install
echo "compiling doom matrix"
cd ../.. && make
