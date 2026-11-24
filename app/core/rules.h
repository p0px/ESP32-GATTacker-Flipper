#pragma once
#include "wire.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RULE_COUNT 8
#define RULE_BYTES 32
typedef struct {
  bool enabled;
  char operation; /* A: any, R: read response, W: write, N: notification. */
  char uuid[37];  /* Empty matches any characteristic. */
  bool patch;
  uint16_t offset;
  uint8_t length;
  uint8_t value[RULE_BYTES];
} TrafficRule;

bool rule_parse(TrafficRule *rule, char **fields, size_t count);
void rule_format(const TrafficRule *rule, char *out, size_t size);
bool rule_apply(const TrafficRule *rules, char operation, const char *uuid, uint8_t *data,
                size_t *length, size_t capacity);
bool rule_hex(const char *text, uint8_t *out, size_t capacity, size_t *length);
void rule_to_hex(const uint8_t *data, size_t length, char *out);

#ifdef __cplusplus
}
#endif
