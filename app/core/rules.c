#include "rules.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

static int nibble(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  c = (char)tolower((unsigned char)c);
  return c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
}

bool rule_hex(const char *text, uint8_t *out, size_t capacity, size_t *length) {
  size_t n = strlen(text);
  if (n % 2 || n / 2 > capacity)
    return false;
  for (size_t i = 0; i < n; i++)
    if (nibble(text[i]) < 0)
      return false;
  for (size_t i = 0; i < n / 2; i++)
    out[i] = (uint8_t)((nibble(text[i * 2]) << 4) | nibble(text[i * 2 + 1]));
  *length = n / 2;
  return true;
}

void rule_to_hex(const uint8_t *data, size_t length, char *out) {
  static const char HEX[] = "0123456789ABCDEF";
  for (size_t i = 0; i < length; i++) {
    out[i * 2] = HEX[data[i] >> 4];
    out[i * 2 + 1] = HEX[data[i] & 15];
  }
  out[length * 2] = 0;
}

bool rule_parse(TrafficRule *rule, char **f, size_t count) {
  if (count != 6)
    return false;
  TrafficRule next = {0};
  uint32_t enabled, patch, offset;
  if (!wire_number(f[0], 1, &enabled) || strlen(f[1]) != 1 || !strchr("ARWN", f[1][0]) ||
      strlen(f[2]) > 36 || !wire_number(f[3], 1, &patch) || !wire_number(f[4], 516, &offset))
    return false;
  for (const char *p = f[2]; *p; p++)
    if (!isxdigit((unsigned char)*p) && *p != '-')
      return false;
  size_t length;
  if (!rule_hex(f[5], next.value, RULE_BYTES, &length))
    return false;
  next.enabled = enabled;
  next.operation = f[1][0];
  strcpy(next.uuid, f[2]);
  next.patch = patch;
  next.offset = (uint16_t)offset;
  next.length = (uint8_t)length;
  *rule = next;
  return true;
}

void rule_format(const TrafficRule *rule, char *out, size_t size) {
  char hex[RULE_BYTES * 2 + 1];
  rule_to_hex(rule->value, rule->length, hex);
  snprintf(out, size, "%u\t%c\t%s\t%u\t%u\t%s", rule->enabled,
           rule->operation ? rule->operation : 'A', rule->uuid, rule->patch, rule->offset, hex);
}

static bool same_uuid(const char *a, const char *b) {
  while (*a && *b) {
    if (tolower((unsigned char)*a++) != tolower((unsigned char)*b++))
      return false;
  }
  return *a == *b;
}

bool rule_apply(const TrafficRule *rules, char op, const char *uuid, uint8_t *data, size_t *length,
                size_t capacity) {
  for (size_t i = 0; i < RULE_COUNT; i++) {
    const TrafficRule *r = &rules[i];
    if (!r->enabled || (r->operation != 'A' && r->operation != op))
      continue;
    if (r->uuid[0] && !same_uuid(r->uuid, uuid))
      continue;
    if (r->patch) {
      if (r->offset > *length || r->length > *length - r->offset)
        return false;
      memcpy(data + r->offset, r->value, r->length);
    } else {
      if (r->length > capacity)
        return false;
      memcpy(data, r->value, r->length);
      *length = r->length;
    }
    return true;
  }
  return false;
}
