#include <SDL.h>
#include <SDL_main.h>

#include "engine.h"
#include "gesture.h"
#include "ffplay.h"
#include "screen.h"
#include "screenui.h"
#include "screenui_progresser.h"
#include "screenui_time_span.h"
#include "screenui_corner.h"
#include "screenui_play_button.h"
#include "screenui_center_announcement.h"
#include "system_tray.h"
#include "sniffer.h"
#include "parser.h"
#include "next_url.h"
#include "watching_quality_deamon.h"
#include "reporter.h"
#include "userid.h"
#include "utility.hpp"
#include "log.h"
#include "version_checker.h"
#include "version.h"

#include <memory>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/thread.hpp>
#include <iostream>
#include <sstream>

using namespace std;

static std::shared_ptr<boost::asio::io_service>		main_thread_ioservice = std::make_shared<boost::asio::io_service>();
static bool											opaque_mode = true;
static bool											sniffer_mode = true;
static bool											episode_loading = false;

static bool											screen_moving_mode = false;
static pair<int,int>								screen_moving_cursor_tag;

static bool											abort_request = false;


bool engine_sniffer_mode()
{
	return sniffer_mode;
}

void engine_sniffer_mode(bool on)
{
	main_thread_ioservice->post( [=]()
	{
		sniffer_mode = on;
		system_tray_states_sync_from_engine();
	});
}

void engine_sniffered_from_sniffer(std::string src_page)
{
	main_thread_ioservice->post( [=]()
	{
		auto url = src_page;
		if( parser_is_parsing_url(url) )
		{
			//DEBUG << "engine_sniffered_from_sniffer : is parsing, abandoned";
			return;
		}
		else if( url == ffplay_episode_src_page() ) // ffplay_episode_src_page implicates being playing
		{
			//DEBUG << "engine_sniffered_from_sniffer : is playing, abandoned";
			return;
		}

		engine_episode_loading(true);
		parser_parse(url,[=](std::vector<std::string> urls)
		{
			// called from parser threads
			engine_episode_loading(true);
			engine_open_stream_request_from_sniffer(url,urls);
			engine_episode_loading(false);
		});
	});
}

// episode
void engine_open_stream_request_from_sniffer(std::string src_page,std::vector<std::string> urls)
{
	main_thread_ioservice->post( [=]()
	{
		DEBUG << "engine_open_stream_request_from_sniffer";
		if( !engine_sniffer_mode() )
			return;
		/*
		if( ffplay_episode_playing() )
			return;
		*/

		// parse failed
		if( urls.size() == 0 )
			return;

		{
			stringstream ss;
			ss << "src_page = " << src_page;
			reporter_report("PlayStart",ss.str());
		}

		wacthing_quality_daemon_reset_from_engine();
		screen_show(false);
		screenui_show(false);
		center_announcement_show(false);
		ffplay_episode_stop();
		ffplay_episode_start(src_page,urls);
		play_button_state_sync();
	});
}

void engine_streamplay_size_obtain_from_ffplay(int width,int height)
{
	main_thread_ioservice->post( [=]()
	{
		ffplay_streamplay_size_obtain_from_engine(width,height);
	});
}

void engine_episode_size_obtain_from_ffplay(int width,int height)
{
	main_thread_ioservice->post( [=]()
	{
		engine_episode_loading(false);
		if( width <= 1 || height <= 1 )
		{
			WARN << "engine_episode_size_obtain_from_ffplay : width or height meaningless , " << width << "x" << height;
			return;
		}
		screen_show(true);
		screen_size_gracefully(width,height);
		screen_alpha(1.0f);
		screenui_alpha(1.0f);
		screenui_show(true);
		screen_toggle_restore();
	});
}

void engine_clips_buffered_up_from_stream(std::string filename)
{
	main_thread_ioservice->post( [=]()
	{
		ffplay_clips_buffered_up_from_engine(filename);
	});
}

void engine_clips_no_buffer_alarm_from_streamplay(std::string which_part)
{
	main_thread_ioservice->post( [=]()
	{
		DEBUG << "engine_clips_no_buffer_alarm_from_streamplay : " << which_part;
		wacthing_quality_daemon_no_buffer_from_engine();
	});
}

void engine_clips_playing_stopped_from_streamplay()
{
	main_thread_ioservice->post( [=]()
	{
		ffplay_episode_next_clips();
	});
}

void engine_episode_playing_stopped_from_ffplay()
{
	main_thread_ioservice->post( [=]()
	{
		{
			stringstream ss;
			ss << "reason = " << "finished";
			reporter_report("PlayStop",ss.str());
		}

		// in case no video ratio aspect obtained
		engine_episode_loading(false);
		screen_show(false);
		screenui_show(false);
		center_announcement_show(false);
		ffplay_episode_stop();
		//engine_next_from_screenui();
	});
}

void engine_episode_playing_failed(std::string reason)
{
	main_thread_ioservice->post( [=]()
	{
		DEBUG << "engine_episode_playing_failed, reason : " << reason;

		{
			stringstream ss;
			ss << "reason = " << reason;
			reporter_report("PlayFailed",ss.str());
		}

		// in case no video ratio aspect obtained
		engine_episode_loading(false);
		screen_show(false);
		screenui_show(false);
		center_announcement_show(false);
		ffplay_episode_stop();
		system_tray_balloon("播放失败了, 有可能是该视频网站不支持在线拖动进度条(比如土豆)，只能拖动进度条的已经缓冲好的部分；或者是网络问题。原因:" + reason,"warning");
		//ffplay_episode_start(last_played_urls);
	});
}

void engine_play_pause_from_screenui()
{
	main_thread_ioservice->post( [=]()
	{
		if( ffplay_episode_playing() )
		{
			ffplay_episode_toggle_pause();
			play_button_state_sync();
		}
		else
		{

		}
	});
}

void engine_next_from_screenui()
{
	main_thread_ioservice->post( [=]()
	{
		engine_episode_loading(true);
		next_url_from_engine(ffplay_episode_src_page());
	});
}

void engine_parsed_from_next_url(std::string src_page,std::vector<std::string> urls)
{                                                                                                                                                                                                                                                                                                                                                                                                           
	main_thread_ioservice->post( [=]()
	{
		engine_episode_loading(false);
		engine_open_stream_request_from_sniffer(src_page,urls);
	});
}

void engine_screen_size(double ratio)
{
	main_thread_ioservice->post( [=]()
	{
		screen_toggle_restore();

		auto video_width_height = ffplay_episode_size();
		if( video_width_height.first <= 0 || video_width_height.second <= 0 )
		{
			WARN << "engine_screen_size : video_width_height meaningless";
			return;
		}
		if( ratio <= 0 || ratio >= 4 )
		{
			WARN << "engine_screen_size : ratio meaningless , " << ratio;
			return;
		}
		else
		{
			screen_size(ratio*video_width_height.first,ratio*video_width_height.second);
		}
	});
}

void engine_screen_position_changed_from_system()
{
	main_thread_ioservice->post( [=]()
	{
		screenui_move();
	});
}

void engine_screen_close_from_screenui()
{
	main_thread_ioservice->post( [=]()
	{
		{
			stringstream ss;
			ss << "reason = " << "by_user";
			reporter_report("PlayStop",ss.str());
		}

		screen_show(false);
		screenui_show(false);
		center_announcement_show(false);
		ffplay_episode_stop();
	});
}

void engine_screen_close_from_system()
{
	main_thread_ioservice->post( [=]()
	{
		screen_show(false);
		screenui_show(false);
		center_announcement_show(false);
		ffplay_episode_stop();
	});
}

void engine_screen_toggle_minimize_from_system()
{
	main_thread_ioservice->post( [=]()
	{
		screen_toggle_minimize();
		screenui_show(false);
	});
}

void engine_screen_toggle_maximize_from_system()
{
	main_thread_ioservice->post( [=]()
	{
		screen_toggle_fullscreen();
		screenui_show(true);
	});
}

void engine_screen_restore_from_system()
{
	main_thread_ioservice->post( [=]()
	{
		screen_corner_clip(true);
		screen_toggle_restore();
	});
}

void engine_screen_toggle_topmost()
{
	main_thread_ioservice->post( [=]()
	{
		screen_toggle_topmost();
	});
}

void engine_screen_mouse_left_up()
{
	return;
	main_thread_ioservice->post( [=]()
	{
		if( !screen_moving_mode )
			return;
		screen_moving_mode = false;
	});
}

void engine_screen_mouse_left_down()
{
	return;
	main_thread_ioservice->post( [=]()
	{
		if( !opaque_mode )
			return;
		if( screen_moving_mode )
			return;
		if( screen_fullscreen() )
			return;

		POINT p;
		GetCursorPos(&p);
		screen_moving_cursor_tag = make_pair(p.x,p.y);
		//screenui_show(false);
		screen_moving_mode = true;
	});
}

void engine_screen_mouse_move()
{
	return;
	main_thread_ioservice->post( [=]()
	{
		if( !screen_moving_mode )
			return;
		
		POINT p;
		GetCursorPos(&p);

		pair<int,int> displacement = make_pair(p.x-screen_moving_cursor_tag.first,p.y-screen_moving_cursor_tag.second);
		if( displacement.first == 0 && displacement.second == 0 )
			return;
		pair<int,int> old_wp = screen_position();
		pair<int,int> new_wp = make_pair(old_wp.first+displacement.first,old_wp.second+displacement.second);
		screen_position(new_wp.first,new_wp.second);
		screen_moving_cursor_tag = make_pair(p.x,p.y);
	});
}

void engine_screen_keyboard_left()
{
	main_thread_ioservice->post( [=]()
	{
		static double last_current_pos = -1;
		double current_pos = ffplay_episode_current_playing_pos();
		if( current_pos >= 0 )
			last_current_pos = current_pos - 25.0;
		else
		{
			if( last_current_pos >= 0 )
				last_current_pos = last_current_pos - 25.0;
			else
				return;
		}
		ffplay_episode_seek(last_current_pos);
	});
}

void engine_screen_keyboard_right()
{
	main_thread_ioservice->post( [=]()
	{
		static double last_current_pos = -1;
		double current_pos = ffplay_episode_current_playing_pos();
		if( current_pos >= 0 )
			last_current_pos = current_pos + 5.0;
		else
		{
			if( last_current_pos >= 0 )
				last_current_pos = last_current_pos + 5.0;
			else
				return;
		}
		ffplay_episode_seek(last_current_pos);
	});
}

bool engine_screen_opaque_mode()
{
	return opaque_mode;
}

void engine_screen_opaque_mode(bool opaque)
{
	main_thread_ioservice->post( [=]()
	{
		if( !ffplay_episode_playing() )
			return;
		if( opaque_mode == opaque )
			return;
		if( screen_minimized() )
			return;

		opaque_mode = opaque;
		if( !opaque )
		{
			screenui_set_opaque(false);
		}
		if( !screen_topmost() )
		{
			screen_toggle_topmost();
		}

		screen_set_opaque_mode(opaque_mode);
		system_tray_states_sync_from_engine();
	});
}

void engine_episode_loading(bool on)
{
	main_thread_ioservice->post( [=]()
	{
		episode_loading = on;
		system_tray_states_sync_from_engine();
	});
}

bool engine_episode_loading()
{
	return episode_loading;
}

static void idle(boost::system::error_code const & ec,std::shared_ptr<boost::asio::deadline_timer> timer)
{
	if( ec )
	{
		return;
	}
	else
	{
		//std::cout << "idle " << std::endl;
		/*
		// test close window
		static int i = 0;
		if( i++ == 500 )
		{
			printf("screen close\n");
			ffplay_screen_close();
			printf("screen closed\n");
			return;
		}
		*/

		// test change size
		
		static int first_initialize_count = 0;
		if( first_initialize_count < 10)
		{
			screen_center_in_monitor();
			first_initialize_count++;
		}
		
		screenui_idle();

		// no gesture for static ui
		gesture_idle();

		progresser_idle();

		time_span_idle();

		ffplay_idle();

		watching_quality_deamon_idle();

		reporter_idle();

		version_checker_idle();

		timer->expires_from_now(boost::posix_time::milliseconds(50));
		timer->async_wait( boost::bind( &idle , boost::asio::placeholders::error , timer ) );
	}
}

void engine_drag_progresser_move_from_screenui(double current_pos)
{

}

void engine_drag_progresser_end_from_screenui(double current_pos)
{
	main_thread_ioservice->post( [=]()
	{
		ffplay_episode_seek(current_pos);
	});
}

void engine_redraw_progresser_from_screenui()
{
	main_thread_ioservice->post( [=]()
	{
		render_progresser();
	});
}

static int				drag_corner_start_x;
static int				drag_corner_start_y;
std::pair<int,int>		drag_corner_start_screen_size;
std::pair<int,int>		drag_corner_start_screen_position;

void engine_drag_corner_start(corner_dir dir, int x, int y)
{
	drag_corner_start_x = x;
	drag_corner_start_y = y;
	drag_corner_start_screen_size = screen_size();
	drag_corner_start_screen_position = screen_position();
}

void engine_drag_corner_move(corner_dir dir, int x, int y)
{
	static int last_x=-1;
	static int last_y=-1;
	static int frame_count=0;
	frame_count++;
	if( frame_count%30 != 0 )
		return;
	if( last_x == x && last_y == y )
		return;
	last_x = x;
	last_y = y;

	main_thread_ioservice->post( [=]()
	{
		int delta_x = x - drag_corner_start_x;
		int delta_y = y - drag_corner_start_y;
		//DEBUG << "Drag_corner : " << dir << " , " << delta_x << " , " << delta_y;
		auto size = drag_corner_start_screen_size;
		auto pos = drag_corner_start_screen_position;
		switch (dir)
		{
			case LEFTUP:
				size.first -= delta_x;
				size.second -= delta_y;
				pos.first += delta_x;
				pos.second += delta_y;
				break;
			case LEFTBOTTOM:
				size.first -= delta_x;
				size.second += delta_y;
				pos.first += delta_x;
				break;
			case RIGHTUP:
				size.first += delta_x;
				size.second -= delta_y;
				pos.second += delta_y;
				break;
			case RIGHTBOTTOM:
				size.first += delta_x;
				size.second += delta_y;
				break;
		}

		// keep video aspect ratio
		{
			auto video_width_height = ffplay_episode_size();
			if( video_width_height.first <= 0 || video_width_height.second <= 0 )
			{
				WARN << "engine_drag_corner_move : video_width_height meaningless";
				return;
			}
			double aspect_ratio = (double)video_width_height.first / (double)video_width_height.second;
			double height = size.second;
			double width = size.first;
			if (width < 20 + 42 + 12 + 42 + 12 + 96 + 12 + 30 + 30 + 100)
			{
				width = 20 + 42 + 12 + 42 + 12 + 96 + 12 + 30 + 30 + 100;
			}
			height = width / aspect_ratio;
			size.first = (int)width;
			size.second = (int)height;
		}
		//

		screen_size(size.first,size.second);
		screen_position(pos.first,pos.second);
		screenui_show(true);
	});
}

void engine_drag_corner_end(corner_dir dir, int x, int y)
{

}

void engine_abort()
{
	DEBUG << "engine_abort";
	abort_request = true;
}

/* Called from the main */
int main(int argc, char **argv)
{
	log_init("nnkk.log");

	if( argc >= 2 )
	{
		if( string(argv[1]) == "-shutdown" )
		{
			if( screen_global_existing() )
			{
				SendMessage(FindWindow("NingNingKanKanScreen",NULL),WM_QUIT,0,0);
			}
			return 0;
		}
	}

	if( screen_global_existing() )
	{
		DEBUG << "another instance is running";
		return 0;
	}

	if( !userid_init() )
	{
		// really not a fatal error
	}

	if( !reporter_init() )
		return 1;

	if( ffplay_init(argc,argv) != 0 )
		return 1;

	if( !screen_create() )
		return 1;

	if( !parser_init() )
		return 1;

	if( !system_tray_init() )
		return 1;

	if( !sniffer_init() )
		return 1;

	if( !version_checker_init() )
	{
	}

	std::vector<std::string> urls;
	// tudou failed address
	//urls.push_back("http://222.73.92.102/sohu/5/232/120/IuWk4ITHiF71LjHdeFeC21.mp4?start=195&key=ySnOPovdsnzRwH7Q48TCGAX0cn_4TE7knx3TBg..&ch=tv&catcode=101104;101117;101122&plat=flash_Windows7");
	//urls.push_back("http://180.153.94.202/f4v/56/21107356.cgo.2.f4v?10000&key=5bfeeb2deaf2088296d0ca50cf66df0040966d0994&playtype=1&tk=151409832013195773110065479&brt=2&bc=0&nt=0&du=2985100&ispid=23&rc=200&inf=1&si=11000&npc=3379&pp=0&ul=2&mt=-1&sid=10000&au=0&pc=0&cip=222.73.44.31%20%20%20&hf=1&id=tudou&itemid=108729546&fi=21107356&sz=104422277");
	//urls.push_back("http://f.youku.com/player/getFlvPath/sid/135560810037110251230_00/st/flv/fileid/0300020100505E9685E96E019C3C1C0F0A5409-0D56-7FB3-A666-81E49C57E135?K=d7e1dff3a75479a028283880,k2:1d0b9ba543eb0b1df");
	//urls.push_back("http://hc.yinyuetai.com/uploads/videos/common/EDE1013B995F103D4A4B860584C79869.flv?sc=c192f6acdb4116eb");
	//urls.push_back("http://122.227.245.133/cdde9f149568153-1355779188-2061827213/data4/hc.yinyuetai.com/uploads/videos/common/EDE1013B995F103D4A4B860584C79869.flv?sc=c192f6acdb4116eb");
	//urls.push_back("yiyuetai_cantplay.mp4");
	//urls.push_back("sample2.flv");
	engine_open_stream_request_from_sniffer("",urls);

	system_tray_balloon("已经运行，请正常访问视频网站进行观看！\n版本 "MAIN_VERSION);


	reporter_report_login();
	{
		boost::asio::io_service::work wk(*main_thread_ioservice);

		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);

		std::shared_ptr<boost::asio::deadline_timer> timer = std::make_shared<boost::asio::deadline_timer>(*main_thread_ioservice);
		timer->expires_from_now(boost::posix_time::milliseconds(50));
		timer->async_wait( boost::bind( &idle , boost::asio::placeholders::error , timer ) );

		while( !abort_request )
		{
			if( !main_thread_ioservice->run_one() )
				boost::this_thread::sleep( boost::posix_time::milliseconds(10) );
		}
	}
	reporter_report_logout();

	version_checker_init();

	sniffer_uninit();

	system_tray_uninit();

	parser_uninit();

	ffplay_uninit();

	screen_close();

	reporter_uninit();

	userid_uninit();
	
    return 0;
}

////////////////////////////////
