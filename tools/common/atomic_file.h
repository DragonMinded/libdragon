#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <stdexcept>
#include <unistd.h>    // getpid(), unlink
#include <sys/stat.h>  // mode constants
#include <fcntl.h>     // open, O_CREAT, O_EXCL
#include <errno.h>
#include <inttypes.h>  // PRIx64
#include "nanotime.h"  // nanotime()


class AtomicFile {
public:
    AtomicFile(const std::string &finalPath, const std::string &tempDir = "")
        : finalPath_(finalPath), committed_(false)
    {
        std::string dir;
        if (!tempDir.empty()) {
            dir = tempDir;
        } else {
            auto pos = finalPath.find_last_of('/');
            dir = (pos == std::string::npos) ? "." : finalPath.substr(0, pos);
        }

        char tmp[1024];
        snprintf(tmp, sizeof(tmp), "%s/%x.%" PRIx64 ".tmp", 
                dir.c_str(), getpid(), nanotime());
        tmpPath_ = tmp;

        // Create a temporary file atomically
        int fd = ::open(tmpPath_.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0644);
        if (fd < 0) {
            throw std::runtime_error("Cannot create temporary file " + tmpPath_ + ": " + strerror(errno));
        }

        // Associate FILE*
        file_ = ::fdopen(fd, "wb");
        if (!file_) {
            ::close(fd);
            ::unlink(tmpPath_.c_str());
            throw std::runtime_error("fdopen() failed for " + tmpPath_ + ": " + strerror(errno));
        }
    }

    ~AtomicFile() {
        if (!committed_) {
            if (file_) fclose(file_);
            ::unlink(tmpPath_.c_str());
        }
    }

    FILE *stream() { return file_; }

    void commit() {
        if (committed_) return;
        fflush(file_);
        fclose(file_);
        file_ = nullptr;

        if (::rename(tmpPath_.c_str(), finalPath_.c_str()) != 0) {
            throw std::runtime_error("Cannot rename " + tmpPath_ + " to " + finalPath_ + ": " + strerror(errno));
        }
        committed_ = true;
    }

private:
    std::string finalPath_;
    std::string tmpPath_;
    FILE *file_ = nullptr;
    bool committed_;
};
