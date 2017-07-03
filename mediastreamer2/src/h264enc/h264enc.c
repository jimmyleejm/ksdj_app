/*
mediastreamer2 H264 plugin
Copyright (C) 2006-2010 Belledonne Communications SARL (simon.morlat@linphone.org)

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#ifdef HAVE_CONFIG_H
#include "mediastreamer-config.h"
#endif

#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msticker.h"
#include "mediastreamer2/msvideo.h"
#include "mediastreamer2/rfc3984.h"

#ifdef _MSC_VER
#include <stdint.h>
#endif

#include "h264enc/favc_avcodec.h"
#include "h264enc/ratecontrol.h"
#include "h264enc/favc_version.h"
#include "h264enc/h264.h"


#define RC_MARGIN	10000	/*bits per sec*/

/* the goal of this small object is to tell when to send I frames at startup: at 2 and 4 seconds*/
typedef struct VideoStarter{
	uint64_t next_time;
	int i_frame_count;
}VideoStarter;


static void video_starter_init(VideoStarter *vs){
	vs->next_time=0;
	vs->i_frame_count=0;
}

static void video_starter_first_frame(VideoStarter *vs, uint64_t curtime){
	vs->next_time=curtime+2000;
}

static bool_t video_starter_need_i_frame(VideoStarter *vs, uint64_t curtime){
	if (vs->next_time==0) return FALSE;
	if (curtime>=vs->next_time){
		vs->i_frame_count++;
		if (vs->i_frame_count==1){
			vs->next_time+=2000;
		}else{
			vs->next_time=0;
		}
		return TRUE;
	}
	return FALSE;
}

typedef struct _EncData{
	int enc_fd;
	video_params params;
	MSVideoSize vsize;
	int bitrate;
	float fps;
	int mode;
	uint64_t framenum;
	Rfc3984Context *packer;
	int keyframe_int;
	VideoStarter starter;
	bool_t generate_keyframe;
}EncData;


static void enc_init(MSFilter *f){
	EncData *d=ms_new(EncData,1);
	d->enc_fd=0;
	MS_VIDEO_SIZE_ASSIGN(d->vsize,CIF);
	d->bitrate=384000;
	d->fps=25;
	d->keyframe_int=10; /*10 seconds */
	d->mode=0;
	d->framenum=0;
	d->generate_keyframe=FALSE;
	d->packer=NULL;
	f->data=d;
}

static void enc_uninit(MSFilter *f){
	EncData *d=(EncData*)f->data;
	ms_free(d);
}

static void apply_bitrate(MSFilter *f, int target_bitrate){
	EncData *d=(EncData*)f->data;
	video_params *params=&d->params;
	float bitrate;

	d->bitrate = target_bitrate;

	params->bit_rate = (int)(d->bitrate);

	ms_message("params->bit_rate = %d .\n",  params->bit_rate);	
}

static void enc_preprocess(MSFilter *f){
	EncData *d=(EncData*)f->data;
	video_params *params=&d->params;

	d->packer=rfc3984_new();
	rfc3984_set_mode(d->packer,d->mode);
	rfc3984_enable_stap_a(d->packer,FALSE);

	params->qmax = 40;		// Default value = 51
	params->qmin = 10;		// Default value = 0

#if defined ( __ENABLE_RATE_CONTROL__ )    
	params->quant = 20;		// Init Quant (0 ~ 51)
#else
	params->quant = FIX_QUANT;
#endif    
	params->width = d->vsize.width;
	params->height = d->vsize.height;
	params->framerate = (int)d->fps;
	params->frame_rate_base = 1;
	params->gop_size = IPInterval;

	apply_bitrate(f,d->bitrate);

    printf("params->width=%d,params->height=%d\n",params->width,params->height);
	d->enc_fd = h264_init(params);
	if (d->enc_fd == -1 ) ms_error("Fail to create H264 encoder.");

	d->framenum=0;
	video_starter_init(&d->starter);
}

static void h264_xnals_init( h264_nal_t xnals )
{
	xnals.i_type       = 0;
	xnals.i_ipoffset   = 0;
	xnals.i_iplen      = 0;
	xnals.i_spsoffset  = 0;
	xnals.i_spslen     = 0;
	xnals.i_ppsoffset  = 0;
	xnals.i_ppslen     = 0;
}

static uint8_t *s_pu8IFrameBuf = NULL;
static uint32_t s_u32IFrameBufSize = 0;

static void h264_nals_to_msgb( uint8_t *Buffer, uint32_t num_nals, MSQueue * nalus ) {
	uint32_t uNextSyncWordAdr=0, uBitLen;
	uint32_t uSyncWord = 0x01000000;
	uint32_t uCurFrame = NAL_UNKNOWN;
	uint8_t  u_type;
	uint32_t uFrameLen;
	nal_type eFrameMask;
	uint32_t uFrameOffset;
	h264_nal_t  nals;
	mblk_t *m;

	h264_xnals_init(nals);

	nals.p_payload  = Buffer;

	//ms_message("nals.p_payload[0]  = %d.\n", nals.p_payload[0]);
	//ms_message("nals.p_payload[1]  = %d.\n", nals.p_payload[1]);
	//ms_message("nals.p_payload[2]  = %d.\n", nals.p_payload[2]);
	//ms_message("nals.p_payload[3]  = %d.\n", nals.p_payload[3]);
	//ms_message("nals.i_iplen       = %d.\n", num_nals);

	if((nals.p_payload[4] & 0x1F) == NAL_SLICE_IDR) {
		// It is I-frame
		nals.i_iplen = num_nals;
		nals.i_type = NAL_SLICE_IDR;
		//ms_message("xnals.i_iplen I   = %d.\n", nals.i_iplen);
		//ms_message("xnals.i_type  I   = 0x%x.\n", nals.i_type);
		m=allocb(nals.i_iplen + 10, 0);		
		memcpy(m->b_wptr, (Buffer + 4), nals.i_iplen-4);
		m->b_wptr += nals.i_iplen-4;
		ms_queue_put(nalus,m);

		return;

	} else if ((nals.p_payload[4] & 0x1F) == NAL_SLICE) {
		// It is P-frame
		nals.i_iplen = num_nals;
		nals.i_type = NAL_SLICE;
		//ms_message("xnals.i_iplen P   = %d.\n", nals.i_iplen);
		//ms_message("xnals.i_type  P   = 0x%x.\n", nals.i_type);
		m=allocb(nals.i_iplen + 10, 0);		
		memcpy(m->b_wptr, (Buffer + 4), nals.i_iplen-4);
		m->b_wptr += nals.i_iplen-4;
		ms_queue_put(nalus,m);

		return;

	} else {
		uBitLen = (num_nals - 4); //4:sync word len
		while( uBitLen ) {
			if(memcmp(nals.p_payload + uNextSyncWordAdr, &uSyncWord, sizeof(uint32_t)) == 0) {

				u_type = nals.p_payload[uNextSyncWordAdr + 4] & 0x1F;
				
				if (uCurFrame == NAL_SPS) {
					nals.i_spslen = uNextSyncWordAdr - nals.i_spsoffset;
					//ms_message("uBitLen::nals.i_spslen SPS     = %d.\n", nals.i_spslen);
				} else if (uCurFrame == NAL_PPS) {
					nals.i_ppslen = uNextSyncWordAdr - nals.i_ppsoffset;
					//ms_message("uBitLen::nals.i_ppslen PPS     = %d.\n", nals.i_ppslen);
				}
				
				if(u_type == NAL_SPS) {
					uCurFrame = NAL_SPS;
					nals.i_spsoffset = uNextSyncWordAdr;
					nals.i_type |= NAL_SPS;
					//ms_message("uBitLen::nals.i_spsoffset SPS  = %d.\n", nals.i_spsoffset);
					//ms_message("uBitLen::nals.i_type      SPS  = 0x%x.\n", nals.i_type);
				} else if(u_type == NAL_PPS) {
					uCurFrame =  NAL_PPS;
					nals.i_ppsoffset = uNextSyncWordAdr;
					nals.i_type |= NAL_PPS;
					//ms_message("uBitLen::nals.i_ppsoffset PPS  = %d.\n", nals.i_ppsoffset);
					//ms_message("uBitLen::nals.i_type      PPS  = 0x%x.\n", nals.i_type);
				} else if(u_type == NAL_SLICE_IDR) {
					nals.i_ipoffset = uNextSyncWordAdr;
					nals.i_iplen = num_nals - uNextSyncWordAdr;
					nals.i_type |= NAL_SLICE_IDR;
					//ms_message("uBitLen::nals.i_ipoffset IDR   = %d.\n", nals.i_ipoffset);
					//ms_message("uBitLen::nals.i_iplen    IDR   = %d.\n", nals.i_iplen);
					//ms_message("uBitLen::nals.i_type     IDR   = 0x%x.\n", nals.i_type);
					break;
				} else if(u_type == NAL_SLICE) {
					nals.i_ipoffset = uNextSyncWordAdr;
					nals.i_iplen = num_nals - uNextSyncWordAdr;
					nals.i_type |= NAL_SLICE;
					//ms_message("uBitLen::nals.i_ipoffset P    = %d.\n", nals.i_ipoffset);
					//ms_message("uBitLen::nals.i_iplen    P    = %d.\n", nals.i_iplen);
					//ms_message("uBitLen::nals.i_type     P    = 0x%x.\n", nals.i_type);
					break;
				}
			}
			uNextSyncWordAdr ++;
			uBitLen --;
		}

		if(uBitLen == 0) {
			ms_error("Unknown H264 frame \n");
			uNextSyncWordAdr = 0;
		}
	}

	if(num_nals > s_u32IFrameBufSize){
		if(s_pu8IFrameBuf){
			free(s_pu8IFrameBuf);
			s_u32IFrameBufSize = 0;
		}
		s_pu8IFrameBuf = malloc(num_nals + 100);
		if(s_pu8IFrameBuf){
			s_u32IFrameBufSize = num_nals;
		}
		else{
			s_u32IFrameBufSize = 0;
		}
	}

	if(s_pu8IFrameBuf == NULL){
		ms_error("h264_nals_to_msgb: s_pu8IFrameBuf is null\n");
		return;
	}

	memcpy(s_pu8IFrameBuf, Buffer, num_nals);

	if (nals.i_type & NAL_SLICE) {
		eFrameMask = nals.i_type;
		while(eFrameMask) {
			if(eFrameMask & NAL_SPS){
				uFrameOffset = nals.i_spsoffset + 4;
				uFrameLen = nals.i_spslen - 4 ;
				eFrameMask &= (~NAL_SPS);
			}
			else if (eFrameMask & NAL_PPS){
				uFrameOffset = nals.i_ppsoffset + 4;
				uFrameLen = nals.i_ppslen - 4;
				eFrameMask &= (~NAL_PPS);
			}
			else{
				uFrameOffset = nals.i_ipoffset + 4;
				uFrameLen = nals.i_iplen - 4;
				eFrameMask = 0;
			}
			m=allocb(uFrameLen + 10, 0);
			memcpy(m->b_wptr, (s_pu8IFrameBuf + uFrameOffset), uFrameLen);
			m->b_wptr += uFrameLen;
			ms_queue_put(nalus,m);
		}
	} else {
		uFrameOffset = nals.i_ipoffset + 4;
		uFrameLen = nals.i_iplen - 4;
		m=allocb(uFrameLen + 10, 0);		
		memcpy(m->b_wptr, (s_pu8IFrameBuf + uFrameOffset), uFrameLen);
		m->b_wptr += uFrameLen;
		ms_queue_put(nalus,m);
	}
}

static void enc_process(MSFilter *f) {
	EncData *d=(EncData*)f->data;
	video_params *params=&d->params;
	uint32_t ts=f->ticker->time*90LL;
	uint32_t yimage_size;

	mblk_t *im;
	MSPicture pic;
	MSQueue nalus;
	ms_queue_init(&nalus);

	while((im=ms_queue_get(f->inputs[0]))!=NULL) {
		if (ms_yuv_buf_init_from_mblk(&pic,im)==0) {

			AVFrame xpic;
			int length = 0;

			/*send I frame 2 seconds and 4 seconds after the beginning */
			if (video_starter_need_i_frame(&d->starter,f->ticker->time))
				d->generate_keyframe=TRUE;

			if( u32BufPhyAddr != NULL) {

				yimage_size = d->vsize.width * d->vsize.height;
				xpic.data[0]=(uint8_t *)(u32BufPhyAddr);
				xpic.data[1]=(uint8_t *)(u32BufPhyAddr + yimage_size);
				xpic.data[2]=(uint8_t *)(u32BufPhyAddr + yimage_size + (yimage_size >> 2));
				xpic.data[3]=NULL;

				length = favc_encode(params, pDecbuf, (void *)&xpic); 

				if (length > 0) {
					h264_nals_to_msgb(pDecbuf, length, &nalus);
					rfc3984_pack(d->packer,&nalus,f->outputs[0],ts);
					d->framenum++;
					if (d->framenum==0)
						video_starter_first_frame(&d->starter,f->ticker->time);
				} else {
					ms_error("h264_encoder_encode() error.");
					h264_close();
					d->enc_fd = h264_init(params);
				}
			}
		}
		freemsg(im);
	}
}

static void enc_postprocess(MSFilter *f){
	EncData *d=(EncData*)f->data;
	rfc3984_destroy(d->packer);
	d->packer=NULL;
	h264_close();
	d->enc_fd=0;
}

static int enc_get_br(MSFilter *f, void*arg){
	EncData *d=(EncData*)f->data;
	*(int*)arg=d->bitrate;
	return 0;
}

static int enc_set_br(MSFilter *f, void *arg){
	EncData *d=(EncData*)f->data;
	d->bitrate=*(int*)arg;

	if (d->enc_fd){
		ms_filter_lock(f);
		apply_bitrate(f,d->bitrate);
		if ((d->enc_fd = h264_reinit(&d->params)) == -1 ) {
			ms_error("H264_encoder_reconfig() failed.");
		}
		ms_filter_unlock(f);
		return 0;
	}
	
	if (d->bitrate>=1024000){
		d->vsize.width = MS_VIDEO_SIZE_VGA_W;	//MS_VIDEO_SIZE_SVGA_W;
		d->vsize.height = MS_VIDEO_SIZE_VGA_H;
		d->fps=18;
	}else if (d->bitrate>=512000){
		d->vsize.width = MS_VIDEO_SIZE_VGA_W;	//MS_VIDEO_SIZE_VGA_W;
		d->vsize.height = MS_VIDEO_SIZE_VGA_H;
		d->fps=18;
	} else if (d->bitrate>=256000){
		d->vsize.width = MS_VIDEO_SIZE_VGA_W;	//S_VIDEO_SIZE_VGA_W;
		d->vsize.height = MS_VIDEO_SIZE_VGA_H;
		d->fps=18;
	}else if (d->bitrate>=170000){
		d->vsize.width=MS_VIDEO_SIZE_CIF_W;
		d->vsize.height=MS_VIDEO_SIZE_CIF_H;
		d->fps=25;
	}else if (d->bitrate>=128000){
		d->vsize.width=MS_VIDEO_SIZE_QVGA_W;
		d->vsize.height=MS_VIDEO_SIZE_QVGA_H;
		d->fps=25;
	}else if (d->bitrate>=64000){
		d->vsize.width=MS_VIDEO_SIZE_QCIF_W;
		d->vsize.height=MS_VIDEO_SIZE_QCIF_H;
		d->fps=15;
	}else{
		d->vsize.width=MS_VIDEO_SIZE_QCIF_W;
		d->vsize.height=MS_VIDEO_SIZE_QCIF_H;
		d->fps=15;
	}	

	d->fps=10; //jimmy ++ fps set
	ms_message("bitrate requested...: %d (%d x %d)\n", d->bitrate, d->vsize.width, d->vsize.height);
	return 0;
}

static int enc_set_fps(MSFilter *f, void *arg){
	EncData *d=(EncData*)f->data;
	d->fps=*(float*)arg;
	return 0;
}

static int enc_get_fps(MSFilter *f, void *arg){
	EncData *d=(EncData*)f->data;
	*(float*)arg=d->fps;
	return 0;
}

static int enc_get_vsize(MSFilter *f, void *arg){
	EncData *d=(EncData*)f->data;
	*(MSVideoSize*)arg=d->vsize;
	return 0;
}

static int enc_set_vsize(MSFilter *f, void *arg){
	EncData *d=(EncData*)f->data;

	d->vsize=*(MSVideoSize*)arg;

	return 0;
}

static int enc_add_fmtp(MSFilter *f, void *arg){
	EncData *d=(EncData*)f->data;
	const char *fmtp=(const char *)arg;
	char value[12];
	if (fmtp_get_value(fmtp,"packetization-mode",value,sizeof(value))){
		d->mode=atoi(value);

	}
//jimmy ++ there slave the problem for pc
	    printf("packetization-mode set to %i",d->mode);
	//	d->mode=1;

	return 0;
}

static int enc_req_vfu(MSFilter *f, void *arg){
	EncData *d=(EncData*)f->data;
	d->generate_keyframe=TRUE;
	return 0;
}


static MSFilterMethod enc_methods[]={
	{	MS_FILTER_SET_FPS	,	enc_set_fps	},
	{	MS_FILTER_SET_BITRATE	,	enc_set_br	},
	{	MS_FILTER_GET_BITRATE	,	enc_get_br	},
	{	MS_FILTER_GET_FPS	,	enc_get_fps	},
	{	MS_FILTER_GET_VIDEO_SIZE,	enc_get_vsize	},
	{	MS_FILTER_SET_VIDEO_SIZE,	enc_set_vsize	},
	{	MS_FILTER_ADD_FMTP	,	enc_add_fmtp	},
	{	MS_FILTER_REQ_VFU	,	enc_req_vfu	},
	{	0			,	NULL		}
};

#ifndef _MSC_VER

MSFilterDesc ms_h264_enc_desc={
	.id=MS_H264_ENC_ID,
	.name="MSH264Enc",
	.text="A H264 encoder based on N32926UxDN FAVC Codec IPCore",
	.category=MS_FILTER_ENCODER,
	.enc_fmt="H264",
	.ninputs=1,
	.noutputs=1,
	.init=enc_init,
	.preprocess=enc_preprocess,
	.process=enc_process,
	.postprocess=enc_postprocess,
	.uninit=enc_uninit,
	.methods=enc_methods
};

#else

MSFilterDesc ms_h264_enc_desc={
	MS_H264_ENC_ID,
	"MSH264Enc",
	"A H264 encoder based on N32926UxDN FAVC Codec IPCore",
	MS_FILTER_ENCODER,
	"H264",
	1,
	1,
	enc_init,
	enc_preprocess,
	enc_process,
	enc_postprocess,
	enc_uninit,
	enc_methods
};

#endif

MS_FILTER_DESC_EXPORT(ms_h264_enc_desc)

