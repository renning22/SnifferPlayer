#pragma once

#include <vector>
#include <string>
#include <utility>

// caution: not thread-safe, only called from engine main thread!
// (except a few one instruction functions.)

int ffplay_init(int argc,char* argv[]);

int ffplay_uninit();

bool ffplay_episode_start(std::string src_page,std::vector<std::string> urls);

bool ffplay_episode_next_clips();

void ffplay_episode_toggle_pause();

bool ffplay_episode_playing();

bool ffplay_episode_pausing();

void ffplay_episode_stop();

bool ffplay_episode_stopped();

void ffplay_episode_seek(double pos);

std::string ffplay_episode_src_page();

double ffplay_episode_current_playing_pos();

double ffplay_episode_how_long_buffered(double tolerance_buffer_secs);

std::vector<std::pair<double,double>> ffplay_episode_buffer_status(bool merge=true);

double ffplay_episode_duration();

void ffplay_streamplay_size_obtain_from_engine(int width,int height);

std::pair<int,int> ffplay_episode_size();

void ffplay_clips_buffered_up_from_engine(std::string filename);

void ffplay_idle();
