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

//#include "cmdutils.h"

//#include <unistd.h>
#include <assert.h>

#include <SDL.h>
#include <SDL_surface.h>

#include "packetqueue.h"
#include "ffplay.h"
#include "engine.h"
#include "screen.h"
#include "speaker.h"
#include "stream.h"
#include "libffmpeg.h"
#include "log.h"
#include "utility.hpp"

#include <boost/thread.hpp>
#include <Windows.h>



#define MAX_QUEUE_SIZE (100 * 1024 * 1024)
#define MIN_FRAMES (999999)


static int workaround_bugs = 1;
static int fast = 0;
static int lowres = 0;
static int idct = FF_IDCT_AUTO;
static enum AVDiscard skip_frame       = AVDISCARD_DEFAULT;
static enum AVDiscard skip_idct        = AVDISCARD_DEFAULT;
static enum AVDiscard skip_loop_filter = AVDISCARD_DEFAULT;
static int error_concealment = 3;
static int show_status = 1;
static int audio_disable;
static int video_disable;
static int wanted_stream[AVMEDIA_TYPE_NB] =
{
    -1,
    -1,
    -1,
	-1,
	-1
};
static int genpts = 0;
static int seek_by_bytes = -1;
static int64_t start_time = AV_NOPTS_VALUE;

/////////////////

struct StreamState
{
    AVFormatContext *ic;

    AVInputFormat *iformat;

	bool abort_request;

	double stream_duration;

	// init before start, safe to read
	std::string filename;

    int stream_index[AVMEDIA_TYPE_NB];

	
	///
    bool seek_request;
	bool seek_need_interrupt;
    int seek_flags;
    int64_t seek_position;
    int64_t seek_relative;
	double seek_pos;
	bool had_ever_seeked;
	///

	/////
	int video_stream_index;
	PacketQueue * video_queue;
	/////

	/////
	int audio_stream_index;
	PacketQueue * audio_queue;
	/////

	/////
	int subtitle_stream_index;
	PacketQueue * subtitle_queue;
	/////

	
    SDL_Thread *read_tid;
	bool has_eof;
	bool has_failed;
	bool has_started;
	bool is_ready_to_play;
	double stream_buffer_pos;
};



static bool local_block_waiting_ready_to_play(StreamState *is)
{
	int try_count = 0;
	if( !block_wait([&](){ return is->is_ready_to_play; },
					[&](){ return is->abort_request||is->has_failed||try_count>=500; },
					[&](){ try_count++;SDL_Delay(20); }) )
	{
		if( try_count >= 500 )
		{
			DEBUG << "Stream : block_waiting_ready_to_play timeout";
		}
		return false;
	}
	return true;
}


static int decode_interrupt_cb(void *ctx)
{
    StreamState *is = (StreamState*)ctx;
	if( is->seek_need_interrupt )
		DEBUG << "Stream : decode_interrupt cause by is->seek_need_interrupt";
    return is->abort_request || is->seek_need_interrupt;
}


/* seek in the stream */
static bool stream_seek(StreamState *is, int64_t pos, int64_t rel, int seek_by_bytes)
{
	int try_count = 0;
	if( !local_block_waiting_ready_to_play(is) )
	{
		DEBUG << "Stream : seek failed , stream not ready for long #1";
		is->has_failed = true;
		return false;
	}

    if( !is->seek_request )
	{
        int64_t ts;
        ts = pos * AV_TIME_BASE;
        if (is->ic->start_time != AV_NOPTS_VALUE)
            ts += is->ic->start_time;
		
		is->seek_pos = pos;
        is->seek_position = ts;
        is->seek_relative = rel;
        is->seek_flags &= ~AVSEEK_FLAG_BYTE;
        if (seek_by_bytes)
            is->seek_flags |= AVSEEK_FLAG_BYTE;
		is->seek_need_interrupt = true;
        is->seek_request = true;

		if( !block_wait([&](){ return is->seek_request==false; },
						[&](){ return is->abort_request||is->has_failed||try_count>=500; },
						[&](){ try_count++; SDL_Delay(20); }) )
		{
			if( try_count >= 500 )
			{
				DEBUG << "Stream : seek failed timeout";
				is->has_failed = true;
			}
			return false;
		}
    }
	return true;
}

/* open a given stream. Return 0 if OK */
static int stream_component_open(StreamState *is, int stream_index)
{
    AVFormatContext *ic = is->ic;
    AVCodecContext *avctx = NULL;
    AVCodec *codec = NULL;
    AVDictionary *opts = NULL;

    if (stream_index < 0 || stream_index >= ic->nb_streams)
        return 0;

    avctx = ic->streams[stream_index]->codec;

    codec = avcodec_find_decoder(avctx->codec_id);
    //opts = filter_codec_opts(codec_opts, codec, ic, ic->streams[stream_index]);

    if (!codec)
        return -1;

    avctx->workaround_bugs   = workaround_bugs;
    avctx->lowres            = lowres;
    if(avctx->lowres > codec->max_lowres)
	{
        av_log(avctx, AV_LOG_WARNING, "The maximum value for lowres supported by the decoder is %d\n",
                codec->max_lowres);
        avctx->lowres= codec->max_lowres;
    }

    avctx->idct_algo         = idct;
    avctx->skip_frame        = skip_frame;
    avctx->skip_idct         = skip_idct;
    avctx->skip_loop_filter  = skip_loop_filter;
    avctx->error_concealment = error_concealment;

    if(avctx->lowres)
		avctx->flags |= CODEC_FLAG_EMU_EDGE;
    if(fast)
		avctx->flags2 |= CODEC_FLAG2_FAST;
    if(codec->capabilities & CODEC_CAP_DR1)
        avctx->flags |= CODEC_FLAG_EMU_EDGE;

    if (!av_dict_get(opts, "threads", NULL, 0))
        av_dict_set(&opts, "threads", "auto", 0);

	ffmpeg_lock();
    if (!codec || avcodec_open2(avctx, codec, &opts) < 0)
	{
		ffmpeg_unlock();
        return -1;
	}
	ffmpeg_unlock();

    if (auto t = av_dict_get(opts, "", NULL, AV_DICT_IGNORE_SUFFIX))
	{
        av_log(NULL, AV_LOG_ERROR, "Option %s not found.\n", t->key);
        return AVERROR_OPTION_NOT_FOUND;
    }

    ic->streams[stream_index]->discard = AVDISCARD_DEFAULT;
    return 0;
}


static void stream_component_close(StreamState *is, int stream_index)
{
    AVFormatContext *ic = is->ic;
    AVCodecContext *avctx;

    if (stream_index < 0 || stream_index >= ic->nb_streams)
        return;

    avctx = ic->streams[stream_index]->codec;

    switch (avctx->codec_type)
	{
    case AVMEDIA_TYPE_AUDIO:
        break;
    case AVMEDIA_TYPE_VIDEO:
        break;
    case AVMEDIA_TYPE_SUBTITLE:
        break;
    default:
        break;
    }

    ic->streams[stream_index]->discard = AVDISCARD_ALL;

	ffmpeg_lock();
    avcodec_close(avctx);
	ffmpeg_unlock();

    switch (avctx->codec_type)
	{
    case AVMEDIA_TYPE_AUDIO:
        is->audio_stream_index = -1;
        break;
    case AVMEDIA_TYPE_VIDEO:
        is->video_stream_index = -1;
        break;
    case AVMEDIA_TYPE_SUBTITLE:
        is->subtitle_stream_index = -1;
        break;
    default:
        break;
    }
}

static void read_trigger_eof(StreamState * is)
{
	if( is->has_eof == false )
	{
		if( is->audio_stream_index >= 0 )
		{
			is->audio_queue->insert( PacketQueue::get_eof_packet() );
		}
		if( is->video_stream_index >= 0 )
		{
			is->video_queue->insert( PacketQueue::get_eof_packet() );
		}
		if( is->subtitle_stream_index >= 0 )
		{
			is->subtitle_queue->insert( PacketQueue::get_eof_packet() );
		}

		// correct the duration of stream
		{
			double real_duration = is->stream_buffer_pos;
			if( is->stream_duration != real_duration )
			{
				INFO << "Stream : correct duration from " << is->stream_duration << " to " << real_duration << " : " << is->filename;
				is->stream_duration = real_duration;
			}
		}
		//

		engine_clips_buffered_up_from_stream(is->filename);
		is->has_eof = true;
		INFO << "Stream : has end of file , " << is->filename;
	}

}

/* this thread gets the stream from the disk or the network */
static int read_thread(void *arg)
{
    StreamState *is = (StreamState*)arg;
    AVFormatContext *ic = NULL;
    int err, i, ret;
    AVPacket pkt1, *pkt = &pkt1;
    int pkt_in_play_range = 0;
    AVDictionaryEntry *t = NULL;
    AVDictionary *format_opts = NULL;
    int orig_nb_streams;

	is->has_started = true;

	is->video_stream_index = -1;
	is->audio_stream_index = -1;
	is->subtitle_stream_index = -1;

    ic = avformat_alloc_context();
    ic->interrupt_callback.callback = decode_interrupt_cb;
    ic->interrupt_callback.opaque = is;

    err = avformat_open_input(&ic, is->filename.c_str(), is->iformat, &format_opts);
	is->ic = ic;
    if (err < 0)
	{
        ERROR << "Stream : avformat_open_input() , url=" << is->filename.c_str() << " , error=" <<err;
        ret = -1;
        goto fail;
    }

    if ((t = av_dict_get(format_opts, "", NULL, AV_DICT_IGNORE_SUFFIX)))
	{
        av_log(NULL, AV_LOG_ERROR, "Option %s not found.\n", t->key);
        ret = -1;
        goto fail;
    }

    if (genpts)
        ic->flags |= AVFMT_FLAG_GENPTS;

    //opts = setup_find_stream_info_opts(ic, codec_opts);

    orig_nb_streams = ic->nb_streams;

    err = avformat_find_stream_info(ic, NULL);
    if (err < 0)
	{
        ERROR << "Stream : could not find codec parameters , url=" << is->filename;
        ret = -1;
        goto fail;
    }

	/*
    for (i = 0; i < orig_nb_streams; i++)
        av_dict_free(&opts[i]);
    av_freep(&opts);
	*/

    if (ic->pb)
        ic->pb->eof_reached = 0; // FIXME hack, ffplay maybe should not use url_feof() to test for the end

    if (seek_by_bytes < 0)
        seek_by_bytes = !!(ic->iformat->flags & AVFMT_TS_DISCONT);

    /* if seeking requested, we execute it */
    if (start_time != AV_NOPTS_VALUE)
	{
        int64_t timestamp;

        timestamp = start_time;
        /* add the stream start time */
        if (ic->start_time != AV_NOPTS_VALUE)
            timestamp += ic->start_time;
        ret = avformat_seek_file(ic, -1, INT64_MIN, timestamp, INT64_MAX, 0);
        if (ret < 0)
		{
            ERROR << "Stream : could not seek to position , url=" << is->filename << " , " << (double)timestamp / AV_TIME_BASE;
			ret = -1;
			goto fail;
        }
    }

    for (i = 0; i < ic->nb_streams; i++)
        ic->streams[i]->discard = AVDISCARD_ALL;

    memset(is->stream_index, -1, sizeof(is->stream_index));

    if (!video_disable)
        is->video_stream_index = is->stream_index[AVMEDIA_TYPE_VIDEO] =
            av_find_best_stream(ic, AVMEDIA_TYPE_VIDEO,
                                wanted_stream[AVMEDIA_TYPE_VIDEO], -1, NULL, 0);

    if (!audio_disable)
        is->audio_stream_index = is->stream_index[AVMEDIA_TYPE_AUDIO] =
            av_find_best_stream(ic, AVMEDIA_TYPE_AUDIO,
                                wanted_stream[AVMEDIA_TYPE_AUDIO],
                                is->stream_index[AVMEDIA_TYPE_VIDEO],
                                NULL, 0);

    if (!video_disable)
        is->subtitle_stream_index = is->stream_index[AVMEDIA_TYPE_SUBTITLE] =
            av_find_best_stream(ic, AVMEDIA_TYPE_SUBTITLE,
                                wanted_stream[AVMEDIA_TYPE_SUBTITLE],
                                (is->stream_index[AVMEDIA_TYPE_AUDIO] >= 0 ?
                                 is->stream_index[AVMEDIA_TYPE_AUDIO] :
                                 is->stream_index[AVMEDIA_TYPE_VIDEO]),
                                NULL, 0);

	if( stream_component_open(is,is->video_stream_index) < 0 )
	{
		ret = -1;
		goto fail;
	}
	if( stream_component_open(is,is->audio_stream_index) < 0 )
	{
		ret = -1;
		goto fail;
	}
	if( stream_component_open(is,is->subtitle_stream_index) < 0 )
	{
		ret = -1;
		goto fail;
	}

    if (show_status)
	{
        //av_dump_format(ic, 0, is->filename.c_str(), 0);
    }

	// obtain stream duration!
	{
	    int hours, mins, secs, us;
        secs = ic->duration / AV_TIME_BASE;
        us = ic->duration % AV_TIME_BASE;
        mins = secs / 60;
        secs %= 60;
        hours = mins / 60;
        mins %= 60;

		is->stream_duration = (double)ic->duration / AV_TIME_BASE;
	}
	//

	{
		is->is_ready_to_play = true;
	}

    for (;;)
	{
        if (is->abort_request)
            break;

		/*
        if (is->paused != is->last_paused)
		{
            is->last_paused = is->paused;
            if (is->paused)
                is->read_pause_return = av_read_pause(ic);
            else
                av_read_play(ic);
        }
		*/
#if CONFIG_RTSP_DEMUXER || CONFIG_MMSH_PROTOCOL
        if (is->paused &&
                (!strcmp(ic->iformat->name, "rtsp") ||
                 (ic->pb && !strncmp(input_filename, "mmsh:", 5)))) {
            /* wait 10 ms to avoid trying to get another packet */
            /* XXX: horrible */
            SDL_Delay(10);
            continue;
        }
#endif
        /* if the queue are full, no need to read more */
		/*
        if (   is->audio_queue->size() + is->video_queue->size() + is->subtitle_queue->size() > MAX_QUEUE_SIZE
            || (   (is->audioq   .nb_packets > MIN_FRAMES || is->audio_stream_index < 0)
                && (is->videoq   .nb_packets > MIN_FRAMES || is->video_stream_index < 0)
                && (is->subtitleq.nb_packets > MIN_FRAMES || is->subtitle_stream_index < 0)) )
		{
            SDL_Delay(10);
            continue;
        }
		*/

		if (is->has_eof)
		{
            SDL_Delay(30);
            continue;
        }

        ret = av_read_frame(ic, pkt);
        if (ret < 0)
		{
			if( url_feof(ic->pb) )
				read_trigger_eof(is);
            else if (ret == AVERROR_EOF )
				read_trigger_eof(is);
            if (ic->pb && ic->pb->error)
			{
                ret = -1;
				goto fail;
			}
            SDL_Delay(100); /* wait for user event */
            continue;
        }

		// before reading the first packet of file
		if( is->seek_request )
		{
			is->seek_need_interrupt = false;

            int64_t seek_target = is->seek_position;
            int64_t seek_min    = is->seek_relative > 0 ? seek_target - is->seek_relative + 2: INT64_MIN;
            int64_t seek_max    = is->seek_relative < 0 ? seek_target - is->seek_relative - 2: INT64_MAX;
// FIXME the +-2 is due to rounding being not done in the correct direction in generation
//      of the seek_position/seek_relative variables
			is->audio_queue->clear();
			is->video_queue->clear();
			is->subtitle_queue->clear();

			{
				//ret = avformat_seek_file(is->ic, -1, seek_min, seek_target, seek_max, is->seek_flags);
				ret = av_seek_frame(is->ic,-1,seek_target,is->seek_flags);
				if (ret < 0)
				{
					ERROR << "Stream : av_seek_frame : error while seeking , ret=" << ret;
					goto fail;
				}

				DEBUG << "Stream : seeked to : " << (double)seek_target/AV_TIME_BASE << " , ret=" << ret; 
			}

			is->stream_buffer_pos = seek_target/AV_TIME_BASE;
			//

			is->has_eof = false;
			is->had_ever_seeked = true;
            is->seek_request = false;
			continue;
        }


		//printf("read %d\n",is->video_queue->size());
        /* check if packet is in play range specified by user, then queue, otherwise discard */
		/*
        pkt_in_play_range = duration == AV_NOPTS_VALUE ||
                (pkt->pts - ic->streams[pkt->stream_index]->start_time) *
                av_q2d(ic->streams[pkt->stream_index]->time_base) -
                (double)(start_time != AV_NOPTS_VALUE ? start_time : 0) / 1000000
                <= ((double)duration / 1000000);
		*/

        if (pkt->stream_index == is->audio_stream_index)
		{
			is->audio_queue->insert(*pkt);
        }
		else if (pkt->stream_index == is->video_stream_index)
		{
			bool ret = 0;
            ret = is->video_queue->insert(*pkt);
			//////////////
			if( ret == true )
			{
				is->stream_buffer_pos = max(is->stream_buffer_pos , (pkt->pts+pkt->duration) * av_q2d(ic->streams[pkt->stream_index]->time_base) );
			}
			else
			{
				
			}
			//////////////
        }
		else if (pkt->stream_index == is->subtitle_stream_index)
		{
            is->subtitle_queue->insert(*pkt);
        }
		else
		{
            av_free_packet(pkt);
        }
    }

    ret = 0;

fail:
	if( ret < 0 )
	{
		DEBUG << "Stream : has failed , " << is->filename;
		is->has_failed = true;
	}

	block_wait([&](){ return is->abort_request; },
			   [&](){ return is->abort_request; },
			   [&](){ SDL_Delay(50); });

	stream_component_close(is,is->video_stream_index);
	stream_component_close(is,is->audio_stream_index);
	stream_component_close(is,is->subtitle_stream_index);
    if (is->ic)
	{
        avformat_close_input(&is->ic);
    }

    return 0;
}

static bool stream_open(StreamState * is, std::string filename, AVInputFormat *iformat)
{
    is->filename = filename;
    is->iformat = iformat;

    is->read_tid = SDL_CreateThread(read_thread, "Read" , is);
    if (!is->read_tid)
	{
        return false;
    }
    return true;
}
//////////////

void stream_init()
{
}

Stream::Stream()
{
	is = new StreamState();
	memset(is,0,sizeof(*is));

	is->audio_queue = new PacketQueue();
	is->video_queue = new PacketQueue();
	is->subtitle_queue = new PacketQueue();
}

Stream::~Stream()
{
	delete is->audio_queue;
	delete is->video_queue;
	delete is->subtitle_queue;

	delete is;
}


bool Stream::start(std::string const & url)
{
	if( !stopped() || failed() )
		return false;

	INFO << "Stream start : " << url;

	is->stream_buffer_pos = -1;
	is->stream_duration = -1;

	is->seek_request = false;
	is->seek_pos = 0;
	is->had_ever_seeked = false;
	is->seek_need_interrupt = false;
	is->has_started = false;

	if( !stream_open(is, url, NULL) )
	{
		ERROR << "Stream : start : Failed to initialize VideoState";
		return false;
	}
	return true;
}

bool Stream::stopped() const
{
	return is->read_tid==NULL;
}

bool Stream::failed() const
{
	return is->has_failed;
}

bool Stream::has_started() const
{
	return is->has_started;
}

bool Stream::perfect_ending() const
{
	return !is->had_ever_seeked && has_end_of_file();
}

void Stream::stop()
{
	if( stopped() )
		return;

	INFO << "Stream stop : " << is->filename;

	// this is thread-safe, all synchronized.
    /* XXX: use a special url_shutdown call to abort parse cleanly */
	// It is safe because this variable is only readable to other working threads.
	{
		is->abort_request = true;
	}

	SDL_WaitThread(is->read_tid, NULL);
	
	is->read_tid = NULL;
}

bool Stream::seek(double pos)
{
	if( stopped() || failed() )
		return false;
	return stream_seek(is,pos,0,0);
}

bool Stream::block_waiting_ready_to_play()
{
	if( stopped() || failed() )
		return false;
	return local_block_waiting_ready_to_play(is);
}


double Stream::stream_buffer_pos() const
{
	return is->stream_buffer_pos;
}

double Stream::stream_duration() const
{
	return is->stream_duration;
}

std::string Stream::get_filename()
{
	return is->filename;
}

int Stream::get_stream_index(std::string const & type)
{
	if( type == "video" )
		return is->stream_index[AVMEDIA_TYPE_VIDEO];
	else if( type == "audio" )
		return is->stream_index[AVMEDIA_TYPE_AUDIO];
	else if( type == "subtitle" )
		return is->stream_index[AVMEDIA_TYPE_SUBTITLE];
	return -1;
}

AVFormatContext * Stream::get_format_context()
{
	return is->ic;
}

bool Stream::is_ready_to_play() const
{
	return is->is_ready_to_play;
}

double Stream::last_seek_pos() const
{
	return is->seek_pos;
}

bool Stream::has_end_of_file() const
{
	return is->has_eof;
}

PacketQueue * Stream::get_packet_queue(std::string const & type)
{
	if( type == "video" )
		return is->video_queue;
	else if( type == "audio" )
		return is->audio_queue;
	else if( type == "subtitle" )
		return is->subtitle_queue;
	return NULL;
}

bool Stream::query_buffer(double & corrected_pos)
{
	double pos = corrected_pos;
	if( !is_ready_to_play() )
		return false;
	int video_stream_index = get_stream_index("video");
	if( video_stream_index == -1 )
		return false;

	int64_t pts = pos / av_q2d(get_format_context()->streams[video_stream_index]->time_base);

	//DEBUG << "Stream::query_buffer : " << pos << " to " << (double)pts;

	auto video_queue = get_packet_queue("video");
	if( !video_queue )
		return false;

	int64_t next_48_dts = -1;
	AVPacket packet;
	bool ret;
	while(true)
	{
		ret = video_queue->upper_bound(pts,packet,48,next_48_dts);
		if( !ret )
		{
			DEBUG << "Stream::query_buffer : no any packets";
			return false;
		}
		if( packet.flags )
			break;
		pts = packet.dts;
	};

	if( packet.duration == 0 )
		DEBUG << "Stream::query_buffer : packet.duration == 0 , estimate in corrected_pos";

	corrected_pos = (pts - packet.duration/2) * av_q2d(get_format_context()->streams[video_stream_index]->time_base);
	DEBUG << "Stream::query_buffer : correct pos from " << pos << " to " << corrected_pos;

	if( next_48_dts == -1 )
	{
		if( has_end_of_file() )
		{
			DEBUG << "Stream::query_buffer : has something, and to the end";
			return true;
		}
		else
		{
			DEBUG << "Stream::query_buffer : has something, but not enough";
			return false;
		}
	}
	DEBUG << "Stream::query_buffer : alright , buffered " << (next_48_dts - packet.dts)*av_q2d(get_format_context()->streams[video_stream_index]->time_base);
	return true;
}

//////////////