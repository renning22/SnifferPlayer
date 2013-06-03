#pragma once

#include <string>

std::string UTF16toUTF8(std::wstring const & text);

std::wstring ANSItoUTF16(std::string const & text);

std::string ANSItoUTF8(std::string const & text);