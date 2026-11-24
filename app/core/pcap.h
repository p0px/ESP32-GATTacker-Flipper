#pragma once
#include "capture_codec.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Little-endian PCAPNG, one HCI H4-with-direction interface (LINKTYPE 201). */
#define PCAP_BUFFER_SIZE 1400
size_t pcap_header(uint8_t *out);
size_t pcap_header_clock(uint8_t *out, const char *clock);
size_t pcap_packet(uint8_t *out, uint64_t timestamp, bool received,
                   const uint8_t *h4, size_t length);
size_t pcap_statistics(uint8_t *out, uint64_t timestamp, uint32_t saved,
                       uint32_t dropped, bool complete, const char *detail);

#ifdef __cplusplus
}
#endif
