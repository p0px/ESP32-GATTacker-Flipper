#include "ui.h"
#include <stdio.h>
#include <string.h>

static void radar(Canvas *c, unsigned phase) {
  static const int8_t ends[][2] = {{0, -12}, {8, -8}, {12, 0}, {8, 8},
                                  {0, 12}, {-8, 8}, {-12, 0}, {-8, -8}};
  canvas_draw_circle(c, 19, 34, 13);
  canvas_draw_circle(c, 19, 34, 7);
  canvas_draw_disc(c, 19, 34, 1);
  canvas_draw_line(c, 19, 34, 19 + ends[phase % 8][0], 34 + ends[phase % 8][1]);
  canvas_draw_box(c, 23, 26, 2, 2);
  canvas_draw_dot(c, 13, 39);
}

void ui_devices(Canvas *c, const App *app) {
  if (!app->online) {
    ui_empty(c, UiPlug, "No board link", "Check UART setup");
  } else if (app->scan_id || app->state == 1 || app->results_id) {
    unsigned elapsed = (furi_get_tick() - app->scan_started) / furi_ms_to_ticks(1000);
    radar(c, furi_get_tick() / furi_ms_to_ticks(150));
    canvas_set_font(c, FontPrimary);
    canvas_draw_str(c, 39, 26, app->results_id ? "Loading..." : "Scanning...");
    canvas_set_font(c, FontSecondary);
    char text[32];
    snprintf(text, sizeof(text), "%us / about 10s", elapsed);
    canvas_draw_str(c, 39, 38, app->results_id ? "Receiving devices" : text);
    canvas_draw_rframe(c, 39, 44, 84, 5, 1);
    unsigned progress = elapsed < 10 ? elapsed * 8 : 80;
    if (progress) canvas_draw_box(c, 41, 46, progress, 1);
  } else if (app->device_count) {
    unsigned first = (app->selected / 2) * 2;
    for (unsigned row = 0; row < 2 && first + row < app->device_count; row++) {
      unsigned index = first + row, y = 14 + row * 20;
      const Device *device = &app->devices[index];
      char signal[12];
      ui_card(c, y, 20, index == app->selected);
      ui_fit(c, 4, y + 8, 92, device->name);
      snprintf(signal, sizeof(signal), "%d", device->rssi);
      canvas_draw_str_aligned(c, 120, y + 8, AlignRight, AlignBottom, signal);
      canvas_draw_str(c, 4, y + 17, device->mac);
      unsigned bars = device->rssi > -55 ? 4 : device->rssi > -70 ? 3
                                           : device->rssi > -85 ? 2 : 1;
      for (unsigned bar = 0; bar < bars; bar++)
        canvas_draw_box(c, 112 + bar * 2, y + 18 - (bar + 1) * 2, 1, (bar + 1) * 2);
      canvas_set_color(c, ColorBlack);
    }
    ui_scroll(c, first, app->device_count, 2);
    ui_footer(c, "Back", "Open", app->selected, app->device_count);
    return;
  } else {
    ui_empty(c, UiRadar, "No devices", "OK to scan again");
  }
  ui_footer(c, "Back", app->online && app->state != 1 && !app->results_id ? "Scan" : "", 0, 0);
}

void ui_target_info(Canvas *c, const App *app) {
  const Device *device = &app->selected_device;
  char signal[40];
  // The full advertised name is available on a second page instead of being cut off.
  if (app->page) {
    ui_text(c, device->name, 0);
    ui_footer(c, "Setup", "Done", 1, 2);
    return;
  }
  ui_icon(c, 3, 17, UiTarget);
  canvas_set_font(c, FontPrimary);
  ui_fit(c, 20, 25, 106, device->name);
  canvas_set_font(c, FontSecondary);
  canvas_draw_str(c, 3, 38, device->mac);
  if (device->rssi)
    snprintf(signal, sizeof(signal), "%d dBm / %s", device->rssi,
             device->rssi > -65 ? "Strong signal"
             : device->rssi > -85 ? "Fair signal" : "Weak signal");
  else
    snprintf(signal, sizeof(signal), "Signal not available");
  ui_fit(c, 3, 50, 123, signal);
  ui_footer(c, "Setup", "Done", 0, 2);
}

static void link_line(Canvas *c, int from, int to, bool connected) {
  if (connected) {
    canvas_draw_line(c, from, 32, to, 32);
    canvas_draw_line(c, to - 3, 30, to, 32);
    canvas_draw_line(c, to - 3, 34, to, 32);
  } else {
    for (int x = from; x < to; x += 4)
      canvas_draw_line(c, x, 32, x + 1, 32);
  }
}

void ui_session(Canvas *c, const App *app) {
  const char *state = !app->online ? "Board disconnected"
                      : app->session_operation == SessionPreparingCapture ? "Preparing PCAP..."
                      : app->session_operation == SessionStopping ? "Stopping session..."
                      : app->session_operation == SessionSaving ? "Saving PCAP..."
                      : app->session_operation == SessionStarting && app->state < 3 ? "Starting..."
                      : !app_session_active(app) ? "Previous session"
                      : app->state == 7 && !app->target_connected ? "Target disconnected"
                      : app->state == 7 && !app->phone_connected ? "Waiting for central"
                      : app->state == 7 ? "Connected" : app_state_name(app->state);
  ui_fit(c, 2, 22, 124, state);
  ui_icon(c, 13, 25, UiPhone);
  ui_icon(c, 53, 26, UiChip);
  ui_icon(c, 93, 25, UiTarget);
  link_line(c, 28, 47, app->online && app->phone_connected);
  link_line(c, 70, 87, app->online && app->target_connected);
  canvas_draw_str(c, 1, 46, "Central");
  canvas_draw_str_aligned(c, 59, 46, AlignCenter, AlignBottom, "ESP32");
  canvas_set_font(c, FontPrimary);
  canvas_draw_str_aligned(c, 99, 46, AlignCenter, AlignBottom, "Target");
  canvas_set_font(c, FontSecondary);
  canvas_draw_str_aligned(c, 127, 54, AlignRight, AlignBottom, "(peripheral)");
  ui_footer(c, "Actions", "Menu", 0, 0);
}
