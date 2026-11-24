#include "pcap.h"
#include <stdio.h>
#include <string.h>

static void le16(uint8_t *out, uint16_t n) {
  out[0] = n;
  out[1] = n >> 8;
}

static void le32(uint8_t *out, uint32_t n) {
  for (unsigned i = 0; i < 4; i++) out[i] = n >> (i * 8);
}

static void le64(uint8_t *out, uint64_t n) {
  le32(out, (uint32_t)n);
  le32(out + 4, (uint32_t)(n >> 32));
}

static size_t block(uint8_t *out, uint32_t type, size_t length) {
  le32(out, type);
  le32(out + 4, length);
  le32(out + length - 4, length);
  return length;
}

static size_t option(uint8_t *out, uint16_t type, const void *data, size_t length) {
  size_t padded = (length + 3) & ~(size_t)3;
  le16(out, type);
  le16(out + 2, length);
  memset(out + 4, 0, padded);
  memcpy(out + 4, data, length);
  return padded + 4;
}

size_t pcap_header(uint8_t *out) {
  return pcap_header_clock(out, "Flipper RTC (1 s accuracy)");
}

size_t pcap_header_clock(uint8_t *out, const char *clock) {
  memset(out, 0, PCAP_BUFFER_SIZE);
  le32(out + 8, 0x1a2b3c4d);
  le16(out + 12, 1);
  le64(out + 16, UINT64_MAX);
  size_t used = block(out, 0x0a0d0d0a, 28);
  uint8_t *idb = out + used;
  le16(idb + 8, 201);
  le32(idb + 12, CAPTURE_H4_MAX + 4);
  const char name[] = "ESP32 VHCI";
  char description[320];
  snprintf(description, sizeof(description), "Host/controller HCI; no RF headers. "
      "Start before connecting for address and GATT discovery context. "
      "Clock anchored to %s; ESP timestamps in us.", clock);
  size_t end = 16;
  end += option(idb + end, 2, name, sizeof(name) - 1);
  end += option(idb + end, 3, description, strlen(description));
  return used + block(idb, 1, end + 8);
}

static bool valid_h4(const uint8_t *h4, size_t length) {
  if (!length || length > CAPTURE_H4_MAX) return false;
  switch (h4[0]) {
  case 1: return length >= 4 && length == (size_t)h4[3] + 4;
  case 2: return length >= 5 && length == 5 + (size_t)h4[3] + ((size_t)h4[4] << 8);
  case 4: return length >= 3 && length == (size_t)h4[2] + 3;
  default: return false;
  }
}

size_t pcap_packet(uint8_t *out, uint64_t timestamp, bool received,
                   const uint8_t *h4, size_t length) {
  if (!valid_h4(h4, length)) return 0;
  size_t payload = length + 4, padded = (payload + 3) & ~(size_t)3;
  memset(out, 0, 32 + padded);
  le32(out + 12, timestamp >> 32);
  le32(out + 16, timestamp);
  le32(out + 20, payload);
  le32(out + 24, payload);
  /* Wireshark/libpcap convention: big-endian 0 sent, 1 received. */
  out[31] = received ? 1 : 0;
  memcpy(out + 32, h4, length);
  return block(out, 6, 32 + padded);
}

size_t pcap_statistics(uint8_t *out, uint64_t timestamp, uint32_t saved,
                       uint32_t dropped, bool complete, const char *detail) {
  memset(out, 0, PCAP_BUFFER_SIZE);
  le32(out + 12, timestamp >> 32);
  le32(out + 16, timestamp);
  size_t used = 20;
  char comment[400];
  snprintf(comment, sizeof(comment), "%s %s",
           complete ? "Capture drained; loss counts are final."
                    : "Interrupted capture; final loss count unknown (lower bounds below).",
           detail ? detail : "Loss breakdown unavailable.");
  used += option(out + used, 1, comment, strlen(comment));
  uint8_t number[8];
  le64(number, (uint64_t)saved + dropped);
  used += option(out + used, 4, number, sizeof(number));
  le64(number, dropped);
  used += option(out + used, 5, number, sizeof(number));
  return block(out, 5, used + 8);
}
