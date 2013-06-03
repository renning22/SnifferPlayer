#include "screenui.h"
#include "engine.h"
#include "ffplay.h"
#include "libpng.h"
#include "utility.hpp"
#include <SDL.h>

#include <Windows.h>

#include "screenui_close_button.h"
#include "screenui_progresser.h"
#include "screenui_play_button.h"
#include "screenui_next_button.h"
#include "screenui_time_span.h"
#include "screenui_corner.h"
#include "screenui_panel.h"
#include "screenui_center_announcement.h"
#include "screenui_scale_buttons.h"

#include "log.h"

using namespace std;

///////////////////
void screenui_create()
{
	create_panel();
	move_panel();

	create_close_button();
	move_close_button();

	create_play_button();
	move_play_button();

	create_next_button();
	move_next_button();

	create_time_span();
	move_time_span();

	create_progresser();
	move_progresser();

	//create_corner();
	//move_corner();

	create_center_announcement();
	move_center_announcement();

	create_scale_buttons();
	move_scale_buttons();
}

void screenui_show(bool show)
{
	show_panel(show);
	show_close_button(show);
	show_play_button(show);
	show_next_button(show);
	show_time_span(show);
	show_progresser(show);
	show_scale_buttons(show);
	//show_corner(show);
}

void screenui_move()
{
	move_panel();
	move_close_button();
	move_play_button();
	move_next_button();
	move_time_span();
	move_progresser();
	//move_corner();
	move_center_announcement();
	move_scale_buttons();
}

void screenui_close()
{
	close_panel();
	close_close_button();
	close_play_button();
	close_next_button();
	close_time_span();
	close_progresser();
	//close_corner();
	close_center_announcement();
	close_scale_buttons();
}

float screenui_alpha_ = 0.0f;
bool gradually_to_opaque = false;

void screenui_set_opaque(bool is_opaque)
{
	gradually_to_opaque = is_opaque;
}

float screenui_alpha()
{
	return screenui_alpha_;
}

void screenui_alpha(float t,bool force_draw)
{
	if( !force_draw )
	{
		if( screenui_alpha_ == t )
			return;
	}
	screenui_alpha_ = t;
	set_alpha_panel(t);
	set_alpha_close_button(t);
	set_alpha_play_button(t);
	set_alpha_next_button(t);
	set_alpha_time_span(t);
	set_alpha_progresser(t);
	set_alpha_scale_buttons(t);
	//set_alpha_corner(t);
}

void screenui_idle()
{
	static unsigned int frame_count = 0;
	float target_alpha = 0;
	bool force_draw = false;
	if( frame_count % 50 == 0 )
	{
		// force to redraw, cause sometimes, drawing discarded when windows are invisible.
		force_draw = true;
		engine_screen_position_changed_from_system();
	}
	frame_count++;

	if( gradually_to_opaque )
	{
		target_alpha = 1.0f;
	}
	else
	{
		target_alpha = 0.0f;
	}

	change_gradually(
	[]()->float{return screenui_alpha();},
	[=](float t){screenui_alpha(t,force_draw);},
	target_alpha,
	0.001f,
	0.3f);
}

///////////////////