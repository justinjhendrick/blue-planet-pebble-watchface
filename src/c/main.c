#include <pebble.h>

#define DEBUG_TIME (false)
#define DEBUG_BBOX (false)
#define PBL_IS_ROUND (PBL_IF_ROUND_ELSE(true, false))

#if PBL_DISPLAY_WIDTH >= 260
  #define FULL_RADIUS_INSET (8)
#else
  #define FULL_RADIUS_INSET (0)
#endif

#define COL_BG (GColorBlack)
#define COL_DIAL (GColorWhite)
#define COL_TICK (GColorBlack)
#define COL_ARM (GColorWhite)

static Window* s_window;
static Layer* s_layer;

static void debug_bbox(GContext* ctx, GRect bbox) {
  if (DEBUG_BBOX) {
    graphics_context_set_stroke_color(ctx, GColorRed);
    graphics_context_set_stroke_width(ctx, 1);
    graphics_draw_rect(ctx, bbox);
  }
}

static void debug_time(struct tm* now) {
#if DEBUG_TIME
  now->tm_min = now->tm_sec;           // [0 - 59]
  now->tm_hour = now->tm_sec % 24;     // [0 - 23]
  now->tm_mday = now->tm_sec % 31 + 1; // [1 - 31]
  now->tm_mon = now->tm_sec % 12;      // [0 - 11]
  now->tm_wday = now->tm_sec % 7;      // [0 -  6]
#endif
#ifdef HOUR_OVERRIDE
  now->tm_hour = HOUR_OVERRIDE;
#endif
#ifdef MINUTE_OVERRIDE
  now->tm_min = MINUTE_OVERRIDE;
#endif
}

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

static GRect rect_from_midpoint(GPoint midpoint, GSize size) {
  GRect ret;
  ret.origin.x = midpoint.x - size.w / 2;
  ret.origin.y = midpoint.y - size.h / 2;
  ret.size = size;
  return ret;
}

static int min(int a, int b) {
  if (a < b) {
    return a;
  }
  return b;
}

static int max(int a, int b) {
  if (a > b) {
    return a;
  }
  return b;
}


static void draw_dial(GContext* ctx, GPoint center, int dial_radius) {
  graphics_context_set_fill_color(ctx, COL_DIAL);
  graphics_fill_circle(ctx, center, dial_radius);
  graphics_context_set_fill_color(ctx, COL_BG);
  graphics_fill_circle(ctx, center, dial_radius * 17 / 20);
}

static void draw_ticks(GContext* ctx, GPoint center, int dial_radius) {
  int width_5m_ticks = 3;
  for (int16_t tick_minute = 0; tick_minute < 60; tick_minute += 5) {
    int tick_deg = tick_minute * 360 / 60;
    GPoint minute_tick_outer = cartesian_from_polar(center, dial_radius, tick_deg);
    graphics_context_set_stroke_color(ctx, COL_TICK);
    graphics_context_set_stroke_width(ctx, width_5m_ticks);
    int tick_length = dial_radius * 2 / 20;
    GPoint minute_tick_inner = cartesian_from_polar(center, dial_radius - tick_length, tick_deg);
    graphics_draw_line(ctx, minute_tick_inner, minute_tick_outer);
  }
}

static void draw_arm(GContext* ctx, GPoint c, int r, int deg) {
  graphics_context_set_stroke_color(ctx, COL_ARM);
  graphics_context_set_fill_color(ctx, COL_ARM);
  graphics_fill_circle(ctx, c, 4);
  GPoint inner = cartesian_from_polar(c, r * 4 / 20, deg);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_line(ctx, c, inner);
  graphics_context_set_stroke_width(ctx, 7);
  GPoint outer = cartesian_from_polar(c, r * 16 / 20, deg);
  graphics_draw_line(ctx, inner, outer);
}

static void update_layer(Layer* layer, GContext* ctx) {
  time_t temp = time(NULL);
  struct tm* now = localtime(&temp);
  debug_time(now);
  GRect bounds = layer_get_bounds(layer);
  
  graphics_context_set_fill_color(ctx, COL_BG);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  int full_radius = min(bounds.size.h, bounds.size.w) / 2 - FULL_RADIUS_INSET;
  int min_deg = 360 * now->tm_min / 60;

  int total_mins = 12 * 60;
  int current_mins = now->tm_hour * 60 + now->tm_min;
  int hour_angle = current_mins * TRIG_MAX_ANGLE / total_mins;
  
  GPoint bounds_center = grect_center_point(&bounds);
  GPoint min_dial_center = cartesian_from_polar_trigangle(bounds_center, full_radius / 2, hour_angle);

  draw_dial(ctx, bounds_center, full_radius);
  draw_ticks(ctx, bounds_center, full_radius);
  
  int min_rad = full_radius * 8 / 20;
  draw_dial(ctx, min_dial_center, min_rad);
  draw_arm(ctx, min_dial_center, min_rad, min_deg);
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
