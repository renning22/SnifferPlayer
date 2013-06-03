#include "engine.h"
#include "ffplay.h"
#include "screen.h"
#include "screenui.h"
#include "screenui_panel.h"
#include "screenui_center_announcement.h"
#include "utility.hpp"

#include <deque>

#include <Windows.h>

using namespace std;

void gesture_idle()
{
	if( ffplay_episode_playing() )
	{
		POINT p;
		GetCursorPos(&p);

		bool mouse_left_down = GetAsyncKeyState(VK_LBUTTON)!=0;
		bool mouse_right_down = GetAsyncKeyState(VK_RBUTTON)!=0;

		pair<int,int> desktop_size = []()->pair<int,int>
		{
			RECT desktop;
			const HWND hDesktop = GetDesktopWindow();
			GetWindowRect(hDesktop, &desktop);
			return make_pair(desktop.right,desktop.bottom);
		}();
		
		bool cursor_inside = screen_cursor_inside_test(p.x,p.y);

		pair<int,int> screen_size_ = screen_size();

		pair<float,float> screen_size_normalized = [&]()->pair<float,float>
		{
			pair<float,float> ret;
			ret.first = (float)screen_size_.first / desktop_size.first;
			ret.second = (float)screen_size_.second / desktop_size.second;
			ret.first = min(1,ret.first);
			ret.first = max(-1,ret.first);
			ret.second = min(1,ret.second);
			ret.second = max(-1,ret.second);
			return ret;
		}();

		pair<int,int> screen_position_ = screen_position();

		float desktop_max_distance = [&]()->float
		{
			return sqrt((float)desktop_size.first*desktop_size.first+desktop_size.second*desktop_size.second);
		}();

		float screen_max_distance = [&]()->float
		{
			return sqrt((float)(screen_size_.first*screen_size_.first)+(screen_size_.second*screen_size_.second));
		}();

		std::pair<float,float> screen_center_distance_nomorlized = [&]()->std::pair<float,float>
		{
			std::pair<float,float> ret;
			std::pair<float,float> center;
			center.first = (float)(screen_position_.first + screen_size_.first/2) / desktop_size.first;
			center.second = (float)(screen_position_.second + screen_size_.second/2) / desktop_size.second;
			ret.first = (float)p.x / desktop_size.first;
			ret.second = (float)p.y / desktop_size.second;
			ret.first = ret.first - center.first;
			ret.second = ret.second - center.second;
			ret.first = min(1,ret.first);
			ret.first = max(-1,ret.first);
			ret.second = min(1,ret.second);
			ret.second = max(-1,ret.second);
			return ret;
		}();

		std::pair<float,float> screen_boundary_distance_nomorlized = [&]()->std::pair<float,float>
		{
			std::pair<float,float> ret;
			if( fabs(screen_center_distance_nomorlized.first) <= screen_size_normalized.first/2 )
				ret.first = 0;
			else
				ret.first = fabs(screen_center_distance_nomorlized.first) - screen_size_normalized.first/2;

			if( fabs(screen_center_distance_nomorlized.second) <= screen_size_normalized.second/2 )
				ret.second = 0;
			else
				ret.second = fabs(screen_center_distance_nomorlized.second) - screen_size_normalized.second/2;
			return ret;
		}();

		float screen_center_distance = [&]()->float
		{
			POINT wp;
			wp.x = screen_position_.first + screen_size_.first/2;
			wp.y = screen_position_.second + screen_size_.second/2;
			return sqrt((float)(p.x-wp.x)*(p.x-wp.x) + (p.y-wp.y)*(p.y-wp.y));
		}();

		float screen_boundary_manhattan_distance = [&]()->float
		{
			if( cursor_inside )
				return 0;
			float h = max( (screen_position_.first-screen_size_.first/2) - p.x , p.x - (screen_position_.first+screen_size_.first/2) );
			float v = max( (screen_position_.second-screen_size_.second/2) - p.y , p.y - (screen_position_.second+screen_size_.second/2) );
			return max(v,h);
		}();

		// cursors stable
		if( engine_screen_opaque_mode() )
		{
			static deque<pair<float,float>> cursor_traces;
			static bool last_very_stable = false;
			bool very_stable = true;
			
			if( cursor_traces.size() > 20 )
				cursor_traces.pop_front();
			cursor_traces.push_back( std::make_pair((float)p.x,(float)p.y) );

			if( screen_fullscreen() )
			{
				for(auto i=cursor_traces.begin();i!=cursor_traces.end();i++)
				{
					if( i == cursor_traces.begin() )
						continue;
					auto j = i-1;
					if( *i != *j )
					{
						very_stable = false;
						break;
					}
				}
			}
			else
			{
				if( screenui_panel_cursor_near_test(p.x,p.y) )
				{
					very_stable = false;
				}
				else
				{
					very_stable = true;
				}
			}


			if( very_stable )
			{
				// very stable mouse
				screenui_set_opaque(false);
			}
			else
			{
				screenui_set_opaque(true);
			}
			last_very_stable = very_stable;
		}

		// dont alter , 2012-12-27
		/*
		static deque<pair<float,float>> cursor_traces;
		static int cursors_outside_successive_frames = 0;
		static int cursors_inside_successive_frames = 0;
		if( cursor_inside )
		{
			cursors_outside_successive_frames = 0;
			cursors_inside_successive_frames++;

			if( cursor_traces.size() > 10 )
				cursor_traces.pop_front();
			cursor_traces.push_back( std::make_pair((float)p.x,(float)p.y) );

			float energe = 0;
			int corners = 0;
			pair<float,float> last_vec;
			for(auto i=cursor_traces.begin();i!=cursor_traces.end();i++)
			{
				if( i == cursor_traces.begin() )
					continue;
				auto j = i-1;
				energe += sqrt((float)(i->first-j->first)*(i->first-j->first)+(i->second-j->second)*(i->second-j->second));

				pair<float,float> this_vec = make_pair( i->first-j->first , i->second-j->second );
				if( last_vec.first != 0 || last_vec.second != 0 )
				{
					float dp = this_vec.first*last_vec.first + this_vec.second*last_vec.second;
					if( dp < 0.1f )
						corners++;
				}
				last_vec = this_vec;
			}
			if( energe > 0.8f*screen_max_distance &&
				corners >= 2 )
			{
				if( !engine_screen_opaque_mode() )
				{
					engine_screen_opaque_mode(true);
				}
			}
				
			//printf("%d , %f\n",corners,1.0f*screen_max_distance);
		}
		else
		{
			cursors_outside_successive_frames++;
			cursors_inside_successive_frames = 0;

			if( cursor_traces.size() )
				cursor_traces.pop_front();

			if( cursors_outside_successive_frames >= 10 )
			{
				if( engine_screen_opaque_mode() )
				{
					engine_screen_opaque_mode(false);
				}
			}
		}
		*/

		// change alpha
		{
			float target_alpha = 0;
			if( !engine_screen_opaque_mode() )
			{
				bool always_transparant = mouse_left_down || mouse_right_down;
				always_transparant = false;
				static int almost_transparent_successive_frames = 0;
				float base_alpha = [&]()->float
				{
					float x;
					if( always_transparant )
						x = 0;
					else
						x = (float)almost_transparent_successive_frames / 120;
					return 0.20f*exp(-x*x);
				}();

				{
					//old alg
					//target_alpha = pow(screen_center_distance/desktop_max_distance,0.25f);

					if( always_transparant )
						target_alpha = base_alpha;
					else
					{
						float x = max(screen_boundary_distance_nomorlized.first,screen_boundary_distance_nomorlized.second) / 0.120;
						target_alpha = min(1 , 1-exp(-x*x) + base_alpha );
					}
				}

				if( target_alpha <= 0.30f )
					almost_transparent_successive_frames++;
				else
					almost_transparent_successive_frames=0;

				screenui_set_opaque(false);
			}
			else
			{
				target_alpha = 1.0f;
				/*
				if (abs(p.y - (screen_position_.second + screen_size_.second)) <= 100)
				{
					screenui_set_opaque(true);
				}
				else
				{
					screenui_set_opaque(false);
				}
				*/
			}
			change_gradually(
			[]()->float{return screen_alpha();},
			[](float t){screen_alpha(t);},
			target_alpha,
			0.001f,
			0.3f);

			change_gradually(
			[]()->float{return center_announcement_alpha();},
			[](float t){center_announcement_alpha(t);},
			target_alpha*0.60f,
			0.001f,
			0.3f);
		}


		// screen corner
		{
			if( screen_fullscreen() )
			{
				screen_corner_clip(false);
			}
			else
			{
				screen_corner_clip(true);
			}
		}
	}

}