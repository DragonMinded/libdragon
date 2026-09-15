extern "C" {
#include "H264SwDecApi.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Debug prints */
#define DEBUG(argv) printf argv

#include "opttarget.h"

#include "extraFlags.h"

}

// This is just H264SwDecRet with clearer names
enum StreamStatus {
    PIC_READY                  = 0, // Picture is ready and no more data is on buffer
    STREAM_ENDED               = 1, // Input stream was decoded but no picture is ready, thus get more data
    STREAM_ERROR               = 2  // Internal stream error
};

class Stream {
public:
    Stream();
    ~Stream();

    u8* GetFrame(u32* outImageWidth, u32* outImageHeight);
    void SetStream(u8* strmBuffer, u32 strmLength);
    void UpdateStream(u8* strmBuffer, u32 strmLength);
    StreamStatus BroadwayDecode();

private:
    u8 *streamBuffer = NULL;

    H264SwDecInst decInst;
    H264SwDecInput decInput;
    H264SwDecOutput decOutput;
    H264SwDecPicture decPicture;
    H264SwDecInfo decInfo;

    u32 picDecodeNumber;
    u32 picSize;
    u32 currPackagePos;
    u32 bufferSize;
};


u32 broadwayGetMajorVersion();

u32 broadwayGetMinorVersion();