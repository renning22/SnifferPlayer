#pragma once

void screenui_create();

void screenui_show(bool show);

void screenui_move();

void screenui_close();

void screenui_set_opaque(bool is_opaque);

float screenui_alpha();

void screenui_alpha(float t,bool force_draw=false);

void screenui_idle();

