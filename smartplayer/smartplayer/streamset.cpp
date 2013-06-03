
#include "streamset.h"
#include "stream.h"

#include <map>


std::map<std::string,Stream*>	streamset;



bool streamset_open(std::string const & name)
{
	std::map<std::string,Stream*>::iterator i = streamset.find(name);
	if( i != streamset.end() )
	{
		return true;
	}
	streamset[name] = new Stream();
	streamset[name]->start(name);
}


Stream * streamset_get(std::string const & name)
{
	std::map<std::string,Stream*>::iterator i = streamset.find(name);
	if( i != streamset.end() )
	{
		return i->second;
	}
	return NULL;
}


void streamset_idle()
{
}