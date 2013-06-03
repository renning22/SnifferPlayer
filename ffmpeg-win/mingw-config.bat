export CPPFLAGS="-I/local/include"
export LDFLAGS="-L/local/lib"
./configure --enable-memalign-hack --target-os=mingw32 --enable-shared --disable-static --enable-gpl --enable-version3 --enable-pthreads --enable-runtime-cpudetect --enable-avisynth --enable-bzlib --enable-libvo-aacenc --enable-libx264 --enable-zlib