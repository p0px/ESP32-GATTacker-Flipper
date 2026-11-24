#include "../app.h"
#include <stdio.h>

const TrafficEvent *app_traffic_at(const App *app, unsigned index) {
  return &app->events[(app->event_head + index) % EVENT_COUNT];
}

void app_traffic_resume(App *app) {
  app->paused = false;
  app->selected = app->event_count ? app->event_count - 1 : 0;
}

void app_traffic_reset(App *app) {
  app->event_head = app->event_count = app->event_total = app->event_at = 0;
  app->paused = false;
}

void app_traffic_receive(App *app, char **fields, size_t count) {
  if (count != 7) return;
  unsigned index = (app->event_head + app->event_count) % EVENT_COUNT;
  if (app->event_count == EVENT_COUNT) {
    app->event_head = (app->event_head + 1) % EVENT_COUNT;
    // Keep the browsed item in place until it ages out of the bounded window.
    if (app->screen == ScreenTraffic && app->paused && app->selected) app->selected--;
  } else {
    app->event_count++;
  }
  TrafficEvent *event = &app->events[index];
  event->operation = fields[1][0];
  snprintf(event->direction, sizeof(event->direction), "%s", fields[2]);
  snprintf(event->uuid, sizeof(event->uuid), "%s", fields[3]);
  snprintf(event->handle, sizeof(event->handle), "%s", fields[4]);
  snprintf(event->data, sizeof(event->data), "%s", fields[5]);
  snprintf(event->detail, sizeof(event->detail), "%s", fields[6]);
  app->event_total++;
  app->event_at = furi_get_tick();
  if (app->screen == ScreenTraffic && !app->paused) app_traffic_resume(app);
}
