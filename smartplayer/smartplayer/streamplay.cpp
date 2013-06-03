#include "config.h"
#include <inttypes.h>

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


#include <SDL.h>
#include <SDL_surface.h>

#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include <Windows.h>

#include "streamplay.h"
#include "stream.h"
#include "speaker.h"
#include "screen.h"
#include "packetqueue.h"
#include "engine.h"
#include "utility.hpp"
#include "libffmpeg.h"
#include "log.h"

#define USING_NING_FAST_RENDER

//////

/* no AV sync correction is done if below the AV sync threshold */
#define AV_SYNC_THRESHOLD 0.01
/* no AV correction is done if too big error */
#define AV_NOSYNC_THRESHOLD 10.0

/* maximum audio speed change to get correct sync */
#define SAMPLE_CORRECTION_PERCENT_MAX 10

/* we use about AUDIO_DIFF_AVG_NB A-V differences to make the average */
#define AUDIO_DIFF_AVG_NB   20

/* NOTE: the size must be big enough to compensate the hardware audio buffersize size */
//#define SAMPLE_ARRAY_SIZE (2 * 65536)

#define VIDEO_PICTURE_QUEUE_SIZE 12
#define SUBPICTURE_QUEUE_SIZE 4

typedef struct VideoPicture
{
    double pts;                                  ///< presentation time stamp for this picture
    double duration;                             ///< expected duration of the frame
    int64_t pos;                                 ///< byte position in file
    int skip;
    SDL_Texture *bmp;
    int width, height; /* source height & width */
    int allocated;
    int reallocate;
    enum PixelFormat pix_fmt;

#ifdef CONFIG_AVFILTER
    AVFilterBufferRef *picref;
#endif
} VideoPicture;

struct SubPicture
{
    double pts; /* presentation time stamp for this picture */
    AVSubtitle sub;
};

enum
{
    AV_SYNC_AUDIO_MASTER, /* default choice */
    AV_SYNC_VIDEO_MASTER,
    AV_SYNC_EXTERNAL_CLOCK, /* synchronize to an external clock */
};

enum ShowMode
{
    SHOW_MODE_NONE = -1,
	SHOW_MODE_VIDEO = 0,
	SHOW_MODE_WAVES,
	SHOW_MODE_RDFT,
	SHOW_MODE_NB
};

struct VideoState
{
	struct PlayContext
	{
		AVCodecContext * codec;
		AVRational time_base;
	};

    boost::thread * video_thread;
    boost::thread * refresh_thread;

	Stream * stream_;

	//
    int abort_request;
	//

	//
    int paused;
	//

	//
	ShowMode show_mode;
	//

	//
    int av_sync_type;
    double external_clock; /* external clock base */
    int64_t external_clock_time;
	//

	////// audio
    int audio_stream_index;
	int64_t audio_packetqueue_last_extracted_dts;
    double audio_clock;
    double audio_diff_cum; /* used for AV difference average computation */
    double audio_diff_avg_coef;
    double audio_diff_threshold;
    int audio_diff_avg_count;
    PlayContext * audio_stream;
    int audio_hw_buf_size;
    DECLARE_ALIGNED(16,uint8_t,audio_buf2)[AVCODEC_MAX_AUDIO_FRAME_SIZE * 4];
    uint8_t silence_buf[SPEAKER_AUDIO_BUFFER_SIZE];
    uint8_t *audio_buf;
    unsigned int audio_buf_size; /* in bytes */
    int audio_buf_index; /* in bytes */
    int audio_write_buf_size;
    AVPacket audio_pkt_temp;
    AVPacket audio_pkt;
    enum AVSampleFormat audio_src_fmt;
    enum AVSampleFormat audio_tgt_fmt;
    int audio_src_channels;
    int audio_tgt_channels;
    int64_t audio_src_channel_layout;
    int64_t audio_tgt_channel_layout;
    int audio_src_freq;
    int audio_tgt_freq;
    struct SwrContext *swr_ctx;
    double audio_current_pts;
    double audio_current_pts_drift;
    int frame_drops_early;
    int frame_drops_late;
    AVFrame * audio_frame;
	bool audio_waiting_for_packets;
	//////

    //int16_t sample_array[SAMPLE_ARRAY_SIZE];
    //int sample_array_index;
    //int last_i_start;
    //RDFTContext *rdft;
    //int rdft_bits;
    //FFTSample *rdft_data;
    //int xpos;

	////// subtitle
    SDL_Thread *subtitle_tid;
	int64_t subtitle_packetqueue_last_extracted_dts;
    int subtitle_stream_index;
    int subtitle_stream_changed;
    PlayContext * subtitle_stream;
    SubPicture subpq[SUBPICTURE_QUEUE_SIZE];
    int subpq_size, subpq_rindex, subpq_windex;
    SDL_mutex *subpq_mutex;
    SDL_cond *subpq_cond;
	//////

	////// video frame
    double frame_timer;
    double frame_last_pts;
    double frame_last_duration;
    double frame_last_dropped_pts;
    double frame_last_returned_time;
    double frame_last_filter_delay;
    int64_t frame_last_dropped_pos;
    double video_clock;                          ///< pts of last decoded frame / predicted pts of next decoded frame
	double video_current_pts;                    ///< current displayed pts (different from video_clock if frame fifos are used)
    double video_current_pts_drift;              ///< video_current_pts - time (av_gettime) at which we updated video_current_pts - used to have running video pts
    int64_t video_current_pos;                   ///< current displayed file pos
    VideoPicture pictq[VIDEO_PICTURE_QUEUE_SIZE];
    int pictq_size, pictq_rindex, pictq_windex;
    SDL_mutex *pictq_mutex;
    SDL_cond *pictq_cond;
	//////

	////// video stream
    int video_stream_index;
	int64_t video_packetqueue_last_extracted_dts;
    PlayContext * video_stream;
	//////

#ifndef CONFIG_AVFILTER
    struct SwsContext *img_convert_ctx;
#endif

#ifdef CONFIG_AVFILTER
    AVFilterContext *out_video_filter;          ///< the last filter in the video chain
#endif

    int refresh;

	// flush
	bool audio_flush_request;
	bool video_flush_request;
	bool subtitle_flush_request;
	//

	//
	// exports video playing data
	bool has_stopped;
	bool has_played;
	double start_playing_pos;
	double stream_current_playing_pos;
	boost::asio::io_service * main_event_loop;
	boost::thread * main_thread;
	//
};



/* options specified by the user */
static int show_status = 1;
static AVInputFormat *file_iformat = 0;
static const char *input_filename = 0;
static const char *window_title = 0;
static int display_disable;
static int av_sync_type = /*AV_SYNC_VIDEO_MASTER*/AV_SYNC_AUDIO_MASTER;
static int64_t duration = AV_NOPTS_VALUE;
static int decoder_reorder_pts = -1;
static int loop = 1;
static int framedrop = 1/*-1*/;
static enum ShowMode show_mode = SHOW_MODE_NONE;
static int rdftspeed = 20;
#ifdef CONFIG_AVFILTER
static char *vfilters = NULL;
#endif

/* current context */


static inline int compute_mod(int a, int b)
{
    return a < 0 ? a%b + b : a%b;
}

static inline void fill_rectangle(SDL_Surface *screen,int x, int y, int w, int h, int color)
{
    SDL_Rect rect;
    rect.x = x;
    rect.y = y;
    rect.w = w;
    rect.h = h;
    SDL_FillRect(screen, &rect, color);
}

static void free_subpicture(SubPicture *sp)
{
    avsubtitle_free(&sp->sub);
}

static int subtitle_thread(void *arg)
{
    VideoState *is = (VideoState*)arg;
    SubPicture *sp;
    AVPacket pkt1, *pkt = &pkt1;
    int got_subtitle;
    double pts;
    int i, j;
    int r, g, b, y, u, v, a;

    for (;;)
	{
        while (is->paused && !is->abort_request)
		{
            SDL_Delay(10);
        }

		if ( is->subtitle_flush_request )
		{
            avcodec_flush_buffers(is->subtitle_stream->codec);
			is->subtitle_flush_request = false;
        }

        //if (packet_queue_get(&is->subtitleq, pkt, 1) < 0)
        //    break;
		PacketQueue * subtitle_queue = is->stream_->get_packet_queue("subtitle");
		if( !subtitle_queue )
			break;

		int64_t next_k_dts;
		if( !block_wait(
			[&]()->bool{ return subtitle_queue->upper_bound(is->subtitle_packetqueue_last_extracted_dts,*pkt,-1,next_k_dts); },
			[&]()->bool{ return is->abort_request; },
			[](){ SDL_Delay(50); } ) )
			break;

		is->subtitle_packetqueue_last_extracted_dts = pkt->dts;

        SDL_LockMutex(is->subpq_mutex);
        while (is->subpq_size >= SUBPICTURE_QUEUE_SIZE &&
               !is->abort_request)
		{
            SDL_CondWait(is->subpq_cond, is->subpq_mutex);
        }
        SDL_UnlockMutex(is->subpq_mutex);

        if (is->abort_request)
            return 0;

        sp = &is->subpq[is->subpq_windex];

       /* NOTE: ipts is the PTS of the _first_ picture beginning in
           this packet, if any */
        pts = 0;
        if (pkt->pts != AV_NOPTS_VALUE)
            pts = av_q2d(is->subtitle_stream->time_base) * pkt->pts;

        avcodec_decode_subtitle2(is->subtitle_stream->codec, &sp->sub,
                                 &got_subtitle, pkt);

        if (got_subtitle && sp->sub.format == 0) {
            sp->pts = pts;

            for (i = 0; i < sp->sub.num_rects; i++)
            {
                for (j = 0; j < sp->sub.rects[i]->nb_colors; j++)
                {
					// don't do that, try RGB
					/*
                    RGBA_IN(r, g, b, a, (uint32_t*)sp->sub.rects[i]->pict.data[1] + j);
                    y = RGB_TO_Y_CCIR(r, g, b);
                    u = RGB_TO_U_CCIR(r, g, b, 0);
                    v = RGB_TO_V_CCIR(r, g, b, 0);
                    YUVA_OUT((uint32_t*)sp->sub.rects[i]->pict.data[1] + j, y, u, v, a);
					*/
                }
            }

            /* now we can update the picture count */
            if (++is->subpq_windex == SUBPICTURE_QUEUE_SIZE)
                is->subpq_windex = 0;
            SDL_LockMutex(is->subpq_mutex);
            is->subpq_size++;
            SDL_UnlockMutex(is->subpq_mutex);
        }
        //av_free_packet(pkt);
    }
    return 0;
}



static void video_image_display(VideoState *is)
{
    VideoPicture *vp;
    SubPicture *sp;
    AVPicture pict;
    float aspect_ratio;
    int width, height, x, y;
    SDL_Rect rect;
    int i;

    vp = &is->pictq[is->pictq_rindex];
#ifdef USING_NING_FAST_RENDER
    if(vp->picref)
#else
	if(vp->bmp)
#endif
	{
#ifdef CONFIG_AVFILTER
         if (vp->picref->video->sample_aspect_ratio.num == 0)
             aspect_ratio = 0;
         else
             aspect_ratio = av_q2d(vp->picref->video->sample_aspect_ratio);
#else

        /* XXX: use variable in the frame */
        if (is->video_stream->sample_aspect_ratio.num)
            aspect_ratio = av_q2d(is->video_stream->sample_aspect_ratio);
        else if (is->video_stream->codec->sample_aspect_ratio.num)
            aspect_ratio = av_q2d(is->video_stream->codec->sample_aspect_ratio);
        else
            aspect_ratio = 0;
#endif
        if (aspect_ratio <= 0.0)
            aspect_ratio = 1.0;
        aspect_ratio *= (float)vp->width / (float)vp->height;

		// no subtitle now!
		/*
        if (is->subtitle_stream)
		{
            if (is->subpq_size > 0) {
                sp = &is->subpq[is->subpq_rindex];

                if (vp->pts >= sp->pts + ((float) sp->sub.start_display_time / 1000))
				{
					void * pixels[3];
					int pitches[3];
					if( SDL_LockTexture (vp->bmp,NULL,pixels,pitches) == 0 )
					{
						pict.data[0] = (uint8_t*)pixels[0];
						pict.data[1] = (uint8_t*)pixels[2];
						pict.data[2] = (uint8_t*)pixels[1];

						pict.linesize[0] = pitches[0];
						pict.linesize[1] = pitches[2];
						pict.linesize[2] = pitches[1];

						// please use SDL, see original ffplay.c
						for (i = 0; i < sp->sub.num_rects; i++)
							blend_subrect(&pict, sp->sub.rects[i], vp->bmp->w, vp->bmp->h);


						SDL_UnlockTexture (vp->bmp);
					}
					else
					{
						printf("SDL_LockTexture(): %s\n",SDL_GetError());
					}
                }
            }
        }
		*/

        /* XXX: we suppose the screen has a 1.0 pixel ratio */
		/*
        height = is->height;
        width = ((int)rint(height * aspect_ratio)) & ~1;
        if (width > is->width)
		{
            width = is->width;
            height = ((int)rint(width / aspect_ratio)) & ~1;
        }
        x = (is->width - width) / 2;
        y = (is->height - height) / 2;
        rect.x = is->xleft + x;
        rect.y = is->ytop  + y;
        rect.w = FFMAX(width,  1);
        rect.h = FFMAX(height, 1);
		*/

		int time_stamp = ::SDL_GetTicks();
#ifdef USING_NING_FAST_RENDER
		{
			auto screen = screen_draw_begin();
			if( screen.get<0>() )
			{
				static int sws_flags = SWS_FAST_BILINEAR;
				static SwsContext * img_convert_ctx = 0;

				if( screen.get<2>() > 1 && screen.get<3>() > 1 )
				{
					img_convert_ctx = sws_getCachedContext(img_convert_ctx,
						vp->width, vp->height, vp->pix_fmt, screen.get<2>(), screen.get<3>(),
						PIX_FMT_BGR0, sws_flags, NULL, NULL, NULL);

					if (img_convert_ctx)
					{
						AVPicture pict;
						memset(pict.data,0,sizeof(pict.data));
						memset(pict.linesize,0,sizeof(pict.linesize));
						pict.data[0] = screen.get<0>();
						pict.linesize[0] = screen.get<1>();

						sws_scale(img_convert_ctx, vp->picref->data, vp->picref->linesize,
									0, vp->height, pict.data, pict.linesize);
					}
				}
				screen_draw_end();
			}
		}
#else
		SDL_RenderClear(renderer);
		SDL_RenderCopy(renderer,vp->bmp,NULL,&rect);
#endif

		//printf("dispalying costs : %u ms , queue:%d\n",::SDL_GetTicks()-time_stamp,is->pictq_size);
    }
}


/* display the current picture, if any */
static void video_display(VideoState *is)
{
	video_image_display(is);
}

/* get the current video clock value */
static double get_video_clock(VideoState *is)
{
    if (is->paused)
	{
        return is->video_current_pts;
    }
	else
	{
        return is->video_current_pts_drift + av_gettime() / 1000000.0;
    }
}


/* get the current audio clock value */
static double get_audio_clock(VideoState *is)
{
    if (is->paused) {
        return is->audio_current_pts;
    } else {
        return is->audio_current_pts_drift + av_gettime() / 1000000.0;
    }
}


/* get the current external clock value */
static double get_external_clock(VideoState *is)
{
    int64_t ti;
    ti = av_gettime();
    return is->external_clock + ((ti - is->external_clock_time) * 1e-6);
}

/* get the current master clock value */
static double get_master_clock(VideoState *is)
{
    double val;

    if (is->av_sync_type == AV_SYNC_VIDEO_MASTER) {
        if (is->video_stream)
            val = get_video_clock(is);
        else
            val = get_audio_clock(is);
    } else if (is->av_sync_type == AV_SYNC_AUDIO_MASTER) {
        if (is->audio_stream)
            val = get_audio_clock(is);
        else
            val = get_video_clock(is);
    } else {
        val = get_external_clock(is);
    }
    return val;
}

/* pause or resume the video */
static void stream_toggle_pause(VideoState *is)
{
    is->paused = !is->paused;
}

static double compute_target_delay(double delay, VideoState *is)
{
    double sync_threshold, diff;

    /* update delay to follow master synchronisation source */
    if (((is->av_sync_type == AV_SYNC_AUDIO_MASTER && is->audio_stream) ||
         is->av_sync_type == AV_SYNC_EXTERNAL_CLOCK))
	{
        /* if video is slave, we try to correct big delays by
           duplicating or deleting a frame */
        diff = get_video_clock(is) - get_master_clock(is);

        /* skip or repeat frame. We take into account the
           delay to compute the threshold. I still don't know
           if it is the best guess */
        sync_threshold = FFMAX(AV_SYNC_THRESHOLD, delay);
        if (fabs(diff) < AV_NOSYNC_THRESHOLD)
		{
            if (diff <= -sync_threshold)
                delay = 0;
            else if (diff >= sync_threshold)
                delay = 2 * delay;
        }
    }

	/*
    av_log(NULL,3, "video: delay=%0.3f A-V=%f\n",
            delay, -diff);
	*/
    return delay;
}

static void pictq_next_picture(VideoState *is)
{
    SDL_LockMutex(is->pictq_mutex);
    /* update queue size and signal for next picture */
    if (++is->pictq_rindex == VIDEO_PICTURE_QUEUE_SIZE)
        is->pictq_rindex = 0;
    is->pictq_size--;
    SDL_CondSignal(is->pictq_cond);
    SDL_UnlockMutex(is->pictq_mutex);
}

static void update_video_pts(VideoState *is, double pts, int64_t pos)
{
    double time = av_gettime() / 1000000.0;
    /* update current video pts */
    is->video_current_pts = pts;
    is->video_current_pts_drift = is->video_current_pts - time;
    is->video_current_pos = pos;
    is->frame_last_pts = pts;
}

/* called to display each frame */
static void video_refresh(VideoState *is)
{
    VideoPicture *vp;
    double time;

	static int almost_end_but_stuck_successive_count = 0;
    SubPicture *sp, *sp2;

    if(is->video_stream)
	{
retry:
        if (is->pictq_size == 0)
		{
            SDL_LockMutex(is->pictq_mutex);
            if (is->frame_last_dropped_pts != AV_NOPTS_VALUE && is->frame_last_dropped_pts > is->frame_last_pts)
			{
                update_video_pts(is, is->frame_last_dropped_pts, is->frame_last_dropped_pos);
                is->frame_last_dropped_pts = AV_NOPTS_VALUE;
            }
            SDL_UnlockMutex(is->pictq_mutex);

			if( !is->has_stopped )
			{
				if(	is->stream_current_playing_pos != -1 &&
					is->stream_->stream_duration() != -1 &&
					fabs(is->stream_current_playing_pos-is->stream_->stream_duration()) <= 5.0 &&
					(is->stream_->has_end_of_file() || almost_end_but_stuck_successive_count >= 50 ) )
				{
					is->has_stopped = true;
					DEBUG << "Streamplay : has end of file , skipped " << is->stream_->stream_duration()-is->stream_current_playing_pos;
					if( almost_end_but_stuck_successive_count >= 50 )
						DEBUG << "Streamplay : because of almost_end_but_stuck_successive_count";
					engine_clips_playing_stopped_from_streamplay();
				}
				else
				{
					almost_end_but_stuck_successive_count++;
				}
			}
			else
			{
				// nothing to play in buffer.
			}
            // nothing to do, no picture to display in the que
        }
		else
		{
			almost_end_but_stuck_successive_count = 0;

            double last_duration, duration, delay;
            /* dequeue the picture */
            vp = &is->pictq[is->pictq_rindex];

            if (vp->skip)
			{
                pictq_next_picture(is);
                goto retry;
            }

            if (is->paused)
                goto display;

            /* compute nominal last_duration */
            last_duration = vp->pts - is->frame_last_pts;
            if (last_duration > 0 && last_duration < 10.0)
			{
                /* if duration of the last frame was sane, update last_duration in video state */
                is->frame_last_duration = last_duration;
            }
            delay = compute_target_delay(is->frame_last_duration, is);

            time = av_gettime()/1000000.0;
            if (time < is->frame_timer + delay)
			{
                return;
			}

            if (delay > 0)
                is->frame_timer += delay * FFMAX(1, floor((time-is->frame_timer) / delay));

            SDL_LockMutex(is->pictq_mutex);
            update_video_pts(is, vp->pts, vp->pos);
            SDL_UnlockMutex(is->pictq_mutex);

			//
			{
				is->stream_current_playing_pos = std::max(is->stream_current_playing_pos,vp->pts);
			}
			//

            if (is->pictq_size > 1)
			{
                VideoPicture *nextvp = &is->pictq[(is->pictq_rindex + 1) % VIDEO_PICTURE_QUEUE_SIZE];
                duration = nextvp->pts - vp->pts; // More accurate this way, 1/time_base is often not reflecting FPS
            }
			else
			{
                duration = vp->duration;
            }

            if((framedrop>0 || (framedrop && is->audio_stream)) && time > is->frame_timer + duration)
			{
                if(is->pictq_size > 1)
				{
                    is->frame_drops_late++;
                    pictq_next_picture(is);
					DEBUG << "Streamplay : Drop late : " << is->frame_drops_late;
                    goto retry;
                }
            }

            if (is->subtitle_stream)
			{
                if (is->subtitle_stream_changed)
				{
                    SDL_LockMutex(is->subpq_mutex);

                    while (is->subpq_size) {
                        free_subpicture(&is->subpq[is->subpq_rindex]);

                        /* update queue size and signal for next picture */
                        if (++is->subpq_rindex == SUBPICTURE_QUEUE_SIZE)
                            is->subpq_rindex = 0;

                        is->subpq_size--;
                    }
                    is->subtitle_stream_changed = 0;

                    SDL_CondSignal(is->subpq_cond);
                    SDL_UnlockMutex(is->subpq_mutex);
                }
				else
				{
                    if (is->subpq_size > 0) {
                        sp = &is->subpq[is->subpq_rindex];

                        if (is->subpq_size > 1)
                            sp2 = &is->subpq[(is->subpq_rindex + 1) % SUBPICTURE_QUEUE_SIZE];
                        else
                            sp2 = NULL;

                        if ((is->video_current_pts > (sp->pts + ((float) sp->sub.end_display_time / 1000)))
                                || (sp2 && is->video_current_pts > (sp2->pts + ((float) sp2->sub.start_display_time / 1000))))
                        {
                            free_subpicture(sp);

                            /* update queue size and signal for next picture */
                            if (++is->subpq_rindex == SUBPICTURE_QUEUE_SIZE)
                                is->subpq_rindex = 0;

                            SDL_LockMutex(is->subpq_mutex);
                            is->subpq_size--;
                            SDL_CondSignal(is->subpq_cond);
                            SDL_UnlockMutex(is->subpq_mutex);
                        }
                    }
                }
            }

display:
            /* display picture */
            if (!display_disable)
                video_display(is);

            if (!is->paused)
                pictq_next_picture(is);
        }
    }
	else if (is->audio_stream)
	{
        /* draw the next audio frame */

        /* if only audio stream, then display the audio bars (better
           than nothing, just to test the implementation */

        /* display picture */
        if (!display_disable)
            video_display(is);
    }

    if (0&&show_status)
	{
        static int64_t last_time;
        int64_t cur_time;
        int aqsize, vqsize, sqsize;
        double av_diff;

        cur_time = av_gettime();
        if (!last_time || (cur_time - last_time) >= 30000)
		{
            aqsize = 0;
            vqsize = 0;
            sqsize = 0;

			PacketQueue * audio_queue = is->stream_->get_packet_queue("audio");
			PacketQueue * video_queue = is->stream_->get_packet_queue("video");
			PacketQueue * subtitle_queue = is->stream_->get_packet_queue("subtitle");

            if (is->audio_stream && audio_queue)
				aqsize = audio_queue->size();
            if (is->video_stream && video_queue)
				vqsize = video_queue->size();
            if (is->subtitle_stream && subtitle_queue)
				sqsize = subtitle_queue->size();
            av_diff = 0;
            if (is->audio_stream && is->video_stream)
                av_diff = get_audio_clock(is) - get_video_clock(is);
			
            printf("%7.2f A-V:%7.3f fd=%4d aq=%3.2fKB vq=%3.2fKB sq=%dB f=%"PRId64"/%"PRId64" tm=%7.3f lpts=%3.3f cpts=%3.3f cdrt=%3.3f\n",
                   get_master_clock(is),
                   av_diff,
                   is->frame_drops_early + is->frame_drops_late,
                   (float)aqsize / 1024.0f,
                   (float)vqsize / 1024.0f,
                   sqsize,
                   is->video_stream ? is->video_stream->codec->pts_correction_num_faulty_dts : 0,
                   is->video_stream ? is->video_stream->codec->pts_correction_num_faulty_pts : 0,
				   is->frame_timer,
				   is->frame_last_pts,
				   is->video_current_pts,
				   is->video_current_pts_drift);
            fflush(stdout);
			
            last_time = cur_time;
        }
    }
}

/* allocate a picture (needs to do that in main thread to avoid
   potential locking problems */
static void alloc_picture(VideoState *is)
{
    VideoPicture *vp;

    vp = &is->pictq[is->pictq_windex];

    if (vp->bmp)
	{
		SDL_DestroyTexture(vp->bmp);
	}

#ifdef CONFIG_AVFILTER
    if (vp->picref)
        avfilter_unref_buffer(vp->picref);
    vp->picref = NULL;

    vp->width   = is->out_video_filter->inputs[0]->w;
    vp->height  = is->out_video_filter->inputs[0]->h;
    vp->pix_fmt = static_cast<PixelFormat>(is->out_video_filter->inputs[0]->format);
#else
    vp->width   = is->video_stream->codec->width;
    vp->height  = is->video_stream->codec->height;
    vp->pix_fmt = is->video_stream->codec->pix_fmt;
#endif

	//vp->bmp = SDL_CreateTexture(renderer,/*SDL_PIXELFORMAT_YV12*/SDL_PIXELFORMAT_RGB888,SDL_TEXTUREACCESS_STREAMING, vp->width, vp->height);

	/*
	{
		Uint32 format;
		int access;
		int w,h;
		int ret = SDL_QueryTexture(vp->bmp,&format,&access,&w,&h);
		int tttt=123;
	}
	*/

    if (!vp->bmp/* || vp->bmp->pitches[0] < vp->width*/)
	{
        /* SDL allocates a buffer smaller than requested if the video
         * overlay hardware is unable to support the requested size. */
        fprintf(stderr, "Error: the video system does not support an image\n"
                        "size of %dx%d pixels. Try using -lowres or -vf \"scale=w:h\"\n"
                        "to reduce the image size.\n", vp->width, vp->height );
        //do_exit(is);
    }

    SDL_LockMutex(is->pictq_mutex);
    vp->allocated = 1;
    SDL_CondSignal(is->pictq_cond);
    SDL_UnlockMutex(is->pictq_mutex);
}

static int queue_picture(VideoState *is, AVFrame *src_frame, double pts1, int64_t pos)
{
    VideoPicture *vp;
    double frame_delay, pts = pts1;
	
    /* compute the exact PTS for the picture if it is omitted in the stream
     * pts1 is the dts of the pkt / pts of the frame */
    if (pts != 0)
	{
        /* update video clock with pts, if present */
        is->video_clock = pts;
    }
	else
	{
        pts = is->video_clock;
    }
    /* update video clock for next frame */
    frame_delay = av_q2d(is->video_stream->codec->time_base);
    /* for MPEG2, the frame can be repeated, so we update the
       clock accordingly */
    frame_delay += src_frame->repeat_pict * (frame_delay * 0.5);
    is->video_clock += frame_delay;

#if defined(DEBUG_SYNC) && 0
    printf("frame_type=%c clock=%0.3f pts=%0.3f\n",
           av_get_picture_type_char(src_frame->pict_type), pts, pts1);
#endif

    /* wait until we have space to put a new picture */
    SDL_LockMutex(is->pictq_mutex);
    while (is->pictq_size >= VIDEO_PICTURE_QUEUE_SIZE && !is->abort_request)
	{
        SDL_CondWait(is->pictq_cond, is->pictq_mutex);
    }
    SDL_UnlockMutex(is->pictq_mutex);

    if (is->abort_request)
        return -1;

    vp = &is->pictq[is->pictq_windex];

    vp->duration = frame_delay;

#ifdef USING_NING_FAST_RENDER
	{
#ifdef CONFIG_AVFILTER
		if (vp->picref)
			avfilter_unref_buffer(vp->picref);
		vp->picref = (AVFilterBufferRef*)src_frame->opaque;

		vp->width   = is->out_video_filter->inputs[0]->w;
		vp->height  = is->out_video_filter->inputs[0]->h;
		vp->pix_fmt = static_cast<PixelFormat>(is->out_video_filter->inputs[0]->format);
#else
		vp->width   = is->video_stream->codec->width;
		vp->height  = is->video_stream->codec->height;
		vp->pix_fmt = is->video_stream->codec->pix_fmt;
#endif
		vp->pts = pts;
		vp->pos = pos;
		vp->skip = 0;

		/* now we can update the picture count */
		SDL_LockMutex(is->pictq_mutex);
		if (++is->pictq_windex == VIDEO_PICTURE_QUEUE_SIZE)
			is->pictq_windex = 0;
		is->pictq_size++;
		SDL_UnlockMutex(is->pictq_mutex);
	}
#else

    /* alloc or resize hardware picture buffer */
    if (!vp->bmp || vp->reallocate ||
#ifdef CONFIG_AVFILTER
        vp->width  != is->out_video_filter->inputs[0]->w ||
        vp->height != is->out_video_filter->inputs[0]->h)
	{
#else
        vp->width != is->video_stream->codec->width ||
        vp->height != is->video_stream->codec->height) {
#endif
        SDL_Event event;

        vp->allocated  = 0;
        vp->reallocate = 0;

        /* the allocation must be done in the main thread to avoid
           locking problems */
        is->main_thread_event->post( boost::bind(alloc_picture,is) );

        /* wait until the picture is allocated */
        SDL_LockMutex(is->pictq_mutex);
        while (!vp->allocated && !is->videoq.abort_request)
		{
            SDL_CondWait(is->pictq_cond, is->pictq_mutex);
        }
        /* if the queue is aborted, we have to pop the pending ALLOC event or wait for the allocation to complete */
        if (is->videoq.abort_request && SDL_PeepEvents(&event, 1, SDL_GETEVENT, FF_ALLOC_EVENT, FF_ALLOC_EVENT) != 1)
		{
            while (!vp->allocated)
			{
                SDL_CondWait(is->pictq_cond, is->pictq_mutex);
            }
        }
        SDL_UnlockMutex(is->pictq_mutex);

        if (is->videoq.abort_request)
            return -1;
    }

    /* if the frame is not skipped, then display it */
    if (vp->bmp)
	{
        AVPicture pict = { { 0 } };
#ifdef CONFIG_AVFILTER
        if (vp->picref)
            avfilter_unref_buffer(vp->picref);
        vp->picref = (AVFilterBufferRef*)src_frame->opaque;
#endif

        /* get a pointer on the bitmap */

		void * pixels[3] = {NULL};
		int pitches[3] = {0};
        if( SDL_LockTexture(vp->bmp,NULL,pixels,pitches) == 0 )
		{
			pict.data[0] = (uint8_t*)pixels[0];
			pict.data[1] = (uint8_t*)pixels[2];
			pict.data[2] = (uint8_t*)pixels[1];

			pict.linesize[0] = pitches[0];
			pict.linesize[1] = pitches[2];
			pict.linesize[2] = pitches[1];
		
			int time_stamp = ::SDL_GetTicks();

#ifdef CONFIG_AVFILTER
			// FIXME use direct rendering
			// Fixed by Ning
			/*
			av_picture_copy(&pict, (AVPicture *)src_frame,
							vp->pix_fmt, vp->width, vp->height);
			*/
#else
			sws_flags = av_get_int(sws_opts, "sws_flags", NULL);
			is->img_convert_ctx = sws_getCachedContext(is->img_convert_ctx,
				vp->width, vp->height, vp->pix_fmt, vp->width, vp->height,
				PIX_FMT_YUV420P, sws_flags, NULL, NULL, NULL);
			if (is->img_convert_ctx == NULL) {
				fprintf(stderr, "Cannot initialize the conversion context\n");
				exit(1);
			}
			sws_scale(is->img_convert_ctx, src_frame->data, src_frame->linesize,
					  0, vp->height, pict.data, pict.linesize);
#endif
			/* update the bitmap content */
			SDL_UnlockTexture(vp->bmp);

			//printf("queue picture costs : %u ms\n",::SDL_GetTicks()-time_stamp);

			vp->pts = pts;
			vp->pos = pos;
			vp->skip = 0;

			/* now we can update the picture count */
			SDL_LockMutex(is->pictq_mutex);
			if (++is->pictq_windex == VIDEO_PICTURE_QUEUE_SIZE)
				is->pictq_windex = 0;
			is->pictq_size++;
			SDL_UnlockMutex(is->pictq_mutex);
		}
		else
		{
			fprintf(stderr,"SDL_LockTexture(): %d\n", SDL_GetError());
		}
    }
#endif

    return 0;
}


static int get_video_frame(VideoState *is, AVFrame *frame, int64_t *pts)
{
	AVPacket pkt;
    int got_picture, i;
	bool just_flushed_need_a_key_frame = false;

	if( is->video_flush_request )
	{
		just_flushed_need_a_key_frame = true;

        avcodec_flush_buffers(is->video_stream->codec);

        SDL_LockMutex(is->pictq_mutex);
        // Make sure there are no long delay timers (ideally we should just flush the que but thats harder)
        for (i = 0; i < VIDEO_PICTURE_QUEUE_SIZE; i++)
		{
            is->pictq[i].skip = 1;
        }
        while (is->pictq_size && !is->abort_request)
		{
            SDL_CondWait(is->pictq_cond, is->pictq_mutex);
        }
        is->video_current_pos = -1;
        is->frame_last_pts = AV_NOPTS_VALUE;
        is->frame_last_duration = 0;
        is->frame_timer = (double)av_gettime() / 1000000.0;
        is->frame_last_dropped_pts = AV_NOPTS_VALUE;

        SDL_UnlockMutex(is->pictq_mutex);

		is->video_flush_request = false;
    }

	PacketQueue * video_queue = is->stream_->get_packet_queue("video");
	if( !video_queue )
		return -1;

	int skipped_packet_count = -1;
	do
	{
		skipped_packet_count++;
		unsigned int tried_count = 0;
		int64_t next_k_dts;
		while( !block_wait(
			[&]()->bool{ return video_queue->upper_bound(is->video_packetqueue_last_extracted_dts,pkt,-1,next_k_dts); },
			[&]()->bool{ return is->abort_request || is->paused || tried_count >= 5; },
			[&](){ SDL_Delay(10); tried_count++;} ) )
		{
			if( is->abort_request )
			{
				return -1;
			}
			else if( is->paused )
			{
				SDL_Delay(20);
			}
			else
			{
				if( tried_count >= 5 )
				{
					// after tried 5 times ... but got no data in buffer
					engine_clips_no_buffer_alarm_from_streamplay("video");
					SDL_Delay(20);
				}
			}

		}

		is->video_packetqueue_last_extracted_dts = pkt.dts;

		if( pkt.flags )
			just_flushed_need_a_key_frame = false;

	}while(just_flushed_need_a_key_frame);

	if( skipped_packet_count > 0 )
	{
		DEBUG << "Streamplay : skipped " << skipped_packet_count << " packets for startup";
	}

    avcodec_decode_video2(is->video_stream->codec, frame, &got_picture, &pkt);

    if (got_picture)
	{
        int ret = 1;

        if (decoder_reorder_pts == -1)
		{
            *pts = av_frame_get_best_effort_timestamp(frame);
        }
		else if (decoder_reorder_pts)
		{
            *pts = frame->pkt_pts;
        }
		else
		{
            *pts = frame->pkt_dts;
        }

        if (*pts == AV_NOPTS_VALUE)
		{
            *pts = 0;
        }

        if (((is->av_sync_type == AV_SYNC_AUDIO_MASTER && is->audio_stream) || is->av_sync_type == AV_SYNC_EXTERNAL_CLOCK) &&
             (framedrop>0 || (framedrop && is->audio_stream)))
		{
            SDL_LockMutex(is->pictq_mutex);

            if (is->frame_last_pts != AV_NOPTS_VALUE && *pts)
			{
                double clockdiff = get_video_clock(is) - get_master_clock(is);
                double dpts = av_q2d(is->video_stream->time_base) * *pts;
                double ptsdiff = dpts - is->frame_last_pts;
                if (fabs(clockdiff) < AV_NOSYNC_THRESHOLD &&
                     ptsdiff > 0 && ptsdiff < AV_NOSYNC_THRESHOLD &&
                     clockdiff + ptsdiff - is->frame_last_filter_delay < 0)
				{
					DEBUG << "Streamplay : frame drop first";
                    is->frame_last_dropped_pos = pkt.pos;
                    is->frame_last_dropped_pts = dpts;
                    is->frame_drops_early++;
                    ret = 0;
                }
            }
            SDL_UnlockMutex(is->pictq_mutex);
        }

        if (ret)
            is->frame_last_returned_time = av_gettime() / 1000000.0;

        return ret;
    }
	else
	{
		int meiyougotpicture=1;
	}
    return 0;
}

#ifdef CONFIG_AVFILTER
typedef struct {
    VideoState *is;
    AVFrame *frame;
    int use_dr1;
} FilterPriv;

static int input_get_buffer(AVCodecContext *codec, AVFrame *pic)
{
	std::cout << "input_get_buffer: thread: " << boost::this_thread::get_id() << std::endl;
    AVFilterContext *ctx = (AVFilterContext*)codec->opaque;
    AVFilterBufferRef  *ref;
    int perms = AV_PERM_WRITE;
    int i, w, h, stride[AV_NUM_DATA_POINTERS];
    unsigned edge;
    int pixel_size;

    av_assert0(codec->flags & CODEC_FLAG_EMU_EDGE);

    if (codec->codec->capabilities & CODEC_CAP_NEG_LINESIZES)
        perms |= AV_PERM_NEG_LINESIZES;

    if (pic->buffer_hints & FF_BUFFER_HINTS_VALID) {
        if (pic->buffer_hints & FF_BUFFER_HINTS_READABLE) perms |= AV_PERM_READ;
        if (pic->buffer_hints & FF_BUFFER_HINTS_PRESERVE) perms |= AV_PERM_PRESERVE;
        if (pic->buffer_hints & FF_BUFFER_HINTS_REUSABLE) perms |= AV_PERM_REUSE2;
    }
    if (pic->reference) perms |= AV_PERM_READ | AV_PERM_PRESERVE;

    w = codec->width;
    h = codec->height;

    if(av_image_check_size(w, h, 0, codec) || codec->pix_fmt<0)
        return -1;
	
    avcodec_align_dimensions2(codec, &w, &h, stride);
    edge = codec->flags & CODEC_FLAG_EMU_EDGE ? 0 : avcodec_get_edge_width();
    w += edge << 1;
    h += edge << 1;
    if (codec->pix_fmt != ctx->outputs[0]->format)
	{
        av_log(codec, AV_LOG_ERROR, "Pixel format mismatches %d %d\n", codec->pix_fmt, ctx->outputs[0]->format);
        return -1;
    }
	

    if (!(ref = avfilter_get_video_buffer(ctx->outputs[0], perms, w, h)))
        return -1;

    pixel_size = av_pix_fmt_descriptors[ref->format].comp[0].step_minus1 + 1;
    ref->video->w = codec->width;
    ref->video->h = codec->height;
	
    for (i = 0; i < 4; i ++) {
        unsigned hshift = (i == 1 || i == 2) ? av_pix_fmt_descriptors[ref->format].log2_chroma_w : 0;
        unsigned vshift = (i == 1 || i == 2) ? av_pix_fmt_descriptors[ref->format].log2_chroma_h : 0;

        if (ref->data[i]) {
            ref->data[i]    += ((edge * pixel_size) >> hshift) + ((edge * ref->linesize[i]) >> vshift);
        }
        pic->data[i]     = ref->data[i];
        pic->linesize[i] = ref->linesize[i];
    }
	
    pic->opaque = ref;
    pic->type   = FF_BUFFER_TYPE_USER;
    pic->reordered_opaque	 = codec->reordered_opaque;
    pic->width               = codec->width;
    pic->height              = codec->height;
    pic->format              = codec->pix_fmt;
    pic->sample_aspect_ratio = codec->sample_aspect_ratio;
    if (codec->pkt) pic->pkt_pts = codec->pkt->pts;
    else            pic->pkt_pts = AV_NOPTS_VALUE;
    return 0;
}

static void input_release_buffer(AVCodecContext *codec, AVFrame *pic)
{
	std::cout << "input_release_buffer: thread: " << boost::this_thread::get_id() << std::endl;
    memset(pic->data, 0, sizeof(pic->data));
    avfilter_unref_buffer((AVFilterBufferRef*)pic->opaque);
}

static int input_reget_buffer(AVCodecContext *codec, AVFrame *pic)
{
	std::cout << "input_reget_buffer: thread: " << boost::this_thread::get_id() << std::endl;
    AVFilterBufferRef *ref = (AVFilterBufferRef*)pic->opaque;

    if (pic->data[0] == NULL) {
        pic->buffer_hints |= FF_BUFFER_HINTS_READABLE;
        return codec->get_buffer(codec, pic);
    }

    if ((codec->width != ref->video->w) || (codec->height != ref->video->h) ||
        (codec->pix_fmt != ref->format)) {
        av_log(codec, AV_LOG_ERROR, "Picture properties changed.\n");
        return -1;
    }

    pic->reordered_opaque = codec->reordered_opaque;
    if (codec->pkt) pic->pkt_pts = codec->pkt->pts;
    else            pic->pkt_pts = AV_NOPTS_VALUE;
    return 0;
}

static int input_init(AVFilterContext *ctx, const char *args, void *opaque)
{
	//std::cout << "input_init: thread: " << boost::this_thread::get_id() << std::endl;
    FilterPriv *priv = (FilterPriv*)ctx->priv;
    AVCodecContext *codec;
    if (!opaque) return -1;

    priv->is = (VideoState*)opaque;
    codec    = priv->is->video_stream->codec;
    codec->opaque = ctx;
    if (codec->codec->capabilities & CODEC_CAP_DR1)
	{
        av_assert0(codec->flags & CODEC_FLAG_EMU_EDGE);
        priv->use_dr1 = 1;
		/*
        codec->get_buffer     = input_get_buffer;
        codec->release_buffer = input_release_buffer;
        codec->reget_buffer   = input_reget_buffer;
        codec->thread_safe_callbacks = 0;
		*/
    }

    priv->frame = avcodec_alloc_frame();

    return 0;
}

static void input_uninit(AVFilterContext *ctx)
{
	//std::cout << "input_uninit: thread: " << boost::this_thread::get_id() << std::endl;
    FilterPriv *priv = (FilterPriv*)ctx->priv;
    av_free(priv->frame);
}

static int input_request_frame(AVFilterLink *link)
{
	//std::cout << "input_request_frame: thread: " << boost::this_thread::get_id() << std::endl;
    FilterPriv *priv = (FilterPriv*)link->src->priv;
    AVFilterBufferRef *picref;
    int64_t pts = 0;
    int ret;

    while (!(ret = get_video_frame(priv->is, priv->frame, &pts)))
	{
        //av_free_packet(&pkt);
	}

    if (ret < 0)
        return -1;

    if (priv->use_dr1 && priv->frame->opaque)
	{
        picref = avfilter_ref_buffer((AVFilterBufferRef*)priv->frame->opaque, ~0);
    }
	else
	{
        picref = avfilter_get_video_buffer(link, AV_PERM_WRITE, link->w, link->h);
        av_image_copy(picref->data, picref->linesize,
                      (const uint8_t **)(void **)priv->frame->data, priv->frame->linesize,
                      static_cast<PixelFormat>(picref->format), link->w, link->h);
    }
    //av_free_packet(&pkt);

    avfilter_copy_frame_props(picref, priv->frame);
    picref->pts = pts;

    avfilter_start_frame(link, picref);
    avfilter_draw_slice(link, 0, link->h, 1);
    avfilter_end_frame(link);
	
    return 0;
}

static int input_query_formats(AVFilterContext *ctx)
{
    FilterPriv *priv = (FilterPriv*)ctx->priv;
    enum PixelFormat pix_fmts[] =
	{
        priv->is->video_stream->codec->pix_fmt, PIX_FMT_NONE
    };

    avfilter_set_common_pixel_formats(ctx, avfilter_make_format_list(reinterpret_cast<const int *>(pix_fmts)));
    return 0;
}

static int input_config_props(AVFilterLink *link)
{
    FilterPriv *priv  = (FilterPriv*)link->src->priv;
    VideoState::PlayContext * s = priv->is->video_stream;

    link->w = s->codec->width;
    link->h = s->codec->height;
    link->sample_aspect_ratio = s->codec->sample_aspect_ratio;
    link->time_base = s->time_base;

    return 0;
}

struct HackedStaticAVFilter:
	AVFilter
{
	HackedStaticAVFilter()
	{
		name      = "ffplay_input";

		priv_size = sizeof(FilterPriv);

		init      = input_init;
		uninit    = input_uninit;

		query_formats = input_query_formats;

		inputs    = inputs_instance;
		outputs   = outputs_instance;

		/*
		inputs    = (AVFilterPad[]) {{ .name = NULL }},
		outputs   = (AVFilterPad[]) {{ .name = "default",
										.type = AVMEDIA_TYPE_VIDEO,
										.request_frame = input_request_frame,
										.config_props  = input_config_props, },
									  { .name = NULL }},
		*/

		inputs_instance[0].name = NULL;
		outputs_instance[0].name = "default";
		outputs_instance[0].type = AVMEDIA_TYPE_VIDEO;
		outputs_instance[0].request_frame = input_request_frame;
		outputs_instance[0].config_props = input_config_props;
		outputs_instance[1].name = NULL;
	}


	AVFilterPad		inputs_instance[1];
	AVFilterPad		outputs_instance[2];
};

static HackedStaticAVFilter input_filter;

/*
static AVFilter input_filter =
{
    .name      = "ffplay_input",

    .priv_size = sizeof(FilterPriv),

    .init      = input_init,
    .uninit    = input_uninit,

    .query_formats = input_query_formats,

    .inputs    = (AVFilterPad[]) {{ .name = NULL }},
    .outputs   = (AVFilterPad[]) {{ .name = "default",
                                    .type = AVMEDIA_TYPE_VIDEO,
                                    .request_frame = input_request_frame,
                                    .config_props  = input_config_props, },
                                  { .name = NULL }},
};
*/

static int configure_video_filters(AVFilterGraph *graph, VideoState *is, const char *vfilters)
{
    static const enum PixelFormat pix_fmts[] = { PIX_FMT_YUV420P/*PIX_FMT_BGR0*/, PIX_FMT_NONE };

    int ret;
    AVBufferSinkParams *buffersink_params = av_buffersink_params_alloc();
    AVFilterContext *filt_src = NULL, *filt_out = NULL, *filt_format;
	
	{
		static int sws_flags = SWS_BICUBIC;
		char sws_flags_str[128];
		sprintf(sws_flags_str,/* sizeof(sws_flags_str),*/ "flags=%d", sws_flags);
		graph->scale_sws_opts = av_strdup(sws_flags_str);
	}

    if ((ret = avfilter_graph_create_filter(&filt_src, &input_filter, "src",
                                            NULL, is, graph)) < 0)
        return ret;

#if FF_API_OLD_VSINK_API
    ret = avfilter_graph_create_filter(&filt_out,
                                       avfilter_get_by_name("buffersink"),
                                       "out", NULL, reinterpret_cast<void *>(const_cast<PixelFormat*>(pix_fmts)), graph);
#else
    buffersink_params->pixel_fmts = pix_fmts;
    ret = avfilter_graph_create_filter(&filt_out,
                                       avfilter_get_by_name("buffersink"),
                                       "out", NULL, buffersink_params, graph);
#endif
    av_freep(&buffersink_params);
    if (ret < 0)
        return ret;

    if ((ret = avfilter_link(filt_src, 0, filt_out, 0)) < 0)
        return ret;
	/*
    if ((ret = avfilter_graph_create_filter(&filt_format,
                                            avfilter_get_by_name("format"),
                                            "format", "yuv420p", NULL, graph)) < 0)
        return ret;
    if ((ret = avfilter_link(filt_format, 0, filt_out, 0)) < 0)
        return ret;

    if (vfilters)
	{
        AVFilterInOut *outputs = avfilter_inout_alloc();
        AVFilterInOut *inputs  = avfilter_inout_alloc();

        outputs->name    = av_strdup("in");
        outputs->filter_ctx = filt_src;
        outputs->pad_idx = 0;
        outputs->next    = NULL;

        inputs->name    = av_strdup("out");
        inputs->filter_ctx = filt_format;
        inputs->pad_idx = 0;
        inputs->next    = NULL;

        if ((ret = avfilter_graph_parse(graph, vfilters, &inputs, &outputs, NULL)) < 0)
            return ret;
    }
	else
	{
        if ((ret = avfilter_link(filt_src, 0, filt_format, 0)) < 0)
            return ret;
    }
	*/

    if ((ret = avfilter_graph_config(graph, NULL)) < 0)
        return ret;

    is->out_video_filter = filt_out;

    return ret;
}

#endif  /* CONFIG_AVFILTER */

static int video_thread(void *arg)
{
	DEBUG << "Streamplay : Video thread start";

    VideoState *is = (VideoState*)arg;
    AVFrame *frame = avcodec_alloc_frame();
    int64_t pts_int = AV_NOPTS_VALUE, pos = -1;
    double pts;
    int ret;

#ifdef CONFIG_AVFILTER
    AVFilterGraph *graph = avfilter_graph_alloc();
    AVFilterContext *filt_out = NULL;
    int last_w = is->video_stream->codec->width;
    int last_h = is->video_stream->codec->height;

    if ((ret = configure_video_filters(graph, is, vfilters)) < 0)
        goto the_end;
    filt_out = is->out_video_filter;
#endif

    for (;;)
	{
#ifndef CONFIG_AVFILTER
        AVPacket pkt;
#else
        AVFilterBufferRef *picref;
        AVRational tb = filt_out->inputs[0]->time_base;
#endif
        while (is->paused && !is->abort_request)
            SDL_Delay(10);
		/*
		if( is->abort_request )
			goto the_end;
		*/
		
#ifdef CONFIG_AVFILTER
        if (   last_w != is->video_stream->codec->width
            || last_h != is->video_stream->codec->height)
		{
			// potential crash point, 2012-12-10
            //av_log(NULL, AV_LOG_INFO, "Frame changed from size:%dx%d to size:%dx%d\n",
            //       last_w, last_h, is->video_stream->codec->width, is->video_stream->codec->height);
			DEBUG << "Frame changed from size:" << last_w << "x" << last_h << " to size: " << is->video_stream->codec->width << "x" << is->video_stream->codec->height;

			avfilter_graph_free(&graph);
            graph = avfilter_graph_alloc();
            if ((ret = configure_video_filters(graph, is, vfilters)) < 0)
                goto the_end;
            filt_out = is->out_video_filter;
            last_w = is->video_stream->codec->width;
            last_h = is->video_stream->codec->height;
			
        }

		int time_stamp = ::SDL_GetTicks();
        ret = av_buffersink_get_buffer_ref(filt_out, &picref, 0);
        if (picref)
		{
            avfilter_fill_frame_from_video_buffer_ref(frame, picref);
            pts_int = picref->pts;
            tb      = filt_out->inputs[0]->time_base;
            pos     = picref->pos;
            frame->opaque = picref;

            ret = 1;
        }
		//printf("decode costs : %u ms\n",::SDL_GetTicks()-time_stamp);

        if (ret >= 0 && av_cmp_q(tb, is->video_stream->time_base))
		{
            av_unused int64_t pts1 = pts_int;
            pts_int = av_rescale_q(pts_int, tb, is->video_stream->time_base);
            av_dlog(NULL, "video_thread(): "
                    "tb:%d/%d pts:%"PRId64" -> tb:%d/%d pts:%"PRId64"\n",
                    tb.num, tb.den, pts1,
                    is->video_stream->time_base.num, is->video_stream->time_base.den, pts_int);
        }
#else
        ret = get_video_frame(is, frame, &pts_int, &pkt);
        pos = pkt.pos;
        av_free_packet(&pkt);
#endif

        if (ret < 0)
            goto the_end;

        is->frame_last_filter_delay = av_gettime() / 1000000.0 - is->frame_last_returned_time;
        if (fabs(is->frame_last_filter_delay) > AV_NOSYNC_THRESHOLD / 10.0)
            is->frame_last_filter_delay = 0;

#ifdef CONFIG_AVFILTER
        if (!picref)
            continue;
#endif

        pts = pts_int * av_q2d(is->video_stream->time_base);

        ret = queue_picture(is, frame, pts, pos);

        if (ret < 0)
            goto the_end;
    }
the_end:
#ifdef CONFIG_AVFILTER
    av_freep(&vfilters);
    avfilter_graph_free(&graph);
#endif
    av_free(frame);

	DEBUG << "Streamplay : Video thread end";
    return 0;
}

/* return the wanted number of samples to get better sync if sync_type is video
 * or external master clock */
static int synchronize_audio(VideoState *is, int nb_samples)
{
    int wanted_nb_samples = nb_samples;

    /* if not master, then we try to remove or add samples to correct the clock */
    if (((is->av_sync_type == AV_SYNC_VIDEO_MASTER && is->video_stream) ||
         is->av_sync_type == AV_SYNC_EXTERNAL_CLOCK)) {
        double diff, avg_diff;
        int min_nb_samples, max_nb_samples;

        diff = get_audio_clock(is) - get_master_clock(is);

        if (diff < AV_NOSYNC_THRESHOLD)
		{
            is->audio_diff_cum = diff + is->audio_diff_avg_coef * is->audio_diff_cum;
            if (is->audio_diff_avg_count < AUDIO_DIFF_AVG_NB)
			{
                /* not enough measures to have a correct estimate */
                is->audio_diff_avg_count++;
            }
			else
			{
                /* estimate the A-V difference */
                avg_diff = is->audio_diff_cum * (1.0 - is->audio_diff_avg_coef);

                if (fabs(avg_diff) >= is->audio_diff_threshold) {
                    wanted_nb_samples = nb_samples + (int)(diff * is->audio_src_freq);
                    min_nb_samples = ((nb_samples * (100 - SAMPLE_CORRECTION_PERCENT_MAX) / 100));
                    max_nb_samples = ((nb_samples * (100 + SAMPLE_CORRECTION_PERCENT_MAX) / 100));
                    wanted_nb_samples = FFMIN(FFMAX(wanted_nb_samples, min_nb_samples), max_nb_samples);
                }
                av_dlog(NULL, "diff=%f adiff=%f sample_diff=%d apts=%0.3f vpts=%0.3f %f\n",
                        diff, avg_diff, wanted_nb_samples - nb_samples,
                        is->audio_clock, is->video_clock, is->audio_diff_threshold);
            }
        }
		else
		{
            /* too big difference : may be initial PTS errors, so
               reset A-V filter */
            is->audio_diff_avg_count = 0;
            is->audio_diff_cum       = 0;
        }
    }

    return wanted_nb_samples;
}

/* decode one audio frame and returns its uncompressed size */
static int audio_decode_frame(VideoState *is, double *pts_ptr)
{
    AVPacket *pkt_temp = &is->audio_pkt_temp;
    AVPacket *pkt = &is->audio_pkt;
    AVCodecContext *dec = is->audio_stream->codec;
    int len1, len2, data_size, resampled_data_size;
    int64_t dec_channel_layout;
    int got_frame;
    double pts;
    int flush_complete = 0;
    int wanted_nb_samples;
	bool new_packet = false;

    for (;;)
	{
        /* NOTE: the audio packet can contain several frames */
        while (pkt_temp->size > 0 || (!pkt_temp->data && new_packet))
		{
            if (!is->audio_frame)
			{
                if (!(is->audio_frame = avcodec_alloc_frame()))
                    return AVERROR(ENOMEM);
            }
			else
                avcodec_get_frame_defaults(is->audio_frame);

            if (flush_complete)
                break;

            new_packet = 0;
            len1 = avcodec_decode_audio4(dec, is->audio_frame, &got_frame, pkt_temp);
            if (len1 < 0)
			{
                /* if error, we skip the frame */
                pkt_temp->size = 0;
                break;
            }

            pkt_temp->data += len1;
            pkt_temp->size -= len1;

            if (!got_frame)
			{
                /* stop sending empty packets if the decoder is finished */
                if (!pkt_temp->data && dec->codec->capabilities & CODEC_CAP_DELAY)
                    flush_complete = 1;
                continue;
            }
            data_size = av_samples_get_buffer_size(NULL, dec->channels,
                                                   is->audio_frame->nb_samples,
                                                   dec->sample_fmt, 1);

            dec_channel_layout = (dec->channel_layout && dec->channels == av_get_channel_layout_nb_channels(dec->channel_layout)) ? dec->channel_layout : av_get_default_channel_layout(dec->channels);
            wanted_nb_samples = synchronize_audio(is, is->audio_frame->nb_samples);

            if (dec->sample_fmt != is->audio_src_fmt ||
                dec_channel_layout != is->audio_src_channel_layout ||
                dec->sample_rate != is->audio_src_freq ||
                (wanted_nb_samples != is->audio_frame->nb_samples && !is->swr_ctx))
			{
                if (is->swr_ctx)
                    swr_free(&is->swr_ctx);
                is->swr_ctx = swr_alloc_set_opts(NULL,
                                                 is->audio_tgt_channel_layout, is->audio_tgt_fmt, is->audio_tgt_freq,
                                                 dec_channel_layout,           dec->sample_fmt,   dec->sample_rate,
                                                 0, NULL);
                if (!is->swr_ctx || swr_init(is->swr_ctx) < 0)
				{
                    fprintf(stderr, "Cannot create sample rate converter for conversion of %d Hz %s %d channels to %d Hz %s %d channels!\n",
                        dec->sample_rate,
                        av_get_sample_fmt_name(dec->sample_fmt),
                        dec->channels,
                        is->audio_tgt_freq,
                        av_get_sample_fmt_name(is->audio_tgt_fmt),
                        is->audio_tgt_channels);
                    break;
                }
                is->audio_src_channel_layout = dec_channel_layout;
                is->audio_src_channels = dec->channels;
                is->audio_src_freq = dec->sample_rate;
                is->audio_src_fmt = dec->sample_fmt;
            }

            resampled_data_size = data_size;
            if (is->swr_ctx)
			{
                const uint8_t *in[] = { is->audio_frame->data[0] };
                uint8_t *out[] = {is->audio_buf2};
                if (wanted_nb_samples != is->audio_frame->nb_samples)
				{
                    if (swr_set_compensation(is->swr_ctx, (wanted_nb_samples - is->audio_frame->nb_samples) * is->audio_tgt_freq / dec->sample_rate,
                                                wanted_nb_samples * is->audio_tgt_freq / dec->sample_rate) < 0)
					{
                        fprintf(stderr, "swr_set_compensation() failed\n");
                        break;
                    }
                }
                len2 = swr_convert(is->swr_ctx, out, sizeof(is->audio_buf2) / is->audio_tgt_channels / av_get_bytes_per_sample(is->audio_tgt_fmt),
                                                in, is->audio_frame->nb_samples);
                if (len2 < 0)
				{
                    fprintf(stderr, "audio_resample() failed\n");
                    break;
                }
                if (len2 == sizeof(is->audio_buf2) / is->audio_tgt_channels / av_get_bytes_per_sample(is->audio_tgt_fmt))
				{
                    fprintf(stderr, "warning: audio buffer is probably too small\n");
                    swr_init(is->swr_ctx);
                }
                is->audio_buf = is->audio_buf2;
                resampled_data_size = len2 * is->audio_tgt_channels * av_get_bytes_per_sample(is->audio_tgt_fmt);
            }
			else
			{
                is->audio_buf = is->audio_frame->data[0];
            }

            /* if no pts, then compute it */
            pts = is->audio_clock;
            *pts_ptr = pts;
            is->audio_clock += (double)data_size /
                (dec->channels * dec->sample_rate * av_get_bytes_per_sample(dec->sample_fmt));
			/*
#ifdef _DEBUG
            {
                static double last_clock;
                printf("audio: delay=%0.3f clock=%0.3f pts=%0.3f current_pts=%0.3f get_clock=%0.3f\n",
                       is->audio_clock - last_clock,
					   is->audio_clock, pts , is->audio_current_pts , get_audio_clock(is));
                last_clock = is->audio_clock;
            }
#endif
			*/
            return resampled_data_size;
        }

        /* free the current packet */
        //if (pkt->data)
        //    av_free_packet(pkt);

        memset(pkt_temp, 0, sizeof(*pkt_temp));

        if (is->abort_request)
            return -1;

		if (is->paused )
			return 0;

		if ( is->audio_flush_request )
		{
            avcodec_flush_buffers(dec);
			is->audio_waiting_for_packets = true;
            flush_complete = 0;
			is->audio_flush_request = false;
        }

		PacketQueue * audio_queue = is->stream_->get_packet_queue("audio");
		if( !audio_queue )
			return -1;

		// before read, check if is waiting state.
		if( is->audio_waiting_for_packets )
		{
			int64_t next_k_dts;
			new_packet = audio_queue->upper_bound(is->audio_packetqueue_last_extracted_dts,*pkt,-1,next_k_dts);
			if( new_packet )
			{
				is->audio_packetqueue_last_extracted_dts = pkt->dts;
				is->audio_waiting_for_packets = false;
			}
			else
			{
				return 0;
			}
		}
		else
		{
			/* read next packet */
			unsigned int tried_count = 0;
			int64_t next_k_dts;
			while( !block_wait(
				[&]()->bool{ return audio_queue->upper_bound(is->audio_packetqueue_last_extracted_dts,*pkt,-1,next_k_dts); },
				[&]()->bool{ return is->abort_request || is->paused || tried_count >= 5; },
				[&](){ SDL_Delay(10); tried_count++; } ) )
			{
				if( is->abort_request )
				{
					return -1;
				}
				else if( is->paused )
				{
					// return 0 to play some silence
					return 0;
				}
				else
				{
					if( tried_count )
					{
						engine_clips_no_buffer_alarm_from_streamplay("audio");
						return 0;
					}
				}
			}

			is->audio_packetqueue_last_extracted_dts = pkt->dts;
		}

        *pkt_temp = *pkt;

        /* if update the audio clock with the pts */
        if (pkt->pts != AV_NOPTS_VALUE)
		{
            is->audio_clock = av_q2d(is->audio_stream->time_base)*pkt->pts;
        }
    }
}

/* prepare a new audio buffer */
static void sdl_audio_callback(void *opaque, Uint8 *stream, int len)
{
    VideoState *is = (VideoState*)opaque;
    int audio_size, len1;
    int bytes_per_sec;
    int frame_size = av_samples_get_buffer_size(NULL, is->audio_tgt_channels, 1, is->audio_tgt_fmt, 1);
    double pts;

	int64_t audio_callback_time = av_gettime();

    while (len > 0)
	{
        if (is->audio_buf_index >= is->audio_buf_size)
		{
           audio_size = audio_decode_frame(is, &pts);
           if (audio_size <= 0)
		   {
			   // audio_size = 0 means waiting for something or paused, play silence, Ning.
               /* if error, just output silence */
               is->audio_buf      = is->silence_buf;
               is->audio_buf_size = sizeof(is->silence_buf) / frame_size * frame_size;
           }
		   else
		   {
               //if (is->show_mode != SHOW_MODE_VIDEO)
               //    update_sample_display(is, (int16_t *)is->audio_buf, audio_size);
               is->audio_buf_size = audio_size;
           }
           is->audio_buf_index = 0;
        }
        len1 = is->audio_buf_size - is->audio_buf_index;
        if (len1 > len)
            len1 = len;
        memcpy(stream, (uint8_t *)is->audio_buf + is->audio_buf_index, len1);
        len -= len1;
        stream += len1;
        is->audio_buf_index += len1;
    }
    bytes_per_sec = is->audio_tgt_freq * is->audio_tgt_channels * av_get_bytes_per_sample(is->audio_tgt_fmt);
    is->audio_write_buf_size = is->audio_buf_size - is->audio_buf_index;
    /* Let's assume the audio driver that is used by SDL has two periods. */
    is->audio_current_pts = is->audio_clock - (double)(2 * is->audio_hw_buf_size + is->audio_write_buf_size) / bytes_per_sec;
    is->audio_current_pts_drift = is->audio_current_pts - audio_callback_time / 1000000.0;
}

/* open a given stream. Return 0 if OK */
static int stream_component_open(VideoState *is, int stream_index)
{
	AVFormatContext *ic = is->stream_->get_format_context();
    SDL_AudioSpec wanted_spec, spec;
    AVDictionary *opts = NULL;
    AVDictionaryEntry *t = NULL;
    int64_t wanted_channel_layout = 0;
    int wanted_nb_channels;

    if (stream_index < 0 || stream_index >= ic->nb_streams)
        return -1;

    AVCodecContext * thatctx = ic->streams[stream_index]->codec;
	if( !thatctx )
		return -1;

    //opts = filter_codec_opts(codec_opts, codec, ic, ic->streams[stream_index]);
    AVCodec * codec = avcodec_find_decoder(thatctx->codec_id);
    if (!codec)
        return -1;

	AVCodecContext * thisctx = avcodec_alloc_context3(codec);

	thisctx->width				=	thatctx->width;
	thisctx->height				=	thatctx->height;
	thisctx->coded_width		=	thatctx->coded_width;
	thisctx->coded_height		=	thatctx->coded_height;
	thisctx->pix_fmt			=	thatctx->pix_fmt;
	thisctx->has_b_frames		=	thatctx->has_b_frames;
	thisctx->me_method			=	thatctx->me_method;
	thisctx->time_base			=	thatctx->time_base;
	thisctx->ticks_per_frame	=	thatctx->ticks_per_frame;
	thisctx->extradata_size		=	thatctx->extradata_size;
	thisctx->extradata			=	(uint8_t*)av_malloc(thisctx->extradata_size+FF_INPUT_BUFFER_PADDING_SIZE);
	std::copy(thatctx->extradata,thatctx->extradata+thisctx->extradata_size,thisctx->extradata);
    thisctx->workaround_bugs	=	thatctx->workaround_bugs;
    thisctx->lowres				=	thatctx->lowres;
    if(thisctx->lowres > codec->max_lowres)
	{
        av_log(thisctx, AV_LOG_WARNING, "The maximum value for lowres supported by the decoder is %d\n",
                codec->max_lowres);
        thisctx->lowres = codec->max_lowres;
    }
    thisctx->idct_algo			=	thatctx->idct_algo;
    thisctx->skip_frame			=	thatctx->skip_frame;
    thisctx->skip_idct			=	thatctx->skip_idct;
    thisctx->skip_loop_filter	=	thatctx->skip_loop_filter;
    thisctx->error_concealment	=	thatctx->error_concealment;
	thisctx->flags				=	thatctx->flags;
	thisctx->flags2				=	thatctx->flags2;
	thisctx->sample_rate		=	thatctx->sample_rate;
	thisctx->sample_aspect_ratio=	thatctx->sample_aspect_ratio;
	thisctx->channels			=	thatctx->channels;
	thisctx->channel_layout		=	thatctx->channel_layout;
	thisctx->bit_rate			=	thatctx->bit_rate;
	thisctx->bits_per_coded_sample=	thatctx->bits_per_coded_sample;
	thisctx->request_channel_layout=thatctx->request_channel_layout;
	thisctx->request_channels	=	thatctx->request_channels;
	thisctx->request_sample_fmt	=	thatctx->request_sample_fmt;

    if(thisctx->codec_type == AVMEDIA_TYPE_AUDIO)
	{
		const char *env;
        memset(&is->audio_pkt_temp, 0, sizeof(is->audio_pkt_temp));
        env = SDL_getenv("SDL_AUDIO_CHANNELS");
        if (env)
            wanted_channel_layout = av_get_default_channel_layout(SDL_atoi(env));
        if (!wanted_channel_layout)
		{
            wanted_channel_layout = (thisctx->channel_layout && thisctx->channels == av_get_channel_layout_nb_channels(thisctx->channel_layout)) ? thisctx->channel_layout : av_get_default_channel_layout(thisctx->channels);
            wanted_channel_layout &= ~AV_CH_LAYOUT_STEREO_DOWNMIX;
            wanted_nb_channels = av_get_channel_layout_nb_channels(wanted_channel_layout);
            /* SDL only supports 1, 2, 4 or 6 channels at the moment, so we have to make sure not to request anything else. */
            while (wanted_nb_channels > 0 && (wanted_nb_channels == 3 || wanted_nb_channels == 5 || wanted_nb_channels > 6))
			{
                wanted_nb_channels--;
                wanted_channel_layout = av_get_default_channel_layout(wanted_nb_channels);
            }
        }
        wanted_spec.channels = av_get_channel_layout_nb_channels(wanted_channel_layout);
        wanted_spec.freq = thisctx->sample_rate;
        if (wanted_spec.freq <= 0 || wanted_spec.channels <= 0)
		{
            fprintf(stderr, "Invalid sample rate or channel count!\n");
            return -1;
        }
    }

    if (!av_dict_get(opts, "threads", NULL, 0))
        av_dict_set(&opts, "threads", "auto", 0);

	ffmpeg_lock();
    if (!codec || avcodec_open2(thisctx, codec, &opts) < 0)
	{
		ffmpeg_unlock();
        return -1;
	}
	ffmpeg_unlock();

    if ((t = av_dict_get(opts, "", NULL, AV_DICT_IGNORE_SUFFIX)))
	{
        av_log(NULL, AV_LOG_ERROR, "Option %s not found.\n", t->key);
        return AVERROR_OPTION_NOT_FOUND;
    }

    /* prepare audio output */
    if (thisctx->codec_type == AVMEDIA_TYPE_AUDIO)
	{
        wanted_spec.format = AUDIO_S16SYS;
        wanted_spec.silence = 0;
        wanted_spec.samples = SPEAKER_AUDIO_BUFFER_SIZE;
        wanted_spec.callback = sdl_audio_callback;
        wanted_spec.userdata = is;
        if (SDL_OpenAudio(&wanted_spec, &spec) < 0)
		{
            fprintf(stderr, "SDL_OpenAudio: %s\n", SDL_GetError());
            return -1;
        }
        is->audio_hw_buf_size = spec.size;
        if (spec.format != AUDIO_S16SYS)
		{
            fprintf(stderr, "SDL advised audio format %d is not supported!\n", spec.format);
            return -1;
        }
        if (spec.channels != wanted_spec.channels)
		{
            wanted_channel_layout = av_get_default_channel_layout(spec.channels);
            if (!wanted_channel_layout)
			{
                fprintf(stderr, "SDL advised channel count %d is not supported!\n", spec.channels);
                return -1;
            }
        }
        is->audio_src_fmt = is->audio_tgt_fmt = AV_SAMPLE_FMT_S16;
        is->audio_src_freq = is->audio_tgt_freq = spec.freq;
        is->audio_src_channel_layout = is->audio_tgt_channel_layout = wanted_channel_layout;
        is->audio_src_channels = is->audio_tgt_channels = spec.channels;
    }

    switch (thisctx->codec_type)
	{
    case AVMEDIA_TYPE_AUDIO:
        is->audio_stream = new VideoState::PlayContext();
		is->audio_stream->codec = thisctx;
		is->audio_stream->time_base = ic->streams[stream_index]->time_base;
		is->audio_packetqueue_last_extracted_dts = -1;
		is->audio_flush_request = true;

		if( is->start_playing_pos > 0 )
		{
			is->audio_packetqueue_last_extracted_dts = is->start_playing_pos / av_q2d(is->audio_stream->time_base);
		}

        is->audio_buf_size  = 0;
        is->audio_buf_index = 0;

        /* init averaging filter */
        is->audio_diff_avg_coef  = exp(log(0.01) / AUDIO_DIFF_AVG_NB);
        is->audio_diff_avg_count = 0;
        /* since we do not have a precise anough audio fifo fullness,
           we correct audio sync only if larger than this threshold */
        is->audio_diff_threshold = 2.0 * SPEAKER_AUDIO_BUFFER_SIZE / wanted_spec.freq;

        memset(&is->audio_pkt, 0, sizeof(is->audio_pkt));
        SDL_PauseAudio(0);
        break;
    case AVMEDIA_TYPE_VIDEO:
        is->video_stream = new VideoState::PlayContext();
		is->video_stream->codec = thisctx;
		is->video_stream->time_base = ic->streams[stream_index]->time_base;
		is->video_packetqueue_last_extracted_dts = -1;
		is->video_flush_request = true;
		
		if( is->start_playing_pos > 0 )
		{
			is->video_packetqueue_last_extracted_dts = is->start_playing_pos / av_q2d(is->video_stream->time_base);
		}
		///
		engine_streamplay_size_obtain_from_ffplay(thisctx->width,thisctx->height);
		///

        is->video_thread = new boost::thread( boost::bind(video_thread,is) );
        break;
    case AVMEDIA_TYPE_SUBTITLE:
        is->subtitle_stream = new VideoState::PlayContext();
		is->subtitle_stream->codec = thisctx;
		is->subtitle_stream->time_base = ic->streams[stream_index]->time_base;
		is->subtitle_packetqueue_last_extracted_dts = -1;
		is->subtitle_flush_request = true;

		if( is->start_playing_pos > 0 )
		{
			is->subtitle_packetqueue_last_extracted_dts = is->start_playing_pos / av_q2d(is->subtitle_stream->time_base);
		}
		
        is->subtitle_tid = SDL_CreateThread(subtitle_thread, "Subtitle", is);
        break;
    default:
        break;
    }
    return 0;
}


static void stream_component_close(VideoState *is, int stream_index)
{
	AVFormatContext *ic = is->stream_->get_format_context();
    AVCodecContext *avctx;
	int codec_type = -1;

    if (stream_index < 0 || stream_index >= ic->nb_streams)
        return;

    switch (ic->streams[stream_index]->codec->codec_type)
	{
    case AVMEDIA_TYPE_AUDIO:
		avctx = is->audio_stream->codec;
        break;
    case AVMEDIA_TYPE_VIDEO:
        avctx = is->video_stream->codec;
        break;
    case AVMEDIA_TYPE_SUBTITLE:
        avctx = is->subtitle_stream->codec;
        break;
    default:
        break;
    }

	codec_type = avctx->codec_type;

    switch(codec_type)
	{
    case AVMEDIA_TYPE_AUDIO:
		SDL_CloseAudio();
        //av_free_packet(&is->audio_pkt);
        if (is->swr_ctx)
            swr_free(&is->swr_ctx);
        is->audio_buf = NULL;
        av_free(is->audio_frame);

		/*
        if (is->rdft)
		{
            av_rdft_end(is->rdft);
            av_freep(&is->rdft_data);
            is->rdft = NULL;
            is->rdft_bits = 0;
        }
		*/
        break;
    case AVMEDIA_TYPE_VIDEO:
        /* note: we also signal this mutex to make sure we deblock the
           video thread in all cases */
        SDL_LockMutex(is->pictq_mutex);
        SDL_CondSignal(is->pictq_cond);
        SDL_UnlockMutex(is->pictq_mutex);

		is->video_thread->join();
		delete is->video_thread;
		is->video_thread = NULL;
        break;
    case AVMEDIA_TYPE_SUBTITLE:
        /* note: we also signal this mutex to make sure we deblock the
           video thread in all cases */
        SDL_LockMutex(is->subpq_mutex);
        is->subtitle_stream_changed = 1;
        SDL_CondSignal(is->subpq_cond);
        SDL_UnlockMutex(is->subpq_mutex);

        SDL_WaitThread(is->subtitle_tid, NULL);
		is->subtitle_tid = NULL;
        break;
    default:
        break;
    }

	ffmpeg_lock();
    avcodec_close(avctx);
	ffmpeg_unlock();

	av_free(avctx);
	avctx = NULL;

    switch(codec_type)
	{
    case AVMEDIA_TYPE_AUDIO:
		delete is->audio_stream;
        is->audio_stream = NULL;
        is->audio_stream_index = -1;
        break;
    case AVMEDIA_TYPE_VIDEO:
		delete is->video_stream;
        is->video_stream = NULL;
        is->video_stream_index = -1;
        break;
    case AVMEDIA_TYPE_SUBTITLE:
		delete is->subtitle_stream;
        is->subtitle_stream = NULL;
        is->subtitle_stream_index = -1;
        break;
    default:
        break;
    }
}

static bool stream_open_local(VideoState * is)
{
	int wait_count = 0;
	if( !block_wait([&](){ return is->stream_->is_ready_to_play(); },
					[&](){ return is->abort_request || wait_count >= 20; },
					[&](){ wait_count++; SDL_Delay(1000); }) )
	{
		if( wait_count >= 20 )
		{
			DEBUG << "Streamplay wait for stream ready timeout";
		}
		return false;
	}

	{
		AVFormatContext * ic = is->stream_->get_format_context();
		if( !ic )
		{
			DEBUG << "Streamplay no AVFormatContext";
			return false;
		}
	}

    is->av_sync_type = av_sync_type;

    /* open the streams */

    is->show_mode = show_mode;

    is->audio_stream_index = is->stream_->get_stream_index("audio");
    is->video_stream_index = is->stream_->get_stream_index("video");
    is->subtitle_stream_index = is->stream_->get_stream_index("subtitle");

    if (is->audio_stream_index >= 0)
	{
        if( stream_component_open(is, is->audio_stream_index) < 0 )
		{
			// no audio not a fatal error
			WARN << "Streamplay open audio component failed";
			is->audio_stream_index = -1;
		}
    }

    if (is->video_stream_index >= 0)
	{
		int ret = stream_component_open(is, is->video_stream_index);
		if( ret < 0 )
		{
			ERROR << "Streamplay open video component failed";
			return -1;
		}

		if (is->show_mode == SHOW_MODE_NONE)
			is->show_mode = ret >= 0 ? SHOW_MODE_VIDEO : SHOW_MODE_RDFT;
    }

    if (is->subtitle_stream_index >= 0)
	{
        if( stream_component_open(is, is->subtitle_stream_index) < 0 )
		{
			WARN << "Streamplay open subtitle component failed";
			is->subtitle_stream_index = -1;
		}
    }

    if (is->video_stream_index < 0 && is->audio_stream_index < 0)
	{
		ERROR << "Streamplay : could not open codecs";
		return -1;
    }

	is->refresh_thread = new boost::thread([=]()
	{
		DEBUG << "Streamplay : Refresh thread start";
		while(!is->abort_request)
		{
			if(is->has_played && !is->refresh)
			{
				VideoState * is_ = is;
				is_->refresh = 1;
				is_->main_event_loop->post( [=]()
				{
					video_refresh(is_);
					is_->refresh = 0;
				} );
			}
			//FIXME ideally we should wait the correct time but SDLs event passing is so slow it would be silly
			//usleep(is->audio_stream && is->show_mode != SHOW_MODE_VIDEO ? rdftspeed*1000 : 5000);
			//::Sleep(is->audio_stream && is->show_mode != SHOW_MODE_VIDEO ? rdftspeed*1000 : 5000);
			unsigned int t = is->audio_stream && is->show_mode != SHOW_MODE_VIDEO ? rdftspeed* 1 : 5;
			boost::this_thread::sleep( boost::posix_time::milliseconds(t) );
		}
		DEBUG << "Streamplay : Refresh thread end";
	});

	is->has_played = true;
	return true;
}

void StreamPlay::stream_close()
{
	VideoState * is = this->is_;
    VideoPicture *vp;

	//Sleep(5000);
    /* XXX: use a special url_shutdown call to abort parse cleanly */
	// It is safe because this variable is only readable to other working threads.
	{
		is->abort_request = 1;
	}
	//Sleep(5000);

    is->refresh_thread->join();
	delete is->refresh_thread;
	is->refresh_thread = NULL;

	is->main_thread->join();
	delete is->main_thread;

	delete is_->main_event_loop;
	is_->main_event_loop = NULL;

	/* close each stream */
    if (is->audio_stream_index >= 0)
        stream_component_close(is, is->audio_stream_index);
    if (is->video_stream_index >= 0)
        stream_component_close(is, is->video_stream_index);
    if (is->subtitle_stream_index >= 0)
        stream_component_close(is, is->subtitle_stream_index);

    /* free all pictures */
    for (int i = 0; i < VIDEO_PICTURE_QUEUE_SIZE; i++)
	{
        vp = &is->pictq[i];
#ifdef CONFIG_AVFILTER
        if (vp->picref)
		{
            avfilter_unref_buffer(vp->picref);
            vp->picref = NULL;
        }
#endif
        if (vp->bmp)
		{
			SDL_DestroyTexture(vp->bmp);
            vp->bmp = NULL;
        }

    }

#ifndef CONFIG_AVFILTER
    if (is->img_convert_ctx)
        sws_freeContext(is->img_convert_ctx);
#endif
}


//////////

StreamPlay::StreamPlay(Stream * stream_):
stream_(stream_)
{
	is_ = new VideoState();
	memset(is_,0,sizeof(*is_));
}

StreamPlay::~StreamPlay()
{
	delete is_;
}


bool StreamPlay::start(double start_playing_pos)
{
	if( !stopped() )
		return false;

	INFO << "Streamplay start at " << start_playing_pos  <<" , " << stream_->get_filename();

	memset(is_,0,sizeof(*is_));
	is_->stream_ = stream_;
	
	is_->stream_current_playing_pos = -1;
	is_->start_playing_pos = start_playing_pos;

    /* start video display */
    is_->pictq_mutex = SDL_CreateMutex();
    is_->pictq_cond  = SDL_CreateCond();

    is_->subpq_mutex = SDL_CreateMutex();
    is_->subpq_cond  = SDL_CreateCond();

	is_->main_event_loop = new boost::asio::io_service();

	if( !stream_open_local(is_) )
	{
		DEBUG << "Streamplay open local failed";
		return false;
	}

	is_->main_thread = new boost::thread([=]()
	{
		while(!is_->abort_request)
		{
			if( !is_->main_event_loop->run_one() )
				boost::this_thread::sleep( boost::posix_time::milliseconds(10) );
		}
	});

	if( is_->main_thread )
	{
		return true;
	}
	else
		return false;
}


void StreamPlay::stop()
{
	if( stopped() )
		return;

	INFO << "Streamplay stop : " << is_->stream_->get_filename();

	stream_close();

    SDL_DestroyMutex(is_->pictq_mutex);
    SDL_DestroyCond(is_->pictq_cond);
    SDL_DestroyMutex(is_->subpq_mutex);
    SDL_DestroyCond(is_->subpq_cond);
	is_->pictq_mutex = NULL;
	is_->pictq_cond = NULL;
	is_->subpq_mutex = NULL;
	is_->subpq_cond = NULL;

	// do this to signal has exited.
	is_->main_thread = NULL;
}

bool StreamPlay::stopped() const
{
	return is_->main_thread==NULL;
}

bool StreamPlay::playing() const
{
	if( stopped() )
		return false;
	return true;
}

bool StreamPlay::pausing() const
{
	if( stopped() )
		return false;
	return is_->paused;
}

void StreamPlay::toggle_pause()
{
	if( !playing() )
		return;
	stream_toggle_pause(is_);
}

double StreamPlay::current_playing_pos()
{
	return is_->stream_current_playing_pos;
}