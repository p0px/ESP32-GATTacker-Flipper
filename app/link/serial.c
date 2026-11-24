#include "../app.h"
#include <stdio.h>
#include <string.h>

static const uint32_t BAUD[] = {115200, 230400, 460800};

static void serial_rx(FuriHalSerialHandle *handle, FuriHalSerialRxEvent event, void *context) {
  App *app = context;
  if (event & FuriHalSerialRxEventData) {
    uint8_t byte = furi_hal_serial_async_rx(handle);
    if (!furi_stream_buffer_send(app->rx, &byte, 1, 0))
      app->rx_dropped++;
  }
}

bool app_serial_open(App *app) {
  FuriHalSerialId id = app->port ? FuriHalSerialIdLpuart : FuriHalSerialIdUsart;
  app->serial = furi_hal_serial_control_acquire(id);
  if (!app->serial) {
    app_notice(app, "UART is busy. Close the other GPIO app, then apply UART setup again.");
    return false;
  }
  furi_hal_serial_init(app->serial, BAUD[app->baud_index]);
  furi_hal_serial_async_rx_start(app->serial, serial_rx, app, false);
  app_command(app, "hello");
  return true;
}

void app_serial_close(App *app) {
  if (!app->serial)
    return;
  furi_hal_serial_async_rx_stop(app->serial);
  furi_hal_serial_deinit(app->serial);
  furi_hal_serial_control_release(app->serial);
  app->serial = NULL;
  app->online = false;
  app->capture_capable = false;
  app->capture_remote_capable = false;
  app->capture_ram_capable = false;
  app->pending_id = 0;
  app->scan_id = 0;
  app->results_id = 0;
  app->services_id = 0;
  app->results_loaded = false;
  memset(&app->parser, 0, sizeof(app->parser));
  furi_stream_buffer_reset(app->rx);
}

uint16_t app_command(App *app, const char *command) {
  if (!app->serial) {
    app_notice(app, "UART unavailable. Open UART setup and apply the correct pins and baud.");
    return 0;
  }
  uint8_t *frame = malloc(WIRE_FRAME_MAX);
  if (!frame) {
    app_notice(app, "Not enough memory to send the command.");
    return 0;
  }
  if (++app->next_id == 0)
    app->next_id++;
  size_t size = wire_encode(WireCommand, app->next_id, command, frame, WIRE_FRAME_MAX);
  bool passive = !strcmp(command, "status") || !strcmp(command, "wifi_get") ||
      !strcmp(command, "wifi_networks") ||
      !strncmp(command, "pair_allow\t", 11) || !strncmp(command, "pair_deny\t", 10) ||
      !strncmp(command, "capture_sink", 12) ||
      !strncmp(command, "capture_export_ack\t", 19) ||
      !strncmp(command, "capture_keepalive\t", 18);
  // Session changes have their own reply IDs and wait for authoritative status, not just ACK.
  bool session = !strcmp(command, "stop") || !strncmp(command, "start\t", 6);
  if (size) {
    furi_hal_serial_tx(app->serial, frame, size);
    if (!passive && !session) {
      app->pending_id = app->next_id;
      app->pending_at = furi_get_tick();
      if (!app->notice)
        snprintf(app->message, sizeof(app->message), "Waiting for board...");
    }
  }
  free(frame);
  return size ? app->next_id : 0;
}

void app_refresh(App *app) {
  uint32_t now = furi_get_tick();
  if (now - app->last_poll > furi_ms_to_ticks(1000)) {
    app->last_poll = now;
    app_command(app, app->online ? "status" : "hello");
    app_capture_report(app);
    if (app_wifi_screen(app->screen) && app->screen != ScreenWifiNetworks)
      app_command(app, "wifi_get");
    if (app->screen == ScreenWifiNetworks && !app->wifi_networks_loaded)
      app_command(app, "wifi_networks");
  }
  if (now - app->last_rx > furi_ms_to_ticks(4000)) {
    app->online = false;
    app->scan_id = 0;
    app->results_id = 0;
    app->services_id = 0;
    app->results_loaded = false;
  }
  if (app->pending_id && now - app->pending_at > furi_ms_to_ticks(3000)) {
    if (app->scan_id == app->pending_id)
      app->scan_id = 0;
    if (app->results_id == app->pending_id)
      app->results_id = 0;
    if (app->services_id == app->pending_id)
      app->services_id = 0;
    uint16_t expired = app->pending_id;
    app->pending_id = 0;
    app_wifi_reply(app, expired, "error\tNo settings reply. Check the board before retrying.");
    app_notice(app, "No reply from board. Check the connection before retrying the command.");
  }
}

void app_receive_pending(App *app) {
    uint8_t bytes[256];
    size_t count;
    /* Bound each turn so a continuous UART stream cannot starve buttons. */
    for (unsigned batch = 0; batch < 16; batch++) {
      count = furi_stream_buffer_receive(app->rx, bytes, sizeof(bytes), 0);
      if (!count)
        break;
      for (size_t i = 0; i < count; i++)
        wire_feed(&app->parser, bytes[i], app_receive, app);
    }
}
