#pragma once

#include <string>

bool reporter_init();

void reporter_uninit();

void reporter_report_login();

void reporter_report_logout();

void reporter_report(std::string const & a2,std::string const & a3);

void reporter_idle();