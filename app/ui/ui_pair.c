#include "ui.h"
#include <stdio.h>

void ui_pair(Canvas *canvas, const App *app) {
  char text[80];
  snprintf(text, sizeof(text), "Settings + MCP key\n%s\nAllow this browser?", app->pair_ip);
  ui_text(canvas, text, 0);
  ui_footer(canvas, "Deny", "Allow", 0, 0);
}
