#include "../app.h"
#include <stdio.h>
#include <string.h>

static uint32_t checksum(uint32_t value, const uint8_t *data, size_t length) {
  for (size_t i = 0; i < length; i++) value = (value ^ data[i]) * 16777619U;
  return value;
}

static void acknowledge(App *app, uint32_t offset) {
  char command[80];
  snprintf(command, sizeof(command), "capture_export_ack\t%lu\t%lu",
           (unsigned long)app->capture_token, (unsigned long)offset);
  app_command(app, command);
}

void app_export_abort(App *app) {
  if (!app->capture_exporting) return;
  storage_file_close(app->capture);
  storage_simply_remove(app->storage, app->capture_path);
  app->capture_exporting = false;
  app->capture_buffered = false;
  app->capture_complete = false;
  app->capture_path[0] = 0;
  app_notice(app, "SD copy interrupted; partial file removed.");
}

static void begin(App *app, uint32_t id, uint32_t size, uint32_t packets,
                  uint32_t dropped) {
  if (app->capture_exporting && id == app->capture_token &&
      size == app->capture_export_size) {
    acknowledge(app, 0);
    return;
  }
  if (app->capture_buffered && id == app->capture_token) {
    app->recording = false;
    app->capture_stopping = false;
  }
  if (app->capture_exporting || app->recording || !size || size > 4 * 1024 * 1024 ||
      storage_sd_status(app->storage) != FSE_OK) return;
  storage_simply_mkdir(app->storage, CAPTURE_DIR);
  unsigned index = 0;
  do {
    snprintf(app->capture_path, sizeof(app->capture_path), CAPTURE_DIR "/capture-%04u.pcapng",
             index++);
  } while (storage_file_exists(app->storage, app->capture_path) && index < 10000);
  if (!storage_file_open(app->capture, app->capture_path, FSAM_WRITE, FSOM_CREATE_NEW)) return;
  app->capture_token = id;
  app->capture_export_size = size;
  app->capture_export_hash = 2166136261U;
  app->capture_export_at = furi_get_tick();
  app->capture_bytes = 0;
  app->capture_packets = packets;
  app->capture_dropped = dropped;
  app->capture_sequence = packets;
  app->capture_complete = false;
  app->capture_exporting = true;
  acknowledge(app, 0);
}

static void data(App *app, uint32_t id, uint32_t offset, const char *encoded) {
  if (!app->capture_exporting || id != app->capture_token) return;
  if (offset < app->capture_bytes) {
    acknowledge(app, app->capture_bytes);
    return;
  }
  if (offset != app->capture_bytes) return;
  size_t length = capture_unbase64(encoded, app->capture_buffer,
                                   sizeof(app->capture_buffer));
  if (!length || length > app->capture_export_size - app->capture_bytes) {
    app_export_abort(app);
    return;
  }
  if (storage_file_write(app->capture, app->capture_buffer, length) != length) {
    app_export_abort(app);
    return;
  }
  app->capture_export_hash = checksum(app->capture_export_hash, app->capture_buffer, length);
  app->capture_bytes += length;
  app->capture_export_at = furi_get_tick();
  acknowledge(app, app->capture_bytes);
}

static void end(App *app, uint32_t id, uint32_t size, uint32_t hash) {
  if (!app->capture_exporting) {
    if (app->capture_complete && id == app->capture_token && size == app->capture_bytes)
      acknowledge(app, size + 1);
    return;
  }
  if (id != app->capture_token || size != app->capture_bytes ||
      hash != app->capture_export_hash) {
    app_export_abort(app);
    return;
  }
  if (!storage_file_close(app->capture)) {
    app_export_abort(app);
    return;
  }
  app->capture_exporting = false;
  app->capture_buffered = false;
  app->capture_complete = true;
  acknowledge(app, size + 1);
  snprintf(app->message, sizeof(app->message), "PCAPNG copied to SD: %lu bytes",
           (unsigned long)size);
  app_capture_report(app);
}

void app_export_receive(App *app, WireKind kind, char **fields, size_t count) {
  if (kind != WireEvent || count < 2 || strncmp(fields[0], "capture_export_", 15)) return;
  uint32_t id;
  if (!wire_number(fields[1], UINT32_MAX, &id) || !id) return;
  if (!strcmp(fields[0], "capture_export_abort") && count == 2) {
    if (app->capture_exporting && id == app->capture_token) app_export_abort(app);
    return;
  }
  uint32_t value;
  if (count < 3 || !wire_number(fields[2], UINT32_MAX, &value)) return;
  if (!strcmp(fields[0], "capture_export_begin") && count == 5) {
    uint32_t packets, dropped;
    if (wire_number(fields[3], UINT32_MAX, &packets) &&
        wire_number(fields[4], UINT32_MAX, &dropped)) begin(app, id, value, packets, dropped);
  }
  else if (!strcmp(fields[0], "capture_export_data") && count == 4)
    data(app, id, value, fields[3]);
  else if (!strcmp(fields[0], "capture_export_end") && count == 4) {
    uint32_t hash;
    if (wire_number(fields[3], UINT32_MAX, &hash)) end(app, id, value, hash);
  }
}
