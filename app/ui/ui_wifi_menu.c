#include "ui.h"
#include <stdio.h>

void ui_wifi_label(const App *app, unsigned row, char *out, size_t capacity) {
  const char *label = "";
  switch (app->screen) {
  case ScreenWifi:
    label = row == 0 ? "Join network" : row == 1 ? "Connection status" : "Hotspot / mode";
    break;
  case ScreenMcp:
    label = row == 0 ? (app->wifi_command ? "MCP: saving..." :
            app->wifi_mcp ? "MCP: on" : "MCP: off") :
        row == 1 ? "Server address" : "Access key";
    break;
  case ScreenWifiReview:
    if (row == 1) {
      snprintf(out, capacity, "SSID: %s", app->wifi_station[0] ? app->wifi_station : "not set");
      return;
    }
    label = row == 0 ? "Connect" :
        !app->wifi_station_changed ? "Password: saved" : app->wifi_station_password[0] ?
        "Password: entered" : app->wifi_password_required ? "Password: required" :
        "Password: open network";
    break;
  case ScreenWifiAp:
    if (row == 1) {
      snprintf(out, capacity, "Name: %s", app->wifi_ap);
      return;
    }
    label = row == 0 ? (app->wifi_dirty ? "Apply changes *" : "Apply settings") :
        row == 2 ? (app->wifi_ap_changed ? "Password: entered" : "Password: saved") :
        app->wifi_mode == 0 ? "Mode: hotspot only" :
        app->wifi_mode == 1 ? "Mode: network only" : "Mode: network + AP";
    break;
  case ScreenWifiDiscard:
    label = row == 0 ? "Keep editing" : "Discard changes";
    break;
  default: break;
  }
  snprintf(out, capacity, "%s", label);
}

UiIcon ui_wifi_icon(const App *app, unsigned row) {
  if (app->screen == ScreenWifi) return row == 0 ? UiRadar : row == 1 ? UiLink : UiPlug;
  if (app->screen == ScreenMcp) return row == 0 ? UiChip : row == 1 ? UiLink : UiRules;
  if (app->screen == ScreenWifiReview) return row == 0 ? UiLink : row == 1 ? UiRadar : UiRules;
  if (app->screen == ScreenWifiAp)
    return row == 0 ? UiFile : row == 1 ? UiRadar : row == 2 ? UiRules : UiPlug;
  return row == 0 ? UiBack : UiStop;
}

const char *ui_wifi_action(const App *app) {
  if (app->screen == ScreenMcp && !app->selected)
    return app->wifi_command ? "Wait" : app->wifi_mcp ? "Disable" : "Enable";
  if (app->screen == ScreenWifiReview) return app->selected ? "Edit" : "Connect";
  if (app->screen == ScreenWifiAp) return !app->selected ? "Apply" :
      app->selected == 3 ? "Change" : "Edit";
  if (app->screen == ScreenWifiDiscard) return "Choose";
  return "Open";
}
