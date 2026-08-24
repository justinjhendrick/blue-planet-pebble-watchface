#include <pebble.h>

#define DEBUG_TIME (false)

#define COL_BG (GColorOxfordBlue)
#define COL_DIAL (GColorWhite)
#define COL_TICK (GColorOxfordBlue)
#define COL_ARM (GColorWhite)
#define COL_HDOT (GColorBlue)
#define COL_TEXT (GColorWhite)

static Window* s_window;
static Layer* s_layer;

static GPoint cartesian_from_polar_trigangle(GPoint center, int radius, int trigangle) {
  GPoint ret = {
    .x = (int16_t)(sin_lookup(trigangle) * radius / TRIG_MAX_RATIO) + center.x,
    .y = (int16_t)(-cos_lookup(trigangle) * radius / TRIG_MAX_RATIO) + center.y,
  };
  return ret;
}

static GPoint cartesian_from_polar(GPoint center, int radius, int angle_deg) {
  return cartesian_from_polar_trigangle(center, radius, DEG_TO_TRIGANGLE(angle_deg));
}

static void draw_dial(GContext* ctx, GPoint center, int dial_radius, int thick) {
  graphics_context_set_fill_color(ctx, COL_DIAL);
  graphics_fill_circle(ctx, center, dial_radius);
  graphics_context_set_fill_color(ctx, COL_BG);
  graphics_fill_circle(ctx, center, dial_radius - thick);
}

static void draw_ticks(GContext* ctx, GPoint center, int dial_radius, int thick, int s) {
  for (int16_t tick_minute = 0; tick_minute < 60; tick_minute += 5) {
    int tick_deg = tick_minute * 360 / 60;
    GPoint minute_tick_outer = cartesian_from_polar(center, dial_radius - s, tick_deg);
    graphics_context_set_stroke_color(ctx, COL_TICK);
    graphics_context_set_stroke_width(ctx, 3);
    GPoint minute_tick_inner = cartesian_from_polar(center, dial_radius - thick + s, tick_deg);
    graphics_draw_line(ctx, minute_tick_inner, minute_tick_outer);
  }
}

static void draw_arm(GContext* ctx, GPoint c, int r, int deg) {
  graphics_context_set_stroke_color(ctx, COL_ARM);
  graphics_context_set_fill_color(ctx, COL_ARM);
  graphics_fill_circle(ctx, c, 4);
  GPoint inner = cartesian_from_polar(c, r * 6 / 20, deg);
  graphics_context_set_stroke_width(ctx, 3);
  graphics_draw_line(ctx, c, inner);
  graphics_context_set_stroke_width(ctx, 7);
  GPoint outer = cartesian_from_polar(c, r - 3, deg);
  graphics_draw_line(ctx, inner, outer);
}

static void update_layer(Layer* layer, GContext* ctx) {
  time_t temp = time(NULL);
  static int frames = 0;
  if (DEBUG_TIME) {
    temp += (5 * 60 - 1) * frames;
    frames++;
  }
  struct tm* now = localtime(&temp);
  GRect bounds = layer_get_bounds(layer);
  
  graphics_context_set_fill_color(ctx, COL_BG);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  int border = 0;
  int full_radius = bounds.size.w / 2 + border;
  int min_deg = 360 * now->tm_min / 60;

  int total_mins = 12 * 60;
  int current_mins = now->tm_hour * 60 + now->tm_min;
  int hour_angle = current_mins * TRIG_MAX_ANGLE / total_mins;
  
  GPoint bounds_center = GPoint(
    bounds.origin.x + bounds.size.w / 2,
    bounds.origin.y + bounds.size.w / 2
  );
  int min_dial_center_rad = full_radius * 6 / 20;
  GPoint min_dial_center = cartesian_from_polar_trigangle(bounds_center, min_dial_center_rad, hour_angle);
  int dial_thick = full_radius * 4 / 20;
  int mdial_thick = full_radius * 3 / 40;

  // hours
  draw_dial(ctx, bounds_center, full_radius, dial_thick);
  int tick_setback = 4;
  draw_ticks(ctx, bounds_center, full_radius, dial_thick, tick_setback);
  
  // minutes
  int min_rad = full_radius - dial_thick - min_dial_center_rad;
  draw_dial(ctx, min_dial_center, min_rad, mdial_thick);
  draw_arm(ctx, min_dial_center, min_rad - mdial_thick, min_deg);
  
  // hour mark
  GPoint hmark_inner = cartesian_from_polar_trigangle(bounds_center, min_dial_center_rad + min_rad - mdial_thick + tick_setback, hour_angle);
  GPoint hmark_outer = cartesian_from_polar_trigangle(bounds_center, full_radius - tick_setback, hour_angle);
  graphics_context_set_stroke_color(ctx, GColorOxfordBlue);
  graphics_context_set_stroke_width(ctx, 9);
  graphics_draw_line(ctx, hmark_inner, hmark_outer);
  graphics_context_set_stroke_width(ctx, 5);
  graphics_context_set_stroke_color(ctx, GColorCyan);
  graphics_draw_line(ctx, hmark_inner, hmark_outer);

  // date
  graphics_context_set_text_color(ctx, COL_TEXT);
  int corner_h = 32;
  GRect date_box = GRect(
    bounds.origin.x,
    bounds.origin.y + bounds.size.h - corner_h,
    bounds.size.w / 3,
    corner_h
  );
  int mid_h = 28;
  GRect day_box = GRect(
    date_box.origin.x + date_box.size.w,
    bounds.origin.y + bounds.size.h - mid_h,
    date_box.size.w,
    mid_h
  );
  GRect time_box = GRect(
    day_box.origin.x + day_box.size.w,
    date_box.origin.y,
    date_box.size.w,
    date_box.size.h
  );
  GFont f_lg = fonts_get_system_font(FONT_KEY_GOTHIC_28);
  GFont f_sm = fonts_get_system_font(FONT_KEY_GOTHIC_24);
  char t[40];
  strftime(t, 40, "%b %d", now);
  graphics_draw_text(ctx, t, f_lg, date_box, GTextOverflowModeFill, GTextAlignmentLeft, NULL);
  strftime(t, 40, "%a", now);
  graphics_draw_text(ctx, t, f_sm, day_box, GTextOverflowModeFill, GTextAlignmentCenter, NULL);
  strftime(t, 40, "%H:%M", now);
  graphics_draw_text(ctx, t, f_lg, time_box, GTextOverflowModeFill, GTextAlignmentRight, NULL);
}

static void window_load(Window* window) {
  Layer* window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  s_layer = layer_create(bounds);
  layer_set_update_proc(s_layer, update_layer);
  layer_add_child(window_layer, s_layer);
}

static void window_unload(Window* window) {
  if (s_layer) layer_destroy(s_layer);
}

static void tick_handler(struct tm* now, TimeUnits units_changed) {
  if (s_layer) layer_mark_dirty(s_layer);
}

static void init(void) {
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);
  tick_timer_service_subscribe(DEBUG_TIME ? SECOND_UNIT : MINUTE_UNIT, tick_handler);
}

static void deinit(void) {
  if (s_window) window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
