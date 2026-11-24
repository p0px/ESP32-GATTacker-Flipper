#include "../app.h"
#include <stdio.h>
#include <string.h>

void app_capture_report(App *app) {
  if (!app->online || !app->capture_remote_capable) return;
  const char *state = app->capture_exporting ? "saving" : app->recording ?
      app->capture_buffered ? "empty" :
      app->capture_stopping ? "saving" :
      app->capture_ready ? "recording" : "starting" : !app->capture_path[0] ? "empty" :
      app->capture_complete ? "ready" : "error";
  uint32_t missing = app->capture_sequence > app->capture_packets ?
      app->capture_sequence - app->capture_packets : 0;
  uint32_t dropped = app->capture_dropped + missing;
  if ((app->capture_buffered && !app->capture_exporting) || !strcmp(state, "empty"))
    dropped = 0;
  uint32_t token = app->capture_buffered && !app->capture_exporting ? 0 : app->capture_token;
  uint32_t packets = app->capture_buffered && !app->capture_exporting ? 0 : app->capture_packets;
  uint32_t bytes = app->capture_buffered && !app->capture_exporting ? 0 : app->capture_bytes;
  const char *path = app->capture_buffered && !app->capture_exporting ? "" : app->capture_path;
  char command[256];
  snprintf(command, sizeof(command), "capture_sink\t1\t%u\t%lu\t%s\t%lu\t%lu\t%lu\t%u\t%s",
           storage_sd_status(app->storage) == FSE_OK, (unsigned long)token, state,
           (unsigned long)packets, (unsigned long)dropped,
           (unsigned long)bytes, app->capture_complete, path);
  app_command(app, command);
}

static void reject(App *app, uint32_t id, unsigned reason) {
  char command[64];
  snprintf(command, sizeof(command), "capture_sink_error\t%lu\t%u", (unsigned long)id, reason);
  app_command(app, command);
}

void app_capture_remote(App *app, WireKind kind, char **fields, size_t count) {
  if (kind != WireEvent || count != 3 || strcmp(fields[0], "capture_remote")) return;
  uint32_t id;
  if (!wire_number(fields[2], UINT32_MAX, &id) || !id) return;
  if (!strcmp(fields[1], "start")) {
    // Retransmission of a completed request must never create another file.
    if (app->capture_remote_last == id) { app_capture_report(app); return; }
    if (app->recording || app->capture_exporting || !app->online ||
        app->wifi_edit_requested || app->browse_requested) {
      reject(app, id, 1);
      return;
    }
    if (storage_sd_status(app->storage) != FSE_OK) { reject(app, id, 2); return; }
    app->capture_remote_last = id;
    app->capture_requested_token = id;
    app->recording_return = app_session_active(app) ? ScreenSession : ScreenHome;
    app_capture_toggle(app);
    app->capture_requested_token = 0;
    if (!app->recording) { reject(app, id, 3); return; }
  } else if (!strcmp(fields[1], "stop")) {
    if (id != app->capture_token) { reject(app, id, 1); return; }
    if (app->recording && !app->capture_stopping) app_capture_toggle(app);
  } else return;
  app_capture_report(app);
}
