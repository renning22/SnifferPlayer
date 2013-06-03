#pragma once

#include <utility>

void create_panel();
void move_panel();


void show_panel(bool show);
void close_panel();

void set_alpha_panel(float t);

std::pair<int,int> panel_position();
std::pair<int,int> panel_size();

const void * screenui_panel_hwnd();

bool screenui_panel_cursor_near_test(int x,int y);