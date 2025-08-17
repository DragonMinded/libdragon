/*
    cpaktool - Controller Pak manipulation tool
    Written by Giovanni Bajo <giovannibajo@gmail.com>

    This tool is part of the Libdragon SDK.

    This is free and unencumbered software released into the public domain.

    For more information, please refer to <http://unlicense.org/>
*/
#pragma once
#include "cpaktool.h"
#include "../../src/joybus/cpakfs_internal.h"
#include <cstdio>
#include <cstdint>
#include <string>
#include <memory>
#include <functional>

class CPakFilesystem {
private:
    static constexpr size_t DEXDRIVE_HEADER_SIZE = 0x1040;
    static constexpr const char DEXDRIVE_SIGNATURE[] = "123-456-STD";

public:
    // Constructor opens the pak file and mounts the filesystem
    // skip_header_bytes: >= 0 = skip N bytes, < 0 = auto-detect DexDrive format
    explicit CPakFilesystem(const std::string& filename, bool auto_mount = true, 
                           int skip_header_bytes = -1);
    
    // Destructor automatically unmounts filesystem and closes the file
    ~CPakFilesystem();
    
    // Delete copy constructor and assignment operator to prevent copying
    CPakFilesystem(const CPakFilesystem&) = delete;
    CPakFilesystem& operator=(const CPakFilesystem&) = delete;
    
    // Move constructor and assignment operator
    CPakFilesystem(CPakFilesystem&& other) noexcept;
    CPakFilesystem& operator=(CPakFilesystem&& other) noexcept;
    
    // Get number of banks detected from file size
    int getNumBanks() const { return m_num_banks; }
    
    // Get file size in bytes
    size_t getFileSize() const { return m_file_size; }
    
    // Low-level file operations (if needed)
    FILE* getFileHandle() const { return m_file; }
    
    // Callback function type for file iteration
    using FileCallback = std::function<bool(const char* filename, const dir_t& dir_entry)>;
    
    // Iterate through all files in the pak, calling callback for each
    // Callback should return true to continue iteration, false to stop
    void for_each_file(const FileCallback& callback) const;
    
    // Manual filesystem mounting/unmounting
    bool mountFilesystem();
    void unmountFilesystem();
    
    // Static factory method for creating new pak files
    static std::unique_ptr<CPakFilesystem> create(const std::string& filename, 
                                                  int num_banks, 
                                                  bool overwrite = false);
    
private:
    void setupGlobals();
    void cleanupGlobals();
    void calculateBanks();
    bool detectDexDriveFormat();
    static bool createEmptyFile(const std::string& filename, size_t size);
    
    std::string m_filename;
    FILE* m_file;
    int m_num_banks;
    size_t m_file_size;
    size_t m_pak_offset;
    bool m_globals_set;
    bool m_filesystem_mounted;
};

// RAII wrapper for cpak file operations
class CPakFile {
public:
    // Constructor opens the file
    explicit CPakFile(const std::string& path, int flags);
    
    // Destructor automatically closes the file
    ~CPakFile();
    
    // Delete copy constructor and assignment operator to prevent copying
    CPakFile(const CPakFile&) = delete;
    CPakFile& operator=(const CPakFile&) = delete;
    
    // Move constructor and assignment operator
    CPakFile(CPakFile&& other) noexcept;
    CPakFile& operator=(CPakFile&& other) noexcept;
    
    // Check if the file is valid and open
    bool isValid() const { return m_handle != nullptr; }
    
    // File operations with exception handling
    size_t read(void* buffer, size_t size);
    size_t write(const void* buffer, size_t size);
    
    // Utility methods
    bool exists() const;
    
private:
    void* m_handle;
    std::string m_path;
    int m_flags;
};
