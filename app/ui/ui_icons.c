#include "ui.h"

void ui_icon(Canvas *c, int x, int y, UiIcon icon) {
  switch (icon) {
  case UiInfo:
    canvas_draw_circle(c, x + 5, y + 5, 5);
    canvas_draw_dot(c, x + 5, y + 2);
    canvas_draw_line(c, x + 5, y + 4, x + 5, y + 8);
    break;
  case UiRadar:
    canvas_draw_circle(c, x + 5, y + 5, 5);
    canvas_draw_circle(c, x + 5, y + 5, 2);
    canvas_draw_line(c, x + 5, y + 5, x + 9, y + 1);
    canvas_draw_dot(c, x + 3, y + 2);
    break;
  case UiLink:
    canvas_draw_rframe(c, x, y + 6, 6, 5, 2);
    canvas_draw_rframe(c, x + 5, y, 6, 5, 2);
    canvas_draw_line(c, x + 3, y + 8, x + 8, y + 2);
    break;
  case UiRules:
    for (int row = 0; row < 3; row++) {
      canvas_draw_line(c, x, y + 1 + row * 4, x + 10, y + 1 + row * 4);
      canvas_draw_box(c, x + (row == 1 ? 7 : 2), y + row * 4, 2, 3);
    }
    break;
  case UiFile:
    canvas_draw_frame(c, x + 2, y, 8, 11);
    for (int row = 0; row < 3; row++)
      canvas_draw_line(c, x + 4, y + 3 + row * 2, x + 7, y + 3 + row * 2);
    break;
  case UiPlug:
    canvas_draw_line(c, x + 3, y, x + 3, y + 3);
    canvas_draw_line(c, x + 7, y, x + 7, y + 3);
    canvas_draw_rframe(c, x + 1, y + 3, 9, 5, 1);
    canvas_draw_line(c, x + 5, y + 8, x + 5, y + 11);
    break;
  case UiPacket:
    canvas_draw_line(c, x, y + 3, x + 9, y + 3);
    canvas_draw_line(c, x + 7, y + 1, x + 9, y + 3);
    canvas_draw_line(c, x + 7, y + 5, x + 9, y + 3);
    canvas_draw_line(c, x + 1, y + 8, x + 10, y + 8);
    canvas_draw_line(c, x + 1, y + 8, x + 3, y + 6);
    canvas_draw_line(c, x + 1, y + 8, x + 3, y + 10);
    break;
  case UiStack:
    for (int row = 0; row < 3; row++) {
      canvas_draw_frame(c, x, y + row * 4, 11, 3);
      canvas_draw_dot(c, x + 2, y + row * 4 + 1);
    }
    break;
  case UiRecord:
    canvas_draw_circle(c, x + 5, y + 5, 5);
    canvas_draw_disc(c, x + 5, y + 5, 2);
    break;
  case UiStop:
    canvas_draw_rframe(c, x, y, 11, 11, 2);
    canvas_draw_box(c, x + 3, y + 3, 5, 5);
    break;
  case UiBack:
    canvas_draw_line(c, x + 2, y + 5, x + 9, y + 5);
    canvas_draw_line(c, x + 2, y + 5, x + 5, y + 2);
    canvas_draw_line(c, x + 2, y + 5, x + 5, y + 8);
    break;
  case UiPhone:
    canvas_draw_rframe(c, x + 2, y, 8, 13, 1);
    canvas_draw_line(c, x + 4, y + 2, x + 7, y + 2);
    canvas_draw_dot(c, x + 6, y + 10);
    break;
  case UiChip:
    canvas_draw_frame(c, x + 2, y + 2, 8, 8);
    canvas_draw_box(c, x + 4, y + 4, 4, 4);
    for (int pin = 0; pin < 3; pin++) {
      canvas_draw_line(c, x + 3 + pin * 2, y, x + 3 + pin * 2, y + 1);
      canvas_draw_line(c, x + 3 + pin * 2, y + 10, x + 3 + pin * 2, y + 11);
      canvas_draw_line(c, x, y + 3 + pin * 2, x + 1, y + 3 + pin * 2);
      canvas_draw_line(c, x + 10, y + 3 + pin * 2, x + 11, y + 3 + pin * 2);
    }
    break;
  case UiTarget:
    canvas_draw_rframe(c, x + 1, y, 10, 12, 2);
    canvas_draw_circle(c, x + 6, y + 6, 3);
    canvas_draw_dot(c, x + 6, y + 6);
    break;
  case UiWarning:
    canvas_draw_line(c, x, y + 10, x + 5, y);
    canvas_draw_line(c, x + 5, y, x + 10, y + 10);
    canvas_draw_line(c, x, y + 10, x + 10, y + 10);
    canvas_draw_line(c, x + 5, y + 4, x + 5, y + 6);
    canvas_draw_dot(c, x + 5, y + 8);
    break;
  }
}
