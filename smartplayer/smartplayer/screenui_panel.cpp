#include "screenui.h"
#include "screenui_panel.h"
#include "engine.h"
#include "screen.h"
#include "libpng.h"
#include <SDL.h>

#include <Windows.h>

using namespace std;

//////
static SDL_Window * panel_window = 0;
static SDL_Renderer * panel_renderer = 0;
static SDL_Texture * panel_left_image_texture = 0;
static SDL_Texture * panel_middle_image_texture = 0;
static SDL_Texture * panel_right_image_texture = 0;
static HWND panel_hwnd_ = 0;

int panel_height = 64;
//////

static void render_panel()
{	
	pair<int,int> screen_size_ = screen_size();

	SDL_RenderClear(panel_renderer);

	SDL_Rect left_part;
	left_part.w = 24;
	left_part.h = 64;
	left_part.x = 0;
	left_part.y = 0;
	SDL_RenderCopy(panel_renderer,panel_left_image_texture,NULL,&left_part);

	SDL_Rect middle_part;
	middle_part.w = screen_size_.first - 24 - 24;
	middle_part.h = 64;
	middle_part.x = 24;
	middle_part.y = 0;
	SDL_RenderCopy(panel_renderer,panel_middle_image_texture,NULL,&middle_part);

	SDL_Rect right_part;
	right_part.w = 24;
	right_part.h = 64;
	right_part.x = screen_size_.first - 24;
	right_part.y = 0;
	SDL_RenderCopy(panel_renderer,panel_right_image_texture,NULL,&right_part);
	
	SDL_RenderPresent(panel_renderer);
}

static LRESULT CALLBACK panel_proc(HWND hwnd,UINT uMsg,WPARAM wParam,LPARAM lParam)
{ 
    switch (uMsg) 
    {
        case WM_CREATE:
			{
				panel_window = SDL_CreateWindowFrom((const void*)hwnd);
				panel_renderer = SDL_CreateRenderer(panel_window, -1, SDL_RENDERER_SOFTWARE);
				unsigned char * data;
				int width,height,depth,pitch;
				bool has_alpha;
				{
					load_png("res\\panel_left.png",width,height,has_alpha,depth,pitch,&data);
					SDL_Surface * s = SDL_CreateRGBSurfaceFrom((void*)data,width,height,depth,pitch,0xFF000000,0x00FF0000,0x0000FF00,0x000000FF);
					panel_left_image_texture = SDL_CreateTextureFromSurface(panel_renderer,s);
					free(data);
				}
				{
					load_png("res\\panel_middle.png",width,height,has_alpha,depth,pitch,&data);
					SDL_Surface * s = SDL_CreateRGBSurfaceFrom((void*)data,width,height,depth,pitch,0xFF000000,0x00FF0000,0x0000FF00,0x000000FF);
					panel_middle_image_texture = SDL_CreateTextureFromSurface(panel_renderer,s);
					free(data);
				}
				{
					load_png("res\\panel_right.png",width,height,has_alpha,depth,pitch,&data);
					SDL_Surface * s = SDL_CreateRGBSurfaceFrom((void*)data,width,height,depth,pitch,0xFF000000,0x00FF0000,0x0000FF00,0x000000FF);
					panel_right_image_texture = SDL_CreateTextureFromSurface(panel_renderer,s);
					free(data);
				}
				return DefWindowProc(hwnd, uMsg, wParam, lParam);
			}
		case WM_LBUTTONDOWN:
			PostMessage(*(HWND*)screen_hwnd(),uMsg,wParam,lParam);
			return 0;
		case WM_MOUSEMOVE:
			PostMessage(*(HWND*)screen_hwnd(),uMsg,wParam,lParam);
			return 0;
		case WM_LBUTTONUP:	
			PostMessage(*(HWND*)screen_hwnd(),uMsg,wParam,lParam);
			return 0;
		case WM_RBUTTONDOWN:
			screenui_show(true);
			return 0;
		case WM_RBUTTONUP:
			return 0;
		case WM_KEYDOWN:
			PostMessage(*(HWND*)screen_hwnd(),WM_KEYDOWN,wParam,lParam);
			return 0;
        case WM_PAINT:
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

void create_panel()
{
	WNDCLASS wc = {0};

	HINSTANCE inst = GetModuleHandle(0);

	wc.hbrBackground = /*GetSysColorBrush(COLOR_BTNFACE)*/(HBRUSH)GetStockObject(WHITE_BRUSH);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hInstance = inst;
	wc.lpfnWndProc = panel_proc;
	wc.lpszClassName = "NingNingKanKanScreenPanel";
    wc.cbClsExtra = 0;                // no extra class memory 
    wc.cbWndExtra = 0;                // no extra window memory 
	wc.style = 0;

	RegisterClass(&wc);

	panel_hwnd_ = CreateWindowEx(WS_EX_LAYERED | WS_EX_TOOLWINDOW, "NingNingKanKanScreenPanel", 0,WS_POPUP | WS_VISIBLE, 0, 0, 1, 1, *(HWND*)screen_hwnd(), 0, 0, 0);
	ShowWindow(panel_hwnd_,SW_NORMAL);
}

void move_panel()
{
	pair<int,int> screen_size_ = screen_size();

	pair<int,int> screen_position_ = screen_position();

	double full_screen_offset = 0;
	if( screen_fullscreen() )
	{
		full_screen_offset = - panel_height;
	}

	SDL_SetWindowPosition(panel_window,screen_position_.first, screen_position_.second + screen_size_.second + full_screen_offset);

	SDL_SetWindowSize(panel_window,screen_size_.first,panel_height);

	{
		static pair<int,int> last_screen_size_;
		if( last_screen_size_ != screen_size_ )
		{
			// need to redraw
			render_panel();
		}
		last_screen_size_ = screen_size_;
	}
}

void close_panel()
{
	DestroyWindow(panel_hwnd_);
}

void show_panel(bool show)
{
	if( show )
	{
		ShowWindow(panel_hwnd_, SW_SHOW);
	}
	else
	{
		ShowWindow(panel_hwnd_, SW_HIDE);
	}
	{
		// the first time render!
		static bool last_show = false;
		if( last_show != show && show )
			render_panel();
		last_show = show;
	}
}

void set_alpha_panel(float t)
{	
	SDL_SetWindowAlpha(panel_window, t);
	render_panel();
}

std::pair<int,int> panel_position()
{
	std::pair<int,int> ret;
	if( panel_window )
	{
		SDL_GetWindowPosition(panel_window,&ret.first,&ret.second);
	}
	return ret;
}

std::pair<int,int> panel_size()
{
	std::pair<int,int> ret;
	if( panel_window )
	{
		SDL_GetWindowSize(panel_window,&ret.first,&ret.second);
	}
	return ret;
}


const void * screenui_panel_hwnd()
{
	return &panel_hwnd_;
}

bool screenui_panel_cursor_near_test(int x,int y)
{
	const int border_width = 30;
	if( panel_hwnd_ )
	{
		auto sc = screen_size();
		auto sp = screen_position();

		POINT p = {x,y};
		//ScreenToClient(panel_hwnd_,&p);
		if( p.x >= sp.first-border_width && p.x <= sp.first+sc.first+border_width &&
			p.y >= sp.second+sc.second-panel_height-border_width && p.y <= sp.second+sc.second+panel_height+border_width )
			return true;

		if( p.x >= sp.first-border_width && p.x <= sp.first+sc.first+border_width &&
			p.y >= sp.second-panel_height-border_width && p.y <= sp.second+panel_height+border_width )
			return true;

		return false;
	}
	return false;
}
///////////////////