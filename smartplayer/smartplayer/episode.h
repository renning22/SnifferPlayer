#pragma once
#include <vector>
#include <string>
#include <map>
#include <boost/thread/locks.hpp>
#include <boost/thread/recursive_mutex.hpp>
#include <boost/shared_ptr.hpp>

class Stream;

class Episode
{
public:
	struct Clips
	{
		std::string uri;
		Stream * stream;
		double remembered_duration; // this duration is remembered forever
		boost::shared_ptr<boost::recursive_mutex> lock_;

		Clips();
		Clips(Clips&&);
		~Clips();

		bool start();

		double duration() const;
		void remember_duration();

		boost::shared_ptr<boost::lock_guard<boost::recursive_mutex>> locker();
	};

private:
	std::vector<Clips> clips_set;

	bool has_started_;

public:
	std::string src_page;
	int width;
	int height;

public:
	Episode(std::string src_page,std::vector<std::string> uris);
	~Episode();

	bool start();
	bool stopped();
	void stop();

	void remember_duration();

	bool has_started() const;

	double duration() const;
	std::vector<std::pair<double,double>> buffer_status(bool merge=true) const;

	Clips * index(std::size_t i);
	
	std::size_t size() const;
};