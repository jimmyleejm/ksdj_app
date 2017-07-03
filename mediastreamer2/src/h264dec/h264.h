#ifndef _H264_DECDEFINE_H_
#define _H264_DECDEFINE_H_

//-------------------------------------
// Decoder part
//-------------------------------------
#define FAVC_DECODER_DEV	"/dev/w55fa92_264dec"


#define MAX_WIDTH		640
#define MAX_HEIGHT		480

//-------------------------------------
// Data structure
//-------------------------------------
typedef struct AVFrame {
    uint8_t *data[4];
} AVFrame;

typedef struct AVPacket {
    uint8_t *data;
    int     size;
} AVPacket;

typedef struct video_params
{
    unsigned int bit_rate;
    unsigned int width;   //length per dma buffer
    unsigned int height;
    unsigned int framerate;
    unsigned int frame_rate_base;
    unsigned int gop_size;
    unsigned int qmax;
    unsigned int qmin;
    unsigned int quant;
    unsigned int intra;
} video_params;

#endif //#ifndef _H264_DECDEFINE_H_

