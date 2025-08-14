#!/usr/bin/env python3
import subprocess
import tempfile
import unittest
from pathlib import Path

# Helper to run cpaktool and capture output

def run_cpaktool(args, cwd=None):
    # cpaktool executable expected next to this tests dir: tools/cpaktool/cpaktool
    exe = Path(__file__).resolve().parents[1] / "cpaktool"
    if not exe.exists():
        raise RuntimeError(f"cpaktool executable not found at {exe}. Build it before running tests.")
    cmd = [str(exe)] + args
    proc = subprocess.run(cmd, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    return proc.returncode, proc.stdout, proc.stderr

class TestCommands(unittest.TestCase):
    """Test the semantic functionality of cpaktool commands"""
    
    def setUp(self):
        self.tmpdir = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmpdir.cleanup)
        self.tmp = Path(self.tmpdir.name)

    def test_format_and_fsck_various_sizes(self):
        """Test format and immediate fsck of various pak sizes"""
        test_cases = [
            ("32KB", 32 * 1024), ("128KB", 128 * 1024),  # KB format
            ("1", 32 * 1024), ("4", 128 * 1024)          # banks format
        ]
        
        for size_spec, expected_size in test_cases:
            with self.subTest(size=size_spec):
                pak = self.tmp / f"test_{size_spec}.pak"
                
                # Create the pak file
                if size_spec.endswith("KB"):
                    code, out, err = run_cpaktool(["format", "--size", size_spec[:-2], str(pak)])
                else:  # banks
                    code, out, err = run_cpaktool(["format", "--banks", size_spec, str(pak)])
                
                self.assertEqual(code, 0, f"Failed to format {size_spec}: {err}")
                self.assertEqual(pak.stat().st_size, expected_size)
                
                # Verify with fsck
                code, out, err = run_cpaktool(["test", str(pak)])
                self.assertEqual(code, 0, f"fsck failed on {size_spec}: {err}")
                self.assertIn("No issues found", out)

    def _create_pak(self, size="64KB"):
        """Helper to create a test pak file"""
        pak = self.tmp / f"test_{size.replace('KB', 'kb')}.pak"
        if size.endswith("KB"):
            code, out, err = run_cpaktool(["format", "--size", size[:-2], str(pak)])
        else:
            code, out, err = run_cpaktool(["format", "--banks", size, str(pak)])
        self.assertEqual(code, 0, f"Failed to format {size}: {err}")
        return pak

    def _corrupt_file_at_offset(self, pak_path, offset, corruption_data):
        """Helper to corrupt a pak file at a specific offset"""
        with open(pak_path, "r+b") as f:
            f.seek(offset)
            f.write(bytes([corruption_data]) if isinstance(corruption_data, int) else corruption_data)

    def test_fsck_handles_single_corruption(self):
        """Test that fsck handles single point corruption gracefully"""
        pak = self._create_pak()
        
        # Corrupt first ID sector checksum (other copies remain valid)
        self._corrupt_file_at_offset(pak, 0x20 + 0x1C, 0xFF)
        
        code, out, err = run_cpaktool(["test", str(pak)])
        self.assertEqual(code, 0, "fsck should succeed when backup copies are valid")

    def test_fsck_handles_severe_corruption_and_repair(self):
        """Test fsck with severe corruption and repair functionality"""
        pak = self._create_pak("128KB")
        
        # Corrupt ALL ID sectors and FAT copies
        for sector_offset in [0x20, 0x60, 0x80, 0xC0]:
            self._corrupt_file_at_offset(pak, sector_offset + 0x1C, 0xFF)
        
        self._corrupt_file_at_offset(pak, 0x100 + 1, 0xBB)
        fat_size = 128 * 1024 // 8
        self._corrupt_file_at_offset(pak, 0x100 + fat_size + 1, 0xBB)
        
        # Should still succeed by regenerating data
        code, out, err = run_cpaktool(["test", str(pak)])
        self.assertEqual(code, 0, "fsck should regenerate corrupted data")
        
        # Test repair option
        code, out, err = run_cpaktool(["test", "--repair", str(pak)])
        self.assertEqual(code, 0, "repair should succeed")
        
        code, out, err = run_cpaktool(["test", str(pak)])
        self.assertEqual(code, 0, "fsck should pass after repair")
        self.assertIn("No issues found", out)

    def test_fsck_invalid_files(self):
        """Test fsck behavior on various invalid files"""
        # Non-existent file
        code, out, err = run_cpaktool(["test", str(self.tmp / "nonexistent.pak")])
        self.assertNotEqual(code, 0)
        self.assertIn("File not found", err)
        
        # Text file
        text_file = self.tmp / "text.pak"
        text_file.write_text("Not a pak file")
        code, out, err = run_cpaktool(["test", str(text_file)])
        self.assertNotEqual(code, 0)
        
        # Too small file
        small_file = self.tmp / "small.pak"
        small_file.write_bytes(b"x" * 100)
        code, out, err = run_cpaktool(["test", str(small_file)])
        self.assertNotEqual(code, 0)

    def test_format_file_handling(self):
        """Test format command file creation and overwrite behavior"""
        pak = self.tmp / "test.pak"
        
        # Create new file
        code, out, err = run_cpaktool(["format", "--size", "64", str(pak)])
        self.assertEqual(code, 0)
        self.assertEqual(pak.stat().st_size, 64 * 1024)
        
        # Try overwrite without --force (should fail)
        code, out, err = run_cpaktool(["format", "--size", "32", str(pak)])
        self.assertNotEqual(code, 0)
        self.assertIn("File exists", err)
        self.assertIn("--force", err)
        
        # Overwrite with --force
        code, out, err = run_cpaktool(["format", "--force", "--size", "32", str(pak)])
        self.assertEqual(code, 0)
        self.assertEqual(pak.stat().st_size, 32 * 1024)
        
        # Verify new file is valid
        code, out, err = run_cpaktool(["test", str(pak)])
        self.assertEqual(code, 0)

if __name__ == "__main__":
    unittest.main()
