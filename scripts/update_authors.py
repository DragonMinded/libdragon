#!/usr/bin/env python3
"""
Script to add or update Doxygen author headers in libdragon C/C++/header files.

This script is meant to be run on the preview branch, where the whole history
of file changes can be inspected. Backport to trunks are always squashed so
most of the history is lost there, and this script wouldn't be able to reconstruct
a list of major authors of each file.
"""

import os
import re
import sys
import subprocess
import multiprocessing
from pathlib import Path
import fnmatch
from collections import Counter

# List of files or directories to skip (vendored/third-party)
SKIP_PATHS = [
    'src/fatfs',
    'src/audio/opus',
    'src/audio/libxm',
    'src/video/pl_mpeg',
    'src/video/h264_decoder',
    'src/libcart',
    'src/usb.c',
    'src/rdpq/rdpq_font_builtin.c',
    'include/usb.h',
    'tools/audioconv64/libsamplerate',
    'tools/audioconv64/vadpcm',
    'tools/audioconv64/lzh5_compress.*',
    'tools/audioconv64/dr_*',
    'tools/common/apultra',
    'tools/common/incbin.h',
    'tools/common/lodepng.*',
    'tools/common/lz4',
    'tools/common/shrinkler',
    'tools/common/mips_elf.h',
    'tools/common/stb_ds.h',
    'tools/common/subprocess.h',
    'tools/mkfont/freetype',
    'tools/mkfont/phf.*',
    'tools/mkfont/rect_pack',
    'tools/mkfont/crc64.c',
    'tools/mkmaterial/json.hpp',
    'tools/mkmodel/cgltf.h',
    'tools/mksprite/exoquant.*',

    'tools/ipl3.h',
    # Add more as needed
]

# Author mapping to normalize names and emails
AUTHOR_MAP = {
    # Name variations -> canonical (name, email)
    'Giovanni Bajo': ('Giovanni Bajo', 'giovannibajo@gmail.com'),
    'rasky': ('Giovanni Bajo', 'giovannibajo@gmail.com'),
    'Shaun Taylor': ('Jennifer Taylor', 'dragonminded@dragonminded.com'),
    # Add more mappings as needed
}

def normalize_author(name, email):
    """Normalize author name and email using the author map."""
    # Check if the name is in our mapping
    if name in AUTHOR_MAP:
        return AUTHOR_MAP[name]
    
    # Check if the email is a GitHub masked email (e.g., 127010686+SpookyIluha@users.noreply.github.com)
    github_match = re.match(r'\d+\+([^@]+)@users\.noreply\.github\.com', email)
    if github_match:
        username = github_match.group(1)
        # Return the name with a GitHub user page link instead of the masked email
        return (name, f"https://github.com/{username}")
    
    # Check if the email matches any known author
    for canonical_name, canonical_email in AUTHOR_MAP.values():
        if email == canonical_email:
            return (canonical_name, canonical_email)
    
    # Return as-is if no mapping found
    return (name, email)

def is_git_tracked(filepath):
    """Check if a file is tracked by git."""
    try:
        result = subprocess.run([
            'git', 'ls-files', '--error-unmatch', str(filepath)
        ], capture_output=True, text=True)
        return result.returncode == 0
    except Exception:
        return False

def is_skipped_path(path):
    """Return True if the path matches any skip pattern."""
    for skip in SKIP_PATHS:
        # Support glob patterns
        if fnmatch.fnmatch(path, skip):
            return True
        # Support directory skipping
        if os.path.isdir(skip) and os.path.commonpath([os.path.abspath(path), os.path.abspath(skip)]) == os.path.abspath(skip):
            return True
        # Support direct file match
        if os.path.abspath(path) == os.path.abspath(skip):
            return True
    return False

def is_assembly_file(filepath):
    """Check if file is an assembly file based on extension."""
    return filepath.suffix.lower() in ['.s', '.inc']

def get_authors_for_file(filepath):
    """Get authors for a single file using git blame and git log."""
    try:
        # Get first committer (chronologically) - use normal order and take the last line
        log = subprocess.check_output([
            'git', 'log', '--follow', '--format=%an <%ae>', str(filepath)
        ], encoding='utf-8', errors='replace')
        log_lines = log.splitlines()
        first_committer_line = log_lines[-1] if log_lines else None
        if first_committer_line and '<' in first_committer_line and first_committer_line.endswith('>'):
            name, email = first_committer_line.rsplit(' <', 1)
            email = email.rstrip('>')
            first_author = normalize_author(name.strip(), email.strip())
        else:
            first_author = None

        # Get line-by-line blame for the file
        blame = subprocess.check_output([
            'git', 'blame', '--line-porcelain', str(filepath)
        ], encoding='utf-8', errors='replace')
        if isinstance(blame, bytes):
            blame = blame.decode('utf-8', errors='replace')
        authors = []
        names = []
        for line in blame.splitlines():
            if line.startswith('author-mail '):
                authors.append(line[12:].strip())
            elif line.startswith('author '):
                names.append(line[7:].strip())
        author_pairs = list(zip(names, authors))
        # Filter out uncommitted entries
        filtered_pairs = []
        for name, email in author_pairs:
            email = email.strip('<>')
            if name == "Not Committed Yet" and email == "not.committed.yet":
                continue
            filtered_pairs.append((name, email))
        # Normalize
        normalized_pairs = [normalize_author(name, email) for name, email in filtered_pairs]
        counter = Counter(normalized_pairs)
        total = sum(counter.values())
        # Apply 20% rule
        major_authors = [a for a, c in counter.items() if c / total >= 0.2]
        # Always include the first committer at the top
        authors_list = []
        if first_author:
            authors_list.append(first_author)
        # Add other major authors, sorted alphabetically, excluding first_author if present
        other_authors = [a for a in major_authors if a != first_author]
        other_authors.sort(key=lambda x: x[0].lower())
        authors_list.extend(other_authors)
        # Ensure unique authors (should already be unique, but just in case)
        seen = set()
        unique_authors = []
        for author in authors_list:
            if author not in seen:
                seen.add(author)
                unique_authors.append(author)
        return unique_authors
    except Exception:
        return []

def extract_existing_brief(header_content):
    """Extract the first @brief tag from a file-level comment block."""
    lines = header_content.split('\n')
    for line in lines:
        stripped = line.strip()
        if '@brief' in stripped:
            brief_match = re.search(r'@brief\s+(.+)', stripped)
            if brief_match:
                return brief_match.group(1).strip()
    return None

def find_file_level_header(content):
    """Find existing file-level Doxygen header block."""
    lines = content.split('\n')
    i = 0
    
    # Skip empty lines at the beginning
    while i < len(lines) and lines[i].strip() == '':
        i += 1
    
    # Check if the first non-empty line starts a Doxygen comment block
    if i < len(lines) and (lines[i].strip().startswith('/**') or lines[i].strip().startswith('/*!')):
        start_line = i
        i += 1
        
        # Find the end of the comment block
        while i < len(lines):
            stripped = lines[i].strip()
            if stripped == '*/':
                return start_line, i + 1, '\n'.join(lines[start_line:i+1])
            i += 1
    
    return None, None, None

def create_assembly_header(filename, authors):
    """Create assembly header with 60 sharps."""
    header_lines = [
        "#" * 60,
        f"# {filename}",
        "#",
    ]
    
    # Add authors on multiple lines
    for i, author_info in enumerate(authors):
        name, email = author_info
        if i == 0:
            header_lines.append(f"# Authors: {name} <{email}>")
        else:
            header_lines.append(f"#          {name} <{email}>")
    
    header_lines.extend([
        "#",
        "#" * 60,
        ""
    ])
    return '\n'.join(header_lines)

def create_doxygen_header(filename, authors, existing_brief=None):
    """Create Doxygen header for C/C++ files."""
    header_lines = [
        "/**",
        f" * @file {filename}",
    ]
    for i, author_info in enumerate(authors):
        name, email = author_info
        header_lines.append(f" * @author {name} <{email}>")
    if existing_brief:
        header_lines.append(f" * @brief {existing_brief}")
    header_lines.append(" */\n")
    return '\n'.join(header_lines)

def update_existing_header(header_content, filename, authors):
    """Update @file and @author tags in an existing header while preserving all other content."""
    lines = header_content.split('\n')
    updated_lines = []
    
    # Start with the opening comment line
    updated_lines.append('/**')
    
    # Add @file tag at the beginning
    updated_lines.append(f" * @file {filename}")
    
    # Add @author tags
    for author_info in authors:
        name, email = author_info
        updated_lines.append(f" * @author {name} <{email}>")

    # Extract and preserve existing descriptive content (skip @file, @author tags, but keep @brief)
    descriptive_lines = []
    for line in lines:
        stripped = line.strip()
        if (stripped.startswith('* @file') or 
            stripped.startswith('* @author') or
            stripped == '*/' or
            stripped == '/**' or
            stripped.startswith('/**')):
            # Skip existing @file/@author tags and comment markers
            continue
        elif stripped.startswith('*'):
            # Keep descriptive content (lines starting with * including empty lines)
            descriptive_lines.append(line)
    
    # Add a blank line only if there are descriptive lines and the first one is not empty
    if descriptive_lines:
        first_desc = descriptive_lines[0].strip()
        if first_desc.startswith('*') and not first_desc.startswith('* @') and first_desc != '*':
            updated_lines.append(' *')
    
    # Add the descriptive content
    updated_lines.extend(descriptive_lines)
    
    # Add the closing comment line
    updated_lines.append(" */")
    
    return '\n'.join(updated_lines)

def process_file(filepath, authors=None):
    """Process a single file to add/update Doxygen headers."""
    try:
        # Skip vendored files and directories
        if is_skipped_path(str(filepath)):
            return False
            
        # Skip untracked git files
        if not is_git_tracked(filepath):
            return False
            
        # Read file content
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()
    except UnicodeDecodeError:
        print(f"  Skipping {filepath} (encoding issue)")
        return False
    
    # Get authors if not provided
    if authors is None:
        authors = get_authors_for_file(filepath)
    
    # Handle assembly files
    if is_assembly_file(filepath):
        lines = content.split('\n')
        # Skip empty lines at the beginning
        i = 0
        while i < len(lines) and lines[i].strip() == '':
            i += 1
        
        # Detect top-level sharps block (from first sharps line to next sharps line)
        if i < len(lines) and lines[i].lstrip().startswith('#') and len(lines[i].lstrip()) >= 10 and set(lines[i].lstrip()) == {'#'}:
            # Get indentation from the opening sharps line
            indent = lines[i][:len(lines[i]) - len(lines[i].lstrip())]
            # Find the closing sharps line
            closing_idx = None
            for j in range(i+1, len(lines)):
                l = lines[j].lstrip()
                if l.startswith('#') and len(l) >= 10 and set(l) == {'#'}:
                    closing_idx = j
                    break
            if closing_idx is not None:
                # Build new block: preserve all lines except author lines, insert new authors before closing sharps
                block_lines = []
                # Add any empty lines at the beginning
                block_lines.extend(lines[:i])
                # Opening sharps line
                block_lines.append(lines[i])
                # All lines up to (but not including) closing sharps
                pre_author_lines = []
                has_content_before_authors = False
                for j in range(i+1, closing_idx):
                    l = lines[j].lstrip()
                    if l.startswith('# Authors:') or (l.startswith('#') and ' <' in l and '@' in l and not l.startswith('# Bla bla')):
                        continue  # skip old author lines
                    # Check for actual description (not empty, not filename, not just '#')
                    if l.startswith('#'):
                        content = l[1:].strip()
                        if content and content != filepath.name:
                            has_content_before_authors = True
                    pre_author_lines.append(lines[j])
                # Remove trailing blank lines before authors
                while pre_author_lines and pre_author_lines[-1].lstrip() == '#':
                    pre_author_lines.pop()
                block_lines.extend(pre_author_lines)
                # Always add exactly one blank line before authors
                block_lines.append(f'{indent}#')
                
                for k, author_info in enumerate(authors):
                    name, email = author_info
                    if k == 0:
                        block_lines.append(f"{indent}# Authors: {name} <{email}>")
                    else:
                        block_lines.append(f"{indent}#          {name} <{email}>")
                # Add the closing sharps line
                block_lines.append(lines[closing_idx])
                # Add the rest of the file
                new_lines = block_lines + lines[closing_idx+1:]
                new_content = '\n'.join(new_lines)
            else:
                # No closing sharps, fallback to previous logic
                new_header = create_assembly_header(filepath.name, authors)
                new_content = new_header + content
        else:
            # No top-level sharps block, add full header
            new_header = create_assembly_header(filepath.name, authors)
            new_content = new_header + content
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(new_content)
        return True
    
    # Handle C/C++ files
    # Check if there's already a file-level header
    header_start, header_end, existing_header = find_file_level_header(content)
    
    # Update existing header or create new one
    if header_start is not None and header_end is not None:
        # Update existing header while preserving all content
        updated_header = update_existing_header(existing_header, filepath.name, authors)
        lines = content.split('\n')
        new_content = updated_header + '\n' + '\n'.join(lines[header_end:])
    else:
        # Extract existing brief if available
        existing_brief = None
        if existing_header:
            existing_brief = extract_existing_brief(existing_header)
        
        # Create new header
        new_header = create_doxygen_header(filepath.name, authors, existing_brief)
        new_content = new_header + content
    
    # Write back
    with open(filepath, 'w', encoding='utf-8') as f:
        f.write(new_content)
    
    return True

def process_file_wrapper(filepath):
    """Wrapper function for multiprocessing that returns success status."""
    try:
        return process_file(filepath)
    except Exception as e:
        print(f"Error processing {filepath}: {e}")
        return False

def main():
    """Main function to process files or directories."""
    if len(sys.argv) < 2:
        print("Usage: python3 update_authors.py <file_or_directory> [<file_or_directory> ...]")
        print("       python3 update_authors.py  # Process all C/C++/header/assembly files in the repo")
        sys.exit(1)
    
    # Get list of files to process
    files_to_process = []
    
    if len(sys.argv) == 2 and sys.argv[1] == '.':
        # Process all C/C++/header/assembly files in the repo
        for root, dirs, files in os.walk('.'):
            # Skip .git directory
            if '.git' in dirs:
                dirs.remove('.git')
            # Skip build directories
            if 'build' in dirs:
                dirs.remove('build')
            if 'bin' in dirs:
                dirs.remove('bin')
            
            for file in files:
                filepath = Path(root) / file
                if is_source_file(filepath):
                    files_to_process.append(filepath)
    else:
        # Process specified files/directories
        for arg in sys.argv[1:]:
            path = Path(arg)
            if path.is_file():
                if is_source_file(path):
                    files_to_process.append(path)
                else:
                    print(f"Skipping {path} (not a source file)")
            elif path.is_dir():
                # Recursively find all source files in directory
                for root, dirs, files in os.walk(path):
                    # Skip .git directory
                    if '.git' in dirs:
                        dirs.remove('.git')
                    # Skip build directories
                    if 'build' in dirs:
                        dirs.remove('build')
                    if 'bin' in dirs:
                        dirs.remove('bin')
                    
                    for file in files:
                        filepath = Path(root) / file
                        if is_source_file(filepath):
                            files_to_process.append(filepath)
            else:
                print(f"Warning: {path} does not exist")
    
    if not files_to_process:
        print("No source files found to process")
        return
    
    print(f"Processing {len(files_to_process)} files...")
    
    # Filter out untracked files before getting authors
    tracked_files = [fp for fp in files_to_process if is_git_tracked(fp)]
    untracked_count = len(files_to_process) - len(tracked_files)
    
    if untracked_count > 0:
        print(f"Skipping {untracked_count} untracked files...")
    
    # Always process all tracked files (do not skip files with existing headers)
    print("Process files...")
    
    # Use multiprocessing to process files in parallel
    num_processes = max(1, min(multiprocessing.cpu_count(), len(tracked_files)))
    
    with multiprocessing.Pool(processes=num_processes) as pool:
        # Use map to process files in parallel
        results = pool.map(process_file_wrapper, tracked_files)
        
        # Print summary
        processed = sum(1 for result in results if result)
        print(f"Updated {processed} files")

def is_source_file(filepath):
    """Check if file is a source file that should be processed."""
    # C/C++/header files
    if filepath.suffix.lower() in ['.c', '.cpp', '.h', '.hpp']:
        return True
    # Assembly files
    if filepath.suffix.lower() in ['.s', '.inc']:
        return True
    return False

def get_authors(filepath):
    """Wrapper for testability: get authors for a single file."""
    return get_authors_for_file(filepath)

def has_proper_header(filepath):
    """Check if a file already has a proper Doxygen or assembly header."""
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()
    except (UnicodeDecodeError, FileNotFoundError):
        return False
    
    lines = content.split('\n')
    i = 0
    
    # Skip empty lines at the beginning
    while i < len(lines) and lines[i].strip() == '':
        i += 1
    
    if i >= len(lines):
        return False
    
    # Check for Doxygen header
    if lines[i].strip().startswith('/**') or lines[i].strip().startswith('/*!'):
        # Look for @file and @author tags
        has_file = False
        has_author = False
        for line in lines[i:]:
            stripped = line.strip()
            if stripped == '*/':
                break
            if '@file' in stripped:
                has_file = True
            if '@author' in stripped:
                has_author = True
        return has_file and has_author
    
    # Check for assembly header
    if is_assembly_file(filepath):
        if lines[i].lstrip().startswith('#') and len(lines[i].lstrip()) >= 10 and set(lines[i].lstrip()) == {'#'}:
            # Look for Authors line in the first sharps block
            for j in range(i+1, len(lines)):
                l = lines[j].lstrip()
                if l.startswith('#') and len(l) >= 10 and set(l) == {'#'}:
                    break  # End of first block
                if l.startswith('# Authors:'):
                    return True
    
    return False

def extract_existing_authors(header_content):
    """Extract existing @author tags from a Doxygen header."""
    authors = []
    lines = header_content.split('\n')
    for line in lines:
        stripped = line.strip()
        if stripped.startswith('* @author '):
            # Extract author info from "@author Name <email>" format
            author_part = stripped[10:]  # Remove "* @author "
            # Parse name and email
            if ' <' in author_part and author_part.endswith('>'):
                name_part, email_part = author_part.rsplit(' <', 1)
                email = email_part.rstrip('>')
                authors.append((name_part.strip(), email))
            else:
                # Just name without email
                authors.append((author_part.strip(), ''))
    return authors

def extract_existing_assembly_authors(content):
    """Extract existing authors from assembly header."""
    authors = []
    lines = content.split('\n')
    i = 0
    
    # Skip empty lines at the beginning
    while i < len(lines) and lines[i].strip() == '':
        i += 1
    
    # Look for the first sharps block
    if i < len(lines) and lines[i].lstrip().startswith('#') and len(lines[i].lstrip()) >= 10 and set(lines[i].lstrip()) == {'#'}:
        # Find the closing sharps line
        for j in range(i+1, len(lines)):
            l = lines[j].lstrip()
            if l.startswith('#') and len(l) >= 10 and set(l) == {'#'}:
                break  # End of first block
            if l.startswith('# Authors:'):
                # Parse the first author line
                author_line = l[10:].strip()  # Remove "# Authors: "
                if ' <' in author_line and author_line.endswith('>'):
                    name_part, email_part = author_line.rsplit(' <', 1)
                    email = email_part.rstrip('>')
                    authors.append((name_part.strip(), email))
                else:
                    authors.append((author_line.strip(), ''))
                # Look for continuation lines
                for k in range(j+1, len(lines)):
                    l2 = lines[k].lstrip()
                    if l2.startswith('#') and len(l2) >= 10 and set(l2) == {'#'}:
                        break  # End of block
                    if l2.startswith('#          ') and ' <' in l2 and '@' in l2:
                        # Continuation author line
                        author_line = l2[11:].strip()  # Remove "#          "
                        if ' <' in author_line and author_line.endswith('>'):
                            name_part, email_part = author_line.rsplit(' <', 1)
                            email = email_part.rstrip('>')
                            authors.append((name_part.strip(), email))
                        else:
                            authors.append((author_line.strip(), ''))
                break
    
    return authors

def authors_match(existing_authors, detected_authors):
    """Check if existing authors match detected authors (order-independent)."""
    if len(existing_authors) != len(detected_authors):
        return False
    
    # Normalize both lists for comparison
    existing_set = set()
    detected_set = set()
    
    for name, email in existing_authors:
        # Normalize existing authors
        normalized_name, normalized_email = normalize_author(name, email)
        existing_set.add((normalized_name, normalized_email))
    
    for name, email in detected_authors:
        # Detected authors are already normalized
        detected_set.add((name, email))
    
    return existing_set == detected_set

if __name__ == "__main__":
    main() 