"""Guard against wrong-chip writes and incomplete backup; no serial operations."""
import importlib.util
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location("flash", Path(__file__).resolve().parents[1] / "tools/flash.py")
flash = importlib.util.module_from_spec(spec)
spec.loader.exec_module(flash)


class GuardTests(unittest.TestCase):
    def test_identity(self):
        for chip, capacity in [("ESP32-S3", 4), ("ESP32-C3", 4), ("ESP32-D0WD-V3", 8), ("ESP32-S2", 4)]:
            self.assertFalse(flash.target_ok(f"Chip is {chip} (revision v3.1)\nDetected flash size: {capacity}MB\n"))
        self.assertTrue(flash.target_ok("Chip is ESP32-D0WD-V3 (revision v3.1)\nDetected flash size: 4MB\n"))

    def test_no_install_no_device_access(self):
        with patch.object(flash, "check_package"), patch("sys.argv", ["flash.py", "--port", "mock"]), patch.object(flash, "run") as run:
            with self.assertRaisesRegex(RuntimeError, "Add --install"):
                flash.main()
            run.assert_not_called()

    def test_wrong_target_never_reads_or_writes_flash(self):
        for chip in ("ESP32-S3", "ESP32-C3"):
            with patch.object(flash, "check_package"), patch("sys.argv", ["flash.py", "--port", "mock", "--install"]), patch.object(flash, "run", return_value=f"Chip is {chip}\nDetected flash size: 4MB\n") as run:
                with self.assertRaisesRegex(RuntimeError, "not a classic"):
                    flash.main()
                self.assertEqual(run.call_count, 1)
                self.assertIn("flash_id", run.call_args.args[0])

    def test_incomplete_backup_never_writes(self):
        with tempfile.TemporaryDirectory() as root, patch.object(flash, "ROOT", Path(root)), patch.object(flash, "check_package"), patch("sys.argv", ["flash.py", "--port", "mock", "--install"]), patch.object(flash, "run", return_value="Chip is ESP32-D0WD-V3\nDetected flash size: 4MB\n") as run:
            with self.assertRaisesRegex(RuntimeError, "Backup incomplete"):
                flash.main()
            self.assertEqual(run.call_count, 2)
            self.assertFalse(any("write_flash" in call.args[0] for call in run.call_args_list))

    def test_app_update_requires_matching_partitions(self):
        with tempfile.TemporaryDirectory() as root:
            package=Path(root)/"firmware/cyd-st7789";package.mkdir(parents=True)
            (package/"partitions.bin").write_bytes(b"expected-table")
            def fake_run(command,capture=False):
                if "flash_id" in command:return "Chip is ESP32-D0WD-V3\nDetected flash size: 4MB\n"
                if "read_flash" in command:Path(command[-1]).write_bytes(bytes(4096))
            with patch.object(flash,"ROOT",Path(root)),patch.object(flash,"check_package"),patch("sys.argv",["flash.py","--port","mock","--update-app"]),patch.object(flash,"run",side_effect=fake_run) as run:
                with self.assertRaisesRegex(RuntimeError,"Partition table differs"):flash.main()
                self.assertEqual(run.call_count,2)

    def test_app_update_writes_only_app(self):
        with tempfile.TemporaryDirectory() as root:
            package=Path(root)/"firmware/cyd-st7789";package.mkdir(parents=True)
            expected=b"expected-table";(package/"partitions.bin").write_bytes(expected)
            def fake_run(command,capture=False):
                if "flash_id" in command:return "Chip is ESP32-D0WD-V3\nDetected flash size: 4MB\n"
                if "read_flash" in command:
                    if command[-3]=="0x8000":Path(command[-1]).write_bytes(expected+bytes(4096-len(expected)))
                    else:Path(command[-1]).write_bytes(bytes(0x180000))
            with patch.object(flash,"ROOT",Path(root)),patch.object(flash,"check_package"),patch("sys.argv",["flash.py","--port","mock","--update-app"]),patch.object(flash,"run",side_effect=fake_run) as run:
                flash.main()
                writes=[c.args[0] for c in run.call_args_list if "write_flash" in c.args[0]]
                self.assertEqual(len(writes),1)
                self.assertEqual(writes[0][-2:], ["0x10000",str(package/"firmware.bin")])
                self.assertNotIn("0x190000",writes[0])


if __name__ == "__main__":
    unittest.main()
