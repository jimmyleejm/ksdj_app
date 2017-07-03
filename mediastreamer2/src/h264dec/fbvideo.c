#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <signal.h>
#include <linux/fb.h>
#include <unistd.h>

#include "vpe.h"
#include "port.h"
#include "h264.h"


#include <sys/stat.h>

#define  address_size 640*480*3/2

#define DBGPRINTF(...)


//extern void linphonec_out(const char *fmt,...);

//pthread_mutex_t Fb_mutex ;

/* VPE device */
char vpe_device[] = "/dev/vpe";
int vpe_fd=-1;
int refresh_flag=0;
/* VPOST related */
static struct fb_var_screeninfo g_fb_var;
static char *fbdevice = "/dev/fb0";
__u32 g_u32VpostWidth, g_u32VpostHeight, fb_bpp;
unsigned int FB_PAddress;
void* FB_VAddress;
unsigned int g_u32VpostBufMapSize = 0;	/* For munmap use */
//jimmy +++ if used _USE_FBIOPAN_ that i can't refresh framebuffer after video ?????
//#define _USE_FBIOPAN_

int fb_fd=-1;

#define DISPLAY_MODE_CBYCRY		4
#define DISPLAY_MODE_YCBYCR		5
#define DISPLAY_MODE_CRYCBY		6
#define VIDEO_FORMAT_CHANGE		_IOW('v', 50, unsigned int)	//frame buffer format change

#define IOCTL_LCD_GET_DMA_BASE      	_IOR('v', 32, unsigned int *)
#define IOCTL_LCD_ENABLE_INT		_IO('v', 28)
#define IOCTL_LCD_DISABLE_INT		_IO('v', 29)

// VPE
int g_i32LastWidth;
int g_i32LastHeight;

static int WaitVPEngine()
{
	unsigned int value;
	int ret=0;
	while ( ioctl (vpe_fd, VPE_WAIT_INTERRUPT, &value) ) {
		if (errno == EINTR) {
			continue;
		} else {
			printf ("%s: VPE_WAIT_INTERRUPT failed: %s\n", __FUNCTION__, strerror (errno));
			ret = -1;
			break;
		}
	}
	return ret;
}

read_address(unsigned long * address)
{
    int   i, j;
    int   words = 10;
    int   devmem;
    off_t PageOffset, PageAddress;
      //flag=0;
    unsigned long *hw_addr = (unsigned long *) address;//(unsigned long *) 0x84d000;
    unsigned long  read_data;


    devmem = open("/dev/mem", O_RDWR | O_SYNC);
    PageOffset = (off_t) hw_addr % getpagesize();
    PageAddress = (off_t) (hw_addr - PageOffset);

    hw_addr = (unsigned long *) mmap(0, address_size+PageOffset, PROT_READ|PROT_WRITE, MAP_SHARED, devmem, PageAddress);

	
    memset(hw_addr+PageOffset,0,480*2*sizeof(char)*10);
 //   memset(hw_addr+PageOffset+640*480,0,480*sizeof(char)*2);
  //  memset(hw_addr+PageOffset+640*480+640*480/4,0,480*sizeof(char)*2);*/
    
/*
    for (i = 0, j = 0; i < words; i++, j+=4)
    {
       // printf("Writing to hardware model\n");

       // *(hw_addr+j) = 0xabcd0000+j;

        read_data = *(hw_addr+PageOffset+j);

        printf("Read from hardware %x\n",read_data);

    }*/
}

int RenderDisplay(void* data, int toggle, int Srcwidth, int Srcheight, int Tarwidth, int Tarheight)
{
	

	//unsigned int volatile vworkbuf, tmpsize;
	AVFrame             *pict=(AVFrame *)data;	
	char* pDstBuf=NULL;
	unsigned int value;
	vpe_transform_t vpe_setting;
	unsigned int ptr_y, ptr_u, ptr_v;
	//int step=0;
	int total_x;
	//int total_y;
	int PAT_WIDTH, PAT_HEIGHT;
	unsigned int ret =0;	

	//printf("Srcwidth=%d Srcheight=%d",Srcwidth,Srcheight); //320 240
    //printf("Tarwidth=%d Tarheight=%d",Tarwidth,Tarheight);

	if ( g_u32VpostWidth < Tarwidth ) {
		Tarheight = Tarheight*g_u32VpostWidth/Tarwidth;
		Tarwidth  = g_u32VpostWidth;
	}
	if ( g_u32VpostHeight < Tarheight ) {
		Tarwidth  = Tarwidth*g_u32VpostHeight/Tarheight;
		Tarheight = g_u32VpostHeight;
	}
	if ( g_i32LastWidth != Tarwidth  || g_i32LastHeight != Tarheight ) {
		//jimmy // for not fresh lcd 
		/*
		if ( FB_VAddress ) {
			printf("Clean FB\n");
			memset ( FB_VAddress, 0x0, g_u32VpostBufMapSize );
		}*/
	}
	g_i32LastWidth  = Tarwidth;
	g_i32LastHeight = Tarheight;

#ifdef _USE_FBIOPAN_


	if ( toggle ) {
		g_fb_var.yoffset = 0;
		pDstBuf = FB_PAddress+g_u32VpostBufMapSize/2;
	} else {
		g_fb_var.yoffset = g_fb_var.yres;
		pDstBuf = FB_PAddress;
	}
	if (ioctl(fb_fd, FBIOPAN_DISPLAY, &g_fb_var) < 0) {
		printf("ioctl FBIOPAN_DISPLAY\n");
	}

#else
	pDstBuf = FB_PAddress;
#endif
	
	PAT_WIDTH = (Srcwidth+3) & ~0x03;
	PAT_HEIGHT = Srcheight;
   // printf("PAT_WIDTH =%d,PAT_HEIGHT=%d\n",PAT_WIDTH,PAT_HEIGHT);
	total_x = (Srcwidth + 15) & ~0x0F;
/*	   
	ptr_y = (unsigned int)pict->data[0]+480*4*2;
	ptr_u = (unsigned int)pict->data[1]+480*2;		
	ptr_v = (unsigned int)pict->data[2]+480*2;*/

	ptr_y = (unsigned int)pict->data[0];
	ptr_u = (unsigned int)pict->data[1];		
	ptr_v = (unsigned int)pict->data[2];

/*
	memset(pict->data[0],0,480*4*sizeof(char));
	memset(pict->data[1],0,480*sizeof(char));
	memset(pict->data[2],0,480*sizeof(char));*/

/*
printf("ptr_y is %x\n",ptr_y);
printf("ptr_u is %x\n",ptr_u);
printf("ptr_u is %x\n",ptr_v);
*/
	//read_address((unsigned long *)ptr_y);


	//WaitVPEngine();

	// Get Video Decoder IP Register size
	if( ioctl(vpe_fd, VPE_INIT, NULL)  < 0) {
		close(vpe_fd);
		printf("VPE_INIT fail\n");
		ret=-1;	goto L_VPE_EXIT;
	}	
	
	value = 0x5a;
	if((ioctl(vpe_fd, VPE_IO_SET, &value)) < 0) {
		close(vpe_fd);
		printf("VPE_IO_SET fail\n");
		ret=-2;	goto L_VPE_EXIT;
	}			
	
	value = 0;		
	//value = 1;	
	ioctl(vpe_fd, VPE_SET_MMU_MODE, &value);	
	
	vpe_setting.src_width = PAT_WIDTH;	
		
	vpe_setting.src_addrPacY = ptr_y;				
	vpe_setting.src_addrU = ptr_u;	
	vpe_setting.src_addrV = ptr_v;				
	vpe_setting.src_format = VPE_SRC_PLANAR_YUV420;
	vpe_setting.src_width = PAT_WIDTH ;
	vpe_setting.src_height = PAT_HEIGHT;		

	vpe_setting.src_leftoffset = 0;
	vpe_setting.src_rightoffset = total_x - PAT_WIDTH;
	vpe_setting.dest_addrPac = (unsigned int)pDstBuf;	
	vpe_setting.dest_format = VPE_DST_PACKET_RGB565;
	vpe_setting.dest_width = Tarwidth;
	vpe_setting.dest_height = Tarheight;
	//jimmy ???
	//printf("g_u32VpostHeight is %d\n",g_u32VpostHeight);
	//printf("g_u32VpostWidth is %d\n",g_u32VpostWidth);
	if (g_u32VpostHeight > Tarheight) {  // Put image at the center of panel
	    int offset_Y;
	    offset_Y = (g_u32VpostHeight - Tarheight)/2 * g_u32VpostWidth * 2;      // For RGB565
	  //  printf("offset_Y is %d\n",offset_Y);
	    offset_Y = 168000;//60 * g_u32VpostWidth * 2; 
	   //offset_Y = -5 * g_u32VpostWidth * 2;  //ok !! but  system  crash 
	    vpe_setting.dest_addrPac = (unsigned int)pDstBuf + offset_Y;
		//printf("pDstBuf is %d\n",pDstBuf);

	}
	if ( g_u32VpostWidth > Tarwidth ) {

		vpe_setting.dest_leftoffset = (g_u32VpostWidth-Tarwidth)/2;
		vpe_setting.dest_rightoffset = (g_u32VpostWidth-Tarwidth)/2;

	} else {
		vpe_setting.dest_width = g_u32VpostWidth;
		vpe_setting.dest_height = g_u32VpostHeight;
		vpe_setting.dest_leftoffset = 0;
		vpe_setting.dest_rightoffset = 0;		
	}	
	vpe_setting.algorithm = VPE_SCALE_DDA;
	vpe_setting.rotation = VPE_OP_NORMAL;//VPE_OP_NORMAL;		
   //jimmy +++ change the place of windows
   
//  vpe_setting.src_leftoffset=50;

 // vpe_setting.src_rightoffset=-50;  //jimmy+++it  is set the place in the picture
//  printf("vpe_setting.src_leftoffset =%d,vpe_setting.src_rightoffset=%d\n",vpe_setting.src_leftoffset,vpe_setting.src_rightoffset);
//   vpe_setting.dest_width=0;
 //   vpe_setting.dest_height=300;
//   vpe_setting.dest_width=0;

 //  printf("vpe_setting.dest_width=%d,vpe_setting.dest_height=%d\n",vpe_setting.dest_width,vpe_setting.dest_height);

	//vpe_setting.dest_leftoffset=0;
	//vpe_setting.dest_rightoffset=480;
      vpe_setting.dest_leftoffset=0;
	  vpe_setting.dest_rightoffset=320;
	//printf("vpe_setting.dest_leftoffset=%d,vpe_setting.dest_rightoffset=%d\n",vpe_setting.dest_leftoffset,vpe_setting.dest_rightoffset);
	if((ioctl(vpe_fd, VPE_SET_FORMAT_TRANSFORM, &vpe_setting)) < 0)	{
		close(vpe_fd);
		printf("VPE_IO_GET fail\n");
		ret=-3;	goto L_VPE_EXIT;
	}
	if((ioctl(vpe_fd, VPE_TRIGGER, NULL)) < 0) {
		close(vpe_fd);
		printf("VPE_TRIGGER info fail\n");
		ret=-4;	goto L_VPE_EXIT;
	}
L_VPE_EXIT:
	return ret;
	
	return 0;
}

static void FiniFB()
{
	if(fb_fd>0) {
		printf("FiniFB\n");
		//jimmy // for not fresh lcd 
		//memset ( FB_VAddress, 0x0, g_u32VpostBufMapSize );
		//usleep(100);
		//munmap(FB_VAddress, g_u32VpostBufMapSize);
		close(fb_fd);
		fb_fd = -1;
		g_u32VpostBufMapSize = 0;
		FB_VAddress = NULL;
	}
}

static int InitFB()
{
//jimmy
//if(fb_fd<0)
//{
	fb_fd = open(fbdevice, O_RDWR);
	if (fb_fd == -1) {
		perror("open fbdevice fail");
		return -1;
	}	
	if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &g_fb_var) < 0) {
		perror("ioctl FBIOGET_VSCREENINFO");
		close(fb_fd);
		fb_fd = -1;
		return -1;
	}	
	g_u32VpostWidth = g_fb_var.xres;
	g_u32VpostHeight = g_fb_var.yres;
	fb_bpp = g_fb_var.bits_per_pixel/8;	
	printf("FB width =%d, height = %d, bpp=%d\n",g_u32VpostWidth,g_u32VpostHeight,fb_bpp);	
	// Get FB Virtual Buffer address
	if (ioctl(fb_fd, IOCTL_LCD_GET_DMA_BASE, &FB_PAddress) < 0) {
		perror("ioctl IOCTL_LCD_GET_DMA_BASE ");
		close(fb_fd);
		return -1;
	}
#ifdef _USE_FBIOPAN_
	g_u32VpostBufMapSize = g_u32VpostWidth*g_u32VpostHeight*fb_bpp*2;	/* MAP 2 buffer */
#else
	g_u32VpostBufMapSize = g_u32VpostWidth*g_u32VpostHeight*fb_bpp*1;	/* MAP 1 buffer */
#endif
	//jimmy // for not fresh the led
/*
	FB_VAddress = mmap(NULL, 	g_u32VpostBufMapSize, 
					PROT_READ|PROT_WRITE, 
					MAP_SHARED, 
					fb_fd, 
					0);

	if ( FB_VAddress == MAP_FAILED) {
		printf("LCD Video Map failed!\n");
		close(fb_fd);
		return -1;
	}*/
//}
	return 0;
}

static void FiniVPE(void)
{
	if(vpe_fd>0) {
		WaitVPEngine();
		close(vpe_fd);
		vpe_fd = -1;
	}
}

static int InitVPE(void)
{
	//jimmy
	//if(vpe_fd<0)
	//	{
			vpe_fd = open(vpe_device, O_RDWR);		
			if (vpe_fd < 0) {
				printf("open %s error\n", vpe_device);
				return -1;	
			}	
	//	}
			return 0;
		
}

int InitDisplay(void)
{
	printf("InitDisplay\n");
	refresh_flag=1;
	printf(" InitDisplay refresh_flag is %d\n",refresh_flag);
	if ( InitFB() < 0 ) {
		printf("InitFB failure\n");
	}
	else if ( InitVPE() < 0 ) {
		printf("InitVPE failure\n");
		FiniFB();
	} else
		return 0;

	return -1;
}

void FiniDisplay(void)
{


	FiniVPE();
	FiniFB();

	printf("FiniDisplay\n");
	refresh_flag=0;
	printf(" FiniDisplay refresh_flag is %d\n",refresh_flag);
	//linphonec_out("123");


	
}


