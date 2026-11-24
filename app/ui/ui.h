#pragma once
#include "../app.h"

typedef enum {
  UiRadar, UiLink, UiRules, UiFile, UiPlug, UiPacket, UiStack, UiRecord,
  UiStop, UiBack, UiPhone, UiChip, UiTarget, UiWarning, UiInfo,
} UiIcon;

void ui_icon(Canvas *canvas, int x, int y, UiIcon icon);
void ui_fit(Canvas *canvas, int x, int y, unsigned width, const char *text);
void ui_header(Canvas *canvas, const App *app, const char *title);
void ui_footer(Canvas *canvas, const char *back, const char *action,
               unsigned index, unsigned count);
void ui_card(Canvas *canvas, unsigned y, unsigned height, bool selected);
void ui_scroll(Canvas *canvas, unsigned first, unsigned count, unsigned visible);
unsigned ui_text(Canvas *canvas, const char *text, unsigned page);
void ui_empty(Canvas *canvas, UiIcon icon, const char *title, const char *hint);
void ui_menu(Canvas *canvas, const App *app);
void ui_wifi_label(const App *app, unsigned row, char *out, size_t capacity);
UiIcon ui_wifi_icon(const App *app, unsigned row);
const char *ui_wifi_action(const App *app);
void ui_devices(Canvas *canvas, const App *app);
void ui_target(Canvas *canvas, const App *app);
void ui_target_info(Canvas *canvas, const App *app);
void ui_session_progress(Canvas *canvas, const App *app);
void ui_session(Canvas *canvas, const App *app);
void ui_traffic(Canvas *canvas, const App *app);
void ui_event(Canvas *canvas, App *app);
void ui_services(Canvas *canvas, const App *app);
void ui_service(Canvas *canvas, App *app);
void ui_diagnostics(Canvas *canvas, App *app);
void ui_capture(Canvas *canvas, App *app);
void ui_recording(Canvas *canvas, const App *app);
const char *ui_operation(char operation);
const char *ui_direction(const char *direction);
void ui_uuid_short(const char *uuid, char *out, size_t size);

void ui_wifi(Canvas *canvas, const App *app);
void ui_pair(Canvas *canvas, const App *app);
