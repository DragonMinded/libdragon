/*
    cpaktool - Controller Pak manipulation tool
    Written by Giovanni Bajo <giovannibajo@gmail.com>

    This tool is part of the Libdragon SDK.

    This is free and unencumbered software released into the public domain.

    For more information, please refer to <http://unlicense.org/>
*/
#include "cpakwrapper.h"
#include "cpaktool.h"
#include <sys/stat.h>
#include <cerrno>
#include <cstring>
#include <ctime>
#include <cstdlib>
#include <exception>
#include <stdexcept>
#include <unistd.h>
#include <fcntl.h>

extern "C" {
    int fileno(FILE *stream);
}

CPakFilesystem::CPakFilesystem(const std::string& filename, bool auto_mount)
    : m_filename(filename)
    , m_file(nullptr)
    , m_num_banks(0)
    , m_file_size(0)
    , m_pak_offset(0)
    , m_globals_set(false)
    , m_filesystem_mounted(false)
{
    m_file = fopen(filename.c_str(), "r+b");
    if (!m_file) {
        throw std::runtime_error("Cannot open file '" + filename + "': " + strerror(errno));
    }
    
    // Detect DexDrive format and set offset if needed
    if (detectDexDriveFormat()) {
        m_pak_offset = DEXDRIVE_HEADER_SIZE;
    }
    
    // Get file size and calculate banks
    calculateBanks();
    
    // Setup global variables for cpaklib
    setupGlobals();
    
    // Mount the filesystem only if requested
    if (auto_mount) {
        mountFilesystem();
    }
}

CPakFilesystem::~CPakFilesystem() {
    unmountFilesystem();
    cleanupGlobals();
    if (m_file) {
        fclose(m_file);
        m_file = nullptr;
    }
}

CPakFilesystem::CPakFilesystem(CPakFilesystem&& other) noexcept
    : m_filename(std::move(other.m_filename))
    , m_file(other.m_file)
    , m_num_banks(other.m_num_banks)
    , m_file_size(other.m_file_size)
    , m_pak_offset(other.m_pak_offset)
    , m_globals_set(other.m_globals_set)
    , m_filesystem_mounted(other.m_filesystem_mounted)
{
    other.m_file = nullptr;
    other.m_pak_offset = 0;
    other.m_globals_set = false;
    other.m_filesystem_mounted = false;
    
    // Update globals to point to this instance
    if (m_globals_set) {
        setupGlobals();
    }
}

CPakFilesystem& CPakFilesystem::operator=(CPakFilesystem&& other) noexcept {
    if (this != &other) {
        // Clean up current state
        unmountFilesystem();
        cleanupGlobals();
        if (m_file) {
            fclose(m_file);
        }
        
        // Move from other
        m_filename = std::move(other.m_filename);
        m_file = other.m_file;
        m_num_banks = other.m_num_banks;
        m_file_size = other.m_file_size;
        m_pak_offset = other.m_pak_offset;
        m_globals_set = other.m_globals_set;
        m_filesystem_mounted = other.m_filesystem_mounted;
        
        other.m_file = nullptr;
        other.m_pak_offset = 0;
        other.m_globals_set = false;
        other.m_filesystem_mounted = false;
        
        // Update globals
        if (m_globals_set) {
            setupGlobals();
        }
    }
    return *this;
}

void CPakFilesystem::setupGlobals() {
    g_pak = m_file;
    g_num_banks = m_num_banks;
    g_pak_offset = static_cast<int>(m_pak_offset);
    m_globals_set = true;
}

void CPakFilesystem::cleanupGlobals() {
    if (m_globals_set) {
        g_pak = nullptr;
        g_num_banks = 0;
        g_pak_offset = 0;
        m_globals_set = false;
    }
}

void CPakFilesystem::calculateBanks() {
    if (!m_file) return;
    
    struct stat st;
    if (fstat(fileno(m_file), &st) == 0) {
        m_file_size = st.st_size;
        size_t data_size = m_file_size - m_pak_offset;
        m_num_banks = static_cast<int>(data_size / BANK_SIZE);
        if (m_num_banks <= 0) m_num_banks = 1;
    }
}

bool CPakFilesystem::detectDexDriveFormat() {
    if (!m_file) return false;
    long original_pos = ftell(m_file);
    
    char signature[sizeof(DEXDRIVE_SIGNATURE)] = {0};
    fseek(m_file, 0, SEEK_SET);
    fread(signature, 1, sizeof(DEXDRIVE_SIGNATURE), m_file);
    fseek(m_file, original_pos, SEEK_SET);
    return memcmp(signature, DEXDRIVE_SIGNATURE, sizeof(DEXDRIVE_SIGNATURE)) == 0;
}

bool CPakFilesystem::mountFilesystem() {
    if (m_file && !m_filesystem_mounted) {
        if (cpakfs_mount(JOYPAD_PORT_1, "cpak:/") == 0) {
            m_filesystem_mounted = true;
            return true;
        }
    }
    return m_filesystem_mounted;
}

void CPakFilesystem::unmountFilesystem() {
    if (m_filesystem_mounted) {
        cpakfs_unmount(JOYPAD_PORT_1);
        m_filesystem_mounted = false;
    }
}

void CPakFilesystem::for_each_file(const FileCallback& callback) const {
    if (!m_filesystem_mounted) return;
    
    dir_t dir;
    if (cpak_dir_findfirst("/", &dir) == 0) {
        do {
            if (dir.d_type == DT_REG) { // Regular file
                if (!callback(dir.d_name, dir)) {
                    break; // Callback returned false, stop iteration
                }
            }
        } while (cpak_dir_findnext("/", &dir) == 0);
    }
}

// Static factory method for creating new pak files
std::unique_ptr<CPakFilesystem> CPakFilesystem::create(const std::string& filename, 
                                                       int num_banks, 
                                                       bool overwrite) {
    // Check if file exists and handle overwrite logic
    struct stat st;
    if (stat(filename.c_str(), &st) == 0 && !overwrite) {
        errno = EEXIST;
        return nullptr; // File exists and overwrite not allowed
    }
    
    // Calculate total size
    size_t total_size = num_banks * BANK_SIZE;
    
    // Create empty file
    if (!createEmptyFile(filename, total_size)) {
        // errno is already set by createEmptyFile
        return nullptr;
    }
    
    try {
        // Create wrapper and set number of banks (don't auto-mount empty file)
        auto wrapper = std::make_unique<CPakFilesystem>(filename, false);
        
        // Override the calculated banks with the requested number
        wrapper->m_num_banks = num_banks;
        wrapper->setupGlobals();
        
        // Format the filesystem
        std::srand(std::time(nullptr)); // Initialize random number generator
        int result = cpakfs_format(JOYPAD_PORT_1, true); // Always erase for fresh format
        if (result < 0) {
            unlink(filename.c_str()); // Remove the file on failure
            errno = EIO; // I/O error during format
            return nullptr;
        }
        
        // Now mount the formatted filesystem
        if (!wrapper->mountFilesystem()) {
            unlink(filename.c_str()); // Remove the file on failure
            errno = EIO; // Filesystem mount error
            return nullptr;
        }
        
        return wrapper;
        
    } catch (const std::exception&) {
        unlink(filename.c_str()); // Remove the file on failure
        errno = ENOENT; // File access error
        return nullptr;
    }
}

bool CPakFilesystem::createEmptyFile(const std::string& filename, size_t size) {
    FILE* file = fopen(filename.c_str(), "wb");
    if (!file) {
        return false;
    }
    
    // Create file of the right size by writing zeros
    if (size > 0) {
        // Write zeros to create a file of the exact size
        const size_t buffer_size = 4096;
        std::vector<uint8_t> buffer(buffer_size, 0);
        
        size_t remaining = size;
        while (remaining > 0) {
            size_t to_write = std::min(remaining, buffer_size);
            if (fwrite(buffer.data(), 1, to_write, file) != to_write) {
                fclose(file);
                unlink(filename.c_str());
                return false;
            }
            remaining -= to_write;
        }
    }
    
    fclose(file);
    return true;
}

//
// CPakFile implementation
//

CPakFile::CPakFile(const std::string& path, int flags)
    : m_handle(nullptr)
    , m_path(path)
    , m_flags(flags)
{
    m_handle = cpak_file_open(path.c_str(), flags);
    if (!m_handle) {
        throw std::runtime_error("Failed to open cpak file '" + path + "': " + std::strerror(errno));
    }
}

CPakFile::~CPakFile() {
    if (m_handle) {
        cpak_file_close(m_handle);
        m_handle = nullptr;
    }
}

CPakFile::CPakFile(CPakFile&& other) noexcept
    : m_handle(other.m_handle)
    , m_path(std::move(other.m_path))
    , m_flags(other.m_flags)
{
    other.m_handle = nullptr;
}

CPakFile& CPakFile::operator=(CPakFile&& other) noexcept {
    if (this != &other) {
        // Clean up current state
        if (m_handle) {
            cpak_file_close(m_handle);
        }
        
        // Move from other
        m_handle = other.m_handle;
        m_path = std::move(other.m_path);
        m_flags = other.m_flags;
        
        other.m_handle = nullptr;
    }
    return *this;
}

size_t CPakFile::read(void* buffer, size_t size) {
    if (!m_handle) {
        throw std::runtime_error("Attempt to read from closed cpak file");
    }
    
    int result = cpak_file_read(m_handle, buffer, static_cast<int>(size));
    if (result < 0) {
        throw std::runtime_error("Failed to read from cpak file '" + m_path + "': " + std::strerror(errno));
    }
    
    return static_cast<size_t>(result);
}

size_t CPakFile::write(const void* buffer, size_t size) {
    if (!m_handle) {
        throw std::runtime_error("Attempt to write to closed cpak file");
    }
    
    int result = cpak_file_write(m_handle, buffer, static_cast<int>(size));
    if (result < 0) {
        throw std::runtime_error("Failed to write to cpak file '" + m_path + "': " + std::strerror(errno));
    }
    
    if (static_cast<size_t>(result) != size) {
        throw std::runtime_error("Incomplete write to cpak file '" + m_path + "': " + 
                                std::to_string(result) + " of " + std::to_string(size) + " bytes written");
    }
    
    return static_cast<size_t>(result);
}

bool CPakFile::exists() const {
    // Try to open the file for reading to check if it exists
    void* test_handle = cpak_file_open(m_path.c_str(), O_RDONLY);
    if (test_handle) {
        cpak_file_close(test_handle);
        return true;
    }
    return false;
}
