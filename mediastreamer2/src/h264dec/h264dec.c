/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2010  Belledonne Communications SARL 
Author: Simon Morlat <simon.morlat@linphone.org>

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
#include "mediastreamer2/rfc3984.h"
#include "mediastreamer2/msvideo.h"
#include "mediastreamer2/msticker.h"

#include <stdio.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/videodev.h>
#include <linux/fb.h>
#include "ortp/b64.h"

#include "h264dec/favc_avcodec.h"
#include "h264dec/favc_version.h"
#include "h264dec/h264.h"
#include "h264dec/fbvideo.h"

int ins_call_flag=0;

typedef struct _DecData{
	mblk_t *yuv_msg;
	mblk_t *sps,*pps;
	Rfc3984Context unpacker;
	MSPicture 	outbuf;
	avc_workbuf_t	outputbuf;
	video_params video_setting;
	int m_favc_dec_fd;

	void* output_vir_buf;
	int toggle_flag;
	unsigned int packet_num;
	uint8_t *bitstream;
	int bitstream_size;
	uint64_t last_error_reported_time;
	bool_t first_image_decoded;
	ms_mutex_t	m_nvt_mutex;
}DecData;


static int nvt_h264_fini(DecData *d)
{
	ms_mutex_lock(&d->m_nvt_mutex);
	if ( d->output_vir_buf ) {
		munmap((char *)d->output_vir_buf, d->outputbuf.size);
		d->output_vir_buf = (void*)MAP_FAILED;
		d->outputbuf.size = 0;
	}        
        
	if ( d->m_favc_dec_fd > 0 ) {
		close(d->m_favc_dec_fd);
		d->m_favc_dec_fd = -1;
	}

	ms_mutex_unlock(&d->m_nvt_mutex);

	return 0;
}

static int nvt_h264_init(DecData *d)
{
	FAVC_DEC_PARAM      tDecParam;
	video_params 	*video_setting = &(d->video_setting);

	ms_mutex_lock(&d->m_nvt_mutex);
	// Open H264 encoder device
	d->m_favc_dec_fd = open(FAVC_DECODER_DEV, O_RDWR);
	if( d->m_favc_dec_fd < 0 ){
		ms_message("Fail to open %s\n",FAVC_DECODER_DEV);	
		goto open_dec_fail;
	}
	// Get bitstream information
	if( ioctl(d->m_favc_dec_fd, FAVC_IOCTL_DECODE_GET_BSSIZE, &d->bitstream_size) < 0 ){
		ms_message("H264 decoder FAVC_IOCTL_ENCODE_GET_BSINFO failed");
		close(d->m_favc_dec_fd);
		goto open_dec_fail;
	}
	// Get output buffer information
	if( ioctl(d->m_favc_dec_fd, FAVC_IOCTL_DECODE_GET_OUTPUT_INFO, &d->outputbuf) < 0 ){
		ms_message("H264 decoder FAVC_IOCTL_DECODE_GET_OUTPUT_INFO failed");
		close(d->m_favc_dec_fd);
		goto open_dec_fail;
	}
	if ( (d->output_vir_buf=mmap(NULL, d->outputbuf.size, PROT_READ|PROT_WRITE, MAP_SHARED, d->m_favc_dec_fd, 0) )  == MAP_FAILED ) {
		ms_message("Map Output Buffer Failed!\n");
		close(d->m_favc_dec_fd);
		goto open_dec_fail;
	}
	if (d->outputbuf.size ==0) {
		nvt_h264_fini(d);
		goto open_dec_fail;
	}
	ms_message("d->outputbuf.size= %d\n", d->outputbuf.size);
	memset(&tDecParam, 0, sizeof(FAVC_DEC_PARAM));
	tDecParam.u32API_version 	= H264VER;
	tDecParam.u32MaxWidth 		= video_setting->width;
	tDecParam.u32MaxHeight 		= video_setting->height;

	// For file output or VPE convert planr to packet format
	tDecParam.u32FrameBufferWidth 	= video_setting->width;
	tDecParam.u32FrameBufferHeight 	= video_setting->height;

	// 1->Packet YUV422 format, 0-> planar YUV420 foramt
	tDecParam.u32OutputFmt = 0; 

	// Output : Packet YUV422 or Planar YUV420
	if ( ioctl(d->m_favc_dec_fd,FAVC_IOCTL_DECODE_INIT, &tDecParam) < 0) {
	        ms_message("FAVC_IOCTL_DECODE_INIT: memory allocation failed\n");
		nvt_h264_fini(d);
		goto open_dec_fail;
	}

	ms_mutex_unlock(&d->m_nvt_mutex);
	return 0;	// Success

open_dec_fail:
	d->m_favc_dec_fd=-1;
	ms_mutex_unlock(&d->m_nvt_mutex);
	return -1;	// Failed
}

static int favc_decode(DecData *d, unsigned char *frame, void *data, int size)
{
	MSPicture           *pict=(MSPicture *)data;
	FAVC_DEC_PARAM      tDecParam;

	int ret_value=0;

	ms_mutex_lock(&d->m_nvt_mutex);
	if ( d->m_favc_dec_fd <= 0 ) {
		ms_mutex_unlock(&d->m_nvt_mutex);
		return -1;
	}
    
	tDecParam.pu8Display_addr[0] = (unsigned int)pict->planes[0];
	tDecParam.pu8Display_addr[1] = (unsigned int)pict->planes[1];
	tDecParam.pu8Display_addr[2] = (unsigned int)pict->planes[2];
/*
	printf("pu8Display_addr[0] is %x\n",tDecParam.pu8Display_addr[0]);
    printf("pu8Display_addr[1] is %x\n",tDecParam.pu8Display_addr[1]);
    printf("pu8Display_addr[2] is %x\n",tDecParam.pu8Display_addr[2]);*/
	tDecParam.u32Pkt_size =	(unsigned int)size;
	tDecParam.pu8Pkt_buf = frame;

	tDecParam.crop_x = 0;
	tDecParam.crop_y = 0;
    
	tDecParam.u32OutputFmt = 0; // 1->Packet YUV422 format, 0-> planar YUV420 foramt

	if( ioctl(d->m_favc_dec_fd, FAVC_IOCTL_DECODE_FRAME, &tDecParam) < 0)
	{
	        ms_message("FAVC_IOCTL_DECODE_FRAME: Failed.ret=%x\n", ret_value);
		ms_mutex_unlock(&d->m_nvt_mutex);
		nvt_h264_fini(d);
        	return -1;
	}
   // printf("tDecParam.tResult.u32Width=%d,tDecParam.tResult.u32Height=%d\n",tDecParam.tResult.u32Width,tDecParam.tResult.u32Height);
	pict->w=tDecParam.tResult.u32Width;//320;
	pict->h=tDecParam.tResult.u32Height;//240;

	ms_mutex_unlock(&d->m_nvt_mutex);
	if (tDecParam.tResult.isDisplayOut ==0)
		return 0;
	else
    		return tDecParam.got_picture;
}


static void dec_open(DecData *d){

	if ( nvt_h264_init( d ) < 0)
	        ms_message("Failed on nuvoton H264 codec initialize\n");
}

static void dec_init(MSFilter *f){

	printf("dec_init!!!\n");
	if ( InitDisplay() < 0 ) {        
		ms_message("Failed on Display initialize\n");
		return;
	}
	DecData *d=(DecData*)ms_new(DecData,1);
	d->video_setting.width = d->video_setting.height = -1;

	d->yuv_msg=NULL;
	d->sps=NULL;
	d->pps=NULL;
	rfc3984_init(&d->unpacker);
	ms_mutex_init(&d->m_nvt_mutex,NULL);
	d->packet_num=0;

	d->output_vir_buf = (void*)MAP_FAILED;
	d->m_favc_dec_fd=-1;

	dec_open(d);
	d->outbuf.w=0;
	d->outbuf.h=0;
	d->bitstream_size=32768;
	d->bitstream=ms_malloc0(d->bitstream_size);
	d->last_error_reported_time=0;
	d->toggle_flag=0;
	f->data=d;
}

static void dec_preprocess(MSFilter* f) {
	DecData *s=(DecData*)f->data;
	s->first_image_decoded = FALSE;
}

static void dec_reinit(DecData *d){
	nvt_h264_fini(d);
	dec_open(d);
}

static void dec_uninit(MSFilter *f){
	DecData *d=(DecData*)f->data;
	printf("dec_uninit!!!\n");
	rfc3984_uninit(&d->unpacker);

	FiniDisplay();
	//jimmy +++ for bug
	nvt_h264_fini(d);

	ms_mutex_destroy(&d->m_nvt_mutex);

	if (d->yuv_msg) freemsg(d->yuv_msg);
	if (d->sps) freemsg(d->sps);
	if (d->pps) freemsg(d->pps);

	ms_free(d->bitstream);

	ms_free(d);

}

static void update_sps(DecData *d, mblk_t *sps){
	if (d->sps)
		freemsg(d->sps);
	d->sps=dupb(sps);
}

static void update_pps(DecData *d, mblk_t *pps){
	if (d->pps)
		freemsg(d->pps);
	if (pps) d->pps=dupb(pps);
	else d->pps=NULL;
}


static bool_t check_sps_change(DecData *d, mblk_t *sps){
	bool_t ret=FALSE;
	if (d->sps){
		ret=(msgdsize(sps)!=msgdsize(d->sps)) || (memcmp(d->sps->b_rptr,sps->b_rptr,msgdsize(sps))!=0);
		if (ret) {
			ms_message("SPS changed ! %i,%i",msgdsize(sps),msgdsize(d->sps));
			update_sps(d,sps);
			update_pps(d,NULL);
		}
	} else {
		ms_message("Receiving first SPS");
		update_sps(d,sps);
	}
	return ret;
}

static bool_t check_pps_change(DecData *d, mblk_t *pps){
	bool_t ret=FALSE;
	if (d->pps){
		ret=(msgdsize(pps)!=msgdsize(d->pps)) || (memcmp(d->pps->b_rptr,pps->b_rptr,msgdsize(pps))!=0);
		if (ret) {
			ms_message("PPS changed ! %i,%i",msgdsize(pps),msgdsize(d->pps));
			update_pps(d,pps);
		}
	}else {
		ms_message("Receiving first PPS");
		update_pps(d,pps);
	}
	return ret;
}


static void enlarge_bitstream(DecData *d, int new_size){
	d->bitstream_size=new_size;
	d->bitstream=ms_realloc(d->bitstream,d->bitstream_size);
}

static int nalusToFrame(DecData *d, MSQueue *naluq, bool_t *new_sps_pps){
	mblk_t *im;
	uint8_t *dst=d->bitstream,*src,*end;
	int nal_len;
	bool_t start_picture=TRUE;
	uint8_t nalu_type;
	*new_sps_pps=FALSE;
	end=d->bitstream+d->bitstream_size;
	while((im=ms_queue_get(naluq))!=NULL){
		src=im->b_rptr;
		nal_len=im->b_wptr-src;

		if (dst+nal_len+100>end){
			int pos=dst-d->bitstream;
			enlarge_bitstream(d, d->bitstream_size+nal_len+100);
			dst=d->bitstream+pos;
			end=d->bitstream+d->bitstream_size;
		}

		if (src[0]==0 && src[1]==0 && src[2]==0 && src[3]==1){
			int size=im->b_wptr-src;
			/*workaround for stupid RTP H264 sender that includes nal markers */
			memcpy(dst,src,size);
			dst+=size;
		}else{
			nalu_type=(*src) & ((1<<5)-1);
			if (nalu_type==7)
				*new_sps_pps=check_sps_change(d,im) || *new_sps_pps;
			if (nalu_type==8)
				*new_sps_pps=check_pps_change(d,im) || *new_sps_pps;
			if (start_picture || nalu_type==7/*SPS*/ || nalu_type==8/*PPS*/ ){
				*dst++=0;
				start_picture=FALSE;
			}		
			/*prepend nal marker*/
			*dst++=0;
			*dst++=0;
			*dst++=1;
			*dst++=*src++;
			while(src<(im->b_wptr-3)){
				if (src[0]==0 && src[1]==0 && src[2]<3){
					*dst++=0;
					*dst++=0;
					*dst++=3;
					src+=2;
				}
				*dst++=*src++;
			}
			*dst++=*src++;
			*dst++=*src++;
			*dst++=*src++;
		}
		freemsg(im);
	}
	return dst-d->bitstream;
}

//jimmy +++ h264 decoder and display
static void dec_process(MSFilter *f){
	DecData *d=(DecData*)f->data;
	mblk_t *im;
	MSQueue nalus;
	MSPicture orig[2], *pict_ptr;

	ms_queue_init(&nalus);

	while((im=ms_queue_get(f->inputs[0]))!=NULL){
		/*push the sps/pps given in sprop-parameter-sets if any*/
		if (d->packet_num==0 && d->sps && d->pps){
			mblk_set_timestamp_info(d->sps,mblk_get_timestamp_info(im));
			mblk_set_timestamp_info(d->pps,mblk_get_timestamp_info(im));
			rfc3984_unpack(&d->unpacker,d->sps,&nalus);
			rfc3984_unpack(&d->unpacker,d->pps,&nalus);
			d->sps=NULL;
			d->pps=NULL;
		}
		rfc3984_unpack(&d->unpacker,im,&nalus);
		if (!ms_queue_empty(&nalus)){
			int size;
			uint8_t *p,*end;
			bool_t need_reinit=FALSE;

			size=nalusToFrame(d,&nalus,&need_reinit);
			p=d->bitstream;
			if (need_reinit)
				dec_reinit(d);

		#define DEF_TOGGLE
		#ifndef DEF_TOGGLE
			orig[0].planes[0] = (unsigned char *) d->outputbuf.addr;
    			orig[0].planes[1] = (unsigned char *)(d->outputbuf.addr+ (MAX_WIDTH*MAX_HEIGHT) );
			orig[0].planes[2] = (unsigned char *)(d->outputbuf.addr+ (MAX_WIDTH*MAX_HEIGHT) + (MAX_WIDTH*MAX_HEIGHT/4));
			orig[0].planes[3] = 0;
			pict_ptr = &orig[0];
		#else
			orig[0].planes[0] = (unsigned char *) d->outputbuf.addr;
    			orig[0].planes[1] = (unsigned char *)(d->outputbuf.addr+ (MAX_WIDTH*MAX_HEIGHT) );
			orig[0].planes[2] = (unsigned char *)(d->outputbuf.addr+ (MAX_WIDTH*MAX_HEIGHT) + (MAX_WIDTH*MAX_HEIGHT/4));
			orig[0].planes[3] = 0;
			orig[1].planes[0] = (unsigned char *) d->outputbuf.addr+ (d->outputbuf.size/2);
    			orig[1].planes[1] = (unsigned char *)(d->outputbuf.addr+ (d->outputbuf.size/2)+ (MAX_WIDTH*MAX_HEIGHT) );
			orig[1].planes[2] = (unsigned char *)(d->outputbuf.addr+ (d->outputbuf.size/2)+ (MAX_WIDTH*MAX_HEIGHT) + (MAX_WIDTH*MAX_HEIGHT/4));
			orig[1].planes[3] = 0;
			if ( d->toggle_flag )	pict_ptr = &orig[1];
			else			pict_ptr = &orig[0];

		#endif
			p=d->bitstream;
			end=d->bitstream+size;

			while (end-p>0) {
				int got_picture=0;
				int pkt_size = end-p;

				if ( pkt_size > (MAX_WIDTH*MAX_HEIGHT*2) ) {
					p+=pkt_size;
					break;
		                } 
				got_picture=favc_decode(d, (unsigned char *)p, (void *)pict_ptr,  pkt_size);
				if ( got_picture < 0 ) {
					nvt_h264_fini(d);
					ms_warning("ms_AVdecoder_process: error %i.",pkt_size);
					if ((f->ticker->time - d->last_error_reported_time)>5000 || d->last_error_reported_time==0) {
						d->last_error_reported_time=f->ticker->time;
						ms_filter_notify_no_arg(f,MS_VIDEO_DECODER_DECODING_ERRORS);
					}
					p+=pkt_size;
					break;

				} else {
					if ( pict_ptr->planes[0] != NULL ) {//__u32 g_u32VpostWidth, g_u32VpostHeight
					 //   printf("go RenderDisplay!!\n");
					 //	printf("pict_ptr->w=%d,pict_ptr->h=%d\n",pict_ptr->w,pict_ptr->h);
					 if(ins_call_flag==0)
					 	{
						RenderDisplay((void *)pict_ptr->planes, d->toggle_flag, pict_ptr->w, pict_ptr->h,  pict_ptr->w, pict_ptr->h);
					 	}
						d->toggle_flag = (d->toggle_flag+1)%2;
					}
					if (!d->first_image_decoded) {
						ms_filter_notify_no_arg(f,MS_VIDEO_DECODER_FIRST_IMAGE_DECODED);
						d->first_image_decoded = TRUE;
					}
				}
				p+=pkt_size;
			}
		}
		d->packet_num++;
	}

	//FiniDisplay();
}

static int dec_add_fmtp(MSFilter *f, void *arg){
	DecData *d=(DecData*)f->data;
	const char *fmtp=(const char *)arg;
	char value[256];
	if (fmtp_get_value(fmtp,"sprop-parameter-sets",value,sizeof(value))){
		char * b64_sps=value;
		char * b64_pps=strchr(value,',');
		if (b64_pps){
			*b64_pps='\0';
			++b64_pps;
			ms_message("Got sprop-parameter-sets : sps=%s , pps=%s",b64_sps,b64_pps);
			d->sps=allocb(sizeof(value),0);
			d->sps->b_wptr+=b64_decode(b64_sps,strlen(b64_sps),d->sps->b_wptr,sizeof(value));
			d->pps=allocb(sizeof(value),0);
			d->pps->b_wptr+=b64_decode(b64_pps,strlen(b64_pps),d->pps->b_wptr,sizeof(value));
		}
	}
	return 0;
}

static int reset_first_image(MSFilter* f, void *data) {
	DecData *d=(DecData*)f->data;
	d->first_image_decoded = FALSE;
	return 0;
}

static MSFilterMethod  h264_dec_methods[]={
	{	MS_FILTER_ADD_FMTP				, dec_add_fmtp	},
	{   MS_VIDEO_DECODER_RESET_FIRST_IMAGE_NOTIFICATION	, reset_first_image },
	{	0						, NULL	}
};

#ifndef _MSC_VER

MSFilterDesc ms_h264_dec_desc={
	.id=MS_H264_DEC_ID,
	.name="MSH264Dec",
	.text="A H264 decoder based on N32926UxDN FAVC Codec IPCore",
	.category=MS_FILTER_DECODER,
	.enc_fmt="H264",
	.ninputs=1,
	.noutputs=1,
	.init=dec_init,
	.preprocess=dec_preprocess,
	.process=dec_process,
	.uninit=dec_uninit,
	.methods=h264_dec_methods
};

#else

MSFilterDesc ms_h264_dec_desc={
	MS_H264_DEC_ID,
	"MSH264Dec",
	"A H264 decoder based on N32926UxDN FAVC Codec IPCore",
	MS_FILTER_DECODER,
	"H264",
	1,
	1,
	dec_init,
	dec_preprocess,
	dec_process,
	NULL,
	dec_uninit,
	h264_dec_methods
};

#endif

MS_FILTER_DESC_EXPORT(ms_h264_dec_desc)

