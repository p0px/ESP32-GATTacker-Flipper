"""Exercise the actual Flipper network picker with host sanitizers and no hardware UI."""

import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


@unittest.skipUnless(shutil.which("cc"), "requires a C compiler")
class WifiNetworksTest(unittest.TestCase):
  """Verify picker ordering, incomplete scans, malformed entries and credential transitions."""

  def test_picker(self) -> None:
    """Run production parser/selection code against packet loss and real selection scenarios."""
    with tempfile.TemporaryDirectory() as temporary:
      folder = Path(temporary)
      header = (ROOT / "app/app.h").read_text()
      header = "\n".join(line for line in header.splitlines()
                         if not line.startswith("#include <"))
      stubs = (ROOT / "tests/ui_host_stubs.h").read_text()
      (folder / "app.h").write_text(stubs + "\n" + header)
      for name in ("wifi/wifi_networks.c", "wifi/wifi_flow.c", "ui/navigation.c"):
        target = folder / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(ROOT / "app" / name, target)
      shutil.copytree(ROOT / "app/core", folder / "core")
      binary = folder / "test"
      command = ["cc", "-std=c17", "-Wall", "-Wextra", "-Werror",
                 "-fsanitize=address,undefined", "-I", str(folder), "-I", str(ROOT / "app"),
                 str(ROOT / "tests/wifi_networks_test.c"), str(folder / "wifi/wifi_networks.c"),
                 str(folder / "wifi/wifi_flow.c"),
                 str(folder / "ui/navigation.c"), str(folder / "core/wire.c"),
                 str(folder / "core/rules.c"), "-o", str(binary)]
      subprocess.run(command, check=True)
      subprocess.run([str(binary)], check=True)
