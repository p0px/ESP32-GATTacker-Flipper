#include "ui.h"
#include <stdio.h>
#include <string.h>

void ui_fit(Canvas *c, int x, int y, unsigned width, const char *text) {
  char line[128];
  snprintf(line, sizeof(line), "%s", text);
  if (canvas_string_width(c, line) > width) {
    size_t length = strlen(line);
    while (length && canvas_string_width(c, line) + canvas_string_width(c, "..") > width)
      line[--length] = 0;
    snprintf(line + length, sizeof(line) - length, "..");
  }
  canvas_draw_str(c, x, y, line);
}

void ui_header(Canvas *c, const App *app, const char *title) {
  canvas_set_font(c, FontPrimary);
  if (canvas_string_width(c, title) > 100) canvas_set_font(c, FontSecondary);
  ui_fit(c, 2, 9, 100, title);
  canvas_set_font(c, FontSecondary);
  if (app->notice) {
    ui_icon(c, 115, 0, UiWarning);
  } else if (app->recording) {
    canvas_draw_disc(c, 104, 5, 2);
    canvas_draw_str(c, 109, 9, "REC");
  } else {
    ui_icon(c, 115, 0, app->online ? UiLink : UiPlug);
    if (!app->online)
      canvas_draw_line(c, 114, 10, 127, 0);
  }
  canvas_draw_line(c, 0, 12, 127, 12);
}

void ui_footer(Canvas *c, const char *back, const char *action,
               unsigned index, unsigned count) {
  canvas_set_color(c, ColorBlack);
  canvas_set_font(c, FontSecondary);
  canvas_draw_line(c, 0, 55, 127, 55);
  canvas_draw_line(c, 1, 60, 6, 60);
  canvas_draw_line(c, 1, 60, 3, 58);
  canvas_draw_line(c, 1, 60, 3, 62);
  canvas_draw_str(c, 9, 63, back);
  if (action && *action) {
    canvas_draw_rframe(c, 53, 58, 7, 6, 1);
    canvas_draw_dot(c, 56, 60);
    canvas_draw_str(c, 63, 63, action);
  }
  if (count) {
    char position[16];
    snprintf(position, sizeof(position), "%u/%u", index + 1, count);
    canvas_draw_str_aligned(c, 127, 63, AlignRight, AlignBottom, position);
  }
}

void ui_card(Canvas *c, unsigned y, unsigned height, bool selected) {
  if (selected) {
    canvas_draw_rbox(c, 0, y, 123, height, 2);
    canvas_set_color(c, ColorWhite);
  }
}

void ui_scroll(Canvas *c, unsigned first, unsigned count, unsigned visible) {
  if (count <= visible)
    return;
  canvas_set_color(c, ColorBlack);
  canvas_draw_line(c, 126, 15, 126, 52);
  unsigned height = 38 * visible / count;
  if (height < 4)
    height = 4;
  if (first > count - visible)
    first = count - visible;
  unsigned y = 15 + (38 - height) * first / (count - visible);
  canvas_draw_box(c, 125, y, 3, height);
}

unsigned ui_text(Canvas *c, const char *text, unsigned page) {
  unsigned row = 0;
  const char *p = text;
  while (*p) {
    char line[128];
    size_t n = 0, space = 0;
    while (p[n] && p[n] != '\n' && n < sizeof(line) - 2) {
      line[n] = p[n] == '\t' ? ' ' : p[n];
      line[n + 1] = 0;
      if (canvas_string_width(c, line) > 124)
        break;
      if (line[n] == ' ')
        space = n;
      n++;
    }
    if (p[n] && p[n] != '\n' && space > 6)
      n = space;
    line[n] = 0;
    if (row / 4 == page)
      canvas_draw_str(c, 2, 22 + (row % 4) * 10, line);
    p += n;
    if (*p == '\n' || *p == ' ' || *p == '\t')
      p++;
    row++;
  }
  return row ? (row + 3) / 4 : 1;
}

void ui_empty(Canvas *c, UiIcon icon, const char *title, const char *hint) {
  canvas_draw_rframe(c, 3, 22, 25, 25, 4);
  ui_icon(c, 10, 29, icon);
  canvas_set_font(c, FontPrimary);
  ui_fit(c, 35, 29, 91, title);
  canvas_set_font(c, FontSecondary);
  ui_fit(c, 35, 42, 91, hint);
}

const char *ui_operation(char operation) {
  return operation == 'W' ? "Write" : operation == 'N' ? "Notify"
         : operation == 'R' ? "Read" : "Any traffic";
}

const char *ui_direction(const char *direction) {
  if (!strcmp(direction, "victim -> target")) return "Central > Target";
  if (!strcmp(direction, "target -> victim")) return "Target > Central";
  if (strstr(direction, "cache")) return "Cached traffic";
  return direction;
}

void ui_uuid_short(const char *uuid, char *out, size_t size) {
  if (strlen(uuid) > 16)
    snprintf(out, size, "%.8s..%.4s", uuid, uuid + strlen(uuid) - 4);
  else
    snprintf(out, size, "%s", uuid[0] ? uuid : "Any UUID");
}
