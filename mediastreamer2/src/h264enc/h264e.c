/****************************************************************************
 *                                                                          *
 * Copyright (c) 2014 Nuvoton Technology Corp. All rights reserved.         *
 *                                                                          *
 ***************************************************************************/
 
/****************************************************************************
 * 
 * FILENAME
 *     h264e.c
 *
 * VERSION
 *     1.0
 *
 * HISTORY
 *     2014/06/30		 Ver 1.0 Created by BA20 YBWang
 *
 * REMARK
 *     None
 **************************************************************************/
#include <unistd.h>  
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <string.h>
#include <linux/videodev.h>
#include <linux/videodev2.h>
#include <sys/mman.h>
#include <errno.h>
#include <inttypes.h>

#include "favc_avcodec.h"
#include "ratecontrol.h"
#include "favc_version.h"
#include "h264.h"

//#define	WRITE_FILE		1

#ifdef	WRITE_FILE
	#define dout_name1	"/mnt/nand1-2/encQVGA.264"
#endif

FILE			*din;
FILE			*dout, *dout2, *dout_info;
int			favc_enc_fd=0;
int			tlength=0;
int			writebyte; 
H264RateControl		h264_ratec;
static int		favc_quant=0;
uint8_t			*pDecbuf = NULL;
avc_workbuf_t		bitstreambuf;


typedef unsigned long long uint64;


int h264_init(video_params *params)
{
    FAVC_ENC_PARAM     enc_param;

    if( favc_enc_fd == 0 ) {
        favc_enc_fd=open(FAVC_ENCODER_DEV,O_RDWR);
    }

    if( favc_enc_fd <= 0 ) {
        printf("Failed to open %s\n",FAVC_ENCODER_DEV);
        return -1;
    }

    //-----------------------------------------------
    //  driver handle 1
    //-----------------------------------------------    
    // Get Bitstream Buffer Information	
    if(ioctl(favc_enc_fd, FAVC_IOCTL_ENCODE_GET_BSINFO, &bitstreambuf) < 0) {
        printf("Get avc Enc bitstream info fail\n");
        return -1;
    }	

    //printf("Get Enc BS Buffer Physical addr = 0x%x, size = 0x%x,\n",bitstreambuf.addr,bitstreambuf.size);
	
    pDecbuf = mmap(NULL, bitstreambuf.size, PROT_READ|PROT_WRITE, MAP_SHARED, favc_enc_fd, bitstreambuf.offset);

    if( pDecbuf == MAP_FAILED ) {
        printf("Map ENC Bitstream Buffer Failed!\n");
        return -1;;
    }
	
    //printf("Mapped ENC Bitstream Buffer Virtual addr = 0x%x\n", pDecbuf);

    //-----------------------------------------------
    //  Issue Encode parameter to driver handle 1 & 2
    //-----------------------------------------------    
    memset(&enc_param, 0, sizeof(FAVC_ENC_PARAM));

    enc_param.u32API_version = H264VER;  
    enc_param.u32FrameWidth  = params->width;
    enc_param.u32FrameHeight = params->height;
   
    enc_param.fFrameRate     = (float)params->framerate/(float)params->frame_rate_base;
    enc_param.u32IPInterval  = params->gop_size; //60, IPPPP.... I, next I frame interval
    enc_param.u32MaxQuant    = params->qmax;
    enc_param.u32MinQuant    = params->qmin;
    enc_param.u32Quant       = params->quant; //32
    enc_param.u32BitRate     = params->bit_rate;
    enc_param.ssp_output     = -1;
    enc_param.intra          = -1;
    enc_param.bROIEnable     = 0;
    enc_param.u32ROIX        = 0;
    enc_param.u32ROIY        = 0;
    enc_param.u32ROIWidth    = 0;
    enc_param.u32ROIHeight   = 0;

#if defined ( __ENABLE_RATE_CONTROL__ )
    memset(&h264_ratec, 0, sizeof(H264RateControl));
    H264RateControlInit(&h264_ratec, enc_param.u32BitRate,
        RC_DELAY_FACTOR,RC_AVERAGING_PERIOD, RC_BUFFER_SIZE_BITRATE, 
        (int)enc_param.fFrameRate,
        (int) enc_param.u32MaxQuant, 
        (int)enc_param.u32MinQuant,
        (unsigned int)enc_param.u32Quant, 
        enc_param.u32IPInterval);
#endif

    favc_quant = params->quant;

    //printf("Application : FAVC_IOCTL_ENCODE_INIT\n");

    if (ioctl(favc_enc_fd, FAVC_IOCTL_ENCODE_INIT, &enc_param) < 0) {
        printf("Handler_1 Error to set FAVC_IOCTL_ENCODE_INIT\n");
        return -1;
    }
#if defined(WRITE_FILE)
    tlength = 0;
    writebyte = 0; 

    dout=fopen(dout_name1,"wb");
    printf("Use encoder output name %s\n",dout_name1);
#endif
    
    return favc_enc_fd;
}

int h264_reinit(video_params *params)
{
    FAVC_ENC_PARAM     enc_param;

    //-----------------------------------------------
    //  Issue Encode parameter to driver handle 1 & 2
    //-----------------------------------------------    
    memset(&enc_param, 0, sizeof(FAVC_ENC_PARAM));

    enc_param.u32API_version = H264VER;  
    enc_param.u32FrameWidth  = params->width;
    enc_param.u32FrameHeight = params->height;
   
    enc_param.fFrameRate     = (float)params->framerate/(float)params->frame_rate_base;
    enc_param.u32IPInterval  = params->gop_size; //60, IPPPP.... I, next I frame interval
    enc_param.u32MaxQuant    = params->qmax;
    enc_param.u32MinQuant    = params->qmin;
    enc_param.u32Quant       = params->quant; //32
    enc_param.u32BitRate     = params->bit_rate;
    enc_param.ssp_output     = -1;
    enc_param.intra          = -1;
    enc_param.bROIEnable     = 0;
    enc_param.u32ROIX        = 0;
    enc_param.u32ROIY        = 0;
    enc_param.u32ROIWidth    = 0;
    enc_param.u32ROIHeight   = 0;

#if defined ( __ENABLE_RATE_CONTROL__ )
    memset(&h264_ratec, 0, sizeof(H264RateControl));
    H264RateControlInit(&h264_ratec, enc_param.u32BitRate,
        RC_DELAY_FACTOR,RC_AVERAGING_PERIOD, RC_BUFFER_SIZE_BITRATE, 
        (int)enc_param.fFrameRate,
        (int) enc_param.u32MaxQuant, 
        (int)enc_param.u32MinQuant,
        (unsigned int)enc_param.u32Quant, 
        enc_param.u32IPInterval);
#endif

    favc_quant = params->quant;

    //printf("Application : FAVC_IOCTL_ENCODE_INIT\n");

    if (ioctl(favc_enc_fd, FAVC_IOCTL_ENCODE_INIT, &enc_param) < 0) {
        printf("Handler_1 Error to set FAVC_IOCTL_ENCODE_INIT\n");
        return -1;
    }

    return favc_enc_fd;
}



int h264_close(void)
{
    //printf("Enter enc_close\n");       
    if(pDecbuf) {
        munmap((void *)pDecbuf, bitstreambuf.size);
    } 

    if(favc_enc_fd) {
        close(favc_enc_fd);
    }

    favc_enc_fd = 0;

#if defined(WRITE_FILE)
    fclose(dout);
#endif   
    return 0;
}

int favc_encode(video_params *params, unsigned char *frame, void * data)
{
    AVFrame           *pict=(AVFrame *)data;
    FAVC_ENC_PARAM    enc_param;

    enc_param.pu8YFrameBaseAddr = (unsigned char *)pict->data[0];   //input user continued virtual address (Y), Y=0 when NVOP
    enc_param.pu8UFrameBaseAddr = (unsigned char *)pict->data[1];   //input user continued virtual address (U)
    enc_param.pu8VFrameBaseAddr = (unsigned char *)pict->data[2];   //input user continued virtual address (V)

    enc_param.bitstream = frame;  //output User Virtual address   
    enc_param.ssp_output = -1;
    enc_param.intra = -1;
    enc_param.u32IPInterval = 0; // use default IPInterval that set in INIT

    enc_param.u32Quant = favc_quant;
    enc_param.bitstream_size = 0;

    if (ioctl(favc_enc_fd, FAVC_IOCTL_ENCODE_FRAME, &enc_param) < 0)
    {
        printf("Dev =%d Error to set FAVC_IOCTL_ENCODE_FRAME\n", favc_enc_fd);
        return 0;
    }

#if defined ( __ENABLE_RATE_CONTROL__ )
    if (enc_param.keyframe == 0) {
        H264RateControlUpdate(&h264_ratec, enc_param.u32Quant, enc_param.bitstream_size , 0);
    } else  {
        H264RateControlUpdate(&h264_ratec, enc_param.u32Quant, enc_param.bitstream_size , 1);
    }
    favc_quant = h264_ratec.rtn_quant;
#endif

    params->intra = enc_param.keyframe;

#if defined(WRITE_FILE)
    writebyte = fwrite((void *)frame, sizeof(char), enc_param.bitstream_size, dout);   
    tlength += writebyte;  
#endif

    return enc_param.bitstream_size;
}


