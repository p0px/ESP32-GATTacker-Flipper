#include "../app.h"
#include "gattacker_icons.h"
#include <stdio.h>
#include <string.h>

void app_capture_browse(App *app) {
  /* Called by the app loop outside its model lock, never from a GUI callback. */
  furi_mutex_acquire(app->mutex, FuriWaitForever);
  app->browse_requested = false;
  if (app->recording || (app->online && app->state >= 3 && app->state <= 7)) {
    app_notice(app, "Stop the session and recording before browsing saved captures.");
    furi_mutex_release(app->mutex);
    return;
  }
  furi_mutex_release(app->mutex);
  // Hand off the viewport before opening the modal. Keep GUI calls outside the app lock.
  view_port_enabled_set(app->viewport, false);
  DialogsApp *dialogs = furi_record_open(RECORD_DIALOGS);
  FuriString *path = furi_string_alloc_set(app->capture_path[0] ? app->capture_path : CAPTURE_DIR);
  DialogsFileBrowserOptions options;
  dialog_file_browser_set_basic_options(&options, ".pcapng|.tsv", &I_capture_10x10);
  options.base_path = CAPTURE_DIR;
  options.hide_ext = false;
  bool chosen = dialog_file_browser_show(dialogs, path, path, &options);
  // The app viewport stays disabled through file I/O and dialog teardown.
  furi_mutex_acquire(app->mutex, FuriWaitForever);
  if (chosen) {
    File *file = storage_file_alloc(app->storage);
    if (storage_file_open(file, furi_string_get_cstr(path), FSAM_READ, FSOM_OPEN_EXISTING)) {
      const char *extension = strrchr(furi_string_get_cstr(path), '.');
      app->capture_pcap = extension && !strcmp(extension, ".pcapng");
      if (app->capture_pcap) {
        snprintf(app->capture_text, sizeof(app->capture_text),
                 "PCAPNG / Bluetooth HCI\n%lu bytes\nOpen in Wireshark\nFilter: btatt\n"
                 "Copy this file from apps_data/gattacker on the SD card.\n"
                 "Packet loss details: Wireshark > Capture File Properties.",
                 (unsigned long)storage_file_size(file));
      } else {
        size_t count = storage_file_read(file, app->capture_text, sizeof(app->capture_text) - 1);
        app->capture_text[count] = 0;
      }
      storage_file_close(file);
      snprintf(app->capture_path, sizeof(app->capture_path), "%s", furi_string_get_cstr(path));
      app_open(app, ScreenCapture);
      snprintf(app->message, sizeof(app->message), "%s",
               app->capture_pcap ? "Open the saved PCAPNG in Wireshark" : "Preview: first 8 KB");
    } else
      app_notice(app, "Cannot read this capture. Check that the SD card is available.");
    storage_file_free(file);
  }
  furi_string_free(path);
  furi_record_close(RECORD_DIALOGS);
  // Do not replay a queued opening/Back button into the restored app screen.
  furi_message_queue_reset(app->input);
  app->pending_id = 0;
  app->scan_id = app->results_id = app->services_id = 0;
  app->last_poll = furi_get_tick();
  app_command(app, "status");
  furi_mutex_release(app->mutex);
  view_port_enabled_set(app->viewport, true);
}

void app_settings_load(App *app) {
  File *file = storage_file_alloc(app->storage);
  uint8_t bytes[2];
  if (storage_file_open(file, CAPTURE_DIR "/uart.bin", FSAM_READ, FSOM_OPEN_EXISTING)) {
    if (storage_file_read(file, bytes, 2) == 2 && bytes[0] < 2 && bytes[1] < 3) {
      app->port = bytes[0];
      app->baud_index = bytes[1];
    }
    storage_file_close(file);
  }
  storage_file_free(file);
}

void app_settings_save(App *app) {
  storage_simply_mkdir(app->storage, CAPTURE_DIR);
  File *file = storage_file_alloc(app->storage);
  uint8_t bytes[] = {(uint8_t)app->port, (uint8_t)app->baud_index};
  if (storage_file_open(file, CAPTURE_DIR "/uart.bin", FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
    storage_file_write(file, bytes, sizeof(bytes));
    storage_file_close(file);
  }
  storage_file_free(file);
}
