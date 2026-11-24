#include "../app.h"
#include "../core/rules.h"
#include <gui/modules/text_input.h>
#include <gui/view_dispatcher.h>
#include <stdio.h>
#include <string.h>

static const char *const MODES[] = {"ap", "station", "both"};

void app_wifi_receive(App *app, char **fields, size_t count) {
  app_wifi_networks_receive(app, fields, count);
  if (!strcmp(fields[0], "wifi_key") && count == 2) {
    if (app->screen == ScreenWifiKey && strlen(fields[1]) == 32)
      snprintf(app->wifi_key, sizeof(app->wifi_key), "%s", fields[1]);
    return;
  }
  if (strcmp(fields[0], "wifi") || (count != 8 && count != 10))
    return;
  if (!app->wifi_dirty) {
    for (unsigned i = 0; i < 3; i++)
      if (!strcmp(fields[1], MODES[i]))
        app->wifi_mode = i;
    uint8_t name[33] = {0};
    size_t length;
    if (rule_hex(fields[2], name, 32, &length)) {
      name[length] = 0;
      snprintf(app->wifi_ap, sizeof(app->wifi_ap), "%s", (char *)name);
    }
    if (rule_hex(fields[3], name, 32, &length)) {
      name[length] = 0;
      snprintf(app->wifi_station, sizeof(app->wifi_station), "%s", (char *)name);
    }
    app->wifi_mcp = !strcmp(fields[7], "1");
  }
  snprintf(app->wifi_state, sizeof(app->wifi_state), "%s", fields[4]);
  snprintf(app->wifi_ip, sizeof(app->wifi_ip), "%s", fields[5]);
  app->wifi_ap_active = !strcmp(fields[6], "1");
  app->wifi_reason = 0;
  app->wifi_error[0] = 0;
  if (count == 10) {
    wire_number(fields[8], UINT32_MAX, &app->wifi_reason);
    snprintf(app->wifi_error, sizeof(app->wifi_error), "%s", fields[9]);
  }
  app->wifi_loaded = true;
}

typedef struct {
  App *app;
  ViewDispatcher *dispatcher;
  bool accepted;
} WifiEditor;

static void edited(void *context) {
  WifiEditor *editor = context;
  editor->accepted = true;
  view_dispatcher_stop(editor->dispatcher);
}

static bool back(void *context) {
  WifiEditor *editor = context;
  view_dispatcher_stop(editor->dispatcher);
  return true;
}

static void tick(void *context) {
  WifiEditor *editor = context;
  App *app = editor->app;
  furi_mutex_acquire(app->mutex, FuriWaitForever);
  app_receive_pending(app);
  app_refresh(app);
  app_capture_refresh(app);
  app_session_refresh(app);
  furi_mutex_release(app->mutex);
}

void app_wifi_edit(App *app) {
  app->wifi_edit_requested = false;
  unsigned field = app->wifi_field;
  const char *headers[] = {"", "AP network name", "New AP password", "Network name",
                           "Network password"};
  const char *current = field == 1 ? app->wifi_ap : field == 3 ? app->wifi_station : "";
  char buffer[64];
  snprintf(buffer, sizeof(buffer), "%s", current);
  WifiEditor editor = {.app = app, .dispatcher = view_dispatcher_alloc(), .accepted = false};
  TextInput *input = text_input_alloc();
  text_input_set_header_text(input, headers[field]);
  unsigned minimum = field == 2 || (field == 4 && app->wifi_password_required) ? 8 :
      field == 4 ? 0 : 1;
  text_input_set_minimum_length(input, minimum);
  text_input_set_result_callback(input, edited, &editor, buffer, field == 1 || field == 3 ? 33 : 64,
                                 false);
  view_dispatcher_set_event_callback_context(editor.dispatcher, &editor);
  view_dispatcher_set_navigation_event_callback(editor.dispatcher, back);
  view_dispatcher_set_tick_event_callback(editor.dispatcher, tick, 20);
  view_dispatcher_add_view(editor.dispatcher, 0, text_input_get_view(input));
  // Never hold the app lock while switching GUI ownership.
  view_port_enabled_set(app->viewport, false);
  view_dispatcher_attach_to_gui(editor.dispatcher, app->gui, ViewDispatcherTypeFullscreen);
  view_dispatcher_switch_to_view(editor.dispatcher, 0);
  view_dispatcher_run(editor.dispatcher);
  view_dispatcher_remove_view(editor.dispatcher, 0);
  view_dispatcher_free(editor.dispatcher);
  text_input_free(input);
  if (editor.accepted) app_wifi_edited(app, buffer);
  memset(buffer, 0, sizeof(buffer));
  view_port_enabled_set(app->viewport, true);
}
