#pragma once

#include <vector>

void create_progresser();
void move_progresser();
void render_progresser();

bool progresser_cursor_inside_test(int xPos, int yPos);

void set_total_length(double length);
void set_buffer_pos(std::vector< std::pair<double,double> > const & buffer_status_);
void set_current_pos(double pos);

void show_progresser(bool show);
void close_progresser();

void set_alpha_progresser(float t);

void progresser_idle();

bool progresser_is_dragging();