#include "Decoder.h"

Stream::Stream() {
    H264SwDecRet ret;
#ifdef DISABLE_OUTPUT_REORDERING
    u32 disableOutputReordering = 1;
#else
    u32 disableOutputReordering = 0;
#endif

    // Initialize decoder instance. 
    ret = H264SwDecInit(&decInst, disableOutputReordering);
    if (ret != H264SWDEC_OK) {
        // TODO: stop this Stream object from trying to decode any video!
        // Perhaps use a flag to indicate that the initialization was successful?
        DEBUG(("DECODER INITIALIZATION FAILED\n"));
        return;
    }

    picDecodeNumber = 1;
}

Stream::~Stream() {
    if (streamBuffer) {
        free(streamBuffer);
    }
}

void Stream::SetStream(u8* strmBuffer, u32 strmLength) {
    u8* new_buffer = new u8[strmLength];
    if (new_buffer == NULL) {
        DEBUG(("UNABLE TO ALLOCATE MEMORY\n"));
        exit(1);
    }
    
    memcpy(new_buffer, strmBuffer, strmLength);
    streamBuffer     = new_buffer;
    decInput.pStream = new_buffer;
    decInput.dataLen = strmLength;
    currPackagePos   = 0;
    bufferSize       = strmLength;
}


void Stream::UpdateStream(u8* strmBuffer, u32 strmLength) {
    u32 new_buffer_size = bufferSize - currPackagePos + strmLength;
    u8* new_buffer = new u8[new_buffer_size];
    if (new_buffer == NULL) {
        DEBUG(("UNABLE TO ALLOCATE MEMORY\n"));
        exit(1);
    }

    u32 last_package_size = bufferSize-currPackagePos;
    // keep the current package that is being processed
    memcpy(new_buffer, streamBuffer+currPackagePos, last_package_size);
    // and add the new one
    memcpy(new_buffer+last_package_size, strmBuffer, strmLength);
    free(streamBuffer);
    streamBuffer = new_buffer;

    bufferSize = new_buffer_size;
    currPackagePos = 0;

    // Update the decoder input buffer
    decInput.pStream = streamBuffer;
    decInput.dataLen = new_buffer_size;   
}


u8* Stream::GetFrame(u32* outImageWidth, u32* outImageHeight) {
    *outImageWidth = decInfo.picWidth;
    *outImageHeight = decInfo.picHeight;

    return (u8*)decPicture.pOutputPicture;
}


StreamStatus Stream::BroadwayDecode() {
    decInput.picId = picDecodeNumber;

    H264SwDecRet ret = H264SwDecDecode(decInst, &decInput, &decOutput);
    StreamStatus status;

    switch (ret) {
        case H264SWDEC_HDRS_RDY_BUFF_NOT_EMPTY:
            // Stream headers were successfully decoded, thus stream information is available for query now. 
            ret = H264SwDecGetInfo(decInst, &decInfo);
            if (ret != H264SWDEC_OK) {
                return STREAM_ERROR;
            }

            picSize = decInfo.picWidth * decInfo.picHeight;
            picSize = (3 * picSize) / 2;

            decInput.dataLen -= decOutput.pStrmCurrPos - decInput.pStream;
            decInput.pStream = decOutput.pStrmCurrPos;

            // Try to decode again to get a picture. The user is not interested on the Headers info...
            return BroadwayDecode();

            break;

        case H264SWDEC_PIC_RDY_BUFF_NOT_EMPTY:
            // Picture is ready and more data remains in the input buffer,
            // update input structure.
            decInput.dataLen -= decOutput.pStrmCurrPos - decInput.pStream;
            decInput.pStream = decOutput.pStrmCurrPos;

            currPackagePos = bufferSize - decInput.dataLen;
            picDecodeNumber++;

            while (H264SwDecNextPicture(decInst, &decPicture, 1) == H264SWDEC_PIC_RDY) { }

            break;

        case H264SWDEC_PIC_RDY:
            currPackagePos = bufferSize;
            picDecodeNumber++;

            decInput.dataLen -= decOutput.pStrmCurrPos - decInput.pStream;
            currPackagePos = bufferSize - decInput.dataLen;
            
            while (H264SwDecNextPicture(decInst, &decPicture, 1) == H264SWDEC_PIC_RDY) { }

            break;

        case H264SWDEC_STRM_PROCESSED:
        case H264SWDEC_STRM_ERR:
            // Input stream was decoded but no picture is ready, thus get more data. 
            // decInput.dataLen = 0;
            break;
              
        default:
            break;
    }

    switch (ret) {
        case H264SWDEC_PIC_RDY_BUFF_NOT_EMPTY:
        case H264SWDEC_PIC_RDY:
            status = PIC_READY;
            break;

        case H264SWDEC_STRM_PROCESSED:
            status = STREAM_ENDED;
            break;

        case H264SWDEC_STRM_ERR:
        default:
            status = STREAM_ERROR;
    }


  return status;
}


u32 broadwayGetMajorVersion() {
    return H264SwDecGetAPIVersion().major;
}

u32 broadwayGetMinorVersion() {
    return H264SwDecGetAPIVersion().minor;
}

