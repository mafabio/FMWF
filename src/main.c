#include <pebble.h>

static Window *s_main_window;
static TextLayer *s_time_layer;
static TextLayer *s_bt_layer;
static TextLayer *s_date_layer;
static Layer *s_canvas_layer;
static GFont s_time_font;
static GFont s_bt_font;
static int s_battery_level;

static void update_time() {
  // Get a tm structure
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);

  // Write the current hours and minutes into a buffer
  static char s_buffer[8];
  static char s_buffer_date[14];
  strftime(s_buffer, sizeof(s_buffer), clock_is_24h_style() ?
                                          "%k:%M" : "%l:%M", tick_time);
  strftime(s_buffer_date, sizeof(s_buffer_date), "%a\n%D", tick_time);

  // Display this time on the TextLayer
  text_layer_set_text(s_time_layer, s_buffer);
  text_layer_set_text(s_date_layer, s_buffer_date);
}

static void battery_handler(BatteryChargeState charge_state) {
  s_battery_level = charge_state.charge_percent;
  layer_mark_dirty(s_canvas_layer);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_time();
}

static void bt_update_label(bool connected) {
  if (connected) {
    text_layer_set_text_color(s_bt_layer, GColorBlack);
    text_layer_set_text(s_bt_layer, "connected");
  } else {
    text_layer_set_text_color(s_bt_layer, GColorRed);
    text_layer_set_text(s_bt_layer, "not connected");
    vibes_long_pulse();
  }
}

static void canvas_update_proc(Layer *this_layer, GContext *ctx) {
  // read bounds
  GRect bounds = layer_get_bounds(this_layer);
  
  // modifying inset if needed
  //GRect frame = grect_inset(bounds, GEdgeInsets(30));
  
  // battery indicator color
  if (s_battery_level >= 40)
    graphics_context_set_fill_color(ctx, GColorGreen);
  if ((s_battery_level < 40) && (s_battery_level > 20))
    graphics_context_set_fill_color(ctx, GColorYellow);
  if (s_battery_level <= 20)
    graphics_context_set_fill_color(ctx, GColorRed);
  
  // drawing battery indicator
  graphics_fill_radial(ctx, bounds, GOvalScaleModeFitCircle, 12,
                       DEG_TO_TRIGANGLE(0), 
                       DEG_TO_TRIGANGLE((360*s_battery_level)/100));
}

static void main_window_load(Window *window) {
  // Get information about the Window
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  
  // get battery level
  BatteryChargeState charge_state = battery_state_service_peek();
  s_battery_level = charge_state.charge_percent;
  
  // BATTERY LEVEL DISPLAY layer
  s_canvas_layer = layer_create(GRect(0, 0, bounds.size.w, bounds.size.h));
  layer_add_child(window_layer, s_canvas_layer);
  layer_set_update_proc(s_canvas_layer, canvas_update_proc);
  layer_mark_dirty(s_canvas_layer);
  
  // CREATING FONTS for time
  s_time_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_INSOMNIA_REGULAR_48));
  // for btluetooth and date
  s_bt_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_INSOMNIA_REGULAR_16));
  
  // TIME DISPLAY
  s_time_layer = text_layer_create(
      GRect(0, PBL_IF_ROUND_ELSE(58, 52), bounds.size.w, 50));
  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_text_color(s_time_layer, GColorBlack);
  //text_layer_set_text(s_time_layer, "00:00");
  text_layer_set_font(s_time_layer, s_time_font);
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);
  // Add it as a child layer to the Window's root layer
  layer_add_child(window_layer, text_layer_get_layer(s_time_layer));
  
  // BLUETOOTH create text layer for bt connection feedback
  s_bt_layer = text_layer_create(
      GRect(0, 120, bounds.size.w, 25));
  text_layer_set_background_color(s_bt_layer, GColorClear);
  text_layer_set_text_color(s_bt_layer, GColorBlack);
  // update text
  bt_update_label(connection_service_peek_pebble_app_connection());
  text_layer_set_font(s_bt_layer, s_bt_font);
  text_layer_set_text_alignment(s_bt_layer, GTextAlignmentCenter);
  // Add it as a child layer to the Window's root layer
  layer_add_child(window_layer, text_layer_get_layer(s_bt_layer));
  
  // DATE create text layer
  s_date_layer = text_layer_create(
      GRect(0, 25, bounds.size.w, 50));
  text_layer_set_background_color(s_date_layer, GColorClear);
  text_layer_set_text_color(s_date_layer, GColorBlack);
  // update text
  text_layer_set_font(s_date_layer, s_bt_font);
  text_layer_set_text_alignment(s_date_layer, GTextAlignmentCenter);
  // Add it as a child layer to the Window's root layer
  layer_add_child(window_layer, text_layer_get_layer(s_date_layer));  
}

static void main_window_unload(Window *window) {
  // Destroy time TextLayer
  text_layer_destroy(s_time_layer);
  // Destroy bt TextLayer
  text_layer_destroy(s_bt_layer);
  // Destroy date display layer
  text_layer_destroy(s_date_layer);
  // Destroy canvas layer
  layer_destroy(s_canvas_layer);
  // Unload GFont
  fonts_unload_custom_font(s_time_font);
}

static void init() {
  // Create main Window element and assign to pointer
  s_main_window = window_create();

  // Set handlers to manage the elements inside the Window
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });

  // Show the Window on the watch, with animated=true
  window_stack_push(s_main_window, true);
  
  // Setting locale
  setlocale(LC_ALL, "");
  
  // Make sure the time is displayed from the start
  update_time();
  
  // Register with TickTimerService
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  
  // subscribe for battery change level notification
  battery_state_service_subscribe(battery_handler);
  
  // subscribe to bt service status updates
  connection_service_subscribe((ConnectionHandlers) {
    .pebble_app_connection_handler = bt_update_label
  });
}

static void deinit() {
  // Destroy Window
  window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
