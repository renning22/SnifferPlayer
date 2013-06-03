#pragma once

#include <string>

void create_center_announcement();

void close_center_announcement();

void move_center_announcement();

void center_announcement_show(bool show);

void center_announcement_alpha(float t);

float center_announcement_alpha();

void center_announcement_caption(std::string content);

void center_annoucement_idle();