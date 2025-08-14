#include "cpakwrapper.h"
#include "cpaktool.h"
#include <sys/stat.h>
#include <cerrno>
#include <cstring>
#include <ctime>
#include <cstdlib>
#include <exception>
#include <unistd.h>

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
