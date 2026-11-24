#include "wire.h"
#include <string.h>

static uint16_t crc_byte(uint16_t crc, uint8_t byte) {
  crc ^= (uint16_t)byte << 8;
  for (unsigned i = 0; i < 8; i++)
    crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
  return crc;
}

static size_t escape(uint8_t byte, uint8_t *out) {
  if (byte == 0x7e || byte == 0x7d) {
    out[0] = 0x7d;
    out[1] = byte ^ 0x20;
    return 2;
  }
  out[0] = byte;
  return 1;
}

size_t wire_encode(WireKind kind, uint16_t id, const char *text, uint8_t *out, size_t size) {
  size_t length = strlen(text);
  if (length >= WIRE_MAX || size < (length + 6) * 2 + 2)
    return 0;
  uint8_t header[] = {1, (uint8_t)kind, (uint8_t)id, (uint8_t)(id >> 8)};
  uint16_t crc = 0xffff;
  size_t used = 0;
  out[used++] = 0x7e;
  for (size_t i = 0; i < length + sizeof(header); i++) {
    uint8_t byte = i < sizeof(header) ? header[i] : (uint8_t)text[i - sizeof(header)];
    crc = crc_byte(crc, byte);
    used += escape(byte, out + used);
  }
  used += escape((uint8_t)crc, out + used);
  used += escape((uint8_t)(crc >> 8), out + used);
  out[used++] = 0x7e;
  return used;
}

static void finish(WireParser *p, WireReceive receive, void *context) {
  if (!p->used && !p->overflow)
    return;
  if (p->overflow || p->escaped || p->used < 6 || p->bytes[0] != 1) {
    p->errors++;
    return;
  }
  size_t end = p->used - 2;
  uint16_t crc = 0xffff;
  for (size_t i = 0; i < end; i++)
    crc = crc_byte(crc, p->bytes[i]);
  uint16_t expected = p->bytes[end] | ((uint16_t)p->bytes[end + 1] << 8);
  if (crc != expected || memchr(p->bytes + 4, 0, end - 4) || p->bytes[1] < WireCommand ||
      p->bytes[1] > WireEvent) {
    p->errors++;
    return;
  }
  p->bytes[end] = 0;
  uint16_t id = p->bytes[2] | ((uint16_t)p->bytes[3] << 8);
  receive(context, (WireKind)p->bytes[1], id, (char *)p->bytes + 4);
}

void wire_feed(WireParser *p, uint8_t byte, WireReceive receive, void *context) {
  if (byte == 0x7e) {
    finish(p, receive, context);
    p->used = 0;
    p->escaped = p->overflow = false;
    return;
  }
  if (p->overflow)
    return;
  if (!p->escaped && byte == 0x7d) {
    p->escaped = true;
    return;
  }
  if (p->escaped)
    byte ^= 0x20;
  p->escaped = false;
  if (p->used >= sizeof(p->bytes) - 1)
    p->overflow = true;
  else
    p->bytes[p->used++] = byte;
}

size_t wire_fields(char *record, char **fields, size_t capacity) {
  if (!capacity)
    return 0;
  size_t count = 1;
  fields[0] = record;
  for (char *p = record; *p; p++) {
    if (*p != '\t')
      continue;
    if (count == capacity)
      return 0;
    *p = 0;
    fields[count++] = p + 1;
  }
  return count;
}

bool wire_number(const char *text, uint32_t maximum, uint32_t *out) {
  uint32_t value = 0;
  if (!text || !*text)
    return false;
  for (; *text; text++) {
    if (*text < '0' || *text > '9')
      return false;
    uint32_t digit = (uint32_t)(*text - '0');
    if (digit > maximum || value > (maximum - digit) / 10)
      return false;
    value = value * 10 + digit;
  }
  *out = value;
  return true;
}
