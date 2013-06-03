#ifndef _ENGINE_H_
#define _ENGINE_H_

#include <vector>
#include <string>

bool engine_sniffer_mode();

void engine_sniffer_mode(bool on);

void engine_sniffered_from_sniffer(std::string src_page);

void engine_open_stream_request_from_sniffer(std::string src_page,std::vector<std::string> urls);

void engine_streamplay_size_obtain_from_ffplay(int width,int height);

void engine_episode_size_obtain_from_ffplay(int width,int height);

void engine_clips_buffered_up_from_stream(std::string filename);

void engine_clips_no_buffer_alarm_from_streamplay(std::string which_part);

void engine_clips_playing_stopped_from_streamplay();

void engine_episode_playing_stopped_from_ffplay();

void engine_episode_playing_failed(std::string reason);

void engine_play_pause_from_screenui();

void engine_next_from_screenui();

void engine_parsed_from_next_url(std::string src_page,std::vector<std::string> urls);

void engine_screen_size(double ratio);

void engine_screen_position_changed_from_system();

void engine_screen_close_from_screenui();

void engine_screen_close_from_system();

void engine_screen_toggle_minimize_from_system();

void engine_screen_toggle_maximize_from_system();

void engine_screen_restore_from_system();

void engine_screen_toggle_topmost();

void engine_screen_mouse_left_up();

void engine_screen_mouse_left_down();

void engine_screen_mouse_move();

void engine_screen_keyboard_left();

void engine_screen_keyboard_right();

bool engine_screen_opaque_mode();

void engine_screen_opaque_mode(bool opaque);

void engine_episode_loading(bool on);

bool engine_episode_loading();

/////////////////////////

void engine_drag_progresser_move_from_screenui(double current_pos);

void engine_drag_progresser_end_from_screenui(double current_pos);

void engine_redraw_progresser_from_screenui();

enum corner_dir
{
	LEFTUP,
	LEFTBOTTOM,
	RIGHTUP,
	RIGHTBOTTOM
};

void engine_drag_corner_start(corner_dir dir, int x, int y);

void engine_drag_corner_move(corner_dir dir, int x, int y);

void engine_drag_corner_end(corner_dir dir, int x, int y);

void engine_abort();

#endif
