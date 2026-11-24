#include "../app.h"
#include <stdio.h>
#include <string.h>

static void open_hooks(App *app) {
  app->hooks_return = app->screen;
  app_open(app, ScreenHooks);
}

static void previous_setup(App *app) {
  app->target_return = ScreenSession;
  if (strcmp(app->selected_device.mac, app->session_mac)) {
    memset(&app->selected_device, 0, sizeof(app->selected_device));
    snprintf(app->selected_device.mac, sizeof(app->selected_device.mac), "%s", app->session_mac);
    snprintf(app->selected_device.name, sizeof(app->selected_device.name), "Previous target");
  }
  app_open(app, ScreenTarget);
}

static void start_scan(App *app) {
  app->device_count = 0;
  app->results_loaded = false;
  app->scan_active = true;
  app->scan_started = furi_get_tick();
  app->scan_id = app_command(app, "scan");
  app_open(app, ScreenDevices);
}

static void activate(App *app) {
  switch (app->screen) {
  case ScreenPair:
    app_pair_reply(app, true);
    break;
  case ScreenHome:
    switch (app_home_item(app, app->selected)) {
    case HomeScan:
      start_scan(app);
      break;
    case HomeSession:
      app_open(app, ScreenSession);
      break;
    case HomeHooks:
      open_hooks(app);
      break;
    case HomeCaptures:
      app->browse_requested = true;
      break;
    case HomeSettings:
      app_open(app, ScreenSettings);
      break;
    case HomeAbout:
      app_open(app, ScreenAbout);
      break;
    }
    break;
  case ScreenDevices:
    if (app->scan_id || app->results_id || app->state == 1) break;
    if (app->device_count) {
      app->selected_device = app->devices[app->selected];
      app->target_return = ScreenDevices;
      app_open(app, ScreenTarget);
    } else if (app->online && app->state != 1 && !app->scan_id && !app->results_id)
      start_scan(app);
    break;
  case ScreenTarget:
    if (!app->selected) {
      app_session_start(app);
    } else if (app->selected == 1) {
      if (app->recording) {
        app->recording_return = ScreenTarget;
        app_open(app, ScreenRecording);
      } else if (app->capture_on_start || (app->online && app->capture_capable)) {
        app->capture_on_start = !app->capture_on_start;
      } else {
        app_notice(app, "PCAP needs a connected board with HCI capture support.");
      }
    } else if (app->selected == 2) {
      open_hooks(app);
    } else {
      app_open(app, ScreenTargetInfo);
    }
    break;
  case ScreenSession:
    if (app->page) {
      app->page = 0;
      break;
    }
    switch (app->selected) {
    case 0:
      app->page = 1;
      break;
    case 1:
      app_open(app, ScreenTraffic);
      app_traffic_resume(app);
      break;
    case 2:
      if (app_session_active(app) && (app->state == 7 || !app->service_count)) {
        app->service_count = 0;
        app->services_id = app_command(app, "services");
      }
      app_open(app, ScreenServices);
      break;
    case 3:
      open_hooks(app);
      break;
    case 4:
      if (!app_session_active(app)) {
        previous_setup(app);
        break;
      }
      app->recording_return = ScreenSession;
      if (app->recording || app->capture_exporting)
        app_open(app, ScreenRecording);
      else
        app_capture_toggle(app);
      break;
    case 5:
      app_session_stop(app, false);
      break;
    }
    break;
  case ScreenTraffic:
    if (app->event_count) {
      // Inspection owns one immutable copy; background collection never stops.
      app->inspected_event = *app_traffic_at(app, app->selected);
      app->paused = true;
      app->screen = ScreenEvent;
      app->page = 0;
    }
    break;
  case ScreenServices:
    if (app->service_count) {
      app->screen = ScreenService;
      app->page = 0;
    }
    break;
  case ScreenSettings:
    app_open(app, app->selected == 0 ? ScreenWifi :
             app->selected == 1 ? ScreenMcp : ScreenUart);
    if (app->screen != ScreenUart) app_command(app, "wifi_get");
    break;
  case ScreenUart:
    if (app->recording && app->selected < 3) {
      app_notice(app, "Save the recording before changing UART settings.");
      break;
    }
    if (!app->selected)
      app->port ^= 1;
    else if (app->selected == 1)
      app->baud_index = (app->baud_index + 1) % 3;
    else if (app->selected == 2) {
      app_serial_close(app);
      app_settings_save(app);
      app_serial_open(app);
    } else app_open(app, ScreenDiagnostics);
    break;
  case ScreenWifi: case ScreenMcp: case ScreenWifiReview: case ScreenWifiAp:
  case ScreenWifiDiscard:
    app_wifi_activate(app);
    break;
  case ScreenWifiNetworks:
    app_wifi_networks_activate(app);
    break;
  case ScreenRecording:
    if (app->capture_exporting) app_notice(app, "Copying RAM capture to SD. Please wait.");
    else if (app->recording) app_capture_toggle(app);
    else {
      app_open(app, app->recording_return);
      app->selected = app->screen == ScreenTarget ? 1 : 4;
    }
    break;
  case ScreenTargetInfo:
    app_open(app, ScreenTarget);
    app->selected = 3;
    break;
  case ScreenSessionProgress:
    if (app->session_operation == SessionStopFailed) {
      app_session_stop(app, false);
    } else if (app->session_operation == SessionStopped) {
      bool same_target = app->session_mac[0] &&
          !strcmp(app->session_mac, app->selected_device.mac);
      app_open(app, same_target ? ScreenTarget : ScreenHome);
    } else if (app->session_operation == SessionStartFailed) {
      app_open(app, ScreenSession);
    }
    break;
  case ScreenExit:
    if (!app->selected)
      app->running = false;
    else if (app->selected == 1)
      app_session_stop(app, true);
    else
      app_open(app, ScreenHome);
    break;
  default:
    break;
  }
}

void app_handle(App *app, const InputEvent *event) {
  if (event->type != InputTypeShort && event->type != InputTypeRepeat)
    return;
  if (event->type == InputTypeRepeat &&
      (event->key == InputKeyOk || event->key == InputKeyBack))
    return;
  if (app->notice) {
    if (event->key == InputKeyOk || event->key == InputKeyBack)
      app->notice = false;
    return;
  }
  if (event->key == InputKeyBack) {
    if (app->screen == ScreenPair) {
      app_pair_reply(app, false);
    } else if (app->screen == ScreenHome)
      app_open(app, ScreenExit);
    else if (app_wifi_back(app)) {
      return;
    } else if (app->screen == ScreenUart) {
      app_open(app, ScreenSettings);
      app->selected = 2;
    } else if (app->screen == ScreenCapture) {
      app_home_select(app, HomeCaptures);
      app->browse_requested = true;
    } else if (app->screen == ScreenDiagnostics) {
      app_open(app, ScreenUart);
      app->selected = 3;
    } else if (app->screen == ScreenTargetInfo) {
      app_open(app, ScreenTarget);
      app->selected = 3;
    } else if (app->screen == ScreenHooks) {
      if (app->hooks_return == ScreenHome) app_home_select(app, HomeHooks);
      else {
        app_open(app, app->hooks_return);
        app->selected = app->screen == ScreenSession ? 3 : 2;
      }
    } else if (app->screen == ScreenRecording) {
      app_open(app, app->recording_return);
      app->selected = app->screen == ScreenTarget ? 1 : 4;
    } else if (app->screen == ScreenSessionProgress) {
      app_open(app, ScreenSession);
      app->selected = app_rows(app) - 1;
    } else if (app->screen == ScreenEvent) {
      app_open(app, ScreenTraffic);
      app_traffic_resume(app);
    } else if (app->screen == ScreenService)
      app->screen = ScreenServices;
    else if (app->screen == ScreenTarget) {
      if (app->target_return == ScreenSession) {
        app_open(app, ScreenSession);
        app->selected = 4;
      } else {
        app_open(app, ScreenDevices);
        for (unsigned i = 0; i < app->device_count; i++) {
          if (!strcmp(app->devices[i].mac, app->selected_device.mac)) {
            app->selected = i;
            break;
          }
        }
      }
    } else if (app->screen == ScreenTraffic || app->screen == ScreenServices) {
      unsigned selection = app->screen == ScreenTraffic ? 1 : 2;
      app_open(app, ScreenSession);
      app->selected = selection;
    } else if (app->screen == ScreenSession) {
      if (app->page) app->page = 0;
      else {
        app_home_select(app, HomeSession);
      }
    } else if (app->screen == ScreenSettings) {
      app_home_select(app, HomeSettings);
    } else if (app->screen == ScreenAbout) {
      app_home_select(app, HomeAbout);
    } else
      app_open(app, ScreenHome);
    return;
  }
  if (event->key == InputKeyOk) {
    activate(app);
    return;
  }
  if (app->screen == ScreenSession && app->page)
    return;
  if (app->screen == ScreenDevices && (app->scan_id || app->results_id || app->state == 1))
    return;
  int delta = event->key == InputKeyUp || event->key == InputKeyLeft ? -1 : 1;
  if (app->screen == ScreenEvent || app->screen == ScreenCapture ||
      app->screen == ScreenService || app->screen == ScreenDiagnostics ||
      app->screen == ScreenTargetInfo) {
    if (delta > 0 && app->page + 1 < app->page_count)
      app->page++;
    else if (delta < 0 && app->page)
      app->page--;
  } else if (event->key == InputKeyLeft || event->key == InputKeyRight) {
    if (app->screen == ScreenTraffic) {
      if (event->key == InputKeyRight) app_traffic_resume(app);
      else app->paused = true;
    }
  } else {
    if (app->screen == ScreenTraffic)
      app->paused = true;
    unsigned rows = app_rows(app);
    if (rows)
      app->selected = (app->selected + rows + delta) % rows;
  }
}
