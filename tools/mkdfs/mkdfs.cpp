#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <vector>
#include <string>
#include <filesystem>
#include <iostream>
#include "dfsinternal.h"
#include "../common/binout.c"
#include "../common/binout.h"

namespace fs = std::filesystem;

struct SourceFile {
    std::string path;
    std::vector<uint8_t> data;
};

std::vector<SourceFile> file_all;

void print_help(const char * const prog_name)
{
    fprintf(stderr, "Usage: %s <File> <Directory>\n", prog_name);
    fprintf(stderr, "  where <File> is the resulting filesystem image\n");
    fprintf(stderr, "  and <Directory> is the directory (including subdirectories) to include\n");
}

static uint32_t prime_hash(const char *str, uint32_t prime)
{
    uint32_t hash = 0;
    while(*str) {
        char c = *str++;
        hash = (hash * prime) + c;
    }
    return hash;
}


void read_file(std::string path, std::vector<uint8_t> &data)
{
    FILE *file = fopen(path.c_str(), "rb");
    if(!file) {
        return;
    }
    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    data.resize(size);
    fseek(file, 0, SEEK_SET);
    fread(&data[0], size, 1, file);
    fclose(file);
}

void read_all_files(std::string root_dir)
{
    for (auto & dir_entry : fs::recursive_directory_iterator(root_dir)) {
        if(dir_entry.is_regular_file()) {
            std::string path = dir_entry.path();
            SourceFile file;
            file.path = path.substr(root_dir.length()+1);
            read_file(path, file.data);
            file_all.push_back(file);
        }
    }
}

bool write_dfs(const char *out_file)
{
    FILE *out = fopen(out_file, "wb");
    if(!out) {
        fprintf(stderr, "Cannot create file: %s\n",out_file);
        return false;
    }
    uint32_t num_files = file_all.size();
    w32(out, DFS_MAGIC);
    w32(out, num_files);
    for(uint32_t i=0; i<num_files; i++) {
        w32(out, prime_hash(file_all[i].path.c_str(), 31));
        w32_placeholderf(out, "filedata%zd", i);
        w32(out, file_all[i].data.size());
    }
    for(uint32_t i=0; i<num_files; i++) {
        w32_placeholderf(out, "path%zd", i);
    }
    for(uint32_t i=0; i<num_files; i++) {
        const char *path = file_all[i].path.c_str();
        walign(out, 2);
        placeholder_set(out, "path%zd", i);
        uint16_t length = strlen(path);
        w16(out, length);
        fwrite(path, length, 1, out);
    }
    walign(out, 2);
    placeholder_set(out, "data");
    for(uint32_t i=0; i<num_files; i++) {
        walign(out, 2);
        placeholder_set(out, "filedata%zd", i);
        fwrite(&file_all[i].data[0], file_all[i].data.size(), 1, out);
    }
    fclose(out);
    return true;
}

int main(int argc, char *argv[])
{
    if(argc != 3)
    {
        print_help(argv[0]);
        return -1;
    }
    read_all_files(argv[2]);
    if(!write_dfs(argv[1]))
    {
        return 1;
    }
    return 0;
}
