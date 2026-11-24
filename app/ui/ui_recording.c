#include "ui.h"
#include <stdio.h>

void ui_recording(Canvas *c, const App *app) {
  const char *state = app->capture_exporting ? "Copying to SD" :
      !app->recording ? (app->capture_complete ? "Saved" : "Capture stopped") :
      app->capture_stopping ? "Finishing RAM" : app->capture_ready ?
      app->capture_buffered ? "Recording in RAM" : "Recording" : "Starting...";
  ui_icon(c, 3, 16, app->recording ? UiRecord : UiFile);
  canvas_set_font(c, FontPrimary);
  ui_fit(c, 20, 25, 106, state);
  canvas_set_font(c, FontSecondary);
  char line[80];
  snprintf(line, sizeof(line), "%lu packets / %lu KB", (unsigned long)app->capture_packets,
           (unsigned long)(app->capture_bytes / 1024));
  canvas_draw_str(c, 3, 36, line);
  uint32_t missing = app->capture_sequence - app->capture_packets;
  if (app->capture_exporting)
    snprintf(line, sizeof(line), "SD copy: %lu / %lu KB",
             (unsigned long)(app->capture_bytes / 1024),
             (unsigned long)(app->capture_export_size / 1024));
  else if (app->capture_buffered)
    snprintf(line, sizeof(line), "Dropped: %lu  RAM: %lu KB",
             (unsigned long)app->capture_dropped,
             (unsigned long)(app->capture_capacity / 1024));
  else
    snprintf(line, sizeof(line), "Lost: ESP %lu / Link %lu",
             (unsigned long)app->capture_dropped, (unsigned long)missing);
  canvas_draw_str(c, 3, 46, line);
  ui_footer(c, app->recording_return == ScreenTarget ? "Setup" : "Session",
             app->capture_exporting ? "Wait" :
             app->recording ? (app->capture_stopping ? "Wait" : "Save") : "Done",
             0, 0);
}
