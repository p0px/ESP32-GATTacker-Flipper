#include "ui.h"
#include <stdio.h>
#include <string.h>

static const char *title(const App *app) {
  static const char *const titles[] = {
      "ESP32-GATTacker", "Scanned devices", "Target", "Session", "Traffic", "Event",
      "GATT services", "Hooks and rules", "Settings", "Leave app?",
      "Capture preview", "GATT details", "Diagnostics", "Packet capture",
      "Target info", "Session control", "About", "Wi-Fi", "Connection", "MCP key",
      "Wi-Fi networks", "UART / board", "Agent control", "MCP server",
      "Network setup", "Hotspot / mode", "Unsaved changes",
      "Browser access",
  };
  if (app->screen == ScreenSession) {
    if (app->page)
      return app->active_mac[0] ? app->active_mac
          : app->session_mac[0] ? app->session_mac : "Connection map";
    if (!app_session_active(app)) return "Previous session";
  }
  if (app->screen == ScreenTarget && app->selected_device.name[0])
    return app->selected_device.name;
  if (app->screen == ScreenCapture && app->capture_path[0]) {
    const char *name = strrchr(app->capture_path, '/');
    return name ? name + 1 : app->capture_path;
  }
  return titles[app->screen];
}

void app_draw(Canvas *c, void *context) {
  App *app = context;
  /* GUI owns its viewport lock here. Waiting for the app would let storage or
   * a modal handoff stall the entire GUI; the main loop schedules another draw. */
  if (furi_mutex_acquire(app->mutex, 0) != FuriStatusOk) return;
  canvas_clear(c);
  char traffic_title[40];
  const char *heading = app->notice ? "Attention" : title(app);
  if (!app->notice && app->screen == ScreenTraffic) {
    snprintf(traffic_title, sizeof(traffic_title), "%s #%lu",
             app->paused ? "Browse" : "Live", (unsigned long)app->event_total);
    heading = traffic_title;
  }
  ui_header(c, app, heading);
  if (app->notice) {
    ui_text(c, app->message, 0);
    ui_footer(c, "Close", "Close", 0, 0);
  } else {
    switch (app->screen) {
    case ScreenWifi: case ScreenWifiStatus: case ScreenWifiKey: case ScreenWifiNetworks:
    case ScreenMcp: case ScreenMcpInfo: case ScreenWifiReview: case ScreenWifiAp:
    case ScreenWifiDiscard:
      ui_wifi(c, app); break;
    case ScreenPair: ui_pair(c, app); break;
    case ScreenDevices: ui_devices(c, app); break;
    case ScreenTarget:
      ui_target(c, app);
      break;
    case ScreenTargetInfo:
      app->page_count = 2;
      ui_target_info(c, app);
      break;
    case ScreenSessionProgress:
      ui_session_progress(c, app);
      break;
    case ScreenTraffic: ui_traffic(c, app); break;
    case ScreenEvent: ui_event(c, app); break;
    case ScreenServices: ui_services(c, app); break;
    case ScreenService: ui_service(c, app); break;
    case ScreenHooks:
      ui_empty(c, UiRules, "Web UI hooks", "Placeholder");
      canvas_draw_str(c, 2, 53, "Use the ESP32 web UI.");
      ui_footer(c, "Back", "", 0, 0);
      break;
    case ScreenAbout:
      ui_text(c, "github.com/p0px\nmade with <3 p0px\n"
              "Scan > select > Start.\nPCAP in device setup.", 0);
      ui_footer(c, "Back", "", 0, 0);
      break;
    case ScreenDiagnostics: ui_diagnostics(c, app); break;
    case ScreenCapture:
      ui_capture(c, app);
      break;
    case ScreenRecording:
      ui_recording(c, app);
      break;
    case ScreenSession:
      if (app->page) {
        ui_session(c, app);
        break;
      }
      // The actions share the same focused menu layout as the home screen.
      ui_menu(c, app);
      break;
    default: ui_menu(c, app); break;
    }
  }
  furi_mutex_release(app->mutex);
}

void app_input(InputEvent *event, void *context) {
  App *app = context;
  // Only enqueue events used by the app; release/press noise must not crowd out short taps.
  if (event->type == InputTypeShort || event->type == InputTypeRepeat)
    furi_message_queue_put(app->input, event, 0);
}
