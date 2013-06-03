#include "episode.h"
#include "utility.hpp"
#include "stream.h"
#include "log.h"
#include <SDL.h>


Episode::Clips::Clips()
{
	stream = new Stream();
	remembered_duration = -1;
	lock_ = boost::shared_ptr<boost::recursive_mutex>(new boost::recursive_mutex);
}

Episode::Clips::Clips(Clips && c)
{
	auto locker = c.locker();
	uri = c.uri;
	stream = c.stream;
	remembered_duration = c.remembered_duration;

	c.uri = "";
	c.stream = NULL;
	c.remembered_duration = -1;

	lock_ = boost::shared_ptr<boost::recursive_mutex>(new boost::recursive_mutex);
}

Episode::Clips::~Clips()
{
	if( stream )
		delete stream;
	lock_.reset();
}

bool Episode::Clips::start()
{
	return stream->start(uri);
}

double Episode::Clips::duration() const
{
	if( remembered_duration > 0 )
	{
		return remembered_duration;
	}
	else if( stream->stream_duration() > 0 )
	{
		return stream->stream_duration();
	}
	return -1;
}

void Episode::Clips::remember_duration()
{
	if( stream->stream_duration() > 0 )
	{
		remembered_duration = stream->stream_duration();
	}
}

boost::shared_ptr<boost::lock_guard<boost::recursive_mutex>> Episode::Clips::locker()
{
	return boost::shared_ptr<boost::lock_guard<boost::recursive_mutex>>(new boost::lock_guard<boost::recursive_mutex>(*lock_));
}


Episode::Episode(std::string src_page,std::vector<std::string> uris):
src_page(src_page),
has_started_(false)
{
	clips_set.clear();
	clips_set.resize(uris.size());
	for(std::size_t i=0;i!=clips_set.size();i++)
	{
		clips_set[i].uri = uris[i];
	}
}

Episode::~Episode()
{
}

bool Episode::start()
{
	if( !stopped() )
		return false;
	if( has_started() )
		return false;
	if( clips_set.size() == 0 )
		return false;

	width = -1;
	height = -1;

	INFO << "Episode start :";
	for(std::size_t i=0;i!=clips_set.size();i++)
		INFO << "\t" << clips_set[i].uri;

	for(std::size_t i=0;i!=clips_set.size();i++)
	{
		clips_set[i].start();
	}

	has_started_ = true;

	return true;
}

bool Episode::stopped()
{
	bool has_stopped = true;
	for(auto i=clips_set.begin();i!=clips_set.end();i++)
	{
		if( !i->stream->stopped() )
			has_stopped = false;
	}
	return has_stopped;
}

void Episode::stop()
{
	INFO << "Episode stop";
	for(auto i=clips_set.begin();i!=clips_set.end();i++)
	{
		i->stream->stop();
	}
}

void Episode::remember_duration()
{
	for(auto i=clips_set.begin();i!=clips_set.end();i++)
	{
		i->remember_duration();
	}
}

bool Episode::has_started() const
{
	return has_started_;
}

double Episode::duration() const
{
	double sum = 0;
	for(auto it=clips_set.begin();it!=clips_set.end();it++)
	{
		if( it->duration() > 0 )
			sum += it->duration();
	}
	return sum;
}

std::vector<std::pair<double,double>> Episode::buffer_status(bool merge) const
{
	std::vector<std::pair<double,double>> ret;
	std::pair<double,double> current(0,0);
	double sum = 0;
	for(auto it=clips_set.begin();it!=clips_set.end();it++)
	{
		current.first = sum + it->stream->last_seek_pos();
		current.second = sum + it->stream->stream_buffer_pos();
		ret.push_back(current);
		sum += it->duration();
	}
	if( merge )
	{
		std::vector<std::pair<double,double>> after_merged;
		if( ret.empty() )
			return ret;
		for(auto it=ret.begin();it!=ret.end();it++)
		{
			if( it==ret.begin() )
			{
				current.first = it->first;
				current.second = it->second;
				continue;
			}
			auto jj = it-1;
			// merge open balls of 0.5s radius
			if( fabs( it->first - jj->second ) < 0.5 )
			{
				current.second = it->second;
			}
			else
			{
				after_merged.push_back(current);
				current.first = it->first;
				current.second = it->second;
			}
		}
		after_merged.push_back(current);
		return after_merged;
	}
	else
	{
		return ret;
	}
}

Episode::Clips * Episode::index(std::size_t i)
{
	if( i >= clips_set.size() )
		return NULL;
	else
		return &clips_set[i];
}

std::size_t Episode::size() const
{
	return clips_set.size();
}