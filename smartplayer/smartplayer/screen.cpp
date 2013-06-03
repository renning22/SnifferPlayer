#include "engine.h"
#include "screen.h"
#include "system_tray.h"
// quote this only for UI create/close window in screen thread or no work with OS.
#include "screenui.h"
#include "utility.hpp"
#include "website.h"

extern "C"
{
#include "libswresample/swresample.h"
#include "libswscale/swscale.h"
#include "libavformat/avformat.h"
}

#include <SDL.h>
#include <SDL_thread.h>
#include <SDL_surface.h>

#include <boost/thread.hpp>
#include <windows.h>
#include <Windowsx.h>

#include "log.h"
/////

#define MINIMIZE_WINDOW_SIZE 300

static boost::recursive_mutex lock;

static SDL_Window * window = 0;
static HWND window_hwnd = 0;
static boost::thread * window_thread;
static SDL_Renderer * renderer = 0;
static SDL_Surface * surface_during_drawing = 0;

static bool screen_topmost_ = true;

static bool corner_clip = true;
/////

static LRESULT CALLBACK ffplay_screen_proc(HWND hwnd,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
	using namespace std;
    switch(uMsg)
    {
		case WM_USER+1:
		{	
			// system tray
			if( wParam == 0 )
			{
				switch(lParam)
				{
				case 1029:
					// go to website
					ShellExecute(hwnd, NULL, website_download_url().c_str(), NULL, NULL, SW_SHOWNORMAL);
					break;
				case WM_LBUTTONDOWN:
					break;
				case WM_LBUTTONUP:
					system_tray_left_button_up();
					break;
				case WM_LBUTTONDBLCLK:
					system_tray_item_selected(2);
					break;
				case WM_MOUSEMOVE:
					break;
				case WM_RBUTTONDOWN:
					break;
				case WM_RBUTTONUP:
					system_tray_right_button_up();
					break;
				case WM_RBUTTONDBLCLK:
					system_tray_item_selected(1);
					break;
				}
			}
		}
		return 0;
		case WM_QUIT:
		{
			engine_abort();
		}
		return 0;
		case WM_NCHITTEST:
		{
			auto ret = DefWindowProc(hwnd, uMsg, wParam, lParam);
			if( ret >= HTLEFT && ret <= HTBOTTOMRIGHT )
				return ret;
			else
				return HTCAPTION;
			/*
			const int border = 5;
			POINT p = {GET_X_LPARAM(lParam),GET_Y_LPARAM(lParam)};
			RECT client;
			GetClientRect(hwnd,&client);
			ScreenToClient(hwnd,&p);
			bool up = abs(client.top-p.y) < border;
			bool bottom = abs(client.bottom-p.y) < border;
			bool left = abs(client.left-p.x) < border;
			bool right = abs(client.right-p.x) < border;
			if( up )
				return HTTOP;
			else if( bottom )
				return HTBOTTOM;
			else if( left )
				return HTLEFT;
			else if( right )
				return HTRIGHT;
			else
				return HTCAPTION;
			*/
		}
		return 0;
		case WM_LBUTTONDOWN:
		{
			engine_screen_mouse_left_down();
		}
		return 0;
		case WM_LBUTTONUP:
		{
			engine_screen_mouse_left_up();
		}
		return 0;
		case WM_LBUTTONDBLCLK:
		case WM_NCLBUTTONDBLCLK:
		{
			engine_screen_toggle_maximize_from_system();
		}
		return 0;
		case WM_RBUTTONDOWN:
		case WM_NCRBUTTONDOWN:
		{
			system_tray_right_button_up();
		}
		return 0;
		case WM_RBUTTONDBLCLK:
		{
			engine_screen_toggle_maximize_from_system();
		}
		return 0;
		case WM_MOUSEMOVE:
		{
			engine_screen_mouse_move();
		}
		return 0;
		case WM_KEYDOWN:
		{
			switch(wParam)
			{
			case VK_LEFT:
				engine_screen_keyboard_left();
				break;
			case VK_RIGHT:
				engine_screen_keyboard_right();
				break;
			case VK_EXECUTE:
			case VK_RETURN:
				engine_screen_toggle_maximize_from_system();
				break;
			case VK_ESCAPE:
				if( screen_fullscreen() )
				{
					engine_screen_toggle_maximize_from_system();
				}
				break;
			case VK_SPACE:
			case VK_MEDIA_PLAY_PAUSE:
			case VK_PLAY:
				engine_play_pause_from_screenui();
				break;
			case VK_MEDIA_STOP:
				engine_screen_close_from_screenui();
				break;
			case VK_MEDIA_NEXT_TRACK:
				engine_next_from_screenui();
				break;
			case VK_MEDIA_PREV_TRACK:
				break;
			case VK_VOLUME_UP:
				break;
			case VK_VOLUME_DOWN:
				break;
			}
		}
		return 0;
		case WM_WINDOWPOSCHANGING:
		{
			boost::lock_guard<decltype(lock)> guard(lock);
			WINDOWPOS * wp = reinterpret_cast<WINDOWPOS*>(lParam);
			DEBUG << "WM_WINDOWPOSCHANGING : " << std::hex << wp->hwnd << " , " << std::oct << wp->x << " , " << wp->y << " , " << wp->cx << " , " << wp->cy << " , " << std::hex << wp->hwndInsertAfter;
			if( !(wp->flags&SWP_NOMOVE) )
				SDL_SetWindowPosition(window,wp->x,wp->y);
			if( !(wp->flags&SWP_NOSIZE) )
			{
				if( wp->cx < MINIMIZE_WINDOW_SIZE )
					wp->cx = MINIMIZE_WINDOW_SIZE;
				if( wp->cy <  MINIMIZE_WINDOW_SIZE )
					wp->cy = MINIMIZE_WINDOW_SIZE;
				SDL_SetWindowSize(window,wp->cx,wp->cy);
			}
		}
		return 0;
		case WM_WINDOWPOSCHANGED:
		{
			DEBUG << "WM_WINDOWPOSCHANGED";
			engine_screen_position_changed_from_system();
		}
		return 0;
		case WM_MOVE:
		{
			DEBUG << "WM_MOVE";
			SetWindowPos(window_hwnd,0,GET_X_LPARAM(lParam),GET_Y_LPARAM(lParam),0,0,SWP_NOSIZE | SWP_NOZORDER | SWP_NOCOPYBITS);
		}
		return 0;
		case WM_SIZE:
		{
			DEBUG << "WM_SIZE";
			SetWindowPos(window_hwnd,0,0,0,GET_X_LPARAM(lParam),GET_Y_LPARAM(lParam),SWP_NOMOVE | SWP_NOZORDER | SWP_NOCOPYBITS);
		}
		return 0;
		case WM_COMMAND:
		{
			if( HIWORD(wParam) == 0 )
			{
				system_tray_item_selected(LOWORD(wParam));
			}
		}
		return DefWindowProc(hwnd, uMsg, wParam, lParam);
		case WM_SYSCOMMAND:
		{
			switch(GET_SC_WPARAM(wParam))
			{
			case SC_CLOSE:
				engine_screen_close_from_system();
				return 0;
			case SC_MINIMIZE:
				//engine_screen_toggle_minimize_from_system();
				break;
			case SC_MAXIMIZE:
				DEBUG << "SC_MAXIMIZE";
				//engine_screen_toggle_maximize_from_system();
				break;
			case SC_RESTORE:
				DEBUG << "SC_RESTORE";
				//engine_screen_restore_from_system();
				break;
			case SC_MOVE:
				DEBUG << "SC_MOVE";
				break;
			case SC_SIZE:
				DEBUG << "SC_SIZE";
				break;
			case SC_KEYMENU:
				break;
			case SC_ARRANGE:
				DEBUG << "SC_ARRANGE";
				break;
			}
		}
		{
			bool ret = DefWindowProc(hwnd, uMsg, wParam, lParam);
			DEBUG << " return = " << ret;
			return ret;
		}
		default:
			return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}
}

static bool screen_window_create()
{ 
	boost::lock_guard<decltype(lock)> guard(lock);

	if( window )
	{
		return false;
	}

	WNDCLASS wc = {0};

	HINSTANCE inst = GetModuleHandle(0);

	wc.hbrBackground = /*GetSysColorBrush(COLOR_BTNFACE)*/(HBRUSH)GetStockObject(WHITE_BRUSH);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hInstance = inst;
	wc.lpfnWndProc = ffplay_screen_proc;
	wc.lpszClassName = "NingNingKanKanScreen";
    wc.cbClsExtra = 0;                // no extra class memory 
    wc.cbWndExtra = 0;                // no extra window memory 
	wc.style = CS_DBLCLKS;
	wc.hIcon = (HICON)LoadImage(NULL,"res//system_tray.ico",IMAGE_ICON,0,0,LR_LOADFROMFILE | LR_SHARED);

	RegisterClass(&wc);

	window_hwnd = CreateWindowEx(/*WS_EX_TOOLWINDOW |*/ WS_EX_TOPMOST | WS_EX_LAYERED, "NingNingKanKanScreen", "ÄþÄþ¿´¿´", /*WS_POPUP |*/ WS_OVERLAPPED | WS_SYSMENU | WS_SIZEBOX | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_VISIBLE, 0, 0, MINIMIZE_WINDOW_SIZE, MINIMIZE_WINDOW_SIZE, 0, 0, 0, 0);

	if( !window_hwnd )
	{
        ERROR <<  "SDL_CreateWindow() , " << SDL_GetError();
        return false;
	}

	ShowWindow(window_hwnd, SW_NORMAL);

	window = SDL_CreateWindowFrom((const void*)window_hwnd);
    if( !window )
	{
        ERROR << "SDL_CreateWindow() , " << SDL_GetError();
        return false;
    }

	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if( !renderer )
	{
        ERROR << "SDL_CreateRenderer() , " << SDL_GetError();
		screen_close();
		return false;
    }

    return true;
}

bool screen_create()
{
	static bool init_finished_signal = false;
	static bool init_failed_signal = false;
	window_thread = new boost::thread([&]()
	{
		DEBUG << "UI thread started";
		if( !screen_window_create() )
		{
			printf("screen_window_create() failed.\n");
			init_failed_signal = true;
			return;
		}

		screen_show(false);

		screenui_create();
		screenui_show(false);

		init_finished_signal = true;

		BOOL bRet;
		MSG msg;
		while( window && window_hwnd && (bRet = GetMessage( &msg, 0, 0, 0 )) != 0)
		{
			if (bRet == -1)
			{
				// handle the error and possibly exit
				screenui_close();
				break;
			}
			else
			{
				TranslateMessage(&msg); 
				DispatchMessage(&msg); 
			}
		}
		DEBUG << "UI thread ended";
	});

	// block untill finish
	block_wait([&](){ return init_finished_signal; },
			   [&](){ return init_failed_signal; },
			   [&](){ SDL_Delay(50); });
	return true;
}

bool screen_close()
{
	boost::lock_guard<decltype(lock)> guard(lock);

	if( !window )
		return true;

	if( renderer )
	{
		SDL_DestroyRenderer(renderer);
		renderer = NULL;
	}

	if( window_hwnd )
		PostMessage(window_hwnd,WM_QUIT,0,0);

	if( window )
	{
		SDL_DestroyWindow(window);
		window = NULL;
		window_hwnd = NULL;
	}

	if( window_thread )
	{
		window_thread->join();
		delete window_thread;
		window_thread = 0;
	}
	return 0;
}

bool screen_global_existing()
{
	boost::lock_guard<decltype(lock)> guard(lock);
	return FindWindow("NingNingKanKanScreen",NULL)!=0;
}

void screen_show(bool show)
{
	//boost::lock_guard<decltype(lock)> guard(lock);
	if( window )
	{
		if( show )
		{
			screen_toggle_restore();
			ShowWindow(window_hwnd, SW_SHOW);
		}
		else
		{
			ShowWindow(window_hwnd, SW_HIDE);
		}
	}
}

bool screen_cursor_inside_test(int x,int y)
{
	boost::lock_guard<decltype(lock)> guard(lock);
	if( window && window_hwnd )
	{
		std::pair<int,int> sc = screen_size();
		POINT p = {x,y};
		ScreenToClient(window_hwnd,&p);
		if( p.x < 0 || p.x >= sc.first ||
			p.y < 0 || p.y >= sc.second )
			return false;
		else
			return true;
	}
	return false;
}

void screen_position(int x,int y)
{
	boost::lock_guard<decltype(lock)> guard(lock);
	if( window )
	{
		PostMessage(window_hwnd,WM_MOVE,SIZE_RESTORED,MAKELPARAM(x,y));
		//SetWindowPos(window_hwnd,0,x,y,0,0,SWP_NOSIZE | SWP_NOZORDER | SWP_NOCOPYBITS);
	}
}


std::pair<int,int> screen_position()
{
	boost::lock_guard<decltype(lock)> guard(lock);
	std::pair<int,int> ret;
	if( window )
	{
		SDL_GetWindowPosition(window,&ret.first,&ret.second);
	}
	return ret;
}

void screen_size(int width,int height,bool raletive_center)
{
	boost::lock_guard<decltype(lock)> guard(lock);
	if( window )
	{
		// please keep center centered
		if( raletive_center )
		{
			auto old_pos = screen_position();
			auto old_size = screen_size();
			auto displacement = std::make_pair( width-old_size.first , height-old_size.second );
			screen_position(old_pos.first-displacement.first/2,old_pos.second-displacement.second/2);
		}
		PostMessage(window_hwnd,WM_SIZE,SIZE_RESTORED,MAKELPARAM(width,height));
		//SetWindowPos(window_hwnd,0,0,0,width,height,SWP_NOMOVE | SWP_NOZORDER | SWP_NOCOPYBITS);
	}
}


std::pair<int,int> screen_size()
{
	boost::lock_guard<decltype(lock)> guard(lock);
	std::pair<int,int> ret;
	if( window )
	{
		SDL_GetWindowSize(window,&ret.first,&ret.second);
	}
	return ret;
}

// u see what different with screen_size
void screen_size_gracefully(int width,int height)
{
	boost::lock_guard<decltype(lock)> guard(lock);
	auto ss = screen_size();
	if( ss.first <= MINIMIZE_WINDOW_SIZE || ss.second <= MINIMIZE_WINDOW_SIZE )	// if no insane size
	{
		screen_size(width,height);
	}
	else
	{
		double old_aspect_ratio = (double)ss.first/(double)ss.second;
		double new_aspect_ratio = (double)width/(double)height;
		double ratio_ratio = new_aspect_ratio - old_aspect_ratio;
		if( fabs(ratio_ratio) > 0.3 )
		{
			DEBUG << "screen_size_gracefully : ratio_ratio diff too large , " << old_aspect_ratio << " to " << new_aspect_ratio;
			screen_size(width,height);
		}
	}
}


float screen_alpha()
{
	boost::lock_guard<decltype(lock)> guard(lock);
	if( window )
		return SDL_GetWindowAlpha(window);
	return 0;
}

void screen_alpha(float alpha)
{
	boost::lock_guard<decltype(lock)> guard(lock);
	if( window )
	{
		SDL_SetWindowAlpha(window,alpha);
	}
}

void screen_corner_clip(bool clip)
{
	corner_clip = clip;
}

void screen_set_opaque_mode(bool opaque)
{
	boost::lock_guard<decltype(lock)> guard(lock);
	if( window && window_hwnd )
	{
		LONG old_style = GetWindowLong(window_hwnd,GWL_EXSTYLE);
		if( !opaque )
		{
			SetWindowLong(window_hwnd,GWL_EXSTYLE,old_style | WS_EX_TRANSPARENT);
		}
		else
		{
			SetWindowLong(window_hwnd,GWL_EXSTYLE,old_style & (~(LONG)WS_EX_TRANSPARENT));
		}
	}
}

void screen_center_in_monitor()
{
	boost::lock_guard<decltype(lock)> guard(lock);
	if( window && window_hwnd )
	{
		MONITORINFO mi = {0};
		mi.cbSize = sizeof(mi);
		GetMonitorInfo(MonitorFromWindow(window_hwnd,MONITOR_DEFAULTTOPRIMARY), &mi);
		auto old_size = screen_size();
		auto monitor_center = std::make_pair((mi.rcMonitor.left+mi.rcMonitor.right)/2,(mi.rcMonitor.top+mi.rcMonitor.bottom)/2);
		screen_position(monitor_center.first - old_size.first/2, monitor_center.second - old_size.second/2);
	}
}

void screen_toggle_fullscreen()
{
	boost::lock_guard<decltype(lock)> guard(lock);
	static std::pair<int,int> remembered_position;
	static std::pair<int,int> remembered_size;
	if( window && window_hwnd )
	{
		/*
		if( screen_fullscreen() )
		{
			screen_position(remembered_position.first, remembered_position.second);
			screen_size(remembered_size.first,remembered_size.second,false);

			DWORD oldstyle = GetWindowLong(window_hwnd,GWL_STYLE);
			SetWindowLong(window_hwnd, GWL_STYLE,oldstyle & ~WS_MAXIMIZE);
		}
		else
		{
			remembered_position = screen_position();
			remembered_size = screen_size();

			MONITORINFO mi = {0};
			mi.cbSize = sizeof(mi);
			GetMonitorInfo(MonitorFromWindow(window_hwnd,MONITOR_DEFAULTTOPRIMARY), &mi);
			screen_position(mi.rcMonitor.left, mi.rcMonitor.top);
			screen_size(mi.rcMonitor.right - mi.rcMonitor.left,mi.rcMonitor.bottom - mi.rcMonitor.top,false);

			DWORD oldstyle = GetWindowLong(window_hwnd,GWL_STYLE);
			SetWindowLong(window_hwnd, GWL_STYLE,oldstyle | WS_MAXIMIZE);
		}
		*/

		if( screen_fullscreen() )
		{
			PostMessage(window_hwnd,WM_SYSCOMMAND,SC_RESTORE,0);
		}
		else
		{
			PostMessage(window_hwnd,WM_SYSCOMMAND,SC_MAXIMIZE,0);
		}
	}
}

bool screen_fullscreen()
{
	boost::lock_guard<decltype(lock)> guard(lock);
	bool ret=false;
	if( window_hwnd )
	{
		DWORD oldstyle = GetWindowLong(window_hwnd,GWL_STYLE);
		ret = oldstyle & WS_MAXIMIZE;
	}
	return ret;
}

bool screen_minimized()
{
	boost::lock_guard<decltype(lock)> guard(lock);
	bool ret=false;
	if( window_hwnd )
	{
		DWORD oldstyle = GetWindowLong(window_hwnd,GWL_STYLE);
		ret = oldstyle & WS_MINIMIZE;
	}
	return ret;
}

void screen_toggle_minimize()
{
	boost::lock_guard<decltype(lock)> guard(lock);
	if( window_hwnd )
	{
		if( screen_minimized() )
		{
			PostMessage(window_hwnd,WM_SYSCOMMAND,SC_RESTORE,0);
		}
		else
		{
			PostMessage(window_hwnd,WM_SYSCOMMAND,SC_MINIMIZE,0);
		}
	}
}

void screen_toggle_restore()
{
	boost::lock_guard<decltype(lock)> guard(lock);
	if( screen_fullscreen() )
	{
		screen_toggle_fullscreen();
	}
	else if( screen_minimized() )
	{
		screen_toggle_minimize();
	}
}

bool screen_topmost()
{
	return screen_topmost_;
}

void screen_toggle_topmost()
{
	boost::lock_guard<decltype(lock)> guard(lock);
	screen_topmost_ = !screen_topmost_;
	if( screen_topmost_ )
	{
		SetWindowPos(window_hwnd,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE | SWP_NOSIZE | SWP_NOCOPYBITS | SWP_NOSENDCHANGING);
	}
	else
	{
		SetWindowPos(window_hwnd,HWND_NOTOPMOST,0,0,0,0,SWP_NOMOVE | SWP_NOSIZE | SWP_NOCOPYBITS | SWP_NOSENDCHANGING);
	}
}

const void * screen_hwnd()
{
	return &window_hwnd;
}

boost::tuple<uint8_t*,int,int,int> screen_draw_begin()
{
	boost::tuple<uint8_t*,int,int,int> ret;
	lock.lock();
	surface_during_drawing = SDL_GetWindowSurface(window);
	if( surface_during_drawing )
	{
		SDL_LockSurface(surface_during_drawing);
		ret.get<0>() = (uint8_t*)surface_during_drawing->pixels;
		ret.get<1>() = surface_during_drawing->pitch;
		ret.get<2>() = surface_during_drawing->w;
		ret.get<3>() = surface_during_drawing->h;
		return ret;
	}
	else
	{
		lock.unlock();
		return ret;
	}
}

void screen_draw_end()
{
	if( corner_clip )
	{
		static int radius = 20;
		radius = min(20, min(surface_during_drawing->w,surface_during_drawing->h));

		std::size_t pitch = surface_during_drawing->pitch;
		std::size_t depth = pitch/surface_during_drawing->w;
		uint8_t * data = (uint8_t*)surface_during_drawing->pixels;

		auto func = [&](std::size_t sx,std::size_t sy,int dx,int dy)
		{
			for(std::size_t x=0;x<radius;x++)
			for(std::size_t y=0;y<radius;y++)
			{
				std::size_t nx = sx+x*dx;
				std::size_t ny = sy+y*dy;
				if( x*x + y*y > radius*radius )
				{
					data[ny*pitch + nx*depth + 3] = 0;
					data[ny*pitch + nx*depth + 0] = 0;
					data[ny*pitch + nx*depth + 1] = 0;
					data[ny*pitch + nx*depth + 2] = 0;
				}
			}
		};

		func(radius-1,radius-1,-1,-1);
		func(radius-1,surface_during_drawing->h-radius,-1,1);
		func(surface_during_drawing->w-radius,radius-1,1,-1);
		func(surface_during_drawing->w-radius,surface_during_drawing->h-radius,1,1);
	}

	SDL_UnlockSurface(surface_during_drawing);

	SDL_RenderPresent(renderer);
	lock.unlock();
}