#pragma once

#include <string>

class Stream;


bool streamset_open(std::string const & name);
Stream * streamset_get(std::string const & name);

//void streamset_foreach(

void streamset_idle();