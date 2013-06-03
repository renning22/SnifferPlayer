#include "screenui_corner.h"
#include "engine.h"
#include "screen.h"
#include "libpng.h"
#include <SDL.h>

#include <Windows.h>

using namespace std;

int corner_width = 40;
int corner_height = 40;

static LRESULT CALLBACK corners_proc(HWND hwnd,UINT uMsg,WPARAM wParam,LPARAM lParam);

//////
class corner
{
public:
	SDL_Window * window;
	SDL_Renderer * renderer;
	SDL_Texture * image_texture;
	HWND hwnd;

	int width;
	int height;

	corner_dir dir;
	int dragging_last_pos_x, dragging_last_pos_y;

	bool is_dragging;

	bool last_show;
	//////

	corner() : window(NULL), renderer(NULL), image_texture(NULL), hwnd(NULL), width(corner_width), height(corner_height), is_dragging(false) , last_show(false)
	{
	}

	void render()
	{
		//TODO: dragging状态的图不一样
		SDL_RenderClear(renderer);
		SDL_RenderCopy(renderer,image_texture,NULL,NULL);
		SDL_RenderPresent(renderer);
	}

	void set_alpha(float t)
	{
		SDL_SetWindowAlpha(window, t);
		render();
	}

	void move()
	{		
		pair<int,int> screen_size_ = screen_size();
		
		int relative_pos_x, relative_pos_y;

		switch (dir)
		{
			case LEFTUP:			
				relative_pos_x = 0;
				relative_pos_y = 0;
				break;
			case LEFTBOTTOM:
				relative_pos_x = 0;
				relative_pos_y = screen_size_.second - corner_height;
				break;
			case RIGHTUP:
				relative_pos_x = screen_size_.first - corner_width;
				relative_pos_y = 0;
				break;
			case RIGHTBOTTOM:
				relative_pos_x = screen_size_.first - corner_width;
				relative_pos_y = screen_size_.second - corner_height;
				break;
		}

		pair<int,int> screen_position_ = screen_position();

		SDL_SetWindowAlpha(window,0.6f);
		SDL_SetWindowPosition(window,screen_position_.first + relative_pos_x, screen_position_.second + relative_pos_y);
		SDL_SetWindowSize(window, width, height);
	}

	void close()
	{
		DestroyWindow(hwnd);
	}

	void show(bool show)
	{
		if( show )
		{	
			ShowWindow(hwnd, SW_SHOW);
		}
		else
		{
			ShowWindow(hwnd, SW_HIDE);
		}
		{
			// the first time render!
			if( last_show != show && show )
				render();
			last_show = show;
		}
	}

	void create(char* png_path, corner_dir _dir)
	{
		dir = _dir;

		hwnd = CreateWindowEx(WS_EX_LAYERED | WS_EX_TOOLWINDOW, "NingNingKanKanScreenCorner", 0,WS_POPUP | WS_VISIBLE, 0, 0, 1, 1, *(HWND*)screen_hwnd(), 0, 0, 0);
		SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG)this);

		window = SDL_CreateWindowFrom((const void*)hwnd);
		renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
		unsigned char * data;
		int width,height,depth,pitch;
		bool has_alpha;
		{
			load_png(png_path,width,height,has_alpha,depth,pitch,&data);
			SDL_Surface * s = SDL_CreateRGBSurfaceFrom((void*)data,width,height,depth,pitch,0xFF000000,0x00FF0000,0x0000FF00,0x000000FF);
			image_texture = SDL_CreateTextureFromSurface(renderer,s);
			free(data);
		}

		ShowWindow(hwnd, SW_NORMAL);
	}
};

corner corners[4];

static LRESULT CALLBACK corners_proc(HWND hwnd,UINT uMsg,WPARAM wParam,LPARAM lParam)
{ 
	corner* c = (corner*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
	switch (uMsg) 
	{
		case WM_CREATE:				
			return DefWindowProc(hwnd, uMsg, wParam, lParam);
		case WM_LBUTTONDOWN:
			{
				POINT p;
				::GetCursorPos(&p);
				c->dragging_last_pos_x = p.x;
				c->dragging_last_pos_y = p.y;

				c->is_dragging = true;
				SetCapture(c->hwnd);

				engine_drag_corner_start(c->dir, p.x, p.y);
			}
			return 0;
		case WM_MOUSEMOVE:
			{
				if (c->dir == LEFTUP || c->dir == RIGHTBOTTOM )
				{
					SetCursor(LoadCursor(NULL,IDC_SIZENWSE));
				}
				else
				{
					SetCursor(LoadCursor(NULL,IDC_SIZENESW));
				}

				if (c->is_dragging)
				{
					POINT p;
					::GetCursorPos(&p);
					int xPos = p.x;
					int yPos = p.y;

					//printf("drag move: %d %d\n", xPos, yPos);
					
					engine_drag_corner_move(c->dir, p.x, p.y);

					c->dragging_last_pos_x = xPos;
					c->dragging_last_pos_y = yPos;
				}
			}
			return 0;
		case WM_LBUTTONUP:
			{
				ReleaseCapture();
				
				POINT p;
				::GetCursorPos(&p);
				int xPos = p.x;
				int yPos = p.y;

				c->is_dragging = false;

				engine_drag_corner_end(c->dir, p.x, p.y);
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


void create_corner()
{
	WNDCLASS wc = {0};

	HINSTANCE inst = GetModuleHandle(0);

	wc.hbrBackground = /*GetSysColorBrush(COLOR_BTNFACE)*/(HBRUSH)GetStockObject(WHITE_BRUSH);
	wc.hCursor = NULL;
	wc.hInstance = inst;
	wc.lpfnWndProc = corners_proc;
	wc.lpszClassName = "NingNingKanKanScreenCorner";
	wc.cbClsExtra = 0;                // no extra class memory 
	wc.cbWndExtra = 0;                // no extra window memory 
	wc.style = 0;

	RegisterClass(&wc);

	corners[0].create("res//leftup.png", LEFTUP);
	corners[1].create("res//leftbottom.png", LEFTBOTTOM);
	corners[2].create("res//rightup.png", RIGHTUP);
	corners[3].create("res//rightbottom.png", RIGHTBOTTOM);
}

void move_corner()
{
	for (int i = 0; i < 4; ++i)
	{
		corners[i].move();
	}
}

void close_corner()
{
	for (int i = 0; i < 4; ++i)
	{
		corners[i].close();
	}
}

void show_corner(bool show)
{
	for (int i = 0; i < 4; ++i)
	{
		corners[i].show(show);
	}
}

void set_alpha_corner(float t)
{
	for (int i = 0; i < 4; ++i)
	{
		corners[i].set_alpha(t);
	}
}
///////////////////