#include "engine.h"
#include "screen.h"
#include "ffplay.h"
#include "screenui_center_announcement.h"

#include "log.h"

#include <boost/lexical_cast.hpp>
#include <Windows.h>

using namespace std;

static bool is_waiting_for_buffer_state = false;
static unsigned int last_no_buffer_signal_ticks = 0;
static double last_buffering_percentage = -1;
static unsigned int last_buffering_percentage_ticks = 0;

// Question: Do you know why there is no need to have a lock?

static double calculate_buffered_ratio()
{
	double buffered = ffplay_episode_how_long_buffered(11.0);
	double buffered_ratio_ = min(1.0,buffered/10.0);
	return buffered_ratio_;
}
void wacthing_quality_daemon_reset_from_engine()
{
	is_waiting_for_buffer_state = false;
}

void wacthing_quality_daemon_no_buffer_from_engine()
{
	if( is_waiting_for_buffer_state )
		return;

	double buffered_ratio = calculate_buffered_ratio();
	if( buffered_ratio >= 1 )
	{
		DEBUG << "wacthing_quality_daemon_no_buffer_from_engine : it seemed a negtive-positive alarm";
		return;
	}

	engine_play_pause_from_screenui();

	is_waiting_for_buffer_state = true;
	last_no_buffer_signal_ticks = GetTickCount();
}

void watching_quality_deamon_idle()
{
	static unsigned int frame_count = 0;
	frame_count++;
	if( frame_count % 10 != 0 )
		return;

	if( ffplay_episode_playing() )
	{
		if( is_waiting_for_buffer_state )
		{
			double buffered_ratio = calculate_buffered_ratio();
			static int animating_frame_count = 0;
			center_announcement_caption( "缓冲" + string(" ") + boost::lexical_cast<string>( (int)(buffered_ratio*100) ) + string("%") );
			animating_frame_count++;
			center_announcement_show(true);

			// at least wait 1 sec, check if quit this state
			if( fabs((double)GetTickCount() - (double)last_no_buffer_signal_ticks) > 1000 )
			{
				if( !ffplay_episode_pausing() )
				{
					is_waiting_for_buffer_state = false;
				}
				else 
				{
					if( buffered_ratio >= 1.0 )
					{
						engine_play_pause_from_screenui();
						is_waiting_for_buffer_state = false;
					}
					else
					{
						if( last_buffering_percentage != buffered_ratio )
						{
							last_buffering_percentage = buffered_ratio;
							last_buffering_percentage_ticks = GetTickCount();
						}

						if( fabs((double)GetTickCount() - (double)last_buffering_percentage_ticks) > 20*1000 )
						{
							engine_episode_playing_failed("缓冲等待超时,网络不给力。");
							last_buffering_percentage = -1;
						}
					}
				}
			}
		}
		else
		{
			center_announcement_show(false);
		}
	}
	else
	{
		center_announcement_show(false);
	}
}