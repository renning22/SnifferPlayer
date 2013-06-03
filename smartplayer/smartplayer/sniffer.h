#ifndef _SNIFFER_H_
#define _SNIFFER_H_

#include "libCapGet.h"

#include <string>

bool sniffer_init();

void sniffer_uninit();

// caution, multi-threads visiting this class
class URLSniffer: public CapGetDelegate
{
public:
	void GetCaptured(std::string url);

	void GetCaptured(std::string url, std::string header);
};


#endif
