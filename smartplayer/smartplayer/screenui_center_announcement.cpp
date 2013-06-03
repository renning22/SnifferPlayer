#include "screenui_panel.h"
#include "screen.h"
#include "engine.h"
#include "libpng.h"
#include <Windows.h>
#include <string>
#include <utility>
#include <SDL.h>
#include <boost/lexical_cast.hpp>
#include <boost/thread.hpp>

using namespace std;

//////
static boost::recursive_mutex lock;

static HWND center_announcement_hwnd = 0;

static int center_announcement_width = 100;
static int center_announcement_height = 25;

static std::string content;

static float last_alpha = 0;
//////


static void render_center_announcement()
{
	::InvalidateRect(center_announcement_hwnd, NULL, TRUE);
}

static LRESULT CALLBACK center_announcement_proc(HWND hwnd,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
	static HFONT hFont = NULL;
    switch (uMsg) 
    {
        case WM_CREATE:
			hFont = CreateFont(21,8,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY, VARIABLE_PITCH,TEXT("Times New Roman"));
			return DefWindowProc(hwnd, uMsg, wParam, lParam);
		case WM_LBUTTONDOWN:
			PostMessage(*(HWND*)screen_hwnd(),WM_KEYDOWN,wParam,lParam);
			return 0;
		case WM_LBUTTONUP:
			PostMessage(*(HWND*)screen_hwnd(),WM_KEYDOWN,wParam,lParam);
			return 0;
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


void create_center_announcement()
{
	WNDCLASS wc = {0};

	HINSTANCE inst = GetModuleHandle(0);

	wc.hbrBackground = /*GetSysColorBrush(COLOR_BTNFACE)*/(HBRUSH)GetStockObject(WHITE_BRUSH);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hInstance = inst;
	wc.lpfnWndProc = center_announcement_proc;
	wc.lpszClassName = "NingNingKanKanScreenCenterAnnouncement";
    wc.cbClsExtra = 0;                // no extra class memory 
    wc.cbWndExtra = 0;                // no extra window memory 
	wc.style = 0;

	RegisterClass(&wc);

	center_announcement_hwnd = CreateWindowEx(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW, "NingNingKanKanScreenCenterAnnouncement", 0,WS_POPUP | WS_VISIBLE, 0, 0, 1, 1, *(HWND*)screen_hwnd(), 0, 0, 0);
	
	ShowWindow(center_announcement_hwnd,SW_NORMAL);

}

void close_center_announcement()
{
	DestroyWindow(center_announcement_hwnd);
}

void move_center_announcement()
{
	pair<int,int> screen_size_ = screen_size();

	pair<int,int> screen_position_ = screen_position();

	SetWindowPos(center_announcement_hwnd,NULL,screen_position_.first+screen_size_.first/2-center_announcement_width/2,screen_position_.second+screen_size_.second/2-center_announcement_height/2,center_announcement_width,center_announcement_height,SWP_NOZORDER | SWP_NOCOPYBITS);
}

void center_announcement_show(bool show)
{
	if( show )
	{
		ShowWindow(center_announcement_hwnd, SW_SHOW);
	}
	else
	{
		ShowWindow(center_announcement_hwnd, SW_HIDE);
	}
}

void center_announcement_alpha(float t)
{
	boost::lock_guard<decltype(lock)> guard(lock);
	if( t == last_alpha )
		return;
	last_alpha = t;
	SetLayeredWindowAttributes(center_announcement_hwnd, 0, (uint8_t)255*t, LWA_ALPHA);
	render_center_announcement();
}

float center_announcement_alpha()
{
	return last_alpha;
}

void center_announcement_caption(std::string _content)
{
	boost::lock_guard<decltype(lock)> guard(lock);
	content = _content;
	render_center_announcement();
}

void center_annoucement_idle()
{


}