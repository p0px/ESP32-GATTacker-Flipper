#include "ui.h"
#include <stdio.h>
#include <string.h>

void ui_target(Canvas *c, const App *app) {
  const char *labels[] = {"Start session", app->recording ? "Recording status"
      : app->capture_on_start ? "Record PCAP: ON" : "Record PCAP: off",
      "Hooks and rules", "Target info"};
  const UiIcon icons[] = {UiLink, UiRecord, UiRules, UiTarget};
  canvas_draw_str(c, 3, 23, app->selected_device.mac);
  unsigned first = (app->selected / 2) * 2;
  for (unsigned row = 0; row < 2; row++) {
    unsigned index = first + row, y = 27 + row * 13;
    bool selected = index == app->selected;
    ui_card(c, y, 13, selected);
    ui_icon(c, 3, y + 1, icons[index]);
    canvas_set_font(c, selected ? FontPrimary : FontSecondary);
    ui_fit(c, 19, y + 10, 101, labels[index]);
    canvas_set_color(c, ColorBlack);
  }
  ui_footer(c, app->target_return == ScreenSession ? "Session" : "List",
      app->selected == 0 ? "Start"
      : app->selected == 1 && !app->recording ? "Toggle" : "Open", app->selected, 4);
}

void ui_session_progress(Canvas *c, const App *app) {
  SessionOperation operation = app->session_operation;
  if (operation == SessionStopFailed || operation == SessionStartFailed) {
    ui_text(c, app->session_error, 0);
    ui_footer(c, "Session", operation == SessionStopFailed ? "Retry" : "Menu", 0, 0);
    return;
  }
  bool done = operation == SessionStopped;
  bool saving = operation == SessionSaving;
  ui_icon(c, 3, 17, saving ? UiRecord : UiStop);
  canvas_set_font(c, FontPrimary);
  ui_fit(c, 20, 25, 106, done ? "Session stopped" : saving ? "Saving PCAP..." : "Stopping...");
  canvas_set_font(c, FontSecondary);
  char line[64];
  const char *detail = done ? "Board confirmed idle"
      : saving ? "Draining queued packets"
      : app->session_command ? "Waiting for board" : "Finishing connection";
  ui_fit(c, 3, 37, 121, detail);
  if (done && app->session_had_capture) {
    snprintf(line, sizeof(line), "%s: %lu packets",
             app->capture_complete ? "Saved" : "Partial", (unsigned long)app->capture_packets);
    ui_fit(c, 3, 49, 121, line);
  } else if (done) {
    canvas_draw_str(c, 3, 49, "No active connection");
  } else {
    unsigned phase = furi_get_tick() / furi_ms_to_ticks(200) % 4;
    for (unsigned i = 0; i < 4; i++) {
      if (i == phase) canvas_draw_box(c, 3 + i * 9, 44, 6, 5);
      else canvas_draw_frame(c, 3 + i * 9, 44, 6, 5);
    }
  }
  ui_footer(c, "Session", done ? "Done" : "", 0, 0);
}
