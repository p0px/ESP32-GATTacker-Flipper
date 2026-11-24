#include "capture_codec.h"
#include <string.h>

static const char ALPHABET[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

size_t capture_base64(const uint8_t *data, size_t length, char *out, size_t capacity) {
  size_t needed = ((length + 2) / 3) * 4;
  if (needed >= capacity) return 0;
  size_t used = 0;
  for (size_t i = 0; i < length; i += 3) {
    uint32_t bits = (uint32_t)data[i] << 16;
    if (i + 1 < length) bits |= (uint32_t)data[i + 1] << 8;
    if (i + 2 < length) bits |= data[i + 2];
    out[used++] = ALPHABET[bits >> 18];
    out[used++] = ALPHABET[(bits >> 12) & 63];
    out[used++] = i + 1 < length ? ALPHABET[(bits >> 6) & 63] : '=';
    out[used++] = i + 2 < length ? ALPHABET[bits & 63] : '=';
  }
  out[used] = 0;
  return used;
}

size_t capture_unbase64(const char *text, uint8_t *out, size_t capacity) {
  size_t length = strlen(text), used = 0;
  if (!length || length % 4) return 0;
  for (size_t i = 0; i < length; i += 4) {
    uint32_t bits = 0;
    unsigned padding = 0;
    for (unsigned j = 0; j < 4; j++) {
      char c = text[i + j];
      bits <<= 6;
      if (c == '=') {
        if (j < 2 || i + 4 != length) return 0;
        padding++;
      } else {
        const char *p = strchr(ALPHABET, c);
        if (!p || padding) return 0;
        bits |= (uint32_t)(p - ALPHABET);
      }
    }
    if (used + 3 - padding > capacity) return 0;
    if ((padding == 1 && (bits & 0xff)) || (padding == 2 && (bits & 0xffff))) return 0;
    out[used++] = bits >> 16;
    if (padding < 2) out[used++] = bits >> 8;
    if (!padding) out[used++] = bits;
  }
  return used;
}

bool capture_u64(const char *text, uint64_t *value) {
  uint64_t result = 0;
  if (!*text) return false;
  for (; *text; text++) {
    if (*text < '0' || *text > '9') return false;
    unsigned digit = *text - '0';
    if (result > (UINT64_MAX - digit) / 10) return false;
    result = result * 10 + digit;
  }
  *value = result;
  return true;
}
