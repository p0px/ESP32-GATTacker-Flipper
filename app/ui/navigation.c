#include "../app.h"
#include <stdio.h>

const char *app_state_name(unsigned state) {
  static const char *const names[] = {"Idle",     "Scanning",   "Scan complete",
                                      "Starting", "Connecting", "Connect failed",
                                      "Cloning",  "Running",    "Failed"};
  return state < 9 ? names[state] : "Unknown";
}

void app_open(App *app, Screen screen) {
  app->screen = screen;
  app->exit_after_stop = false;
  app->selected = 0;
  app->page = 0;
  app->page_count = 1;
}

void app_notice(App *app, const char *message) {
  snprintf(app->message, sizeof(app->message), "%s", message);
  app->notice = true;
}

bool app_session_active(const App *app) {
  return app->state >= 3 ||
      (app->session_operation >= SessionPreparingCapture &&
       app->session_operation <= SessionSaving);
}

bool app_has_session(const App *app) {
  return app_session_active(app) || app->session_mac[0];
}

HomeItem app_home_item(const App *app, unsigned index) {
  return (HomeItem)(index + (!app_has_session(app) && index >= HomeSession));
}

unsigned app_home_index(const App *app, HomeItem item) {
  if (item == HomeSession && !app_has_session(app)) return HomeScan;
  return item - (!app_has_session(app) && item > HomeSession);
}

void app_home_select(App *app, HomeItem item) {
  app_open(app, ScreenHome);
  app->selected = app_home_index(app, item);
}

unsigned app_rows(const App *app) {
  switch (app->screen) {
  case ScreenHome: return app_has_session(app) ? 6 : 5;
  case ScreenDevices: return app->device_count;
  case ScreenTarget: return 4;
  case ScreenSession: return app_session_active(app) ? 6 : 5;
  case ScreenTraffic: return app->event_count;
  case ScreenServices: return app->service_count;
  case ScreenSettings: case ScreenWifi: case ScreenMcp: case ScreenWifiReview: return 3;
  case ScreenUart: case ScreenWifiAp: return 4;
  case ScreenWifiDiscard: return 2;
  case ScreenWifiNetworks: return 2 + app->wifi_network_count;
  case ScreenExit: return 3;
  default: return 1;
  }
}
