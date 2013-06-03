#include "sniffer.h"
#include "engine.h"
#include "log.h"

static std::shared_ptr<URLSniffer>					this_sniffer;


bool sniffer_init()
{
	{
		int error_code;
		std::string error_info;
		if (Initialize(error_code, error_info) < 0)
		{
			ERROR << "sniffer : error ," <<  error_code << " , " << error_info;
			return false;
		}
	}
	this_sniffer = std::make_shared<URLSniffer>();
	StartCapture(this_sniffer);
	return true;
}

void sniffer_uninit()
{
	StopCapture();
}

void URLSniffer::GetCaptured(std::string url)
{
	//DEBUG << "Sniffed : " << url;
	engine_sniffered_from_sniffer(url);
}

void URLSniffer::GetCaptured(std::string url, std::string header)
{
	//DEBUG << "Sniffed_with_header : " << url << " , header : " << header;
	engine_sniffered_from_sniffer(url);
}