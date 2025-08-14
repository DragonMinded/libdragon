#pragma once
#include "cpaktool.h"
#include "../../src/joybus/cpakfs_internal.h"
#include <cstdio>
#include <cstdint>
#include <string>
#include <memory>

class ControllerPakWrapper {
public:
    // Constructor opens the pak file and sets up the environment
    explicit ControllerPakWrapper(const std::string& filename, const std::string& mode = "r+b");
    
    // Destructor automatically closes the file and cleans up
    ~ControllerPakWrapper();
    
    // Delete copy constructor and assignment operator to prevent copying
    ControllerPakWrapper(const ControllerPakWrapper&) = delete;
    ControllerPakWrapper& operator=(const ControllerPakWrapper&) = delete;
    
    // Move constructor and assignment operator
    ControllerPakWrapper(ControllerPakWrapper&& other) noexcept;
    ControllerPakWrapper& operator=(ControllerPakWrapper&& other) noexcept;
    
    // Check if the pak file is valid and open
    bool isValid() const { return m_file != nullptr; }
    
    // Get number of banks detected from file size
    int getNumBanks() const { return m_num_banks; }
    
    // Get file size in bytes
    size_t getFileSize() const { return m_file_size; }
    
    // Low-level file operations (if needed)
    FILE* getFileHandle() const { return m_file; }
    
    // Static factory method for creating new pak files
    static std::unique_ptr<ControllerPakWrapper> create(const std::string& filename, 
                                                        int num_banks, 
                                                        bool overwrite = false);
    
private:
    void setupGlobals();
    void cleanupGlobals();
    void calculateBanks();
    static bool createEmptyFile(const std::string& filename, size_t size);
    
    std::string m_filename;
    FILE* m_file;
    int m_num_banks;
    size_t m_file_size;
    bool m_globals_set;
};
