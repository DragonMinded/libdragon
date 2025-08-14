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
        commands = ["format", "test"]
        
        for cmd in commands:
            with self.subTest(command=cmd):
                # Test help
                code, out, err = run_cpaktool([cmd, "--help"])
                self.assertEqual(code, 0)
                self.assertIn("Usage:", out)
                self.assertIn(cmd, out)
                
                # Test missing argument
                code, out, err = run_cpaktool([cmd])
                self.assertNotEqual(code, 0)
                self.assertIn(f"{cmd} command requires exactly one argument", err)
                
                # Test extra arguments
                code, out, err = run_cpaktool([cmd, str(self.pak), "extra"])
                self.assertNotEqual(code, 0)
                self.assertIn(f"{cmd} command requires exactly one argument", err)

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

        # Test --banks option  
        pak2 = self.tmp / "test2.pak"
        code, out, err = run_cpaktool(["format", "--banks", "2", str(pak2)])
        self.assertEqual(code, 0)
        self.assertTrue(pak2.exists())

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

    def test_error_cases(self):
        """Test invalid command and no command cases"""
        # Invalid command
        code, out, err = run_cpaktool(["invalidcommand"])
        self.assertNotEqual(code, 0)
        
        # No command provided
        code, out, err = run_cpaktool([])
        self.assertNotEqual(code, 0)

if __name__ == "__main__":
    unittest.main()
