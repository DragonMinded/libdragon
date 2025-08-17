#!/usr/bin/env python3
"""
CLI Argument Parsing Tests for cpaktool

WHAT TO TEST:
- Option argument validation (e.g., --debug-bufsize must be positive)
- Conflicting options (e.g., --size and --banks are mutually exclusive)
- Complex parsing logic (e.g., human-readable size parsing, special formats)
- Required arguments and their validation
- Global option placement (before/after command)
- If an option requires a value, ensure it is correctly parsed with or without
  the equals sign (e.g., --option=value vs --option value)

WHAT NOT TO TEST:
- Simple option acceptance (if tool accepts -l, --long without validation)
- Basic help output (unless specific format requirements)
- File existence errors (these are functional, not CLI parsing issues)
- Options that just set boolean flags without validation
- Aliases of commands (unless they have unique parsing logic)
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

class TestCLI(unittest.TestCase):
    """Test CLI argument parsing and option handling for all commands"""
    
    def setUp(self):
        self.tmpdir = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmpdir.cleanup)
        self.tmp = Path(self.tmpdir.name)
        self.pak = self.tmp / "test.pak"

    # Global help and version tests
    def test_global_options(self):
        """Test global help and version options"""
        code, out, err = run_cpaktool(["--help"])
        self.assertEqual(code, 0)
        self.assertIn("cpaktool - Controller Pak manipulation tool", out)

        code, out, err = run_cpaktool(["--version"])
        self.assertEqual(code, 0)
        self.assertIn("cpaktool 1.0", out)

    # Test command help and argument validation
    def test_command_help_and_args(self):
        """Test command help and argument validation"""
        commands = ["format", "test", "list"]
        
        for cmd in commands:
            with self.subTest(command=cmd):
                # Test help
                code, out, err = run_cpaktool([cmd, "--help"])
                self.assertEqual(code, 0)
                self.assertIn("Usage:", out)
                self.assertIn(cmd, out)
                
                # Test missing argument (except list which can work without patterns)
                if cmd != "list":
                    code, out, err = run_cpaktool([cmd])
                    self.assertNotEqual(code, 0)
                    self.assertIn(f"{cmd} command requires exactly one argument", err)
                    
                    # Test extra arguments
                    code, out, err = run_cpaktool([cmd, str(self.pak), "extra"])
                    self.assertNotEqual(code, 0)
                    self.assertIn(f"{cmd} command requires exactly one argument", err)

    def test_delete_command_args(self):
        """Test delete command argument validation"""
        # Test help
        code, out, err = run_cpaktool(["delete", "--help"])
        self.assertEqual(code, 0)
        self.assertIn("Usage:", out)
        self.assertIn("delete", out)
        
        # Test missing arguments
        code, out, err = run_cpaktool(["delete"])
        self.assertNotEqual(code, 0)
        self.assertIn("delete command requires at least two arguments", err)
        
        # Test only pak file argument (missing pattern)
        code, out, err = run_cpaktool(["delete", str(self.pak)])
        self.assertNotEqual(code, 0)
        self.assertIn("delete command requires at least two arguments", err)

    # Test global options can be placed before or after command
    def test_verbose_option_placement(self):
        """Test -v option before and after command name"""
        # Before command
        code, out, err = run_cpaktool(["-v", "format", str(self.pak)])
        self.assertIsInstance(code, int)
        
        code, out, err = run_cpaktool(["-v", "test"])
        self.assertNotEqual(code, 0)
        self.assertIn("test command requires exactly one argument", err)
        
        # After command  
        code, out, err = run_cpaktool(["format", "-v", str(self.pak)])
        self.assertIsInstance(code, int)
        
        code, out, err = run_cpaktool(["test", "-v"])
        self.assertNotEqual(code, 0)
        self.assertIn("test command requires exactly one argument", err)

    # Test command-specific options
    def test_format_options(self):
        """Test format command options"""
        # Test --size option
        code, out, err = run_cpaktool(["format", "--size", "64", str(self.pak)])
        self.assertEqual(code, 0)
        self.assertTrue(self.pak.exists())

        # Test --size option with = format
        pak_eq = self.tmp / "test_eq.pak"
        code, out, err = run_cpaktool(["format", "--size=64", str(pak_eq)])
        self.assertEqual(code, 0)
        self.assertTrue(pak_eq.exists())

        # Test --banks option  
        pak2 = self.tmp / "test2.pak"
        code, out, err = run_cpaktool(["format", "--banks", "2", str(pak2)])
        self.assertEqual(code, 0)
        self.assertTrue(pak2.exists())

        # Test --banks option with = format
        pak2_eq = self.tmp / "test2_eq.pak"
        code, out, err = run_cpaktool(["format", "--banks=2", str(pak2_eq)])
        self.assertEqual(code, 0)
        self.assertTrue(pak2_eq.exists())

        # Test short options work
        pak3 = self.tmp / "test3.pak"
        code, out, err = run_cpaktool(["format", "-s", "64", str(pak3)])
        self.assertEqual(code, 0)

        pak4 = self.tmp / "test4.pak"
        code, out, err = run_cpaktool(["format", "-b", "2", str(pak4)])
        self.assertEqual(code, 0)

    def test_format_size_and_banks_mutual_exclusion(self):
        """Test mutual exclusion between --size and --banks options"""
        conflicts = [
            ["--size", "32", "--banks", "2"],
            (["-s", "64", "-b", "2"]),
            ["--size", "32", "-b", "1"]
        ]
        
        for i, args in enumerate(conflicts):
            pak = self.tmp / f"conflict{i}.pak"
            code, out, err = run_cpaktool(["format"] + args + [str(pak)])
            self.assertNotEqual(code, 0)
            self.assertIn("Cannot specify both", err)

    def test_format_valid_usage(self):
        """Test valid --size and --banks usage"""
        # Test size values
        for size in ["32", "128"]:
            pak = self.tmp / f"size_{size}.pak"
            code, out, err = run_cpaktool(["format", "--size", size, str(pak)])
            self.assertEqual(code, 0)
        
        # Test bank values
        for banks in ["1", "4"]:
            pak = self.tmp / f"banks_{banks}.pak"
            code, out, err = run_cpaktool(["format", "--banks", banks, str(pak)])
            self.assertEqual(code, 0)

    def test_test_command_options(self):
        """Test test command options"""
        # Test -r and --repair options (will fail due to missing pak but should parse)
        code, out, err = run_cpaktool(["test", "-r"])
        self.assertNotEqual(code, 0)
        self.assertIn("test command requires exactly one argument", err)

        code, out, err = run_cpaktool(["test", "--repair"])
        self.assertNotEqual(code, 0)
        self.assertIn("test command requires exactly one argument", err)
        
        # Create a test pak to verify --level option parsing works
        pak = self.tmp / "test_level.pak"
        code, out, err = run_cpaktool(["format", str(pak)])
        self.assertEqual(code, 0)
        
        # Test --level with space format
        code, out, err = run_cpaktool(["test", "--level", "INFO", str(pak)])
        self.assertEqual(code, 0)
        
        # Test --level with = format
        code, out, err = run_cpaktool(["test", "--level=INFO", str(pak)])
        self.assertEqual(code, 0)
        
        # Test different level values with = format
        for level in ["WARNING", "ERROR"]:
            code, out, err = run_cpaktool(["test", f"--level={level}", str(pak)])
            self.assertEqual(code, 0)

    def test_add_extract_option_validation(self):
        """Test ADD and EXTRACT option validation"""
        # ADD: invalid debug-bufsize
        code, out, err = run_cpaktool(["add", "--debug-bufsize", "0", str(self.pak), "file.txt"])
        self.assertNotEqual(code, 0)
        self.assertIn("Buffer size must be positive", err)
        
        # ADD: invalid debug-bufsize with = format
        code, out, err = run_cpaktool(["add", "--debug-bufsize=0", str(self.pak), "file.txt"])
        self.assertNotEqual(code, 0)
        self.assertIn("Buffer size must be positive", err)
        
        code, out, err = run_cpaktool(["add", "--debug-bufsize", "-5", str(self.pak), "file.txt"]) 
        self.assertNotEqual(code, 0)
        
        code, out, err = run_cpaktool(["add", "--debug-bufsize=-5", str(self.pak), "file.txt"]) 
        self.assertNotEqual(code, 0)
        
        # EXTRACT: invalid debug-bufsize
        code, out, err = run_cpaktool(["extract", "--debug-bufsize", "0", str(self.pak)])
        self.assertNotEqual(code, 0)
        self.assertIn("Buffer size must be positive", err)
        
        # EXTRACT: invalid debug-bufsize with = format
        code, out, err = run_cpaktool(["extract", "--debug-bufsize=0", str(self.pak)])
        self.assertNotEqual(code, 0)
        self.assertIn("Buffer size must be positive", err)
        
        # ADD: gamecode format (should accept any format, not validated at CLI level)
        code, out, err = run_cpaktool(["add", "--gamecode", "ABCD.EF", str(self.pak), "file.txt"])
        self.assertNotEqual(code, 0)  # Will fail because file doesn't exist, but option is parsed
        self.assertNotIn("gamecode", err)  # Should not be a gamecode error
        
        # ADD: gamecode format with = format
        code, out, err = run_cpaktool(["add", "--gamecode=ABCD.EF", str(self.pak), "file.txt"])
        self.assertNotEqual(code, 0)  # Will fail because file doesn't exist, but option is parsed
        self.assertNotIn("gamecode", err)  # Should not be a gamecode error

    def test_list_options(self):
        """Test list command option validation"""
        # Test list command requires pak file argument
        code, out, err = run_cpaktool(["list"])
        self.assertNotEqual(code, 0)
        self.assertIn("list command requires at least one argument", err)
        
        # Test --sort option requires value
        code, out, err = run_cpaktool(["list", "--sort"])
        self.assertNotEqual(code, 0)
        self.assertIn("Option --sort requires a value", err)
        
        # Create a test pak to verify --sort option parsing works with = format
        pak = self.tmp / "test_sort.pak"
        code, out, err = run_cpaktool(["format", str(pak)])
        self.assertEqual(code, 0)
        
        # Test --sort with space format
        code, out, err = run_cpaktool(["list", "--sort", "name", str(pak)])
        self.assertEqual(code, 0)
        
        # Test --sort with = format  
        code, out, err = run_cpaktool(["list", "--sort=name", str(pak)])
        self.assertEqual(code, 0)
        
        # Test --sort with = format for different sort options
        for sort_opt in ["size", "date"]:
            code, out, err = run_cpaktool(["list", f"--sort={sort_opt}", str(pak)])
            self.assertEqual(code, 0)

    def test_crc_implies_long(self):
        """Test that --crc option implies --long format"""
        # Create a pak for testing
        pak = self.tmp / "test.pak"
        code, out, err = run_cpaktool(["format", "--size", "32", str(pak)])
        self.assertEqual(code, 0)
        
        # Test --crc without any files should show header (indicating --long format)
        code, out, err = run_cpaktool(["list", "--crc", str(pak)])
        self.assertEqual(code, 0)
        # Long format should include column headers with CRC32
        self.assertIn("Game", out)
        self.assertIn("Pub", out)
        self.assertIn("Filename", out)
        self.assertIn("CRC32", out)

    def test_error_cases(self):
        """Test invalid command and no command cases"""
        # Invalid command
        code, out, err = run_cpaktool(["invalidcommand"])
        self.assertNotEqual(code, 0)
        
        # No command provided
        code, out, err = run_cpaktool([])
        self.assertNotEqual(code, 0)

    def test_convert_command_options(self):
        """Test convert command options with = format"""
        # Create test files for convert command
        input_pak = self.tmp / "input.pak"
        output_pak = self.tmp / "output.pak"
        
        code, out, err = run_cpaktool(["format", str(input_pak)])
        self.assertEqual(code, 0)
        
        # Test --from and --to with = format (will fail due to unsupported formats, but should parse)
        code, out, err = run_cpaktool(["convert", "--from=raw", "--to=dexdrive", str(input_pak), str(output_pak)])
        # The command should parse the options correctly, even if it fails later
        self.assertNotIn("Option --from requires a value", err)
        self.assertNotIn("Option --to requires a value", err)

    def test_skip_header_option(self):
        """Test --skip-header option parsing and validation"""
        # Create a minimal pak file for testing
        pak_path = self.tmp / "test_skip.pak"
        code, out, err = run_cpaktool(["format", str(pak_path)])
        self.assertEqual(code, 0, f"Failed to create test pak: {err}")
        
        # Test valid --skip-header with 0, which should work
        code, out, err = run_cpaktool(["--skip-header", "0", "list", str(pak_path)])
        self.assertEqual(code, 0, f"--skip-header 0 should work: {err}")

        # Test that non-zero skip values fail on a standard pak file
        for value in ["10", "100", "4160"]:
            with self.subTest(value=value):
                code, out, err = run_cpaktool(["--skip-header", value, "list", str(pak_path)])
                self.assertNotEqual(code, 0, f"--skip-header {value} should fail on a standard pak")
                self.assertIn("Not a valid Controller Pak file", err)
        
        # Test --skip-header with verbose to check logging
        code, out, err = run_cpaktool(["--verbose", "--skip-header", "0", "list", str(pak_path)])
        self.assertEqual(code, 0)
        self.assertIn("Skipping 0 header bytes (manual setting)", out)
        
        code, out, err = run_cpaktool(["--verbose", "--skip-header", "4160", "list", str(pak_path)])
        self.assertNotEqual(code, 0)
        self.assertIn("Skipping 4160 header bytes (manual setting)", out)
        
        # Test auto-detect (no --skip-header) - just verify it works
        code, out, err = run_cpaktool(["--verbose", "list", str(pak_path)])
        self.assertEqual(code, 0)
        # Just verify the listing works without error, no specific message expected

    def test_skip_header_validation(self):
        """Test --skip-header option validation"""
        pak_path = self.tmp / "test_skip2.pak"
        code, out, err = run_cpaktool(["format", str(pak_path)])
        self.assertEqual(code, 0, f"Failed to create test pak: {err}")
        
        # Test invalid values
        invalid_values = ["-1", "abc", "10.5", ""]
        for value in invalid_values:
            with self.subTest(value=value):
                if value == "":
                    # Missing value
                    code, out, err = run_cpaktool(["--skip-header", "list", str(pak_path)])
                else:
                    code, out, err = run_cpaktool(["--skip-header", value, "list", str(pak_path)])
                self.assertNotEqual(code, 0, f"--skip-header {value} should fail")
                self.assertIn("--skip-header", err)
        
        # Test missing value
        code, out, err = run_cpaktool(["--skip-header"])
        self.assertNotEqual(code, 0)
        self.assertIn("Option --skip-header requires a value", err)

    def test_skip_header_placement(self):
        """Test --skip-header option can be placed before or after command"""
        pak_path = self.tmp / "test_skip3.pak"
        code, out, err = run_cpaktool(["format", str(pak_path)])
        self.assertEqual(code, 0, f"Failed to create test pak: {err}")
        
        # Before command
        code, out, err = run_cpaktool(["--skip-header", "0", "list", str(pak_path)])
        self.assertEqual(code, 0, f"--skip-header before command should work: {err}")
        
        # After command
        code, out, err = run_cpaktool(["list", "--skip-header", "0", str(pak_path)])
        self.assertEqual(code, 0, f"--skip-header after command should work: {err}")
        
        # Test = format before command
        code, out, err = run_cpaktool(["--skip-header=0", "list", str(pak_path)])
        self.assertEqual(code, 0, f"--skip-header=0 before command should work: {err}")
        
        # Test = format after command  
        code, out, err = run_cpaktool(["list", "--skip-header=0", str(pak_path)])
        self.assertEqual(code, 0, f"--skip-header=0 after command should work: {err}")

    def test_dexdrive_autodetect(self):
        """Test DexDrive format auto-detection"""
        # Create a fake DexDrive file with the correct signature
        dexdrive_path = self.tmp / "dexdrive_test.n64"
        
        # Write DexDrive signature + header + minimal pak data
        with open(dexdrive_path, "wb") as f:
            # DexDrive signature
            f.write(b"123-456-STD\x00")
            # Pad to 0x1040 bytes (4160 decimal)
            f.write(b"\x00" * (0x1040 - 12))
            # Write a minimal formatted pak (32KB)
            # For simplicity, write zeros (we're testing header detection, not pak validity)
            f.write(b"\x00" * 32768)
        
        # Test auto-detection with verbose
        code, out, err = run_cpaktool(["--verbose", "list", str(dexdrive_path)])
        self.assertNotEqual(code, 0) # This is expected to fail because the pak data is invalid
        self.assertIn("Skipping 4160 header bytes (DexDrive format auto-detected)", out)
        
        # Test manual override of auto-detection
        code, out, err = run_cpaktool(["--verbose", "--skip-header", "0", "list", str(dexdrive_path)])
        self.assertNotEqual(code, 0) # This is also expected to fail
        self.assertIn("Skipping 0 header bytes (manual setting)", out)

if __name__ == "__main__":
    unittest.main()
