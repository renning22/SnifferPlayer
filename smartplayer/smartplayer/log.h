#pragma once

#include <string>

#include "log4cpp/Category.hh"

void log_init(std::string filename);

log4cpp::Category & log_stream();

#define DEBUG (log_stream() << log4cpp::Priority::DEBUG)
#define INFO (log_stream() << log4cpp::Priority::INFO)
#define NOTICE (log_stream() << log4cpp::Priority::NOTICE)
#define WARN (log_stream() << log4cpp::Priority::WARN)
#define ERROR (log_stream() << log4cpp::Priority::ERROR)
#define FATAL (log_stream() << log4cpp::Priority::FATAL)