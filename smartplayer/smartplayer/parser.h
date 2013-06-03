#pragma once

#include <boost/function.hpp>
#include <vector>

bool parser_init();

void parser_uninit();

std::string parser_url_filter(std::string host,std::string uri,std::string referer);

void parser_parse(std::string url,boost::function<void(std::vector<std::string>)> async);

bool parser_is_parsing_url(std::string url);