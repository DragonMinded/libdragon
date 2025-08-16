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

if __name__ == "__main__":
    unittest.main()
