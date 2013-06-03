#include "log4cpp/Category.hh"
#include "log4cpp/Appender.hh"
#include "log4cpp/FileAppender.hh"
#include "log4cpp/RollingFileAppender.hh"
#include "log4cpp/OstreamAppender.hh"
#include "log4cpp/Layout.hh"
#include "log4cpp/BasicLayout.hh"
#include "log4cpp/PatternLayout.hh"
#include "log4cpp/Priority.hh"

#ifdef _DEBUG
void log_init(std::string filename)
{
	CreateDirectory("log",NULL);
	auto layout1 = new log4cpp::PatternLayout();
	layout1->setConversionPattern("%d [%p] %m%n");
	log4cpp::Appender *appender1 = new log4cpp::RollingFileAppender("default", "log\\"+filename,4*1024*1024,1024);
	appender1->setLayout(layout1);

	auto layout2 = new log4cpp::PatternLayout();
	layout2->setConversionPattern("%d [%p] %m%n");
	log4cpp::Appender *appender2 = new log4cpp::OstreamAppender("console", &std::cout);
	appender2->setLayout(layout2);

	log4cpp::Category & root = log4cpp::Category::getRoot();
	root.setPriority(log4cpp::Priority::DEBUG);
	root.addAppender(appender1);
	root.addAppender(appender2);

	root << log4cpp::Priority::INFO << "Log inited";
}
#else
void log_init(std::string filename)
{
	CreateDirectory("log",NULL);
	auto layout1 = new log4cpp::PatternLayout();
	layout1->setConversionPattern("%d [%p] %m%n");
	log4cpp::Appender *appender1 = new log4cpp::RollingFileAppender("default", "log\\"+filename,4*1024*1024,1024);
	appender1->setLayout(layout1);

	log4cpp::Category & root = log4cpp::Category::getRoot();
	root.setPriority(log4cpp::Priority::WARN);
	root.addAppender(appender1);

	root << log4cpp::Priority::INFO << "Log inited";
}
#endif

log4cpp::Category & log_stream()
{
	return log4cpp::Category::getRoot();
}