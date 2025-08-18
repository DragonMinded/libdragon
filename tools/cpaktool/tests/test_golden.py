import unittest
import subprocess
import os
import tempfile
import shutil
from pathlib import Path
import json

def run_cpaktool(args, cwd=None):
    # cpaktool executable expected next to this tests dir: tools/cpaktool/cpaktool
    exe = Path(__file__).resolve().parents[1] / "cpaktool"
    if not exe.exists():
        raise RuntimeError(f"cpaktool executable not found at {exe}. Build it before running tests.")
    cmd = [str(exe)] + args
    proc = subprocess.run(cmd, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    return proc.returncode, proc.stdout, proc.stderr

class TestGolden(unittest.TestCase):
    def _use_golden_file(self, name):
        golden_path = os.path.join(os.path.dirname(__file__), "golden", name)
        test_path = os.path.join(self.tempdir, name)
        shutil.copy(golden_path, test_path)
        return test_path

    def setUp(self):
        self.tempdir = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, self.tempdir)

    def test_forsaken_64(self):
        # Use golden file from a temporary copy
        test_path = self._use_golden_file("forsaken_64.n64")

        # List files with JSON output to check for corruption
        code, out, err = run_cpaktool(["list", "--json", "--crc", test_path], cwd=self.tempdir)
        self.assertEqual(code, 0)
        data = json.loads(out)
        self.assertEqual(len(data), 2, "Expected to find 2 files")
        
        # Find the entries for both files
        nomercy_file = next((f for f in data if f['filename'] == "NO MERCY"), None)
        forsaken_file = next((f for f in data if f['filename'] == "FORSAKEN"), None)

        self.assertIsNotNone(nomercy_file, "File 'NO MERCY' not found in output")
        self.assertIsNotNone(forsaken_file, "File 'FORSAKEN' not found in output")

        # Check that NO MERCY is corrupted (crc32 is null) and FORSAKEN is not
        self.assertIsNone(nomercy_file['crc32'], "NO MERCY file should have a null CRC32")
        self.assertIsNotNone(forsaken_file['crc32'], "FORSAKEN file should have a valid CRC32")

        # Test the file for errors
        code, out, err = run_cpaktool(["test", test_path], cwd=self.tempdir)
        self.assertNotEqual(code, 0)
        self.assertIn("Found 1 issue", out)

        # Repair the file
        code, out, err = run_cpaktool(["test", "--repair", test_path], cwd=self.tempdir)
        self.assertEqual(code, 0)
        self.assertIn("Fixed 1 issue", out)

        # List files again to verify the fix
        code, out, err = run_cpaktool(["list", "--json", test_path], cwd=self.tempdir)
        self.assertEqual(code, 0)
        
        data = json.loads(out)
        self.assertEqual(len(data), 1)
        self.assertEqual(data[0]['filename'], "FORSAKEN")

if __name__ == '__main__':
    unittest.main()
