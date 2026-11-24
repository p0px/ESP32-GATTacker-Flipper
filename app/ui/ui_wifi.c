#include "ui.h"
#include <stdio.h>
#include <string.h>

static const char *connection_label(const char *state) {
  const char *names[][2] = {
      {"ap", "Access point"}, {"connected", "Connected"}, {"connecting", "Joining network"},
      {"waiting_for_ip", "Getting IP address"}, {"connection_failed", "Join failed"},
      {"authentication_failed", "Authentication failed"},
      {"network_not_found", "Network not found"},
      {"connection_timeout", "Connection timed out"},
  };
  for (unsigned i = 0; i < sizeof(names) / sizeof(names[0]); i++)
    if (!strcmp(state, names[i][0])) return names[i][1];
  return state;
}

static void networks(Canvas *c, const App *app) {
  unsigned count = app_rows(app), start = app->selected / 3 * 3;
  for (unsigned i = start; i < start + 3 && i < count; i++) {
    unsigned y = 14 + (i - start) * 13;
    ui_card(c, y, 13, i == app->selected);
    canvas_set_font(c, i == app->selected ? FontPrimary : FontSecondary);
    if (i < 2) {
      const char *label = i == 1 ? "Enter SSID manually" : !app->online ? "Board offline - retry" :
          app->wifi_scanning ? "Scanning..." : app->wifi_scan_error[0] ? "Scan failed - retry" :
          "Scan again";
      ui_fit(c, 4, y + 10, 115, label);
    } else {
      const WifiNetwork *entry = &app->wifi_networks[i - 2];
      ui_fit(c, 4, y + 10, 90, entry->ssid);
      char signal[8];
      snprintf(signal, sizeof(signal), "%d", entry->rssi);
      canvas_set_font(c, FontSecondary);
      canvas_draw_str_aligned(c, 120, y + 10, AlignRight, AlignBottom, signal);
    }
    canvas_set_color(c, ColorBlack);
  }
  if (count == 2) {
    canvas_set_font(c, FontSecondary);
    ui_fit(c, 4, 50, 119, app->wifi_scan_error[0] ? app->wifi_scan_error :
           app->wifi_networks_loaded ? "No named networks found" : "Searching 2.4 GHz Wi-Fi");
  }
  const char *action = app->selected < 2 ? (app->selected ? "Type" : "Scan") :
      app->wifi_networks[app->selected - 2].secured ? "Password" : "Select";
  ui_scroll(c, start, count, 3);
  ui_footer(c, "Back", action, app->selected, count);
}

void ui_wifi(Canvas *c, const App *app) {
  if (app->screen == ScreenWifiNetworks) {
    networks(c, app);
    return;
  }
  if (app->screen == ScreenWifiKey) {
    if (!app->wifi_key[0])
      ui_text(c, "Requesting access key...", 0);
    else {
      char text[96];
      snprintf(text, sizeof(text), "MCP bearer key\n%.16s\n%.16s\nKeep this private.",
               app->wifi_key, app->wifi_key + 16);
      ui_text(c, text, 0);
    }
    ui_footer(c, "Back", "", 0, 0);
    return;
  }
  if (app->screen == ScreenMcpInfo) {
    char text[160];
    snprintf(text, sizeof(text), "MCP %s\nhttp://%s/mcp\nAuth: bearer key",
             app->wifi_mcp ? "enabled" : "disabled",
             app->wifi_ip[0] ? app->wifi_ip : "1.3.3.7");
    ui_text(c, text, 0);
    ui_footer(c, "Back", "", 0, 0);
    return;
  }
  if (app->screen == ScreenWifiStatus) {
    char text[192], detail[64] = "Web UI: use either IP";
    if (app->wifi_reason)
      snprintf(detail, sizeof(detail), "Retrying; reason %lu", (unsigned long)app->wifi_reason);
    else if (app->wifi_error[0])
      snprintf(detail, sizeof(detail), "%s", app->wifi_error);
    snprintf(text, sizeof(text), "%s\nAP: %s\nLAN: %s\n%s",
             app->wifi_command ? "Applying settings..." :
             app->online ? connection_label(app->wifi_state) : "Board offline",
             app->wifi_ap_active ? "1.3.3.7" : "off",
             app->wifi_ip[0] && app->online ? app->wifi_ip : "not connected", detail);
    ui_text(c, text, 0);
    ui_footer(c, "Back", "", 0, 0);
    return;
  }
  if (!app->wifi_loaded) {
    ui_empty(c, UiPlug, "Reading settings", "Waiting for ESP32");
    ui_footer(c, "Back", "Retry", 0, 0);
    return;
  }
  ui_menu(c, app);
}
