#pragma once

#include <string>

bool system_tray_init();

void system_tray_uninit();

void system_tray_left_button_up();

void system_tray_right_button_up();

void system_tray_item_selected(int id);

void system_tray_states_sync_from_engine();

bool system_tray_balloon(std::string text,std::string type = "");