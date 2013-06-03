#include "website.h"

std::string website_root_url()
{
	return "http://nn-kk.com";
}

std::string website_download_url()
{
	return "http://nn-kk.com";
}

std::string website_parser_url(std::string const & target_url)
{
	return "http://nn-kk.com/?url="+target_url;
}

std::string website_next_url(std::string const & target_url)
{
	return "http://nn-kk.com/Next.aspx?url="+target_url;
}

std::string website_stats_url()
{
	return "http://nn-kk.com/Stats.aspx";
}

std::string website_version_url(std::string const & current_version)
{
	return "http://nn-kk.com/Version.aspx?version="+current_version;
}

std::string website_flvcd_parser_url(std::string const & target_url)
{
	return "http://www.flvcd.com/parse.php?kw="+target_url;
}