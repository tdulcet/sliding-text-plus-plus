#include <pebble.h>
#include "num2words.h"

typedef enum
{
	MOVING_IN,
	IN_FRAME,
	PREPARE_TO_MOVE,
	MOVING_OUT
} SlideState;

typedef struct
{
	TextLayer *label;
	SlideState state;  // animation state
	char *next_string; // what to say in the next phase of animation
	bool unchanged_font;

	int left_pos;
	int right_pos;
	int still_pos;

	int movement_delay;
	int delay_count;
} SlidingRow;

typedef struct
{
	/*TextLayer *demo_label;*/
	SlidingRow rows[7];
	int last_hour;
	int last_minute;
	int last_wday;
	int last_month;
	int last_mday;

	bool clock_24h_style;

	GFont arial_black;
	GFont arial;
	GFont arial_small;

	Window *window;
	Animation *animation;

	struct SlidingTextRenderState
	{
		// double buffered string storage
		char mdays[2][32];
		uint8_t next_mdays;
		char months[2][32];
		uint8_t next_months;
		char wdays[2][32];
		uint8_t next_wdays;
		char first_hours[2][32];
		char second_hours[2][32];
		uint8_t next_hours;
		char first_minutes[2][32];
		char second_minutes[2][32];
		uint8_t next_minutes;

		/*struct SlidingTextRenderDemoTime {
	  int secs;
	  int mins;
	  int hour;
	} demo_time;*/

	} render_state;

} SlidingTextData;

SlidingTextData *s_data;

#if PBL_DISPLAY_HEIGHT >= 200
#define LARGE_FONT_SIZE 42
#define SMALL_FONT_SIZE 20
#else
#define LARGE_FONT_SIZE 32
#define SMALL_FONT_SIZE 16
#endif

static void init_sliding_row(SlidingTextData *data, SlidingRow *row, GRect pos, GFont font,
							 int delay)
{
	row->label = text_layer_create(pos);
	text_layer_set_text_alignment(row->label, PBL_IF_ROUND_ELSE(GTextAlignmentCenter, GTextAlignmentLeft));
	text_layer_set_background_color(row->label, GColorClear);
	text_layer_set_text_color(row->label, GColorWhite);

	if (font)
	{
		text_layer_set_font(row->label, font);
		row->unchanged_font = true;
	}
	else
	{
		row->unchanged_font = false;
	}

	row->state = IN_FRAME;
	row->next_string = NULL;

	row->left_pos = pos.origin.x - pos.size.w;
	row->right_pos = pos.origin.x + pos.size.w;
	row->still_pos = pos.origin.x;

	row->movement_delay = delay;
	row->delay_count = 0;

	/*data->last_hour = -1;
  data->last_minute = -1;*/
}

static void get_sliding_row_frames(SlidingTextData *data, GRect full_bounds, GRect bounds, GRect frames[7])
{
	const int main_spacing = (((data->clock_24h_style ? 30 : 32) * LARGE_FONT_SIZE / 32) * bounds.size.h) / full_bounds.size.h;
	const int main_center = bounds.origin.y + (bounds.size.h / 2) - (((data->clock_24h_style ? PBL_IF_ROUND_ELSE(15, 13) : PBL_IF_ROUND_ELSE(34, 32)) * bounds.size.h) / full_bounds.size.h);

	frames[0] = GRect(bounds.origin.x, data->clock_24h_style ? main_center - main_spacing * 2 : main_center - main_spacing - 2, bounds.size.w, LARGE_FONT_SIZE + 10);

	frames[1] = GRect(bounds.origin.x, main_center - main_spacing - 2, bounds.size.w, LARGE_FONT_SIZE + 10);

	frames[2] = GRect(bounds.origin.x, main_center, bounds.size.w, LARGE_FONT_SIZE + 10);

	frames[3] = GRect(bounds.origin.x, main_center + main_spacing, bounds.size.w, LARGE_FONT_SIZE + 10);

	frames[4] = GRect(bounds.origin.x, bounds.origin.y + ((((data->clock_24h_style ? PBL_IF_ROUND_ELSE(5, 0) : PBL_IF_ROUND_ELSE(10, 0)) * SMALL_FONT_SIZE / 16) * bounds.size.h) / full_bounds.size.h), bounds.size.w, SMALL_FONT_SIZE + 6);

	frames[5] = GRect(bounds.origin.x, bounds.origin.y + bounds.size.h - ((((data->clock_24h_style ? PBL_IF_ROUND_ELSE(42, 35) : PBL_IF_ROUND_ELSE(52, 40)) * SMALL_FONT_SIZE / 16) * bounds.size.h) / full_bounds.size.h), bounds.size.w, SMALL_FONT_SIZE + 6);

	frames[6] = GRect(bounds.origin.x, bounds.origin.y + bounds.size.h - ((((data->clock_24h_style ? PBL_IF_ROUND_ELSE(27, 20) : PBL_IF_ROUND_ELSE(32, 20)) * SMALL_FONT_SIZE / 16) * bounds.size.h) / full_bounds.size.h), bounds.size.w, SMALL_FONT_SIZE + 6);
}

static void set_sliding_row_frame(SlidingRow *row, GRect pos)
{
	Layer *layer = text_layer_get_layer(row->label);
	GRect frame = layer_get_frame(layer);

	row->left_pos = pos.origin.x - pos.size.w;
	row->right_pos = pos.origin.x + pos.size.w;
	row->still_pos = pos.origin.x;

	if (row->state == IN_FRAME || row->state == PREPARE_TO_MOVE)
	{
		frame.origin.x = row->still_pos;
	}

	frame.origin.y = pos.origin.y;
	frame.size.w = pos.size.w;
	frame.size.h = pos.size.h;

	layer_set_frame(layer, frame);
}

static void apply_sliding_row_layout(void)
{
	SlidingTextData *data = s_data;
	Layer *window_layer = window_get_root_layer(data->window);
	GRect full_bounds = layer_get_bounds(window_layer);
	GRect bounds = layer_get_unobstructed_bounds(window_layer);

	GRect frames[7];
	get_sliding_row_frames(data, full_bounds, bounds, frames);

	for (size_t i = 0; i < ARRAY_LENGTH(data->rows); ++i)
	{
		set_sliding_row_frame(&data->rows[i], frames[i]);
	}
}

static void unobstructed_area_change(AnimationProgress progress, void *context)
{
	apply_sliding_row_layout();
}

static void slide_in_text(SlidingTextData *data, SlidingRow *row, char *new_text)
{
	(void)data;

	const char *old_text = text_layer_get_text(row->label);
	if (old_text)
	{
		row->next_string = new_text;
		row->state = PREPARE_TO_MOVE;
	}
	else
	{
		text_layer_set_text(row->label, new_text);
		Layer *layer = text_layer_get_layer(row->label);
		GRect frame = layer_get_frame(layer);
		frame.origin.x = row->right_pos;
		layer_set_frame(layer, frame);
		row->state = MOVING_IN;
	}
}

static bool update_sliding_row(SlidingTextData *data, SlidingRow *row)
{
	(void)data;

	GRect frame = layer_get_frame(text_layer_get_layer(row->label));
	bool something_changed = true;
	switch (row->state)
	{
	case PREPARE_TO_MOVE:
		frame.origin.x = row->still_pos;
		row->delay_count++;
		if (row->delay_count > row->movement_delay)
		{
			row->state = MOVING_OUT;
			row->delay_count = 0;
		}
		break;

	case MOVING_IN:
	{
		int speed = abs(frame.origin.x - row->still_pos) / 3 + 1;
		frame.origin.x -= speed;
		if (frame.origin.x <= row->still_pos)
		{
			frame.origin.x = row->still_pos;
			row->state = IN_FRAME;
		}
	}
	break;

	case MOVING_OUT:
	{
		int speed = abs(frame.origin.x - row->still_pos) / 3 + 1;
		frame.origin.x -= speed;

		if (frame.origin.x <= row->left_pos)
		{
			frame.origin.x = row->right_pos;
			row->state = MOVING_IN;
			text_layer_set_text(row->label, row->next_string);
			row->next_string = NULL;
		}
	}
	break;

	case IN_FRAME:
	default:
		something_changed = false;
		break;
	}
	if (something_changed)
	{
		layer_set_frame(text_layer_get_layer(row->label), frame);
	}
	return something_changed;
}

static void animation_update(struct Animation *animation, const AnimationProgress time_normalized)
{
	SlidingTextData *data = s_data;

	struct SlidingTextRenderState *rs = &data->render_state;

	time_t now = time(NULL);
	struct tm t = *localtime(&now);

	bool something_changed = false;

	if (data->last_minute != t.tm_min)
	{
		something_changed = true;

		minute_to_formal_words(t.tm_min, rs->first_minutes[rs->next_minutes], rs->second_minutes[rs->next_minutes]);
		if (data->last_hour != t.tm_hour || t.tm_min <= 20 || t.tm_min / 10 != data->last_minute / 10)
		{
			slide_in_text(data, &data->rows[2], rs->first_minutes[rs->next_minutes]);
		}
		else
		{
			// The tens line didn't change, so swap to the correct buffer but don't animate
			text_layer_set_text(data->rows[2].label, rs->first_minutes[rs->next_minutes]);
		}
		slide_in_text(data, &data->rows[3], rs->second_minutes[rs->next_minutes]);
		rs->next_minutes = !rs->next_minutes;
		data->last_minute = t.tm_min;
	}

	if (data->last_hour != t.tm_hour)
	{
		if (data->clock_24h_style)
		{
			hour_to_24h_word(t.tm_hour, rs->first_hours[rs->next_hours], rs->second_hours[rs->next_hours]);
			if (t.tm_hour <= 20 || t.tm_hour / 10 != data->last_hour / 10)
			{
				slide_in_text(data, &data->rows[0], rs->first_hours[rs->next_hours]);
			}
			else
			{
				// The tens line didn't change, so swap to the correct buffer but don't animate
				text_layer_set_text(data->rows[0].label, rs->first_hours[rs->next_hours]);
			}
			slide_in_text(data, &data->rows[1], rs->second_hours[rs->next_hours]);
		}
		else
		{
			hour_to_12h_word(t.tm_hour, rs->first_hours[rs->next_hours]);
			slide_in_text(data, &data->rows[0], rs->first_hours[rs->next_hours]);
		}
		rs->next_hours = !rs->next_hours;
		data->last_hour = t.tm_hour;
	}

	if (data->last_wday != t.tm_wday)
	{
		strftime(rs->wdays[rs->next_wdays], sizeof(rs->wdays[rs->next_wdays]), "%A", &t);
		slide_in_text(data, &data->rows[4], rs->wdays[rs->next_wdays]);
		rs->next_wdays = !rs->next_wdays;
		data->last_wday = t.tm_wday;
	}

	if (data->last_month != t.tm_mon)
	{
		strftime(rs->months[rs->next_months], sizeof(rs->months[rs->next_months]), "%B", &t);
		slide_in_text(data, &data->rows[5], rs->months[rs->next_months]);
		rs->next_months = !rs->next_months;
		data->last_month = t.tm_mon;
	}

	if (data->last_mday != t.tm_mday)
	{
		day_to_formal_words(t.tm_mday, rs->mdays[rs->next_mdays]);
		slide_in_text(data, &data->rows[6], rs->mdays[rs->next_mdays]);
		rs->next_mdays = !rs->next_mdays;
		data->last_mday = t.tm_mday;
	}

	for (size_t i = 0; i < ARRAY_LENGTH(data->rows); ++i)
	{
		something_changed = update_sliding_row(data, &data->rows[i]) || something_changed;
	}

	if (!something_changed)
	{
		animation_unschedule(data->animation);
	}
}

static void make_animation()
{
	s_data->animation = animation_create();
	animation_set_duration(s_data->animation, ANIMATION_DURATION_INFINITE);
	// the animation will stop itself
	static const struct AnimationImplementation s_animation_implementation = {
		.update = animation_update,
	};
	animation_set_implementation(s_data->animation, &s_animation_implementation);
	animation_schedule(s_data->animation);
}

static void handle_minute_tick(struct tm *tick_time, TimeUnits units_changed)
{
	make_animation();
}

static void handle_deinit(void)
{
	if (!s_data)
	{
		return;
	}

	tick_timer_service_unsubscribe();
	unobstructed_area_service_unsubscribe();

	for (size_t i = 0; i < ARRAY_LENGTH(s_data->rows); ++i)
	{
		if (s_data->rows[i].label)
		{
			text_layer_destroy(s_data->rows[i].label);
			s_data->rows[i].label = NULL;
		}
	}

	if (s_data->arial_black)
	{
		fonts_unload_custom_font(s_data->arial_black);
	}
	if (s_data->arial)
	{
		fonts_unload_custom_font(s_data->arial);
	}
	if (s_data->arial_small)
	{
		fonts_unload_custom_font(s_data->arial_small);
	}

	if (s_data->window)
	{
		window_destroy(s_data->window);
	}

	free(s_data);
	s_data = NULL;
}

static void handle_init()
{
	// setlocale(LC_ALL, "");

	SlidingTextData *data = (SlidingTextData *)calloc(1, sizeof(SlidingTextData));
	if (!data)
	{
		return;
	}
	s_data = data;

	data->last_hour = -1;
	data->last_minute = -1;
	data->last_wday = -1;
	data->last_month = -1;
	data->last_mday = -1;

	data->clock_24h_style = clock_is_24h_style();

	data->window = window_create();

	window_set_background_color(data->window, GColorBlack);

#if PBL_DISPLAY_HEIGHT >= 200
	data->arial_black = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_ARIAL_BLACK_42));
	data->arial = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_ARIAL_42));
	data->arial_small = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_ARIAL_20));
#else
	data->arial_black = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_ARIAL_BLACK_32));
	data->arial = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_ARIAL_32));
	data->arial_small = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_ARIAL_16));
#endif

	Layer *window_layer = window_get_root_layer(data->window);
	GRect full_bounds = layer_get_bounds(window_layer);
	GRect bounds = layer_get_unobstructed_bounds(window_layer);

	GRect row_frames[7];
	get_sliding_row_frames(data, full_bounds, bounds, row_frames);

	init_sliding_row(data, &data->rows[0], row_frames[0], data->arial_black, data->clock_24h_style ? 9 : 6);
	layer_add_child(window_layer, text_layer_get_layer(data->rows[0].label));

	init_sliding_row(data, &data->rows[1], row_frames[1], data->arial_black, 6);
	layer_add_child(window_layer, text_layer_get_layer(data->rows[1].label));

	init_sliding_row(data, &data->rows[2], row_frames[2], data->arial, 3);
	layer_add_child(window_layer, text_layer_get_layer(data->rows[2].label));

	init_sliding_row(data, &data->rows[3], row_frames[3], data->arial, 0);
	layer_add_child(window_layer, text_layer_get_layer(data->rows[3].label));

	init_sliding_row(data, &data->rows[4], row_frames[4], data->arial_small, 0);
	layer_add_child(window_layer, text_layer_get_layer(data->rows[4].label));

	init_sliding_row(data, &data->rows[5], row_frames[5], data->arial_small, 3);
	layer_add_child(window_layer, text_layer_get_layer(data->rows[5].label));

	init_sliding_row(data, &data->rows[6], row_frames[6], data->arial_small, 0);
	layer_add_child(window_layer, text_layer_get_layer(data->rows[6].label));

	UnobstructedAreaHandlers unobstructed_handlers = {
		.change = unobstructed_area_change
	};
	unobstructed_area_service_subscribe(unobstructed_handlers, NULL);

	/*GFont norm14 = fonts_get_system_font(FONT_KEY_GOTHIC_14);

  data->demo_label = text_layer_create(GRect(0, -3, 100, 20));
  text_layer_set_background_color(data->demo_label, GColorClear);
  text_layer_set_text_color(data->demo_label, GColorWhite);
  text_layer_set_font(data->demo_label, norm14);
  text_layer_set_text(data->demo_label, "demo mode");
  layer_add_child(window_layer, text_layer_get_layer(data->demo_label));

  layer_set_hidden(text_layer_get_layer(data->demo_label), true);
  layer_mark_dirty(window_layer);*/

	make_animation();

	tick_timer_service_subscribe(MINUTE_UNIT, handle_minute_tick);

	const bool animated = true;
	window_stack_push(data->window, animated);
}

int main(void)
{
	handle_init();

	app_event_loop();

	handle_deinit();
}
