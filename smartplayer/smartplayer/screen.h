#pragma once

#include <utility>
#include <boost/tuple/tuple.hpp>
#include <inttypes.h>

// Not thread-safe excepted "screen_draw_begin()" and "screen_draw_end()", which
// can be called by another thread while calling in the other functions.

bool screen_create();
bool screen_close();

bool screen_global_existing();

void screen_show(bool show);
bool screen_cursor_inside_test(int x,int y);
void screen_position(int x,int y);
std::pair<int,int> screen_position();
void screen_size(int width,int height,bool raletive_center=true);
void screen_size_gracefully(int width,int height);
std::pair<int,int> screen_size();
float screen_alpha();
void screen_alpha(float alpha);
void screen_corner_clip(bool clip);
void screen_set_opaque_mode(bool opaque);

void screen_center_in_monitor();

bool screen_fullscreen();
void screen_toggle_fullscreen();
bool screen_minimized();
void screen_toggle_minimize();
void screen_toggle_restore();

bool screen_topmost();
void screen_toggle_topmost();

const void * screen_hwnd();

boost::tuple<uint8_t*,int,int,int> screen_draw_begin();
void screen_draw_end();
