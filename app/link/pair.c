#include "../app.h"
#include <stdio.h>
#include <string.h>

void app_pair_receive(App *app, WireKind kind, char **fields, size_t count) {
  if (kind != WireEvent || count != 3 || strcmp(fields[0], "pair_request")) return;
  uint32_t id;
  size_t length = strlen(fields[2]);
  if (!wire_number(fields[1], UINT32_MAX, &id) || !id || !length || length > 15)
    return;
  for (size_t i = 0; i < length; i++)
    if ((fields[2][i] < '0' || fields[2][i] > '9') && fields[2][i] != '.') return;
  if (app->screen != ScreenPair) app->pair_return = app->screen;
  app->pair_id = id;
  app->pair_at = furi_get_tick();
  snprintf(app->pair_ip, sizeof(app->pair_ip), "%s", fields[2]);
  app->notice = false;
  app_open(app, ScreenPair);
}

void app_pair_reply(App *app, bool allow) {
  if (!app->pair_id) return;
  char command[32];
  snprintf(command, sizeof(command), "pair_%s\t%lu", allow ? "allow" : "deny",
           (unsigned long)app->pair_id);
  app_command(app, command);
  app->pair_id = 0;
  app_open(app, app->pair_return);
}

void app_pair_refresh(App *app) {
  if (app->screen != ScreenPair) return;
  if (app->online && furi_get_tick() - app->pair_at < furi_ms_to_ticks(60000)) return;
  app->pair_id = 0;
  app_open(app, app->pair_return);
}
