#include "screenui_progresser.h"
#include "screenui_panel.h"
#include "engine.h"
#include "ffplay.h"
#include "screen.h"
#include "libpng.h"
#include <SDL.h>

#include <Windows.h>

using namespace std;

//////
static SDL_Window * progresser_window = 0;
static SDL_Renderer * progresser_renderer = 0;
static SDL_Surface * progresser_image = 0;
static SDL_Texture * progresser_image_texture_whole = 0;
static SDL_Texture * progresser_image_texture_buffer_pos = 0;
static SDL_Texture * progresser_image_texture_slider = 0;
static HWND progresser_hwnd = 0;

bool dragging = false;

int progresser_texture_slider_width = 0;

int progresser_texture_height = 0;
int progresser_texture_whole_height = 0;
int progresser_texture_buffer_height = 0;
int progresser_texture_slider_height = 0;

int progresser_width = 200; //会动态改变

double total_length = 1000.0f;
std::vector< std::pair<double,double> > buffer_status;
double current_pos = 0.0f;
//////


static LRESULT CALLBACK progresser_proc(HWND hwnd,UINT uMsg,WPARAM wParam,LPARAM lParam)
{ 
    switch (uMsg) 
    {
        case WM_CREATE:
			{
				progresser_window = SDL_CreateWindowFrom((const void*)hwnd);
				progresser_renderer = SDL_CreateRenderer(progresser_window, -1, SDL_RENDERER_SOFTWARE);
				SDL_Surface * ws = SDL_GetWindowSurface(progresser_window);
				unsigned char * data;
				int width,height,depth,pitch;
				bool has_alpha;
				{
					load_png("res\\progresser_whole.png",width,height,has_alpha,depth,pitch,&data);
					SDL_Surface * s = SDL_CreateRGBSurfaceFrom((void*)data,width,height,depth,pitch,0xFF000000,0x00FF0000,0x0000FF00,0x000000FF);
					progresser_width = width;
					progresser_texture_whole_height = height;
					if (height > progresser_texture_height)
					{
						progresser_texture_height = height;
					}
					progresser_image_texture_whole = SDL_CreateTextureFromSurface(progresser_renderer,s);
					SDL_SetTextureBlendMode(progresser_image_texture_whole,SDL_BLENDMODE_BLEND);
					free(data);
				}
				{
					load_png("res\\progresser_buffer_pos.png",width,height,has_alpha,depth,pitch,&data);
					SDL_Surface * s = SDL_CreateRGBSurfaceFrom((void*)data,width,height,depth,pitch,0xFF000000,0x00FF0000,0x0000FF00,0x000000FF);
					progresser_texture_buffer_height = height;
					if (height > progresser_texture_height)
					{
						progresser_texture_height = height;
					}
					progresser_image_texture_buffer_pos = SDL_CreateTextureFromSurface(progresser_renderer,s);
					SDL_SetTextureBlendMode(progresser_image_texture_buffer_pos,SDL_BLENDMODE_BLEND);
					free(data);
				}
				{
					load_png("res\\progresser_slider.png",width,height,has_alpha,depth,pitch,&data);
					SDL_Surface * s = SDL_CreateRGBSurfaceFrom((void*)data,width,height,depth,pitch,0xFF000000,0x00FF0000,0x0000FF00,0x000000FF);
					progresser_texture_slider_width = width;
					progresser_texture_slider_height = height;
					if (height > progresser_texture_height)
					{
						progresser_texture_height = height;
					}
					progresser_image_texture_slider = SDL_CreateTextureFromSurface(progresser_renderer,s);
					SDL_SetTextureBlendMode(progresser_image_texture_slider,SDL_BLENDMODE_BLEND);
					free(data);
				}
			}
			return DefWindowProc(hwnd, uMsg, wParam, lParam);
		case WM_LBUTTONDOWN:
			{
				int xPos = (short)LOWORD(lParam); 
				int yPos = (short)HIWORD(lParam); 
				//printf("Progresser Down: %d %d\n", xPos, yPos);

				//判断是否在slider范围内
				int current_pos_in_pixel = (int)(progresser_width * (current_pos / total_length) + 0.5);
				if (xPos >= current_pos_in_pixel - progresser_texture_slider_width / 2 && xPos <= current_pos_in_pixel + progresser_texture_slider_width / 2)
				{					
				}
				else
				{
					current_pos = total_length * xPos / progresser_width;
					current_pos = max(0.0,current_pos);
					current_pos = min(total_length,current_pos);
					engine_redraw_progresser_from_screenui();
				}
				dragging = true;
				SetCapture(hwnd);
			}
			return 0;
		case WM_MOUSEMOVE:
			{
				if (dragging)
				{
					int xPos = (short)LOWORD(lParam); 
					int yPos = (short)HIWORD(lParam); 
					//printf("Progresser Move: %d %d\n", xPos, yPos);

					current_pos = total_length * xPos / progresser_width;
					current_pos = max(0.0,current_pos);
					current_pos = min(total_length,current_pos);
					//printf("move, debug: %.2f, %d, %d, %.2f\n", total_length, xPos, progresser_width, current_pos);
					engine_redraw_progresser_from_screenui();

					engine_drag_progresser_move_from_screenui(current_pos);
				}
			}
			return 0;
		case WM_LBUTTONUP:
			{
				ReleaseCapture();
				int xPos = (short)LOWORD(lParam); 
				int yPos = (short)HIWORD(lParam); 

				dragging = false;

				current_pos = total_length * xPos / progresser_width;
				current_pos = max(0.0,current_pos);
				current_pos = min(total_length,current_pos);

				printf("Progresser BUTTONUP: %d %d %.2lf %d\n", xPos, yPos, total_length, progresser_width);

				engine_redraw_progresser_from_screenui();

				engine_drag_progresser_end_from_screenui(current_pos);
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
			DeleteObject(progresser_image);
			return DefWindowProc(hwnd, uMsg, wParam, lParam);
		default:
			return DefWindowProc(hwnd, uMsg, wParam, lParam);
    } 
}

void create_progresser()
{
	WNDCLASS wc = {0};

	HINSTANCE inst = GetModuleHandle(0);

	wc.hbrBackground = /*GetSysColorBrush(COLOR_BTNFACE)*/(HBRUSH)GetStockObject(WHITE_BRUSH);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hInstance = inst;
	wc.lpfnWndProc = progresser_proc;
	wc.lpszClassName = "NingNingKanKanScreenProgresser";
    wc.cbClsExtra = 0;                // no extra class memory 
    wc.cbWndExtra = 0;                // no extra window memory 
	wc.style = 0;

	RegisterClass(&wc);
	
	progresser_hwnd = CreateWindowEx(WS_EX_LAYERED | WS_EX_TOOLWINDOW, "NingNingKanKanScreenProgresser", 0,WS_POPUP | WS_VISIBLE, 0, 0, 1, 1, *(HWND*)screenui_panel_hwnd(), 0, 0, 0);
	ShowWindow(progresser_hwnd,SW_NORMAL);
}

void render_progresser()
{	
	//printf("render, current_pos: %.2f\n", current_pos);

	if (progresser_width >= progresser_texture_slider_width)
	{
		pair<int,int> screen_size_ = screen_size();

		SDL_RenderClear(progresser_renderer);
	
		SDL_Rect whole_rect;
		whole_rect.w = progresser_width;  
		whole_rect.h = progresser_texture_whole_height;
		whole_rect.x = 0;
		whole_rect.y = (progresser_texture_height - progresser_texture_whole_height) / 2;
		SDL_RenderCopy(progresser_renderer,progresser_image_texture_whole,NULL,&whole_rect);

		for(auto it = buffer_status.begin();it != buffer_status.end();it++)
		{
			SDL_Rect buffer_rect;
			buffer_rect.w = (it->second-it->first)/total_length*progresser_width;
			buffer_rect.h = progresser_texture_buffer_height;
			buffer_rect.x = it->first/total_length*progresser_width;
			buffer_rect.y = (progresser_texture_height - progresser_texture_buffer_height) / 2;
			SDL_RenderCopy(progresser_renderer,progresser_image_texture_buffer_pos,NULL,&buffer_rect);
		}
		/*
		if (total_length - buffer_pos < 1.0)
		{
			SDL_Rect buffer_rect;
			buffer_rect.w = progresser_width;  
			buffer_rect.h = progresser_texture_buffer_height;
			buffer_rect.x = 0;															
			buffer_rect.y = (progresser_texture_height - progresser_texture_buffer_height) / 2;
			SDL_RenderCopy(progresser_renderer,progresser_image_texture_buffer_pos,NULL,&buffer_rect);
		}
		else
		{
			SDL_Rect buffer_rect;
			buffer_rect.w = progresser_texture_slider_width / 2 + (int)(buffer_pos / total_length * (progresser_width - progresser_texture_slider_width) + 0.5);  
			buffer_rect.h = progresser_texture_buffer_height;
			buffer_rect.x = 0;
			buffer_rect.y = (progresser_texture_height - progresser_texture_buffer_height) / 2;
			SDL_RenderCopy(progresser_renderer,progresser_image_texture_buffer_pos,NULL,&buffer_rect);
		}
		*/
		SDL_Rect slider_rect;
		slider_rect.w = progresser_texture_slider_width;  
		slider_rect.h = progresser_texture_slider_height;
		slider_rect.x = (int)(current_pos / total_length * (progresser_width - progresser_texture_slider_width) + 0.5);
		slider_rect.y = (progresser_texture_height - progresser_texture_slider_height) / 2;
		SDL_RenderCopy(progresser_renderer,progresser_image_texture_slider,NULL,&slider_rect);
	
		SDL_RenderPresent(progresser_renderer);
	}
}

void set_total_length(double length)
{
	total_length = length; //TODO: 到时候改一下，total_length没有设置的话所有时间都不响应，控件也不显示
}

void set_buffer_pos(std::vector< std::pair<double,double> > const & buffer_status_)
{
	buffer_status = buffer_status_;
}

void set_current_pos(double pos)
{
	current_pos = pos;
}

void move_progresser()
{
	pair<int,int> panel_size_ = panel_size();

	pair<int,int> panel_position_ = panel_position();
												//play	    time        next
	progresser_width = panel_size_.first - (20 + 42 + 12 + 96 + 12 + 12 + 42 + 30);

	//printf("progresser_width: %d\n", progresser_width);
	
	if (progresser_width >= progresser_texture_slider_width)
	{
		SDL_SetWindowPosition(progresser_window,panel_position_.first + 20 + 42 + 12 + 96 + 12, panel_position_.second + (64 - progresser_texture_height) / 2);
		SDL_SetWindowSize(progresser_window, progresser_width, progresser_texture_height);
	}
}

void show_progresser(bool show)
{
	if( show )
	{
		ShowWindow(progresser_hwnd, SW_SHOW);
	}
	else
	{
		ShowWindow(progresser_hwnd, SW_HIDE);
	}
	{
		// the first time render!
		static bool last_show = false;
		if( last_show != show && show )
			render_progresser();
		last_show = show;
	}
}

void close_progresser()
{
	DestroyWindow(progresser_hwnd);
}

void set_alpha_progresser(float t)
{
	SDL_SetWindowAlpha(progresser_window, t);
	render_progresser();
}

bool progresser_is_dragging()
{
	return dragging;
}

void progresser_idle()
{	
	if( ffplay_episode_playing() )
	{
		static int frame_count = 0;
		frame_count++;
		// syncronize the progresser!
		if( frame_count % 10 == 0 )
		{
			if( !progresser_is_dragging() )
			{
				set_total_length( ffplay_episode_duration() );

				auto buffer_status = ffplay_episode_buffer_status();
				set_buffer_pos( buffer_status );
				double pos = ffplay_episode_current_playing_pos();
				if( pos >= 0 )
				{
					set_current_pos( pos );
				}
				render_progresser();
			}
		}

	}
}