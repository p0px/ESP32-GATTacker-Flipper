#include "app.h"
#include <stdlib.h>
#include <string.h>

int32_t gattacker_main(void *context) {
  UNUSED(context);
  App *app = calloc(1, sizeof(App));
  if (!app)
    return -1;
  app->running = true;
  app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
  app->input = furi_message_queue_alloc(8, sizeof(InputEvent));
  app->rx = furi_stream_buffer_alloc(16384, 1);
  app->gui = furi_record_open(RECORD_GUI);
  app->storage = furi_record_open(RECORD_STORAGE);
  app->capture = storage_file_alloc(app->storage);
  app->expansion = furi_record_open(RECORD_EXPANSION);
  expansion_disable(app->expansion);
  app_settings_load(app);
  app_serial_open(app);
  app->viewport = view_port_alloc();
  view_port_draw_callback_set(app->viewport, app_draw, app);
  view_port_input_callback_set(app->viewport, app_input, app);
  gui_add_view_port(app->gui, app->viewport, GuiLayerFullscreen);
  uint32_t last_draw = 0;
  while (app->running) {
    InputEvent input;
    bool pressed = furi_message_queue_get(app->input, &input, 20) == FuriStatusOk;
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app_receive_pending(app);
    if (pressed)
      app_handle(app, &input);
    app_refresh(app);
    // Drain the recorder while still servicing UART before honoring an exit request.
    if (!app->running && app->recording) {
      app->exit_after_capture = true;
      app->running = true;
      if (!app->capture_stopping) app_capture_toggle(app);
    }
    app_capture_refresh(app);
    app_session_refresh(app);
    app_pair_refresh(app);
    furi_mutex_release(app->mutex);
    /* The native browser owns GUI callbacks until it closes. Never enter it under
     * the app lock, or call viewport methods while that lock is held. */
    if (app->browse_requested) app_capture_browse(app);
    if (app->wifi_edit_requested) app_wifi_edit(app);
    uint32_t now = furi_get_tick();
    // Match the 10 Hz live preview and leave GUI/USB time for modal teardown.
    if (pressed || now - last_draw >= furi_ms_to_ticks(100)) {
      view_port_update(app->viewport);
      last_draw = now;
    }
  }
  if (app->recording)
    app_capture_finish(app, false);
  if (app->capture_exporting)
    app_export_abort(app);
  app_serial_close(app);
  expansion_enable(app->expansion);
  view_port_enabled_set(app->viewport, false);
  gui_remove_view_port(app->gui, app->viewport);
  view_port_free(app->viewport);
  storage_file_free(app->capture);
  furi_record_close(RECORD_EXPANSION);
  furi_record_close(RECORD_STORAGE);
  furi_record_close(RECORD_GUI);
  furi_stream_buffer_free(app->rx);
  furi_message_queue_free(app->input);
  furi_mutex_free(app->mutex);
  free(app);
  return 0;
}
