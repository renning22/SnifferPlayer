#include <string>
#include <sstream>
#include <iomanip>

using namespace std;

static string clean_url(std::string const & url)
{
	if( url.substr(0,strlen("http://")) != "http://"
		&& url.substr(0,strlen("rtmp://")) != "rtmp://"
		&& url.substr(0,strlen("rtmpe://")) != "rtmpe://" )
		return "";
	return url.substr(url.find("://")+strlen("://"));
}

string url_encode(std::string const & input)
{
    ostringstream ssOut;
    ssOut << std::setbase(16);
    for(string::const_iterator i = input.begin(); i != input.end(); ++i)
    {
		unsigned char c = *i;
		if(isalnum(c))
            ssOut << c;
        else
			ssOut << '%' << std::uppercase << std::setw(2) << std::setfill('0') << (unsigned int)c;
    }
    return ssOut.str();
}

string url_host(std::string const & url)
{
	auto cu = clean_url(url);
	if( cu.find_first_of('/') == string::npos )
	{
		return cu;
	}
	else
	{
		return cu.substr(0,cu.find_first_of('/'));
	}
}

string url_uri(std::string const & url)
{
	auto cu = clean_url(url);
	if( cu.find_first_of('/') == string::npos )
	{
		return "";
	}
	else
	{
		return cu.substr(cu.find_first_of('/'));
	}
}

bool url_validate(std::string const & url)
{
	if( !url_uri(url).empty() )
		return true;
	else
		return false;
}