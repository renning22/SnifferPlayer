#pragma once

#include <string>

void create_time_span();
void move_time_span();

void time_span_set_caption(std::string content);
void time_span_set_caption_two_times_format(double a,double b);

void show_time_span(bool show);
void close_time_span();

void set_alpha_time_span(float t);

void time_span_idle();