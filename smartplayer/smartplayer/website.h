#pragma once

#include <string>

std::string website_root_url();

std::string website_download_url();

std::string website_parser_url(std::string const & target_url);

std::string website_next_url(std::string const & target_url);

std::string website_stats_url();

std::string website_version_url(std::string const & current_version);

std::string website_flvcd_parser_url(std::string const & target_url);