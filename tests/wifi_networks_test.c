#include "app.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static char last_command[480];
uint16_t app_command(App *app, const char *command) {
  snprintf(last_command, sizeof(last_command), "%s", command);
  return ++app->next_id;
}

static void receive(App *app, const char *message) {
  char copy[128];
  snprintf(copy, sizeof(copy), "%s", message);
  char *fields[6];
  size_t count = wire_fields(copy, fields, 6);
  app_wifi_networks_receive(app, fields, count);
}

int main(void) {
  App app = {.screen = ScreenWifiNetworks, .online = true, .wifi_loaded = true};
  strcpy(app.wifi_ap, "Research board");
  app_wifi_scan(&app);
  receive(&app, "wifi_networks\t1\t");
  assert(app.wifi_scanning && !app.wifi_networks_loaded);
  receive(&app, "wifi_networks\t0\t");
  receive(&app, "wifi_network\t0\t486f6d65\t-80\t1"); // Home, weaker duplicate.
  receive(&app, "wifi_network\t2\t43616665\t-60\t0"); // Cafe, open.
  receive(&app, "wifi_networks_end\t3");
  assert(!app.wifi_networks_loaded); // Missing entry must not become selectable.
  receive(&app, "wifi_network\t1\t486f6d65\t-40\t1");
  receive(&app, "wifi_networks_end\t3");
  assert(app.wifi_networks_loaded && app.wifi_network_count == 2);
  assert(!strcmp(app.wifi_networks[0].ssid, "Home"));
  assert(app.wifi_networks[0].rssi == -40);
  strcpy(app.wifi_station_password, "previous-network-secret");
  app.selected = 2;
  app_wifi_networks_activate(&app);
  assert(!strcmp(app.wifi_station, "Home") && app.wifi_mode == 2);
  assert(app.wifi_edit_requested && app.wifi_field == 4 && app.wifi_dirty);
  assert(!app.wifi_station_password[0] && app.wifi_station_changed);
  assert(app.screen == ScreenWifiReview && app.selected == 0);
  // Connect cannot send a newly selected secured network with an empty password.
  app.wifi_edit_requested = false;
  uint16_t previous_command = app.next_id;
  app_wifi_activate(&app);
  assert(app.wifi_edit_requested && app.next_id == previous_command);
  app_wifi_edited(&app, "new-network-password");
  app.wifi_edit_requested = false;
  app_wifi_activate(&app);
  assert(!strncmp(last_command, "wifi_set\tboth\t", 14));
  assert(app.screen == ScreenWifiStatus && app.wifi_command);
  assert(app_wifi_back(&app) && app.screen == ScreenWifiStatus); // Save still pending.
  app.notice = false;
  app_wifi_reply(&app, app.wifi_command, "error\tCould not save");
  assert(app.screen == ScreenWifiReview && app.wifi_dirty && !app.wifi_command);
  assert(!strcmp(app.wifi_station_password, "new-network-password"));
  app.notice = false;
  app_wifi_activate(&app);
  app_wifi_reply(&app, app.wifi_command, "ok\tsaved");
  assert(app.screen == ScreenWifiStatus && !app.wifi_dirty && !app.wifi_station_password[0]);
  assert(!app.wifi_command && !strcmp(last_command, "wifi_get"));
  app_wifi_back(&app);
  assert(app.screen == ScreenWifi && app.selected == 1);

  // Selecting an open network must not retain a previous network's password.
  app.screen = ScreenWifiNetworks;
  app.selected = 3;
  app.wifi_edit_requested = false;
  strcpy(app.wifi_station_password, "another-secret");
  app_wifi_networks_activate(&app);
  assert(!strcmp(app.wifi_station, "Cafe") && !app.wifi_edit_requested);
  assert(!app.wifi_station_password[0] && app.wifi_station_changed);
  app_wifi_back(&app);
  assert(app.screen == ScreenWifiDiscard && app.selected == 0);
  app_wifi_activate(&app); // Keep editing preserves the draft.
  assert(app.screen == ScreenWifiReview && app.wifi_dirty);
  app_wifi_back(&app);
  app.selected = 1;
  app_wifi_activate(&app);
  assert(app.screen == ScreenWifiNetworks && !app.wifi_dirty && !app.wifi_loaded);
  assert(!app.wifi_station_password[0] && !strcmp(last_command, "wifi_get"));
  app.wifi_loaded = true;

  // Reject malformed records, and expose a completed empty scan rather than an endless spinner.
  app.screen = ScreenWifiNetworks;
  app_wifi_scan(&app);
  receive(&app, "wifi_networks\t0\tESP_ERR_WIFI_STATE");
  receive(&app, "wifi_network\t0\t410042\t-20\t1");
  receive(&app, "wifi_network\t16\t41\t-20\t1");
  receive(&app, "wifi_network\t0\t41\tinvalid\t1");
  assert(app.wifi_network_mask == 0);
  receive(&app, "wifi_networks_end\t0");
  assert(app.wifi_networks_loaded && !app.wifi_scanning && app.wifi_network_count == 0);
  app.selected = 1;
  app_wifi_networks_activate(&app);
  assert(app.wifi_edit_requested && app.wifi_field == 3 && app.screen == ScreenWifiReview);
  receive(&app, "wifi_networks\t1\t");
  assert(!app.wifi_scanning); // Late scan traffic cannot change a different screen.
  app.screen = ScreenMcp;
  app.selected = 0;
  app_wifi_activate(&app);
  assert(app.wifi_mcp && app.wifi_command && app.screen == ScreenMcp);
  app_wifi_reply(&app, app.wifi_command, "ok\tsaved");
  assert(!app.wifi_dirty && !app.wifi_command);
  app_wifi_back(&app);
  assert(app.screen == ScreenSettings && app.selected == 1);
  return 0;
}
