#include "log.h"
#include "engine.h"
#include "parser.h"
#include "http.h"
#include "website.h"

#include <string>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <iostream>

static std::string get_next_url(std::string src_page)
{
	try
	{
		std::string content = http_get_content(website_next_url(src_page));
		std::stringstream ss(content);
		std::string file_path;
		if( std::getline(ss,file_path) )
		{
			return file_path;
		}
	}
	catch (std::string e)
	{
		WARN << "get_next_url : " << e;
	}
	return "";
}

void next_url_from_engine(std::string src_page)
{
	std::string next_url = get_next_url(src_page);
	if (next_url.empty())
	{
		return;
	}
	else
	{
		DEBUG << "Next_url: " << next_url;
	}

	parser_parse(next_url,[=](std::vector<std::string> urls)
	{
		engine_parsed_from_next_url(next_url,urls);
	});
}