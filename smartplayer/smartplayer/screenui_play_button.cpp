#include "screenui_play_button.h"
#include "screenui_panel.h"
#include "engine.h"
#include "ffplay.h"
#include "screen.h"
#include "libpng.h"
#include <SDL.h>

#include <Windows.h>

using namespace std;

//////
static SDL_Window * play_button_window = 0;
static SDL_Renderer * play_button_renderer = 0;
static SDL_Texture * play_button_image_texture = 0;
static SDL_Texture * pause_button_image_texture = 0;
static HWND play_button_hwnd = 0;

int play_button_width = 42;
int play_button_height = 42;

bool is_playing = true;
//////

static void render_play_button()
{
	//SDL_SetRenderDrawColor(play_button_renderer,0,0,255,10);
	if (is_playing)
	{
		SDL_RenderClear(play_button_renderer);
		SDL_RenderCopy(play_button_renderer,pause_button_image_texture,NULL,NULL);
		SDL_RenderPresent(play_button_renderer);
	}
	else
	{
		SDL_RenderClear(play_button_renderer);
		SDL_RenderCopy(play_button_renderer,play_button_image_texture,NULL,NULL);
		SDL_RenderPresent(play_button_renderer);
	}
}

static LRESULT CALLBACK play_button_proc(HWND hwnd,UINT uMsg,WPARAM wParam,LPARAM lParam)
{ 
    switch (uMsg) 
    {
        case WM_CREATE:
			{
				play_button_window = SDL_CreateWindowFrom((const void*)hwnd);
				play_button_renderer = SDL_CreateRenderer(play_button_window, -1, SDL_RENDERER_SOFTWARE);
				unsigned char * data;
				int width,height,depth,pitch;
				bool has_alpha;
				{
					load_png("res\\play_button.png",width,height,has_alpha,depth,pitch,&data);
					SDL_Surface * s = SDL_CreateRGBSurfaceFrom((void*)data,width,height,depth,pitch,0xFF000000,0x00FF0000,0x0000FF00,0x000000FF);
					play_button_image_texture = SDL_CreateTextureFromSurface(play_button_renderer,s);
					free(data);
				}
				{
					load_png("res\\pause_button.png",width,height,has_alpha,depth,pitch,&data);
					SDL_Surface * s = SDL_CreateRGBSurfaceFrom((void*)data,width,height,depth,pitch,0xFF000000,0x00FF0000,0x0000FF00,0x000000FF);
					pause_button_image_texture = SDL_CreateTextureFromSurface(play_button_renderer,s);
					free(data);
				}
				return DefWindowProc(hwnd, uMsg, wParam, lParam);
			}
		case WM_LBUTTONDOWN:
			return 0;
		case WM_LBUTTONUP:
			engine_play_pause_from_screenui();
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

void create_play_button()
{
	WNDCLASS wc = {0};

	HINSTANCE inst = GetModuleHandle(0);

	wc.hbrBackground = /*GetSysColorBrush(COLOR_BTNFACE)*/(HBRUSH)GetStockObject(WHITE_BRUSH);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hInstance = inst;
	wc.lpfnWndProc = play_button_proc;
	wc.lpszClassName = "NingNingKanKanScreenPlayButton";
    wc.cbClsExtra = 0;                // no extra class memory 
    wc.cbWndExtra = 0;                // no extra window memory 
	wc.style = 0;

	RegisterClass(&wc);

	play_button_hwnd = CreateWindowEx(WS_EX_LAYERED | WS_EX_TOOLWINDOW, "NingNingKanKanScreenPlayButton", 0,WS_POPUP | WS_VISIBLE, 0, 0, 1, 1, *(HWND*)screenui_panel_hwnd(), 0, 0, 0);
	ShowWindow(play_button_hwnd,SW_NORMAL);
}



void move_play_button()
{
	pair<int,int> panel_size_ = panel_size();

	pair<int,int> panel_position_ = panel_position();

	SDL_SetWindowPosition(play_button_window,panel_position_.first + 20, panel_position_.second + 11);
	SDL_SetWindowSize(play_button_window,play_button_width,play_button_height);
}

void close_play_button()
{
	DestroyWindow(play_button_hwnd);
}

void show_play_button(bool show)
{
	if( show )
	{
		ShowWindow(play_button_hwnd, SW_SHOW);
	}
	else
	{
		ShowWindow(play_button_hwnd, SW_HIDE);
	}
	{
		// the first time render!
		static bool last_show = false;
		if( last_show != show && show )
			render_play_button();
		last_show = show;
	}
}

void set_alpha_play_button(float t)
{	
	SDL_SetWindowAlpha(play_button_window, t);
	render_play_button();
}

// this is safe to call ffplay cause called by main thread (engine)
void play_button_state_sync()
{
	is_playing = !ffplay_episode_pausing();
	render_play_button();
}

bool play_button_state()
{
	return is_playing;
}

void play_button_idle()
{
}
///////////////////