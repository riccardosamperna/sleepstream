#include <pebble.h>

#define PLAY_MUSIC 1 
#define ACTIVATION_KEY 0
#define THRESHOLD 250

static Window *window;
static TextLayer *text_layer;
const uint32_t inbound_size = 64;
const uint32_t outbound_size = 64;

void out_sent_handler(DictionaryIterator *sent, void *context) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Message Sent!");
}

void out_failed_handler(DictionaryIterator *failed, AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "App Message Failed to Send!");
}

static void app_message_init(void) {
  // Register message handlers
  app_message_register_outbox_sent(out_sent_handler);
  app_message_register_outbox_failed(out_failed_handler);
  // Init buffer
  app_message_open(inbound_size, outbound_size);  
}

void send_music_activation(void) {
  DictionaryIterator *iter;

	if (app_message_outbox_begin(&iter) != APP_MSG_OK) {
    return;
  }

	Tuplet value = TupletInteger(ACTIVATION_KEY, PLAY_MUSIC);
  dict_write_tuplet(iter, &value);

	app_message_outbox_send();
}

int16_t axis_subtraction (int16_t first, int16_t second) {
	if ( first < 0 && second < 0 )
		return ((first*(-1)) - (second*(-1)));
	if (first < 0 && second > 0)
		return ((first*(-1)) - second);
	if (first > 0 && second < 0)
		return (first - (second*(-1)));

	return first - second;
}

void accel_data_handler(AccelData *data, uint32_t num_samples) {
  AccelData *temp = data;
	int16_t previous_position_x = temp->x, previous_position_y = temp->y, previous_position_z = temp->z;
  uint32_t remaining = num_samples - 1;
	int16_t biggest_x = 0, biggest_y = 0, biggest_z = 0;
	int16_t biggest = 0;
	
	temp++;

	for (uint32_t i=0; i < remaining; i++, temp++) {
		int16_t r_x = axis_subtraction(temp->x,previous_position_x);
		if (r_x > biggest_x)
			biggest_x = r_x;
		previous_position_x=temp->x;

		int16_t r_y = axis_subtraction(temp->y,previous_position_y);
		if (r_y > biggest_y)
			biggest_y = r_y;
		previous_position_y=temp->y;

		int16_t r_z = axis_subtraction(temp->z,previous_position_z);
		if (r_x > biggest_x)
			biggest_z = r_z;
		previous_position_z=temp->z;
	}

	if (biggest_x > biggest_y)
		biggest = biggest_x;
	else
		biggest = biggest_y;
	
	if (biggest_z > biggest)
		biggest = biggest_z;	

	if (biggest > THRESHOLD)
		send_music_activation();
}

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  text_layer_set_text(text_layer, "Select");
}

static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
  accel_service_set_sampling_rate(ACCEL_SAMPLING_10HZ);
  accel_data_service_subscribe(25, &accel_data_handler);
	app_message_init();
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
  accel_data_service_unsubscribe();
}

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  text_layer = text_layer_create((GRect) { .origin = { 0, 72 }, .size = { bounds.size.w, 20 } });
  text_layer_set_text(text_layer, "Press up button to start, down or back button to stop");
  text_layer_set_text_alignment(text_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(text_layer));
}

static void window_unload(Window *window) {
  text_layer_destroy(text_layer);
}

static void init(void) {
  window = window_create();
  window_set_click_config_provider(window, click_config_provider);
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  const bool animated = true;
  window_stack_push(window, animated);
}

static void deinit(void) {
  accel_data_service_unsubscribe();
  window_destroy(window);
}

int main(void) {
  init();

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", window);

  app_event_loop();
  deinit();
}