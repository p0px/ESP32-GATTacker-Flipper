#include "../app.h"
#include "../core/rules.h"
#include <stdio.h>
#include <string.h>

bool app_wifi_screen(Screen screen) {
  return (screen >= ScreenWifi && screen <= ScreenWifiNetworks) ||
      (screen >= ScreenMcp && screen <= ScreenWifiDiscard);
}

static void clear_draft(App *app) {
  app->wifi_dirty = app->wifi_ap_changed = app->wifi_station_changed = false;
  app->wifi_password_required = false;
  memset(app->wifi_ap_password, 0, sizeof(app->wifi_ap_password));
  memset(app->wifi_station_password, 0, sizeof(app->wifi_station_password));
}

static void save(App *app) {
  static const char *const modes[] = {"ap", "station", "both"};
  char command[480], ap[65], sta[65], ap_pass[127] = "-", sta_pass[127] = "-";
  rule_to_hex((uint8_t *)app->wifi_ap, strlen(app->wifi_ap), ap);
  rule_to_hex((uint8_t *)app->wifi_station, strlen(app->wifi_station), sta);
  if (app->wifi_ap_changed)
    rule_to_hex((uint8_t *)app->wifi_ap_password, strlen(app->wifi_ap_password), ap_pass);
  if (app->wifi_station_changed)
    rule_to_hex((uint8_t *)app->wifi_station_password, strlen(app->wifi_station_password),
                sta_pass);
  snprintf(command, sizeof(command), "wifi_set\t%s\t%s\t%s\t%s\t%s\t%u", modes[app->wifi_mode],
           ap, ap_pass, sta, sta_pass, app->wifi_mcp);
  app->wifi_save_return = app->screen;
  app->wifi_command = app_command(app, command);
  memset(command, 0, sizeof(command));
  memset(ap_pass, 0, sizeof(ap_pass));
  memset(sta_pass, 0, sizeof(sta_pass));
  if (app->wifi_command && app->screen != ScreenMcp) app_open(app, ScreenWifiStatus);
}

void app_wifi_reply(App *app, uint16_t id, const char *text) {
  if (!app->wifi_command || id != app->wifi_command) return;
  app->wifi_command = 0;
  if (!strncmp(text, "ok\t", 3)) {
    clear_draft(app);
    app_command(app, "wifi_get");
  } else {
    app_open(app, app->wifi_save_return);
    if (app->screen == ScreenMcp) {
      clear_draft(app);
      app->wifi_loaded = false;
      app_command(app, "wifi_get");
    }
    app_notice(app, !strncmp(text, "error\t", 6) ? text + 6 : text);
  }
}

void app_wifi_edited(App *app, const char *text) {
  unsigned field = app->wifi_field;
  if (field == 3 && strcmp(app->wifi_station, text)) {
    // Never reuse a previous network's password after editing its SSID.
    memset(app->wifi_station_password, 0, sizeof(app->wifi_station_password));
    app->wifi_station_changed = true;
    app->wifi_password_required = false; // Manual entry also supports open networks.
  }
  char *out = field == 1 ? app->wifi_ap : field == 2 ? app->wifi_ap_password :
      field == 3 ? app->wifi_station : app->wifi_station_password;
  snprintf(out, field == 1 || field == 3 ? 33 : 64, "%s", text);
  if (field == 2) app->wifi_ap_changed = true;
  if (field == 4) app->wifi_station_changed = true;
  app->wifi_dirty = true;
}

void app_wifi_activate(App *app) {
  unsigned row = app->selected;
  if (app->screen == ScreenWifiDiscard) {
    Screen from = app->wifi_discard_from;
    if (!row) {
      app_open(app, from);
      app->selected = app->wifi_discard_row;
    } else {
      clear_draft(app);
      app->wifi_loaded = false;
      app_command(app, "wifi_get");
      app_open(app, from == ScreenWifiReview ? ScreenWifiNetworks : ScreenWifi);
      if (from == ScreenWifiAp) app->selected = 2;
    }
    return;
  }
  if (app->wifi_command) return;
  if (!app->online || !app->wifi_loaded) {
    app_command(app, "wifi_get");
    app_notice(app, "Waiting for Wi-Fi settings. Check the board connection.");
    return;
  }
  if (app->screen == ScreenWifi) {
    app_open(app, row == 0 ? ScreenWifiNetworks : row == 1 ? ScreenWifiStatus : ScreenWifiAp);
    if (!row) app_wifi_scan(app);
  } else if (app->screen == ScreenMcp) {
    if (!row) {
      app->wifi_mcp = !app->wifi_mcp;
      app->wifi_dirty = true;
      save(app);
    } else {
      app_open(app, row == 1 ? ScreenMcpInfo : ScreenWifiKey);
      if (row == 2) app_command(app, "wifi_key");
    }
  } else if (app->screen == ScreenWifiReview) {
    if (!row) {
      if (!app->wifi_station[0]) {
        app_notice(app, "Choose a network or enter its name first.");
      } else if (app->wifi_password_required && app->wifi_station_changed &&
                 strlen(app->wifi_station_password) < 8) {
        app->wifi_field = 4;
        app->wifi_edit_requested = true;
      } else {
        if (!app->wifi_mode) app->wifi_mode = 2;
        save(app);
      }
    } else {
      app->wifi_field = row == 1 ? 3 : 4;
      app->wifi_edit_requested = true;
    }
  } else if (app->screen == ScreenWifiAp) {
    if (!row) save(app);
    else if (row == 3) {
      app->wifi_mode = (app->wifi_mode + 1) % 3;
      app->wifi_dirty = true;
    } else {
      app->wifi_field = row;
      app->wifi_edit_requested = true;
    }
  }
}

bool app_wifi_back(App *app) {
  if (!app_wifi_screen(app->screen)) return false;
  if (app->wifi_command) {
    app_notice(app, "Saving settings. Please wait for the board reply.");
    return true;
  }
  Screen screen = app->screen;
  if ((screen == ScreenWifiReview || screen == ScreenWifiAp) && app->wifi_dirty) {
    app->wifi_discard_from = screen;
    app->wifi_discard_row = app->selected;
    app_open(app, ScreenWifiDiscard);
  } else if (screen == ScreenWifiDiscard) {
    app_open(app, app->wifi_discard_from);
    app->selected = app->wifi_discard_row;
  } else if (screen == ScreenWifi || screen == ScreenMcp) {
    app_open(app, ScreenSettings);
    app->selected = screen == ScreenMcp ? 1 : 0;
  } else if (screen == ScreenWifiKey || screen == ScreenMcpInfo) {
    memset(app->wifi_key, 0, sizeof(app->wifi_key));
    app_open(app, ScreenMcp);
    app->selected = screen == ScreenWifiKey ? 2 : 1;
  } else if (screen == ScreenWifiReview) {
    app_open(app, ScreenWifiNetworks);
  } else {
    app_open(app, ScreenWifi);
    app->selected = screen == ScreenWifiStatus ? 1 : screen == ScreenWifiAp ? 2 : 0;
  }
  return true;
}
