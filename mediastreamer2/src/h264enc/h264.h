#ifndef _H264_DEFINE_H_
#define _H264_DEFINE_H_

//-------------------------------------
// Encoder part
//-------------------------------------
#define FAVC_ENCODER_DEV	"/dev/w55fa92_264enc"
#define FIX_QUANT		0
#define IPInterval		9

//-------------------------------------
// Data structure
//-------------------------------------
typedef struct AVFrame {
    uint8_t *data[4];
} AVFrame;

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
    AVFrame *coded_frame;
    char *priv;
} video_params;

typedef enum nal_unit_type_e
{
    NAL_UNKNOWN     = 0x00000000,
    NAL_SLICE       = 0x00000001,
    NAL_SLICE_DPA   = 0x00000002,
    NAL_SLICE_DPB   = 0x00000003,
    NAL_SLICE_DPC   = 0x00000004,
    NAL_SLICE_IDR   = 0x00000005,    /* ref_idc != 0 */
    NAL_SEI         = 0x00000006,    /* ref_idc == 0 */
    NAL_SPS         = 0x00000007,
    NAL_PPS         = 0x00000008,
    NAL_AUD         = 0x00000009,
    NAL_FILLER      = 0x0000000C,
    /* ref_idc == 0 for 6,9,10,11,12 */
} nal_type;

typedef struct h264_nal_t {

    nal_type  i_type;        /* nal_unit_type_e */
    uint32_t  i_ipoffset;
    uint32_t  i_spsoffset;   /* nal sps offset*/
    uint32_t  i_ppsoffset;   /* nal pps offset*/
    uint32_t  i_iplen;
    uint32_t  i_spslen;      /* nal sps lenght*/
    uint32_t  i_ppslen;      /* nal pps lenght*/
    /* If param->b_annexb is set, Annex-B bytestream with startcode.
     * Otherwise, startcode is replaced with a 4-byte size.
     * This size is the size used in mp4/similar muxing; it is equal to i_payload-4 
    */
    uint8_t  *p_payload;

} h264_nal_t;

uint8_t	*pDecbuf;

int h264_init(video_params *params);
int h264_reinit(video_params *params);
int h264_close(void);
int favc_encode(video_params *params, unsigned char *frame, void * data);


#endif //#ifndef _H264_DEFINE_H_
