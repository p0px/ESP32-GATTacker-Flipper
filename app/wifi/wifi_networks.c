#include "../app.h"
#include "../core/rules.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void app_wifi_scan(App *app) {
  app->wifi_network_count = 0;
  app->wifi_network_mask = 0;
  app->wifi_networks_loaded = false;
  app->wifi_scanning = true;
  app->wifi_scan_error[0] = 0;
  app->selected = 0;
  app_command(app, "wifi_scan");
}

static void finish(App *app, unsigned total) {
  if (app->wifi_network_mask != (uint16_t)((1u << total) - 1))
    return; // A dropped UART record must trigger another poll, not a partial picker.
  // qsort is not exported by the Flipper SDK; this bounded list needs no library sort.
  for (unsigned i = 1; i < total; i++) {
    WifiNetwork entry = app->wifi_networks[i];
    unsigned at = i;
    while (at && app->wifi_networks[at - 1].rssi < entry.rssi) {
      app->wifi_networks[at] = app->wifi_networks[at - 1];
      at--;
    }
    app->wifi_networks[at] = entry;
  }
  unsigned kept = 0;
  for (unsigned i = 0; i < total; i++) {
    WifiNetwork entry = app->wifi_networks[i];
    if (!entry.ssid[0]) continue; // Hidden networks use manual SSID entry.
    bool duplicate = false;
    for (unsigned j = 0; j < kept; j++)
      if (!strcmp(entry.ssid, app->wifi_networks[j].ssid) &&
          entry.secured == app->wifi_networks[j].secured) duplicate = true;
    if (!duplicate) app->wifi_networks[kept++] = entry;
  }
  app->wifi_network_count = kept;
  app->wifi_scanning = false;
  app->wifi_networks_loaded = true;
  if (app->selected >= kept + 2) app->selected = 0;
}

void app_wifi_networks_receive(App *app, char **fields, size_t count) {
  if (app->screen != ScreenWifiNetworks) return;
  if (!strcmp(fields[0], "wifi_networks") && count == 3) {
    app->wifi_scanning = !strcmp(fields[1], "1");
    app->wifi_network_mask = 0;
    app->wifi_network_count = 0;
    app->wifi_networks_loaded = false;
    snprintf(app->wifi_scan_error, sizeof(app->wifi_scan_error), "%s", fields[2]);
  } else if (!strcmp(fields[0], "wifi_network") && count == 5) {
    uint32_t index;
    uint8_t name[33] = {0};
    size_t length;
    char *end;
    long rssi = strtol(fields[3], &end, 10);
    if (!wire_number(fields[1], 15, &index) || !fields[3][0] || *end ||
        rssi < -127 || rssi > 0 || (strcmp(fields[4], "0") && strcmp(fields[4], "1")) ||
        !rule_hex(fields[2], name, 32, &length) || memchr(name, 0, length)) return;
    WifiNetwork *entry = &app->wifi_networks[index];
    memcpy(entry->ssid, name, sizeof(name));
    entry->rssi = rssi;
    entry->secured = !strcmp(fields[4], "1");
    app->wifi_network_mask |= 1u << index;
  } else if (!strcmp(fields[0], "wifi_networks_end") && count == 2) {
    uint32_t total;
    if (wire_number(fields[1], 16, &total)) finish(app, total);
  }
}

void app_wifi_networks_activate(App *app) {
  if (!app->online) {
    app_notice(app, "Board offline. Check UART, then scan again.");
    return;
  }
  if (!app->wifi_loaded) {
    app_command(app, "wifi_get");
    return;
  }
  unsigned row = app->selected;
  if (!row) {
    if (!app->wifi_scanning) app_wifi_scan(app);
    return;
  }
  if (row == 1) {
    app_open(app, ScreenWifiReview);
    app->selected = 0;
    app->wifi_field = 3;
    app->wifi_edit_requested = true;
    return;
  }
  if (!app->wifi_networks_loaded || row - 2 >= app->wifi_network_count) return;
  const WifiNetwork *entry = &app->wifi_networks[row - 2];
  bool changed = strcmp(app->wifi_station, entry->ssid) != 0;
  snprintf(app->wifi_station, sizeof(app->wifi_station), "%s", entry->ssid);
  if (!app->wifi_mode) app->wifi_mode = 2; // Keep the recovery AP when selecting a network.
  app->wifi_dirty = true;
  app->wifi_password_required = entry->secured;
  if (changed || !entry->secured) {
    memset(app->wifi_station_password, 0, sizeof(app->wifi_station_password));
    app->wifi_station_changed = true;
  }
  app_open(app, ScreenWifiReview);
  app->selected = 0;
  if (entry->secured && changed) {
    app->wifi_field = 4;
    app->wifi_edit_requested = true;
  }
}
