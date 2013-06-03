#pragma once

#include <string>

std::string url_encode(std::string const & input);

std::string url_host(std::string const & url);

std::string url_uri(std::string const & url);

bool url_validate(std::string const & url);
