"""Validate the production PCAPNG writer and capture codec without Bluetooth hardware."""

import base64
import ctypes
import shutil
import struct
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BUFFER_SIZE = 1400


@unittest.skipUnless(shutil.which("gcc"), "requires gcc")
class PcapTest(unittest.TestCase):
  """Exercise the real C serializer and codec, with Wireshark for decoded packets."""

  @classmethod
  def setUpClass(cls) -> None:
    """Build the maintained C source as a shared library in a temporary directory."""
    cls.directory = tempfile.TemporaryDirectory()
    cls.addClassCleanup(cls.directory.cleanup)
    cls.folder = Path(cls.directory.name)
    library = cls.folder / "capture.so"
    subprocess.run([
      "gcc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-shared", "-fPIC",
      str(ROOT / "app/core/pcap.c"), str(ROOT / "app/core/capture_codec.c"), "-o", str(library),
    ], check=True, capture_output=True)
    cls.lib = ctypes.CDLL(str(library))
    pointer = ctypes.c_void_p
    size = ctypes.c_size_t
    cls.lib.pcap_header.argtypes = [pointer]
    cls.lib.pcap_header.restype = size
    cls.lib.pcap_packet.argtypes = [pointer, ctypes.c_uint64, ctypes.c_bool, pointer, size]
    cls.lib.pcap_packet.restype = size
    cls.lib.pcap_statistics.argtypes = [
      pointer, ctypes.c_uint64, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_bool,
      ctypes.c_char_p,
    ]
    cls.lib.pcap_statistics.restype = size
    cls.lib.capture_base64.argtypes = [pointer, size, pointer, size]
    cls.lib.capture_base64.restype = size
    cls.lib.capture_unbase64.argtypes = [ctypes.c_char_p, pointer, size]
    cls.lib.capture_unbase64.restype = size
    cls.lib.capture_u64.argtypes = [ctypes.c_char_p, ctypes.POINTER(ctypes.c_uint64)]
    cls.lib.capture_u64.restype = ctypes.c_bool

  @unittest.skipUnless(shutil.which("tshark") and shutil.which("capinfos"),
                       "requires tshark and capinfos")
  def test_wireshark_decodes_att_and_direction(self) -> None:
    """Require actual ATT decoding, both HCI directions, exact bytes, and microsecond timing."""
    out = ctypes.create_string_buffer(BUFFER_SIZE)
    size = self.lib.pcap_header(out)
    data = out.raw[:size]
    start = 1794390000123456
    # A read request for attribute 0x0025 and its one-byte response, connection handle 1.
    packets = [bytes.fromhex("02 01 20 07 00 03 00 04 00 0a 25 00"),
               bytes.fromhex("02 01 20 06 00 02 00 04 00 0b 42")]
    for index, h4 in enumerate(packets):
      size = self.lib.pcap_packet(out, start + index * 65000, bool(index), h4, len(h4))
      self.assertGreater(size, 0)
      data += out.raw[:size]
    size = self.lib.pcap_statistics(out, start + 65000, 2, 3, True, b"ESP=2; link=1")
    data += out.raw[:size]
    capture = self.folder / "att.pcapng"
    capture.write_bytes(data)
    result = subprocess.run([
      "tshark", "-n", "-r", str(capture), "-T", "fields",
      "-e", "btatt.opcode", "-e", "btatt.handle", "-e", "hci_h4.direction",
      "-e", "bthci_acl.chandle", "-e", "frame.time_epoch", "-e", "_ws.malformed",
    ], capture_output=True, text=True)
    self.assertEqual(result.returncode, 0, result.stderr)
    rows = [line.split("\t") for line in result.stdout.splitlines()]
    self.assertEqual(len(rows), 2)
    self.assertEqual(rows[0][:4], ["0x0a", "0x0025", "0x00", "0x0001"])
    self.assertEqual(rows[1][0], "0x0b")
    self.assertEqual(rows[1][2:4], ["0x01", "0x0001"])
    self.assertEqual(rows[0][4], "1794390000.123456000")
    self.assertEqual(rows[1][4], "1794390000.188456000")
    self.assertEqual([row[5] for row in rows], ["", ""])
    info = subprocess.run(["capinfos", "-I", str(capture)], check=True,
                          capture_output=True, text=True).stdout
    self.assertIn("Number of packets = 2", info)
    self.assertIn("Number of stat entries = 1", info)
    # capinfos confirms the ISB is readable but does not print its counters.
    stats_size = struct.unpack_from("<I", data, len(data) - 4)[0]
    offset = len(data) - stats_size + 20
    options: dict[int, bytes] = {}
    while offset < len(data) - 4:
      code, length = struct.unpack_from("<HH", data, offset)
      if code == 0:
        break
      options[code] = data[offset + 4:offset + 4 + length]
      offset += 4 + (length + 3) // 4 * 4
    self.assertEqual(struct.unpack("<Q", options[5])[0], 3)
    self.assertIn(b"Capture drained", options[1])
    self.assertIn(b"ESP=2; link=1", options[1])

  def test_blocks_are_aligned_and_preserve_maximum_packet(self) -> None:
    """Check all blocks, including a full 1024-byte ACL payload, without truncation."""
    out = ctypes.create_string_buffer(BUFFER_SIZE)
    h4 = bytes.fromhex("02 01 20 00 04") + bytes(range(256)) * 4
    size = self.lib.pcap_packet(out, 0xffffffffffffffff, True, h4, len(h4))
    self.assertEqual(size % 4, 0)
    self.assertEqual(struct.unpack_from("<I", out.raw, 4)[0], size)
    self.assertEqual(struct.unpack_from("<I", out.raw, size - 4)[0], size)
    self.assertEqual(struct.unpack_from("<II", out.raw, 20), (1033, 1033))
    self.assertEqual(out.raw[28:32], b"\0\0\0\1")
    self.assertEqual(out.raw[32:32 + len(h4)], h4)
    self.assertEqual(self.lib.pcap_packet(out, 0, False, h4, len(h4) - 1), 0)
    self.assertEqual(self.lib.pcap_packet(out, 0, False, b"", 0), 0)

  def test_base64_matches_standard_library_and_rejects_bad_records(self) -> None:
    """Test padding, full UART-sized payloads, short output buffers, and malformed input."""
    out = ctypes.create_string_buffer(BUFFER_SIZE)
    for length in (1, 2, 3, 255, 256, 257, 1029):
      payload = bytes(i % 256 for i in range(length))
      expected = base64.b64encode(payload)
      self.assertEqual(self.lib.capture_base64(payload, length, out, BUFFER_SIZE), len(expected))
      self.assertEqual(out.value, expected)
      self.assertEqual(self.lib.capture_unbase64(expected, out, BUFFER_SIZE), length)
      self.assertEqual(out.raw[:length], payload)
      self.assertEqual(self.lib.capture_unbase64(expected, out, length - 1), 0)
    for invalid in (b"", b"A", b"AA=A", b"AA==AAAA", b"AB==", b"AAB=", b"!!!!"):
      self.assertEqual(self.lib.capture_unbase64(invalid, out, BUFFER_SIZE), 0, invalid)
    value = ctypes.c_uint64()
    self.assertTrue(self.lib.capture_u64(b"18446744073709551615", ctypes.byref(value)))
    self.assertEqual(value.value, 2**64 - 1)
    for invalid in (b"", b"-1", b"1x", b"18446744073709551616"):
      self.assertFalse(self.lib.capture_u64(invalid, ctypes.byref(value)))


if __name__ == "__main__":
  unittest.main()
