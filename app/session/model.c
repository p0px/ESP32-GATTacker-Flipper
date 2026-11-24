#include "../app.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void sort_devices(App *app) {
  // qsort is not exported to external apps by the supported firmware SDKs.
  for (unsigned i = 1; i < app->device_count; i++) {
    Device device = app->devices[i];
    unsigned j = i;
    while (j && (app->devices[j - 1].rssi < device.rssi ||
        (app->devices[j - 1].rssi == device.rssi &&
         strcmp(app->devices[j - 1].mac, device.mac) > 0))) {
      app->devices[j] = app->devices[j - 1];
      j--;
    }
    app->devices[j] = device;
  }
}

static void device_receive(App *app, char **f, size_t count) {
  if (count != 5)
    return;
  uint32_t index;
  if (!wire_number(f[1], DEVICE_COUNT - 1, &index))
    return;
  Device *device = &app->devices[index];
  snprintf(device->mac, sizeof(device->mac), "%s", f[2]);
  snprintf(device->name, sizeof(device->name), "%s", f[4][0] ? f[4] : "Unnamed");
  device->rssi = atoi(f[3]);
  if (app->device_count <= index)
    app->device_count = index + 1;
}

static void service_receive(App *app, char **f) {
  uint32_t handle;
  if (app->service_count == SERVICE_COUNT || !wire_number(f[2], UINT16_MAX, &handle))
    return;
  Service service = {};
  service.handle = handle;
  service.characteristic = !strncmp(f[1], "C:", 2);
  if (service.characteristic) {
    char *end;
    unsigned long flags = strtoul(f[1] + 2, &end, 16);
    if (*end || end == f[1] + 2 || flags > UINT8_MAX)
      return;
    service.properties = flags;
  } else if (strcmp(f[1], "S"))
    return;
  snprintf(service.uuid, sizeof(service.uuid), "%s", f[3]);
  app->services[app->service_count++] = service;
}

void app_receive(void *context, WireKind kind, uint16_t id, const char *text) {
  App *app = context;
  if (kind == WireCommand)
    return;
  HomeItem selected_home = app->screen == ScreenHome
      ? app_home_item(app, app->selected) : HomeScan;
  app->online = true;
  app->last_rx = furi_get_tick();
  if (kind == WireReply && app->scan_id && id == app->scan_id)
    app->scan_id = 0;
  if (kind == WireReply && app->services_id && id == app->services_id)
    app->services_id = 0;
  if (kind == WireReply && app->results_id && id == app->results_id) {
    app->results_id = 0;
    app->results_loaded = !strncmp(text, "ok\t", 3);
    // Sort only after the indexed stream ends; sorting earlier corrupts later indices.
    if (app->results_loaded)
      sort_devices(app);
  }
  if (kind == WireReply && id == app->pending_id) {
    app->pending_id = 0;
    if (!strncmp(text, "error\t", 6)) {
      const char *message = strchr(text, '\t');
      snprintf(app->message, sizeof(app->message), "%s", message ? message + 1 : text);
      app->scan_active = false;
      app->notice = true;
    }
  }
  if (kind == WireReply) app_wifi_reply(app, id, text);
  char record[WIRE_MAX];
  snprintf(record, sizeof(record), "%s", text);
  char *f[12];
  size_t count = wire_fields(record, f, 12);
  if (!count)
    return;
  app_wifi_receive(app, f, count);
  app_pair_receive(app, kind, f, count);
  app_capture_receive(app, kind, id, f, count);
  app_export_receive(app, kind, f, count);
  app_capture_remote(app, kind, f, count);
  app_session_receive(app, kind, id, f, count);
  if (!strcmp(f[0], "status") && count == 8) {
    unsigned previous = app->state;
    app->state = (unsigned)atoi(f[1]);
    app->target_connected = atoi(f[2]) != 0;
    app->phone_connected = atoi(f[3]) != 0;
    app->browser_hooks = atoi(f[4]) != 0;
    app->board_dropped = (uint32_t)strtoul(f[6], NULL, 10);
    const char *ram = strstr(f[7], "ram:");
    const char *largest = strstr(f[7], "largest:");
    const char *timing = strstr(f[7], "uart_us:");
    app->internal_ram = ram ? (uint32_t)strtoul(ram + 4, NULL, 10) : 0;
    app->largest_block = largest ? (uint32_t)strtoul(largest + 8, NULL, 10) : 0;
    app->telemetry_us = timing ? (uint32_t)strtoul(timing + 8, NULL, 10) : 0;
    if (previous != 1 && app->state == 1) {
      app->scan_started = furi_get_tick();
      app->device_count = 0;
      app->results_loaded = false;
      app->scan_active = true;
      if (app->screen == ScreenDevices)
        app->selected = 0;
    }
    if (!app->scan_id && app->state == 0 && app->scan_active) {
      app->scan_active = false;
      snprintf(app->message, sizeof(app->message), "Scan interrupted; retry");
    }
    if (app->state == 2)
      app->scan_active = false;
    // Fetch once per completed scan, including an empty result set.
    if (app->state == 2 && !app->results_loaded && !app->scan_id && !app->results_id)
      app->results_id = app_command(app, "results");
    if (previous != 7 && app->state == 7 && !app->service_count)
      app->services_id = app_command(app, "services");
  } else if (!strcmp(f[0], "target") && count == 2) {
    snprintf(app->active_mac, sizeof(app->active_mac), "%s", f[1]);
    // Keep local start intent fixed until the board has finished that transition.
    if (f[1][0] && app->session_operation != SessionPreparingCapture &&
        app->session_operation != SessionStarting)
      snprintf(app->session_mac, sizeof(app->session_mac), "%s", f[1]);
  } else if (!strcmp(f[0], "device"))
    device_receive(app, f, count);
  else if (!strcmp(f[0], "traffic"))
    app_traffic_receive(app, f, count);
  else if (!strcmp(f[0], "service") && count == 4) {
    service_receive(app, f);
  }
  // A session started from the web UI must not move the user's highlighted home action.
  if (app->screen == ScreenHome)
    app->selected = app_home_index(app, selected_home);
  if (app->screen == ScreenSession && !app->page && app->selected >= app_rows(app))
    app->selected = app_rows(app) - 1;
}
