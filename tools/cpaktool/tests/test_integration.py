#!/usr/bin/env python3
import subprocess
import tempfile
import unittest
from pathlib import Path
import hashlib

def run_cpaktool(args, cwd=None, capture_output=True):
    exe = Path(__file__).resolve().parents[1] / "cpaktool"
    if not exe.exists():
        raise RuntimeError(f"cpaktool executable not found at {exe}. Build it before running tests.")
    cmd = [str(exe)] + args
    if capture_output:
        proc = subprocess.run(cmd, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        return proc.returncode, proc.stdout, proc.stderr
    else:
        proc = subprocess.run(cmd, cwd=cwd, text=True)
        return proc.returncode, "", ""

class TestIntegration(unittest.TestCase):
    """Integration tests focusing on roundtrip add/extract with various buffer sizes"""
    
    def setUp(self):
        self.tmpdir = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmpdir.cleanup)
        self.tmp = Path(self.tmpdir.name)

    def _create_pak(self, size="64"):
        pak = self.tmp / "test.pak"
        # Remove existing pak file if present
        if pak.exists():
            pak.unlink()
        code, out, err = run_cpaktool(["format", "--size", size, str(pak)])
        if code != 0:
            self.fail(f"Failed to format: {err}\nStdout: {out}")
        return pak

    def _file_hash(self, path):
        return hashlib.md5(path.read_bytes()).hexdigest()
    
    def _validate_cyclic_content(self, actual_content, expected_size, start_offset=0, name="file"):
        """Validate that content matches expected cyclic pattern and report specific errors"""
        if len(actual_content) < expected_size:
            self.fail(f"{name}: Content too short - got {len(actual_content)}, expected at least {expected_size}")
        
        # Check the pattern up to expected_size
        for i in range(expected_size):
            expected_byte = (start_offset + i) % 256
            actual_byte = actual_content[i]
            if actual_byte != expected_byte:
                # Report context around the error
                start_ctx = max(0, i - 5)
                end_ctx = min(len(actual_content), i + 6)
                actual_ctx = actual_content[start_ctx:end_ctx]
                expected_ctx = bytes((start_offset + j) % 256 for j in range(start_ctx, end_ctx))
                self.fail(f"{name}: Byte mismatch at position {i} - expected {expected_byte}, got {actual_byte}\n"
                         f"Context around error:\n"
                         f"  Expected: {expected_ctx.hex()}\n" 
                         f"  Actual:   {actual_ctx.hex()}\n"
                         f"  Position: {' ' * (2 * (i - start_ctx))}^^")

    def test_roundtrip_various_bufsizes(self):
        """Test add/extract roundtrip with various buffer sizes including pathological cases"""
        
        # Create test files with cpakfs-compatible names (A-Z, 0-9, supported symbols only)
        test_files_data = [
            (50, "SMALL.TXT"),
            (256, "EXACT.BIN"), 
            (1000, "LARGE.DAT"),
            (1, "TINY.X")
        ]
        
        # Pathological buffer sizes: primes, powers of 2, edge cases
        buffer_sizes = [1, 2, 3, 5, 7, 11, 13, 17, 31, 64, 127, 128, 129, 255, 256, 257, 512, 1024, 4096]
        add_buffer_sizes = [1, 13, 256, 4096]  # Sample of add buffer sizes
        
        # Create all test combinations as subtests
        for add_bufsize in add_buffer_sizes:
            for extract_bufsize in buffer_sizes:
                for file_size, file_name in test_files_data:
                    with self.subTest(file=file_name, size=file_size, add_buf=add_bufsize, extract_buf=extract_bufsize):
                        self._test_single_file_roundtrip(file_name, file_size, add_bufsize, extract_bufsize)
    
    def _test_single_file_roundtrip(self, file_name, file_size, add_bufsize, extract_bufsize):
        """Test roundtrip for a single file with specific buffer sizes"""
        pak = self._create_pak("128")
        
        # Create test file with cyclic pattern
        test_file = self.tmp / file_name
        content = bytes((i % 256) for i in range(file_size))
        test_file.write_bytes(content)
        
        # Add file with specific buffer size
        code, out, err = run_cpaktool(["add", "--debug-bufsize", str(add_bufsize), str(pak), str(test_file)])
        if code != 0:
            self.fail(f"Add failed for {file_name} (size={file_size}) with add_buf={add_bufsize}: {err}\nStdout: {out}")
        
        # Extract with different buffer size
        extract_dir = self.tmp / f"extract_{file_name}_{add_bufsize}_{extract_bufsize}"
        extract_dir.mkdir(exist_ok=True)
        
        # Clean extract dir
        for f in extract_dir.glob("*"):
            f.unlink()
        
        code, out, err = run_cpaktool(["extract", "--debug-bufsize", str(extract_bufsize), str(pak)], cwd=extract_dir)
        if code != 0:
            self.fail(f"Extract failed for {file_name} (size={file_size}) with extract_buf={extract_bufsize}: {err}\nStdout: {out}")
        
        # Verify file was extracted
        extracted_file = extract_dir / f"DRAG.ON-{file_name}"
        self.assertTrue(extracted_file.exists(), f"File {file_name} not extracted with buffers add={add_bufsize}, extract={extract_bufsize}")
        
        # Validate content with detailed error reporting
        actual_content = extracted_file.read_bytes()
        self._validate_cyclic_content(
            actual_content, 
            file_size, 
            0, 
            f"{file_name} (size={file_size}, add_buf={add_bufsize}, extract_buf={extract_bufsize})"
        )

    def test_pattern_extraction_precision(self):
        """Test precise pattern matching with multiple files"""
        pak = self._create_pak()
        
        # Create files with overlapping patterns using cpakfs-compatible names
        files = {
            "GAME1.SAV": bytes((i % 256) for i in range(100)),  # Detectable pattern
            "GAME1.CFG": bytes((i % 256) for i in range(50, 200)),  # Different offset
            "GAME2.SAV": bytes((i % 256) for i in range(200, 400)),  # Different range
            "OTHER.TXT": bytes((i % 256) for i in range(25))  # Small file
        }
        
        for name, content in files.items():
            f = self.tmp / name
            f.write_bytes(content)
            code, out, err = run_cpaktool(["add", str(pak), str(f)])
            if code != 0:
                self.fail(f"Failed to add {name}: {err}\nStdout: {out}")
        
        extract_dir = self.tmp / "extract"
        extract_dir.mkdir()
        
        # Test various patterns with updated expected names
        pattern_tests = [
            ("*GAME1*", ["DRAG.ON-GAME1.SAV", "DRAG.ON-GAME1.CFG"]),
            ("*.SAV", ["DRAG.ON-GAME1.SAV", "DRAG.ON-GAME2.SAV"]),
            ("*OTHER*", ["DRAG.ON-OTHER.TXT"]),
            ("DRAG.ON-GAME2.*", ["DRAG.ON-GAME2.SAV"])
        ]
        
        for pattern, expected_files in pattern_tests:
            with self.subTest(pattern=pattern):
                # Clean extract dir
                for f in extract_dir.glob("*"):
                    f.unlink()
                
                code, out, err = run_cpaktool(["extract", str(pak), pattern], cwd=extract_dir)
                if code != 0:
                    self.fail(f"Pattern {pattern} failed: {err}\nStdout: {out}")
                
                extracted_names = sorted([f.name for f in extract_dir.glob("*")])
                expected_names = sorted(expected_files)
                self.assertEqual(extracted_names, expected_names, 
                               f"Pattern {pattern} extracted wrong files")
                
                # Verify content integrity
                for extracted_file in extract_dir.glob("*"):
                    original_name = extracted_file.name.replace("DRAG.ON-", "")
                    expected_content = files[original_name]
                    actual_content = extracted_file.read_bytes()
                    # Remove cpakfs padding (files are padded to 256-byte boundaries)
                    actual_content = actual_content[:len(expected_content)]
                    
                    # Use detailed validation for cyclic patterns
                    if original_name == "GAME1.SAV":
                        self._validate_cyclic_content(actual_content, 100, 0, f"{original_name} with pattern {pattern}")
                    elif original_name == "GAME1.CFG":
                        self._validate_cyclic_content(actual_content, 150, 50, f"{original_name} with pattern {pattern}")
                    elif original_name == "GAME2.SAV":
                        self._validate_cyclic_content(actual_content, 200, 200, f"{original_name} with pattern {pattern}")
                    elif original_name == "OTHER.TXT":
                        self._validate_cyclic_content(actual_content, 25, 0, f"{original_name} with pattern {pattern}")
                    else:
                        self.assertEqual(actual_content, expected_content,
                                       f"Content mismatch for {original_name} with pattern {pattern}")

    def test_large_file_chunking(self):
        """Test large file handling with small buffers"""
        pak = self._create_pak("256")  # Large pak
        
        # Create a file larger than typical buffer sizes with cpakfs-compatible name
        large_file = self.tmp / "LARGE.BIN"
        # Create pattern that's detectable if chunks are misaligned
        # Use cyclic byte pattern with header/footer markers
        header = b"HEADER"
        footer = b"FOOTER"
        middle_size = 10000
        middle = bytes((i % 256) for i in range(middle_size))
        content = header + middle + footer
        large_file.write_bytes(content)
        
        # Test with very small buffer that forces many read/write operations
        for bufsize in [1, 7, 127]:  # Odd sizes that don't align well
            with self.subTest(bufsize=bufsize):
                # Clean pak and add file
                pak.unlink()
                pak = self._create_pak("256")
                
                code, out, err = run_cpaktool(["add", "--debug-bufsize", str(bufsize), str(pak), str(large_file)])
                if code != 0:
                    self.fail(f"Add failed with bufsize {bufsize}: {err}\nStdout: {out}")
                
                # Extract with same small buffer
                extract_dir = self.tmp / f"extract_large_{bufsize}"
                extract_dir.mkdir(exist_ok=True)
                for f in extract_dir.glob("*"):
                    f.unlink()
                
                code, out, err = run_cpaktool(["extract", "--debug-bufsize", str(bufsize), str(pak)], cwd=extract_dir)
                if code != 0:
                    self.fail(f"Extract failed with bufsize {bufsize}: {err}\nStdout: {out}")
                
                # Verify exact content
                extracted = extract_dir / "DRAG.ON-LARGE.BIN"
                self.assertTrue(extracted.exists())
                
                extracted_content = extracted.read_bytes()
                # Check header first
                if not extracted_content.startswith(b"HEADER"):
                    self.fail(f"Missing header with bufsize {bufsize}")
                
                # Validate the middle section with cyclic pattern
                header_len = len(b"HEADER")
                footer_len = len(b"FOOTER")
                if len(extracted_content) >= len(content):
                    middle_content = extracted_content[header_len:header_len + middle_size]
                    self._validate_cyclic_content(middle_content, middle_size, 0, f"middle section (bufsize {bufsize})")
                    
                    # Check footer
                    footer_start = header_len + middle_size
                    if extracted_content[footer_start:footer_start + footer_len] != b"FOOTER":
                        self.fail(f"Missing or corrupted footer with bufsize {bufsize}")
                else:
                    self.fail(f"File too short with bufsize {bufsize}: {len(extracted_content)} < {len(content)}")

    def test_embedded_null_characters(self):
        """Test handling of embedded NUL characters in filenames"""
        pak = self._create_pak("128KB")
        
        # Create test files with content
        content1 = "Content for regular file"
        content2 = "Content for file with embedded NUL"
        
        # Create files with specific names that will test embedded NUL handling
        # The \x01 in the filename will be converted to \x00 in the N64 codepage
        file1 = self.tmp / "GAME.01-foo.txt"
        file2 = self.tmp / "GAME.01-foo\x01bar.txt"  # This represents "foo\x00bar" in N64 codepage
        
        file1.write_text(content1)
        file2.write_text(content2)
        
        # Add both files to the pak
        code, out, err = run_cpaktool(["add", str(pak), str(file1)])
        self.assertEqual(code, 0, f"Failed to add regular file: {err}")
        
        code, out, err = run_cpaktool(["add", str(pak), str(file2)])
        self.assertEqual(code, 0, f"Failed to add file with embedded NUL: {err}")
        
        # Verify both files are listed and distinguishable
        code, out, err = run_cpaktool(["list", "-1", str(pak)])
        self.assertEqual(code, 0, f"Failed to list files: {err}")
        
        # Both files should be present in the listing
        # Note: filenames are converted to uppercase in N64 codepage
        # The file with embedded NUL should be shown with \x01 (or escaped form)
        self.assertIn("GAME.01-FOO.TXT", out)
        # The exact representation of \x01 in output may vary, but it should be distinguishable
        # Let's check that there are exactly 2 files listed with different names
        file_lines = [line.strip() for line in out.split('\n') if "GAME.01-FOO" in line and ".TXT" in line]
        self.assertEqual(len(file_lines), 2, f"Expected 2 files, but found: {file_lines}")
        
        # Verify the files have different names
        self.assertNotEqual(file_lines[0], file_lines[1], "The two files should have different names")
        
        # One should be the regular file, one should contain \x01
        regular_line = None
        null_line = None
        for line in file_lines:
            if "<NUL>" in line:
                null_line = line
            elif line == "GAME.01-FOO.TXT":
                regular_line = line
        
        self.assertIsNotNone(regular_line, f"Could not find regular file in listing: {file_lines}")
        self.assertIsNotNone(null_line, f"Could not find file with embedded NUL in listing: {file_lines}")
        
        # Extract all files and verify content
        extract_dir = self.tmp / "extract_null_test"
        extract_dir.mkdir()
        code, out, err = run_cpaktool(["extract", str(pak)], cwd=extract_dir)
        self.assertEqual(code, 0, f"Failed to extract files: {err}")
        
        # Check that we have exactly 2 extracted files
        extracted_files = list(extract_dir.glob("GAME.01-FOO*"))
        self.assertEqual(len(extracted_files), 2, f"Expected 2 extracted files, found: {extracted_files}")
        
        # Find the regular file and the one with embedded NUL
        regular_file = None
        null_file = None
        
        for f in extracted_files:
            if "%00" in f.name:
                null_file = f
            elif f.name == "GAME.01-FOO.TXT":
                regular_file = f
        
        # Verify we found both files
        self.assertIsNotNone(regular_file, "Could not find regular file after extraction")
        self.assertIsNotNone(null_file, "Could not find file with embedded NUL after extraction")
        
        # Verify content is correct
        self.assertEqual(regular_file.read_text(), content1)
        self.assertEqual(null_file.read_text(), content2)
        
        # Test that we can delete them individually
        # First, let's extract again to a fresh directory for deletion test
        extract_dir2 = self.tmp / "extract_delete_test"
        extract_dir2.mkdir()
        code, out, err = run_cpaktool(["extract", str(pak)], cwd=extract_dir2)
        self.assertEqual(code, 0)
        
        # Try to delete just the regular file (case-sensitive match)
        code, out, err = run_cpaktool(["delete", str(pak), "GAME.01-FOO.TXT"])
        self.assertEqual(code, 0, f"Failed to delete regular file: {err}")
        
        # Verify only the file with embedded NUL remains
        code, out, err = run_cpaktool(["list", "-1", str(pak)])
        self.assertEqual(code, 0)
        self.assertNotIn("GAME.01-FOO.TXT", out)
        
        # There should still be 1 file remaining
        file_lines = [line.strip() for line in out.split('\n') if "GAME.01-FOO" in line and ".TXT" in line]
        self.assertEqual(len(file_lines), 1, f"Expected 1 file remaining, but found: {file_lines}")
        
        # The remaining file should be the one with <NUL>
        self.assertIn("<NUL>", file_lines[0], f"Remaining file should contain <NUL>: {file_lines[0]}")
        
        # Final integrity check
        code, out, err = run_cpaktool(["test", str(pak)])
        self.assertEqual(code, 0, f"Pak integrity check failed: {err}")


    def test_crc_error_handling(self):
        """Test --crc option shows <error> for corrupted files"""
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
        
        # Manually corrupt the pak by corrupting the first_page field in a note
        # For 64KB pak: 256 pages, FAT = 2 pages, layout = ID(1) + FAT1(2) + FAT2(2) + notes 
        # Note table starts at page 5 = offset 0x500
        # Each note is 32 bytes: gamecode[4] + pubcode[2] + first_page[2] + other[24]
        # So first_page is at offset 6 within each note
        pak_data = pak.read_bytes()
        corrupted_data = bytearray(pak_data)
        
        # Corrupt the first_page field of the second note (SAVE.01-data.bin)
        # Notes are ordered by their index, second note is at offset 32
        note_table_offset = 0x500  # Page 5
        second_note_offset = note_table_offset + 32  # Second note
        first_page_offset = second_note_offset + 6   # first_page field
        
        # Set first_page to an invalid value (bank=255, page=255) 
        corrupted_data[first_page_offset] = 0xFF     # bank = 255 (invalid)
        corrupted_data[first_page_offset + 1] = 0xFF # page = 255 (invalid)
        
        # Write corrupted data back
        pak.write_bytes(corrupted_data)
        
        # Test --crc with corrupted file - should show <error> and continue  
        code, out, err = run_cpaktool(["list", "--crc", str(pak)])
        self.assertEqual(code, 0, f"Failed to list corrupted pak: {err}")
        self.assertIn("CRC32", out)
        self.assertIn("<error>", out)  # Should show error for corrupted file
        
        # Verify other files still show correct CRC values
        import zlib
        self.assertIn(f"{zlib.crc32(test_files[0][1]) & 0xffffffff:08X}", out)  # test1.txt should work
        self.assertIn(f"{zlib.crc32(test_files[2][1]) & 0xffffffff:08X}", out)  # empty.dat should work

        # Verify that the pak is marked as corrupted by fsck
        code, out, err = run_cpaktool(["test", str(pak)])
        self.assertNotEqual(code, 0, f"fsck should fail on corrupted pak: {err}")
        self.assertIn("invalid first page", out)

    def test_brute_force_add_remove_cycles(self):
        """Brute force test with random add/remove file operations"""
        import random
        import zlib
        import time
        
        # Set seed for reproducible tests
        random.seed(1)
        
        # Create 1MB pak (1024KB = 32 banks)
        pak = self._create_pak("1024")
        
        # Track files currently in the pak
        # Format: {filename: (size, crc32, content)}
        current_files = {}
        
        # Available game codes to use for variety
        game_codes = ["GAME.01", "TEST.42", "DEMO.ZZ", "PLAY.99", "SAVE.00"]
        
        # File extensions to use
        extensions = ["TXT", "DAT", "BIN", "SAV", "CFG"]
        
        def get_pak_usage():
            """Get current pak usage in bytes (accounting for 256-byte padding)"""
            total_used = 0
            for filename, (size, _, _) in current_files.items():
                # Files are padded to 256-byte boundaries
                padded_size = ((size + 255) // 256) * 256
                total_used += padded_size
            return total_used
        
        def get_available_space():
            """Get available space in bytes"""
            # 1MB pak has about 1MB - overhead for FAT and note tables
            # Roughly 1024*1024 - 16*256 = ~1044480 - 4096 = ~1040384 bytes
            max_capacity = 1024 * 1024 - 16 * 256  # Conservative estimate
            return max_capacity - get_pak_usage()
        
        def generate_filename():
            """Generate a random valid filename"""
            game_code = random.choice(game_codes)
            # Limit base name to 8 characters to be safe with 16-char limit
            base_name = ''.join(random.choices("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", k=random.randint(6, 10)))
            ext = random.choice(extensions)
            return f"{game_code}-{base_name}.{ext}"
        
        def generate_content(size):
            """Generate random content of specified size"""
            return bytes(random.randint(0, 255) for _ in range(size))
        
        def verify_pak_integrity():
            """Verify pak filesystem integrity and CRC values"""
            # Test filesystem
            code, out, err = run_cpaktool(["test", "--level", "INFO", str(pak)])
            if code != 0:
                self.fail(f"Filesystem integrity check failed: {err}\nStdout: {out}")
            
            # List with CRC using JSON format for reliable parsing
            code, out, err = run_cpaktool(["list", "--json", "--crc", str(pak)])
            if code != 0:
                self.fail(f"Failed to list files with CRC: {err}")

            # Parse JSON output and verify CRCs
            try:
                import json
                files_data = json.loads(out)
                actual_files = {f["full_name"]: int(f["crc32"], 16) for f in files_data}

            except (json.JSONDecodeError, KeyError, TypeError) as e:
                self.fail(f"Failed to parse JSON output: {e}\nOutput: {out}")
            
            # Verify all expected files are present with correct CRCs
            for filename, (size, expected_crc, content) in current_files.items():
                # Convert our filename to the expected format
                # Our format: GAME.PB-name.ext -> GAME.PB-NAME.EXT (uppercase)
                expected_filename = filename.upper()
                
                if expected_filename not in actual_files:
                    available_files = list(actual_files.keys())
                    self.fail(f"File {expected_filename} not found in pak listing. Available files: {available_files}")
                
                actual_crc = actual_files.pop(expected_filename)
                if actual_crc != expected_crc:
                    self.fail(f"CRC mismatch for {expected_filename}: expected {expected_crc:08X}, got {actual_crc:08X}")
        
            if actual_files:
                available_files = list(actual_files.keys())
                self.fail(f"Extra files found in the pak that shouldn't be there: {available_files}")

        # Perform 10 cycles: fill to 100%, empty to 75%
        cycles = 9
        
        for cycle in range(cycles):
            #print(f"Starting cycle {cycle + 1}/{cycles}")
            
            # Fill phase: add files until ~100% full
            while get_available_space() > 512:  # Keep small margin for filesystem overhead
                # Choose file size that fits in available space
                available = get_available_space()
                max_file_size = min(512 * 1024, available - 512)  # Leave 512 bytes margin
                
                if max_file_size < 1:
                    break
                
                file_size = random.randint(1, max_file_size)
                filename = generate_filename()
                content = generate_content(file_size)
                crc32 = zlib.crc32(content) & 0xffffffff
                
                # Write file to disk
                test_file = self.tmp / filename
                test_file.write_bytes(content)
                
                # Add to pak
                #print(f"Adding {filename} ({file_size} bytes, CRC32: {crc32:08X})")
                code, out, err = run_cpaktool(["add", str(pak), str(test_file)])
                if code != 0:
                    #print(f"Failed to add {filename}: {err}")
                    test_file.unlink()  # Clean up
                    if "no space left" in err.lower() or "too many files" in err.lower():
                        break
                    else:
                        self.fail(f"Failed to add {filename}: {err}")
                
                # Update tracking
                current_files[filename] = (file_size, crc32, content)
                
                # Clean up temp file
                test_file.unlink()
                
                # Verify integrity after each add
                verify_pak_integrity()
            
            #print(f"Cycle {cycle + 1}: Filled to {get_pak_usage()} bytes")
            if cycle == cycles - 1:
                #print("Final cycle reached, stopping add phase.")
                break

            # Empty phase: remove files until ~75% full
            target_usage = int(get_pak_usage() * 0.75)
            
            while get_pak_usage() > target_usage and current_files:
                # Pick random file to delete
                filename = random.choice(list(current_files.keys()))
                
                # Delete from pak
                #print(f"Deleting {filename} from pak")
                code, out, err = run_cpaktool(["delete", str(pak), filename])
                if code != 0:
                    self.fail(f"Failed to delete {filename}: {err}")
                
                # Remove from tracking
                del current_files[filename]
                
                # Verify integrity after each delete
                verify_pak_integrity()
            
            #print(f"Cycle {cycle + 1}: Emptied to {get_pak_usage()} bytes")
        
        # Final verification
        verify_pak_integrity()
        
        #print(f"Brute force test completed successfully")
        #print(f"Final state: {len(current_files)} files, {get_pak_usage()} bytes used")
        
        # Uncomment to copy pak to /tmp for manual inspection
        #import shutil
        #shutil.copy2(str(pak), "/tmp/cpaktool_bruteforce_test.pak")
        #print(f"Pak copied to /tmp/cpaktool_bruteforce_test.pak for inspection")

    def test_filename_sanitization_roundtrip(self):
        """Test complete filename sanitization roundtrip: sanitized files → pak → extract → compare"""
        import json
        
        # Test cases organized by category - each group will be its own subtest with separate pak
        test_groups = {
            "windows_reserved_names": [
                # Windows reserved names (first character gets percent-encoded)
                ("%43ON.TXT", "CON.TXT"),
                ("%50RN.DAT", "PRN.DAT"), 
                ("%41UX.BIN", "AUX.BIN"),
                ("%4EUL.CFG", "NUL.CFG"),
                ("%43OM1.SAV", "COM1.SAV"),
                ("%43OM9.TMP", "COM9.TMP"),
                ("%4CPT1.LOG", "LPT1.LOG"),
                ("%4CPT9.BAK", "LPT9.BAK"),
            ],
            "path_separators": [
                # Path separators and problematic characters
                ("FILE%2FSLASH.TXT", "FILE/SLASH.TXT"),
                # Note: backslash (\), dollar ($), and many others are not in N64 codepage
                ("FILE%3FQUEST.SAV", "FILE?QUEST.SAV"),
                ("FILE%2ASTAR.TMP", "FILE*STAR.TMP"), 
                ("FILE%22QUOTE.LOG", "FILE\"QUOTE.LOG"),
                ("FILE%3ACOLON.BAK", "FILE:COLON.BAK"),
                ("FILE%21EXCL.BIN", "FILE!EXCL.BIN"),
            ],
            "trailing_chars": [
                # Trailing spaces and dots (problematic on Windows)
                ("FILE%20.TXT", "FILE .TXT"),
                ("FILE%20%20.DAT", "FILE  .DAT"),
                ("NAME%2E.BIN", "NAME..BIN"),
                ("NAME%2E%2E.CFG", "NAME...CFG"),
                ("BOTH%20%2E.SAV", "BOTH ..SAV"),
                ("MIXED%2E%20.TMP", "MIXED. .TMP"),
                ("COMBO%20%2E%20.LOG", "COMBO . .LOG"),
                ("FINAL%2E%2E%20.BAK", "FINAL.. .BAK"),
            ],
            "control_chars": [
                # Control characters and special cases
                ("FILE%00NULL.TXT", "FILE\x00NULL.TXT"),
                ("TWO%00%00NULL.DAT", "TWO\x00\x00NULL.DAT"),
            ],
            "complex_cases": [
                # Complex combinations of multiple problematic characters
                ("%43ON%20%2E.TXT", "CON ..TXT"),  # Reserved name + trailing space + dot
                ("FILE%2F%3F%2A.DAT", "FILE/?*.DAT"),  # Multiple path separators
                ("NAME%00%20%2E.BIN", "NAME\x00 ..BIN"),  # NUL + space + dot
                ("%50RN%2FDIR%2A.CFG", "PRN/DIR*.CFG"),  # Reserved + paths
                ("LONG%2F%3D%2B.SAV", "LONG/=+.SAV"),  # Multiple special chars
                ("MIX%00%2F%20%2E.TMP", "MIX\x00/ ..TMP"),  # Mixed everything
                ("%41UX%40%3F%2A.LOG", "AUX@?*.LOG"),  # Reserved + symbols
                ("END%22%2C%2E%3A.BAK", "END\",.:.BAK"),  # Quote + comparison + pipe
            ]
        }
        
        for group_name, test_cases in test_groups.items():
            with self.subTest(group=group_name):
                self._test_sanitization_group(test_cases, group_name)
    
    def _test_sanitization_group(self, test_cases, group_name):
        """Test a specific group of sanitization cases"""
        import json
        
        pak = self._create_pak("1024")  # Large pak for all test files
        
        # Create a directory with sanitized filenames
        source_dir = self.tmp / f"source_{group_name}"
        source_dir.mkdir()
        
        # Step 1: Create pre-sanitized files on disk
        original_contents = {}
        for sanitized_filename, original_name in test_cases:
            content = f"Content for {original_name} in {group_name}".encode('utf-8')
            original_contents[original_name] = content
            
            source_file = source_dir / sanitized_filename
            source_file.write_bytes(content)
        
        # Step 2: Add all files to pak (de-sanitization will happen automatically)
        for sanitized_filename, original_name in test_cases:
            source_file = source_dir / sanitized_filename
            code, out, err = run_cpaktool(["add", str(pak), str(source_file)])
            self.assertEqual(code, 0, f"Failed to add {sanitized_filename}:\n{err}")
        
        # Step 3: Verify pak contains de-sanitized names
        code, out, err = run_cpaktool(["list", "--json", str(pak)])
        self.assertEqual(code, 0, f"Failed to list pak contents: {err}")
        
        try:
            files_data = json.loads(out)
        except json.JSONDecodeError as e:
            self.fail(f"Failed to parse JSON output: {e}\nOutput: {out[:500]}...")
        
        pak_filenames = [f["full_name"] for f in files_data]
        self.assertEqual(len(pak_filenames), len(test_cases), 
                        f"Expected {len(test_cases)} files in pak, got {len(pak_filenames)}")
        
        # Verify each expected pak name is present (de-sanitized, with DRAG.ON prefix, uppercase)
        expected_pak_names = set()
        for sanitized_filename, expected_pak_name in test_cases:
            # The pak should contain the de-sanitized version with DRAG.ON- prefix, uppercase
            pak_display_name = ("DRAG.ON-" + expected_pak_name).upper()
            expected_pak_names.add(pak_display_name)
        
        actual_pak_names = set(pak_filenames)
        self.assertEqual(actual_pak_names, expected_pak_names,
                        f"Pak should contain de-sanitized names.\nExpected: {expected_pak_names}\nActual: {actual_pak_names}")
        
        # Step 4: Extract to new directory (should re-sanitize)
        extract_dir = self.tmp / f"extracted_{group_name}"
        extract_dir.mkdir()
        
        code, out, err = run_cpaktool(["extract", str(pak)], cwd=extract_dir)
        self.assertEqual(code, 0, f"Failed to extract pak: {err}")
        
        # Step 5: Verify content integrity
        source_files = {f.name: f.read_bytes() for f in source_dir.iterdir() if f.is_file()}
        extracted_files = {f.name: f.read_bytes() for f in extract_dir.iterdir() if f.is_file()}
        
        self.assertEqual(len(source_files), len(extracted_files),
                        f"Expected {len(source_files)} extracted files, got {len(extracted_files)}")
        
        # Create mapping from original names to what was actually extracted
        extracted_name_mapping = {}
        for extracted_name in extracted_files.keys():
            # Remove DRAG.ON- prefix to get the extracted filename
            if extracted_name.startswith("DRAG.ON-"):
                clean_name = extracted_name[8:]  # Remove "DRAG.ON-"
                extracted_name_mapping[clean_name] = extracted_name
        
        # Verify each file has the correct content, accounting for actual sanitization behavior
        verified_files = 0
        for sanitized_filename, original_name in test_cases:
            # Find the corresponding extracted file by content match
            found_extracted = None
            for clean_name, full_extracted_name in extracted_name_mapping.items():
                if full_extracted_name in extracted_files:
                    extracted_content = extracted_files[full_extracted_name]
                    expected_content = original_contents[original_name]
                    # Remove cpakfs padding
                    extracted_content = extracted_content[:len(expected_content)]
                    
                    if extracted_content == expected_content:
                        found_extracted = full_extracted_name
                        verified_files += 1
                        break
            
            self.assertIsNotNone(found_extracted, 
                               f"Could not find extracted file with correct content for {original_name}")
        
        # Step 6: Verify pak integrity
        code, out, err = run_cpaktool(["test", str(pak)])
        self.assertEqual(code, 0, f"Pak integrity check failed: {err}")

    def test_reserved_gamecode_sanitization(self):
        """Test sanitization when the gamecode itself is a Windows reserved name"""
        import json
        
        pak = self._create_pak("1024")
        
        # Test cases where the gamecode is a Windows reserved name
        # Format: (input_filename, expected_cpak_name, expected_extracted_name)
        test_cases = [
            # LPT1.WN should become %4CPT1.WN in extracted filename
            ("TEST1.TXT", "LPT1.WN-TEST1.TXT", "%4CPT1.WN-TEST1.TXT"),
            ("DATA.BIN", "LPT1.WN-DATA.BIN", "%4CPT1.WN-DATA.BIN"),
            
            # COM1.ZZ should become %43OM1.ZZ in extracted filename  
            ("SAVE.DAT", "COM1.ZZ-SAVE.DAT", "%43OM1.ZZ-SAVE.DAT"),
            ("CONFIG.CFG", "COM1.ZZ-CONFIG.CFG", "%43OM1.ZZ-CONFIG.CFG"),
            
            # LPT2.AA should become %4CPT2.AA in extracted filename
            ("PRINT.LOG", "LPT2.AA-PRINT.LOG", "%4CPT2.AA-PRINT.LOG"),
            
            # COM2.BB should become %43OM2.BB in extracted filename
            ("SERIAL.TXT", "COM2.BB-SERIAL.TXT", "%43OM2.BB-SERIAL.TXT"),
        ]
        
        # Step 1: Create files and add them with specific gamecodes
        source_dir = self.tmp / "source_reserved_gamecode"
        source_dir.mkdir()
        
        original_contents = {}
        for input_filename, expected_cpak_name, expected_extracted_name in test_cases:
            content = f"Content for {input_filename} with reserved gamecode".encode('utf-8')
            original_contents[expected_cpak_name] = content
            
            # Create file on disk
            source_file = source_dir / input_filename
            source_file.write_bytes(content)
            
            # Extract gamecode from expected_cpak_name for --gamecode option
            gamecode = expected_cpak_name.split('-')[0]  # e.g., "LPT1.WN"
            
            # Add to pak with specific gamecode
            code, out, err = run_cpaktool(["add", "--gamecode", gamecode, str(pak), str(source_file)])
            self.assertEqual(code, 0, f"Failed to add {input_filename} with gamecode {gamecode}:\n{err}")
        
        # Step 2: Verify pak contains the expected cpak names (without percent encoding)
        code, out, err = run_cpaktool(["list", "--json", str(pak)])
        self.assertEqual(code, 0, f"Failed to list pak contents: {err}")
        
        try:
            files_data = json.loads(out)
        except json.JSONDecodeError as e:
            self.fail(f"Failed to parse JSON output: {e}\nOutput: {out[:500]}...")
        
        pak_filenames = [f["full_name"] for f in files_data]
        self.assertEqual(len(pak_filenames), len(test_cases), 
                        f"Expected {len(test_cases)} files in pak, got {len(pak_filenames)}")
        
        # Verify each expected cpak name is present (uppercase)
        expected_pak_names = set()
        for _, expected_cpak_name, _ in test_cases:
            expected_pak_names.add(expected_cpak_name.upper())
        
        actual_pak_names = set(pak_filenames)
        self.assertEqual(actual_pak_names, expected_pak_names,
                        f"Pak should contain expected cpak names.\nExpected: {expected_pak_names}\nActual: {actual_pak_names}")
        
        # Step 3: Extract files and verify they have percent-encoded gamecodes
        extract_dir = self.tmp / "extracted_reserved_gamecode"
        extract_dir.mkdir()
        
        code, out, err = run_cpaktool(["extract", str(pak)], cwd=extract_dir)
        self.assertEqual(code, 0, f"Failed to extract pak: {err}")
        
        # Step 4: Verify extracted filenames have percent-encoded gamecodes
        extracted_files = {f.name: f.read_bytes() for f in extract_dir.iterdir() if f.is_file()}
        
        self.assertEqual(len(extracted_files), len(test_cases),
                        f"Expected {len(test_cases)} extracted files, got {len(extracted_files)}")
        
        # Verify each expected extracted filename is present
        expected_extracted_names = set()
        for _, _, expected_extracted_name in test_cases:
            expected_extracted_names.add(expected_extracted_name.upper())
        
        actual_extracted_names = set(extracted_files.keys())
        self.assertEqual(actual_extracted_names, expected_extracted_names,
                        f"Extracted files should have percent-encoded gamecodes.\nExpected: {expected_extracted_names}\nActual: {actual_extracted_names}")
        
        # Step 5: Verify content integrity
        verified_files = 0
        for _, expected_cpak_name, expected_extracted_name in test_cases:
            extracted_filename = expected_extracted_name.upper()
            if extracted_filename in extracted_files:
                extracted_content = extracted_files[extracted_filename]
                expected_content = original_contents[expected_cpak_name]
                # Remove cpakfs padding
                extracted_content = extracted_content[:len(expected_content)]
                
                self.assertEqual(extracted_content, expected_content,
                               f"Content mismatch for {extracted_filename}")
                verified_files += 1
        
        self.assertEqual(verified_files, len(test_cases),
                        f"Expected to verify {len(test_cases)} files, but only verified {verified_files}")
        
        # Step 6: Verify pak integrity
        code, out, err = run_cpaktool(["test", str(pak)])
        self.assertEqual(code, 0, f"Pak integrity check failed: {err}")

if __name__ == "__main__":
    unittest.main()
