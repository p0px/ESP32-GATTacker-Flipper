#include "rules.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
  unsigned count;
  uint16_t id;
  char text[WIRE_MAX];
} Received;

static void receive(void *context, WireKind kind, uint16_t id, const char *text) {
  Received *received = context;
  assert(kind == WireCommand);
  received->count++;
  received->id = id;
  strcpy(received->text, text);
}

static void frame_tests(void) {
  WireParser parser = {0};
  Received received = {0};
  uint8_t frame[WIRE_FRAME_MAX];
  const char *message = "rule_set\t1\t0\t1\tA\t\t0\t0\t7D7E~}";
  size_t length = wire_encode(WireCommand, 0x7e7d, message, frame, sizeof(frame));
  assert(length);
  for (size_t i = 0; i < length; i++)
    wire_feed(&parser, frame[i], receive, &received);
  assert(received.count == 1 && received.id == 0x7e7d && !strcmp(received.text, message));
  frame[6] ^= 1;
  for (size_t i = 0; i < length; i++)
    wire_feed(&parser, frame[i], receive, &received);
  assert(received.count == 1 && parser.errors == 1);
  for (unsigned i = 0; i < WIRE_MAX * 3; i++)
    wire_feed(&parser, 'x', receive, &received);
  length = wire_encode(WireCommand, 2, "stop", frame, sizeof(frame));
  for (size_t i = 0; i < length; i++)
    wire_feed(&parser, frame[i], receive, &received);
  assert(received.count == 2 && !strcmp(received.text, "stop"));
  assert(!wire_encode(WireCommand, 1, "test", frame, 2));
  uint32_t number = 22;
  assert(!wire_number("4294967296", UINT32_MAX, &number) && number == 22);
  assert(!wire_number("-1", UINT32_MAX, &number));
  assert(wire_number("4294967295", UINT32_MAX, &number) && number == UINT32_MAX);
}

static void rule_tests(void) {
  TrafficRule rules[RULE_COUNT] = {0};
  char text[] = "1\tN\tABCD\t1\t1\t00FF";
  char *fields[6];
  assert(wire_fields(text, fields, 6) == 6);
  assert(rule_parse(&rules[0], fields, 6));
  uint8_t data[10] = {1, 2, 3, 4};
  size_t length = 4;
  assert(!rule_apply(rules, 'W', "abcd", data, &length, sizeof(data)));
  assert(data[1] == 2);
  assert(rule_apply(rules, 'N', "abcd", data, &length, sizeof(data)));
  assert(length == 4 && data[0] == 1 && data[1] == 0 && data[2] == 255 && data[3] == 4);
  rules[0].offset = 4;
  assert(!rule_apply(rules, 'N', "ABCD", data, &length, sizeof(data)));
  rules[0].patch = false;
  assert(rule_apply(rules, 'N', "ABCD", data, &length, sizeof(data)) && length == 2);
  rules[0].length = 0;
  assert(rule_apply(rules, 'N', "ABCD", data, &length, sizeof(data)) && length == 0);
  uint8_t output[4] = {9};
  length = 99;
  assert(!rule_hex("0Z", output, 4, &length) && output[0] == 9 && length == 99);
  assert(!rule_hex("ABC", output, 4, &length));
  fields[5] = "GG";
  TrafficRule saved = rules[0];
  assert(!rule_parse(&rules[0], fields, 6));
  assert(!memcmp(&saved, &rules[0], sizeof(saved)));
}

int main(void) {
  frame_tests();
  rule_tests();
  puts("Framing, recovery, validation, matching, and payload tests passed");
  return 0;
}
