#pragma once

#include <boost/function.hpp>
#include <boost/tuple/tuple.hpp>

/* SDL audio buffer size, in samples. Should be small to have precise
   A/V sync as SDL does not have hardware buffer fullness info. */
#define SPEAKER_AUDIO_BUFFER_SIZE 1024

void speaker_init();

bool speaker_open(int frequency,uint8_t channels,boost::function<void(uint8_t * buf,int len)> callback);

bool speaker_close();