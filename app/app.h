#pragma once
#include "core/pcap.h"
#include "core/wire.h"
#include <dialogs/dialogs.h>
#include <expansion/expansion.h>
#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <storage/storage.h>

#define DEVICE_COUNT 50
#define EVENT_COUNT 24
#define SERVICE_COUNT 64
#define CAPTURE_DIR "/ext/apps_data/gattacker"

typedef enum {
  ScreenHome,
  ScreenDevices,
  ScreenTarget,
  ScreenSession,
  ScreenTraffic,
  ScreenEvent,
  ScreenServices,
  ScreenHooks,
  ScreenSettings,
  ScreenExit,
  ScreenCapture,
  ScreenService,
  ScreenDiagnostics,
  ScreenRecording,
  ScreenTargetInfo,
  ScreenSessionProgress,
  ScreenAbout,
  ScreenWifi, ScreenWifiStatus, ScreenWifiKey, ScreenWifiNetworks,
  ScreenUart, ScreenMcp, ScreenMcpInfo, ScreenWifiReview, ScreenWifiAp, ScreenWifiDiscard,
  ScreenPair,
} Screen;
typedef enum {
  HomeScan, HomeSession, HomeHooks, HomeCaptures, HomeSettings, HomeAbout,
} HomeItem;
typedef enum {
  SessionNone, SessionPreparingCapture, SessionStarting, SessionStopping, SessionSaving,
  SessionStopped, SessionStartFailed, SessionStopFailed,
} SessionOperation;
typedef struct {
  char name[32];
  char mac[18];
  int rssi;
} Device;
typedef struct {
  char ssid[33];
  int rssi;
  bool secured;
} WifiNetwork;
typedef struct {
  char uuid[37];
  char operation;
  char direction[24];
  char handle[8];
  char data[301];
  char detail[72];
} TrafficEvent;

typedef struct {
  char uuid[37];
  uint16_t handle;
  uint8_t properties;
  bool characteristic;
} Service;
typedef struct {
  Gui *gui;
  ViewPort *viewport;
  FuriMutex *mutex;
  FuriMessageQueue *input;
  FuriStreamBuffer *rx;
  FuriHalSerialHandle *serial;
  Expansion *expansion;
  Storage *storage;
  File *capture;
  WireParser parser;
  Screen screen;
  Screen recording_return;
  Screen hooks_return;
  Screen target_return;
  SessionOperation session_operation;
  bool capture_on_start;
  bool session_accepted;
  bool session_had_capture;
  uint16_t session_command;
  uint32_t session_at;
  uint32_t session_retry_at;
  char session_mac[18];
  char session_error[80];
  bool wifi_loaded, wifi_dirty, wifi_ap_changed, wifi_station_changed;
  bool wifi_mcp, wifi_ap_active, wifi_edit_requested;
  unsigned wifi_mode, wifi_field;
  uint16_t wifi_command;
  char wifi_ap[33], wifi_station[33], wifi_ap_password[64], wifi_station_password[64];
  char wifi_state[32], wifi_ip[16], wifi_key[33];
  uint32_t wifi_reason;
  char wifi_error[48];
  WifiNetwork wifi_networks[16];
  unsigned wifi_network_count;
  uint16_t wifi_network_mask;
  bool wifi_scanning, wifi_networks_loaded;
  char wifi_scan_error[48];
  Screen wifi_save_return, wifi_discard_from;
  Screen pair_return;
  uint32_t pair_id, pair_at;
  char pair_ip[16];
  unsigned wifi_discard_row;
  bool wifi_password_required;
  unsigned selected;
  unsigned page;
  unsigned page_count;
  bool notice;
  bool browse_requested;
  bool running;
  bool online;
  bool paused;
  bool recording;
  bool capture_capable;
  bool capture_ram_capable;
  bool capture_buffered;
  bool capture_remote_capable;
  bool capture_exporting;
  uint32_t capture_export_size;
  uint32_t capture_export_hash;
  uint32_t capture_export_at;
  bool capture_ready;
  bool capture_stopping;
  bool capture_pcap;
  bool capture_complete;
  bool exit_after_capture;
  uint16_t capture_command;
  uint32_t capture_token;
  uint32_t capture_requested_token;
  uint32_t capture_remote_last;
  uint32_t capture_at;
  uint32_t capture_heartbeat;
  uint32_t capture_packets;
  uint32_t capture_sequence;
  uint32_t capture_dropped;
  bool capture_health;
  uint32_t capture_health_accepted;
  uint32_t capture_full;
  uint32_t capture_invalid;
  uint32_t capture_peak;
  uint32_t capture_capacity;
  uint32_t capture_rejected;
  uint32_t capture_bytes;
  uint64_t capture_timestamp;
  uint8_t capture_buffer[PCAP_BUFFER_SIZE];
  uint8_t capture_batch[4096];
  size_t capture_batch_size;
  uint32_t capture_batch_packets;
  uint32_t capture_batch_at;
  uint8_t capture_h4[CAPTURE_H4_MAX];
  bool exit_after_stop;
  unsigned port;
  unsigned baud_index;
  uint16_t next_id;
  uint16_t pending_id;
  uint16_t scan_id;
  uint16_t results_id;
  uint16_t services_id;
  uint32_t pending_at;
  uint32_t scan_started;
  uint32_t last_rx;
  uint32_t last_poll;
  volatile uint32_t rx_dropped;
  uint32_t board_dropped;
  uint32_t internal_ram;
  uint32_t largest_block;
  uint32_t telemetry_us;
  unsigned state;
  unsigned device_count;
  bool results_loaded;
  bool scan_active;
  unsigned event_count;
  unsigned event_head;
  uint32_t event_total;
  uint32_t event_at;
  unsigned service_count;
  bool target_connected;
  bool phone_connected;
  bool browser_hooks;
  char message[80];
  char active_mac[18];
  char capture_path[128];
  char capture_text[8192];
  /* GUI callbacks have a small firmware-owned stack; keep capture parsing storage here. */
  char capture_line[WIRE_MAX];
  Device selected_device;
  Device devices[DEVICE_COUNT];
  TrafficEvent events[EVENT_COUNT];
  TrafficEvent inspected_event;
  Service services[SERVICE_COUNT];
} App;

void app_draw(Canvas *canvas, void *context);
void app_input(InputEvent *event, void *context);
void app_handle(App *app, const InputEvent *event);
void app_receive(void *context, WireKind kind, uint16_t id, const char *text);
const TrafficEvent *app_traffic_at(const App *app, unsigned index);
void app_traffic_receive(App *app, char **fields, size_t count);
void app_traffic_reset(App *app);
void app_traffic_resume(App *app);
uint16_t app_command(App *app, const char *command);
void app_refresh(App *app);
void app_session_start(App *app);
void app_session_stop(App *app, bool exit_when_stopped);
void app_session_receive(App *app, WireKind kind, uint16_t id, char **fields, size_t count);
void app_session_refresh(App *app);
bool app_serial_open(App *app);
void app_serial_close(App *app);
void app_capture_toggle(App *app);
void app_capture_report(App *app);
void app_capture_remote(App *app, WireKind kind, char **fields, size_t count);
void app_export_receive(App *app, WireKind kind, char **fields, size_t count);
void app_export_abort(App *app);
void app_capture_receive(App *app, WireKind kind, uint16_t id, char **fields, size_t count);
void app_capture_refresh(App *app);
void app_capture_finish(App *app, bool complete);
void app_capture_browse(App *app);
void app_settings_load(App *app);
void app_settings_save(App *app);
unsigned app_rows(const App *app);
bool app_session_active(const App *app);
bool app_has_session(const App *app);
HomeItem app_home_item(const App *app, unsigned index);
unsigned app_home_index(const App *app, HomeItem item);
void app_home_select(App *app, HomeItem item);
void app_open(App *app, Screen screen);
const char *app_state_name(unsigned state);
void app_notice(App *app, const char *message);

void app_wifi_receive(App *app, char **fields, size_t count);
void app_wifi_activate(App *app);
bool app_wifi_screen(Screen screen);
bool app_wifi_back(App *app);
void app_wifi_reply(App *app, uint16_t id, const char *text);
void app_wifi_edited(App *app, const char *text);
void app_wifi_edit(App *app);
void app_wifi_scan(App *app);
void app_wifi_networks_receive(App *app, char **fields, size_t count);
void app_wifi_networks_activate(App *app);
void app_receive_pending(App *app);
void app_pair_receive(App *app, WireKind kind, char **fields, size_t count);
void app_pair_reply(App *app, bool allow);
void app_pair_refresh(App *app);
