#include <limits>

#include "config.h"
#include <inttypes.h>
#include <math.h>
#include <limits.h>
#include <signal.h>

extern "C"
{

#include "libavutil/avstring.h"
#include "libavutil/colorspace.h"
#include "libavutil/mathematics.h"
#include "libavutil/pixdesc.h"
#include "libavutil/imgutils.h"
#include "libavutil/dict.h"
#include "libavutil/parseutils.h"
#include "libavutil/samplefmt.h"
#include "libavutil/avassert.h"
#include "libavformat/avformat.h"
// visit private http function
#include "libavformat/http.h"
//
#include "libavdevice/avdevice.h"
#include "libswscale/swscale.h"
#include "libavutil/opt.h"
#include "libavcodec/avfft.h"
#include "libswresample/swresample.h"

#ifdef CONFIG_AVFILTER
# include "libavfilter/avcodec.h"
# include "libavfilter/avfilter.h"
# include "libavfilter/avfiltergraph.h"
# include "libavfilter/buffersink.h"
#endif

}

//#include <unistd.h>
#include <assert.h>

#include <SDL.h>
#include <SDL_thread.h>
#include <SDL_surface.h>

#include "engine.h"
#include "screenui.h"
#include "screen.h"
#include "ffplay.h"
#include "stream.h"
#include "streamplay.h"
#include "episode.h"
#include "utility.hpp"

#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <windows.h>

#include "log.h"

////////////
////////////

static std::shared_ptr<boost::asio::io_service>		working_ioservice;

static std::shared_ptr<boost::thread>				working_thread;

static int											abort_request = 0;

static int lockmgr(void **mtx, enum AVLockOp op)
{
   switch(op) {
      case AV_LOCK_CREATE:
          *mtx = SDL_CreateMutex();
          if(!*mtx)
              return 1;
          return 0;
      case AV_LOCK_OBTAIN:
          return !!SDL_LockMutex((SDL_mutex*)*mtx);
      case AV_LOCK_RELEASE:
          return !!SDL_UnlockMutex((SDL_mutex*)*mtx);
      case AV_LOCK_DESTROY:
          SDL_DestroyMutex((SDL_mutex*)*mtx);
          return 0;
   }
   return 1;
}


static bool delete_stream(Episode::Clips * clips)
{
	Stream * &stream = clips->stream;

	clips->remember_duration();

	Stream * that_stream = stream;
	working_ioservice->post( [=]()
	{
		Stream * that_that_stream = that_stream;
		that_that_stream->stop();
		block_wait(	[&](){ return that_that_stream->stopped(); },
					[&](){ return false; },
					[&](){ SDL_Delay(50); });
		delete that_that_stream;
	});

	stream = new Stream();

	return true;
}

static bool restart_stream(Episode::Clips * clips)
{
	DEBUG << "ffplay : restart_stream : " << clips->uri;

	delete_stream(clips);

	return clips->start();
}

static bool seek_stream(Episode::Clips * clips,double & corrected_pos)
{
	Stream * &stream = clips->stream;
	double pos = corrected_pos;

	int try_count = 0;
	for(;try_count<3;try_count++)
	{
		if( stream->has_started() )
		if( stream->has_end_of_file() || (!stream->failed() && !stream->stopped()) )
		{
			if( pos >= stream->last_seek_pos() )
			{
				// now assume buffer is continuous!
				if( pos < stream->stream_buffer_pos() )
				{
					// in buffer, now to redirect stream
					if( stream->query_buffer(corrected_pos) )
					{
						// perfect enough
						DEBUG << "seek_stream : seek in buffer, enough";
						return true;
					}
					else
					{
						DEBUG << "seek_stream : seek in buffer, not enough, wait ( seek in net )";
						// not enough, just wait
						// maybe from pos to the end has no any keyframe. to be fixed.
						// in this case ,to be guaranteed, seek in net.
						//return true;
					}
				}
				else
				{
					// don't wait, too far away, just redirect
				}
			}
		}
		stream->seek(pos);
		// stopped because perfect finished
		if( stream->stopped() || stream->failed() )
		{
			WARN << "ffplay : seek_stream : seeked failed , restart a stream, at " << pos;
			restart_stream(clips);
		}
		else
		{
			break;
		}
	}
	if( try_count >= 3 )
	{
		WARN << "ffplay : seek_stream : failed , tried so many times, maybe this stream doesn't support seek online";
		return false;
	}
	return true;
}

/////
// extern funcs
/////

int ffplay_init(int argc,char* argv[])
{
	int flags;

    //av_log_set_flags(AV_LOG_SKIP_REPEATED);
    //parse_loglevel(argc, argv, options);

    /* register all codecs, demux and protocols */
    avcodec_register_all();
#ifdef CONFIG_AVDEVICE
    avdevice_register_all();
#endif
#ifdef CONFIG_AVFILTER
    avfilter_register_all();
#endif
    av_register_all();
    avformat_network_init();


	// init private http headers
	//avformat_set_http_header(
	//"Accept:text/html,application/xhtml+xml,application/xml;q=0.9,*/""*;q=0.8" "\r\n"
	//"Accept-Charset:ISO-8859-1,utf-8;q=0.7,*;q=0.3" "\r\n"
	//"Accept-Encoding:gzip,deflate,sdch" "\r\n"
	//"Accept-Language:en-US,en;q=0.8" "\r\n"
	//"Cache-Control:no-cache" "\r\n"
	//"Connection:keep-alive" "\r\n"
	//"Pragma:no-cache" "\r\n"
	//"User-Agent:Mozilla/5.0 (Windows NT 6.1; WOW64) AppleWebKit/537.11 (KHTML, like Gecko) Chrome/23.0.1271.97 Safari/537.11" "\r\n"
	//);
	//
    //init_opts();

    //signal(SIGINT , sigterm_handler); /* Interrupt (ANSI).    */
    //signal(SIGTERM, sigterm_handler); /* Termination (ANSI).  */

    //show_banner(argc, argv, options);

    //parse_options(NULL, argc, argv, options, opt_input_file);

	/*
    if (!input_filename)
	{
        show_usage();
        fprintf(stderr, "An input file must be specified\n");
        fprintf(stderr, "Use -h to get full help or, even better, run 'man %s'\n", program_name);
        exit(1);
    }
	*/

    flags = SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER;
    //if (audio_disable)
    //    flags &= ~SDL_INIT_AUDIO;
//#if !defined(__MINGW32__) && !defined(__APPLE__)
//    flags |= SDL_INIT_EVENTTHREAD; /* Not supported on Windows or Mac OS X */
//#endif
    if (SDL_Init (flags))
	{
        fprintf(stderr, "Could not initialize SDL - %s\n", SDL_GetError());
        fprintf(stderr, "(Did you set the DISPLAY variable?)\n");
		return -1;
        //exit(1);
    }

    //SDL_EventState(SDL_ACTIVEEVENT, SDL_IGNORE);
    //SDL_EventState(SDL_SYSWMEVENT, SDL_IGNORE);
    //SDL_EventState(SDL_USEREVENT, SDL_IGNORE);

    if (av_lockmgr_register(lockmgr))
	{
        fprintf(stderr, "Could not initialize lock manager!\n");
		return -1;
    }

	working_ioservice = std::make_shared<boost::asio::io_service>();

	working_thread = std::make_shared<boost::thread>(
	[=]()
	{
		DEBUG << "ffplay : Working thread started";
		//boost::asio::io_service::work wk(*working_ioservice);
		while( !abort_request )
		{
			if( !working_ioservice->poll() )
				boost::this_thread::sleep( boost::posix_time::milliseconds(50) );
		}
		DEBUG << "ffplay : Working thread ended";
	});
	return 0;
}

int ffplay_uninit()
{
	ffplay_episode_stop();

	abort_request = 1;
	working_thread->join();

    av_lockmgr_register(NULL);
    //uninit_opts();
#ifdef CONFIG_AVFILTER
    avfilter_uninit();
#endif
    avformat_network_deinit();
    //if (show_status)
    //    printf("\n");
    av_log(NULL, AV_LOG_QUIET, "%s", "");
	
    SDL_Quit();
	return 0;
}

static StreamPlay * stream_play_ = NULL;
static std::size_t stream_play_clips_index = 0;
static Episode * this_episode = NULL;
static double last_seeking_pos = 0;

bool ffplay_episode_start(std::string src_page,std::vector<std::string> urls)
{
	ffplay_episode_stop();

	last_seeking_pos = 0;

	this_episode = new Episode(src_page,urls);

	// asynchronized start stream, StreamPlay would handle it.
	working_ioservice->post( [=]()
	{
		this_episode->start();
	});

	stream_play_clips_index = 0;
	auto the_clips = this_episode->index(stream_play_clips_index);
	stream_play_ = new StreamPlay(the_clips->stream);
	bool ret = stream_play_->start();
	if( !ret )
	{
		DEBUG << "ffplay_episode_start failed";
		ffplay_episode_stop();
		engine_episode_playing_failed("启动网络连接失败");
		return false;
	}
	return true;
}

bool ffplay_episode_next_clips()
{
	if( !ffplay_episode_playing() )
		return false;

	if( stream_play_clips_index != this_episode->size() )
		stream_play_clips_index++;

	if( stream_play_clips_index == this_episode->size() )
	{
		engine_episode_playing_stopped_from_ffplay();
	}
	else
	{
		stream_play_->stop();
		block_wait([&](){ return stream_play_->stopped(); },
				   [&](){ return false; },
				   [&](){ SDL_Delay(50); });
		delete stream_play_;
		stream_play_ = NULL;
		auto this_clips = this_episode->index(stream_play_clips_index);

		if( !this_clips->stream->has_started() || this_clips->stream->failed() || this_clips->stream->last_seek_pos() != 0 )
		{
			DEBUG << "ffplay_episode_next_clips : but next stream is failed or not started from beginning, so restart it";
			double pos = 0;
			if( !seek_stream(this_clips,pos) )
			{
				engine_episode_playing_failed("下一小段网络连接失败");
				return false;
			}
		}

		stream_play_ = new StreamPlay(this_clips->stream);
		if( !stream_play_->start() )
		{
			block_wait([&](){ return stream_play_->stopped(); },
					   [&](){ return false; },
					   [&](){ SDL_Delay(50); });
			delete stream_play_;
			stream_play_ = NULL;

			engine_episode_playing_failed("不能播放下一小段");
			return false;
		}
	}
	return true;
}

void ffplay_episode_toggle_pause()
{
	if( !ffplay_episode_playing() )
		return;
	stream_play_->toggle_pause();
}

bool ffplay_episode_playing()
{
	return stream_play_!=NULL;
}

bool ffplay_episode_pausing()
{
	if( !ffplay_episode_playing() )
		return false;
	return stream_play_->pausing();
}

void ffplay_episode_stop()
{
	if( stream_play_ )
	{
		stream_play_->stop();
		block_wait([&](){ return stream_play_->stopped(); },
				   [&](){ return false; },
				   [&](){ SDL_Delay(50); });
		delete stream_play_;
		stream_play_ = NULL;
	}

	if( this_episode )
	{
		auto that_episode = this_episode;
		this_episode = NULL;

		// new thread to do it
		boost::thread t(
		//working_ioservice->post(
		[=]()
		{
			auto that_that_episode = that_episode;
			block_wait(	[&](){ return that_that_episode->has_started(); },
						[&](){ return false; },
						[&](){ SDL_Delay(50); });
			that_that_episode->stop();
			block_wait(	[&](){ return that_that_episode->stopped(); },
						[&](){ return false; },
						[&](){ SDL_Delay(50); });
			delete that_that_episode;
		});
	}
	/*
	if( stream_ )
	{
		stream_->stop();
		block_wait([&](){ return stream_->stopped(); },
				   [&](){ return false; },
				   [&](){ SDL_Delay(50); });
		delete stream_;
		stream_ = NULL;
	}
	*/
}

bool ffplay_episode_stopped()
{
	return this_episode==NULL;
}

void ffplay_episode_seek(double pos)
{
	if( !ffplay_episode_playing() )
		return;

	// range the pos
	{
		double min = 0;
		double max = ffplay_episode_duration();
		if( pos > max )
			pos = max;
		if( pos < min )
			pos = min;
	}

	last_seeking_pos = pos;

	std::size_t target_clips_index = 0;
	double target_pos_in_clips = 0;
	// calculate target_clips_index and pos in it
	{
		std::size_t i;
		double aggregate_duration = 0;
		for(i=0;i!=this_episode->size();i++)
		{
			double this_duration = this_episode->index(i)->duration();
			if( pos >= aggregate_duration && aggregate_duration + this_duration > pos )
			{
				// if too close to the end of a clips, play the next. threshold is 15s
				if( fabs( aggregate_duration + this_duration - pos ) <= 15.0 )
				{
					target_clips_index = i+1;
					target_pos_in_clips = 0;
				}
				else
				{
					target_clips_index = i;
					target_pos_in_clips = pos - aggregate_duration;
				}
				break;
			}
			aggregate_duration += this_duration;
		}
		if( i >= this_episode->size() )
		{
			//ERROR << "ffplay_stream_seek : couldn't calculate target_clips_index and target_pos_in_clips from pos, maybe it's the end of the episode";
			//engine_episode_playing_failed_from_ffplay();
			DEBUG << "ffplay_stream_seek : it seemed seeked to the end, redirect 10.0s before #1";
			ffplay_episode_seek(pos - 10.0);
			return;
		}
		INFO << "ffplay_stream_seek : index: " << target_clips_index << " , pos: " << target_pos_in_clips;
	}

	stream_play_clips_index = target_clips_index;

	// redirect the stream
	auto this_clips = this_episode->index(stream_play_clips_index);
	if( !this_clips )
	{
		// it seemed to the end
		DEBUG << "ffplay_stream_seek : it seemed seeked to the end, redirect 10.0s before #2";
		ffplay_episode_seek(pos - 10.0);
		return;
	}

	//working_ioservice->post( [=]()
	// stop the playing
	{
		stream_play_->stop();
		block_wait([&](){ return stream_play_->stopped(); },
					[&](){ return false; },
					[&](){ SDL_Delay(50); });
		delete stream_play_;
		stream_play_ = NULL;
	}// );

	if( !seek_stream(this_clips,target_pos_in_clips) )
	{
		// it is indeed a failure.
		engine_episode_playing_failed("寻找失败");
		return;
	}

	stream_play_ = new StreamPlay(this_clips->stream);
	stream_play_->start(target_pos_in_clips);
}

std::string ffplay_episode_src_page()
{
	if( this_episode )
	{
		return this_episode->src_page;
	}
	return "";
}

double ffplay_episode_current_playing_pos()
{
	if( !ffplay_episode_playing() )
		return -1;

	double aggregate = 0;
	for(std::size_t i=0;i!=this_episode->size();i++)
	{
		if( i == stream_play_clips_index )
			break;
		aggregate += this_episode->index(i)->duration();
	}
	if( stream_play_->current_playing_pos() < 0 )
	{
		return last_seeking_pos;
	}
	else
		return aggregate + stream_play_->current_playing_pos();
}

double ffplay_episode_how_long_buffered(double tolerance_buffer_secs)
{
	if( this_episode )
	{
		double current_position = ffplay_episode_current_playing_pos();
		if( current_position < 0 )
			return 0;

		double duration = ffplay_episode_duration();
		if( duration < 0 )
			return 0;

		auto buffer_status = ffplay_episode_buffer_status();
		if( buffer_status.empty() )
			return 0;

		const double epsilon = tolerance_buffer_secs/5;
		for(auto i=buffer_status.begin();i!=buffer_status.end();i++)
		{
			if( fabs( i->second - duration ) <= epsilon )
			{
				i->second += tolerance_buffer_secs;
			}
			if( current_position >= i->first - epsilon && current_position <= i->second + epsilon )
			{
				return std::max(0.0,i->second - current_position);
			}
		}
	}
	return 0;
}

std::vector<std::pair<double,double>> ffplay_episode_buffer_status(bool merge)
{
	if( this_episode )
	{
		return this_episode->buffer_status(merge);
	}
	return std::vector<std::pair<double,double>>();
}

double ffplay_episode_duration()
{
	if( this_episode )
	{
		return this_episode->duration();
	}
	return 0;
}

void ffplay_streamplay_size_obtain_from_engine(int width,int height)
{
	if( this_episode )
	{
		if( this_episode->width == -1 || this_episode->height == -1 )
		{
			this_episode->width = width;
			this_episode->height = height;
			
			engine_episode_size_obtain_from_ffplay(width,height);
		}
		else
		{
			if( this_episode->width != width ||
				this_episode->height != height )
			{
				WARN << "ffplay_streamplay_size_obtain_from_engine : warnning , same episode with different width and height, " << this_episode->width << "x" << this_episode->height << " to " << width << "x" << height;
			}
		}
	}
}

std::pair<int,int> ffplay_episode_size()
{
	if( this_episode )
	{
		return std::make_pair(this_episode->width,this_episode->height);
	}
	else
	{
		return std::make_pair(-1,-1);
	}
}

void ffplay_clips_buffered_up_from_engine(std::string filename)
{
	if( this_episode )
	{
		for(std::size_t i=0;i!=this_episode->size();i++)
		{
			if( this_episode->index(i)->uri == filename )
			if( i+1 < this_episode->size() )
			if( this_episode->index(i+1)->stream->last_seek_pos() != 0 )
			{
				INFO << "ffplay_clips_buffered_up_from_engine : start next stream buffering ,(to do) stream" << i+1;
				break;
			}
		}
	}
}


static void downloading_manager_idle()
{
	if( this_episode )
	{
		double sum_duration = 0;
		double current_play_pos = ffplay_episode_current_playing_pos();

		if( current_play_pos > 0 )
		for(std::size_t i=0;i<this_episode->size();sum_duration+=this_episode->index(i)->duration(),i++)
		{
			auto this_clips = this_episode->index(i);
			Stream * &this_stream = this_clips->stream;

			if( i < stream_play_clips_index  )
			{
				if( this_stream->is_ready_to_play() && !this_stream->has_end_of_file() && !this_stream->stopped() && !this_stream->failed() )
				{
					delete_stream(this_clips);
				}
				// previous does not care
				continue;
			}
			else if( i == stream_play_clips_index )
			{
				// let streamplay tackle with stream problems
				continue;
			}

			std::size_t j = i-1;
			if( j >= this_episode->size() )
				continue;

			auto previous_clips = this_episode->index(j);
			Stream * &previous_stream = previous_clips->stream;

			double buffer_ratio = 0;
			bool almost_playing_to_the_end = false;

			if( i == stream_play_clips_index + 1 )
			if( current_play_pos <= sum_duration  )
			{
				// almost playing to the end ,and playing next threshold
				if( fabs( sum_duration - current_play_pos) <= 25.0 )
				{
					almost_playing_to_the_end = true;
				}
			}

			if( previous_stream->has_started() && previous_stream->is_ready_to_play() )
			{
				double previous_stream_duration = previous_clips->duration();
				double previuos_buffered_duration = previous_stream->stream_buffer_pos() - previous_stream->last_seek_pos();
				buffer_ratio = previuos_buffered_duration / previous_stream_duration;
			}

			if( buffer_ratio >= 0.80 || almost_playing_to_the_end )
			{	
				if( almost_playing_to_the_end && i == stream_play_clips_index )
				{
					DEBUG << "ffplay : downloading_manager_idle : almost_playing_to_the_end = true , start next";
				}
				// need to active this stream
				if( (this_stream->stopped() || this_stream->failed()) && !this_stream->perfect_ending() )
				{
					DEBUG << "ffplay : downloading_manager_idle : restart the next playing stream ,reason: stopped=" << this_stream->stopped() << " , failed=" << this_stream->failed() << " , perfect_ending=" << this_stream->perfect_ending()  << " , No." << i;
					restart_stream(this_clips);
					break;
				}
			}
			else
			{	
				if( !this_stream->has_started() )
				{
					// not started is ok
				}
				else if( this_stream->perfect_ending() )
				{
				}
				else
				{
					if( this_stream->is_ready_to_play() && !this_stream->stopped() && !this_stream->failed() )
					{
						DEBUG << "ffplay : downloading_manager_idle : delete a non-prority stream No." << i;
						delete_stream(this_clips);
					}
				}
			}
		}
	}
}

void ffplay_idle()
{
	static int frame_count = 0;
	frame_count++;
	if( frame_count%50 != 0 )
		return;

	if( this_episode )
	{
		this_episode->remember_duration();
	}

	downloading_manager_idle();

	// cancel in this version, to be fixed.
	// function replaced by downloading manager
}