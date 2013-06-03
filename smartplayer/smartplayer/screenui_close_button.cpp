#include "screenui_panel.h"
#include "engine.h"
#include "screen.h"
#include "libpng.h"
#include <SDL.h>

#include <Windows.h>

using namespace std;

//////
static SDL_Window * close_button_window = 0;
static SDL_Renderer * close_button_renderer = 0;
static SDL_Texture * close_button_image_texture = 0;
static HWND close_button_hwnd = 0;

int close_button_width = 32;
int close_button_height = 32;
//////

static void render_close_button()
{
	//SDL_SetRenderDrawColor(close_button_renderer,0,0,255,10);
	SDL_RenderClear(close_button_renderer);
	SDL_RenderCopy(close_button_renderer,close_button_image_texture,NULL,NULL);
	SDL_RenderPresent(close_button_renderer);
}

static LRESULT CALLBACK close_button_proc(HWND hwnd,UINT uMsg,WPARAM wParam,LPARAM lParam)
{ 
    switch (uMsg) 
    {
        case WM_CREATE:
			{
				close_button_window = SDL_CreateWindowFrom((const void*)hwnd);
				close_button_renderer = SDL_CreateRenderer(close_button_window, -1, SDL_RENDERER_SOFTWARE);
				unsigned char * data;
				int width,height,depth,pitch;
				bool has_alpha;
				{
					load_png("res\\close_button.png",width,height,has_alpha,depth,pitch,&data);
					SDL_Surface * s = SDL_CreateRGBSurfaceFrom((void*)data,width,height,depth,pitch,0xFF000000,0x00FF0000,0x0000FF00,0x000000FF);
					close_button_image_texture = SDL_CreateTextureFromSurface(close_button_renderer,s);
					free(data);
				}
				return DefWindowProc(hwnd, uMsg, wParam, lParam);
			}
		case WM_LBUTTONDOWN:
			return 0;
		case WM_LBUTTONUP:
			engine_screen_close_from_screenui();
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

void create_close_button()
{
	WNDCLASS wc = {0};

	HINSTANCE inst = GetModuleHandle(0);

	wc.hbrBackground = /*GetSysColorBrush(COLOR_BTNFACE)*/(HBRUSH)GetStockObject(WHITE_BRUSH);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hInstance = inst;
	wc.lpfnWndProc = close_button_proc;
	wc.lpszClassName = "NingNingKanKanScreenCloseButton";
    wc.cbClsExtra = 0;                // no extra class memory 
    wc.cbWndExtra = 0;                // no extra window memory 
	wc.style = 0;

	RegisterClass(&wc);

	close_button_hwnd = CreateWindowEx(WS_EX_LAYERED | WS_EX_TOOLWINDOW, "NingNingKanKanScreenCloseButton", 0,WS_POPUP | WS_VISIBLE, 0, 0, 1, 1, *(HWND*)screenui_panel_hwnd(), 0, 0, 0);
	ShowWindow(close_button_hwnd,SW_NORMAL);
}



void move_close_button()
{
	pair<int,int> screen_size_ = screen_size();

	pair<int,int> screen_position_ = screen_position();

	double full_screen_offset = 0;
	if( screen_fullscreen() )
	{
		full_screen_offset = + close_button_height + 5;
	}

	SDL_SetWindowPosition(close_button_window,screen_position_.first + screen_size_.first - (close_button_width+5), screen_position_.second - (close_button_height+5) + full_screen_offset);
	SDL_SetWindowSize(close_button_window,close_button_width,close_button_height);
}

void close_close_button()
{
	DestroyWindow(close_button_hwnd);
}

void show_close_button(bool show)
{
	if( show )
	{
		ShowWindow(close_button_hwnd, SW_SHOW);
	}
	else
	{
		ShowWindow(close_button_hwnd, SW_HIDE);
	}
	{
		// the first time render!
		static bool last_show = false;
		if( last_show != show && show )
			render_close_button();
		last_show = show;
	}
}

void set_alpha_close_button(float t)
{	
	SDL_SetWindowAlpha(close_button_window, t);
	render_close_button();
}
///////////////////