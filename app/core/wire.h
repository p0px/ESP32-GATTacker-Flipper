#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WIRE_MAX 1536
#define WIRE_FRAME_MAX ((WIRE_MAX + 6) * 2 + 2)
typedef enum { WireCommand = 1, WireReply = 2, WireEvent = 3 } WireKind;
typedef struct {
  uint8_t bytes[WIRE_MAX + 6];
  size_t used;
  bool escaped;
  bool overflow;
  uint32_t errors;
} WireParser;
typedef void (*WireReceive)(void *context, WireKind kind, uint16_t id, const char *text);

/* Delimited, escaped CRC16 frames recover after boot logs or incomplete packets. */
size_t wire_encode(WireKind kind, uint16_t id, const char *text, uint8_t *out, size_t size);
void wire_feed(WireParser *parser, uint8_t byte, WireReceive receive, void *context);
/* Split a mutable record, preserving empty fields. Returns zero on excess fields. */
size_t wire_fields(char *record, char **fields, size_t capacity);
bool wire_number(const char *text, uint32_t maximum, uint32_t *out);

#ifdef __cplusplus
}
#endif
