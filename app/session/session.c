#include "../app.h"
#include <stdio.h>
#include <string.h>

static bool stopping(const App *app) {
  return app->session_operation == SessionStopping || app->session_operation == SessionSaving;
}

static void failed(App *app, const char *message) {
  app->session_operation = stopping(app) ? SessionStopFailed : SessionStartFailed;
  snprintf(app->session_error, sizeof(app->session_error), "%s", message);
  app->session_command = 0;
  app_open(app, ScreenSessionProgress);
}

static void start_board(App *app) {
  char command[32];
  snprintf(command, sizeof(command), "start\t%s", app->session_mac);
  app->session_operation = SessionStarting;
  app->session_at = furi_get_tick();
  app->session_accepted = false;
  app->session_command = app_command(app, command);
  if (!app->session_command) failed(app, "Start could not be sent. Check the board link.");
}

void app_session_start(App *app) {
  if (app->session_operation == SessionPreparingCapture ||
      app->session_operation == SessionStarting || stopping(app)) {
    app_notice(app, "A session change is in progress. Check Active session.");
    return;
  }
  if (!app->online || (app->state != 0 && app->state != 2)) {
    app_notice(app, "Board unavailable or busy. Stop the active session before starting.");
    return;
  }
  if (!app->selected_device.mac[0]) return;
  // A web-triggered rescan may reorder results while the user edits this device's setup.
  snprintf(app->session_mac, sizeof(app->session_mac), "%s", app->selected_device.mac);
  app->service_count = 0;
  app_traffic_reset(app);
  app->session_at = furi_get_tick();
  app->session_command = 0;
  app->session_error[0] = 0;
  if (app->capture_on_start && !app->recording) {
    app->recording_return = ScreenSession;
    app_capture_toggle(app);
    if (!app->recording) return; // Keep the recorder's specific SD/link error visible.
  }
  app_open(app, ScreenSession);
  app->page = 1;
  if (app->recording && (!app->capture_ready || app->capture_stopping))
    app->session_operation = SessionPreparingCapture;
  else
    start_board(app);
}

void app_session_stop(App *app, bool exit_when_stopped) {
  bool pending = stopping(app);
  app_open(app, ScreenSessionProgress);
  app->exit_after_stop = exit_when_stopped;
  if (pending) return; // Reopening the progress screen must not restart its deadline.
  app->session_operation = SessionStopping;
  app->session_at = furi_get_tick();
  app->session_retry_at = 0;
  app->session_command = 0;
  app->session_accepted = false;
  app->session_had_capture = app->recording;
  app->session_error[0] = 0;
  app_session_refresh(app);
}

void app_session_receive(App *app, WireKind kind, uint16_t id, char **f, size_t count) {
  if (kind != WireReply || !app->session_command || id != app->session_command) return;
  if (!strcmp(f[0], "ok")) {
    app->session_accepted = true;
  } else if (!strcmp(f[0], "error")) {
    const char *message = count > 1 ? f[1] : "Session command failed.";
    if (stopping(app) && strstr(message, "transition in progress")) {
      // The board cannot safely tear Bluetooth down during initialization. Retry when ready.
      app->session_command = 0;
      app->session_accepted = false;
      app->session_retry_at = furi_get_tick();
    } else {
      failed(app, message);
    }
  }
}

static void finish_stop(App *app) {
  app->session_operation = SessionStopped;
  app->session_command = 0;
  app->target_connected = app->phone_connected = false;
  app->active_mac[0] = 0;
  if (app->exit_after_stop) {
    if (app->capture_exporting) app->exit_after_capture = true;
    else app->running = false;
  }
}

void app_session_refresh(App *app) {
  SessionOperation operation = app->session_operation;
  if (operation == SessionStopped && app->online && app->state != 0) {
    app->session_operation = SessionNone;
    if (app->screen == ScreenSessionProgress) {
      app_open(app, ScreenSession);
      app->page = 1;
    }
    return;
  }
  if (operation < SessionPreparingCapture || operation > SessionSaving) return;
  if (operation == SessionSaving) {
    if (!app->recording) {
      if (app->online && app->state != 0)
        failed(app, "Board active again after stopping. Check the other controller.");
      else
        finish_stop(app);
    }
    return; // Teardown was already confirmed; an offline save remains a partial capture.
  }
  if (!app->online) {
    failed(app, stopping(app) ? "Board link lost. Stop is not confirmed. Reconnect and retry."
                             : "Board link lost. Start is not confirmed. Check session status.");
    return;
  }
  uint32_t now = furi_get_tick();
  if (now - app->session_at > furi_ms_to_ticks(30000)) {
    failed(app, stopping(app) ? "Stop not confirmed after 30s. Check the board and retry."
                             : "Start not confirmed after 30s. Check session status.");
    return;
  }
  if (operation == SessionPreparingCapture) {
    if (!app->recording || app->capture_stopping)
      failed(app, "Capture was not ready. The session was not started; check the recording.");
    else if (app->capture_ready)
      start_board(app);
  } else if (operation == SessionStarting && app->session_accepted) {
    if (app->state == 7) {
      app->session_operation = SessionNone;
      app->session_command = 0;
    } else if (app->state == 5 || app->state == 8) {
      failed(app, "Target connection failed. Check range, then stop and try again.");
    }
  } else if (operation == SessionStopping) {
    if (app->state == 0 && app->session_command && app->session_accepted) {
      if (app->recording) {
        // Save after confirmed teardown so the final disconnect packets are included.
        app->session_operation = SessionSaving;
        app_capture_toggle(app);
      } else {
        finish_stop(app);
      }
    } else if (!app->session_command && app->state != 1 && app->state != 3 &&
               app->state != 4 && app->state != 6 &&
               (!app->session_retry_at ||
                now - app->session_retry_at >= furi_ms_to_ticks(500))) {
      app->session_command = app_command(app, "stop");
      if (!app->session_command) failed(app, "Stop could not be sent. Check the board link.");
    }
  }
}
