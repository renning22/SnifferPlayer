#pragma once

#include <string>
#include <map>

std::string http_get_content(std::string const & url,unsigned int timeout_secs=20);

std::string http_post(std::string const & url,std::map<std::string,std::string> const & arguments,unsigned int timeout_secs=20);