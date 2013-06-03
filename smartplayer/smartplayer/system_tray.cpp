#include "screen.h"
#include "engine.h"
#include "version.h"

#include "screenui_play_button.h"

#include <windows.h>
#include <strsafe.h>

NOTIFYICONDATA nid = {};

HMENU menu = NULL;
HMENU menu_screen_size = NULL;

HICON system_tray_icons[] =
{
	(HICON)LoadImage(NULL,"res//system_tray.ico",IMAGE_ICON,0,0,LR_LOADFROMFILE),
	NULL,
	(HICON)LoadImage(NULL,"res//system_tray2.ico",IMAGE_ICON,0,0,LR_LOADFROMFILE),
	(HICON)LoadImage(NULL,"res//system_tray3.ico",IMAGE_ICON,0,0,LR_LOADFROMFILE),
	(HICON)LoadImage(NULL,"res//system_tray4.ico",IMAGE_ICON,0,0,LR_LOADFROMFILE),
};


static bool modify_icon(int number)
{
	nid.hIcon = system_tray_icons[number];

	nid.uFlags &= ~NIF_INFO;

	return Shell_NotifyIcon(NIM_MODIFY, &nid) == 0;
}

static bool modify_balloon(std::string const & text,std::string const & type)
{
	if( text.size() > 200 )
		return false;

	strcpy(nid.szInfo,text.c_str());

	strcpy(nid.szInfoTitle,"宁宁看看");

	if( type == "warning" )
	{
		nid.dwInfoFlags = NIIF_WARNING;
	}
	else if( type == "error" )
	{
		nid.dwInfoFlags = NIIF_ERROR;
	}
	else
	{
		nid.dwInfoFlags = NIIF_NONE;
	}

	nid.uTimeout = 3000;
	nid.uFlags |= NIF_INFO;

	return Shell_NotifyIcon(NIM_MODIFY, &nid) == 0;

}

bool system_tray_init()
{
	// Declare NOTIFYICONDATA details. 
	// Error handling is omitted here for brevity. Do not omit it in your code.

	HWND * hwndp = (HWND*)screen_hwnd();

	memset(&nid,0,sizeof(nid));
	nid.cbSize = sizeof(nid);
	nid.hWnd = *hwndp;
	nid.uID = 0;
	nid.uCallbackMessage = WM_USER+1;
	nid.uVersion = 0;
	nid.uFlags = NIF_ICON | NIF_TIP | NIF_GUID | NIF_MESSAGE;

	// This text will be shown as the icon's tooltip.
	StringCchCopy(nid.szTip, ARRAYSIZE(nid.szTip), "宁宁看看 "MAIN_VERSION);

	// Load the icon for high DPI.
	nid.hIcon = (HICON)LoadImage(NULL,"res//system_tray.ico",IMAGE_ICON,0,0,LR_LOADFROMFILE | LR_SHARED);

	// Show the notification.
	DWORD ret = Shell_NotifyIcon(NIM_ADD, &nid) ? S_OK : E_FAIL;

	return ret==0;
}

void system_tray_uninit()
{
	Shell_NotifyIcon(NIM_DELETE,&nid);

	if( menu )
		DestroyMenu(menu);
	if( menu_screen_size )
		DestroyMenu(menu_screen_size);
}

void system_tray_left_button_up()
{
}

void system_tray_right_button_up()
{
	if( menu )
		DestroyMenu(menu);
	if( menu_screen_size )
		DestroyMenu(menu_screen_size);

	HWND * hwndp = (HWND*)screen_hwnd();

	menu = CreatePopupMenu();
	menu_screen_size = CreatePopupMenu();
	AppendMenu(menu_screen_size,MF_STRING,200+1,"1x");
	AppendMenu(menu_screen_size,MF_STRING,200+2,"1.5x");
	AppendMenu(menu_screen_size,MF_STRING,200+3,"2x");

	AppendMenu(menu,MF_STRING|MF_GRAYED,0,MAIN_VERSION);
	AppendMenu(menu,MF_STRING|MF_SEPARATOR,0,NULL);
	AppendMenu(menu,MF_STRING|(engine_sniffer_mode()?MF_CHECKED:MF_UNCHECKED),1,"开启捕获");
	AppendMenu(menu,MF_STRING|(!engine_screen_opaque_mode()?MF_CHECKED:MF_UNCHECKED),2,"半透明模式");
	AppendMenu(menu,MF_STRING|(screen_topmost()?MF_CHECKED:MF_UNCHECKED)|(!engine_screen_opaque_mode()?MF_DISABLED:MF_ENABLED),3,"总在最上面");
	AppendMenu(menu,MF_STRING|MF_SEPARATOR,0,NULL);
	AppendMenu(menu,MF_STRING,50,"最小化");
	AppendMenu(menu,MF_STRING,51,"最大化");
	AppendMenu(menu,MF_STRING|MF_POPUP,(UINT_PTR)menu_screen_size,"屏幕大小");
	AppendMenu(menu,MF_STRING|MF_SEPARATOR,0,NULL);
	AppendMenu(menu,MF_STRING,100+2,(play_button_state()?"暂停":"播放"));
	AppendMenu(menu,MF_STRING,100+3,"停止");
	AppendMenu(menu,MF_STRING,100+4,"下一集");
	AppendMenu(menu,MF_STRING|MF_SEPARATOR,0,NULL);
	AppendMenu(menu,MF_STRING,1000,"完全退出");


	POINT p;
	GetCursorPos(&p);

	SetForegroundWindow(*hwndp);
	TrackPopupMenu(menu,TPM_LEFTALIGN | TPM_LEFTBUTTON,p.x,p.y,0,*hwndp,NULL);
}

void system_tray_item_selected(int id)
{
	if( id == 1 )
	{
		engine_sniffer_mode(!engine_sniffer_mode());
	}
	else if( id == 2 )
	{
		engine_screen_opaque_mode(!engine_screen_opaque_mode());
	}
	else if( id == 3 )
	{
		engine_screen_toggle_topmost();
	}
	else if( id == 50 )
	{
		engine_screen_toggle_minimize_from_system();
	}
	else if( id == 51 )
	{
		engine_screen_toggle_maximize_from_system();
	}
	else if( id == 100+2 )
	{
		engine_play_pause_from_screenui();
	}
	else if( id == 100+3 )
	{
		engine_screen_close_from_screenui();
	}
	else if( id == 100+4 )
	{
		engine_next_from_screenui();
	}
	else if( id == 200+1 )
	{
		engine_screen_size(1.0);
	}
	else if( id == 200+2 )
	{
		engine_screen_size(1.5);
	}
	else if( id == 200+3 )
	{
		engine_screen_size(2.0);
	}
	else if( id == 1000 )
	{
		engine_abort();
	}
}

//
void system_tray_states_sync_from_engine()
{
	if( engine_episode_loading() )
	{
		modify_icon(4);
	}
	else
	{
		if( !engine_sniffer_mode() )
		{
			modify_icon(2);
		}
		else
		{
			if( engine_screen_opaque_mode() )
			{
				modify_icon(0);
			}
			else
			{
				modify_icon(3);
			}
		}
	}
}

bool system_tray_balloon(std::string text,std::string type)
{
	return modify_balloon(text,type);
}