#include "ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void ui_traffic(Canvas *c, const App *app) {
  if (!app->event_count) {
    ui_empty(c, UiPacket, "No GATT yet",
             app->active_mac[0] ? "Waiting for activity" : "Start a session");
  } else {
    unsigned first = (app->selected / 2) * 2;
    for (unsigned row = 0; row < 2 && first + row < app->event_count; row++) {
      unsigned index = first + row, y = 14 + row * 20;
      const TrafficEvent *event = app_traffic_at(app, index);
      char line[64], uuid[24];
      ui_card(c, y, 20, index == app->selected);
      canvas_draw_frame(c, 3, y + 4, 12, 12);
      snprintf(line, sizeof(line), "%c", event->operation);
      canvas_draw_str(c, 6, y + 13, line);
      snprintf(line, sizeof(line), "%s  h:%s", ui_operation(event->operation), event->handle);
      ui_fit(c, 20, y + 8, 100, line);
      ui_uuid_short(event->uuid, uuid, sizeof(uuid));
      ui_fit(c, 20, y + 17, 100, uuid);
      canvas_set_color(c, ColorBlack);
    }
    ui_scroll(c, first, app->event_count, 2);
  }
  ui_footer(c, "Back", app->event_count ? "Open" : "", 0, 0);
  char activity[24];
  if (app->paused) snprintf(activity, sizeof(activity), "Live>");
  else if (app->event_count) {
    uint32_t age = (furi_get_tick() - app->event_at) / furi_ms_to_ticks(1000);
    if (age < 60) snprintf(activity, sizeof(activity), "%lus ago", (unsigned long)age);
    else snprintf(activity, sizeof(activity), "%lum ago", (unsigned long)(age / 60));
  } else snprintf(activity, sizeof(activity), "<=10/s");
  canvas_draw_str_aligned(c, 127, 63, AlignRight, AlignBottom, activity);
}

static bool is_hex(const char *text) {
  for (const char *p = text; *p; p++)
    if (!strchr("0123456789abcdefABCDEF", *p)) return false;
  return strlen(text) % 2 == 0;
}

void ui_event(Canvas *c, App *app) {
  const TrafficEvent *event = &app->inspected_event;
  char text[420];
  unsigned bytes = strlen(event->data) / 2;
  bool hex = is_hex(event->data);
  if (hex)
    app->page_count = 1 + (bytes ? (bytes + 23) / 24 : 1);
  else {
    snprintf(text, sizeof(text), "%s\n%s", event->data, event->detail);
    app->page_count = 1 + ui_text(c, text, UINT32_MAX);
  }
  if (!app->page) {
    snprintf(text, sizeof(text), "%s\n%s / h:%s\n%s", ui_direction(event->direction),
             ui_operation(event->operation), event->handle, event->uuid);
    ui_text(c, text, 0);
  } else if (hex) {
    if (!bytes)
      ui_empty(c, UiPacket, "No payload", "Read request");
    for (unsigned row = 0; row < 4; row++) {
      unsigned offset = (app->page - 1) * 24 + row * 6;
      if (offset >= bytes) break;
      snprintf(text, sizeof(text), "%03X", offset);
      canvas_draw_str(c, 2, 22 + row * 10, text);
      for (unsigned i = 0; i < 6 && offset + i < bytes; i++) {
        snprintf(text, sizeof(text), "%.2s", event->data + (offset + i) * 2);
        canvas_draw_str(c, 26 + i * 16, 22 + row * 10, text);
      }
    }
  } else {
    snprintf(text, sizeof(text), "%s\n%s", event->data, event->detail);
    ui_text(c, text, app->page - 1);
  }
  ui_footer(c, "Live", "", app->page, app->page_count);
}

static const char *standard_name(const char *uuid) {
  static const char *const ids[] = {"1800", "1801", "180a", "180f", "2a00", "2a01", "2a19"};
  static const char *const names[] = {
      "Generic access", "GATT", "Device info", "Battery", "Device name", "Appearance",
      "Battery level",
  };
  for (unsigned i = 0; i < sizeof(ids) / sizeof(ids[0]); i++)
    if (!strcmp(uuid, ids[i])) return names[i];
  return NULL;
}

void ui_services(Canvas *c, const App *app) {
  if (!app->service_count) {
    ui_empty(c, UiStack, app->services_id ? "Loading GATT" : "No services",
             app->services_id ? "Reading board" : "Start a session");
  } else {
    unsigned first = (app->selected / 2) * 2;
    for (unsigned row = 0; row < 2 && first + row < app->service_count; row++) {
      unsigned index = first + row, y = 14 + row * 20;
      const Service *service = &app->services[index];
      char text[40];
      const char *name = standard_name(service->uuid);
      ui_card(c, y, 20, index == app->selected);
      ui_icon(c, 3, y + 4, service->characteristic ? UiPacket : UiStack);
      ui_uuid_short(service->uuid, text, sizeof(text));
      ui_fit(c, 19, y + 8, 102, name ? name : text);
      snprintf(text, sizeof(text), "%04X  %s%s%s%s", service->handle,
               service->properties & 0x02 ? "R " : "", service->properties & 0x0c ? "W " : "",
               service->properties & 0x10 ? "N " : "", service->properties & 0x20 ? "I" : "");
      canvas_draw_str(c, 19, y + 17, service->characteristic ? text : "Service group");
      canvas_set_color(c, ColorBlack);
    }
    ui_scroll(c, first, app->service_count, 2);
  }
  ui_footer(c, app->target_connected ? "Back" : "Cached", app->service_count ? "Open" : "",
             app->selected, app->service_count);
}

static void property(Canvas *c, int x, int y, const char *label, bool enabled) {
  canvas_draw_frame(c, x, y - 7, 7, 7);
  if (enabled) {
    canvas_draw_line(c, x + 1, y - 4, x + 3, y - 2);
    canvas_draw_line(c, x + 3, y - 2, x + 6, y - 6);
  }
  canvas_draw_str(c, x + 11, y, label);
}

void ui_service(Canvas *c, App *app) {
  const Service *service = &app->services[app->selected];
  app->page_count = service->characteristic ? 2 : 1;
  if (!app->page) {
    char text[150];
    snprintf(text, sizeof(text), "%s\n%s\nHandle: 0x%04X",
             service->characteristic ? "Characteristic UUID" : "Service UUID",
             service->uuid, service->handle);
    ui_text(c, text, 0);
  } else {
    property(c, 3, 24, "Read", service->properties & 0x02);
    property(c, 65, 24, "Write", service->properties & 0x08);
    property(c, 3, 37, "Notify", service->properties & 0x10);
    property(c, 65, 37, "Indicate", service->properties & 0x20);
    property(c, 3, 50, "Write without response", service->properties & 0x04);
  }
  ui_footer(c, "List", "", app->page, app->page_count);
}

void ui_diagnostics(Canvas *c, App *app) {
  char text[200];
  app->page_count = 4;
  if (!app->page)
    snprintf(text, sizeof(text), "Board: %s\nTX %u / RX %u\n%u baud\nState: %s",
             app->online ? "Connected" : "Offline", app->port ? 15 : 13, app->port ? 16 : 14,
             115200U << app->baud_index, app_state_name(app->state));
  else if (app->page == 1)
    snprintf(text, sizeof(text), "Preview RX: %lu\nOmitted: %lu\nUART lost: %lu\nCRC: %lu",
             (unsigned long)app->event_total, (unsigned long)app->board_dropped,
             (unsigned long)app->rx_dropped,
             (unsigned long)app->parser.errors);
  else if (app->page == 2)
    snprintf(text, sizeof(text), "Internal RAM: %lu B\nLargest block: %lu B\nUART peak: %lu us",
             (unsigned long)app->internal_ram, (unsigned long)app->largest_block,
             (unsigned long)app->telemetry_us);
  else if (app->capture_health)
    snprintf(text, sizeof(text), "Capture ESP full: %lu\nInvalid HCI: %lu\nQueue peak: %lu/%lu",
             (unsigned long)app->capture_full, (unsigned long)app->capture_invalid,
             (unsigned long)app->capture_peak, (unsigned long)app->capture_capacity);
  else
    snprintf(text, sizeof(text), "Capture diagnostics\nStart PCAP to measure\nESP queue and loss");
  ui_text(c, text, 0);
  ui_footer(c, "Setup", "", app->page, app->page_count);
}

void ui_capture(Canvas *c, App *app) {
  if (app->capture_pcap) {
    app->page_count = ui_text(c, app->capture_text, app->page);
    ui_footer(c, "Back", "", app->page, app->page_count);
    return;
  }
  const char *p = app->capture_text, *selected = p;
  unsigned count = 0;
  while (*p) {
    if (count == app->page) selected = p;
    const char *end = strchr(p, '\n');
    p = end ? end + 1 : p + strlen(p);
    count++;
  }
  app->page_count = count ? count : 1;
  if (!count) {
    ui_empty(c, UiFile, "Empty capture", "No saved events");
  } else {
    char *line = app->capture_line;
    char text[200], uuid[24];
    size_t length = strcspn(selected, "\n");
    if (length >= sizeof(app->capture_line)) length = sizeof(app->capture_line) - 1;
    memcpy(line, selected, length);
    line[length] = 0;
    char *field[12];
    size_t fields = wire_fields(line, field, 12);
    unsigned long tick = fields ? strtoul(field[0], NULL, 10) : 0;
    if (fields == 9 && !strcmp(field[1], "status")) {
      snprintf(text, sizeof(text), "%lu.%lus / %s\nTarget: %s\nCentral: %s\nOmitted: %s",
               tick / 1000, tick / 100 % 10, app_state_name((unsigned)atoi(field[2])),
               atoi(field[3]) ? "on" : "off", atoi(field[4]) ? "on" : "off", field[7]);
      ui_text(c, text, 0);
    } else if (fields == 8 && !strcmp(field[1], "traffic")) {
      ui_uuid_short(field[4], uuid, sizeof(uuid));
      snprintf(text, sizeof(text), "%lu.%lus %s / h:%s\n%s\n%s\n%.24s", tick / 1000,
               tick / 100 % 10, ui_operation(field[2][0]), field[5],
               ui_direction(field[3]), uuid, field[6][0] ? field[6] : "No payload");
      ui_text(c, text, 0);
    } else {
      // Retain a readable fallback for older captures and partially written final records.
      snprintf(line, sizeof(app->capture_line), "%.*s", (int)length, selected);
      ui_text(c, line, 0);
    }
  }
  ui_footer(c, "Back", "", app->page, app->page_count);
  canvas_draw_str(c, 45, 63, "Preview");
}
