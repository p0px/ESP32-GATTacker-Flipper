"""Compile the real Flipper navigation and receive path with host I/O stubs."""

import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def stage_sources(destination: Path) -> None:
  """Stage production handlers with hardware declarations replaced for a host build."""
  header = (ROOT / "app/app.h").read_text(encoding="utf-8")
  lines = [line for line in header.splitlines() if not line.startswith("#include <")]
  stubs = (ROOT / "tests/ui_host_stubs.h").read_text(encoding="utf-8")
  (destination / "app.h").write_text(stubs + "\n" + "\n".join(lines), encoding="utf-8")
  sources = (
    "link/pair.c", "session/model.c", "session/session.c", "session/traffic.c",
    "ui/input.c", "ui/navigation.c", "wifi/wifi_flow.c", "wifi/wifi_networks.c",
  )
  for name in sources:
    target = destination / name
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(ROOT / "app" / name, target)
  # Keep the production receiver; exclude only the GUI keyboard below WifiEditor.
  receiver = (ROOT / "app/wifi/wifi.c").read_text(encoding="utf-8").split(
    "typedef struct {", 1)[0]
  receiver = "\n".join(line for line in receiver.splitlines()
                       if not line.startswith("#include <gui/"))
  (destination / "wifi/wifi_receive.c").write_text(receiver + "\n", encoding="utf-8")
  shutil.copytree(ROOT / "app/core", destination / "core")
  shutil.copy2(ROOT / "tests/ui_model_test.c", destination / "test.c")


@unittest.skipUnless(shutil.which("cc"), "requires a C compiler")
class UiModelTest(unittest.TestCase):
  """Exercise complete input/receive flows, including settings and session navigation."""

  def test_navigation(self) -> None:
    """Run production menu handlers under sanitizers with real protocol records."""
    with tempfile.TemporaryDirectory() as temporary:
      folder = Path(temporary)
      stage_sources(folder)
      sources = [
        "test.c", "link/pair.c", "session/model.c", "session/session.c", "session/traffic.c",
        "ui/input.c", "ui/navigation.c", "wifi/wifi_flow.c", "wifi/wifi_networks.c",
        "wifi/wifi_receive.c", "core/wire.c", "core/rules.c",
      ]
      binary = folder / "ui-tests"
      subprocess.run(["cc", "-std=c17", "-Wall", "-Wextra", "-Werror",
                      "-fsanitize=address,undefined", "-I.", *sources, "-o", str(binary)],
                     cwd=folder, check=True)
      subprocess.run([str(binary)], check=True)
