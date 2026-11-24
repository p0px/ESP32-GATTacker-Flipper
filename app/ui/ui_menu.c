#include "ui.h"
#include <stdio.h>

static const char *const HOME[] = {
    "Scan devices", "Active session", "Hooks and rules", "Saved captures", "Settings", "About",
};
static const UiIcon HOME_ICONS[] = {UiRadar, UiLink, UiRules, UiFile, UiPlug, UiInfo};
static const char *const SESSION[] = {
    "Connection map", "Traffic preview", "GATT services", "Hooks and rules", "Record PCAP",
    "Stop session",
};
static const UiIcon SESSION_ICONS[] = {UiLink, UiPacket, UiStack, UiRules, UiRecord, UiStop};

static void row_label(const App *app, unsigned i, char *out, size_t size) {
  switch (app->screen) {
  case ScreenHome: {
    HomeItem item = app_home_item(app, i);
    snprintf(out, size, "%s", item == HomeSession && !app_session_active(app)
        ? "Previous session" : HOME[item]);
    break;
  }
  case ScreenSession:
    snprintf(out, size, "%s", i == 4 && !app_session_active(app) ? "Device setup"
        : i == 4 && app->capture_exporting ? "Copying PCAP"
        : i == 4 && app->recording ? "Recording status"
        : i == 5 && (app->session_operation == SessionStopping ||
                     app->session_operation == SessionSaving) ? "Stopping..." : SESSION[i]);
    break;
  case ScreenSettings:
    snprintf(out, size, "%s", i == 0 ? "Wi-Fi" : i == 1 ? "Agent control" : "UART / board");
    break;
  case ScreenUart:
    if (!i)
      snprintf(out, size, "TX %u / RX %u", app->port ? 15 : 13, app->port ? 16 : 14);
    else if (i == 1)
      snprintf(out, size, "%u baud", 115200U << app->baud_index);
    else
      snprintf(out, size, "%s", i == 2 ? "Apply UART setup"
          : "Diagnostics");
    break;
  case ScreenExit:
    snprintf(out, size, "%s", i == 0 ? "Keep ESP running" : i == 1 ? "Stop ESP + exit"
                                                                            : "Stay in app");
    break;
  default: ui_wifi_label(app, i, out, size); break;
  }
}

static UiIcon row_icon(const App *app, unsigned i) {
  if (app->screen == ScreenHome) return HOME_ICONS[app_home_item(app, i)];
  if (app->screen == ScreenSession)
    return i == 4 && !app_session_active(app) ? UiTarget : SESSION_ICONS[i];
  if (app->screen == ScreenExit) return i == 0 ? UiLink : i == 1 ? UiStop : UiBack;
  if (app->screen == ScreenSettings) return i == 0 ? UiRadar : i == 1 ? UiChip : UiPlug;
  if (app->screen == ScreenUart) return i == 3 ? UiChip : UiPlug;
  if (app_wifi_screen(app->screen)) return ui_wifi_icon(app, i);
  return UiInfo;
}

void ui_menu(Canvas *c, const App *app) {
  unsigned count = app_rows(app), start = (app->selected / 3) * 3;
  for (unsigned row = 0; row < 3 && start + row < count; row++) {
    unsigned index = start + row, y = 14 + row * 13;
    bool selected = index == app->selected;
    char label[80];
    row_label(app, index, label, sizeof(label));
    ui_card(c, y, 13, selected);
    ui_icon(c, 3, y + 1, row_icon(app, index));
    canvas_set_font(c, selected ? FontPrimary : FontSecondary);
    ui_fit(c, 19, y + 10, 101, label);
    canvas_set_color(c, ColorBlack);
  }
  ui_scroll(c, start, count, 3);
  const char *action = "Open";
  if (app->screen == ScreenUart)
    action = app->selected < 2 ? "Change" : app->selected == 2 ? "Apply" : "Open";
  if (app->screen == ScreenExit) action = "Choose";
  if (app_wifi_screen(app->screen)) action = ui_wifi_action(app);
  if (app->screen == ScreenSession && app_session_active(app) && app->selected == 4)
    action = app->recording ? "Open" : "REC";
  if (app->screen == ScreenSession && app->selected == 5) action = "Stop";
  ui_footer(c, app->screen == ScreenHome ? "Exit" : "Back", action, app->selected, count);
}
