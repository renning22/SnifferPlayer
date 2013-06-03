#include "version_checker.h"
#include "version.h"
#include "http.h"
#include "website.h"
#include "log.h"
#include "system_tray.h"

#include <Windows.h>
#include <boost/thread.hpp>

using namespace std;

bool version_checker_init()
{
	return true;
}

void version_checker_unit()
{
}

void version_checker_idle()
{
	unsigned int frame_count = 0;
	frame_count++;
	if( frame_count%100 == 0 )
		return;

	static DWORD last_version_check_ticks = 0;

	if( GetTickCount() - last_version_check_ticks > 5*60*1000 )
	{
		boost::thread t([&]()
		{
			string reply;
			try
			{
				reply = http_get_content(website_version_url(MAIN_VERSION));
			}
			catch(string e)
			{
				ERROR << "version_checker_idle : check failed,\n" << e;
				return;
			}

			if( reply == "OK" )
			{
			}
			else if( reply == "UPDATE" )
			{
				system_tray_balloon("有新版本了，快去下载吧！");
			}
			else
			{
				system_tray_balloon(reply);
			}
		});

		last_version_check_ticks = GetTickCount();
	}
}