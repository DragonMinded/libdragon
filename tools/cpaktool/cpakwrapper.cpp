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

ControllerPakWrapper::ControllerPakWrapper(const std::string& filename, const std::string& mode)
    : m_filename(filename)
    , m_file(nullptr)
    , m_num_banks(0)
    , m_file_size(0)
    , m_globals_set(false)
{
    m_file = fopen(filename.c_str(), mode.c_str());
    if (!m_file) {
        return; // isValid() will return false
    }
    
    // Get file size and calculate banks
    calculateBanks();
    
    // Setup global variables for cpaklib
    setupGlobals();
}

ControllerPakWrapper::~ControllerPakWrapper() {
    cleanupGlobals();
    if (m_file) {
        fclose(m_file);
        m_file = nullptr;
    }
}

ControllerPakWrapper::ControllerPakWrapper(ControllerPakWrapper&& other) noexcept
    : m_filename(std::move(other.m_filename))
    , m_file(other.m_file)
    , m_num_banks(other.m_num_banks)
    , m_file_size(other.m_file_size)
    , m_globals_set(other.m_globals_set)
{
    other.m_file = nullptr;
    other.m_globals_set = false;
    
    // Update globals to point to this instance
    if (m_globals_set) {
        setupGlobals();
    }
}

ControllerPakWrapper& ControllerPakWrapper::operator=(ControllerPakWrapper&& other) noexcept {
    if (this != &other) {
        // Clean up current state
        cleanupGlobals();
        if (m_file) {
            fclose(m_file);
        }
        
        // Move from other
        m_filename = std::move(other.m_filename);
        m_file = other.m_file;
        m_num_banks = other.m_num_banks;
        m_file_size = other.m_file_size;
        m_globals_set = other.m_globals_set;
        
        other.m_file = nullptr;
        other.m_globals_set = false;
        
        // Update globals
        if (m_globals_set) {
            setupGlobals();
        }
    }
    return *this;
}

void ControllerPakWrapper::setupGlobals() {
    g_pak = m_file;
    g_num_banks = m_num_banks;
    g_pak_offset = 0;
    m_globals_set = true;
}

void ControllerPakWrapper::cleanupGlobals() {
    if (m_globals_set) {
        g_pak = nullptr;
        g_num_banks = 0;
        g_pak_offset = 0;
        m_globals_set = false;
    }
}

void ControllerPakWrapper::calculateBanks() {
    if (!m_file) return;
    
    struct stat st;
    if (fstat(fileno(m_file), &st) == 0) {
        m_file_size = st.st_size;
        m_num_banks = static_cast<int>(m_file_size / BANK_SIZE);
        if (m_num_banks <= 0) m_num_banks = 1;
    }
}

// Static factory method for creating new pak files
std::unique_ptr<ControllerPakWrapper> ControllerPakWrapper::create(const std::string& filename, 
                                                                   int num_banks, 
                                                                   bool overwrite) {
    // Check if file exists and handle overwrite logic
    struct stat st;
    if (stat(filename.c_str(), &st) == 0 && !overwrite) {
        return nullptr; // File exists and overwrite not allowed
    }
    
    // Calculate total size
    size_t total_size = num_banks * BANK_SIZE;
    
    // Create empty file
    if (!createEmptyFile(filename, total_size)) {
        return nullptr;
    }
    
    // Create wrapper and set number of banks
    auto wrapper = std::make_unique<ControllerPakWrapper>(filename, "r+b");
    if (!wrapper->isValid()) {
        unlink(filename.c_str()); // Remove the file we just created
        return nullptr;
    }
    
    // Override the calculated banks with the requested number
    wrapper->m_num_banks = num_banks;
    wrapper->setupGlobals();
    
    // Format the filesystem
    std::srand(std::time(nullptr)); // Initialize random number generator
    int result = cpakfs_format(JOYPAD_PORT_1, true); // Always erase for fresh format
    if (result < 0) {
        unlink(filename.c_str()); // Remove the file on failure
        return nullptr;
    }
    
    return wrapper;
}

bool ControllerPakWrapper::createEmptyFile(const std::string& filename, size_t size) {
    FILE* file = fopen(filename.c_str(), "wb");
    if (!file) {
        return false;
    }
    
    // Create file of the right size by seeking to the end and writing a byte
    if (size > 0) {
        if (fseek(file, size - 1, SEEK_SET) != 0) {
            fclose(file);
            unlink(filename.c_str());
            return false;
        }
        
        uint8_t zero = 0;
        if (fwrite(&zero, 1, 1, file) != 1) {
            fclose(file);
            unlink(filename.c_str());
            return false;
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
