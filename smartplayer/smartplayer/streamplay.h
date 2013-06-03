#pragma once

struct Stream;
struct VideoState;


// not thread-safe
class StreamPlay
{
public:
	Stream * stream_;
	VideoState * is_;

public:
	StreamPlay(Stream * stream_);
	~StreamPlay();

	bool start(double start_playing_pos=0);
	void stop();
	bool stopped() const;

	bool playing() const;
	bool pausing() const;
	void toggle_pause();

	double current_playing_pos();
	
private:
	void stream_close();
};