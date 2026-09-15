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

    def test_gameshark(self):
        """Test gameshark.pak which contains notes with binary gamecode/pubcode"""
        # Use golden file from a temporary copy
        test_path = self._use_golden_file("gameshark.pak")

        # First, test if we can see the files with the original broken file
        # It should only show 1 file because the second note has status = 0
        code, out, err = run_cpaktool(["list", "--json", "--crc", test_path], cwd=self.tempdir)
        self.assertEqual(code, 0, f"cpaktool list failed: {err}")
        
        data = json.loads(out)
        # With the broken file, we only see the first file
        self.assertEqual(len(data), 1, "Expected to find 1 file in broken gameshark.pak")
        
        # Verify the first file
        turok_file = next((f for f in data if f['filename'] == "TUROK LVL 8"), None)
        self.assertIsNotNone(turok_file, "TUROK LVL 8 file not found in output")
        self.assertEqual(turok_file['game_code'], "NTUE")
        self.assertEqual(turok_file['pub_code'], "51")

        # Test that fsck complains about the broken file
        code, out, err = run_cpaktool(["test", test_path], cwd=self.tempdir)
        self.assertNotEqual(code, 0)

        # Repair the file
        code, out, err = run_cpaktool(["test", "--repair", test_path], cwd=self.tempdir)
        self.assertEqual(code, 0)

        # Now test with the repaired file
        code, out, err = run_cpaktool(["list", "--json", "--crc", test_path], cwd=self.tempdir)
        self.assertEqual(code, 0, f"cpaktool list failed on gs.pak: {err}")
        
        data = json.loads(out)
        # After repair, we should see 3 files (1 ASCII + 2 hex gamecodes)
        self.assertEqual(len(data), 3, "Expected to find 3 files in gs.pak after repair")
        
        # Verify we can find the ASCII note
        turok_file = next((f for f in data if f['filename'] == "TUROK LVL 8"), None)
        self.assertIsNotNone(turok_file, "TUROK LVL 8 file not found in output")
        self.assertEqual(turok_file['game_code'], "NTUE")
        self.assertEqual(turok_file['pub_code'], "51")
        
        # Verify we can find the hex gamecode notes
        hex_files = [f for f in data if f['game_code'] == "3BADD1E5"]
        self.assertEqual(len(hex_files), 2, "Expected to find 2 files with hex gamecode")
        
        for hex_file in hex_files:
            self.assertEqual(hex_file['pub_code'], "FADE")
            self.assertIn(hex_file['filename'], ["SMSM", "ZLZL"])
            self.assertEqual(hex_file['extension'], "1")
            
        # All files should have valid CRC
        for file_entry in data:
            self.assertIsNotNone(file_entry['crc32'], f"File {file_entry['full_name']} should have valid CRC32")

    def test_bh_sram_nul_characters(self):
        """Test bh-sram.n64 which contains files with embedded NUL characters in extensions"""
        # Use golden file from a temporary copy
        test_path = self._use_golden_file("bh-sram.n64")

        # Test with JSON output to verify NUL characters are properly escaped
        code, out, err = run_cpaktool(["list", "--json", test_path], cwd=self.tempdir)
        self.assertEqual(code, 0, f"cpaktool list failed: {err}")
        
        data = json.loads(out)
        self.assertEqual(len(data), 8, "Expected to find 8 files in bh-sram.n64")
        
        # Find files with NUL characters in extensions (should be escaped as \u0000 in JSON)
        nul_files = [f for f in data if "\u0000" in f['extension']]
        self.assertEqual(len(nul_files), 2, "Expected to find 2 files with NUL characters in extensions")
        
        # Verify specific files with NUL characters
        mi_file = next((f for f in data if f['filename'] == "MI"), None)
        wave_race_file = next((f for f in data if f['filename'] == "WAVE RACE 64"), None)
        
        self.assertIsNotNone(mi_file, "MI file not found")
        self.assertIsNotNone(wave_race_file, "WAVE RACE 64 file not found")
        
        # These files should have \u0000RAM as extension (NUL character + RAM)
        self.assertEqual(mi_file['extension'], "\u0000RAM", "MI file should have \\u0000RAM extension")
        self.assertEqual(wave_race_file['extension'], "\u0000RAM", "WAVE RACE 64 file should have \\u0000RAM extension")
        
        # Test with table format to verify NUL characters are displayed as <NUL>
        code, out, err = run_cpaktool(["list", test_path], cwd=self.tempdir)
        self.assertEqual(code, 0, f"cpaktool list failed: {err}")
        
        # In table format, NUL characters should be displayed as <NUL>
        self.assertIn("<NUL>RAM", out, "Table format should show <NUL>RAM for files with embedded NUL characters")
        
        # Count occurrences of <NUL>RAM in table output
        nul_count = out.count("<NUL>RAM")
        self.assertEqual(nul_count, 2, "Should find exactly 2 occurrences of <NUL>RAM in table output")

    def test_res_unused_add_file(self):
        # Use golden file from a temporary copy
        test_path = self._use_golden_file("res_unused.mpk")

        # Create a small source file to add
        src_file = os.path.join(self.tempdir, "foo.bin")
        with open(src_file, "wb") as f:
            f.write(b"hello world")

        # Try to add the file to the pak (this used to fail on partially corrupted FAT reserved entries)
        code, out, err = run_cpaktool(["add", test_path, src_file], cwd=self.tempdir)
        self.assertEqual(code, 0, f"cpaktool add failed: stdout=\n{out}\nstderr=\n{err}")

        # Verify the new file is present (filenames are uppercased on CPak)
        code, out, err = run_cpaktool(["list", "--json", test_path], cwd=self.tempdir)
        self.assertEqual(code, 0, f"cpaktool list failed: {err}")
        data = json.loads(out)
        found = any(f["game_code"] == "DRAG" and f["pub_code"] == "ON" and f["filename"] == "FOO" and f["extension"] == "BIN" for f in data)
        self.assertTrue(found, "Newly added file not found in pak")

if __name__ == '__main__':
    unittest.main()
