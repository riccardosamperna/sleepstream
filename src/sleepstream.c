#include <pebble.h>

#define PLAY_MUSIC 1
#define STOP_MUSIC 2 
#define ACTIVATION_KEY 0
#define STOP_KEY 1
#define THRESHOLD_CHANGE_TIME 300000
#define MINUTE_IN_MSEC 60000

static Window *window;
static TextLayer *text_layer_start;
static TextLayer *text_layer_stopMusic;
static TextLayer *text_layer_stop;
static AppTimer *datalog_timer;
static AppTimer *threshold_timer;
const uint32_t inbound_size = 64;
const uint32_t outbound_size = 64;
DataLoggingSessionRef logging_session;
int value_for_graph = 0;
int tag = 0;
int threshold = 50;

void addto_datalog (void *data) {
	DataLoggingResult result;
	int value[] = {value_for_graph};

	APP_LOG(APP_LOG_LEVEL_DEBUG, "Entrato %d", value_for_graph);

	result=data_logging_log(logging_session, &value , 1);
	
	//debugging
	if (result == DATA_LOGGING_BUSY)
		APP_LOG(APP_LOG_LEVEL_DEBUG, "Someone else is writing to this logging session.");
	if (result == DATA_LOGGING_FULL)
		APP_LOG(APP_LOG_LEVEL_DEBUG, "No more space to save data.");
	if (result == DATA_LOGGING_NOT_FOUND)
		APP_LOG(APP_LOG_LEVEL_DEBUG, "The logging session does not exist.");
	if (result == DATA_LOGGING_CLOSED)
		APP_LOG(APP_LOG_LEVEL_DEBUG, "The logging session was made inactive.");
	if (result == DATA_LOGGING_INVALID_PARAMS)
		APP_LOG(APP_LOG_LEVEL_DEBUG, "An invalid parameter was passed to one of the functions.");

	//value reset
	value_for_graph = 0;
	
	APP_LOG(APP_LOG_LEVEL_DEBUG, "Data added!");
	data_logging_finish(logging_session);

	logging_session=data_logging_create(tag, DATA_LOGGING_INT, 4, false);
	tag++;

	datalog_timer=app_timer_register(MINUTE_IN_MSEC, &addto_datalog, NULL);
}

void change_threshold (void *data) {
	threshold = 50;
}

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

void stop_music(void) {
	DictionaryIterator *iter;
	
	if (app_message_outbox_begin(&iter) != APP_MSG_OK) {
    return;
  }

	Tuplet value = TupletInteger(STOP_KEY, STOP_MUSIC);
  dict_write_tuplet(iter, &value);

	app_message_outbox_send();

	threshold = 150;

	if(threshold_timer == NULL)
		threshold_timer = app_timer_register(THRESHOLD_CHANGE_TIME, &change_threshold, NULL);
	
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

	if (biggest > threshold)
		send_music_activation();

	if(biggest > value_for_graph)
		value_for_graph = biggest;
}

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
	layer_set_hidden(text_layer_get_layer(text_layer_stop), false);
	layer_set_hidden(text_layer_get_layer(text_layer_stopMusic), false);
	layer_set_hidden(text_layer_get_layer(text_layer_start), true);

  accel_service_set_sampling_rate(ACCEL_SAMPLING_10HZ);
  accel_data_service_subscribe(25, &accel_data_handler);
	logging_session=data_logging_create(tag, DATA_LOGGING_INT, 4, false);
	tag++;
	datalog_timer=app_timer_register(MINUTE_IN_MSEC, &addto_datalog, NULL);
	app_message_init();
}

static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
	stop_music();
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
	layer_set_hidden(text_layer_get_layer(text_layer_start), false);
	layer_set_hidden(text_layer_get_layer(text_layer_stop), true);
	layer_set_hidden(text_layer_get_layer(text_layer_stopMusic), true);

  accel_data_service_unsubscribe();
	if (datalog_timer != NULL){
		app_timer_cancel(datalog_timer);
	}
	data_logging_finish(logging_session); 
	if (threshold_timer != NULL){
		app_timer_cancel(threshold_timer);
	}	
}

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  text_layer_stopMusic = text_layer_create((GRect) { .origin = { 0, 10 }, .size = { bounds.size.w, 20 } });
	text_layer_set_text(text_layer_stopMusic, "Stop music ->");
	text_layer_set_text_alignment(text_layer_stopMusic, GTextAlignmentRight);
	layer_set_hidden(text_layer_get_layer(text_layer_stopMusic), true);
	layer_add_child(window_layer, text_layer_get_layer(text_layer_stopMusic));

	text_layer_start = text_layer_create((GRect) { .origin = { 0, 65 }, .size = { bounds.size.w, 20 } });	
	text_layer_set_text(text_layer_start, "Press select to start ->");
	text_layer_set_text_alignment(text_layer_start, GTextAlignmentRight);
	layer_add_child(window_layer, text_layer_get_layer(text_layer_start));

	text_layer_stop = text_layer_create((GRect) { .origin = { 0, 120 }, .size = { bounds.size.w, 20 } });
	text_layer_set_text(text_layer_stop, "Stop monitoring ->");
	text_layer_set_text_alignment(text_layer_stop, GTextAlignmentRight); 
	layer_set_hidden(text_layer_get_layer(text_layer_stop), true);
	layer_add_child(window_layer, text_layer_get_layer(text_layer_stop));
	
}

static void window_unload(Window *window) {
  text_layer_destroy(text_layer_start);
  text_layer_destroy(text_layer_stopMusic);
	text_layer_destroy(text_layer_stop);
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
	if (datalog_timer != NULL){
		app_timer_cancel(datalog_timer);
	}
	data_logging_finish(logging_session);
	if (threshold_timer != NULL){
		app_timer_cancel(threshold_timer);
	}	
  window_destroy(window);
}

int main(void) {
  init();

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", window);

  app_event_loop();
  deinit();
}
