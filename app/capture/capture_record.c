#include "../app.h"
#include <stdio.h>
#include <string.h>

static void stop_board(App *app) {
  char command[48];
  snprintf(command, sizeof(command), "%s\t%lu",
           app->capture_buffered ? "capture_ram_stop" : "capture_stop",
           (unsigned long)app->capture_token);
  app->capture_command = app_command(app, command);
}

static bool write_block(App *app, const uint8_t *data, size_t size) {
  uint32_t position = app->capture_bytes;
  if (size && storage_file_write(app->capture, data, size) == size) {
    app->capture_bytes += size;
    return true;
  }
  // Recover the last complete block if an SD write is short. Never append after corruption.
  bool recovered = storage_file_seek(app->capture, position, true) &&
      storage_file_truncate(app->capture);
  storage_file_close(app->capture);
  app->recording = false;
  app->capture_complete = false;
  stop_board(app);
  app_notice(app, recovered ? "SD write failed. Saved complete packets; recording stopped."
                           : "SD write failed. Capture may be damaged; recording stopped.");
  return false;
}

static bool flush_batch(App *app) {
  if (!app->capture_batch_size) return true;
  size_t size = app->capture_batch_size;
  uint32_t packets = app->capture_batch_packets;
  app->capture_batch_size = 0;
  app->capture_batch_packets = 0;
  if (write_block(app, app->capture_batch, size)) return true;
  app->capture_packets -= packets;
  return false;
}

void app_capture_finish(App *app, bool complete) {
  if (!app->recording) return;
  if (app->capture_buffered) {
    if (!app->capture_stopping && app->capture_token) stop_board(app);
    app->recording = app->capture_buffered = app->capture_stopping = false;
    app_notice(app, "RAM recording status lost. Check the web UI before restarting.");
    return;
  }
  if (!complete && !app->capture_stopping) stop_board(app);
  if (!flush_batch(app)) return;
  uint32_t losses = app->capture_dropped + app->capture_sequence - app->capture_packets;
  char detail[280], health[100];
  bool known = app->capture_health && app->capture_health_accepted == app->capture_sequence &&
      app->capture_full + app->capture_invalid == app->capture_dropped;
  if (known)
    snprintf(health, sizeof(health), "ESP full=%lu invalid=%lu queue peak=%lu/%lu.",
             (unsigned long)app->capture_full, (unsigned long)app->capture_invalid,
             (unsigned long)app->capture_peak, (unsigned long)app->capture_capacity);
  else
    snprintf(health, sizeof(health), "ESP cause breakdown unavailable.");
  snprintf(detail, sizeof(detail), "ESP dropped=%lu; UART/Flipper missing=%lu; rejected=%lu. %s",
           (unsigned long)app->capture_dropped,
           (unsigned long)(app->capture_sequence - app->capture_packets),
           (unsigned long)app->capture_rejected, health);
  size_t size = pcap_statistics(app->capture_buffer, app->capture_timestamp,
                                app->capture_packets, losses, complete, detail);
  if (!write_block(app, app->capture_buffer, size)) return;
  bool closed = storage_file_close(app->capture);
  app->recording = false;
  app->capture_stopping = false;
  app->capture_complete = complete && closed;
  app->capture_command = 0;
  if (!closed)
    app_notice(app, "Could not close the SD file cleanly. Check the capture in Wireshark.");
  else if (!complete)
    app_notice(app, "Recording interrupted. Saved packets are usable; final loss count unknown.");
  else
    snprintf(app->message, sizeof(app->message), "PCAPNG saved: %lu packets, %lu dropped",
             (unsigned long)app->capture_packets, (unsigned long)losses);
}

static void buffered_start(App *app) {
  if (!app->online || !app->capture_capable) {
    app_notice(app, "Connect a board with PSRAM capture support first.");
    return;
  }
  if (storage_sd_status(app->storage) != FSE_OK) {
    app_notice(app, "Flipper SD card unavailable.");
    return;
  }
  app->capture_buffered = app->recording = true;
  app->capture_ready = app->capture_stopping = app->capture_complete = false;
  app->capture_token = app->capture_packets = app->capture_dropped = app->capture_bytes = 0;
  app->capture_path[0] = 0;
  app->capture_at = furi_get_tick();
  char command[48];
  snprintf(command, sizeof(command), "capture_ram_start\t%lu",
           (unsigned long)furi_hal_rtc_get_timestamp());
  app->capture_command = app_command(app, command);
  if (!app->capture_command) {
    app->recording = app->capture_buffered = false;
    return;
  }
  app_open(app, ScreenRecording);
}

void app_capture_toggle(App *app) {
  if (app->capture_exporting) {
    app_notice(app, "Wait for the RAM capture to finish copying to SD.");
    return;
  }
  if (app->recording) {
    if (app->capture_buffered && !app->capture_token) {
      app_notice(app, "Waiting for the board to start RAM recording.");
      return;
    }
    if (!app->capture_stopping) {
      app->capture_stopping = true;
      app->capture_at = furi_get_tick();
      stop_board(app);
    }
    return;
  }
  if (app->capture_ram_capable && !app->capture_requested_token) {
    buffered_start(app);
    return;
  }
  if (!app->online || !app->capture_capable) {
    app_notice(app, "Connect a board with HCI capture firmware before recording PCAPNG.");
    return;
  }
  storage_simply_mkdir(app->storage, CAPTURE_DIR);
  unsigned index = 0;
  do {
    snprintf(app->capture_path, sizeof(app->capture_path), CAPTURE_DIR "/capture-%04u.pcapng",
             index++);
  } while (storage_file_exists(app->storage, app->capture_path) && index < 10000);
  if (!storage_file_open(app->capture, app->capture_path, FSAM_WRITE, FSOM_CREATE_NEW)) {
    app_notice(app, "Cannot create a capture on the SD card.");
    return;
  }
  app->recording = true;
  app->capture_ready = app->capture_stopping = app->capture_complete = false;
  app->capture_packets = app->capture_sequence = app->capture_dropped = app->capture_bytes = 0;
  app->capture_batch_size = app->capture_batch_packets = 0;
  app->capture_batch_at = furi_get_tick();
  app->capture_health = false;
  app->capture_rejected = 0;
  app->capture_at = furi_get_tick();
  uint32_t seconds = furi_hal_rtc_get_timestamp();
  app->capture_timestamp = (uint64_t)seconds * 1000000;
  app->capture_token = app->capture_requested_token ? app->capture_requested_token :
      (seconds ^ app->capture_at) | 1;
  if (!write_block(app, app->capture_buffer, pcap_header(app->capture_buffer))) {
    storage_simply_remove(app->storage, app->capture_path);
    app_notice(app, "SD write failed before recording started. No usable capture was created.");
    return;
  }
  char command[80];
  snprintf(command, sizeof(command), "capture_start\t%lu\t%lu",
           (unsigned long)app->capture_token, (unsigned long)seconds);
  app->capture_command = app_command(app, command);
  if (!app->capture_command) {
    app_capture_finish(app, false);
    return;
  }
  app_open(app, ScreenRecording);
}

static void packet_receive(App *app, char **f, size_t count) {
  uint32_t sequence, direction;
  uint64_t timestamp;
  if (count != 6 || !wire_number(f[2], UINT32_MAX, &sequence) ||
      sequence <= app->capture_sequence || !capture_u64(f[3], &timestamp) ||
      timestamp < app->capture_timestamp || !wire_number(f[4], 1, &direction)) {
    app->capture_rejected++;
    return;
  }
  app->capture_sequence = sequence;
  size_t length = capture_unbase64(f[5], app->capture_h4, sizeof(app->capture_h4));
  size_t size = pcap_packet(app->capture_buffer, timestamp, direction != 0,
                            app->capture_h4, length);
  if (!size) {
    app->capture_rejected++;
    return;
  }
  if (app->capture_batch_size + size > sizeof(app->capture_batch) && !flush_batch(app))
    return;
  if (!app->capture_batch_size) app->capture_batch_at = furi_get_tick();
  memcpy(app->capture_batch + app->capture_batch_size, app->capture_buffer, size);
  app->capture_batch_size += size;
  app->capture_batch_packets++;
  app->capture_packets++;
  app->capture_timestamp = timestamp;
  app->capture_ready = true;
}

static bool buffered_receive(App *app, WireKind kind, uint16_t id,
                             char **f, size_t count) {
  if (!strcmp(f[0], "capture_ram_support") && count == 2) {
    app->capture_ram_capable = !strcmp(f[1], "1");
    return true;
  }
  if (kind == WireReply && app->capture_buffered && id == app->capture_command) {
    if (!strcmp(f[0], "error")) {
      app->recording = app->capture_buffered = app->capture_stopping = false;
      app_notice(app, count > 1 ? f[1] : "RAM capture command failed.");
    } else if (!app->capture_token && !strcmp(f[0], "ok") && count == 2) {
      uint32_t token;
      if (wire_number(f[1], UINT32_MAX, &token) && token) {
        app->capture_token = token;
        app->capture_ready = true;
      }
    }
    return true;
  }
  if (kind != WireEvent || strcmp(f[0], "capture_ram") || count != 11) return false;
  uint32_t token, packets, dropped, bytes, capacity, auto_export, failed;
  if (!wire_number(f[1], UINT32_MAX, &token) ||
      !wire_number(f[3], UINT32_MAX, &packets) ||
      !wire_number(f[4], UINT32_MAX, &dropped) ||
      !wire_number(f[5], UINT32_MAX, &bytes) ||
      !wire_number(f[6], UINT32_MAX, &capacity) ||
      !wire_number(f[7], 1, &auto_export) || !wire_number(f[10], 1, &failed))
    return true;
  bool active = !strcmp(f[2], "recording") || !strcmp(f[2], "saving");
  if (active && auto_export && token && (!app->recording || app->capture_buffered)) {
    if (!app->capture_buffered || app->capture_token != token) {
      app->capture_path[0] = 0;
      app->capture_complete = false;
      app->capture_at = furi_get_tick();
    }
    if (!app->capture_stopping && !strcmp(f[2], "saving"))
      app->capture_at = furi_get_tick();
    app->recording = app->capture_buffered = app->capture_ready = true;
    app->capture_token = token;
    app->capture_stopping = !strcmp(f[2], "saving");
  }
  if (!app->capture_buffered || app->capture_token != token) return true;
  app->capture_packets = packets;
  app->capture_dropped = dropped;
  if (!app->capture_exporting) app->capture_bytes = bytes;
  app->capture_capacity = capacity;
  if (!strcmp(f[2], "ready") && !app->capture_exporting) {
    app->capture_stopping = true;
    if (failed || !strcmp(f[8], "failed")) {
      app->recording = app->capture_buffered = false;
      app_notice(app, "SD copy failed. RAM capture remains in the web UI.");
    }
  } else if (!strcmp(f[2], "empty") && !app->capture_exporting) {
    app->recording = app->capture_buffered = false;
  }
  return true;
}

void app_capture_receive(App *app, WireKind kind, uint16_t id, char **f, size_t count) {
  if (!strcmp(f[0], "capture_sink_support") && count == 2) {
    app->capture_remote_capable = !strcmp(f[1], "1");
    return;
  }
  if (!strcmp(f[0], "capabilities") && count == 2) {
    app->capture_capable = !strcmp(f[1], "hci_pcapng");
    return;
  }
  if (buffered_receive(app, kind, id, f, count)) return;
  if (!app->recording) return;
  if (kind == WireReply && id == app->capture_command && !strcmp(f[0], "error")) {
    app_capture_finish(app, false);
    app_notice(app, count > 1 ? f[1] : "Capture command failed.");
    return;
  }
  if (!strcmp(f[0], "capture_idle") && app->capture_ready) {
    app_capture_finish(app, false);
    return;
  }
  uint32_t token;
  if (count < 2 || !wire_number(f[1], UINT32_MAX, &token) || token != app->capture_token)
    return;
  if (!strcmp(f[0], "capture_begin") && count == 2)
    app->capture_ready = true;
  else if (!strcmp(f[0], "hci"))
    packet_receive(app, f, count);
  else if (!strcmp(f[0], "capture_health") && count == 7) {
    uint32_t values[5];
    for (unsigned i = 0; i < 5; i++)
      if (!wire_number(f[i + 2], UINT32_MAX, &values[i])) return;
    if (!values[4] || values[3] > values[4] ||
        (uint64_t)values[1] + values[2] > UINT32_MAX) return;
    app->capture_health = true;
    app->capture_health_accepted = values[0];
    app->capture_full = values[1];
    app->capture_invalid = values[2];
    app->capture_peak = values[3];
    app->capture_capacity = values[4];
  }
  else if ((!strcmp(f[0], "capture_stats") || !strcmp(f[0], "capture_end")) && count == 4) {
    uint32_t accepted, dropped;
    if (!wire_number(f[2], UINT32_MAX, &accepted) ||
        !wire_number(f[3], UINT32_MAX, &dropped) || accepted < app->capture_sequence) return;
    app->capture_ready = true;
    app->capture_dropped = dropped;
    if (!strcmp(f[0], "capture_end")) {
      bool requested = app->capture_stopping;
      app->capture_sequence = accepted;
      app_capture_finish(app, true);
      if (!requested)
        app_notice(app, "Board ended recording. Queued packets were saved; start a new capture.");
    }
  }
}

void app_capture_refresh(App *app) {
  uint32_t now = furi_get_tick(), elapsed = now - app->capture_at;
  if (app->capture_exporting &&
      now - app->capture_export_at > furi_ms_to_ticks(15000)) app_export_abort(app);
  if (app->capture_buffered) {
    if (app->recording && !app->online && elapsed > furi_ms_to_ticks(10000))
      app_capture_finish(app, false);
    else if (app->recording && !app->capture_ready && elapsed > furi_ms_to_ticks(10000))
      app_capture_finish(app, false);
    else if (app->recording && app->capture_stopping &&
             elapsed > furi_ms_to_ticks(30000))
      app_capture_finish(app, false);
    if (app->exit_after_capture && !app->recording && !app->capture_exporting)
      app->running = false;
    return;
  }
  if (app->recording && app->capture_batch_size &&
      now - app->capture_batch_at >= furi_ms_to_ticks(250)) {
    flush_batch(app);
    app->capture_batch_at = now;
  }
  if (app->recording && (!app->online ||
      (!app->capture_ready && elapsed > furi_ms_to_ticks(5000)) ||
      (app->capture_stopping && elapsed > furi_ms_to_ticks(15000))))
    app_capture_finish(app, false);
  if (app->recording && !app->capture_stopping && app->capture_ready &&
      now - app->capture_heartbeat > furi_ms_to_ticks(1000)) {
    char command[48];
    snprintf(command, sizeof(command), "capture_keepalive\t%lu",
             (unsigned long)app->capture_token);
    app_command(app, command);
    app->capture_heartbeat = now;
  }
  if (app->exit_after_capture && !app->recording && !app->capture_exporting)
    app->running = false;
}
