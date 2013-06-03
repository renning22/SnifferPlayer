#include "screenui_time_span.h"
#include "screenui_panel.h"
#include "engine.h"
#include "ffplay.h"
#include "screen.h"
#include "libpng.h"
#include <SDL.h>
#include <string>
#include <sstream>
#include <Windows.h>

#include <boost/lexical_cast.hpp>

using namespace std;

//////
static HWND time_span_hwnd = 0;

static std::string content;
//////

void time_span_set_caption(std::string _content)
{
	content = _content;
}

void time_span_set_caption_two_times_format(double a,double b)
{
	std::string total_duration_minutes = boost::lexical_cast<std::string>((int)b/60);
	std::string total_duration_seconds_0 = boost::lexical_cast<std::string>((int)b%60/10);
	std::string total_duration_seconds_1 = boost::lexical_cast<std::string>((int)b%60%10);
	std::string current_pos_minutes = boost::lexical_cast<std::string>((int)a/60);
	std::string current_pos_seconds_0 = boost::lexical_cast<std::string>((int)a%60/10);
	std::string current_pos_seconds_1 = boost::lexical_cast<std::string>((int)a%60%10);

	time_span_set_caption(current_pos_minutes + ":" + current_pos_seconds_0 + current_pos_seconds_1 + "/" + total_duration_minutes + ":" + total_duration_seconds_0 + total_duration_seconds_1);
}

static void render_time_span()
{
	::InvalidateRect(time_span_hwnd, NULL, TRUE);
}

static LRESULT CALLBACK time_span_proc(HWND hwnd,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
	static HFONT hFont = NULL;
    switch (uMsg) 
    {
        case WM_CREATE:
			hFont = CreateFont(21,8,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY, VARIABLE_PITCH,TEXT("Times New Roman"));
			return DefWindowProc(hwnd, uMsg, wParam, lParam);
		case WM_LBUTTONDOWN:
			return DefWindowProc(hwnd, uMsg, wParam, lParam);
		case WM_LBUTTONUP:
			return DefWindowProc(hwnd, uMsg, wParam, lParam);
		case WM_KEYDOWN:
			PostMessage(*(HWND*)screen_hwnd(),WM_KEYDOWN,wParam,lParam);
			return 0;
        case WM_PAINT:
			{
				PAINTSTRUCT ps;
				HDC hDC = BeginPaint(hwnd, &ps);

				RECT rect;
				GetClientRect(hwnd, &rect);

				SelectObject(hDC, hFont);
            				
				SetTextColor(hDC, RGB(0,0,0));

				DrawText(hDC, content.c_str(), -1, &rect, DT_CENTER | DT_VCENTER);

				EndPaint(hwnd, &ps);
			}
			return 0;
        case WM_SIZE: 
            // Set the size and position of the window. 
            return 0; 
        case WM_DESTROY:
			return DefWindowProc(hwnd, uMsg, wParam, lParam);
		default:
			return DefWindowProc(hwnd, uMsg, wParam, lParam);
    } 
}

void create_time_span()
{
	WNDCLASS wc = {0};

	HINSTANCE inst = GetModuleHandle(0);

	wc.hbrBackground = /*GetSysColorBrush(COLOR_BTNFACE)*/(HBRUSH)GetStockObject(WHITE_BRUSH);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hInstance = inst;
	wc.lpfnWndProc = time_span_proc;
	wc.lpszClassName = "NingNingKanKanScreenTimeSpan";
    wc.cbClsExtra = 0;                // no extra class memory 
    wc.cbWndExtra = 0;                // no extra window memory 
	wc.style = 0;

	RegisterClass(&wc);

	time_span_hwnd = CreateWindowEx(WS_EX_LAYERED | WS_EX_TOOLWINDOW, "NingNingKanKanScreenTimeSpan", 0,WS_POPUP | WS_VISIBLE, 0, 0, 1, 1, *(HWND*)screenui_panel_hwnd(), 0, 0, 0);
	
	ShowWindow(time_span_hwnd,SW_NORMAL);
}



void move_time_span()
{
	pair<int,int> panel_size_ = panel_size();

	pair<int,int> panel_position_ = panel_position();

	SetWindowPos(time_span_hwnd,NULL,panel_position_.first + 20 + 42 + 12,panel_position_.second + 20,96,24,SWP_NOZORDER | SWP_NOCOPYBITS);
}

void close_time_span()
{
	DestroyWindow(time_span_hwnd);
}

void show_time_span(bool show)
{
	if( show )
	{
		ShowWindow(time_span_hwnd, SW_SHOW);
	}
	else
	{
		ShowWindow(time_span_hwnd, SW_HIDE);
	}
}

void set_alpha_time_span(float t)
{
	SetLayeredWindowAttributes(time_span_hwnd, RGB(255,255,255), (uint8_t)255*t*0.8, LWA_ALPHA/* | LWA_COLORKEY*/);
	render_time_span();
}

void time_span_idle()
{
	if( ffplay_episode_playing() )
	{
		static int frame_count = 0;
		frame_count++;
		if( frame_count % 10 == 0 )
		{
			time_span_set_caption_two_times_format(ffplay_episode_current_playing_pos(),ffplay_episode_duration());
			render_time_span();
		}
	}
}
///////////////////