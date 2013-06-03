#pragma once

#include <string>
#include <utility>
#include <cstdint>

void stream_init();

struct StreamState;
struct PacketQueue;

// not thread-safe
class Stream
{
private:
	StreamState * is;

public:
	Stream();
	~Stream();

	bool start(std::string const & url);
	bool stopped() const;
	bool failed() const;
	bool has_started() const;
	bool perfect_ending() const;
	void stop();
	bool seek(double pos);
	bool block_waiting_ready_to_play();

	double stream_buffer_pos() const;
	double stream_duration() const;


public:
	// this part is for StreamPlay
	// "video", "audio", "subtitle"
	std::string get_filename();
	int get_stream_index(std::string const & type);
	struct AVFormatContext * get_format_context();
	bool is_ready_to_play() const;
	double last_seek_pos() const;
	bool has_end_of_file() const;
	PacketQueue * get_packet_queue(std::string const & type);

	// corrected_pos is the nearest key frame
	bool query_buffer(double & corrected_pos);

};