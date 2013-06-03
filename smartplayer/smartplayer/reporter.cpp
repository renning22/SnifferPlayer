#include "log.h"
#include "http.h"
#include "url.h"
#include "userid.h"
#include "website.h"
#include "reporter.h"
#include "version.h"
#include "system_info.h"
#include "characterset.h"

#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/regex.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <sstream>

using namespace std;

static std::shared_ptr<boost::asio::io_service>		working_ioservice;

static std::vector<std::shared_ptr<boost::thread>>	working_threads;

static boost::recursive_mutex						local_variables_lock;

static auto											started_time = boost::posix_time::second_clock::local_time();

static bool											abort_request = false;


bool reporter_init()
{
	working_ioservice = std::make_shared<boost::asio::io_service>();

	for(int i=0;i!=1;i++)
	{
		working_threads.push_back( std::make_shared<boost::thread>(
		[=]()
		{
			DEBUG << "reporter : Working thread started , " << boost::this_thread::get_id();
			while( !abort_request )
			{
				// need to finish all of the work better
				if( !working_ioservice->poll() )
					boost::this_thread::sleep( boost::posix_time::milliseconds(500) );
			}
			DEBUG << "reporter : Working thread ended , " << boost::this_thread::get_id();
		}) );
	}
	return true;
}

void reporter_uninit()
{
	abort_request = true;
	working_ioservice->stop();
	for(auto it=working_threads.begin();it!=working_threads.end();it++)
		(*it)->join();
}

void reporter_report_login()
{
	stringstream ss;
	ss << "version = " << MAIN_VERSION << " , ";
	ss << "windows_version = " << system_info_windows_version();
	reporter_report("Login",ss.str());
}

void reporter_report_logout()
{
	stringstream ss;
	ss << "duration = " << boost::posix_time::second_clock::local_time()-started_time;
	reporter_report("Logout",ss.str());
}

void reporter_report(std::string const & a2,std::string const &a3)
{
	working_ioservice->post( [=]()
	{
		DEBUG << "reporter_report : report (" << a2 << "," << a3 << ")";
		map<string,string> args;
		args["A1"] = userid_userid();
		args["A2"] = ANSItoUTF8(a2);
		args["A3"] = ANSItoUTF8(a3);
		string reply; 
		try
		{
			reply = http_post(website_stats_url(),args);
			//DEBUG << "reply:\n" << reply;
		}
		catch(string e)
		{
			WARN << "reporter_report : failed , " << e << "\n" << reply;
		}
	});
}

void reporter_idle()
{
	unsigned int frame_count = 0;
	frame_count++;
	if( frame_count%100 == 0 )
		return;

	static DWORD last_heart_beat_ticks = 0;

	if( GetTickCount() - last_heart_beat_ticks > 10*60*1000 )
	{
		stringstream ss;
		ss << "duration = " << boost::posix_time::second_clock::local_time()-started_time;;
		reporter_report("HeartBeat",ss.str());

		last_heart_beat_ticks = GetTickCount();
	}
}