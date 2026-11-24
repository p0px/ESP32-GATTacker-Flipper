/* Native behavioral checks run the production input/model code with hardware I/O disabled. */
#include "app.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint32_t tick = 1000;
static char sent[80];
void app_capture_remote(App *app, WireKind kind, char **fields, size_t count) {
  (void)app; (void)kind; (void)fields; (void)count;
}
void app_export_receive(App *app, WireKind kind, char **fields, size_t count) {
  (void)app; (void)kind; (void)fields; (void)count;
}
uint32_t furi_get_tick(void) { return tick; }
uint32_t furi_ms_to_ticks(uint32_t milliseconds) { return milliseconds; }
uint16_t app_command(App *app, const char *text) {
  snprintf(sent, sizeof(sent), "%s", text);
  return ++app->next_id;
}
void app_capture_receive(App *app, WireKind kind, uint16_t id, char **f, size_t count) {
  (void)app; (void)kind; (void)id; (void)f; (void)count;
}
void app_capture_toggle(App *app) {
  if (app->recording) app->capture_stopping = true;
}
void app_serial_close(App *app) { (void)app; }
bool app_serial_open(App *app) { (void)app; return true; }
void app_settings_save(App *app) { (void)app; }

static void press(App *app, InputKey key) {
  InputEvent input = {.type = InputTypeShort, .key = key};
  app_handle(app, &input);
}

static void test_detail_navigation(void) {
  App app = {.screen = ScreenTargetInfo, .page_count = 2, .device_count = 13};
  snprintf(app.devices[12].mac, sizeof(app.devices[12].mac), "02:00:00:00:00:12");
  app.selected_device = app.devices[12];
  press(&app, InputKeyDown);
  press(&app, InputKeyDown);
  assert(app.page == 1);
  press(&app, InputKeyUp);
  press(&app, InputKeyUp);
  assert(app.page == 0);
  press(&app, InputKeyBack);
  assert(app.screen == ScreenTarget && app.selected == 3);
  press(&app, InputKeyBack);
  assert(app.screen == ScreenDevices && app.selected == 12);
}

static void test_home_preserves_action_when_session_appears(void) {
  App app = {.screen = ScreenHome};
  assert(app_rows(&app) == 5);
  press(&app, InputKeyUp);
  press(&app, InputKeyOk);
  assert(app.screen == ScreenAbout);
  press(&app, InputKeyBack);
  press(&app, InputKeyUp);
  app_receive(&app, WireEvent, 0, "status\t7\t1\t0\t0\t0\t0\thits:0");
  assert(app_rows(&app) == 6);
  press(&app, InputKeyOk);
  assert(app.screen == ScreenSettings);
  press(&app, InputKeyBack);
  app_receive(&app, WireEvent, 0, "target\t02:00:00:00:00:12");
  app_receive(&app, WireEvent, 0, "target\t");
  app_receive(&app, WireEvent, 0, "status\t0\t0\t0\t0\t0\t0\thits:0");
  assert(!app_session_active(&app) && app_rows(&app) == 6);
  press(&app, InputKeyOk);
  assert(app.screen == ScreenSettings);
  app_home_select(&app, HomeSession);
  press(&app, InputKeyOk);
  assert(app.screen == ScreenSession && app_rows(&app) == 5);
  press(&app, InputKeyUp);
  sent[0] = 0;
  press(&app, InputKeyOk);
  assert(app.screen == ScreenTarget && !sent[0]);
  assert(!strcmp(app.selected_device.mac, "02:00:00:00:00:12"));
  press(&app, InputKeyBack);
  assert(app.screen == ScreenSession && app.selected == 4);
}

static void test_notice_does_not_activate_underlying_button(void) {
  App app = {.screen = ScreenTarget, .online = true};
  app_notice(&app, "Board busy");
  press(&app, InputKeyOk);
  assert(!app.notice && app.screen == ScreenTarget && !app.active_mac[0]);
}

static void test_successful_reply_preserves_notice(void) {
  App app = {.screen = ScreenDevices, .online = true, .pending_id = 17, .notice = true};
  snprintf(app.message, sizeof(app.message), "No reply from board");
  app_receive(&app, WireReply, 17, "ok\taccepted");
  assert(!app.pending_id && app.notice);
  assert(!strcmp(app.message, "No reply from board"));
  press(&app, InputKeyOk);
  assert(!app.notice && app.screen == ScreenDevices);
}

static void test_hooks_are_read_only_from_every_entry(void) {
  const Screen screens[] = {ScreenHome, ScreenTarget, ScreenSession};
  const unsigned selections[] = {1, 2, 3};
  for (unsigned i = 0; i < 3; i++) {
    App app = {.screen = screens[i], .selected = selections[i], .browser_hooks = true};
    sent[0] = 0;
    press(&app, InputKeyOk);
    assert(app.screen == ScreenHooks);
    press(&app, InputKeyDown);
    press(&app, InputKeyRight);
    press(&app, InputKeyOk);
    assert(app.screen == ScreenHooks && app.browser_hooks && !sent[0]);
    press(&app, InputKeyBack);
    assert(app.screen == screens[i] && app.selected == selections[i]);
  }
}

static void test_scan_results_sort_after_indexed_stream(void) {
  App app = {.screen = ScreenDevices, .results_id = 40};
  app_receive(&app, WireEvent, 0, "device\t1\t22:22:22:22:22:22\t-40\tStrong");
  app_receive(&app, WireEvent, 0, "device\t0\t11:11:11:11:11:11\t-90\tWeak");
  press(&app, InputKeyDown);
  press(&app, InputKeyOk);
  assert(app.screen == ScreenDevices && app.selected == 0 && !app.selected_device.mac[0]);
  app_receive(&app, WireEvent, 0, "device\t2\t33:33:33:33:33:33\t-65\tMiddle");
  app_receive(&app, WireEvent, 0, "device\t3\t44:44:44:44:44:44\t-40\tEqual");
  app_receive(&app, WireReply, 40, "ok\tdone");
  assert(app.results_loaded && app.device_count == 4 && !app.results_id);
  assert(!strcmp(app.devices[0].name, "Strong") && app.devices[0].rssi == -40);
  assert(!strcmp(app.devices[1].name, "Equal") && app.devices[1].rssi == -40);
  assert(!strcmp(app.devices[2].name, "Middle") && app.devices[2].rssi == -65);
  assert(!strcmp(app.devices[3].name, "Weak") && app.devices[3].rssi == -90);
  press(&app, InputKeyOk);
  assert(!strcmp(app.selected_device.mac, "22:22:22:22:22:22"));
  app_home_select(&app, HomeScan);
  press(&app, InputKeyOk);
  assert(app.screen == ScreenDevices && !app.device_count && !app.results_loaded);
}

static void test_traffic_collects_during_inspection_and_returns_to_latest(void) {
  App app = {.screen = ScreenTraffic, .recording = true};
  app_receive(&app, WireEvent, 0, "traffic\tW\tvictim -> target\t2902\t16\t0100\tWRITE");
  app_receive(&app, WireEvent, 0, "traffic\tN\ttarget -> victim\tfeed\t15\t1234\tNOTIFY");
  assert(app.event_count == 2 && app.selected == 1);
  assert(!strcmp(app_traffic_at(&app, 1)->data, "1234"));
  press(&app, InputKeyOk);
  assert(app.paused && app.screen == ScreenEvent);
  // Reproduce a long capture while details stay open, exceeding the retained window.
  for (unsigned i = 0; i < EVENT_COUNT * 3; i++) {
    char record[100];
    snprintf(record, sizeof(record), "traffic\tN\ttarget -> victim\tfeed\t15\t%04X\tNOTIFY", i);
    tick += 100;
    app_receive(&app, WireEvent, 0, record);
  }
  assert(app.event_count == EVENT_COUNT && app.event_total == EVENT_COUNT * 3 + 2);
  assert(!strcmp(app.inspected_event.data, "1234") && app.event_at == tick);
  assert(!strcmp(app_traffic_at(&app, 0)->data, "0030"));
  assert(!strcmp(app_traffic_at(&app, EVENT_COUNT - 1)->data, "0047"));
  assert(app.recording && !app.capture_stopping);
  sent[0] = 0;
  press(&app, InputKeyOk);
  assert(app.screen == ScreenEvent && !sent[0]);
  press(&app, InputKeyBack);
  assert(app.screen == ScreenTraffic && !app.paused && app.selected == EVENT_COUNT - 1);
  app_receive(&app, WireEvent, 0, "traffic\tW\tvictim -> target\t2902\t16\t0100\tWRITE");
  assert(app_traffic_at(&app, app.selected)->operation == 'W');
}

static void test_browsing_does_not_stop_receiving(void) {
  App app = {.screen = ScreenTraffic};
  for (unsigned i = 0; i < EVENT_COUNT; i++) {
    char record[100];
    snprintf(record, sizeof(record), "traffic\tW\tvictim -> target\tfeed\t13\t%04X\tWRITE", i);
    app_receive(&app, WireEvent, 0, record);
  }
  press(&app, InputKeyUp);
  assert(app.paused && !strcmp(app_traffic_at(&app, app.selected)->data, "0016"));
  app_receive(&app, WireEvent, 0, "traffic\tN\ttarget -> victim\tfeed\t15\t1234\tNOTIFY");
  assert(!strcmp(app_traffic_at(&app, app.selected)->data, "0016"));
  press(&app, InputKeyRight);
  assert(!app.paused && !strcmp(app_traffic_at(&app, app.selected)->data, "1234"));
  press(&app, InputKeyRight);
  assert(!app.paused);
  press(&app, InputKeyLeft);
  app_receive(&app, WireEvent, 0, "traffic\tW\tvictim -> target\tfeed\t13\tabcd\tWRITE");
  assert(app.paused && app.event_total == EVENT_COUNT + 2);
  app_receive(&app, WireEvent, 0, "traffic\tinvalid");
  assert(app.event_total == EVENT_COUNT + 2);
  press(&app, InputKeyBack);
  assert(app.screen == ScreenSession && app.selected == 1);
  press(&app, InputKeyOk);
  assert(!app.paused && !strcmp(app_traffic_at(&app, app.selected)->data, "abcd"));
  app_traffic_reset(&app);
  app_receive(&app, WireEvent, 0, "traffic\tR\ttarget -> victim\t2a00\t3\t01\tREAD");
  assert(app.event_count == 1 && app.event_total == 1 && app.selected == 0);
  assert(!strcmp(app_traffic_at(&app, 0)->uuid, "2a00"));
}

static void test_diagnostics_compatibility(void) {
  App app = {0};
  app_receive(&app, WireEvent, 0,
              "status\t0\t0\t1\t0\t1\t2\thits:0 ram:55128 largest:30720 uart_us:83");
  assert(app.phone_connected && app.board_dropped == 2);
  assert(app.internal_ram == 55128 && app.largest_block == 30720 && app.telemetry_us == 83);
  app_receive(&app, WireEvent, 0, "status\t0\t0\t0\t0\t1\t0\thits:0");
  assert(!app.phone_connected && app.internal_ram == 0 && app.telemetry_us == 0);
}

static void test_capture_back_requests_browser_and_preserves_file(void) {
  App app = {.screen = ScreenCapture, .page = 2};
  snprintf(app.capture_path, sizeof(app.capture_path), CAPTURE_DIR "/capture-0003.pcapng");
  press(&app, InputKeyBack);
  assert(app.browse_requested && app.screen == ScreenHome && app.selected == 2);
  assert(!strcmp(app.capture_path, CAPTURE_DIR "/capture-0003.pcapng"));
  app.browse_requested = false;
  press(&app, InputKeyOk);
  assert(app.browse_requested && app.screen == ScreenHome);
}

static void test_target_setup_preserves_context(void) {
  App app = {.screen = ScreenDevices, .device_count = 3, .selected = 2,
             .online = true, .capture_capable = true};
  snprintf(app.devices[2].mac, sizeof(app.devices[2].mac), "02:00:00:00:00:12");
  press(&app, InputKeyOk);
  assert(app.screen == ScreenTarget && !strcmp(app.selected_device.mac, "02:00:00:00:00:12"));
  press(&app, InputKeyDown);
  press(&app, InputKeyOk);
  assert(app.capture_on_start && !app.recording && app.screen == ScreenTarget);
  press(&app, InputKeyDown);
  press(&app, InputKeyOk);
  assert(app.screen == ScreenHooks);
  press(&app, InputKeyBack);
  assert(app.screen == ScreenTarget && app.selected == 2 && app.capture_on_start);
  press(&app, InputKeyBack);
  assert(app.screen == ScreenDevices && app.selected == 2);
}

static void test_capture_ready_precedes_start(void) {
  App app = {.screen = ScreenDevices, .device_count = 1, .online = true, .state = 2,
             .recording = true, .capture_on_start = true};
  snprintf(app.devices[0].mac, sizeof(app.devices[0].mac), "02:00:00:00:00:12");
  press(&app, InputKeyOk);
  snprintf(app.devices[0].mac, sizeof(app.devices[0].mac), "11:22:33:44:55:66");
  sent[0] = 0;
  press(&app, InputKeyOk);
  assert(app.session_operation == SessionPreparingCapture && !sent[0]);
  app_receive(&app, WireEvent, 0, "target\t11:22:33:44:55:66");
  app_session_refresh(&app);
  assert(!sent[0]);
  app.capture_ready = true;
  app_session_refresh(&app);
  assert(!strcmp(sent, "start\t02:00:00:00:00:12"));
  app_receive(&app, WireReply, app.session_command, "ok\taccepted");
  app_receive(&app, WireEvent, 0, "status\t7\t1\t0\t0\t0\t0\thits:0");
  app_session_refresh(&app);
  assert(app.session_operation == SessionNone && app.screen == ScreenSession);
}

static void test_stop_waits_for_idle_then_saves(void) {
  App app = {.online = true, .state = 4, .recording = true, .capture_ready = true,
             .running = true, .target_connected = true};
  sent[0] = 0;
  app_session_stop(&app, true);
  assert(app.session_operation == SessionStopping && !sent[0] && !app.capture_stopping);
  app.state = 7;
  app_session_refresh(&app);
  assert(!strcmp(sent, "stop"));
  uint16_t command = app.session_command;
  app_receive(&app, WireReply, command + 1, "ok\taccepted");
  assert(!app.session_accepted);
  app_receive(&app, WireReply, command, "ok\taccepted");
  app_session_refresh(&app);
  assert(app.session_operation == SessionStopping && app.running && !app.capture_stopping);
  app_receive(&app, WireEvent, 0, "status\t0\t0\t0\t0\t0\t0\thits:0");
  app_session_refresh(&app);
  assert(app.session_operation == SessionSaving && app.capture_stopping && app.running);
  app.recording = false;
  app.capture_complete = true;
  app_session_refresh(&app);
  assert(app.session_operation == SessionStopped && !app.running);
}

static void test_stop_confirms_even_cached_idle_and_retries_busy(void) {
  App app = {.online = true, .state = 0};
  app_session_stop(&app, false);
  assert(app.session_operation == SessionStopping && app.session_command);
  app_receive(&app, WireReply, app.session_command,
              "error\ttransition in progress; retry stop");
  assert(!app.session_command && app.session_operation == SessionStopping);
  tick += 600;
  app_session_refresh(&app);
  assert(app.session_command && !app.session_accepted);
  tick += 31000;
  app_session_refresh(&app);
  assert(app.session_operation == SessionStopFailed && app.screen == ScreenSessionProgress);
  assert(strstr(app.session_error, "not confirmed"));
  press(&app, InputKeyOk);
  assert(app.session_operation == SessionStopping && app.session_command);
  app.online = false;
  app_session_refresh(&app);
  assert(app.session_operation == SessionStopFailed && strstr(app.session_error, "link lost"));
}

static void test_save_preserves_confirmed_stop_and_external_state(void) {
  App app = {.session_operation = SessionSaving, .recording = false, .online = false,
             .screen = ScreenSessionProgress};
  app_session_refresh(&app);
  assert(app.session_operation == SessionStopped && !app.capture_complete);
  app.online = true;
  app.state = 7;
  app.target_connected = true;
  app_session_refresh(&app);
  assert(app.session_operation == SessionNone && app.screen == ScreenSession);
  assert(app.page == 1 && app.target_connected);
  app.session_operation = SessionSaving;
  app_session_refresh(&app);
  assert(app.session_operation == SessionStopFailed && app.target_connected);
}

static void test_settings_setup_and_back_paths(void) {
  App app = {.screen = ScreenSettings, .online = true};
  assert(app_rows(&app) == 3);
  press(&app, InputKeyOk);
  assert(app.screen == ScreenWifi && !strcmp(sent, "wifi_get"));
  app_receive(&app, WireEvent, 0, "wifi\tboth\t426f617264\t486f6d65\tconnected\t10.0.0.2"
              "\t1\t1\t0\t");
  assert(app.wifi_loaded && app.wifi_mcp);
  press(&app, InputKeyDown);
  press(&app, InputKeyOk);
  assert(app.screen == ScreenWifiStatus);
  press(&app, InputKeyBack);
  assert(app.screen == ScreenWifi && app.selected == 1);
  press(&app, InputKeyDown);
  press(&app, InputKeyOk);
  assert(app.screen == ScreenWifiAp);
  press(&app, InputKeyUp); // Mode row, without saving it.
  press(&app, InputKeyOk);
  assert(app.wifi_dirty);
  press(&app, InputKeyBack);
  assert(app.screen == ScreenWifiDiscard);
  press(&app, InputKeyDown);
  press(&app, InputKeyOk);
  assert(!app.wifi_dirty && app.screen == ScreenWifi && app.selected == 2);
  press(&app, InputKeyBack);
  press(&app, InputKeyDown);
  press(&app, InputKeyOk);
  assert(app.screen == ScreenMcp);
  app.wifi_loaded = true;
  press(&app, InputKeyDown);
  press(&app, InputKeyOk);
  assert(app.screen == ScreenMcpInfo);
  press(&app, InputKeyBack);
  assert(app.screen == ScreenMcp && app.selected == 1);
  press(&app, InputKeyDown);
  press(&app, InputKeyOk);
  strcpy(app.wifi_key, "private-key");
  press(&app, InputKeyBack);
  assert(!app.wifi_key[0] && app.screen == ScreenMcp && app.selected == 2);
  press(&app, InputKeyBack);
  press(&app, InputKeyDown);
  press(&app, InputKeyOk);
  assert(app.screen == ScreenUart);
  press(&app, InputKeyUp);
  press(&app, InputKeyOk);
  assert(app.screen == ScreenDiagnostics);
  press(&app, InputKeyBack);
  assert(app.screen == ScreenUart && app.selected == 3);
  press(&app, InputKeyBack);
  assert(app.screen == ScreenSettings && app.selected == 2);
}

static void test_browser_approval_requires_flipper_button(void) {
  App app = {.screen = ScreenSession, .online = true};
  app_receive(&app, WireEvent, 0, "pair_request\t42\t192.168.1.12");
  assert(app.screen == ScreenPair && app.pair_id == 42);
  assert(!strcmp(app.pair_ip, "192.168.1.12"));
  sent[0] = 0;
  press(&app, InputKeyBack);
  assert(app.screen == ScreenSession && !strcmp(sent, "pair_deny\t42"));
  app_receive(&app, WireEvent, 0, "pair_request\t43\t192.168.1.12");
  sent[0] = 0;
  press(&app, InputKeyOk);
  assert(app.screen == ScreenSession && !strcmp(sent, "pair_allow\t43"));
  app_receive(&app, WireEvent, 0, "pair_request\t44\tbad-ip");
  assert(app.screen == ScreenSession);
  app_receive(&app, WireEvent, 0, "pair_request\t45\t192.168.1.12");
  tick += 60000;
  app_pair_refresh(&app);
  assert(app.screen == ScreenSession && !app.pair_id);
}

int main(void) {
  test_browser_approval_requires_flipper_button();
  test_settings_setup_and_back_paths();
  test_detail_navigation();
  test_home_preserves_action_when_session_appears();
  test_notice_does_not_activate_underlying_button();
  test_successful_reply_preserves_notice();
  test_hooks_are_read_only_from_every_entry();
  test_scan_results_sort_after_indexed_stream();
  test_traffic_collects_during_inspection_and_returns_to_latest();
  test_browsing_does_not_stop_receiving();
  test_diagnostics_compatibility();
  test_capture_back_requests_browser_and_preserves_file();
  test_target_setup_preserves_context();
  test_capture_ready_precedes_start();
  test_stop_waits_for_idle_then_saves();
  test_stop_confirms_even_cached_idle_and_retries_busy();
  test_save_preserves_confirmed_stop_and_external_state();
  puts("UI behavior checks passed");
  return 0;
}
