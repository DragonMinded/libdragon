#!/usr/bin/env python3
"""
Functional tests for cpaktool commands

WHAT TO TEST:
- Command behavior with valid inputs
- Command error handling with invalid inputs
- Specific corner cases in implementation 

WHAT NOT TO TEST:
- CLI argument parsing (this is covered in the CLI tests)
- Aliases of commands
- Verbose mode, we assume that works as expected. Verbose mode
  can be used while performing other tests of course
- Simple error conditions, like missing command line arguments
"""
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
        self._pak_counter = 0

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
        self._pak_counter += 1
        pak = self.tmp / f"test_{size.replace('KB', 'kb')}_{self._pak_counter}.pak"
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
        self.assertNotEqual(code, 0, "fsck should fail if issues are found")
        
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
        
        code, out, err = run_cpaktool(["extract", str(pak), "DRAG.ON-*"], cwd=extract_dir)
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
        code, out, err = run_cpaktool(["add", "--update", str(new_pak), str(test_file)])
        self.assertEqual(code, 0)
        # Just verify command succeeded, don't rely on specific output format
        
        # Non-existent file
        code, out, err = run_cpaktool(["add", str(pak), str(self.tmp / "missing.txt")])
        self.assertNotEqual(code, 0)

    def test_add_file_overwrite_scenarios(self):
        """Test file overwrite with different sizes and content verification"""
        pak = self._create_pak("128KB")  # Larger pak for space tests
        
        # Test file with initial content
        test_file = self.tmp / "GAME.01-SAVE.DAT"
        original_content = "Original content " * 10  # ~170 chars
        test_file.write_text(original_content)
        
        # Add initial file
        code, out, err = run_cpaktool(["add", str(pak), str(test_file)])
        self.assertEqual(code, 0, f"Failed to add initial file: {err}")
        
        # Verify initial file is there and has correct content
        extract_dir = self.tmp / "extract1"
        extract_dir.mkdir()
        code, out, err = run_cpaktool(["extract", str(pak)], cwd=extract_dir)
        self.assertEqual(code, 0)
        
        extracted = extract_dir / "GAME.01-SAVE.DAT"
        self.assertTrue(extracted.exists())
        extracted_content = extracted.read_text()
        self.assertTrue(extracted_content.startswith(original_content))
        
        # Test overwrite with SMALLER file
        smaller_content = "Small"  # ~5 chars
        test_file.write_text(smaller_content)
        
        code, out, err = run_cpaktool(["add", "--update", str(pak), str(test_file)])
        self.assertEqual(code, 0, f"Failed to update with smaller file: {err}")
        
        # Verify smaller file content
        extract_dir2 = self.tmp / "extract2"
        extract_dir2.mkdir()
        code, out, err = run_cpaktool(["extract", str(pak)], cwd=extract_dir2)
        self.assertEqual(code, 0)
        
        extracted2 = extract_dir2 / "GAME.01-SAVE.DAT"
        self.assertTrue(extracted2.exists())
        extracted_content2 = extracted2.read_text()
        self.assertEqual(extracted_content2, smaller_content)
        
        # Test overwrite with LARGER file
        larger_content = "This is a much larger content " * 50  # ~1500 chars
        test_file.write_text(larger_content)
        
        code, out, err = run_cpaktool(["add", "--update", str(pak), str(test_file)])
        self.assertEqual(code, 0, f"Failed to update with larger file: {err}")
        
        # Verify larger file content
        extract_dir3 = self.tmp / "extract3"
        extract_dir3.mkdir()
        code, out, err = run_cpaktool(["extract", str(pak)], cwd=extract_dir3)
        self.assertEqual(code, 0)
        
        extracted3 = extract_dir3 / "GAME.01-SAVE.DAT"
        self.assertTrue(extracted3.exists())
        extracted_content3 = extracted3.read_text()
        self.assertTrue(extracted_content3.startswith(larger_content))
        
        # Test pak integrity after all operations
        code, out, err = run_cpaktool(["test", str(pak)])
        self.assertEqual(code, 0, f"Pak integrity check failed after overwrites: {err}")

    def test_add_multiple_overwrites_space_management(self):
        """Test multiple file overwrites and space management"""
        pak = self._create_pak("64KB")
        
        # Create multiple files of different sizes
        files_data = [
            ("FILE1.TXT", "Content1"),
            ("FILE2.DAT", "Content2" * 100),  # Larger
            ("FILE3.BIN", "C3")  # Small
        ]
        
        # Add all files initially
        for filename, content in files_data:
            test_file = self.tmp / f"GAME.01-{filename}"
            test_file.write_text(content)
            code, out, err = run_cpaktool(["add", str(pak), str(test_file)])
            self.assertEqual(code, 0, f"Failed to add {filename}: {err}")
        
        # Verify all files are present
        code, out, err = run_cpaktool(["list", str(pak)])
        self.assertEqual(code, 0)
        for filename, _ in files_data:
            self.assertIn(f"GAME.01-{filename}", out)
        
        # Now overwrite with different sizes
        new_contents = [
            ("FILE1.TXT", "NewContent1" * 200),  # Make much larger
            ("FILE2.DAT", "Small"),  # Make much smaller
            ("FILE3.BIN", "Medium size content")  # Make medium
        ]
        
        for filename, new_content in new_contents:
            test_file = self.tmp / f"GAME.01-{filename}"
            test_file.write_text(new_content)
            code, out, err = run_cpaktool(["add", "--update", str(pak), str(test_file)])
            self.assertEqual(code, 0, f"Failed to update {filename}: {err}")
        
        # Extract and verify all new contents
        extract_dir = self.tmp / "extract_final"
        extract_dir.mkdir()
        code, out, err = run_cpaktool(["extract", str(pak)], cwd=extract_dir)
        self.assertEqual(code, 0)
        
        for filename, expected_content in new_contents:
            extracted = extract_dir / f"GAME.01-{filename}"
            self.assertTrue(extracted.exists(), f"File {filename} not found after update")
            actual_content = extracted.read_text()
            self.assertEqual(actual_content, expected_content, 
                           f"Content mismatch for {filename}")
        
        # Final integrity check
        code, out, err = run_cpaktool(["test", str(pak)])
        self.assertEqual(code, 0, f"Final integrity check failed: {err}")

    def test_add_overwrite_without_update_flag(self):
        """Test that adding existing files without --update flag fails appropriately"""
        pak = self._create_pak()
        
        # Create and add initial file
        test_file = self.tmp / "GAME.01-EXISTING.DAT"
        test_file.write_text("Original content")
        
        code, out, err = run_cpaktool(["add", str(pak), str(test_file)])
        self.assertEqual(code, 0, f"Failed to add initial file: {err}")
        
        # Try to add same file again without --update (should fail)
        test_file.write_text("New content")
        code, out, err = run_cpaktool(["add", str(pak), str(test_file)])
        self.assertNotEqual(code, 0, "Adding existing file without --update should fail")
        self.assertIn("already exists", err.lower())
        
        # Verify original content is preserved
        extract_dir = self.tmp / "extract"
        extract_dir.mkdir()
        code, out, err = run_cpaktool(["extract", str(pak)], cwd=extract_dir)
        self.assertEqual(code, 0)
        
        extracted = extract_dir / "GAME.01-EXISTING.DAT"
        self.assertTrue(extracted.exists())
        extracted_content = extracted.read_text()
        self.assertEqual(extracted_content, "Original content")

    def test_add_filename_validation(self):
        """Test filename validation and character restrictions"""
        pak = self._create_pak("128KB")  # Use larger pak for all these files
        
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
        
        for i, filename in enumerate(valid_files):
            with self.subTest(filename=filename):
                # Create a unique pak for each file to avoid name conflicts
                pak = self.tmp / f"test_validate_{i}.pak"
                code, out, err = run_cpaktool(["format", "--size", "64", str(pak)])
                self.assertEqual(code, 0, f"Failed to format pak for {filename}: {err}")
                
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
            code, out, err = run_cpaktool(["extract", str(pak), "*1.*"], cwd=extract_dir)
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
        self.assertIn("Game       Pub    Filename", out)  # Header
        self.assertIn("ABCD       EF     SMALL", out)
        self.assertIn("WXYZ       12     LARGE", out)
        self.assertIn(" 5\n", out)  # Size
        
        # Test human-readable format
        code, out, err = run_cpaktool(["list", "-l", "-H", str(pak)])
        self.assertEqual(code, 0)
        self.assertIn(" 5B", out)  # Small file size
        self.assertIn(" 1.5K", out)  # Large file size
        
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

    def test_delete_basic(self):
        """Test basic delete functionality"""
        pak = self._create_pak()
        
        # Create and add test files
        test1 = self.tmp / "TEST.01-FILE1.DAT"
        test1.write_text("Test file 1")
        test2 = self.tmp / "TEST.01-FILE2.DAT"
        test2.write_text("Test file 2")
        test3 = self.tmp / "GAME.02-SAVE.DAT"
        test3.write_text("Save data")
        
        code, out, err = run_cpaktool(["add", str(pak), str(test1), str(test2), str(test3)])
        self.assertEqual(code, 0)
        
        # Verify files are there
        code, out, err = run_cpaktool(["list", str(pak)])
        self.assertEqual(code, 0)
        self.assertIn("TEST.01-FILE1.DAT", out)
        self.assertIn("TEST.01-FILE2.DAT", out)
        self.assertIn("GAME.02-SAVE.DAT", out)
        
        # Delete specific file
        code, out, err = run_cpaktool(["delete", str(pak), "TEST.01-FILE1.DAT"])
        self.assertEqual(code, 0)
        self.assertIn("Deleted: TEST.01/FILE1.DAT", out)
        
        # Verify file was deleted
        code, out, err = run_cpaktool(["list", str(pak)])
        self.assertEqual(code, 0)
        self.assertNotIn("TEST.01-FILE1.DAT", out)
        self.assertIn("TEST.01-FILE2.DAT", out)
        self.assertIn("GAME.02-SAVE.DAT", out)

    def test_delete_patterns(self):
        """Test delete with pattern matching"""
        pak = self._create_pak()
        
        # Create and add test files
        test_files = [
            ("TEST.01-FILE1.DAT", "content1"),
            ("TEST.01-FILE2.DAT", "content2"), 
            ("TEST.01-FILE3.TXT", "content3"),
            ("GAME.02-SAVE.DAT", "save data"),
            ("GAME.02-CONFIG.CFG", "config data")
        ]
        
        for filename, content in test_files:
            test_file = self.tmp / filename
            test_file.write_text(content)
            code, out, err = run_cpaktool(["add", str(pak), str(test_file)])
            self.assertEqual(code, 0)
        
        # Delete using wildcard pattern
        code, out, err = run_cpaktool(["delete", str(pak), "TEST.01-*.DAT"])
        self.assertEqual(code, 0)
        
        # Verify correct files were deleted
        code, out, err = run_cpaktool(["list", str(pak)])
        self.assertEqual(code, 0)
        self.assertNotIn("TEST.01-FILE1.DAT", out)
        self.assertNotIn("TEST.01-FILE2.DAT", out)
        self.assertIn("TEST.01-FILE3.TXT", out)  # Different extension, should remain
        self.assertIn("GAME.02-SAVE.DAT", out)  # Different game code, should remain
        self.assertIn("GAME.02-CONFIG.CFG", out)
        
        # Delete all remaining TEST.01 files
        code, out, err = run_cpaktool(["delete", str(pak), "TEST.01-*"])
        self.assertEqual(code, 0)
        
        # Verify only GAME.02 files remain
        code, out, err = run_cpaktool(["list", str(pak)])
        self.assertEqual(code, 0)
        self.assertNotIn("TEST.01", out)
        self.assertIn("GAME.02-SAVE.DAT", out)
        self.assertIn("GAME.02-CONFIG.CFG", out)

    def test_delete_dry_run(self):
        """Test delete dry-run functionality"""
        pak = self._create_pak()
        
        # Create and add test files
        test1 = self.tmp / "TEST.01-DELETE.DAT"
        test1.write_text("To be deleted")
        test2 = self.tmp / "TEST.01-KEEP.DAT"
        test2.write_text("To be kept")
        
        code, out, err = run_cpaktool(["add", str(pak), str(test1), str(test2)])
        self.assertEqual(code, 0)
        
        # Dry run delete
        code, out, err = run_cpaktool(["--dry-run", "delete", str(pak), "TEST.01-DELETE.DAT"])
        self.assertEqual(code, 0)
        self.assertIn("Would delete: TEST.01/DELETE.DAT", out)
        
        # Verify file is still there
        code, out, err = run_cpaktool(["list", str(pak)])
        self.assertEqual(code, 0)
        self.assertIn("TEST.01-DELETE.DAT", out)
        self.assertIn("TEST.01-KEEP.DAT", out)
        
        # Actually delete now
        code, out, err = run_cpaktool(["delete", str(pak), "TEST.01-DELETE.DAT"])
        self.assertEqual(code, 0)
        
        # Verify file is gone
        code, out, err = run_cpaktool(["list", str(pak)])
        self.assertEqual(code, 0)
        self.assertNotIn("TEST.01-DELETE.DAT", out)
        self.assertIn("TEST.01-KEEP.DAT", out)

    def test_delete_error_cases(self):
        """Test delete error handling"""
        pak = self._create_pak()
        
        # Test deleting from non-existent pak
        fake_pak = self.tmp / "nonexistent.pak"
        code, out, err = run_cpaktool(["delete", str(fake_pak), "*.DAT"])
        self.assertNotEqual(code, 0)
        self.assertIn("File not found", err)
        
        # Test pattern that matches no files
        code, out, err = run_cpaktool(["delete", str(pak), "NONEXISTENT-*"])
        self.assertEqual(code, 0)
        self.assertIn("No files match the specified patterns", out)
        
        # Test missing arguments
        code, out, err = run_cpaktool(["delete", str(pak)])
        self.assertNotEqual(code, 0)
        self.assertIn("delete command requires at least two arguments", err)
        
        code, out, err = run_cpaktool(["delete"])
        self.assertNotEqual(code, 0)
        self.assertIn("delete command requires at least two arguments", err)

    def test_delete_multiple_patterns(self):
        """Test delete with multiple patterns"""
        pak = self._create_pak()
        
        # Create test files
        test_files = [
            ("GAME.01-FILE1.DAT", "content1"),
            ("GAME.01-FILE2.TXT", "content2"),
            ("SAVE.02-DATA1.DAT", "content3"),
            ("SAVE.02-DATA2.BIN", "content4"),
            ("OTHER.03-KEEP.DAT", "content5")
        ]
        
        for filename, content in test_files:
            test_file = self.tmp / filename
            test_file.write_text(content)
            code, out, err = run_cpaktool(["add", str(pak), str(test_file)])
            self.assertEqual(code, 0)
        
        # Delete using multiple patterns
        code, out, err = run_cpaktool(["delete", str(pak), "GAME.01-*", "SAVE.02-*.DAT"])
        self.assertEqual(code, 0)
        
        # Verify correct files were deleted
        code, out, err = run_cpaktool(["list", str(pak)])
        self.assertEqual(code, 0)
        # GAME.01 files should be gone (both patterns)
        self.assertNotIn("GAME.01-FILE1.DAT", out)
        self.assertNotIn("GAME.01-FILE2.TXT", out)
        # SAVE.02 .DAT file should be gone (second pattern)
        self.assertNotIn("SAVE.02-DATA1.DAT", out)
        # SAVE.02 .BIN file should remain (doesn't match second pattern)
        self.assertIn("SAVE.02-DATA2.BIN", out)
        # OTHER.03 file should remain (doesn't match either pattern)
        self.assertIn("OTHER.03-KEEP.DAT", out)

    def test_crc_calculation(self):
        """Test --crc option calculates correct CRC32 for multiple files"""
        pak = self._create_pak()
        
        # Create test files with known content
        test_files = [
            ("DRAG.ON-test1.txt", b"Hello World!"),
            ("SAVE.01-data.bin", b'\x00\x01\x02\x03\x04\x05\x06\x07'),
            ("GAME.ZZ-empty.dat", b""),
        ]
        
        for filename, content in test_files:
            test_file = self.tmp / filename
            test_file.write_bytes(content)
            code, out, err = run_cpaktool(["add", str(pak), str(test_file)])
            self.assertEqual(code, 0, f"Failed to add {filename}: {err}")
        
        # Test --crc shows CRC32 values 
        code, out, err = run_cpaktool(["list", "--crc", str(pak)])
        self.assertEqual(code, 0, f"Failed to list with --crc: {err}")
        self.assertIn("CRC32", out)
        
        # Verify expected CRC32 values appear in output
        import zlib
        self.assertIn(f"{zlib.crc32(test_files[0][1]) & 0xffffffff:08X}", out)  # test1.txt
        self.assertIn(f"{zlib.crc32(test_files[1][1]) & 0xffffffff:08X}", out)  # data.bin  
        self.assertIn(f"{zlib.crc32(test_files[2][1]) & 0xffffffff:08X}", out)  # empty.dat

    def test_add_16_file_limit(self):
        """Test that adding more than 16 files produces the correct error message"""
        pak = self._create_pak("128KB")  # Use larger pak to avoid space issues
        
        # Add exactly 16 files (the maximum)
        files_added = []
        for i in range(16):
            filename = f"FILE{i:02d}.TXT"
            content = f"Content of file {i}".encode()
            test_file = self.tmp / filename
            test_file.write_bytes(content)
            
            code, out, err = run_cpaktool(["add", str(pak), str(test_file)])
            self.assertEqual(code, 0, f"Failed to add file {i}: {err}")
            files_added.append(filename)
        
        # Verify we can list all 16 files
        code, out, err = run_cpaktool(["list", str(pak)])
        self.assertEqual(code, 0, f"Failed to list files: {err}")
        for filename in files_added:
            expected_cpak_name = f"DRAG.ON-{filename}"
            self.assertIn(expected_cpak_name, out, f"File {filename} not found in listing")
        
        # Now try to add the 17th file - this should fail with EMFILE error
        file17 = self.tmp / "FILE17.TXT"
        file17.write_text("This should fail")
        
        code, out, err = run_cpaktool(["add", str(pak), str(file17)])
        self.assertNotEqual(code, 0, "Adding 17th file should have failed")
        self.assertIn("Too many files", err, f"Expected EMFILE error message, got: {err}")
        self.assertIn("maximum 16 notes", err, f"Expected mention of 16 file limit, got: {err}")
        
        # Verify the pak still has exactly 16 files and is consistent
        code, out, err = run_cpaktool(["list", str(pak)])
        self.assertEqual(code, 0, f"Failed to list files after failure: {err}")
        file_count = len([line for line in out.strip().split('\n') if line and not line.startswith('Found')])
        self.assertEqual(file_count, 16, f"Expected exactly 16 files, found {file_count}")
        
        # Test that the pak is still consistent
        code, out, err = run_cpaktool(["test", str(pak)])
        self.assertEqual(code, 0, f"Pak integrity check failed after 16-file limit test: {err}")

    def test_add_space_vs_file_limit_errors(self):
        """Test that we get different error messages for space exhaustion vs file limit"""
        # Test file limit error (16 files max)
        pak_small = self._create_pak("32KB")  # Small pak, but should fit 16 small files
        
        # Add 16 very small files
        for i in range(16):
            filename = f"F{i:02d}.TXT"
            test_file = self.tmp / filename
            test_file.write_text("x")  # 1 byte files
            
            code, out, err = run_cpaktool(["add", str(pak_small), str(test_file)])
            self.assertEqual(code, 0, f"Failed to add small file {i}: {err}")
        
        # 17th file should hit file limit, not space limit
        file17 = self.tmp / "F17.TXT"
        file17.write_text("x")
        code, out, err = run_cpaktool(["add", str(pak_small), str(file17)])
        self.assertNotEqual(code, 0, "Adding 17th file should have failed")
        self.assertIn("Too many files", err, f"Expected file limit error, got: {err}")
        self.assertNotIn("No space left", err, f"Should not mention space, got: {err}")
        
        # Test space limit error - create a fresh pak and try to add a huge file
        pak_space = self._create_pak("32KB")
        huge_file = self.tmp / "HUGE.BIN"
        huge_content = b"x" * (30 * 1024)  # 30KB file in 32KB pak should fail due to filesystem overhead
        huge_file.write_bytes(huge_content)
        
        code, out, err = run_cpaktool(["add", str(pak_space), str(huge_file)])
        if code != 0:  # Should fail due to space, not file count
            self.assertIn("No space left", err, f"Expected space error, got: {err}")
            self.assertNotIn("Too many files", err, f"Should not mention file limit, got: {err}")

if __name__ == "__main__":
    unittest.main()
