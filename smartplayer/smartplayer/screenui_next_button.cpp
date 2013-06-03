#include "screenui_next_button.h"
#include "screenui_panel.h"
#include "engine.h"
#include "screen.h"
#include "libpng.h"
#include <SDL.h>

#include <Windows.h>

using namespace std;

//////
static SDL_Window * next_button_window = 0;
static SDL_Renderer * next_button_renderer = 0;
static SDL_Texture * next_button_image_texture = 0;
static SDL_Texture * pause_button_image_texture = 0;
static HWND next_button_hwnd = 0;

int next_button_width = 42;
int next_button_height = 42;

//////

static void render_next_button()
{
	SDL_RenderClear(next_button_renderer);
	SDL_RenderCopy(next_button_renderer,next_button_image_texture,NULL,NULL);
	SDL_RenderPresent(next_button_renderer);
}

static LRESULT CALLBACK next_button_proc(HWND hwnd,UINT uMsg,WPARAM wParam,LPARAM lParam)
{ 
    switch (uMsg) 
    {
        case WM_CREATE:
			{
				next_button_window = SDL_CreateWindowFrom((const void*)hwnd);
				next_button_renderer = SDL_CreateRenderer(next_button_window, -1, SDL_RENDERER_SOFTWARE);
				unsigned char * data;
				int width,height,depth,pitch;
				bool has_alpha;
				{
					load_png("res\\next_button.png",width,height,has_alpha,depth,pitch,&data);
					SDL_Surface * s = SDL_CreateRGBSurfaceFrom((void*)data,width,height,depth,pitch,0xFF000000,0x00FF0000,0x0000FF00,0x000000FF);
					next_button_image_texture = SDL_CreateTextureFromSurface(next_button_renderer,s);
					free(data);
				}
				return DefWindowProc(hwnd, uMsg, wParam, lParam);
			}
		case WM_LBUTTONDOWN:
			return 0;
		case WM_LBUTTONUP:
			engine_next_from_screenui();
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

void create_next_button()
{
	WNDCLASS wc = {0};

	HINSTANCE inst = GetModuleHandle(0);

	wc.hbrBackground = /*GetSysColorBrush(COLOR_BTNFACE)*/(HBRUSH)GetStockObject(WHITE_BRUSH);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hInstance = inst;
	wc.lpfnWndProc = next_button_proc;
	wc.lpszClassName = "NingNingKanKanScreenNextButton";
    wc.cbClsExtra = 0;                // no extra class memory 
    wc.cbWndExtra = 0;                // no extra window memory 
	wc.style = 0;

	RegisterClass(&wc);

	next_button_hwnd = CreateWindowEx(WS_EX_LAYERED | WS_EX_TOOLWINDOW, "NingNingKanKanScreenNextButton", 0,WS_POPUP | WS_VISIBLE, 0, 0, 1, 1, *(HWND*)screenui_panel_hwnd(), 0, 0, 0);
	ShowWindow(next_button_hwnd,SW_NORMAL);
}



void move_next_button()
{
	pair<int,int> panel_size_ = panel_size();

	pair<int,int> panel_position_ = panel_position();

	SDL_SetWindowPosition(next_button_window,panel_position_.first + panel_size_.first - 42 - 30, panel_position_.second + 11);
	SDL_SetWindowSize(next_button_window,next_button_width,next_button_height);
}

void close_next_button()
{
	DestroyWindow(next_button_hwnd);
}

void show_next_button(bool show)
{
	if( show )
	{
		ShowWindow(next_button_hwnd, SW_SHOW);
	}
	else
	{
		ShowWindow(next_button_hwnd, SW_HIDE);
	}
	{
		// the first time render!
		static bool last_show = false;
		if( last_show != show && show )
			render_next_button();
		last_show = show;
	}
}

void set_alpha_next_button(float t)
{	
	SDL_SetWindowAlpha(next_button_window, t);
	render_next_button();
}
///////////////////