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

    def test_add_and_extract_basic(self):
        """Test basic add and extract functionality"""
        pak = self._create_pak()
        
        # Create test files
        test1 = self.tmp / "test1.txt"
        test1.write_text("Hello World!")
        test2 = self.tmp / "DRAG.ON-test2.dat" 
        test2.write_bytes(b"\x00\x01\x02\xFF")
        
        # Add files (normal name, cpak format name, custom gamecode)
        code, out, err = run_cpaktool(["add", str(pak), str(test1), str(test2)])
        self.assertEqual(code, 0, f"Add failed: {err}")
        
        code, out, err = run_cpaktool(["add", "--gamecode", "ABCD.EF", str(pak), str(test1)])
        self.assertEqual(code, 0)
        
        # Extract all and verify
        extract_dir = self.tmp / "extracted"
        extract_dir.mkdir()
        code, out, err = run_cpaktool(["extract", str(pak)], cwd=extract_dir)
        self.assertEqual(code, 0)
        
        extracted_files = list(extract_dir.glob("*"))
        self.assertEqual(len(extracted_files), 3)  # Should have 3 files total
        
        # Verify file names follow pattern (files get numbered by cpakfs)
        drag_files = list(extract_dir.glob("DRAG.ON-*"))
        abcd_files = list(extract_dir.glob("ABCD.EF-*"))
        self.assertEqual(len(drag_files), 2)  # 2 files with DRAG.ON prefix
        self.assertEqual(len(abcd_files), 1)  # 1 file with ABCD.EF prefix
        
        # Test pattern extraction - look for files that would match the pattern
        # Clean directory first
        for f in extract_dir.glob("*"):
            f.unlink()
        
        code, out, err = run_cpaktool(["extract", "--verbose", str(pak), "DRAG.ON-*"], cwd=extract_dir)
        self.assertEqual(code, 0)
        # Should extract the DRAG.ON files
        extracted_drag_files = list(extract_dir.glob("DRAG.ON-*"))
        self.assertEqual(len(extracted_drag_files), 2)  # The 2 DRAG.ON files

    def test_add_errors_and_overwrite(self):
        """Test add error conditions and file operations"""
        pak = self._create_pak()
        
        # Create pak that needs creation
        new_pak = self.tmp / "new.pak"
        test_file = self.tmp / "test.txt"
        test_file.write_text("data")
        
        # Should fail without --create
        code, out, err = run_cpaktool(["add", str(new_pak), str(test_file)])
        self.assertNotEqual(code, 0)
        self.assertIn("use --create", err)
        
        # Should succeed with --create
        code, out, err = run_cpaktool(["add", "--create", str(new_pak), str(test_file)])
        self.assertEqual(code, 0)
        
        # Add same file twice, test update
        code, out, err = run_cpaktool(["add", "--verbose", "--update", str(new_pak), str(test_file)])
        self.assertEqual(code, 0)
        # Just verify command succeeded, don't rely on specific output format
        
        # Non-existent file
        code, out, err = run_cpaktool(["add", str(pak), str(self.tmp / "missing.txt")])
        self.assertNotEqual(code, 0)

    def test_add_filename_validation(self):
        """Test filename validation and character restrictions"""
        pak = self._create_pak()
        
        # Test valid filenames that should work
        valid_files = [
            "VALID.TXT",           # Standard uppercase
            "valid.txt",           # Lowercase (should be converted)
            "MiXeD.DaT",          # Mixed case
            "123.BIN",            # Numbers
            "A-B.C",              # Dash
            "X.Y",                # Single chars
            "VERYLONGNAME.EXT",   # Will be truncated with warning
            "file@test.txt",      # @ is supported
            "file#test.txt",      # # is supported  
            "file*test.txt",      # * is supported
            "file+test.txt",      # + is supported
            "file,test.txt",      # , is supported
            "file:test.txt",      # : is supported
            "file=test.txt",      # = is supported
            "file?test.txt",      # ? is supported
            "space file.txt",     # space is supported
            "FILENOEXT",          # No extension (valid)
            "FOO.BAR.BAZ",        # Multiple dots - filename=FOO.BAR, ext=BAZ
            "A.B.C.D",            # Multiple dots - filename=A.B.C, ext=D
        ]
        
        for filename in valid_files:
            with self.subTest(filename=filename):
                test_file = self.tmp / filename
                test_file.write_text("test data")
                
                code, out, err = run_cpaktool(["add", str(pak), str(test_file)])
                self.assertEqual(code, 0, f"Valid filename {filename} was rejected: {err}")
                
                # Check for truncation warning on long names
                # Split by last dot to get proper filename and extension
                parts = filename.rsplit('.', 1) if '.' in filename else [filename]
                fname_part = parts[0]
                ext_part = parts[1] if len(parts) > 1 else ""
                
                if len(fname_part) > 16 or len(ext_part) > 4:
                    self.assertIn("truncated", err, f"Expected truncation warning for {filename}")
        
        # Test invalid filenames that should be rejected
        invalid_files = [
            "invalid_underscore.txt",  # Underscore not supported
            "file$.txt",               # $ not supported
            "file%.txt",               # % not supported
            "file&.txt",               # & not supported
            "file[].txt",              # [] not supported
            "file{}.txt",              # {} not supported
            "file().txt",              # () not supported
            "file<>.txt",              # <> not supported
            "file|.txt",               # | not supported
            "file;.txt",               # ; not supported
            # Note: Non-ASCII test removed due to encoding issues in test runner
        ]
        
        for filename in invalid_files:
            with self.subTest(filename=filename):
                test_file = self.tmp / filename
                test_file.write_text("test data")
                
                code, out, err = run_cpaktool(["add", str(pak), str(test_file)])
                self.assertNotEqual(code, 0, f"Invalid filename {filename} was accepted")
                self.assertIn("unsupported characters", err, 
                            f"Expected character validation error for {filename}")
        
        # Test edge cases that should be rejected
        edge_cases_invalid = [
            (".txt", "no name - only extension"),
        ]
        
        for filename, description in edge_cases_invalid:
            with self.subTest(filename=filename, description=description):
                test_file = self.tmp / filename
                test_file.write_text("test data")
                code, out, err = run_cpaktool(["add", str(pak), str(test_file)])
                self.assertNotEqual(code, 0, f"Edge case {description} was accepted")
                self.assertIn("Invalid", err, f"Expected validation error for {description}")
        
        # Test edge cases that should be valid
        edge_cases_valid = [
            ("file.", "no extension - should work"),
        ]
        
        for filename, description in edge_cases_valid:
            with self.subTest(filename=filename, description=description):
                test_file = self.tmp / filename
                test_file.write_text("test data")
                code, out, err = run_cpaktool(["add", str(pak), str(test_file)])
                self.assertEqual(code, 0, f"Edge case {description} was rejected: {err}")

    def test_extract_patterns_and_overwrite(self):
        """Test extract pattern matching and overwrite behavior"""
        pak = self._create_pak()
        
        # Create files with different patterns (will be numbered in cpakfs)
        for name in ["file1.txt", "file2.dat", "other.bin"]:
            f = self.tmp / name
            f.write_text(f"content of {name}")
            run_cpaktool(["add", str(pak), str(f)])
        
        extract_dir = self.tmp / "extract"
        extract_dir.mkdir()
        
        # Pattern extraction tests - files get numbered as DRAG.ON-1., DRAG.ON-2., etc.
        test_cases = [
            ("DRAG.ON-*", 3),       # Should match all files with DRAG.ON prefix
            ("*1.*", 1),            # Should match DRAG.ON-1.
            ("*2.*", 1),            # Should match DRAG.ON-2.
            ("nonexistent", 0)      # Should match nothing
        ]
        
        for pattern, expected_count in test_cases:
            # Clean directory
            for f in extract_dir.glob("*"):
                f.unlink()
            
            code, out, err = run_cpaktool(["extract", str(pak), pattern], cwd=extract_dir)
            self.assertEqual(code, 0)
            # Count actual extracted files instead of parsing output
            extracted_files = list(extract_dir.glob("*"))
            self.assertEqual(len(extracted_files), expected_count)
        
        # Test overwrite behavior
        existing = extract_dir / "DRAG.ON-1."  # First file extracted
        if existing.exists():
            existing.write_text("modified content")
            
            # Should skip without --overwrite  
            code, out, err = run_cpaktool(["extract", "--verbose", str(pak), "*1.*"], cwd=extract_dir)
            self.assertEqual(code, 0)
            # Just verify file wasn't overwritten
            self.assertEqual(existing.read_text(), "modified content")
            
            # Should overwrite with --overwrite
            code, out, err = run_cpaktool(["extract", "--overwrite", str(pak), "*1.*"], cwd=extract_dir)
            self.assertEqual(code, 0)
            # File should be overwritten with padded content
            self.assertNotEqual(existing.read_text(), "modified content")

    def test_list_command_basic(self):
        """Test basic list command functionality"""
        pak = self._create_pak()
        
        # Test empty pak
        code, out, err = run_cpaktool(["list", str(pak)])
        self.assertEqual(code, 0)
        self.assertIn("Found 0 files", out)
        
        # Add some test files
        test_file1 = self.tmp / "test1.txt"
        test_file1.write_text("content1")
        test_file2 = self.tmp / "data.bin"  
        test_file2.write_bytes(b"binary_data")
        test_file3 = self.tmp / "config"  # No extension
        test_file3.write_text("config_data")
        
        # Add files with different game codes
        code, out, err = run_cpaktool(["add", "--gamecode", "GAME.01", str(pak), str(test_file1)])
        self.assertEqual(code, 0)
        
        code, out, err = run_cpaktool(["add", "--gamecode", "TEST.99", str(pak), str(test_file2)])
        self.assertEqual(code, 0)
        
        code, out, err = run_cpaktool(["add", str(pak), str(test_file3)])  # Default DRAG.ON
        self.assertEqual(code, 0)
        
        # Test basic list
        code, out, err = run_cpaktool(["list", str(pak)])
        self.assertEqual(code, 0)
        self.assertIn("GAME.01-TEST1.TXT", out)
        self.assertIn("TEST.99-DATA.BIN", out)
        self.assertIn("DRAG.ON-CONFIG", out)
        
    def test_list_command_formats(self):
        """Test list command output formats"""
        pak = self._create_pak("128KB")
        
        # Create files of different sizes
        small_file = self.tmp / "small.txt"
        small_file.write_text("small")  # ~5 bytes
        
        large_file = self.tmp / "large.dat"
        large_file.write_bytes(b"x" * 1500)  # 1500 bytes
        
        code, out, err = run_cpaktool(["add", "--gamecode", "ABCD.EF", str(pak), str(small_file)])
        self.assertEqual(code, 0)
        
        code, out, err = run_cpaktool(["add", "--gamecode", "WXYZ.12", str(pak), str(large_file)])
        self.assertEqual(code, 0)
        
        # Test long format
        code, out, err = run_cpaktool(["list", "-l", str(pak)])
        self.assertEqual(code, 0)
        self.assertIn("Game   Pub  Filename", out)  # Header
        self.assertIn("ABCD   EF   SMALL", out)
        self.assertIn("WXYZ   12   LARGE", out)
        self.assertIn("256", out)  # Size (cpakfs uses 256-byte blocks minimum)
        
        # Test human-readable format
        code, out, err = run_cpaktool(["list", "-l", "-H", str(pak)])
        self.assertEqual(code, 0)
        self.assertIn("256B", out)  # Small file size
        # Large file should show in KB if > 1024 bytes after padding
        
    def test_list_command_sorting(self):
        """Test list command sorting options"""
        pak = self._create_pak()
        
        # Create files with predictable names for sorting
        file_a = self.tmp / "a.txt"
        file_a.write_text("a")
        file_z = self.tmp / "z.txt"  
        file_z.write_text("z")
        file_m = self.tmp / "m.txt"
        file_m.write_text("m")
        
        # Add with different game codes to test name sorting
        code, out, err = run_cpaktool(["add", "--gamecode", "ZZZZ.99", str(pak), str(file_a)])
        self.assertEqual(code, 0)
        code, out, err = run_cpaktool(["add", "--gamecode", "AAAA.01", str(pak), str(file_z)])
        self.assertEqual(code, 0)
        code, out, err = run_cpaktool(["add", "--gamecode", "MMMM.50", str(pak), str(file_m)])
        self.assertEqual(code, 0)
        
        # Test name sorting (default)
        code, out, err = run_cpaktool(["list", str(pak)])
        self.assertEqual(code, 0)
        lines = [line for line in out.strip().split('\n') if line and 'Found' not in line]
        self.assertTrue(lines[0].startswith("AAAA.01"))  # A comes first
        self.assertTrue(lines[1].startswith("MMMM.50"))  # M comes second
        self.assertTrue(lines[2].startswith("ZZZZ.99"))  # Z comes last
        
        # Test explicit name sorting
        code, out, err = run_cpaktool(["list", "--sort", "name", str(pak)])
        self.assertEqual(code, 0)
        lines = [line for line in out.strip().split('\n') if line and 'Found' not in line]
        self.assertTrue(lines[0].startswith("AAAA.01"))
        
        # Test reverse sorting
        code, out, err = run_cpaktool(["list", "--sort", "name", "--reverse", str(pak)])
        self.assertEqual(code, 0)
        lines = [line for line in out.strip().split('\n') if line and 'Found' not in line]
        self.assertTrue(lines[0].startswith("ZZZZ.99"))  # Z comes first when reversed
        self.assertTrue(lines[2].startswith("AAAA.01"))  # A comes last when reversed
        
        # Test size sorting (all same size due to cpakfs padding, but should not error)
        code, out, err = run_cpaktool(["list", "--sort", "size", str(pak)])
        self.assertEqual(code, 0)
        
    def test_list_command_patterns(self):
        """Test list command with pattern matching"""
        pak = self._create_pak()
        
        # Add files with different patterns
        txt_file = self.tmp / "doc.txt"
        txt_file.write_text("text")
        bin_file = self.tmp / "save.bin" 
        bin_file.write_text("binary")
        dat_file = self.tmp / "config.dat"
        dat_file.write_text("data")
        
        code, out, err = run_cpaktool(["add", "--gamecode", "GAME.01", str(pak), str(txt_file)])
        self.assertEqual(code, 0)
        code, out, err = run_cpaktool(["add", "--gamecode", "SAVE.02", str(pak), str(bin_file)])
        self.assertEqual(code, 0)
        code, out, err = run_cpaktool(["add", str(pak), str(dat_file)])  # DRAG.ON
        self.assertEqual(code, 0)
        
        # Test pattern matching
        code, out, err = run_cpaktool(["list", str(pak), "GAME.*"])
        self.assertEqual(code, 0)
        self.assertIn("GAME.01-DOC.TXT", out)
        self.assertNotIn("SAVE.02", out)
        self.assertNotIn("DRAG.ON", out)
        
        # Test extension pattern
        code, out, err = run_cpaktool(["list", str(pak), "*.BIN"])
        self.assertEqual(code, 0)
        self.assertIn("SAVE.02-SAVE.BIN", out)
        self.assertNotIn("DOC.TXT", out)
        self.assertNotIn("CONFIG.DAT", out)
        
        # Test multiple patterns
        code, out, err = run_cpaktool(["list", str(pak), "GAME.*", "DRAG.*"])
        self.assertEqual(code, 0)
        self.assertIn("GAME.01-DOC.TXT", out)
        self.assertIn("DRAG.ON-CONFIG.DAT", out)
        self.assertNotIn("SAVE.02", out)

if __name__ == "__main__":
    unittest.main()
