#!/usr/bin/env python3

import unittest
import tempfile
import os
import shutil
import subprocess
from pathlib import Path
from unittest.mock import patch, MagicMock
import sys

# Add the current directory to the path so we can import update_authors
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import update_authors


class TestUpdateHeaders(unittest.TestCase):
    def setUp(self):
        """Set up test environment."""
        self.test_dir = tempfile.mkdtemp()
        self.original_cwd = os.getcwd()
        os.chdir(self.test_dir)
        
        # Initialize git repository for testing
        subprocess.run(['git', 'init'], check=True, capture_output=True)
        subprocess.run(['git', 'config', 'user.name', 'Test User'], check=True, capture_output=True)
        subprocess.run(['git', 'config', 'user.email', 'test@example.com'], check=True, capture_output=True)

    def tearDown(self):
        """Clean up test environment."""
        os.chdir(self.original_cwd)
        shutil.rmtree(self.test_dir)

    def create_test_file(self, filename, content):
        """Create a test file with given content and commit it to git."""
        filepath = Path(filename)
        filepath.parent.mkdir(parents=True, exist_ok=True)
        with open(filepath, 'w') as f:
            f.write(content)
        
        subprocess.run(['git', 'add', str(filepath)], check=True, capture_output=True)
        subprocess.run(['git', 'commit', '-m', f'Add {filename}'], check=True, capture_output=True)
        return filepath

    def test_normalize_author_github_masked_email(self):
        """Test GitHub masked email normalization."""
        name, email = update_authors.normalize_author("SpookyIluha", "127010686+SpookyIluha@users.noreply.github.com")
        self.assertEqual(name, "SpookyIluha")
        self.assertEqual(email, "https://github.com/SpookyIluha")

    def test_normalize_author_regular_email(self):
        """Test regular email normalization."""
        name, email = update_authors.normalize_author("John Doe", "john@example.com")
        self.assertEqual(name, "John Doe")
        self.assertEqual(email, "john@example.com")

    def test_normalize_author_author_map(self):
        """Test author map normalization."""
        name, email = update_authors.normalize_author("rasky", "some@email.com")
        self.assertEqual(name, "Giovanni Bajo")
        self.assertEqual(email, "giovannibajo@gmail.com")

    def test_is_assembly_file(self):
        """Test assembly file detection."""
        self.assertTrue(update_authors.is_assembly_file(Path("test.s")))
        self.assertTrue(update_authors.is_assembly_file(Path("test.inc")))
        self.assertFalse(update_authors.is_assembly_file(Path("test.c")))
        self.assertFalse(update_authors.is_assembly_file(Path("test.h")))

    def test_is_source_file(self):
        """Test source file detection."""
        self.assertTrue(update_authors.is_source_file(Path("test.c")))
        self.assertTrue(update_authors.is_source_file(Path("test.cpp")))
        self.assertTrue(update_authors.is_source_file(Path("test.h")))
        self.assertTrue(update_authors.is_source_file(Path("test.hpp")))
        self.assertTrue(update_authors.is_source_file(Path("test.s")))
        self.assertFalse(update_authors.is_source_file(Path("test.txt")))
        self.assertFalse(update_authors.is_source_file(Path("test.md")))

    def test_is_skipped_path(self):
        """Test path skipping logic."""
        # Test with a file that should be skipped
        with patch.object(update_authors, 'SKIP_PATHS', ['vendor*', '*.md']):
            # Mock os.path.isdir to return False for our test paths
            with patch('os.path.isdir', return_value=False):
                self.assertTrue(update_authors.is_skipped_path('vendor/something.c'))
                self.assertTrue(update_authors.is_skipped_path('README.md'))
                self.assertFalse(update_authors.is_skipped_path('src/main.c'))

    def test_extract_existing_brief(self):
        """Test extraction of existing @brief tags."""
        header_content = """
/**
 * @file test.c
 * @brief This is a test file
 * @author Test User <test@example.com>
 */
"""
        brief = update_authors.extract_existing_brief(header_content)
        self.assertEqual(brief, "This is a test file")

        # Test with no @brief
        header_content = """
/**
 * @file test.c
 * @author Test User <test@example.com>
 */
"""
        brief = update_authors.extract_existing_brief(header_content)
        self.assertIsNone(brief)

    def test_find_file_level_header(self):
        """Test finding existing file-level headers."""
        content = """
/**
 * @file test.c
 * @author Test User <test@example.com>
 */
int main() {
    return 0;
}
"""
        start, end, header = update_authors.find_file_level_header(content)
        self.assertEqual(start, 1)
        self.assertEqual(end, 5)
        self.assertIsNotNone(header)
        if header:
            self.assertIn("@file test.c", header)
            self.assertIn("@author Test User", header)

        # Test with no header
        content = """
int main() {
    return 0;
}
"""
        start, end, header = update_authors.find_file_level_header(content)
        self.assertIsNone(start)
        self.assertIsNone(end)
        self.assertIsNone(header)

    def test_create_doxygen_header(self):
        """Test Doxygen header creation."""
        authors = [("Test User", "test@example.com"), ("Another User", "another@example.com")]
        header = update_authors.create_doxygen_header("test.c", authors)
        
        self.assertIn("@file test.c", header)
        self.assertIn("@author Test User <test@example.com>", header)
        self.assertIn("@author Another User <another@example.com>", header)
        self.assertTrue(header.endswith(" */\n"))

    def test_create_doxygen_header_with_brief(self):
        """Test Doxygen header creation with existing brief."""
        authors = [("Test User", "test@example.com")]
        header = update_authors.create_doxygen_header("test.c", authors, "This is a test file")
        
        self.assertIn("@file test.c", header)
        self.assertIn("@author Test User <test@example.com>", header)
        self.assertIn("@brief This is a test file", header)

    def test_create_assembly_header(self):
        """Test assembly header creation."""
        authors = [("Test User", "test@example.com"), ("Another User", "another@example.com")]
        header = update_authors.create_assembly_header("test.s", authors)
        
        self.assertIn("############################################################", header)
        self.assertIn("# test.s", header)
        self.assertIn("# Authors: Test User <test@example.com>", header)
        self.assertIn("#          Another User <another@example.com>", header)

    def test_update_existing_header(self):
        """Test updating existing headers."""
        header_content = """
/**
 * @file oldname.c
 * @author Old User <old@example.com>
 * @brief Old description
 * @version 1.0
 */
"""
        authors = [("New User", "new@example.com")]
        updated = update_authors.update_existing_header(header_content, "newname.c", authors)
        
        self.assertIn("@file newname.c", updated)
        self.assertIn("@author New User <new@example.com>", updated)
        self.assertIn("@brief Old description", updated)
        self.assertIn("@version 1.0", updated)
        self.assertNotIn("@author Old User", updated)

    def test_process_file_no_existing_header_c(self):
        """Test processing C file with no existing header."""
        content = """
int main() {
    return 0;
}
"""
        filepath = self.create_test_file("test.c", content)
        def mock_check_output(args, **kwargs):
            if 'log' in args:
                return "Test User <test@example.com>"
            else:
                return "author Test User\nauthor-mail <test@example.com>"
        with patch('subprocess.check_output', side_effect=mock_check_output):
            result = update_authors.process_file(filepath)
            self.assertTrue(result)
            with open(filepath, 'r') as f:
                new_content = f.read()
            self.assertIn("/**", new_content)
            self.assertIn("@file test.c", new_content)
            self.assertIn("@author Test User <test@example.com>", new_content)

    def test_process_file_existing_header_c(self):
        """Test processing C file with existing header."""
        header_content = """
/**
 * @file test.c
 * @author Old User <old@example.com>
 * @brief Old description
 */
"""
        content = header_content + "int main() {\n    return 0;\n}\n"
        filepath = self.create_test_file("test.c", content)
        def mock_check_output(args, **kwargs):
            if 'log' in args:
                return "New User <new@example.com>"
            else:
                return "author New User\nauthor-mail <new@example.com>"
        with patch('subprocess.check_output', side_effect=mock_check_output):
            result = update_authors.process_file(filepath)
            self.assertTrue(result)
            with open(filepath, 'r') as f:
                new_content = f.read()
            self.assertIn("@file test.c", new_content)
            self.assertIn("@author New User <new@example.com>", new_content)
            self.assertIn("@brief Old description", new_content)
            self.assertNotIn("@author Old User", new_content)

    def test_process_file_assembly_no_header(self):
        """Test processing assembly file with no existing header."""
        content = """
    .text
    .globl main
main:
    jr $ra
"""
        filepath = self.create_test_file("test.s", content)
        def mock_check_output(args, **kwargs):
            if 'log' in args:
                return "Test User <test@example.com>"
            else:
                # Each line in the file gets a pair
                return "author Test User\nauthor-mail <test@example.com>\n" * 5
        with patch('subprocess.check_output', side_effect=mock_check_output):
            result = update_authors.process_file(filepath)
            self.assertTrue(result)
            with open(filepath, 'r') as f:
                new_content = f.read()
            self.assertIn("# Authors: Test User <test@example.com>", new_content)

    def test_process_file_assembly_existing_header(self):
        """Test processing assembly file with existing header."""
        content = """
############################################################
# Bla bla existing text 1
# Bla bla existing text 2
# Bla bla existing text 3
#
#
#
# Authors: Old User <old@example.com>
############################################################

    .text
    .globl main
main:
    jr $ra
"""
        filepath = self.create_test_file("test.s", content)
        def mock_check_output(args, **kwargs):
            if 'log' in args:
                return "New User <new@example.com>"
            else:
                return "author New User\nauthor-mail <new@example.com>\n" * 5
        with patch('subprocess.check_output', side_effect=mock_check_output):
            result = update_authors.process_file(filepath)
            self.assertTrue(result)
            with open(filepath, 'r') as f:
                new_content = f.read()
            self.assertIn("# Authors: New User <new@example.com>", new_content)
            self.assertNotIn("# Authors: Old User", new_content)

    def test_process_file_assembly_existing_header_no_authors(self):
        """Test processing assembly file with existing header but no authors section."""
        content = """
############################################################
# test.s
#
# Some other comment
#
#
############################################################

    .text
    .globl main
main:
    jr $ra
"""
        filepath = self.create_test_file("test.s", content)
        def mock_check_output(args, **kwargs):
            if 'log' in args:
                return "Test User <test@example.com>"
            else:
                return "author Test User\nauthor-mail <test@example.com>\n" * 5
        with patch('subprocess.check_output', side_effect=mock_check_output):
            result = update_authors.process_file(filepath)
            self.assertTrue(result)
            with open(filepath, 'r') as f:
                new_content = f.read()
            self.assertIn("# Authors: Test User <test@example.com>", new_content)

    def test_process_file_assembly_multiple_sharps_blocks(self):
        """Test that only the first sharps block is updated in assembly files with multiple blocks."""
        content = """
############################################################
# test.s
#
#
#
# Authors: Old User <old@example.com>
############################################################

    .text
    .globl main
main:
    jr $ra

############################################################
# This is a second block
# Authors: Should Stay <untouched@example.com>
#
############################################################
"""
        filepath = self.create_test_file("test.s", content)
        def mock_check_output(args, **kwargs):
            if 'log' in args:
                return "New User <new@example.com>"
            else:
                return "author New User\nauthor-mail <new@example.com>\n" * 5
        with patch('subprocess.check_output', side_effect=mock_check_output):
            result = update_authors.process_file(filepath)
            self.assertTrue(result)
            with open(filepath, 'r') as f:
                new_content = f.read()
            self.assertIn("# Authors: New User <new@example.com>", new_content)
            self.assertNotIn("# Authors: Old User <old@example.com>", new_content)
            self.assertIn("# Authors: Should Stay <untouched@example.com>", new_content)
            self.assertIn("# This is a second block", new_content)

    def test_process_file_assembly_tab_indented_sharps(self):
        """Test that author lines in assembly headers preserve tab indentation of the sharp block."""
        content = """	############################################################
	#
	# Some description
	#
	#
	############################################################

.text
main:
    jr $ra
"""
        filepath = self.create_test_file("test.s", content)
        def mock_check_output(args, **kwargs):
            if 'log' in args:
                return "New User <new@example.com>"
            else:
                return "author New User\nauthor-mail <new@example.com>\n" * 5
        with patch('subprocess.check_output', side_effect=mock_check_output):
            result = update_authors.process_file(filepath)
            self.assertTrue(result)
            with open(filepath, 'r') as f:
                new_content = f.read()
            self.assertIn("\t# Authors: New User <new@example.com>", new_content)

    def test_process_file_original_obj_map_c(self):
        """Test processing the original obj_map.c file content."""
        content = """/**
 * This is a very simple hash map that uses open adressing (linear probing).
 * The hash function is the identity for now, since it uses integer keys.
 */

#include \"obj_map.h\"

void obj_map_new(obj_map_t *map)
{
    // Implementation here
}
"""
        filepath = self.create_test_file("obj_map.c", content)
        def mock_check_output(args, **kwargs):
            if 'log' in args:
                return "Test User <test@example.com>"
            else:
                return "author Test User\nauthor-mail <test@example.com>\n" * 10
        with patch('subprocess.check_output', side_effect=mock_check_output):
            result = update_authors.process_file(filepath)
            self.assertTrue(result)
            with open(filepath, 'r') as f:
                new_content = f.read()
            lines = new_content.split('\n')
            self.assertEqual(lines[0].strip(), "/**")
            self.assertEqual(lines[1].lstrip(), "* @file obj_map.c")
            self.assertEqual(lines[2].lstrip(), "* @author Test User <test@example.com>")
            self.assertEqual(lines[3].lstrip(), "*")
            self.assertIn("This is a very simple hash map", lines[4])
            self.assertIn("The hash function is the identity for now, since it uses integer keys", new_content)
            self.assertIn("#include \"obj_map.h\"", new_content)
            self.assertIn("void obj_map_new(obj_map_t *map)", new_content)

    def test_process_file_skip_vendored(self):
        """Test that vendored files are skipped."""
        content = "int main() { return 0; }"
        filepath = self.create_test_file("vendor/test.c", content)
        
        with patch.object(update_authors, 'SKIP_PATHS', ['vendor*']):
            result = update_authors.process_file(filepath)
            self.assertFalse(result)

    def test_process_file_skip_untracked(self):
        """Test that untracked files are skipped."""
        filepath = Path("untracked.c")
        with open(filepath, 'w') as f:
            f.write("int main() { return 0; }")
        
        result = update_authors.process_file(filepath)
        self.assertFalse(result)

    def test_get_authors_mock(self):
        """Test get_authors with mocked git commands."""
        filepath = Path("test.c")
        
        # Mock git log output (first committer)
        mock_log_output = "Another User <another@example.com>\nTest User <test@example.com>"
        # Mock git blame output
        mock_blame_output = """
author Test User
author-mail <test@example.com>
author Test User
author-mail <test@example.com>
author Another User
author-mail <another@example.com>
"""
        def mock_check_output(args, **kwargs):
            if 'log' in args:
                return mock_log_output
            else:
                return mock_blame_output
        with patch('subprocess.check_output', side_effect=mock_check_output):
            authors = update_authors.get_authors(filepath)
            self.assertEqual(len(authors), 2)
            # Test User is the first committer
            self.assertEqual(authors[0][0], "Test User")
            self.assertEqual(authors[0][1], "test@example.com")
            # Another User should be second (alphabetically, but only one left)
            self.assertEqual(authors[1][0], "Another User")
            self.assertEqual(authors[1][1], "another@example.com")

    def test_get_authors_with_github_masked_emails(self):
        """Test get_authors with GitHub masked emails."""
        filepath = Path("test.c")
        
        # Mock git log output
        mock_log_output = "SpookyIluha <127010686+SpookyIluha@users.noreply.github.com>"
        
        # Mock git blame output
        mock_blame_output = """
author SpookyIluha
author-mail <127010686+SpookyIluha@users.noreply.github.com>
"""
        
        def mock_check_output(args, **kwargs):
            if 'log' in args:
                return mock_log_output
            else:
                return mock_blame_output
        
        with patch('subprocess.check_output', side_effect=mock_check_output):
            authors = update_authors.get_authors(filepath)
            # Should return normalized authors with GitHub URLs
            self.assertEqual(len(authors), 1)
            self.assertEqual(authors[0][0], "SpookyIluha")
            self.assertEqual(authors[0][1], "https://github.com/SpookyIluha")

    def test_edge_case_empty_file(self):
        """Test processing an empty file."""
        filepath = self.create_test_file("empty.c", "")
        
        with patch.object(update_authors, 'get_authors') as mock_get_authors:
            mock_get_authors.return_value = [("Test User", "test@example.com")]
            
            result = update_authors.process_file(filepath)
            self.assertTrue(result)
            
            with open(filepath, 'r') as f:
                new_content = f.read()
            
            self.assertIn("/**", new_content)
            self.assertIn("@file empty.c", new_content)

    def test_edge_case_file_with_only_comments(self):
        """Test processing a file with only comments."""
        content = """
// This is a comment
/* Another comment */
"""
        filepath = self.create_test_file("comments.c", content)
        
        with patch.object(update_authors, 'get_authors') as mock_get_authors:
            mock_get_authors.return_value = [("Test User", "test@example.com")]
            
            result = update_authors.process_file(filepath)
            self.assertTrue(result)
            
            with open(filepath, 'r') as f:
                new_content = f.read()
            
            self.assertIn("/**", new_content)
            self.assertIn("@file comments.c", new_content)
            self.assertIn("// This is a comment", new_content)

    def test_edge_case_malformed_header(self):
        """Test processing a file with malformed header."""
        content = """
/**
 * @file test.c
 * @author Test User <test@example.com>
 * This is malformed (missing @)
int main() {
    return 0;
}
"""
        filepath = self.create_test_file("malformed.c", content)
        def mock_check_output(args, **kwargs):
            if 'log' in args:
                return "New User <new@example.com>"
            else:
                return "author New User\nauthor-mail <new@example.com>"
        with patch('subprocess.check_output', side_effect=mock_check_output):
            result = update_authors.process_file(filepath)
            self.assertTrue(result)
            with open(filepath, 'r') as f:
                new_content = f.read()
            # Should still process the file and add proper header
            self.assertIn("@file malformed.c", new_content)
            self.assertIn("@author New User <new@example.com>", new_content)

    def test_get_authors_alphabetical_ordering(self):
        """Test that authors are sorted alphabetically."""
        def mock_check_output(args, **kwargs):
            if 'blame' in args:
                return """author Test User
author-mail <test@example.com>
author Another User
author-mail <another@example.com>
author Test User
author-mail <test@example.com>"""
            elif 'log' in args:
                return "Test User <test@example.com>"
            return ""

        with patch('subprocess.check_output', side_effect=mock_check_output):
            authors = update_authors.get_authors_for_file(Path("test.c"))
            # Should be sorted alphabetically, with first committer first
            self.assertEqual(authors[0][0], "Test User")  # First committer
            self.assertEqual(authors[1][0], "Another User")  # Alphabetically first

    def test_idempotency_c_file(self):
        """Test that processing a C file multiple times doesn't change the content."""
        # Create a test C file with existing header
        test_content = '''/**
 * @file test.c
 * @author Test Author <test@example.com>
 *
 * This is a test file.
 */

#include <stdio.h>

int main() {
    return 0;
}
'''
        
        filepath = self.create_test_file("test.c", test_content)
        
        def mock_check_output(args, **kwargs):
            if 'blame' in args:
                return """author Test Author
author-mail <test@example.com>
author Test Author
author-mail <test@example.com>"""
            elif 'log' in args:
                return "Test Author <test@example.com>"
            return ""

        with patch('subprocess.check_output', side_effect=mock_check_output):
            # Process the file once
            update_authors.process_file(filepath)
            content1 = filepath.read_text()
            
            # Process the file again
            update_authors.process_file(filepath)
            content2 = filepath.read_text()
            
            # Content should be identical
            self.assertEqual(content1, content2, "C file content changed after second processing")

    def test_idempotency_assembly_file(self):
        """Test that processing an assembly file multiple times doesn't change the content."""
        # Create a test assembly file with existing header
        test_content = '''############################################################
# test.S
#
# This is a test assembly file.
#
# Authors: Test Author <test@example.com>
############################################################

    .text
    .globl main
main:
    jr ra
    nop
'''
        
        filepath = self.create_test_file("test.S", test_content)
        
        def mock_check_output(args, **kwargs):
            if 'blame' in args:
                return """author Test Author
author-mail <test@example.com>
author Test Author
author-mail <test@example.com>"""
            elif 'log' in args:
                return "Test Author <test@example.com>"
            return ""

        with patch('subprocess.check_output', side_effect=mock_check_output):
            # Process the file once
            update_authors.process_file(filepath)
            content1 = filepath.read_text()
            
            # Process the file again
            update_authors.process_file(filepath)
            content2 = filepath.read_text()
            
            # Content should be identical
            self.assertEqual(content1, content2, "Assembly file content changed after second processing")

    def test_idempotency_c_file_with_multiple_authors(self):
        """Test idempotency with multiple authors."""
        # Create a test C file with existing header and multiple authors
        test_content = '''/**
 * @file test.c
 * @author Author One <author1@example.com>
 * @author Author Two <author2@example.com>
 *
 * This is a test file with multiple authors.
 */

#include <stdio.h>

int main() {
    return 0;
}
'''
        
        filepath = self.create_test_file("test.c", test_content)
        
        def mock_check_output(args, **kwargs):
            if 'blame' in args:
                return """author Author One
author-mail <author1@example.com>
author Author Two
author-mail <author2@example.com>
author Author One
author-mail <author1@example.com>"""
            elif 'log' in args:
                return "Author One <author1@example.com>"
            return ""

        with patch('subprocess.check_output', side_effect=mock_check_output):
            # Process the file once
            update_authors.process_file(filepath)
            content1 = filepath.read_text()
            
            # Process the file again
            update_authors.process_file(filepath)
            content2 = filepath.read_text()
            
            # Content should be identical
            self.assertEqual(content1, content2, "C file with multiple authors changed after second processing")

    def test_idempotency_assembly_file_with_description(self):
        """Test idempotency for assembly file with description before authors."""
        # Create a test assembly file with description before authors
        test_content = '''############################################################
# test.S
#
# This is a test assembly file with description.
# It has multiple lines of description.
#
# Authors: Test Author <test@example.com>
############################################################

    .text
    .globl main
main:
    jr ra
    nop
'''
        
        filepath = self.create_test_file("test.S", test_content)
        
        def mock_check_output(args, **kwargs):
            if 'blame' in args:
                return """author Test Author
author-mail <test@example.com>
author Test Author
author-mail <test@example.com>"""
            elif 'log' in args:
                return "Test Author <test@example.com>"
            return ""

        with patch('subprocess.check_output', side_effect=mock_check_output):
            # Process the file once
            update_authors.process_file(filepath)
            content1 = filepath.read_text()
            
            # Process the file again
            update_authors.process_file(filepath)
            content2 = filepath.read_text()
            
            # Content should be identical
            self.assertEqual(content1, content2, "Assembly file with description changed after second processing")

    def test_assembly_header_single_blank_line_before_authors(self):
        """Test that there is always exactly one blank line before authors in assembly headers."""
        # Case 1: Only blank lines before authors
        test_content_blank = '''############################################################
# test.S
#
#
#
# Authors: Test Author <test@example.com>
############################################################

    .text
    .globl main
main:
    jr ra
    nop
'''
        filepath_blank = self.create_test_file("test.S", test_content_blank)
        def mock_check_output(args, **kwargs):
            if 'blame' in args:
                return "author Test Author\nauthor-mail <test@example.com>"
            elif 'log' in args:
                return "Test Author <test@example.com>"
            return ""
        with patch('subprocess.check_output', side_effect=mock_check_output):
            update_authors.process_file(filepath_blank)
            content = filepath_blank.read_text()
            lines = content.splitlines()
            # Find the authors line
            for idx, line in enumerate(lines):
                if 'Authors:' in line:
                    # There must be exactly one blank line (i.e., a line with only #) before authors
                    self.assertTrue(idx > 0)
                    self.assertEqual(lines[idx-1].strip(), '#')
                    # And the line before that must not be blank
                    if idx > 1:
                        self.assertNotEqual(lines[idx-2].strip(), '#')
                    break
            else:
                self.fail('Authors line not found')

        # Case 2: Descriptive content and multiple blank lines before authors
        test_content_desc = '''############################################################
# test.S
#
# This is a test assembly file.
#
#
# Authors: Test Author <test@example.com>
############################################################

    .text
    .globl main
main:
    jr ra
    nop
'''
        filepath_desc = self.create_test_file("test.S", test_content_desc)
        with patch('subprocess.check_output', side_effect=mock_check_output):
            update_authors.process_file(filepath_desc)
            content = filepath_desc.read_text()
            lines = content.splitlines()
            for idx, line in enumerate(lines):
                if 'Authors:' in line:
                    # There must be exactly one blank line (i.e., a line with only #) before authors
                    self.assertTrue(idx > 0)
                    self.assertEqual(lines[idx-1].strip(), '#')
                    # And the line before that must not be blank
                    if idx > 1:
                        self.assertNotEqual(lines[idx-2].strip(), '#')
                    break
            else:
                self.fail('Authors line not found')


if __name__ == '__main__':
    unittest.main() 