#include "http.h"
#include "url.h"
#include "utility.hpp"
#include "log.h"

#include <sstream>

#include <boost/asio.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/thread.hpp>

using namespace std;

string http_get_content(string const & url,unsigned int timeout_secs)
{
	string host = url_host(url);
	string uri = url_uri(url);

	if( host.empty() )
		throw string("http_get_content : not a valid url 1");
	
	if( uri.empty() )
		throw string("http_get_content : not a valid url 2");

	try
	{
		boost::asio::ip::tcp::iostream s;

		// The entire sequence of I/O operations must complete within 20 seconds.
		// If an expiry occurs, the socket is automatically closed and the stream
		// becomes bad.
		s.expires_from_now(boost::posix_time::seconds(timeout_secs));

		// Establish a connection to the server.
		s.connect(host, "http");
		if (!s)
		{
			stringstream ss;
			ss << "http_get_content : Unable connect to: " << host << " , error: " << s.error().message();
			throw ss.str();
		}

		// Send the request. We specify the "Connection: close" header so that the
		// server will close the socket after transmitting the response. This will
		// allow us to treat all data up until the EOF as the content.
		s << "GET " << uri << " HTTP/1.0\r\n";
		s << "Host: " << host << "\r\n";
		s << "Accept: */*\r\n";
		s << "Connection: close\r\n\r\n";

		// By default, the stream is tied with itself. This means that the stream
		// automatically flush the buffered output before attempting a read. It is
		// not necessary not explicitly flush the stream at this point.

		// Check that response is OK.
		std::string http_version;
		s >> http_version;
		unsigned int status_code;
		s >> status_code;
		std::string status_message;
		std::getline(s, status_message);
		if (!s || http_version.substr(0, 5) != "HTTP/")
		{
			stringstream ss;
			ss << "http_get_content : Invalid response , error: " << s.error().value();
			throw ss.str();
		}
		if (status_code != 200)
		{
			stringstream ss;
			ss << "http_get_content : Response returned with status code " << status_code;
			throw ss.str();
		}

		// Process the response headers, which are terminated by a blank line.
		string header;
		while (std::getline(s, header) && header != "\r");

		stringstream ss;
		ss << s.rdbuf();
		string content;
		content = ss.str();
		return content;
	}
	catch (std::exception& e)
	{
		stringstream ss;
		ss << "http_get_content : Exception: " << e.what();
		throw ss.str();
	}
	throw string("http_get_content : Never reached here");
}

string http_post(std::string const & url,std::map<std::string,std::string> const & arguments,unsigned int timeout_secs)
{
	string host = url_host(url);
	string uri = url_uri(url);

	if( host.empty() )
		throw string("http_post : not a valid url 1");
	
	if( uri.empty() )
		throw string("http_post : not a valid url 2");
	try
	{
		boost::asio::ip::tcp::iostream s;

		// The entire sequence of I/O operations must complete within 20 seconds.
		// If an expiry occurs, the socket is automatically closed and the stream
		// becomes bad.
		s.expires_from_now(boost::posix_time::seconds(timeout_secs));

		// Establish a connection to the server.
		s.connect(host, "http");
		if (!s)
		{
			stringstream ss;
			ss << "http_post : Unable connect to: " << host << " , error: " << s.error().message();
			throw ss.str();
		}

		string content;
		{
			stringstream ss;
			for(auto it=arguments.begin();it!=arguments.end();it++)
			{
				if( it != arguments.begin() )
					ss << "&";
				ss << it->first << "=" << url_encode(it->second);
			}
			content = ss.str();
		}
	
		// Send the request. We specify the "Connection: close" header so that the
		// server will close the socket after transmitting the response. This will
		// allow us to treat all data up until the EOF as the content.
		s << "POST " << uri << " HTTP/1.0\r\n";
		s << "Host: " << host << "\r\n";
		s << "Accept: */*\r\n";
		s << "Content-Length: " << content.length() << "\r\n";
		s << "Content-Type: application/x-www-form-urlencoded\r\n";
		s << "Connection: close\r\n\r\n";

		s << content;

		// By default, the stream is tied with itself. This means that the stream
		// automatically flush the buffered output before attempting a read. It is
		// not necessary not explicitly flush the stream at this point.

		// Check that response is OK.
		std::string http_version;
		s >> http_version;
		unsigned int status_code;
		s >> status_code;
		std::string status_message;
		std::getline(s, status_message);
		if (!s || http_version.substr(0, 5) != "HTTP/")
		{
			stringstream ss;
			ss << "http_post : Invalid response , error: " << s.error().value();
			throw ss.str();
		}
		if (status_code != 200)
		{
			stringstream ss;
			ss << "http_post : Response returned with status code " << status_code;
			throw ss.str();
		}

		// Process the response headers, which are terminated by a blank line.
		{
			string header;
			while (std::getline(s, header) && header != "\r");

			stringstream ss;
			ss << s.rdbuf();
			string return_content;
			return_content = ss.str();
			return return_content;
		}
	}
	catch (std::exception& e)
	{
		stringstream ss;
		ss << "http_post : Exception: " << e.what();
		throw ss.str();
	}
	throw string("http_post : Never reached here");
}