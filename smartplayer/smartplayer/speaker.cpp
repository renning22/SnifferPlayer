#include <SDL.h>
#include "speaker.h"


boost::function<void(uint8_t * buf,int len)> callback;

void speaker_init()
{

}

static void sdl_audio_callback(void *opaque, Uint8 *stream, int len)
{
	if( callback )
		callback(stream,len);
}

bool speaker_open(int frequency,uint8_t channels,boost::function<void(uint8_t * buf,int len)> callback)
{
	return false;
}

bool speaker_close()
{
	return false;
}
