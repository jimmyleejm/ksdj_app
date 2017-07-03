/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2006  Simon MORLAT (simon.morlat@linphone.org)

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
#include "mediastreamer2/msvideo.h"
#include "mediastreamer2/msvideoout.h"


struct _MSDisplay;

typedef enum _MSDisplayEventType{
	MS_DISPLAY_RESIZE_EVENT
}MSDisplayEventType;

typedef struct _MSDisplayEvent{
	MSDisplayEventType evtype;
	int w,h;
}MSDisplayEvent;

typedef struct _MSDisplayDesc{
	/*init requests setup of the display window at the proper size, given
	in frame_buffer argument. Memory buffer (data,strides) must be fulfilled
	at return. init() might be called several times upon screen resize*/
	bool_t (*init)(struct _MSDisplay *, struct _MSFilter *f, MSPicture *frame_buffer, MSPicture *selfview_buffer);
	void (*lock)(struct _MSDisplay *);/*lock before writing to the framebuffer*/
	void (*unlock)(struct _MSDisplay *);/*unlock after writing to the framebuffer*/
	void (*update)(struct _MSDisplay *, int new_image, int new_selfview); /*display the picture to the screen*/
	void (*uninit)(struct _MSDisplay *);
	int (*pollevent)(struct _MSDisplay *, MSDisplayEvent *ev);
	long default_window_id;
}MSDisplayDesc;

typedef struct _MSDisplay{
	MSDisplayDesc *desc;
	long window_id; /*window id if the display should use an existing window*/
	void *data;
	bool_t use_external_window;
} MSDisplay;

#define ms_display_init(d,f,fbuf,fbuf_selfview)	(d)->desc->init(d,f,fbuf,fbuf_selfview)
#define ms_display_lock(d)	if ((d)->desc->lock) (d)->desc->lock(d)
#define ms_display_unlock(d)	if ((d)->desc->unlock) (d)->desc->unlock(d)
#define ms_display_update(d, A, B)	if ((d)->desc->update) (d)->desc->update(d, A, B)

int ms_display_poll_event(MSDisplay *d, MSDisplayEvent *ev);

extern MSDisplayDesc ms_nvt_display_desc;

#ifdef __cplusplus
extern "C"{
#endif

/*plugins can set their own display using this method:*/
void ms_display_desc_set_default(MSDisplayDesc *desc);

MSDisplayDesc * ms_display_desc_get_default(void);
void ms_display_desc_set_default_window_id(MSDisplayDesc *desc, long id);

MSDisplay *ms_display_new(MSDisplayDesc *desc);
void ms_display_set_window_id(MSDisplay *d, long window_id);
void ms_display_destroy(MSDisplay *d);

#define MS_VIDEO_OUT_SET_DISPLAY 		MS_FILTER_METHOD(MS_VIDEO_OUT_ID,0,MSDisplay*)
#define MS_VIDEO_OUT_HANDLE_RESIZING 		MS_FILTER_METHOD_NO_ARG(MS_VIDEO_OUT_ID,1)
#define MS_VIDEO_OUT_SET_CORNER 		MS_FILTER_METHOD(MS_VIDEO_OUT_ID,2,int)
#define MS_VIDEO_OUT_AUTO_FIT			MS_FILTER_METHOD(MS_VIDEO_OUT_ID,3,int)
#define MS_VIDEO_OUT_ENABLE_MIRRORING		MS_FILTER_METHOD(MS_VIDEO_OUT_ID,4,int)
#define MS_VIDEO_OUT_GET_NATIVE_WINDOW_ID 	MS_FILTER_METHOD(MS_VIDEO_OUT_ID,5,unsigned long)
#define MS_VIDEO_OUT_GET_CORNER 		MS_FILTER_METHOD(MS_VIDEO_OUT_ID,6,int)
#define MS_VIDEO_OUT_SET_SCALE_FACTOR 		MS_FILTER_METHOD(MS_VIDEO_OUT_ID,7,float)
#define MS_VIDEO_OUT_GET_SCALE_FACTOR 		MS_FILTER_METHOD(MS_VIDEO_OUT_ID,8,float)
#define MS_VIDEO_OUT_SET_SELFVIEW_POS 		MS_FILTER_METHOD(MS_VIDEO_OUT_ID,9,float[3])
#define MS_VIDEO_OUT_GET_SELFVIEW_POS 		MS_FILTER_METHOD(MS_VIDEO_OUT_ID,10,float[3])
#define MS_VIDEO_OUT_SET_BACKGROUND_COLOR 	MS_FILTER_METHOD(MS_VIDEO_OUT_ID,11,int[3])
#define MS_VIDEO_OUT_GET_BACKGROUND_COLOR 	MS_FILTER_METHOD(MS_VIDEO_OUT_ID,12,int[3])

#ifdef __cplusplus
}
#endif

#define SCALE_FACTOR 4.0f
#define SELVIEW_POS_INACTIVE -100.0

static int video_out_set_vsize(MSFilter *f,void *arg);

int ms_display_poll_event(MSDisplay *d, MSDisplayEvent *ev){
	if (d->desc->pollevent)
		return d->desc->pollevent(d,ev);
	else return -1;
}


typedef struct _NvtDisplay{
	MSFilter *filter;
	bool_t nvt_initialized;
	ms_mutex_t nvt_mutex;
	float sv_scalefactor;
	uint8_t *laybuf;	//buffer
	MSVideoSize src_size;
	MSVideoSize screen_size;
} NvtDisplay;


static void nvt_show_window(bool_t show){

}

static long nvt_get_native_window_id(){
	return 0;
}


static void nvt_display_uninit(MSDisplay *obj);

static int nvt_create_window(NvtDisplay *wd, int w, int h){

	wd->screen_size.width = w;
	wd->screen_size.height = h;
	wd->src_size.width = w;
	wd->src_size.height = h;

	return 0;
}

static void yuv_buffer_init(MSPicture *buf, int w, int h, uint8_t *ptr){
	int ysize,usize;
	ysize=w*h;
	usize=ysize/4;
	buf->w=w;
	buf->h=h;
	buf->planes[0]=ptr;
	buf->planes[1]=buf->planes[0]+ysize;
	buf->planes[2]=buf->planes[1]+usize;
	buf->planes[3]=0;
	buf->strides[0]=w;
	buf->strides[1]=w/2;
	buf->strides[2]=buf->strides[1];
	buf->strides[3]=0;
}

static void enlarge_fbbuffer(MSDisplay *obj, MSPicture *fbuf){
	NvtDisplay *wd = (NvtDisplay*)obj->data;
	int size = 0;
	size = (fbuf->w * fbuf->h * 3)/2;
	wd->laybuf=ms_realloc(wd->laybuf,size);
}

static bool_t nvt_display_init(MSDisplay *obj, MSFilter *f, MSPicture *fbuf, MSPicture *fbuf_selfview){
	NvtDisplay *wd = (NvtDisplay*)obj->data;
	int i;
	int size = 0;
	if (wd==NULL){
		char driver[128];

		/* Initialize the NVT library */
		ms_message("Initialize NVT video");
		wd=(NvtDisplay*)ms_new0(NvtDisplay,1);
		wd->filter = f;
		size = (fbuf->w * fbuf->h * 3)/2;
		ms_message("set video output size = 0x%x.\n", size);
		wd->laybuf=ms_malloc0(size);
		ms_message("set video buffer address = 0x%x.\n", wd->laybuf);
		obj->data=wd;
		wd->nvt_initialized=TRUE;
		ms_mutex_init(&wd->nvt_mutex,NULL);
		ms_mutex_lock(&wd->nvt_mutex);

	}else {
		ms_message("Cleaning WD");
		ms_mutex_lock(&wd->nvt_mutex);
	}

	wd->filter = f;

	i=nvt_create_window(wd, fbuf->w, fbuf->h);
	if (i==0){
		if ( wd->laybuf != NULL) {
			enlarge_fbbuffer(obj, fbuf);	//Add by EasyWave;
			yuv_buffer_init(fbuf, fbuf->w, fbuf->h, wd->laybuf);
			fbuf->w=wd->src_size.width;
			fbuf->h=wd->src_size.height;
			nvt_show_window(TRUE);
			obj->window_id=nvt_get_native_window_id();
			ms_mutex_unlock(&wd->nvt_mutex);
			return TRUE;
		} else {
			ms_mutex_unlock(&wd->nvt_mutex);
			return FALSE;
		}
	}
	ms_mutex_unlock(&wd->nvt_mutex);
	return FALSE;
}

static void nvt_display_lock(MSDisplay *obj){
	NvtDisplay *wd = (NvtDisplay*)obj->data;
	ms_mutex_lock(&wd->nvt_mutex);
}

static void nvt_display_unlock(MSDisplay *obj){
	NvtDisplay *wd = (NvtDisplay*)obj->data;
	ms_mutex_unlock(&wd->nvt_mutex);
}

static void nvt_display_update(MSDisplay *obj, int new_image, int new_selfview){
	NvtDisplay *wd = (NvtDisplay*)obj->data;
}

static int nvt_poll_event(MSDisplay *obj, MSDisplayEvent *ev){
	NvtDisplay *wd = (NvtDisplay*)obj->data;

	return -1;
}

static void nvt_display_uninit(MSDisplay *obj){
	NvtDisplay *wd = (NvtDisplay*)obj->data;

	if (wd!=NULL) {
		ms_free(wd->laybuf);
		ms_free(wd);
		wd=NULL;
	}

	ms_message("WD Fred");
	nvt_show_window(FALSE);
}

MSDisplayDesc ms_nvt_display_desc={
	.init=nvt_display_init,
	.lock=nvt_display_lock,
	.unlock=nvt_display_unlock,
	.update=nvt_display_update,
	.uninit=nvt_display_uninit,
	.pollevent=nvt_poll_event,
};


MSDisplay *ms_display_new(MSDisplayDesc *desc){
	MSDisplay *obj=(MSDisplay *)ms_new0(MSDisplay,1);
	obj->desc=desc;
	obj->data=NULL;
	return obj;
}

void ms_display_set_window_id(MSDisplay *d, long id){
	d->window_id=id;
	d->use_external_window=TRUE;
}

void ms_display_destroy(MSDisplay *obj){
	obj->desc->uninit(obj);
	ms_free(obj);
}

static MSDisplayDesc *default_display_desc=&ms_nvt_display_desc;

void ms_display_desc_set_default(MSDisplayDesc *desc){
	default_display_desc=desc;
}

MSDisplayDesc * ms_display_desc_get_default(void){
	return default_display_desc;
}

void ms_display_desc_set_default_window_id(MSDisplayDesc *desc, long id){
	desc->default_window_id=id;
}

typedef struct VideoOut
{
	struct Rational {int num; int den;} ratio;
	MSPicture fbuf;
	MSPicture fbuf_selfview;
	MSPicture local_pic;
	MSRect local_rect;
	mblk_t *local_msg;
	MSVideoSize prevsize;
	int corner; /*for selfview*/
	float scale_factor; /*for selfview*/
	float sv_posx,sv_posy;
	int background_color[3];

	MSDisplay *display;
	bool_t own_display;
	bool_t ready;
	bool_t autofit;
	bool_t mirror;

} VideoOut;

static void set_corner(VideoOut *s, int corner)
{
	s->corner=corner;
	s->local_pic.w=((int)(s->fbuf.w/s->scale_factor)) & ~0x1;
	s->local_pic.h=((int)(s->fbuf.h/s->scale_factor)) & ~0x1;
	s->local_rect.w=s->local_pic.w;
	s->local_rect.h=s->local_pic.h;
	if (corner==1) {
		/* top left corner */
		s->local_rect.x=0;
		s->local_rect.y=0;
	} else if (corner==2) {
		/* top right corner */
		s->local_rect.x=s->fbuf.w-s->local_pic.w;
		s->local_rect.y=0;
	} else if (corner==3) {
		/* bottom left corner */
		s->local_rect.x=0;
		s->local_rect.y=s->fbuf.h-s->local_pic.h;
	} else {
		/* default: bottom right corner */
		/* corner can be set to -1: to disable the self view... */
		s->local_rect.x=s->fbuf.w-s->local_pic.w;
		s->local_rect.y=s->fbuf.h-s->local_pic.h;
	}
	s->fbuf_selfview.w=(s->fbuf.w/1) & ~0x1;
	s->fbuf_selfview.h=(s->fbuf.h/1) & ~0x1;
}

static void set_vsize(VideoOut *s, MSVideoSize *sz){

	s->fbuf.w=sz->width & ~0x1;
	s->fbuf.h=sz->height & ~0x1;
	set_corner(s,s->corner);
	ms_message("Video size set to %ix%i",s->fbuf.w,s->fbuf.h);
}

static void video_out_init(MSFilter  *f){
	printf("1111111111111111111111111111\n");
	ms_message("video_out_init");
	VideoOut *obj=(VideoOut*)ms_new0(VideoOut,1);
	MSVideoSize def_size;
	obj->ratio.num=11;
	obj->ratio.den=9;
	def_size.width=MS_VIDEO_SIZE_CIF_W;
	def_size.height=MS_VIDEO_SIZE_CIF_H;
	obj->prevsize.width=0;
	obj->prevsize.height=0;
	obj->local_msg=NULL;
	obj->corner=0;
	obj->scale_factor=SCALE_FACTOR;
	obj->sv_posx=obj->sv_posy=SELVIEW_POS_INACTIVE;
	obj->background_color[0]=obj->background_color[1]=obj->background_color[2]=0;
	obj->display=NULL;
	obj->own_display=FALSE;
	obj->ready=FALSE;
	obj->autofit=FALSE;
	obj->mirror=FALSE;
	set_vsize(obj,&def_size);
	f->data=obj;
}


static void video_out_uninit(MSFilter *f){
	VideoOut *obj=(VideoOut*)f->data;
	if (obj->display!=NULL && obj->own_display)
		ms_display_destroy(obj->display);

	if (obj->local_msg!=NULL) {
		freemsg(obj->local_msg);
		obj->local_msg=NULL;
	}
	ms_free(obj);
}

static void video_out_prepare(MSFilter *f){
	VideoOut *obj=(VideoOut*)f->data;
	if (obj->display==NULL){
		if (default_display_desc==NULL){
			ms_error("No default display built in !");
			return;
		}
		obj->display=ms_display_new(default_display_desc);
		obj->own_display=TRUE;
	}
	if (!ms_display_init(obj->display,f,&obj->fbuf,&obj->fbuf_selfview)){
		if (obj->own_display) ms_display_destroy(obj->display);
		obj->display=NULL;
	}

	if (obj->local_msg!=NULL) {
		freemsg(obj->local_msg);
		obj->local_msg=NULL;
	}
	set_corner(obj,obj->corner);
	obj->ready=TRUE;
}

static int video_out_handle_resizing(MSFilter *f, void *data){
	/* to be removed */
	return -1;
}

static int _video_out_handle_resizing(MSFilter *f, void *data){
	VideoOut *s=(VideoOut*)f->data;
	MSDisplay *disp=s->display;
	int ret = -1;
	if (disp!=NULL){
		MSDisplayEvent ev;
		ret = ms_display_poll_event(disp,&ev);
		if (ret>0){
			if (ev.evtype==MS_DISPLAY_RESIZE_EVENT){
				MSVideoSize sz;
				sz.width=ev.w;
				sz.height=ev.h;
				ms_filter_lock(f);
				if (s->ready){
					set_vsize(s,&sz);
					s->ready=FALSE;
				}
				ms_filter_unlock(f);
			}
			return 1;
		}
	}
	return ret;
}

static void poll_for_resizing(MSFilter *f) {
	int i;
	for(i=0;i<100;++i){
		int ret = _video_out_handle_resizing(f, NULL);
		if (ret<0) break;
	}
}

static void video_out_postprocess(MSFilter *f){
}

static void video_out_preprocess(MSFilter *f){
	printf("22222222222222222222222222222222\n");
	video_out_prepare(f);
}


static void video_out_process(MSFilter *f){
	printf("3333333333333333333333333333333333\n");
	
	VideoOut *obj=(VideoOut*)f->data;
	mblk_t *inm;
	int update;
	int update_selfview=0;

	poll_for_resizing(f);
	ms_filter_lock(f);

	if (!obj->ready) video_out_prepare(f);

	if (!obj->ready){
		ms_filter_unlock(f);
		if (f->inputs[0]!=NULL)
			ms_queue_flush(f->inputs[0]);
		if (f->inputs[1]!=NULL)
			ms_queue_flush(f->inputs[1]);
		return;
	}

	//get most recent message and draw it
	// there is not user for inputs[1]
	if (f->inputs[1]!=NULL && (inm=ms_queue_peek_last(f->inputs[1]))!=0) {
	#if defined(VPE_MMU_ENABLE)
		MSVideoSize roio;
		if (obj->corner==-1){
			if (obj->local_msg!=NULL) {
				freemsg(obj->local_msg);
				obj->local_msg=NULL;
			}
		}else if (obj->fbuf_selfview.planes[0]!=NULL) {
			MSPicture src;
			if (ms_yuv_buf_init_from_mblk(&src,inm)==0){

				roio.width=src.w;
				roio.height=src.h;
				ms_display_lock(obj->display);
				ms_yuv_buf_copy(src.planes,src.strides,obj->fbuf_selfview.planes,obj->fbuf_selfview.strides,roio);
				if (!mblk_get_precious_flag(inm)) ms_yuv_buf_mirror(&obj->fbuf_selfview);
				ms_display_unlock(obj->display);
				update_selfview=1;
			}
		}else{
			MSPicture src;
			
			if (ms_yuv_buf_init_from_mblk(&src,inm)==0){
				if (obj->local_msg==NULL){
					obj->local_msg=ms_yuv_buf_alloc(&obj->local_pic,
						obj->local_pic.w,obj->local_pic.h);
				}
				if (obj->local_pic.planes[0]!=NULL)
				{
					roio.width=src.w;
					roio.height=src.h;
								
					ms_yuv_buf_copy(src.planes,src.strides,obj->local_pic.planes,obj->local_pic.strides,roio);
					if (!mblk_get_precious_flag(inm)) ms_yuv_buf_mirror(&obj->local_pic);
					update=1;
				}
			}
		}
	#endif
		ms_queue_flush(f->inputs[1]);
	}

	if (f->inputs[0]!=NULL && (inm=ms_queue_peek_last(f->inputs[0]))!=0) {
		MSPicture src;
		MSVideoSize roi;

		if (ms_yuv_buf_init_from_mblk(&src,inm)==0){
		
			MSVideoSize cur,newsize;
			cur.width=obj->fbuf.w;
			cur.height=obj->fbuf.h;
			newsize.width=src.w;
			newsize.height=src.h;
			roi.width=src.w;
			roi.height=src.h;
		#if defined(VPE_MMU_ENABLE)
			if (obj->autofit && !ms_video_size_equal(newsize,obj->prevsize) ) {
				MSVideoSize new_window_size;
				static const MSVideoSize min_size=MS_VIDEO_SIZE_QVGA;
				obj->prevsize=newsize;
				ms_message("received size is %ix%i",newsize.width,newsize.height);
				//don't resize less than QVGA, it is too small
				if (min_size.width*min_size.height>newsize.width*newsize.height){
					new_window_size.width=newsize.width*2;
					new_window_size.height=newsize.height*2;
				}else new_window_size=newsize;

				if (!ms_video_size_equal(new_window_size,cur)){
					set_vsize(obj,&new_window_size);
					ms_message("autofit: new size is %ix%i",newsize.width,newsize.height);					
					video_out_prepare(f);
				}
			}
		
			ms_display_lock(obj->display);
			ms_yuv_buf_copy(src.planes,src.strides,obj->fbuf.planes,obj->fbuf.strides,roi);
			if (obj->mirror && !mblk_get_precious_flag(inm)) ms_yuv_buf_mirror(&obj->fbuf);
			ms_display_unlock(obj->display);
		#endif
		}
		update=1;
		ms_queue_flush(f->inputs[0]);
	}

	
#if defined(VPE_MMU_ENABLE)
	//copy resized local view into main buffer, at bottom left corner:
	if (obj->local_msg!=NULL){
		MSPicture corner=obj->fbuf;
		MSVideoSize roi;
		roi.width=obj->local_pic.w;
		roi.height=obj->local_pic.h;
		corner.w=obj->local_pic.w;
		corner.h=obj->local_pic.h;
		corner.planes[0]+=obj->local_rect.x+(obj->local_rect.y*corner.strides[0]);
		corner.planes[1]+=(obj->local_rect.x/2)+((obj->local_rect.y/2)*corner.strides[1]);
		corner.planes[2]+=(obj->local_rect.x/2)+((obj->local_rect.y/2)*corner.strides[2]);
		corner.planes[3]=0;	
		ms_display_lock(obj->display);
		ms_yuv_buf_copy(obj->local_pic.planes,obj->local_pic.strides,
				corner.planes,corner.strides,roi);
		ms_display_unlock(obj->display);
	}
#endif
	if( update == 1) {
		ms_display_update(obj->display, update, update_selfview);
		update=0;
	}

	ms_filter_unlock(f);
	
}

static int video_out_set_vsize(MSFilter *f,void *arg){
	VideoOut *s=(VideoOut*)f->data;
	ms_filter_lock(f);
	set_vsize(s,(MSVideoSize*)arg);
	ms_filter_unlock(f);
	return 0;
}

static int video_out_set_display(MSFilter *f,void *arg){
	VideoOut *s=(VideoOut*)f->data;
	s->display=(MSDisplay*)arg;
	return 0;
}

static int video_out_auto_fit(MSFilter *f, void *arg){
	VideoOut *s=(VideoOut*)f->data;
	s->autofit=*(int*)arg;
	return 0;
}

static int video_out_set_corner(MSFilter *f,void *arg){
	VideoOut *s=(VideoOut*)f->data;
	s->sv_posx=s->sv_posy=SELVIEW_POS_INACTIVE;
	ms_filter_lock(f);
	set_corner(s, *(int*)arg);
	if (s->display){
		ms_display_lock(s->display);
		{
		int w=s->fbuf.w;
		int h=s->fbuf.h;
		int ysize=w*h;
		int usize=ysize/4;
	#if defined(VPE_MMU_ENABLE)
		memset(s->fbuf.planes[0], 0, ysize);
		memset(s->fbuf.planes[1], 0, usize);
		memset(s->fbuf.planes[2], 0, usize);
		s->fbuf.planes[3]=NULL;
	#endif
		}
		ms_display_unlock(s->display);
	}
	ms_filter_unlock(f);
	return 0;
}

static int video_out_get_corner(MSFilter *f,void *arg){
	VideoOut *s=(VideoOut*)f->data;
	*((int*)arg)=s->corner;
	return 0;
}

static int video_out_set_scalefactor(MSFilter *f,void *arg){
	VideoOut *s=(VideoOut*)f->data;
	s->scale_factor = *(float*)arg;
	if (s->scale_factor<0.5f)
		s->scale_factor = 0.5f;
	ms_filter_lock(f);
	set_corner(s, s->corner);
	if (s->display){
		ms_display_lock(s->display);
		{
		int w=s->fbuf.w;
		int h=s->fbuf.h;
		int ysize=w*h;
		int usize=ysize/4;
	#if defined(VPE_MMU_ENABLE)
		memset(s->fbuf.planes[0], 0, ysize);
		memset(s->fbuf.planes[1], 0, usize);
		memset(s->fbuf.planes[2], 0, usize);
		s->fbuf.planes[3]=NULL;
	#endif
		}
		ms_display_unlock(s->display);
	}
	ms_filter_unlock(f);
	return 0;
}

static int video_out_get_scalefactor(MSFilter *f,void *arg){
	VideoOut *s=(VideoOut*)f->data;
	*((float*)arg)=(float)s->scale_factor;
	return 0;
}


static int video_out_enable_mirroring(MSFilter *f,void *arg){
	VideoOut *s=(VideoOut*)f->data;
	s->mirror=*(int*)arg;
	return 0;
}

static int video_out_get_native_window_id(MSFilter *f, void*arg){
	VideoOut *s=(VideoOut*)f->data;
	unsigned long *id=(unsigned long*)arg;
	*id=0;
	if (s->display){
		*id=s->display->window_id;
		return 0;
	}
	return -1;
}

static int video_out_set_selfview_pos(MSFilter *f,void *arg){
	VideoOut *s=(VideoOut*)f->data;
	s->sv_posx=((float*)arg)[0];
	s->sv_posy=((float*)arg)[1];
	s->scale_factor=(float)100.0/((float*)arg)[2];
	return 0;
}

static int video_out_get_selfview_pos(MSFilter *f,void *arg){
	VideoOut *s=(VideoOut*)f->data;
	((float*)arg)[0]=s->sv_posx;
	((float*)arg)[1]=s->sv_posy;
	((float*)arg)[2]=(float)100.0/s->scale_factor;
	return 0;
}

static int video_out_set_background_color(MSFilter *f,void *arg){
	VideoOut *s=(VideoOut*)f->data;
	s->background_color[0]=((int*)arg)[0];
	s->background_color[1]=((int*)arg)[1];
	s->background_color[2]=((int*)arg)[2];
	return 0;
}

static int video_out_get_background_color(MSFilter *f,void *arg){
	VideoOut *s=(VideoOut*)f->data;
	((int*)arg)[0]=s->background_color[0];
	((int*)arg)[1]=s->background_color[1];
	((int*)arg)[2]=s->background_color[2];
	return 0;
}

static MSFilterMethod methods[]={
	{	MS_FILTER_SET_VIDEO_SIZE		,  video_out_set_vsize },
	{	MS_VIDEO_OUT_SET_DISPLAY		,  video_out_set_display},
	{	MS_VIDEO_OUT_SET_CORNER 		,  video_out_set_corner},
	{	MS_VIDEO_OUT_AUTO_FIT			,  video_out_auto_fit},
	{	MS_VIDEO_OUT_HANDLE_RESIZING		,  video_out_handle_resizing},
	{	MS_VIDEO_OUT_ENABLE_MIRRORING		,  video_out_enable_mirroring},
	{	MS_VIDEO_OUT_GET_NATIVE_WINDOW_ID	,  video_out_get_native_window_id},
	{	MS_VIDEO_OUT_GET_CORNER 		,  video_out_get_corner},
	{	MS_VIDEO_OUT_SET_SCALE_FACTOR 		,  video_out_set_scalefactor},
	{	MS_VIDEO_OUT_GET_SCALE_FACTOR 		,  video_out_get_scalefactor},
	{	MS_VIDEO_OUT_SET_SELFVIEW_POS 	 	,  video_out_set_selfview_pos},
	{	MS_VIDEO_OUT_GET_SELFVIEW_POS    	,  video_out_get_selfview_pos},
	{	MS_VIDEO_OUT_SET_BACKGROUND_COLOR    	,  video_out_set_background_color},
	{	MS_VIDEO_OUT_GET_BACKGROUND_COLOR    	,  video_out_get_background_color},
	/* methods for compatibility with the MSVideoDisplay interface*/
	{	MS_VIDEO_DISPLAY_SET_LOCAL_VIEW_MODE 	, video_out_set_corner },
	{	MS_VIDEO_DISPLAY_ENABLE_AUTOFIT			, video_out_auto_fit },
	{	MS_VIDEO_DISPLAY_ENABLE_MIRRORING		, video_out_enable_mirroring },
	{	MS_VIDEO_DISPLAY_GET_NATIVE_WINDOW_ID		, video_out_get_native_window_id },
	{	MS_VIDEO_DISPLAY_SET_LOCAL_VIEW_SCALEFACTOR	, video_out_set_scalefactor },
	{	MS_VIDEO_DISPLAY_SET_BACKGROUND_COLOR   	, video_out_set_background_color},
	{	0	,NULL}
};

MSFilterDesc ms_video_out_desc={
	.id=MS_VIDEO_OUT_ID,
	.name="MSVideoOut",
	.text=N_("A Nuvoton-based video display"),
	.category=MS_FILTER_OTHER,
	.ninputs=2,
	.noutputs=0,
	.init=video_out_init,
	.preprocess=video_out_preprocess,
	.process=video_out_process,
	.postprocess=video_out_postprocess,
	.uninit=video_out_uninit,
	.methods=methods
};


MS_FILTER_DESC_EXPORT(ms_video_out_desc)
