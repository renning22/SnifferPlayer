#include "screenui.h"
#include "screenui_scale_buttons.h"
#include "screenui_panel.h"
#include "screenui_close_button.h"
#include "engine.h"
#include "screen.h"
#include "system_tray.h"
#include "libpng.h"
#include <SDL.h>
#include <boost/lexical_cast.hpp>
#include <Windows.h>

using namespace std;

#define N 5

static SDL_Window * scale_button_windows[N] = {0};
static SDL_Renderer * scale_button_renderers[N] = {0};
static SDL_Texture * scale_button_image_textures[N] = {0};
static HWND scale_button_hwnds[N] = {0};

int scale_button_width = 32;
int scale_button_height = 32;

static void render_scale_buttons()
{
	for(int i=0;i!=N;i++)
	{
		SDL_RenderClear(scale_button_renderers[i]);
		SDL_RenderCopy(scale_button_renderers[i],scale_button_image_textures[i],NULL,NULL);
		SDL_RenderPresent(scale_button_renderers[i]);
	}
}

static LRESULT CALLBACK scale_buttons_proc(HWND hwnd,UINT uMsg,WPARAM wParam,LPARAM lParam)
{ 
    switch (uMsg) 
    {
        case WM_CREATE:
			return DefWindowProc(hwnd, uMsg, wParam, lParam);
		case WM_LBUTTONDOWN:
			return 0;
		case WM_LBUTTONUP:
			if( hwnd == scale_button_hwnds[0] )
			{
				system_tray_item_selected(51);
			}
			if( hwnd == scale_button_hwnds[1] )
			{
				system_tray_item_selected(50);
			}
			else if( hwnd == scale_button_hwnds[2] )
			{
				system_tray_item_selected(201);
			}
			else if( hwnd == scale_button_hwnds[3] )
			{
				system_tray_item_selected(202);
			}
			else if( hwnd == scale_button_hwnds[4] )
			{
				system_tray_item_selected(203);
			}
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


void create_scale_buttons()
{
	WNDCLASS wc = {0};

	HINSTANCE inst = GetModuleHandle(0);

	wc.hbrBackground = /*GetSysColorBrush(COLOR_BTNFACE)*/(HBRUSH)GetStockObject(WHITE_BRUSH);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hInstance = inst;
	wc.lpfnWndProc = scale_buttons_proc;
	wc.lpszClassName = "NingNingKanKanScreenScaleButtons";
    wc.cbClsExtra = 0;                // no extra class memory 
    wc.cbWndExtra = 0;                // no extra window memory 
	wc.style = 0;

	RegisterClass(&wc);

	for(int i=0;i!=N;i++)
	{
		scale_button_hwnds[i] = CreateWindowEx(WS_EX_LAYERED | WS_EX_TOOLWINDOW, "NingNingKanKanScreenScaleButtons", 0,WS_POPUP | WS_VISIBLE, 0, 0, 1, 1, *(HWND*)screenui_panel_hwnd(), 0, 0, 0);
		scale_button_windows[i] = SDL_CreateWindowFrom((const void*)scale_button_hwnds[i]);
		scale_button_renderers[i] = SDL_CreateRenderer(scale_button_windows[i], -1, SDL_RENDERER_SOFTWARE);
		unsigned char * data;
		int width,height,depth,pitch;
		bool has_alpha;
		{
			if( i == 0 )
				load_png("res\\maximize.png",width,height,has_alpha,depth,pitch,&data);
			else if( i == 1 )
				load_png("res\\minimize.png",width,height,has_alpha,depth,pitch,&data);
			else
				load_png((string("res\\scale")+boost::lexical_cast<string>(i+1-2)+".png").c_str(),width,height,has_alpha,depth,pitch,&data);
			SDL_Surface * s = SDL_CreateRGBSurfaceFrom((void*)data,width,height,depth,pitch,0xFF000000,0x00FF0000,0x0000FF00,0x000000FF);
			scale_button_image_textures[i] = SDL_CreateTextureFromSurface(scale_button_renderers[i],s);
			free(data);
		}
		ShowWindow(scale_button_hwnds[i],SW_NORMAL);
	}
}


void move_scale_buttons()
{
	pair<int,int> ss = screen_size();

	pair<int,int> sp = screen_position();

	double full_screen_offset = 0;
	if( screen_fullscreen() )
	{
		full_screen_offset = + scale_button_height + 5;
	}

	for(int i=0;i!=N;i++)
	{
		SDL_SetWindowPosition(scale_button_windows[i],sp.first + ss.first - (i+2)*(scale_button_width+5), sp.second - (scale_button_height+5) + full_screen_offset);
		SDL_SetWindowSize(scale_button_windows[i],scale_button_width,scale_button_height);
	}
}

void show_scale_buttons(bool show)
{
	for(int i=0;i!=N;i++)
	{
		if( show )
		{
			ShowWindow(scale_button_hwnds[i], SW_SHOW);
		}
		else
		{
			ShowWindow(scale_button_hwnds[i], SW_HIDE);
		}
	}
	{
		// the first time render!
		static bool last_show = false;
		if( last_show != show && show )
			render_scale_buttons();
		last_show = show;
	}
}

void close_scale_buttons()
{
	for(int i=0;i!=N;i++)
	{
		DestroyWindow(scale_button_hwnds[i]);
	}
}

void set_alpha_scale_buttons(float t)
{
	for(int i=0;i!=N;i++)
	{
		SDL_SetWindowAlpha(scale_button_windows[i], t);
	}
	render_scale_buttons();
}

void scale_buttons_idle()
{
}
