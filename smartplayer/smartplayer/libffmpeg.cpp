#include <boost/thread.hpp>

boost::recursive_mutex	lock;

void ffmpeg_lock()
{
	lock.lock();
}


void ffmpeg_unlock()
{
	lock.unlock();
}