#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/thread.hpp>
#include <fstream>
#include <string>

#include "log.h"

using namespace std;

static boost::recursive_mutex local_variables_lock;

static string global_userid;

const char * cfg_filename = "userid.cfg";

static bool read()
{
	ifstream file(cfg_filename);
	if( !file )
	{
		file.close();
		return false;
	}
	else
	{
		file >> global_userid;
		auto this_id = boost::lexical_cast<boost::uuids::uuid>(global_userid);
		global_userid = boost::lexical_cast<string>(this_id);
		if( this_id.is_nil() )
		{
			ERROR << "userid : read illegal userid = " << global_userid;
			return false;
		}
	}
	return true;
}

static bool generate()
{
	auto this_id = boost::uuids::random_generator()();
	global_userid = boost::lexical_cast<string>(this_id);

	DEBUG << "userid : Generate userid = " << global_userid;
	ofstream ofile(cfg_filename);
	if( !ofile )
	{
		ERROR << "userid : couldn't open " << cfg_filename;
		return false;
	}
	ofile << global_userid;
	ofile.close();
}

bool userid_init()
{
	if( !read() )
	{
		return generate();
	}
	return true;
}

void userid_uninit()
{
}

string userid_userid()
{
	boost::lock_guard<boost::recursive_mutex> guard(local_variables_lock);
	return global_userid;
}