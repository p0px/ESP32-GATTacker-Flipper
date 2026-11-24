#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CAPTURE_H4_MAX 1029
size_t capture_base64(const uint8_t *data, size_t length, char *out, size_t capacity);
size_t capture_unbase64(const char *text, uint8_t *out, size_t capacity);
bool capture_u64(const char *text, uint64_t *value);

#ifdef __cplusplus
}
#endif
