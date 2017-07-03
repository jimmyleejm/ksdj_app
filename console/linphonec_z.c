/****************************************************************************
 *
 *  $Id: linphonec.c,v 1.57 2007/11/14 13:40:27 smorlat Exp $
 *
 *  Copyright (C) 2006  Sandro Santilli <strk@keybit.net>
 *  Copyright (C) 2002  Florian Winterstein <flox@gmx.net>
 *  Copyright (C) 2000  Simon MORLAT <simon.morlat@free.fr>
 *
****************************************************************************
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 ****************************************************************************/
#include <string.h>
#ifndef _WIN32_WCE
#include <sys/time.h>
#include <sys/types.h>
#include <linux/limits.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include "private.h" /*coreapi/private.h, needed for LINPHONE_VERSION */
#endif /*_WIN32_WCE*/
#include <limits.h>
#include <ctype.h>
#include <stdlib.h>

#include <linphonecore.h>

#include "linphonec.h"

#ifdef WIN32
#include <ws2tcpip.h>
#include <ctype.h>
#ifndef _WIN32_WCE
#include <conio.h>
#endif /*_WIN32_WCE*/
#else
#include <sys/socket.h>
#include <netdb.h>
#include <sys/un.h>
#include <sys/stat.h>
#endif

#if defined(_WIN32_WCE)

#if !defined(PATH_MAX)
#define PATH_MAX 256
#endif /*PATH_MAX*/

#if !defined(strdup)
#define strdup _strdup
#endif /*strdup*/

#endif /*_WIN32_WCE*/

#ifdef HAVE_GETTEXT
#include <libintl.h>
#ifndef _
#define _(String) gettext(String)
#endif
#else
#define _(something)	(something)
#endif

#ifndef PACKAGE_DIR
#define PACKAGE_DIR ""
#endif

#ifdef HAVE_X11_XLIB_H
#include <X11/Xlib.h>
#endif

#include "keypads.h"
#include <sys/soundcard.h>





#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <asm/ioctl.h>
#include <sys/mman.h>
#include <linux/fb.h>

#include <net/if_arp.h>  
#include <net/if.h>  
#include <arpa/inet.h>  
#include <sys/ioctl.h>  

#include <net/route.h>  
#include <fcntl.h>
#include <pthread.h>



#include     <termios.h>    /*PPSIX 终端控制定义*/
#include <linux/watchdog.h>



#define LCD_ENABLE_INT		_IO('v', 28)

#define PORT 6666

#define Set_door_address6

int call_state=0;
pthread_mutex_t Device_mutex ;

FILE *input;
FILE *output;
char buff_4[2]={0xBB,0xCC};

#ifdef Set_door_address6
char buff_3[6]={0xaf,0x63,0xee,0x01,0xA5,0xFF};//read num of the door
#else
char buff_3[5]={0x63,0xee,0x01,0xA5,0xFF};//read num of the door
#endif

char buff_2[5]={0xAB,0x01,0x05,0xAA,0xFF};//call 
#ifdef Set_door_address6
char buff_1[6]={0xaf,0x63,0x01,0x01,0x05,0xFF};//set num of the door
#else
char buff_1[5]={0x63,0x01,0x01,0x05,0xFF};//set num of the door
#endif

int fist_line=0;

char buff_rec[3]={0};//call 
char buff_men_call_sn[3]={0};//call 
char buff_men_call_sn_men[3]={0};//call 
char buff_open_door[3]={0};//call 
char buff_open_door_men[3]={0};//call 
char buff_glj_open_door[3]={0};//call 

char buff_mkj_open_door[3]={0};//call 
char buff_alarm[3]={0};//call 


char buff_men[3]={0};//call 


int timekeeping_start=0;

int mkj_num=0;

int fglj_mkj_num=0;

int mts_mkj_num=0;

int wgj_call_sn_falg=0;

char wgj_call_sn_open[3]={0};//call 
int  wgj_call_sn_num=0;
char wgj_huang[3]={0};//call 

#ifdef Doorphone_compile
extern int call_glj_flag;
#else
extern int mkj_call_flag;
#endif


/***************************************************************************
 *
 *  Types
 *
 ***************************************************************************/

typedef struct {
	LinphoneAuthInfo *elem[MAX_PENDING_AUTH];
	int nitems;
} LPC_AUTH_STACK;

/***************************************************************************
 *
 *  Forward declarations
 *
 ***************************************************************************/
//#ifndef Doorphone_compile
	extern void linphone_call_start_audio_stream(LinphoneCall *call, const char *cname, bool_t muted, bool_t send_ringbacktone, bool_t use_arc);
//#endif

char *lpc_strip_blanks(char *input);
#if defined(__ENABLE_KEYPAD__) || defined(__ENABLE_VKEYPAD)
char *sip_terminate_call = NULL;
char *sip_answer_call = NULL;
char *sip_addrurl_call = NULL;
#endif
static int handle_configfile_migration(void);
#if !defined(_WIN32_WCE)
static int copy_file(const char *from, const char *to);
#endif /*_WIN32_WCE*/
static int linphonec_parse_cmdline(int argc, char **argv);
static int linphonec_init(int argc, char **argv);
static int linphonec_main_loop (LinphoneCore * opm);
static int linphonec_idle_call (void);
#ifdef HAVE_READLINE
static int linphonec_initialize_readline(void);
static int linphonec_finish_readline();
static char **linephonec_readline_completion(const char *text,
	int start, int end);
#endif

/* These are callback for linphone core */
static void linphonec_prompt_for_auth(LinphoneCore *lc, const char *realm,
	const char *username);
static void linphonec_display_refer (LinphoneCore * lc, const char *refer_to);
static void linphonec_display_something (LinphoneCore * lc, const char *something);
static void linphonec_display_url (LinphoneCore * lc, const char *something, const char *url);
static void linphonec_display_warning (LinphoneCore * lc, const char *something);
static void linphonec_notify_received(LinphoneCore *lc, LinphoneCall *call, const char *from,const char *event);

static void linphonec_notify_presence_received(LinphoneCore *lc,LinphoneFriend *fid);
static void linphonec_new_unknown_subscriber(LinphoneCore *lc,
		LinphoneFriend *lf, const char *url);

static void linphonec_text_received(LinphoneCore *lc, LinphoneChatRoom *cr,
		const LinphoneAddress *from, const char *msg);
static void linphonec_display_status (LinphoneCore * lc, const char *something);
static void linphonec_dtmf_received(LinphoneCore *lc, LinphoneCall *call, int dtmf);
static void print_prompt(LinphoneCore *opm);
void linphonec_out(const char *fmt,...);

void time_keeping_fun(int time);

/***************************************************************************
 *
 * Global variables
 *
 ***************************************************************************/

LinphoneCore *linphonec;
FILE *mylogfile;
#ifdef HAVE_READLINE
static char *histfile_name=NULL;
static char last_in_history[256];
#endif
//auto answer (-a) option


//#ifdef Doorphone_compile
static bool_t auto_answer=FALSE;//jimmy +++ auto_answer
//#else
//static bool_t auto_answer=TRUE;
//#endif

static bool_t answer_call=FALSE;
static bool_t vcap_enabled=FALSE;
static bool_t display_enabled=FALSE;
static bool_t preview_enabled=FALSE;
static bool_t show_general_state=FALSE;

//jimmy +++ if use touch TRUE otherwise use FALSE
//static bool_t unix_socket=FALSE;
//#define Doorphone_compile
#define Doorphone_test

#define jimmy_debug_print


#ifdef Doorphone_compile
int light_run(int toggle);
int kpd_state=0;


#endif

//#ifdef Doorphone_compile
//static bool_t unix_socket=FALSE;
//#else
static bool_t unix_socket=TRUE;
//#endif


static bool_t start_voip=TRUE;
static ms_mutex_t prompt_mutex;

static char received_prompt[PROMPT_MAX_LEN];

static bool_t have_prompt=FALSE;


static bool_t linphonec_running=TRUE;
LPC_AUTH_STACK auth_stack;
static int trace_level = 0;
static char *logfile_name = NULL;
static char configfile_name[PATH_MAX];
static char zrtpsecrets[PATH_MAX];
static const char *factory_configfile_name=NULL;
static char *sip_addr_to_call = NULL; /* for autocall */
static int window_id = 0; /* 0=standalone window, or window id for embedding video */
#if !defined(_WIN32_WCE)
static ortp_pipe_t client_sock=ORTP_PIPE_INVALID;
#endif /*_WIN32_WCE*/
char prompt[PROMPT_MAX_LEN];
#if !defined(_WIN32_WCE)
static ortp_thread_t pipe_reader_th;
static bool_t pipe_reader_run=FALSE;
#endif /*_WIN32_WCE*/
#if !defined(_WIN32_WCE)
static ortp_pipe_t server_sock;
#endif /*_WIN32_WCE*/

bool_t linphonec_camera_enabled=TRUE;

//jimmy +++ for contact with  UI
int server_sockfd=-1;
int client_sockfd=-1;

int ins_to_flag=0;
int call_ins_flag=0;
int tonghua_bug_flag=0;

extern int ins_call_flag;
int times_set=0;
int times_uart=0;
int times_no_do=0;

int lock_flag=0;
int call_occupy_flag=0;
#ifdef Doorphone_compile

int data_num[30];
//int passwd_num[10];
int data_pint=0;

#endif

char buf_showtime[80];
int  vice_used=0;
int  af_no_self=0;
char buf_print[300];



	void watch_dog_func()
		{
				int fd = open("/dev/watchdog", O_WRONLY);	
				if (fd == -1) {		
					perror("watchdog");		
					return -1;
				}	
				printf("Open watchdog ok\n");	
				while (1) {		
						//	printf(" watchdog !!\n");	
						ioctl(fd, WDIOC_KEEPALIVE);		
					sleep(5);	}	
					close(fd);	
					return 0;
		}



	void watch_dog()
{
	pthread_t pid_dog;
//	func_set =func;


		
	printf("main watch_dog \n");

	if (pthread_create(&pid_dog,NULL,(void*)watch_dog_func,NULL)!=0) {
				   printf("Create thread error!\n");
				   return ;
			   }
	//pthread_detach(pid_ui);

}

	

void cur_time(char * time_time)
{
char *wday[]={"星期天","星期一","星期二","星期三","星期四","星期五","星期六"};
time_t timep;
struct tm *p;
time(&timep);
p=localtime(&timep); 
//printf("%d年%02d月%02d日",(1900+p->tm_year),(1+p->tm_mon),p->tm_mday);
//printf("%s %02d:%02d:%02d\n",wday[p->tm_wday],p->tm_hour,p->tm_min,p->tm_sec);

//sprintf(time_time,"%d年%02d月%02d日 %s %02d:%02d:%02d",(1900+p->tm_year),(1+p->tm_mon),p->tm_mday,wday[p->tm_wday]
//,p->tm_hour,p->tm_min,p->tm_sec);

sprintf(time_time,"%02d月%02d日 %s %02d:%02d:%02d",(1+p->tm_mon),p->tm_mday,wday[p->tm_wday]
,p->tm_hour,p->tm_min,p->tm_sec);

//sprintf(time_time,"%02d:%02d:%02d",p->tm_hour,p->tm_min,p->tm_sec);
//sprintf(time_week,"%s",wday[p->tm_wday]);


//printf("%s %02d:%02d:%02d\n",wday[p->tm_wday],p->tm_hour,p->tm_min,p->tm_sec);

}


void linphonec_call_identify(LinphoneCall* call){
	static long callid=1;
	linphone_call_set_user_pointer (call,(void*)callid);
	callid++;
}

LinphoneCall *linphonec_get_call(long id){
	const MSList *elem=linphone_core_get_calls(linphonec);
	for (;elem!=NULL;elem=elem->next){
		LinphoneCall *call=(LinphoneCall*)elem->data;
		if (linphone_call_get_user_pointer (call)==(void*)id){
			return call;
		}
	}
	linphonec_out("Sorry, no call with id %i exists at this time.\n",id);
	return NULL;
}

/***************************************************************************
 *
 * Linphone core callbacks
 *
 ***************************************************************************/

/*
 * Linphone core callback
 */
static void
linphonec_display_refer (LinphoneCore * lc, const char *refer_to)
{
	linphonec_out("Receiving out of call refer to %s\n", refer_to);
}

/*
 * Linphone core callback
 */
static void
linphonec_display_something (LinphoneCore * lc, const char *something)
{
	fprintf (stdout, "%s\n%s", something,prompt);
	fflush(stdout);
}

/*
 * Linphone core callback
 */
static void
linphonec_display_status (LinphoneCore * lc, const char *something)
{
	fprintf (stdout, "%s\n%s", something,prompt);
	fflush(stdout);
}

/*
 * Linphone core callback
 */
static void
linphonec_display_warning (LinphoneCore * lc, const char *something)
{
	fprintf (stdout, "Warning: %s\n%s", something,prompt);
	fflush(stdout);
}

/*
 * Linphone core callback
 */
static void
linphonec_display_url (LinphoneCore * lc, const char *something, const char *url)
{
	fprintf (stdout, "%s : %s\n", something, url);
}

/*
 * Linphone core callback
 */
static void
linphonec_prompt_for_auth(LinphoneCore *lc, const char *realm, const char *username)
{
	/* no prompt possible when using pipes or tcp mode*/
	if (unix_socket){
		linphone_core_abort_authentication(lc,NULL);
	}else{
		LinphoneAuthInfo *pending_auth;

		if ( auth_stack.nitems+1 > MAX_PENDING_AUTH )
		{
			fprintf(stderr,
				"Can't accept another authentication request.\n"
				"Consider incrementing MAX_PENDING_AUTH macro.\n");
			return;
		}

		pending_auth=linphone_auth_info_new(username,NULL,NULL,NULL,realm);
		auth_stack.elem[auth_stack.nitems++]=pending_auth;
	}
}

/*
 * Linphone core callback
 */
static void
linphonec_notify_received(LinphoneCore *lc, LinphoneCall *call, const char *from,const char *event)
{
	if(!strcmp(event,"refer"))
	{
		linphonec_out("The distand endpoint %s of call %li has been transfered, you can safely close the call.\n",
		              from,(long)linphone_call_get_user_pointer (call));
	}
}


/*
 * Linphone core callback
 */
static void
linphonec_notify_presence_received(LinphoneCore *lc,LinphoneFriend *fid)
{
	char *tmp=linphone_address_as_string(linphone_friend_get_address(fid));
	printf("Friend %s is %s\n", tmp, linphone_online_status_to_string(linphone_friend_get_status(fid)));
	ms_free(tmp);
	// todo: update Friend list state (unimplemented)
}

/*
 * Linphone core callback
 */
static void
linphonec_new_unknown_subscriber(LinphoneCore *lc, LinphoneFriend *lf,
		const char *url)
{
	printf("Friend %s requested subscription "
		"(accept/deny is not implemented yet)\n", url);
	// This means that this person wishes to be notified
	// of your presence information (online, busy, away...).

}

static void linphonec_call_updated(LinphoneCall *call){
	const LinphoneCallParams *cp=linphone_call_get_current_params(call);
	if (!linphone_call_camera_enabled (call) && linphone_call_params_video_enabled (cp)){
		linphonec_out("Far end requests to share video.\nType 'camera on' if you agree.\n");
	}
}

static void linphonec_call_encryption_changed(LinphoneCore *lc, LinphoneCall *call, bool_t encrypted, const char *auth_token) {
	long id=(long)linphone_call_get_user_pointer (call);
	if (!encrypted) {
		linphonec_out("Call %i is not fully encrypted and auth token is %s.\n", id,
				(auth_token != NULL) ? auth_token : "absent");
	} else {
		linphonec_out("Call %i is fully encrypted and auth token is %s.\n", id,
				(auth_token != NULL) ? auth_token : "absent");
	}
}



int Draw_Ui(int picture_num)
{

	int fd, ret;
	int i, t = 0;	
	//Cursor cur; 		
	FILE *fpVideoImg;	
	unsigned char *pVideoBuffer;	
	unsigned long uVideoSize;	
	char x[5], y[5];	
//	ActiveWindow window;	
	unsigned int uStartLine;	
//	video_scaling v_scaling;	
	unsigned int select = 0;	
	char inputstring[10];	
	int brightness=2000;
	static struct fb_var_screeninfo var;
	//cur.x = cur.y = 10; memset(x, 0, 5);	
	memset(y, 0, 5);	
	fd = open("/dev/fb0", O_RDWR);	
	if (fd == -1)	
		{		
		printf("Cannot open fb0!\n");		
		return -1;	
		}		
	if (ioctl(fd, FBIOGET_VSCREENINFO, &var) < 0) 
		{ 	
		perror("ioctl FBIOGET_VSCREENINFO");		
		close(fd);		
		return -1;	
		}
	uVideoSize = var.xres * var.yres * var.bits_per_pixel / 8;
	pVideoBuffer = mmap(NULL, uVideoSize, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	if(pVideoBuffer == MAP_FAILED)	
		{		
		printf("LCD Video Map Failed!\n");		
		return -1;
		}
	if (picture_num == 1)		
		fpVideoImg = fopen("/mnt/nand1-1/main.dat", "r"); 	
	else if (picture_num == 2)		
		fpVideoImg = fopen("/mnt/nand1-1/connect.dat", "r"); 

		 if(fpVideoImg == NULL)    
		 	{    	
		 	printf("open Image FILE fail !! \n");    	
			return -1;   
			}  


			if( fread(pVideoBuffer, uVideoSize, 1, fpVideoImg) <= 0)	
				{		
				printf("Cannot Read the Image File!\n");		
			    return -1;	
			}
			ioctl(fd,LCD_ENABLE_INT);
			close(fd);

			return 0;
}

int Rec_call=0;
int mem_to_sn_occupy=0;
int glj_occupy=0;
long call_id;
//jimmy linphonec_call_state_changed
static void linphonec_call_state_changed(LinphoneCore *lc, LinphoneCall *call, LinphoneCallState st, const char *msg){
	char *from=linphone_call_get_remote_address_as_string(call);
	char *from_point;
	long id=(long)linphone_call_get_user_pointer (call);
	int i=0;
	int ret=-1;
	char log_file_name[50];
	char terminate_buf[30];
	FILE* fp_logs,* local_logs;
	int wqj_flag=0;

//	call_id=id;
	//printf("linphonec_call_state_changed!!!\n");
	switch(st){
		case LinphoneCallEnd:
			//jimmy bug!!!
			
			ret=strncmp("Not answered", linphone_reason_to_string(linphone_call_get_reason(call)),sizeof("Not answered"));
			if(ret==0 && glj_occupy==1)
				{
					
				}
			else
				{
				//linphonec_out("Call %i with %s ended (%s).\n", id, from, linphone_reason_to_string(linphone_call_get_reason(call)));
				memset(buf_print,0,sizeof(buf_print));
			sprintf(buf_print,"Call %i with %s ended (%s).\n", id, from, linphone_reason_to_string(linphone_call_get_reason(call)));
			linphonec_out(buf_print);
				glj_occupy=0;

				}
			
			/*

			if(glj_occupy==1)
				{
					linphonec_out("Call %i with %s ended (%s).\n", id, from, linphone_reason_to_string(linphone_call_get_reason(call)));
					glj_occupy=0;
				}
			else
				{
				linphonec_out("Call %i with %s ended (%s).\n", id, from, linphone_reason_to_string(linphone_call_get_reason(call)));

				}
				
			*/

			
			//Draw_Ui(1);
			//jimmy
			//lpc_cmd_call_logs();
			/*
			ortp_mutex_lock(&prompt_mutex);
	
			strcpy(received_prompt,"call-logs");
			//strcpy(received_prompt,"status register\0");
			//	 printf("Receiving command '%s'\n",received_prompt);kfflush(stdout);
			have_prompt=TRUE;
			ortp_mutex_unlock(&prompt_mutex);
			*/
			//jimmy
			#ifdef Doorphone_compile
			

        if(mem_to_sn_occupy==0)
		{
			call_state=0;
			if(ins_to_flag==1)
				{
					 ins_to_flag=0;
					 tonghua_bug_flag=0;

					
					  usleep(50000);
		              printf(" wirte  bb\n");
		          
		              fputc(buff_4[0],output);
		             
		              usleep(50000);
		              fputc(buff_4[1],output);
		              usleep(50000);
						
					  
				}
			
				
			if(call_ins_flag==1)
				{
					  call_ins_flag=0;
				if(vice_used==0)
						{
					  usleep(50000);
		              printf(" wirte  bb\n");
		          
		              fputc(buff_4[0],output);
		             
		              usleep(50000);
		              fputc(buff_4[1],output);
		              usleep(50000);
						}
				
				}
			if(times_set>0)
			{
			times_no_do=1;
			times_set=0;
				}

		light_run(0);
			//jimmy set gpio normal stand
		swich_gpio_set(0,0,1,1);
		}
	else
		{
		mem_to_sn_occupy=0;

	    }
		call_occupy_flag=0;
	
		#endif
		
			if(ret==0 && glj_occupy==1)
			{

			}
		 else
		 	{
			ins_call_flag=0;	 
			Rec_call=0;

		 	}


		#ifdef Doorphone_compile
		call_glj_flag=0;

		#else
		mkj_call_flag=0;
		#endif
		
		break;
		case LinphoneCallResuming:
		//	linphonec_out("Resuming call %i with %s.\n", id, from);
			memset(buf_print,0,sizeof(buf_print));
			sprintf(buf_print,"Resuming call %i with %s.\n", id, from);
			linphonec_out(buf_print);
		break;
		case LinphoneCallStreamsRunning:
		//	linphonec_out("Media streams established with %s for call %i (%s).\n", from,id,( linphone_call_params_video_enabled( linphone_call_get_current_params(call)) ? "video":"audio"));
			call_id=id;
			memset(buf_print,0,sizeof(buf_print));
			sprintf(buf_print,"Media streams established with %s for call %i (%s).\n", from,id,( linphone_call_params_video_enabled( linphone_call_get_current_params(call)) ? "video":"audio"));
			linphonec_out(buf_print);
			
		//Draw_Ui(2);
			//printf("Media streams established with %s for call %i (%s).\n", from,id,( linphone_call_params_video_enabled( linphone_call_get_current_params(call)) ? "video":"audio"));
#ifndef Doorphone_compile	

			from_point=from;
			for(i=0;i<3;i++)
				{
					while(*from_point!='.')
						{
						from_point++;

						}
					from_point++;
					
					if(i==1)
					{
					mkj_num=*from_point-'0';

					}
				}
		
			memset(log_file_name,0,sizeof(log_file_name));
			if(from_point[1] == '>')
				{

				printf("from is %c %c \n",from_point[0],from_point[1]);
				buff_men[0]=0x00;
				buff_men[1]=from_point[0]-'0';
				printf("%02x,%02x\n",buff_men[0],buff_men[1]);
				sprintf(log_file_name,"/mnt/nand1-3/00%c\0",from_point[0]);
			}
			else if(from_point[2] == '>')
				{
				printf("from is %c %c %c \n",from_point[0],from_point[1],from_point[2]);
				buff_men[0]=0x00;
				buff_men[1]=(from_point[0]-'0')*16+from_point[1]-'0';
				printf("%02x,%02x\n",buff_men[0],buff_men[1]);
				sprintf(log_file_name,"/mnt/nand1-3/0%c%c\0",from_point[0],from_point[1]);

				
				}
			else
				{
				
				
				 printf("from is %c %c %c \n",from_point[0],from_point[1],from_point[2]);
				 buff_men[0]=from_point[0]-'0';
				 buff_men[1]=(from_point[1]-'0')*16+from_point[2]-'0';
				
				 printf("%02x,%02x\n",buff_men[0],buff_men[1]);
				 sprintf(log_file_name,"/mnt/nand1-3/%c%c%c\0",from_point[0],from_point[1],from_point[2]);

				 if(from_point[0]=='2' && from_point[1]=='1')
				 	{
					 wqj_flag=from_point[2]-'0';

				 	}

				}


				
			if(Rec_call==1)
				{
				if(ins_call_flag==1)
					{
					//buff_rec
					
					printf("%c%c%c%c%c%c%c%c%c%c\n",0xAF,0xAF,0xAF,0xAF,buff_men[0],buff_men[1],buff_rec[0],buff_rec[1],0xAA,0XFF);
					// printf("%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x\n",0xAF,0xAF,0xAF,0xAF,buff_men[0],buff_men[1],buff_rec[0],buff_rec[1],0xAA,0XFF);	
					
					cur_time(buf_showtime);
					printf("buf_showtime is %s",buf_showtime);
					printf("log_file_name is %s",log_file_name+13);
					//if((fp_logs=fopen("/mnt/nand1-1/121","a+"))==NULL)
					if((fp_logs=fopen(log_file_name,"a+"))==NULL)
					{
					printf("can not open the file.\n");
					//return -1;
					}
					else
						{
						//linphonec_out("Receiving new incoming call from %s,%c%c,%c%c, assigned id %i\n", from,
						//					buff_rec[0]/16+'0',buff_rec[0]%16+'0',buff_rec[1]/16+'0',buff_rec[1]%16+'0',id);

						fprintf(fp_logs,"%s 室内机%s%c%c%c%c 拨打 管理机\n",buf_showtime,log_file_name+13,buff_rec[0]/16+'0',buff_rec[0]%16+'0',buff_rec[1]/16+'0',buff_rec[1]%16+'0');
						fclose (fp_logs);
						}

						if((local_logs=fopen("/mnt/nand1-3/local_log","a+"))==NULL)
						{
						printf("can not open the file.\n");
						//return -1;
						}
					else
						{
						//linphonec_out("Receiving new incoming call from %s,%c%c,%c%c, assigned id %i\n", from,
						//					buff_rec[0]/16+'0',buff_rec[0]%16+'0',buff_rec[1]/16+'0',buff_rec[1]%16+'0',id);

						fprintf(local_logs,"%s 室内机%s%c%c%c%c 拨打 管理机\n",buf_showtime,log_file_name+13,buff_rec[0]/16+'0',buff_rec[0]%16+'0',buff_rec[1]/16+'0',buff_rec[1]%16+'0');
						fclose (local_logs);
						}

						
					}
				else
					{
					buff_glj_open_door[0]=buff_men[0];
					buff_glj_open_door[1]=buff_men[1];

					if(mkj_num==7)
						{
						if(wqj_flag>0)
							{
							printf("%c%c%c%c%c%c%c%c%c%c\n",0xAF,0xAF,0xAF,0xAF,buff_men[0],buff_men[1],0x00,0x00,0xAA,0Xa0+wqj_flag);
							//printf("%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x\n",0xAF,0xAF,0xAF,0xAF,buff_men[0],buff_men[1],0x00,0x00,0xAA,0Xa0+wqj_flag);
							wqj_flag=0;

							}
						else
							{
					printf("%c%c%c%c%c%c%c%c%c%c\n",0xAF,0xAF,0xAF,0xAF,buff_men[0],buff_men[1],0x00,0x00,0xAA,0XFF);
					//printf("%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x\n",0xAF,0xAF,0xAF,0xAF,buff_men[0],buff_men[1],0x00,0x00,0xAA,0XFF);
							}
						}
					else if(mkj_num==8)
						{
						printf("%c%c%c%c%c%c%c%c%c%c\n",0xAF,0xAF,0xAF,0xAF,buff_men[0],buff_men[1],0x00,0x00,0xAA,0X01);
					//printf("%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x\n",0xAF,0xAF,0xAF,0xAF,buff_men[0],buff_men[1],0x00,0x00,0xAA,0X01);

						}
					else if(mkj_num==9)
						{
						printf("%c%c%c%c%c%c%c%c%c%c\n",0xAF,0xAF,0xAF,0xAF,buff_men[0],buff_men[1],0x00,0x00,0xAA,0X02);
					//printf("%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x\n",0xAF,0xAF,0xAF,0xAF,buff_men[0],buff_men[1],0x00,0x00,0xAA,0X02);

						}
		
					cur_time(buf_showtime);
					printf("buf_showtime is %s",buf_showtime);
					printf("log_file_name is %s",log_file_name+13);
					//if((fp_logs=fopen("/mnt/nand1-1/121","a+"))==NULL)
					if((fp_logs=fopen(log_file_name,"a+"))==NULL)
					{
					printf("can not open the file.\n");
					//return -1;
					}
					else
						{
						fprintf(fp_logs,"%s 门口机%s 拨打 管理机\n",buf_showtime,log_file_name+13);
						fclose (fp_logs);
						}

						if((local_logs=fopen("/mnt/nand1-3/local_log","a+"))==NULL)
					{
					printf("can not open the file.\n");
					//return -1;
					}
					else
						{
						fprintf(local_logs,"%s 门口机%s 拨打 管理机\n",buf_showtime,log_file_name+13);
						fclose (local_logs);
						}
					}

				
				}
			else
				{
				if(ins_call_flag==1)
					{
					//buff_rec
					
					printf("%c%c%c%c%c%c%c%c%c%c\n",0xAF,0xAF,0xAF,0xAF,buff_men[0],buff_men[1],buff_rec[0],buff_rec[1],0xBB,0XFF);
					//printf("%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x\n",0xAF,0xAF,0xAF,0xAF,buff_men[0],buff_men[1],buff_rec[0],buff_rec[1],0xBB,0XFF);
					cur_time(buf_showtime);
					printf("buf_showtime is %s",buf_showtime);
					printf("log_file_name is %s",log_file_name+13);
					//if((fp_logs=fopen("/mnt/nand1-1/121","a+"))==NULL)
					if((fp_logs=fopen(log_file_name,"a+"))==NULL)
					{
					printf("can not open the file.\n");
					//return -1;
					}
					else
						{
						//linphonec_out("Receiving new incoming call from %s,%c%c,%c%c, assigned id %i\n", from,
						//					buff_rec[0]/16+'0',buff_rec[0]%16+'0',buff_rec[1]/16+'0',buff_rec[1]%16+'0',id);

						fprintf(fp_logs,"%s 管理机 拨打 室内机%s%c%c%c%c\n",buf_showtime,log_file_name+13,buff_rec[0]/16+'0',buff_rec[0]%16+'0',buff_rec[1]/16+'0',buff_rec[1]%16+'0');
						fclose (fp_logs);
						}

					if((local_logs=fopen("/mnt/nand1-3/local_log","a+"))==NULL)
					{
					printf("can not open the file.\n");
					//return -1;
					}
					else
						{
						//linphonec_out("Receiving new incoming call from %s,%c%c,%c%c, assigned id %i\n", from,
						//					buff_rec[0]/16+'0',buff_rec[0]%16+'0',buff_rec[1]/16+'0',buff_rec[1]%16+'0',id);

						fprintf(local_logs,"%s 管理机 拨打 室内机%s%c%c%c%c\n",buf_showtime,log_file_name+13,buff_rec[0]/16+'0',buff_rec[0]%16+'0',buff_rec[1]/16+'0',buff_rec[1]%16+'0');
						fclose (local_logs);
						}
					}
				else
					{
					printf("%c%c%c%c%c%c%c%c%c%c\n",0xAF,0xAF,0xAF,0xAF,buff_men[0],buff_men[1],0x00,0x00,0xBB,0XFF);
					//printf("%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x\n",0xAF,0xAF,0xAF,0xAF,buff_men[0],buff_men[1],0x00,0x00,0xBB,0XFF);
					cur_time(buf_showtime);
					printf("buf_showtime is %s",buf_showtime);
					printf("log_file_name is %s",log_file_name+13);
					//if((fp_logs=fopen("/mnt/nand1-1/121","a+"))==NULL)
					if((fp_logs=fopen(log_file_name,"a+"))==NULL)
					{
					printf("can not open the file.\n");
					//return -1;
					}
					else
						{
					fprintf(fp_logs,"%s 管理机 拨打 门口机%s\n",buf_showtime,log_file_name+13);
					fclose (fp_logs);

					
						}
						if((local_logs=fopen("/mnt/nand1-3/local_log","a+"))==NULL)
					{
					printf("can not open the file.\n");
					//return -1;
					}
					else
						{
					fprintf(local_logs,"%s 管理机 拨打 门口机%s\n",buf_showtime,log_file_name+13);
					fclose (local_logs);

					
						}

					}

				}
#endif


			glj_occupy=1;
			#ifdef Doorphone_compile
			times_set=60;
			call_occupy_flag=1;
			printf("ins_to_flag is %d\n",ins_to_flag);
			if((call_ins_flag==1) || (ins_to_flag==1) ) 
				{
				printf("tonghua_bug_flag is %d\n",tonghua_bug_flag);
				if(ins_to_flag==1)
				{
				if(tonghua_bug_flag==2)
					{
					printf("tonghua_bug_flag==2\n");
					times_set=0;
					}
				tonghua_bug_flag=0;
				
				}
				
				}
			else
				{
				light_run(1);
				pthread_mutex_lock(&Device_mutex);
				lcd_clean() ;
				disp_hanzi(0x80,"    通话中    ");			
				fist_line=1;
				pthread_mutex_unlock(&Device_mutex);
				}
			#endif

			if((call_ins_flag==1) || (ins_to_flag==1) ) 
				{
					#ifdef Doorphone_compile
					printf("set playbackgain  -7\n");
					ortp_mutex_lock(&prompt_mutex); 											
					strcpy(received_prompt,"playbackgain  -7");
					have_prompt=TRUE;
					ortp_mutex_unlock(&prompt_mutex);

					#else
					printf("set playbackgain  -14\n");
					ortp_mutex_lock(&prompt_mutex);												
					strcpy(received_prompt,"playbackgain  -14");
					have_prompt=TRUE;
					ortp_mutex_unlock(&prompt_mutex);
					#endif
			
				}
			else
				{
					printf("set playbackgain  -10\n");
					ortp_mutex_lock(&prompt_mutex);												
					strcpy(received_prompt,"playbackgain  -10");
					have_prompt=TRUE;
					ortp_mutex_unlock(&prompt_mutex);

				}
			
			
		break;
		case LinphoneCallPausing:
			//linphonec_out("Pausing call %i with %s.\n", id, from);
			memset(buf_print,0,sizeof(buf_print));
			sprintf(buf_print,"Pausing call %i with %s.\n", id, from);
			linphonec_out(buf_print);
		break;
		case LinphoneCallPaused:
		//	linphonec_out("Call %i with %s is now paused.\n", id, from);
			memset(buf_print,0,sizeof(buf_print));
			sprintf(buf_print,"Call %i with %s is now paused.\n", id, from);
			linphonec_out(buf_print);
		break;
		case LinphoneCallPausedByRemote:
			//linphonec_out("Call %i has been paused by %s.\n",id,from);
			memset(buf_print,0,sizeof(buf_print));
			sprintf(buf_print,"Call %i has been paused by %s.\n",id,from);
			linphonec_out(buf_print);
		break;
		case LinphoneCallIncomingReceived:
			linphonec_call_identify(call);
			linphone_call_enable_camera (call,linphonec_camera_enabled);
			id=(long)linphone_call_get_user_pointer (call);
			linphonec_set_caller(from);
			if ( auto_answer)  {
				answer_call=TRUE;
			}
			if(glj_occupy==0)
				{
				if(ins_call_flag==0)
					{
				//linphonec_out("Receiving new incoming call from %s assigned id %i\n", from,id);
				memset(buf_print,0,sizeof(buf_print));
				sprintf(buf_print,"Receiving new incoming call from %s assigned id %i\n", from,id);
				linphonec_out(buf_print);
				#ifndef Doorphone_compile
				mkj_call_flag=1;
				answer_call=TRUE;
				#endif
					}
				else
					{
				//linphonec_out("Receiving new incoming call from %s,%c%c,%c%c, assigned id %i\n", from,
				//	buff_rec[0]/16+'0',buff_rec[0]%16+'0',buff_rec[1]/16+'0',buff_rec[1]%16+'0',id);
				memset(buf_print,0,sizeof(buf_print));
				sprintf(buf_print,"Receiving new incoming call from %s,%c%c,%c%c, assigned id %i\n", from,
					buff_rec[0]/16+'0',buff_rec[0]%16+'0',buff_rec[1]/16+'0',buff_rec[1]%16+'0',id);
				linphonec_out(buf_print);
					}
				Rec_call=1;
				}
			
			#ifdef Doorphone_compile

		printf("Receiving new incoming call from %s assigned id %i\n", from,id);
		if(vice_used==1 && call_ins_flag==1)
			{
			printf("vice_used\n");
			ortp_mutex_lock(&prompt_mutex);
									
			strcpy(received_prompt,"terminate all");
			//strcpy(received_prompt,"status register\0");
			//	 printf("Receiving command '%s'\n",received_prompt);kfflush(stdout);
			have_prompt=TRUE;
			ortp_mutex_unlock(&prompt_mutex);
			break;
			}

		//printf("kpd_state =%d  \n",kpd_state);
		if(kpd_state!=0)
			{
			ortp_mutex_lock(&prompt_mutex);
									
			strcpy(received_prompt,"terminate all");
			//strcpy(received_prompt,"status register\0");
			//	 printf("Receiving command '%s'\n",received_prompt);kfflush(stdout);
			have_prompt=TRUE;
			ortp_mutex_unlock(&prompt_mutex);
			}
		else
			{
			if(call_occupy_flag==0)
				{
					if(call_ins_flag==0)
					{
					swich_gpio_set(1,1,0,1);
					MIC_run(0);
					}
					else
						{
						if (lc->ringstream!=NULL) {
						ms_message("stop ringing");
						ring_stop(lc->ringstream);
						ms_message("ring stopped");
						lc->ringstream=NULL;
						//was_ringing=TRUE;
					}
					if (call->ringing_beep){
						linphone_core_stop_dtmf(lc);
						call->ringing_beep=FALSE;
					}

					}
					call_state=3;
					 time_keeping_fun(60);
					 if(call_ins_flag!=1)
					 	{
					 	 time_keeping_fun(60);
					 memset(data_num,0,sizeof(data_num));
					 pthread_mutex_lock(&Device_mutex);
						lcd_clean() ;
					  disp_hanzi(0x80,"    接通中    ");		
					  fist_line=1;
					  pthread_mutex_unlock(&Device_mutex);
					  data_pint=0;
					 	}
					 call_occupy_flag=1;
						if((call_ins_flag==1) || (ins_to_flag==1) ) 
						{
						}
						else
						{
						light_run(1);
						}
				}
			else
				{
				//printf("call id is ")
				mem_to_sn_occupy=1;
				memset(terminate_buf,0,sizeof(terminate_buf));
				sprintf(terminate_buf,"terminate %i",id);
					printf("terminate is %s\n",terminate_buf);
				ortp_mutex_lock(&prompt_mutex);
											
				strcpy(received_prompt,terminate_buf);
				//strcpy(received_prompt,"status register\0");
				//	 printf("Receiving command '%s'\n",received_prompt);kfflush(stdout);
				have_prompt=TRUE;
				ortp_mutex_unlock(&prompt_mutex);
				}
			}
			#endif


			
			
		break;
		case LinphoneCallOutgoingInit:
			linphonec_call_identify(call);
			id=(long)linphone_call_get_user_pointer (call);
			//linphonec_out("Establishing call id to %s, assigned id %i\n", from,id);
			memset(buf_print,0,sizeof(buf_print));
			sprintf(buf_print,"Establishing call id to %s, assigned id %i\n", from,id);
			linphonec_out(buf_print);
	
		break;
		case LinphoneCallUpdatedByRemote:
			linphonec_call_updated(call);
		break;
		case LinphoneCallOutgoingProgress:
			//linphonec_out("Call %i to %s in progress.\n", id, from);
			memset(buf_print,0,sizeof(buf_print));
			sprintf(buf_print,"Call %i to %s in progress.\n", id, from);
			linphonec_out(buf_print);
			break;
		case LinphoneCallOutgoingRinging:
			//linphonec_out("Call %i to %s ringing.\n", id, from);
			memset(buf_print,0,sizeof(buf_print));
			sprintf(buf_print,"Call %i to %s ringing.\n", id, from);
			linphonec_out(buf_print);
			break;
		case LinphoneCallConnected:
			if(ins_to_flag==1)
				{
				tonghua_bug_flag=1;
				}
			//linphonec_out("Call %i with %s connected.\n", id, from);
			memset(buf_print,0,sizeof(buf_print));
			sprintf(buf_print,"Call %i with %s connected.\n", id, from);
			linphonec_out(buf_print);
			break;
		case LinphoneCallOutgoingEarlyMedia:
			//linphonec_out("Call %i with %s early media.\n", id, from);
			memset(buf_print,0,sizeof(buf_print));
			sprintf(buf_print,"Call %i with %s early media.\n", id, from);
			linphonec_out(buf_print);
			break;
		case LinphoneCallError:
			//linphonec_out("Call %i with %s error.\n", id, from);
			memset(buf_print,0,sizeof(buf_print));
			sprintf(buf_print,"Call %i with %s error.\n", id, from);
			linphonec_out(buf_print);
	
			//Draw_Ui(1);
			#ifdef Doorphone_compile

				if(call_ins_flag==1)
				{
					  call_ins_flag=0;
					}
			usleep(50000);
			  printf(" wirte  bb\n");
		  
			  fputc(buff_4[0],output);
			 
			  usleep(50000);
			  fputc(buff_4[1],output);
			  usleep(50000);
			
			ortp_mutex_lock(&prompt_mutex);
						
			strcpy(received_prompt,"terminate all");
			//strcpy(received_prompt,"status register\0");
			//	 printf("Receiving command '%s'\n",received_prompt);kfflush(stdout);
			have_prompt=TRUE;
			ortp_mutex_unlock(&prompt_mutex);

			light_run(0);
			printf("zhan xian \n");
			fist_line=1;
			 sleep(2);
		if(times_set>0)
			{
			pthread_mutex_lock(&Device_mutex);
			 lcd_clean() ;
			 disp_hanzi(0x80,"占线中请稍候");	
			 pthread_mutex_unlock(&Device_mutex);
			 sleep(2);
			 times_no_do=1;
			 times_set=0;
			 }
			 
			ins_to_flag=0;
			call_occupy_flag=0;
			if(ins_to_flag==1)
				{
				tonghua_bug_flag=0;
				}
			#endif
			ins_call_flag=0;
			#ifdef Doorphone_compile
			call_glj_flag=0;

			#else
			mkj_call_flag=0;
			#endif
			break;
		default:
		break;
	}
	ms_free(from);
}
int call_logs_get_func(int mount,int day,char *ip_buf)
{
	char *month[12]={"Jan","Feb","Mar","Apr","May ","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
	FILE*fp_w,*fp_r;
	int num_line=1,num_mark=0;
	char buf_tmp[100],buf_font[10],buf_sent[300];
	int num_nums=1,i=0;
	
	int sockfd;
	int err,n;
	int cret;
	struct sockaddr_in addr_ser;
	char sendline[100];
	 // char * pHeadPtr;
			if((fp_r=fopen("/mnt/nand1-1/call_logs_data","r"))==NULL)
		//if((fp_r=fopen("call_logs_data","r"))==NULL)
			{
			printf("can not open the file.\n");
			return -1;
			}
		/*
		    if((fp_w=fopen("/mnt/nand1-1/call_logs_data_check","w"))==NULL)
				{
				printf("can not open the file.\n");
				return -1;
				}
		*/
		sockfd=socket(AF_INET,SOCK_STREAM,0);
		if(sockfd==-1)
		{
			printf("socket error\n");
			return -1;
		}
		
		bzero(&addr_ser,sizeof(addr_ser));
		addr_ser.sin_family=AF_INET;
		addr_ser.sin_addr.s_addr=inet_addr(ip_buf);
		addr_ser.sin_port=htons(PORT);
	
	
		
	err=connect(sockfd,(struct sockaddr *)&addr_ser,sizeof(addr_ser));
	if(err==-1)
	{
		printf("connect error\n");
		return -1;
	
	
	}

			//sleep(1);
		//cret=send(sockfd,"heartbeat\n",11,0);
	  //sleep(ph_data->h_time);
		//printf("waiting for server...%d\n",cret);
	
			
			
			sprintf(buf_font,"%s  %d",month[mount-1],day);
			//printf("%s\n",buf_font);
			while(	!feof(fp_r)  )		
			{		
			   memset(buf_tmp, 0 , sizeof(buf_tmp) );
			   fgets( buf_tmp  ,	sizeof(buf_tmp)  , fp_r );
					
		if(( strstr(buf_tmp, buf_font)) != NULL)
			{
				//fprintf(fp_w,"%d\n",num_nums);
				//fprintf(fp_w,"%s",buf_tmp);
				
				memset(buf_sent, 0 , sizeof(buf_sent) );
				
				sprintf(buf_sent,"%d\n",num_nums);
				strcat(buf_sent,buf_tmp);
			//	cret=send(sockfd,buf_num,sizeof(buf_num),0);
			//	cret=send(sockfd,buf_tmp,sizeof(buf_tmp),0);
				printf("~~%s\n",buf_sent);
				for(i=0;i<4;i++)
				{
					 fgets( buf_tmp  ,	sizeof(buf_tmp)  , fp_r);
					 //fprintf(fp_w,"%s",buf_tmp);
					// cret=send(sockfd,buf_tmp,sizeof(buf_tmp),0);
					printf("!!%s\n",buf_sent);
					strcat(buf_sent,buf_tmp);
				}
				num_nums++;
				printf("##%s\n",buf_sent);
			cret=send(sockfd,buf_sent,sizeof(buf_sent),0);
			}

			
			num_line++;

			}
			//sleep(1);
            cret=send(sockfd,"end\n",sizeof("end\n"),0);
	        close(sockfd);
            fclose(fp_r);
		//	fclose(fp_w);
}
			




static void linphonec_dtmf_received(LinphoneCore *lc, LinphoneCall *call, int dtmf){
	char *from=linphone_call_get_remote_address_as_string(call);
	fprintf(stdout,"Receiving tone %c from %s\n",dtmf,from);
	fflush(stdout);
	ms_free(from);
}



static void *prompt_reader_thread(void *arg){
	char *ret;
	char tmp[PROMPT_MAX_LEN];
	while ((ret=fgets(tmp,sizeof(tmp),stdin))!=NULL){
		ms_mutex_lock(&prompt_mutex);
		strcpy(received_prompt,ret);
		have_prompt=TRUE;
		ms_mutex_unlock(&prompt_mutex);
	}
	return NULL;
}

static void start_prompt_reader(void){
	ortp_thread_t th;
	ms_mutex_init(&prompt_mutex,NULL);
	ortp_thread_create(&th,NULL,prompt_reader_thread,NULL);
}
#if !defined(_WIN32_WCE)
static ortp_pipe_t create_server_socket(void){
	char path[128];
#ifndef WIN32
	snprintf(path,sizeof(path)-1,"linphonec-%i",getuid());
#else
	{
		TCHAR username[128];
		DWORD size=sizeof(username)-1;
		GetUserName(username,&size);
		snprintf(path,sizeof(path)-1,"linphonec-%s",username);
	}
#endif
	return ortp_server_pipe_create(path);
}

/*
static void *pipe_thread(void*p){
	char tmp[250];
	server_sock=create_server_socket();
	printf("pipe_thread /n");
	if (server_sock==ORTP_PIPE_INVALID) return NULL;
	while(pipe_reader_run){
		while(client_sock!=ORTP_PIPE_INVALID){ 
#ifndef WIN32
			usleep(20000);
#else
			Sleep(20);
#endif
		}
		client_sock=ortp_server_pipe_accept_client(server_sock);
		if (client_sock!=ORTP_PIPE_INVALID){
			int len;
			//now read from the client 
			if ((len=ortp_pipe_read(client_sock,(uint8_t*)tmp,sizeof(tmp)-1))>0){
				ortp_mutex_lock(&prompt_mutex);
				tmp[len]='\0';
				strcpy(received_prompt,tmp);
				printf("Receiving command '%s'\n",received_prompt);fflush(stdout);
				have_prompt=TRUE;
				ortp_mutex_unlock(&prompt_mutex);
			}else{
				printf("read nothing\n");fflush(stdout);
				ortp_server_pipe_close_client(client_sock);
				client_sock=ORTP_PIPE_INVALID;
			}

		}else{
			if (pipe_reader_run) fprintf(stderr,"accept() failed: %s\n",strerror(errno));
		}
	}
	ms_message("Exiting pipe_reader_thread.");
	fflush(stdout);
	return NULL;
}

*/
void create_server_for_UI ()
{
        
        int server_len;
        struct sockaddr_un server_address;    
      

         memset(&server_address, 0, sizeof(struct sockaddr_un));  
         
        unlink ("/tmp/server_socket");     

        server_sockfd = socket (AF_UNIX, SOCK_STREAM, 0);
 
        server_address.sun_family = AF_UNIX;

        strcpy (server_address.sun_path, "/tmp/server_socket");

        server_len = sizeof (server_address);

        bind (server_sockfd, (struct sockaddr *)&server_address, server_len);

        listen (server_sockfd, 1);
        printf ("Server is waiting for client connect...\n");
        
		

}


int create_accept_for_UI()
{
	struct sockaddr_un client_address;
	int client_len;
	client_len = sizeof (client_address);
		memset(&client_address, 0, sizeof(struct sockaddr_un));  
			client_sockfd = accept (server_sockfd, (struct sockaddr *)&client_address, (socklen_t *)&client_len);
			printf("create_accept_for_UI client_sockfd =%d\n",client_sockfd);
			if (client_sockfd == -1) {
					perror ("accept");
				 //   exit (EXIT_FAILURE);
			}
			printf ("The server is waiting for client data...\n");
 return client_sockfd;

}

int button_one_init()
{
	int fp=-1;
	printf("\n**** GPIO Test Program ****\n");
	if ((fp = fopen("/sys/class/gpio/export", "w")) == NULL) 
		{			
		printf("Cannot open export file.\n");			
		return -1;		
		}		
		fprintf(fp, "%d", 13);		
	//	fprintf(fp, "%d", 14);	
		fclose(fp); 	
		// equivalent shell command "echo in > direction" to set the port as an input		
		if ((fp = fopen("/sys/class/gpio/gpio13/direction", "rb+")) == NULL) 
		{			
		printf("Cannot open direction file.\n");			
		return -1;			
		}		
		fprintf(fp, "in");	
			fclose(fp); 
/*
	    if ((fp = fopen("/sys/class/gpio/gpio14/direction", "rb+")) == NULL) 
		{			
		printf("Cannot open direction file.\n");			
		return -1;			
		}		
		fprintf(fp, "in");	
		fclose(fp); 	

 */
		// equivalent shell command "cat value" 	

}

int button_two_init()
{
	int fp=-1;
	printf("\n**** GPIO Test Program ****\n");
	if ((fp = fopen("/sys/class/gpio/export", "w")) == NULL) 
		{			
		printf("Cannot open export file.\n");			
		return -1;		
		}		
		fprintf(fp, "%d", 14);		
	//	fprintf(fp, "%d", 14);	
		fclose(fp); 	
		// equivalent shell command "echo in > direction" to set the port as an input		
		if ((fp = fopen("/sys/class/gpio/gpio14/direction", "rb+")) == NULL) 
		{			
		printf("Cannot open direction file.\n");			
		return -1;			
		}		
		fprintf(fp, "in");	
			fclose(fp); 

}

int button_one_check()
{
			
		int fp=-1;
		int value=1;
		char buffer[10];
			if ((fp = fopen("/sys/class/gpio/gpio13/value", "rb")) == NULL) 
				{
				printf("Cannot open value file.\n");				
				return -1;				
				} 
			
					fread(buffer, sizeof(char), sizeof(buffer) - 1, fp);
					value = atoi(buffer);	
				//	printf("value: %d\n", value);			
					fclose(fp); 		
					return value;
					
      

}

int button_two_check()
{
			
		int fp=-1;
		int value=1;
		char buffer[10];
			if ((fp = fopen("/sys/class/gpio/gpio14/value", "rb")) == NULL) 
				{
				printf("Cannot open value file.\n");				
				return -1;				
				} 
			
					fread(buffer, sizeof(char), sizeof(buffer) - 1, fp);
					value = atoi(buffer);	
				//	printf("value: %d\n", value);			
					fclose(fp); 		
					return value;
					
      

}


//jimmy pipe_thread
#ifdef Doorphone_compile
#define cs 1
#define sid 2
#define sclk 3
#define uint unsigned int 
#define uchar unsigned char

#define test_bit(bit, array)    (array[bit/8] & (1<<(bit%8)))

#define RETRY_NUM 4
int time_ckeck_kapd_num=0;


int state_monitor_flag=0;

int testdev;
int rec_bb_flag=0;
int memkou_call_sn=0;
int clean_men_flag=0;
int clean_lcd_flag=0;
int glj_num=1;
//char device[] = "/dev/input/event0";
int *  tmp_buf_logs[5];
char wqj_num;
int p_dsp, p_mixer;



extern int record_file(FILE*fp_w);
int call_glj_check(char *ip_check);

static int fd= -1;
static int init_device(void)
{
  char devname[256];
  static char keypad[] = "Keypad";
  struct input_id device_info;
  unsigned int yalv, i, result;
  unsigned char evtype_bitmask[EV_MAX/8 + 1];
  char device[] = "/dev/input/event0";

  result = 0;
  for (i = 0; i < RETRY_NUM; i++) {
    printf("trying to open %s ...\n", device);
  //  if ((fd=open(device,O_RDONLY|O_NONBLOCK)) < 0) {	
   if ((fd=open(device,O_RDONLY)) < 0) {	
      break;
    }
    memset(devname, 0x0, sizeof(devname));
    if (ioctl(fd, EVIOCGNAME(sizeof(devname)), devname) < 0) {
      printf("%s evdev ioctl error!\n", device);
    } else {    
      if (strstr(devname, keypad) != NULL) {
        printf("Keypad device is %s\n", device);
        result = 1;
        break;
      }
    }	
    close(fd);
    device[16]++;
  }
  if (result == 0) {
    printf("can not find any Keypad device!\n");
    return -1;
  }

  ioctl(fd, EVIOCGBIT(0, sizeof(evtype_bitmask)), evtype_bitmask);
    
  printf("Supported event types:\n");
  for (yalv = 0; yalv < EV_MAX; yalv++) {
    if (test_bit(yalv, evtype_bitmask)) {
      /* this means that the bit is set in the event types list */
      printf("  Event type 0x%02x ", yalv);
      switch ( yalv)
	{
	case EV_SYN :
	  printf(" EV_SYN\n");
	  break;
	case EV_REL :
	  printf(" EV_REL\n");
	  break;
	case EV_KEY :
	    printf(" EV_KEY\n");
	    break;
	default:
	  printf(" Other event type: 0x%04hx\n", yalv);
	}
      
    }        
  }
  return 0;
}
void swich_gpio_init()
{
  FILE *fp;
  
  //printf("Cannot open export file.\n");
  
  if ((fp = fopen("/sys/class/gpio/export", "w")) == NULL) {
			printf("swich_gpio_init Cannot open export file.\n");
			//exit(1);
		}
		fprintf(fp, "%d", 15);
		fclose(fp);
		
	if ((fp = fopen("/sys/class/gpio/gpio15/direction", "rb+")) == NULL) {
			printf("swich_gpio_init Cannot open direction file.\n");
			//exit(1);
		}
		fprintf(fp, "out");
		fclose(fp);
		
		
	   if ((fp = fopen("/sys/class/gpio/export", "w")) == NULL) {
			printf("swich_gpio_init Cannot open export file.\n");
			//exit(1);
		}
		fprintf(fp, "%d", 64);
		fclose(fp);
		
		if ((fp = fopen("/sys/class/gpio/gpio64/direction", "rb+")) == NULL) {
			printf(" swich_gpio_init Cannot open direction file.\n");
			//exit(1);
		}
		fprintf(fp, "out");
		fclose(fp);
		
		if ((fp = fopen("/sys/class/gpio/export", "w")) == NULL) {
			printf("swich_gpio_init Cannot open export file.\n");
			//exit(1);
		}
		fprintf(fp, "%d", 65);
		fclose(fp);
				
    if ((fp = fopen("/sys/class/gpio/gpio65/direction", "rb+")) == NULL) {
			printf("swich_gpio_init Cannot open direction file.\n");
			//exit(1);
		}
		fprintf(fp, "out");
		fclose(fp);
		
		if ((fp = fopen("/sys/class/gpio/export", "w")) == NULL) {
			printf("swich_gpio_init Cannot open export file.\n");
			//exit(1);
		}
		fprintf(fp, "%d", 66);
		fclose(fp);


		
		if ((fp = fopen("/sys/class/gpio/gpio66/direction", "rb+")) == NULL) {
			printf("swich_gpio_init Cannot open direction file.\n");
			//exit(1);
		}
		fprintf(fp, "out");
		fclose(fp);		
		
}

int swich_gpio_set(int flag_15,int flag_64,int flag_65,int flag_66)
{
	
	FILE *fp;

	char buffer[10];
	int value, i;
	int toggle = 0;
	
	if ((fp = fopen("/sys/class/gpio/gpio15/value", "rb+")) == NULL) {
				printf("swich_gpio_set Cannot open value file.\n");
				//exit(1);
			} else {
			/*
				toggle = !toggle;
				if(toggle)
					strcpy(buffer,"1");
				else
					strcpy(buffer,"0");
			*/
			if(flag_15)
			  strcpy(buffer,"1");
			else
				strcpy(buffer,"0");
				
				
				fwrite(buffer, sizeof(char), sizeof(buffer) - 1, fp);			
				printf("ste out value: %c\n", buffer[0]);
				fclose(fp);
			}
			
		if ((fp = fopen("/sys/class/gpio/gpio64/value", "rb+")) == NULL) {
				printf("swich_gpio_set Cannot open value file.\n");
				//exit(1);
			} else {
			/*
				toggle = !toggle;
				if(toggle)
					strcpy(buffer,"1");
				else
					strcpy(buffer,"0");
			*/
			if(flag_64)
			  strcpy(buffer,"1");
			else
				strcpy(buffer,"0");
				
				
				fwrite(buffer, sizeof(char), sizeof(buffer) - 1, fp);			
				printf("ste out value: %c\n", buffer[0]);
				fclose(fp);
			}
			
	if ((fp = fopen("/sys/class/gpio/gpio65/value", "rb+")) == NULL) {
				printf("swich_gpio_set Cannot open value file.\n");
				//exit(1);
			} else {
			/*
				toggle = !toggle;
				if(toggle)
					strcpy(buffer,"1");
				else
					strcpy(buffer,"0");
			*/
			if(flag_65)
			  strcpy(buffer,"1");
			else
				strcpy(buffer,"0");
				
				
				fwrite(buffer, sizeof(char), sizeof(buffer) - 1, fp);			
				printf("ste out value: %c\n", buffer[0]);
				fclose(fp);
			}
	if ((fp = fopen("/sys/class/gpio/gpio66/value", "rb+")) == NULL) {
				printf("swich_gpio_set Cannot open value file.\n");
				//exit(1);
			} else {
			/*
				toggle = !toggle;
				if(toggle)
					strcpy(buffer,"1");
				else
					strcpy(buffer,"0");
			*/
			if(flag_66)
			  strcpy(buffer,"1");
			else
				strcpy(buffer,"0");
				
				
				fwrite(buffer, sizeof(char), sizeof(buffer) - 1, fp);			
				printf("ste out value: %c\n", buffer[0]);
				fclose(fp);
			}
			
			return 0;
}


int opendoor_button_gpio_init()
{
	
	int value;

  FILE *fp_phone;
//  char filename[50];
		// equivalent shell command "echo 32 > export" to export the port 
			if ((fp_phone = fopen("/sys/class/gpio/export", "w")) == NULL) {
			printf("Cannot open export file.\n");
			return -1;
		}
		fprintf(fp_phone, "%d", 79);
		fclose(fp_phone);
		
		if ((fp_phone = fopen("/sys/class/gpio/gpio79/direction", "rb+")) == NULL) {
			printf("opendoor_button_gpio_init Cannot open direction file.\n");
			//exit(1);
		}
		fprintf(fp_phone, "in");
		fclose(fp_phone);
		return 0;
}

int opendoor_button_gpio_read()
{
FILE *fp_phone;
char buffer[10];
int  value;

	if ((fp_phone = fopen("/sys/class/gpio/gpio79/value", "rb")) == NULL) {
				printf("opendoor_button_gpio_read Cannot open value file.\n");
				//exit(1);
			} else {
				fread(buffer, sizeof(char), sizeof(buffer) - 1, fp_phone);
				value = atoi(buffer);
				//printf("value gpio68: %d\n", value);
				fclose(fp_phone);
				}
			return value;
}




int door_check_gpio_init()
{
	
	int value;

  FILE *fp_phone;
//  char filename[50];
		// equivalent shell command "echo 32 > export" to export the port 
			if ((fp_phone = fopen("/sys/class/gpio/export", "w")) == NULL) {
			printf("Cannot open export file.\n");
			return -1;
		}
		fprintf(fp_phone, "%d", 47);
		fclose(fp_phone);
		
		if ((fp_phone = fopen("/sys/class/gpio/gpio47/direction", "rb+")) == NULL) {
			printf("door_check_gpio_init Cannot open direction file.\n");
		//	exit(1);
		}
		fprintf(fp_phone, "in");
		fclose(fp_phone);
		return 0;
}





int door_check_gpio_read()
{
FILE *fp_phone;
char buffer[10];
int  value;

	if ((fp_phone = fopen("/sys/class/gpio/gpio47/value", "rb")) == NULL) {
				printf("door_check_gpio_read Cannot open value file.\n");
				//exit(1);
			} else {
				fread(buffer, sizeof(char), sizeof(buffer) - 1, fp_phone);
				value = atoi(buffer);
				//printf("value gpio68: %d\n", value);
				fclose(fp_phone);
				}
			return value;
}






int gpio_init()
{
	
	int value, i;

  FILE *fp_cs,*fp_sid,*fp_sclk;
//  char filename[50];
		// equivalent shell command "echo 32 > export" to export the port 
		if ((fp_cs = fopen("/sys/class/gpio/export", "w")) == NULL) {
			printf("Cannot open export file.\n");
			return -1;
		}
		fprintf(fp_cs, "%d", 76);
		fclose(fp_cs);
		
	
		// equivalent shell command "echo out > direction" to set the port as an input  
		if ((fp_cs = fopen("/sys/class/gpio/gpio76/direction", "rb+")) == NULL) {
			printf("Cannot open direction file.\n");
		  return -1;
		}
		fprintf(fp_cs, "out");
		fclose(fp_cs);
			//usleep(500000);		
		if ((fp_sid = fopen("/sys/class/gpio/export", "w")) == NULL) {
			printf("Cannot open export file.\n");
			return -1;
		}
		fprintf(fp_sid, "%d", 77);
		fclose(fp_sid);
		
		// equivalent shell command "echo out > direction" to set the port as an input  
		if ((fp_sid = fopen("/sys/class/gpio/gpio77/direction", "rb+")) == NULL) {
			printf("Cannot open direction file.\n");
		  return -1;
		}
		fprintf(fp_sid, "out");
		fclose(fp_sid);
		//usleep(500000);
		if ((fp_sclk = fopen("/sys/class/gpio/export", "w")) == NULL) {
			printf("Cannot open export file.\n");
		  return -1;
		}
		fprintf(fp_sclk, "%d", 78);
		fclose(fp_sclk);
		
		// equivalent shell command "echo out > direction" to set the port as an input  
		if ((fp_sclk = fopen("/sys/class/gpio/gpio78/direction", "rb+")) == NULL) {
			printf("Cannot open direction file.\n");
			return -1;
		}
		fprintf(fp_sclk, "out");
		fclose(fp_sclk);
		
  	return 0;
}


int gpio_set(int num,int toggle)
{
char filename[50];
int portnum;
FILE *fp;
char buffer[10];
memset(filename, 0, sizeof(filename));
memset(buffer, 0, sizeof(buffer));
if(num==cs)
	{
		portnum=76;
		}
	else if(num==sid)
		{
			portnum=77;
			
			}
	else if(num==sclk)
		{
			portnum=78;
			}
			
			
			sprintf(filename,"/sys/class/gpio/gpio%d/value",portnum);
			if ((fp = fopen(filename,"rb+")) == NULL) {
				printf("Cannot open value file.\n");
				return -1;
			} else {
				//toggle = !toggle;
				if(toggle)
					strcpy(buffer,"1");
				else
					strcpy(buffer,"0");
			
				fwrite(buffer, sizeof(char), sizeof(buffer) - 1, fp);		
				//if(num==2)	
				//printf("set num is %d out value: %c\n", num,buffer[0]);
				
				fclose(fp);
			}
			//usleep(500000);
	return 0;
}





int buzzer_gpio_init()
{
	
	int value, i;

  FILE *fp_buzzer;
//  char filename[50];
		// equivalent shell command "echo 32 > export" to export the port 
		if ((fp_buzzer = fopen("/sys/class/gpio/export", "w")) == NULL) {
			printf("Cannot open export file.\n");
			return -1;
		}
		fprintf(fp_buzzer, "%d", 67);
		fclose(fp_buzzer);
		
	
		// equivalent shell command "echo out > direction" to set the port as an input  
		if ((fp_buzzer = fopen("/sys/class/gpio/gpio67/direction", "rb+")) == NULL) {
			printf("Cannot open direction file.\n");
		  return -1;
		}
		fprintf(fp_buzzer, "out");
		fclose(fp_buzzer);
			//usleep(500000);		
		
		
  	return 0;
}




int buzzer_run(int toggle)
{
	FILE *fp;
	char buffer[10];
	
		memset(buffer, 0, sizeof(buffer));

	if ((fp = fopen("/sys/class/gpio/gpio67/value", "rb+")) == NULL) {
				printf("buzzer_run Cannot open value file.\n");
				//exit(1);
			} else {
			
				if(toggle)
					strcpy(buffer,"1");
				else
					strcpy(buffer,"0");
			
				fwrite(buffer, sizeof(char), sizeof(buffer) - 1, fp);			
				//printf("buzzer_run ste out value: %c\n", buffer[0]);
				fclose(fp);
			}
			
}


int  MIC_gpio_init()
{
	
	int value, i;

  FILE *fp_MIC;
//  char filename[50];
		// equivalent shell command "echo 32 > export" to export the port 
		if ((fp_MIC = fopen("/sys/class/gpio/export", "w")) == NULL) {
			printf("Cannot open export file.\n");
			return -1;
		}
		fprintf(fp_MIC, "%d", 3);
		fclose(fp_MIC);
		
	
		// equivalent shell command "echo out > direction" to set the port as an input  
		if ((fp_MIC = fopen("/sys/class/gpio/gpio3/direction", "rb+")) == NULL) {
			printf("Cannot open direction file.\n");
		  return -1;
		}
		fprintf(fp_MIC, "out");
		fclose(fp_MIC);
			//usleep(500000);		
		
		
  	return 0;
}




int MIC_run(int toggle)
{
	FILE *fp;
	char buffer[10];
	
		memset(buffer, 0, sizeof(buffer));

	if ((fp = fopen("/sys/class/gpio/gpio3/value", "rb+")) == NULL) {
				printf("MIC_run Cannot open value file.\n");
				//exit(1);
			} else {
			
				if(toggle)
					strcpy(buffer,"1");
				else
					strcpy(buffer,"0");
			
				fwrite(buffer, sizeof(char), sizeof(buffer) - 1, fp);			
				//printf("buzzer_run ste out value: %c\n", buffer[0]);
				fclose(fp);
			}
			
}


int  light_gpio_init()
{
	
	int value, i;

  FILE *fp_light;
//  char filename[50];
		// equivalent shell command "echo 32 > export" to export the port 
		if ((fp_light = fopen("/sys/class/gpio/export", "w")) == NULL) {
			printf("Cannot open export file.\n");
			return -1;
		}
		fprintf(fp_light, "%d", 167);
		fclose(fp_light);
		
	
		// equivalent shell command "echo out > direction" to set the port as an input  
		if ((fp_light = fopen("/sys/class/gpio/gpio167/direction", "rb+")) == NULL) {
			printf("Cannot open direction file.\n");
		  return -1;
		}
		fprintf(fp_light, "out");
		fclose(fp_light);
			//usleep(500000);		
		
		
  	return 0;
}




int light_run(int toggle)
{
	FILE *fp;
	char buffer[10];
	
		memset(buffer, 0, sizeof(buffer));

	if ((fp = fopen("/sys/class/gpio/gpio167/value", "rb+")) == NULL) {
				printf("light_run Cannot open value file.\n");
				//exit(1);
			} else {
			
				if(toggle)
					strcpy(buffer,"1");
				else
					strcpy(buffer,"0");
			
				fwrite(buffer, sizeof(char), sizeof(buffer) - 1, fp);			
				//printf("buzzer_run ste out value: %c\n", buffer[0]);
				fclose(fp);
			}
			
}


int sensor_gpio_init()
{
	
	int value, i;

  FILE *fp_cs,*fp_sid,*fp_sclk;
//  char filename[50];
		// equivalent shell command "echo 32 > export" to export the port 
		if ((fp_cs = fopen("/sys/class/gpio/export", "w")) == NULL) {
			printf("Cannot open export file.\n");
			return -1;
		}
		fprintf(fp_cs, "%d", 168);
		fclose(fp_cs);
		
	
		// equivalent shell command "echo out > direction" to set the port as an input  
		if ((fp_cs = fopen("/sys/class/gpio/gpio168/direction", "rb+")) == NULL) {
			printf("Cannot open direction file.\n");
		  return -1;
		}
		fprintf(fp_cs, "out");
		fclose(fp_cs);
			//usleep(500000);		
		
		
  	return 0;
}


int sensor_run(int toggle)
{

FILE *fp;
char buffer[10];

	memset(buffer, 0, sizeof(buffer));
	if ((fp = fopen("/sys/class/gpio/gpio168/value", "rb+")) == NULL) {
				printf("sensor_run Cannot open value file.\n");
				//exit(1);
			} else {
			
				if(toggle)
					strcpy(buffer,"1");
				else
					strcpy(buffer,"0");
			
				fwrite(buffer, sizeof(char), sizeof(buffer) - 1, fp);			
				printf("ste out value: %c\n", buffer[0]);
				fclose(fp);
			}


}


int  lock_gpio_init()
{
	
	int value, i;

  FILE *fp_cs,*fp_sid,*fp_sclk;
//  char filename[50];
		// equivalent shell command "echo 32 > export" to export the port 
		if ((fp_cs = fopen("/sys/class/gpio/export", "w")) == NULL) {
			printf("Cannot open export file.\n");
			return -1;
		}
		fprintf(fp_cs, "%d", 13);
		fclose(fp_cs);
		
	
		// equivalent shell command "echo out > direction" to set the port as an input  
		if ((fp_cs = fopen("/sys/class/gpio/gpio13/direction", "rb+")) == NULL) {
			printf("Cannot open direction file.\n");
		  return -1;
		}
		fprintf(fp_cs, "out");
		fclose(fp_cs);
			//usleep(500000);		
		
		
  	return 0;
}


int  lock_run(int toggle)
{

FILE *fp;
char buffer[10];

	memset(buffer, 0, sizeof(buffer));
	if ((fp = fopen("/sys/class/gpio/gpio13/value", "rb+")) == NULL) {
				printf("lock_run Cannot open value file.\n");
				//exit(1);
			} else {
			
				if(toggle)
					strcpy(buffer,"1");
				else
					strcpy(buffer,"0");
			
				fwrite(buffer, sizeof(char), sizeof(buffer) - 1, fp);			
				printf("lock_run ste out value: %c\n", buffer[0]);
				fclose(fp);
			}


}

int read_gpio_config()
{

	FILE* fpr_gpio;

	if((fpr_gpio=fopen("/mnt/nand1-1/config/lock_gpio","r"))==NULL)
	{
	printf("can not open the file.\n");
	return -1;
	}
	
	fscanf(fpr_gpio,"%d",&lock_flag);
	fclose(fpr_gpio);
	
    if(lock_flag==0)//diankong
    	{
			lock_run(0);
    	}
	else if(lock_flag==1)//cilisuo
		{
			lock_run(1);
		}
	else
		{
		printf("read lock config error\n");

		}


}


void lock_open_fun()
{
	if(lock_flag==0)
		{
			lock_run(1);
			sleep(6);
			lock_run(0);
		}
	else if(lock_flag==1)
		{
		lock_run(0);
		sleep(6);
		lock_run(1);
		}



}

void lock_open_precess()
{
	pthread_t lock_snj;


	if (pthread_create(&lock_snj,NULL,lock_open_fun,NULL)!=0) {
				   printf("lock_open_precess Create thread error!\n");
				   
				   return ;
			   }
	pthread_detach(lock_snj);

}


void send_command(uchar command_data)  
{   uchar i;
    uchar i_data,
    temp_data1,
    temp_data2;  
    i_data=0xf8;  
    //delay_1ms(10);  
   // usleep(10000);
   // CS=1;  
    gpio_set(cs,1);
    //SCLK=0;   
    gpio_set(sclk,0);
    
    for(i=0;i<8;i++)  
    {   
    	//SID=(bit)(i_data&0x80);  
    	 gpio_set(sid,(i_data&0x80));
    	//SCLK=0;  
    	//SCLK=1; 
    	gpio_set(sclk,0);
    	gpio_set(sclk,1); 
    	
    	i_data=i_data<<1;  
    	}   
    	
    	i_data=command_data;  
    	i_data&=0xf0;  
    	for(i=0;i<8;i++)  
    	{   
    		//SID=(bit)(i_data&0x80);  
    		 gpio_set(sid,(i_data&0x80));
    		//SCLK=0;  
    		//SCLK=1; 
    		gpio_set(sclk,0);
    	  gpio_set(sclk,1);   
    		i_data=i_data<<1;  
    		}   
    		
    		i_data=command_data;  
    		temp_data1=i_data&0xf0;  
    		temp_data2=i_data&0x0f;  
    		temp_data1>>=4;  
    		temp_data2<<=4;   
    		i_data=temp_data1|temp_data2;  
    		i_data&=0xf0;  
    		for(i=0;i<8;i++)  
    		{   
    			//SID=(bit)(i_data&0x80);  
    			 gpio_set(sid,(i_data&0x80));
    			//SCLK=0;  
    			//SCLK=1;
    			gpio_set(sclk,0);
    	    gpio_set(sclk,1);    
    			i_data=i_data<<1;  
    			}   
    		//CS=0;  
    		gpio_set(cs,0);
}

void send_data(uchar command_data)  
{   
	uchar i;   
	uchar i_data,temp_data1,temp_data2;  
	i_data=0xfa;  
	//delay_1ms(10);
	//usleep(10000);  
	//CS=1;   
	gpio_set(cs,1);
	
	for(i=0;i<8;i++)  
	{   
		//SID=(bit)(i_data&0x80);
		gpio_set(sid,(i_data&0x80));   
		//SCLK=0;  
		//SCLK=1;
		gpio_set(sclk,0);
    gpio_set(sclk,1);   
		i_data=i_data<<1;  
		}   
		i_data=command_data;  
		i_data&=0xf0;  
		for(i=0;i<8;i++)  
		{   
			//SID=(bit)(i_data&0x80);
			 gpio_set(sid,(i_data&0x80));  
			//SCLK=0;  
			//SCLK=1; 
			gpio_set(sclk,0);
    	gpio_set(sclk,1);  
			i_data=i_data<<1;  
		}   
		i_data=command_data;  
		temp_data1=i_data&0xf0;  
		temp_data2=i_data&0x0f;  
		temp_data1>>=4;  
		temp_data2<<=4;   
		i_data=temp_data1|temp_data2;  
		i_data&=0xf0;  
		for(i=0;i<8;i++)  
		{   
			//SID=(bit)(i_data&0x80); 
			gpio_set(sid,(i_data&0x80));   
			//SCLK=0;  
			//SCLK=1;
			gpio_set(sclk,0);
    	gpio_set(sclk,1);   
			i_data=i_data<<1;  
			}   
			//CS=0;
			gpio_set(cs,0);  
	} 
int close_audio_play_device()
{
 
  close(p_dsp);
  close(p_mixer);
	
  return 0;	
}

int open_audio_play_device()
{
printf("*****\n");
	p_dsp = open("/dev/dsp", O_RDWR);
	if( p_dsp < 0 ){
		printf("Open dsp error\n");
		return -1;
	}
printf("====\n");	
	p_mixer = open("/dev/mixer", O_RDWR);
	if( p_mixer < 0 ){
		printf("Open mixer error\n");
		return -1;
	}
}


void load_sound_init()  
{   
	int data, oss_format, channels, sample_rate;	

	
		int volume=100;
		int buf;			
	
				buf = volume;
				volume <<= 8;
				volume |= buf;	
		//printf("<=== %s ===>\n", file);
		open_audio_play_device();
		
	
		
	//	data = 0x5050;
		data = 0x5A5A;	
		oss_format=AFMT_S16_LE;
		sample_rate = 8000;
		channels = 2;
		ioctl(p_dsp, SNDCTL_DSP_SETFMT, &oss_format);
		ioctl(p_mixer, MIXER_WRITE(SOUND_MIXER_PCM), &data);
		ioctl(p_dsp, SNDCTL_DSP_SPEED, &sample_rate);
		ioctl(p_dsp, SNDCTL_DSP_CHANNELS, &channels);
	
		printf("the volume is replay %d\n",volume);
		ioctl(p_mixer , MIXER_WRITE(SOUND_MIXER_PCM), &volume); 
		ioctl(p_mixer , MIXER_WRITE(SOUND_MIXER_VOLUME), &volume);	
		ioctl(p_mixer , MIXER_WRITE(SOUND_MIXER_TREBLE), &volume);	
		ioctl(p_mixer , MIXER_WRITE(SOUND_MIXER_BASS), &volume);	
		close_audio_play_device();

}

void lcd_init()  
{   
	uchar command_data;  
	//delay_1ms(100); 
	 //usleep(100000);  
	command_data=0x30;   
	send_command(command_data); /*功能设置:一次送8位数据,基本指令集*/  
	command_data=0x04;   
	send_command(command_data); /*点设定:显示字符/光标从左到右移位,DDRAM地址加1*/ 
	command_data=0x0f;   
	send_command(command_data); /*显示设定:开显示,显示光标,当前显示位反白闪动*/   
	command_data=0x01;   
	send_command(command_data); /*清DDRAM*/  
	command_data=0x0c;   
	send_command(command_data);
	command_data=0x02;   
	send_command(command_data); /*DDRAM地址归位*/  
	command_data=0x80;   
	send_command(command_data); /*把显示地址设为0X80，即为第一行的首位*/  

	testdev = open("/dev/mycdev", O_RDWR); 

	if (-1 == testdev) {
		printf("lcd_init cannot open file./n");
		//exit(1);
	}
	
}
	
void lcd_clean()  
{   
	uchar command_data;  
	//delay_1ms(100); 
	 //usleep(100000);  

	command_data=0x01;   
	send_command(command_data); /*清DDRAM*/  
	command_data=0x0c;   
	send_command(command_data);
	command_data=0x02;   
	send_command(command_data); /*DDRAM地址归位*/  
	command_data=0x80;   
	send_command(command_data); /*把显示地址设为0X80，即为第一行的首位*/  
	
	}



	
	void disp_hanzi(uchar addr,uchar *str)
{
	int  ret;

	if (ret = write(testdev,str, strlen(str)) < 0) {
	   //关于15，具体数值要看在mycdev.c中定义了多长的字符串
	printf("disp_hanzi write error!\n");
	return ;
	}
/*	send_command(addr);
	while(*str)
  { 
   send_data(*str++); 
  } 
	*/
}
   

int SetIfAddr(char *ifname, char *Ipaddr, char *mask,char *gateway)  
{  
    int fd;  
    int rc;  
    struct ifreq ifr;   
    struct sockaddr_in *sin;  
    struct rtentry  rt;  
  
    fd = socket(AF_INET, SOCK_DGRAM, 0);  
    if(fd < 0)  
    {  
            perror("socket   error");       
            return -1;       
    }  
    memset(&ifr,0,sizeof(ifr));   
    strcpy(ifr.ifr_name,ifname);   
    sin = (struct sockaddr_in*)&ifr.ifr_addr;       
    sin->sin_family = AF_INET;       
    //IP地址  
    if(inet_aton(Ipaddr,&(sin->sin_addr)) < 0)     
    {       
        perror("inet_aton   error");       
        return -2;       
    }      
  
    if(ioctl(fd,SIOCSIFADDR,&ifr) < 0)     
    {       
        perror("ioctl   SIOCSIFADDR   error");       
        return -3;       
    }  
    //子网掩码  
    if(inet_aton(mask,&(sin->sin_addr)) < 0)     
    {       
        perror("inet_pton   error");       
        return -4;       
    }      
    if(ioctl(fd, SIOCSIFNETMASK, &ifr) < 0)  
    {  
        perror("ioctl");  
        return -5;  
    }  
    //网关  
    /*
    memset(&rt, 0, sizeof(struct rtentry));  
    memset(sin, 0, sizeof(struct sockaddr_in));  
    sin->sin_family = AF_INET;  
    sin->sin_port = 0;  
    if(inet_aton(gateway, &sin->sin_addr)<0)  
    {  
       printf ( "inet_aton error\n" );  
    }  
    memcpy ( &rt.rt_gateway, sin, sizeof(struct sockaddr_in));  
    ((struct sockaddr_in *)&rt.rt_dst)->sin_family=AF_INET;  
    ((struct sockaddr_in *)&rt.rt_genmask)->sin_family=AF_INET;  
    rt.rt_flags = RTF_GATEWAY;  
    if (ioctl(fd, SIOCADDRT, &rt)<0)  
    {  
        //zError( "ioctl(SIOCADDRT) error in set_default_route\n");  
        close(fd);  
        return -1;  
    }  */
    close(fd);  
    return rc;  
}


//init uart
	int thrd_func(void *arg);
	pthread_t tid;
	/*********************************************************************/
	


	int speed_arr[] = {B38400, B19200, B9600, B4800, B2400, B1200, B300,
					   B38400, B19200, B9600, B4800, B2400, B1200, B300, };
	int name_arr[] = {38400,  19200,  9600,  4800,	2400,  1200,  300, 38400,  
						19200,	9600, 4800, 2400, 1200,  300, };
	void set_speed(int fd, int speed)
	{
		int   i; 
		int   status; 
		struct termios	 Opt;
		tcgetattr(fd, &Opt); 
		for ( i= 0;  i < sizeof(speed_arr) / sizeof(int);  i++) { 
			if	(speed == name_arr[i]) {	 
				tcflush(fd, TCIOFLUSH); 	
				cfsetispeed(&Opt, speed_arr[i]);  
				cfsetospeed(&Opt, speed_arr[i]);   
				status = tcsetattr(fd, TCSANOW, &Opt);	
				if	(status != 0) { 	   
					perror("tcsetattr fd1");  
					return; 	
				}	 
				tcflush(fd,TCIOFLUSH);	 
			}  
		}
	}

	int set_Parity(int fd,int databits,int stopbits,int parity)
	{ 
		struct termios options; 
	//	  options.c_lflag  &= ~(ICANON | ECHO | ECHOE | ISIG);	/*Input*/
	  //  options.c_oflag  &= ~OPOST;	/*Output*/
		if	( tcgetattr( fd,&options)  !=  0) { 
			perror("SetupSerial 1");	 
			return(FALSE);	
		}
		//options.c_cflag &= ~CSIZE; 
		options.c_cflag 	|= (CLOCAL | CREAD);
		 options.c_lflag  &= ~(ICANON | ECHO | ECHOE | ISIG);  /*Input*/
		switch (databits) /*设置数据位数*/
		{	
		case 7: 	
			options.c_cflag |= CS7; 
			break;
		case 8: 	
			options.c_cflag |= CS8;
			break;	 
		default:	
			fprintf(stderr,"Unsupported data size/n"); return (FALSE);	
		}
		switch (parity) 
		{	
			case 'n':
			case 'N':	 
				options.c_cflag &= ~PARENB;   /* Clear parity enable */
				options.c_iflag &= ~INPCK;	   /* Enable parity checking */ 
				break;	
			case 'o':	
			case 'O':	  
				options.c_cflag |= (PARODD | PARENB); /* 设置为奇效验*/  
				options.c_iflag |= INPCK;			  /* Disnable parity checking */ 
				break;	
			case 'e':  
			case 'E':	
				options.c_cflag |= PARENB;	   /* Enable parity */	  
				options.c_cflag &= ~PARODD;   /* 转换为偶效验*/ 	
				options.c_iflag |= INPCK;		/* Disnable parity checking */
				break;
			case 'S': 
			case 's':  /*as no parity*/   
				options.c_cflag &= ~PARENB;
				options.c_cflag &= ~CSTOPB;break;  
			default:   
				fprintf(stderr,"Unsupported parity/n");    
				return (FALSE);  
		}  
		/* 设置停止位*/  
		switch (stopbits)
		{	
			case 1:    
				options.c_cflag &= ~CSTOPB;  
				break;	
			case 2:    
				options.c_cflag |= CSTOPB;	
			   break;
			default:	
				 fprintf(stderr,"Unsupported stop bits/n");  
				 return (FALSE); 
		} 
		/* Set input parity option */ 
		if (parity != 'n')	 
			options.c_iflag |= INPCK; 
			/*
		tcflush(fd,TCIFLUSH);
		options.c_cc[VTIME] = 10; 
		options.c_cc[VMIN] = 0; */
		if (tcsetattr(fd,TCSANOW,&options) != 0)   
		{ 
			perror("SetupSerial 3");   
			return (FALSE);  
		} 
		return (TRUE);	
	}







void *func_pipe(void * fd)
{
	char readbuf[1];
	printf("func_pipe\n");
	char buff_pipe[10];
	char buf_call[30];
	int i=0;
	int ret;
			
			while(1)
			{
				memset(buff_pipe,0,sizeof(buff_pipe));
			read( *(int*)fd, buff_pipe,sizeof(buff_pipe) );
			printf("readbuf is %x\n",buff_pipe[3]);
			switch(buff_pipe[3])
        			{
        				case  0x09 ://alarm
        				break;
        				case  0x10 ://answer
   
				   timekeeping_start=1;
				   times_set=60;

				  
				   MIC_run(1);
				  // light_run(1);
				   printf("MIC_run(1)!!!!\n");

				   if(call_ins_flag==1)
				   	{
				   	sleep(1);
					ortp_mutex_lock(&prompt_mutex);
										
					strcpy(received_prompt,"answer");
					//strcpy(received_prompt,"status register\0");
					//	 printf("Receiving command '%s'\n",received_prompt);kfflush(stdout);
					have_prompt=TRUE;
					ortp_mutex_unlock(&prompt_mutex);
					sleep(1);
				   	}
				   else
				   	{
				   	/*
					   lcd_clean() ;
					  disp_hanzi(0x80,"    通话中    ");		   
					   fist_line=1;
					*/
				   	}

					  if(call_ins_flag!=1)
						{
						ortp_mutex_lock(&prompt_mutex);
																
						sprintf(received_prompt,"chat sip:192.168.7.201 men_to_sn %c,%c",buff_pipe[1],buff_pipe[2]);
						//strcpy(received_prompt,"status register\0");
						//	 printf("Receiving command '%s'\n",received_prompt);kfflush(stdout);
						have_prompt=TRUE;
						ortp_mutex_unlock(&prompt_mutex);
						}

					
				 //  record_logs_fun();
        				break;
        				case  0x11 ://open door

				  if(call_ins_flag!=1)
					{
        				lock_open_precess();
						ortp_mutex_lock(&prompt_mutex);
										
						//strcpy(received_prompt,"chat sip:192.168.7.201 ins_call_command %c,%c",buff[1],buff[2]);
						sprintf(received_prompt,"chat sip:192.168.7.201 ins_open_door %c,%c",buff_pipe[1],buff_pipe[2]);
						//strcpy(received_prompt,"status register\0");
						//	 printf("Receiving command '%s'\n",received_prompt);kfflush(stdout);
						have_prompt=TRUE;
						ortp_mutex_unlock(&prompt_mutex);
				  	}
				  else
				  	{
					  ortp_mutex_lock(&prompt_mutex);
															  
					  //strcpy(received_prompt,"chat sip:192.168.7.201 ins_call_command %c,%c",buff[1],buff[2]);
					  sprintf(received_prompt,"chat sip:192.168.7.21%c wqj_open_door",wqj_num);
					  //strcpy(received_prompt,"status register\0");
					  //   printf("Receiving command '%s'\n",received_prompt);kfflush(stdout);
					  have_prompt=TRUE;
					  ortp_mutex_unlock(&prompt_mutex);
					  usleep(50000);
					  ortp_mutex_lock(&prompt_mutex);
															  
					  //strcpy(received_prompt,"chat sip:192.168.7.201 ins_call_command %c,%c",buff[1],buff[2]);
					  sprintf(received_prompt,"chat sip:192.168.7.201 ins_open_door_wqj %c,%c,%c",buff_pipe[1],buff_pipe[2],wqj_num);
					  //strcpy(received_prompt,"status register\0");
					  //   printf("Receiving command '%s'\n",received_prompt);kfflush(stdout);
					  have_prompt=TRUE;
					  ortp_mutex_unlock(&prompt_mutex);


					  

				  	}
						
        				break;
        				case  0x12 ://hang

						//if( call_occupy_flag==1)
						//	{
        				// swich_gpio_set(0,0,1,1);
						// printf(" swich_gpio_set\n");
        				if(call_ins_flag==1)
					   	{

								
								//usleep(50000);
								/*
								printf(" wirte  bb\n");
							  
								  fputc(buff_4[0],output);
								 
								  usleep(50000);
								  fputc(buff_4[1],output);
								  usleep(50000);
								*/
								ortp_mutex_lock(&prompt_mutex);
											
								strcpy(received_prompt,"terminate all");
								//strcpy(received_prompt,"status register\0");
								//	 printf("Receiving command '%s'\n",received_prompt);kfflush(stdout);
								have_prompt=TRUE;
								ortp_mutex_unlock(&prompt_mutex);
								
								
								

					   	}
        			else if(ins_to_flag == 0)
        					{
        						
							  call_state=0;
							  timekeeping_start=0;
							  
							  //call_occupy_flag=0;
							  swich_gpio_set(0,0,1,1);	
        					}
						else if(ins_to_flag == 1)
							{
							ins_to_flag=0;
							
							ortp_mutex_lock(&prompt_mutex);
										
							strcpy(received_prompt,"terminate all");
			
							have_prompt=TRUE;
							ortp_mutex_unlock(&prompt_mutex);
							//sleep(2);
					
							}
						
						if(times_set>0)
						{
						times_no_do=1;
					    times_set=0;
						}
					
						
						  state_monitor_flag=0;

						//	}
						  
        				break;
        				case  0x13 ://monitor
        				break;
        				case  0x14 ://call
        					if(call_occupy_flag==0 && kpd_state==0)
							{

							printf("!!!!!!!!!call GLJ!!!!!!!!!!!\n");
							call_occupy_flag=1;
							// swich_gpio_set(1,0,1,0);
							 pthread_mutex_lock(&Device_mutex);
							  lcd_clean() ;
							  disp_hanzi(0x80,"占用中请稍候");	
							  fist_line=1;
							  pthread_mutex_unlock(&Device_mutex);
									
							 memset(buf_call,0,sizeof(buf_call));	
							for(i=1;i<=glj_num;i++)
							{
							sprintf(buf_call,"192.168.7.20%d",i);
							printf("buf_call is %s\n",buf_call);
							ret=call_glj_check(buf_call);
							if(ret==0)
								{	
									break;
								}
							}

							if(i==glj_num+1)
								{
									
									pthread_mutex_lock(&Device_mutex);
									lcd_clean() ;
									disp_hanzi(0x80,"占线中请稍候");	
									fist_line=1;
									pthread_mutex_unlock(&Device_mutex);
										buzzer_run(1);
										sleep(1);
										buzzer_run(0);
										sleep(1);

										buzzer_run(1);
										sleep(1);
										buzzer_run(0);
									reset_lcd();

									printf(" wirte  bb\n");

									fputc(buff_4[0],output);

									usleep(50000);
									fputc(buff_4[1],output);
									usleep(50000);
									
									call_occupy_flag=0;

									
								//	swich_gpio_set(0,0,1,1);
									break;
								}
							
							

						   timekeeping_start=1;
						   ins_to_flag=1;
        				   swich_gpio_set(0,1,1,1);
						   
						  


						
							memset(buf_call,0,sizeof(buf_call));	
							sprintf(buf_call,"chat sip:192.168.7.20%d ins_call_command %c,%c",i,buff_pipe[1],buff_pipe[2]);
							printf("buf_call is %s\n",buf_call);
														
						 	ortp_mutex_lock(&prompt_mutex);
										
							//strcpy(received_prompt,"chat sip:192.168.7.201 ins_call_command %c,%c",buff[1],buff[2]);
							//sprintf(received_prompt,"chat sip:192.168.7.201 ins_call_command %c,%c",buff_pipe[1],buff_pipe[2]);
							strcpy(received_prompt,buf_call);
							//	 printf("Receiving command '%s'\n",received_prompt);kfflush(stdout);
							have_prompt=TRUE;
							ortp_mutex_unlock(&prompt_mutex);
							
							usleep(200000);
							memset(buf_call,0,sizeof(buf_call));	
							sprintf(buf_call,"call sip:192.168.7.20%d",i);
							printf("buf_call is %s\n",buf_call);
							
							ortp_mutex_lock(&prompt_mutex);
										
							strcpy(received_prompt,buf_call);
							//strcpy(received_prompt,"status register\0");
							//	 printf("Receiving command '%s'\n",received_prompt);kfflush(stdout);
							have_prompt=TRUE;
							usleep(500000);
							ortp_mutex_unlock(&prompt_mutex);
							
							
							

							  memset(data_num,0,sizeof(data_num));
							 
							  data_pint=0;
							  time_keeping_fun(60);
        						}
							
        				break;
						case  0x15 ://monitor
						if(call_occupy_flag==0 && kpd_state==0 )
							{
			                call_occupy_flag=1;
							memkou_call_sn=1;
							  timekeeping_start=1;
							  swich_gpio_set(0,0,1,0);
							  time_keeping_fun(15);
							
							  state_monitor_flag=1;
							  memset(data_num,0,sizeof(data_num));
							  pthread_mutex_lock(&Device_mutex);
							  lcd_clean() ;
							  disp_hanzi(0x80,"占用中请稍候");	
							  fist_line=1;
							  pthread_mutex_unlock(&Device_mutex);
							  data_pint=0;

							   light_run(1);
							  
							}
        				break;
						case  0xee ://check
						

        				break;
						case  0xb9 ://check

        				break;

						default:
        					
        					break;
        				
			}
		}
	
}

int thrd_func(void *arg)
{
	char ch=0;
	char pepe_send[1];
	int sa_flag=0;
	int fd_pipe[2];
	 pthread_t tid_pipe = 0;
	 char buff[10];
	//int get_flag=0;
	printf("thrd_func!\n");
	if(pipe(fd_pipe) < 0)
        {
                printf("pipe error!\n");
        }

	       
        pthread_create(&tid_pipe, NULL, func_pipe, &fd_pipe[0]);

		sleep(1);
	 while(1)
{
	       
        ch = fgetc(input);

	   if(ch ==0x93)
	   	{
			ch=0x13;
	   	}
	   else if(ch ==0x91)
	   	{
		   ch=0x11;

	   	}
	   
        printf("ch is %x\n",ch);
if(vice_used==0)
{
        if(sa_flag)
        	{
        		buff[sa_flag]=ch;
        		sa_flag++;

        	}
        else if(ch == 0xba)
        	{
				/*
				if(ch==0xb9)
				{
				usleep(50000);
				printf(" wirte  bb\n");

				fputc(buff_4[0],output);
				usleep(50000);
				}*/
        		buff[sa_flag]=ch;
        		sa_flag++;
        	}
		   else if(ch == 0xbb)
        	{
        		if(rec_bb_flag==1)
        			{
						rec_bb_flag=2;
					}
		   	}
		   else if(ch == 0xac)
        	{
        		vice_used=1;
				pthread_mutex_lock(&Device_mutex);
				lcd_clean();

				disp_hanzi(0x80,"单元通话占用");
				fist_line=1;
				pthread_mutex_unlock(&Device_mutex);
				//time_keeping_fun(60);
				time_keeping_uart(60);
		   	}
		   else if(ch == 0xad)
        	{
        		vice_used=1;
				pthread_mutex_lock(&Device_mutex);
				lcd_clean();

				disp_hanzi(0x80,"单元通话占用");
				fist_line=1;
				pthread_mutex_unlock(&Device_mutex);
				//time_keeping_fun(60);
				time_keeping_uart(60);

		   	}
		   else if(ch == 0xaf)
        	{
        		if(af_no_self==0)
        			{
        				vice_used=1;
						pthread_mutex_lock(&Device_mutex);
						lcd_clean();

						disp_hanzi(0x80,"单元通话占用");
						fist_line=1;
						pthread_mutex_unlock(&Device_mutex);
						//time_keeping_fun(60);
						time_keeping_uart(60);
        			}
				else
					{
					af_no_self=0;
					}
		   	}
        	
        	if(sa_flag >=5)
        		{
        			printf("buff[3] is %x\n",buff[3]);
					//pepe_send=buff[3];
        			switch(buff[3])
        			{
        				case  0x09 ://alarm
        				break;
        				case  0x10 ://answer
						usleep(50000);
						printf(" wirte  bb\n");

						fputc(buff_4[0],output);
						usleep(50000);

				 	   	write( fd_pipe[1], buff, sizeof(buff) );
						
					  
        				break;
        				case  0x11 ://open door
        					usleep(50000);
                  printf(" wirte  bb\n");
              
                  fputc(buff_4[0],output);
				  
        				write( fd_pipe[1], buff, sizeof(buff) );

						
        				break;
        				case  0x12 ://hang
        				
        				usleep(50000);
						printf(" wirte  bb\n");

						fputc(buff_4[0],output);
						usleep(50000);
						fputc(buff_4[1],output);
						
        				if( tonghua_bug_flag==0 )
        					{
        				
							write( fd_pipe[1], buff, sizeof(buff) );

        					}
						else
							{
							tonghua_bug_flag=2;

							}
						  
        				break;
        				case  0x13 ://monitor
        				break;
        				case  0x14 ://call
        					if(call_occupy_flag==0 && kpd_state==0)
							{
								printf(" wirte  bb\n");

								fputc(buff_4[0],output);
									write( fd_pipe[1], buff, sizeof(buff) );
								//sleep(50000);
						  
        					}
							
        				break;
						case  0x15 ://monitor
						if(call_occupy_flag==0 && kpd_state==0 )
							{
							  printf(" wirte  bb\n");
			              
			                  fputc(buff_4[0],output);
			                //  usleep(50000);
			                		write( fd_pipe[1], buff, sizeof(buff) );
							  
							  
							}
        				break;
						case  0xee ://check
						
						    fputc(buff_4[0],output);
							lcd_clean() ;
							//send_command(0x90);
							
							send_data(buff[1]/16+'0');
							send_data(buff[1]%16+'0');
							send_data(buff[2]/16+'0');
							send_data(buff[2]%16+'0');
        				break;
						case  0xb9 ://check
							usleep(50000);
							usleep(50000);
							//usleep(50000);
							printf(" wirte  bb\n");
              
			                  fputc(buff_4[0],output);
			                  usleep(50000);
        				break;
        				
        				
        				default:
        					
        					break;
        			}
        			sa_flag=0;
        			
        		}
	 	}
	else
		{
			
			if(ch == 0xcc)
				{
				times_uart=0;
				}
			
		}
	
}
	  pthread_join(func_pipe, NULL);
}


int uart_init()
{

    int fd;
    int nread;
    //int j=0;
    int static time=1;
   
    
   // char buff_5[2]={0xCC,0xFF};

   // char buff_w[bit]={63,01,01,01};
    
    char *dev  = "/dev/ttyS0"; //串口二
   
    char selected=0;


      input = fopen("/dev/ttyS1", "r");
      output = fopen("/dev/ttyS1", "w");
	    setvbuf(input, NULL, _IONBF, 0);
	    setvbuf(output, NULL, _IONBF, 0);
        if(!input || !output) {
        fprintf(stderr, "Unable to open /dev/tty\n");
       // exit(1);
    }
    set_speed(fileno(input),2400);
   //set_speed(fileno(input),1200);
   if (set_Parity(fileno(input),8,1,'S') == FALSE)  
    {
        printf("Set Parity Error/n");
        exit (0);
    }

  

    
/*

						for(j=0;j<5;j++)
						{
                  printf("start wirte\n");
                  fputc(buff_1[j],output);
            }
   						

   		sleep(1);



						for(j=0;j<5;j++)
						{
                  printf("start wirte\n");
                  fputc(buff_2[j],output);
                // fputc(buff_3[j],output);
            }
            
            
   */
 if (pthread_create(&tid,NULL,thrd_func,NULL)!=0) {
		        printf("Create thread error!\n");
		       // exit(1);
		    }
		    

return 0;
}


void reset_lcd()
{
pthread_mutex_lock(&Device_mutex);
	kpd_state=0;
	
	lcd_clean();
	memset(data_num,0,sizeof(data_num));
	data_pint=0;
	fist_line=1;
	disp_hanzi(0x80,"请输入门牌号码");	
	

	send_command(0x90);

	fist_line=0;
	send_data(' ');
	send_data(' ');
	send_data(' ');
	send_data(' ');
	send_data(' ');

	send_data('-');
	send_data('-');
	send_data('-');
	send_data('-');
	send_command(0x90);
    send_data(' ');
	send_data(' ');
	send_data(' ');
	send_data(' ');
	send_data(' ');
	pthread_mutex_unlock(&Device_mutex);
	//sleep(1);

}


void time_func()
{
 while(1)
 	{
		sleep(1);
		times_set--;
		 printf(" times_set %d bb\n",times_set);
		if(times_set<=0)
			{
			
			if(times_no_do==0)
				{
				if(vice_used==0)
					{
					usleep(50000);
				 
			  	  printf("time_func bb cc\n");
				  fputc(buff_4[0],output);
				 
				  usleep(50000);
				  fputc(buff_4[1],output);
				  usleep(50000);
					}

				ortp_mutex_lock(&prompt_mutex);
							
				strcpy(received_prompt,"terminate all");
				//strcpy(received_prompt,"status register\0");
				//	 printf("Receiving command '%s'\n",received_prompt);kfflush(stdout);
				have_prompt=TRUE;
				ortp_mutex_unlock(&prompt_mutex);
				swich_gpio_set(0,0,1,1);	
				}
			else
				{
				times_no_do=0;

				}
			if(vice_used==0)
					{
			reset_lcd();
				}
			else
				{
				pthread_mutex_lock(&Device_mutex);
				lcd_clean();

				disp_hanzi(0x80,"单元通话占用");
				fist_line=1;
				pthread_mutex_unlock(&Device_mutex);
				}
			light_run(0);
			MIC_run(1);		
				
			//sleep(2); ???
			call_state=0;
			//ins_to_flag=0;
			if(memkou_call_sn==1)
				{
				call_occupy_flag=0;
				}
			memkou_call_sn=0;
			state_monitor_flag=0;
		  
		break;

		
			}

		
 	}

}

void time_keeping_fun(int time)
{
	pthread_t pid_t;
	times_set=time;
//	func_set =func;

	if (pthread_create(&pid_t,NULL,time_func,NULL)!=0) {
				   printf("time_keeping_fun Create thread error!\n");
				   //exit(1);
			   }
	pthread_detach(pid_t);

}

void time_uart()
{
 while(1)
 	{
		sleep(1);
		times_uart--;
		 printf(" times_uart %d bb\n",times_uart);
		if(times_uart<=0)
			{
			//vice_used=0;
			reset_lcd();
			vice_used=0;

				break;
			}
		  


		
			}

		
 

}

void time_keeping_uart(int time)
{
	pthread_t pid_uart;
	times_uart=time;
//	func_set =func;

	if (pthread_create(&pid_uart,NULL,time_uart,NULL)!=0) {
				   printf("time_keeping_fun Create thread error!\n");
				   //exit(1);
			   }
	pthread_detach(pid_uart);

}

int call_glj_check(char *ip_check)
{

	int len;

	int sock_chk_fd;
	int err,n;
	int i=0;
	int arg =1;
	int ret=0;
	
	struct sockaddr_in addr_check;
	char smsg[30];
	
	memset (&smsg, 0, sizeof (smsg));
	printf("ip_check is %s \n",ip_check);

	sock_chk_fd=socket(AF_INET,SOCK_STREAM,0);
	   if(sock_chk_fd==-1)
	   {
		   printf("socket error\n");
		   return -1;
	   }
	   
	   bzero(&addr_check,sizeof(addr_check));
	   addr_check.sin_family=AF_INET;
	 //  addr_check.sin_addr.s_addr=inet_addr(ip_check);
	   addr_check.sin_port=htons(PORT);
	   
	   	if (inet_pton(AF_INET, ip_check, &addr_check.sin_addr) <= 0 ){
					printf("inet_pton error!\n");
					return(0);
			};

	  ret = ioctl(sock_chk_fd,FIONBIO,(int)&arg);
	for(i=0;i<10;)
		{
		   err=connect(sock_chk_fd,(struct sockaddr *)&addr_check,sizeof(addr_check));
		   
		   if(err!=0)
		   {
			   printf("connect error\n");
			   i++;
			   if(i>=10)
			   	{
				 return -1;
			   	}
			 //  sleep(1);
			  usleep(500000);
		   }

		   else
		   	{
				break;
		   	}
		}
	   printf("connect with server...\n");
	   
	


		sprintf(smsg,"call_glj_check_send");

		printf("%s\n", smsg);    

		send(sock_chk_fd,smsg,strlen(smsg),0);

		memset (&smsg, 0, sizeof (smsg));

			for(i=0;i<10;)
		{
		   err=recv(sock_chk_fd,smsg,sizeof(smsg),0);
		   
		   if(err<0)
		   {
			   printf("recv error\n");
			   i++;
			   if(i>=10)
			   	{
				 return -1;
			   	}
			 //  sleep(1);
			  usleep(500000);
		   }

		   else
		   	{
				break;
		   	}
		}
		
		printf("smsg is %s...\n",smsg);

		if(smsg[0]=='0')
			{
			close(sock_chk_fd);
			return 0;

			}
		else
			{

			close(sock_chk_fd);
			return -1;
		}
		
		   
}

	





int heart_search_rec()
{
//	setvbuf(stdout, NULL, _IONBF, 0); 
//	fflush(stdout); 


  int sockt_client;
  int len;

  int sockfd;
	int err,n;
	int i=0;
	struct sockaddr_in addr_ser;
	char sendline[20];
  char smsg[300];
   char ipadderss[50];
  memset (&smsg, 0, sizeof (smsg));
  char * pHeadPtr;
   struct ifreq  ifreq1;  
   struct sockaddr_in* psockaddr_in = NULL;  
   char ip[30];
	// 绑定地址
	struct sockaddr_in addrto;
	bzero(&addrto, sizeof(struct sockaddr_in));
	addrto.sin_family = AF_INET;
	addrto.sin_addr.s_addr = htonl(INADDR_ANY);
	addrto.sin_port = htons(6001);
	
	// 广播地址
	struct sockaddr_in from;
	bzero(&from, sizeof(struct sockaddr_in));
	from.sin_family = AF_INET;
	from.sin_addr.s_addr = htonl(INADDR_ANY);
	from.sin_port = htons(6001);
	
	int sock = -1;
	int sock_ip = -1;
	if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) == -1) 
	{   
		printf("socket error\n");
		//cout<<"socket error"<<endl;	
		return 0;
	}   

	const int opt = 1;
	//设置该套接字为广播类型，
	int nb = 0;
	nb = setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (char *)&opt, sizeof(opt));
	if(nb == -1)
	{
		printf("set socket error...\n");
		//cout<<"set socket error..."<<endl;
		return 0;
	}

	if(bind(sock,(struct sockaddr *)&(addrto), sizeof(struct sockaddr_in)) == -1) 
	{ 
		printf("bind error...\n");  
		//cout<<"bind error..."<<endl;
		return 0;
	}

	len = sizeof(struct sockaddr_in);
	

	while(1)
	{
		//从广播地址接受消息
		memset(smsg,0,sizeof(smsg));
		int ret=recvfrom(sock, smsg, sizeof(smsg), 0, (struct sockaddr*)&from,(socklen_t*)&len);
		
		if(ret<=0)
		{
			printf("read error....\n");
			//cout<<"read error...."<<sock<<endl;
		}
		else
		{		
			
		printf("Get a new message:%s\n", smsg);
    printf("IP:%s\n", (char *)inet_ntoa(from.sin_addr));
    printf("Port:%d\n", ntohs(from.sin_port));   
	sscanf(smsg,"herat serarch check%d",&glj_num);
	printf("Get glj_num:%d\n", glj_num);
	if(glj_num>9 || glj_num<=0)
		{
		glj_num=1;

		}
    //printf("nSinSize = %d\n", nSinSize);
			/*
			printf("%s\n", smsg);	
		if((pHeadPtr = strstr(smsg, "<address>")) != NULL)
	{
		pHeadPtr += strlen("<address>");
		i=0;
		while(*pHeadPtr!='<' && *pHeadPtr!='\0')
		{
		ipadderss[i]=*pHeadPtr;
		i++;
		pHeadPtr++;
	  }
  }
  ipadderss[i]='\0';
  printf("ipadderss is %s\n",ipadderss);
		//sleep(1);
		*/
		
		
	
 sockfd=socket(AF_INET,SOCK_STREAM,0);
	if(sockfd==-1)
	{
		printf("socket error\n");
		return -1;
	}
	
	bzero(&addr_ser,sizeof(addr_ser));
	addr_ser.sin_family=AF_INET;
	addr_ser.sin_addr.s_addr=inet_addr((char *)inet_ntoa(from.sin_addr));
	addr_ser.sin_port=htons(PORT);
	
	
		
	err=connect(sockfd,(struct sockaddr *)&addr_ser,sizeof(addr_ser));
	if(err==-1)
	{
		printf("connect error\n");
		return -1;
	
	
	}
	printf("connect with server...\n");
	

		//printf("Input your words:");
		//scanf("%s",&sendline);
		/*
		 if ( (sock_ip = socket(PF_INET, SOCK_DGRAM, 0)) < 0)  
 {  
  perror("Unable to create socket for geting the mac address");  
 // exit(1);  
 }    
		 strcpy(ifreq1.ifr_name, "eth0");  
		if (ioctl(sock_ip, SIOCGIFADDR, &ifreq1) < 0)  
 {  
  perror("Unable to get the ip address");  
  //exit(1);   
 }
   else
   {
        psockaddr_in = (struct sockaddr_in*)&ifreq1.ifr_addr; 
       strcpy(ip, inet_ntoa(psockaddr_in->sin_addr));
        printf("ip %s\n",ip); 
       close(sock_ip);
   }  
  
      sprintf(smsg,"<?xml version=\"1.0\" encoding=\"utf-8\"?><boby><uuid>8b20a4be-a33d-4fb1-896d-64d1fe3aec60</uuid><address>%s</address></boby>",ip);
      
       printf("%s\n", smsg);	*/
 
		send(sockfd,smsg,strlen(smsg),0);
       	

		printf("waiting for server...\n");
		close(sockfd);
		

   }
  }

	return 0;
}


void heart_search_init()
{
	pthread_t pid_heart;

	printf("heart_search_init!!!\n");

	if (pthread_create(&pid_heart,NULL,heart_search_rec,NULL)!=0) {
				   printf("time_keeping_fun Create thread error!\n");
				   //exit(1);
			   }
	//pthread_detach(pid_uart);

}


void time_kapd()
{
int open_door_time=0;
int ret=0;
int door_state=0;
 door_check_gpio_init();
 opendoor_button_gpio_init();
 int reboot_times=0;

 while(1)
 	{
		sleep(1);
		// printf(" time_ckeck_kapd_num %d bb\n",time_ckeck_kapd_num);
		 //lcd check
		 if( time_ckeck_kapd_num>0)
		 	{
			time_ckeck_kapd_num--;
		 	}
		 else if(time_ckeck_kapd_num==0)
		 	{
				if(data_pint>0 || kpd_state!=0)
					{
					/*
					memset(data_num,0,sizeof(data_num));
					lcd_clean() ;
				  disp_hanzi(0x80,"请输入室号");	
				  fist_line=1;
				  data_pint=0;*/
				  reset_lcd();

					}
		 	}
		 else
		 	{
			 time_ckeck_kapd_num=0;

		 	}

		 ret=opendoor_button_gpio_read();
		  if(ret==0)
		 	{

				ortp_mutex_lock(&prompt_mutex);
								
				//strcpy(received_prompt,"chat sip:192.168.7.201 ins_call_command %c,%c",buff[1],buff[2]);
				sprintf(received_prompt,"chat sip:192.168.7.201 mkj_open_door ");
				//strcpy(received_prompt,"status register\0");
				//	 printf("Receiving command '%s'\n",received_prompt);kfflush(stdout);
				have_prompt=TRUE;
				ortp_mutex_unlock(&prompt_mutex);
				
			 	printf("open the door\n");
			lock_open_precess();

		 	}

         //open door state check
		 ret=door_check_gpio_read();
	//	 printf("door_check_gpio_read is %d\n",ret);
		 
		 if(ret==1)
		 	{
			 open_door_time++;
			// printf("open_door_time is %d\n",open_door_time);
			 if(open_door_time>=120) //120
			 	{
			 		
				ortp_mutex_lock(&prompt_mutex);
								
				//strcpy(received_prompt,"chat sip:192.168.7.201 ins_call_command %c,%c",buff[1],buff[2]);
				sprintf(received_prompt,"chat sip:192.168.7.201 door_alarm ");
				//strcpy(received_prompt,"status register\0");
				//	 printf("Receiving command '%s'\n",received_prompt);kfflush(stdout);
				have_prompt=TRUE;
				ortp_mutex_unlock(&prompt_mutex);
			 	open_door_time=0;
				door_state=1;
			 	}

		 	}
		 else
		 	{
		 		 if(door_state==1)
			 	{
					 ortp_mutex_lock(&prompt_mutex);
												 
					 //strcpy(received_prompt,"chat sip:192.168.7.201 ins_call_command %c,%c",buff[1],buff[2]);
					 sprintf(received_prompt,"chat sip:192.168.7.201 door_close ");
					 //strcpy(received_prompt,"status register\0");
					 //   printf("Receiving command '%s'\n",received_prompt);kfflush(stdout);
					 have_prompt=TRUE;
					 ortp_mutex_unlock(&prompt_mutex);
					door_state=0;
				 }
				open_door_time=0;
		 	}

		if((access("/mnt/nand/end",F_OK))==0)
				{
					if(reboot_times==0)
						{
						lcd_clean() ;
						disp_hanzi(0x80,"重启中");
						fist_line=1;
					//printf(" file  exsit. reboot!!\n");
					ortp_mutex_lock(&prompt_mutex);

					strcpy(received_prompt,"quit");
					//strcpy(received_prompt,"status register\0");
					//   printf("Receiving command '%s'\n",received_prompt);kfflush(stdout);
					have_prompt=TRUE;
					ortp_mutex_unlock(&prompt_mutex);
					reboot_times++;
						}
				}


	
 	}

}


void init_time_check_kapd()
{
	pthread_t pid_kapd;


	if (pthread_create(&pid_kapd,NULL,time_kapd,NULL)!=0) {
				   printf("init_time_check_kapd Create thread error!\n");
				   return ;
			   }

}

void call_snj_fun()
{
int f_num;

	tmp_buf_logs[4]='\0';
	swich_gpio_set(1,0,1,0);
	MIC_run(0);
	printf("start wirte\n");
		for(f_num=0;f_num<5;f_num++)
			{
			
			printf("buff_2 is %d\n",buff_2[f_num]);
			fputc(buff_2[f_num],output);
			}
		rec_bb_flag=1;
		sleep(1);
		//usleep(50000);
		if(rec_bb_flag==2)
			{
		call_state=1;
		//start  timekeeping
		time_keeping_fun(60);
		timekeeping_start=1;
		call_occupy_flag=1;
		memkou_call_sn=1;
		printf("timekeeping_starts\n");
		
		memset(data_num,0,sizeof(data_num));
		pthread_mutex_lock(&Device_mutex);
		lcd_clean() ;
		//send_data('1');
		disp_hanzi(0x80,"    接通中    ");	
		//disp_hanzi(0x80,"    通话中	 ");		
		  fist_line=1;
		pthread_mutex_unlock(&Device_mutex);
		  data_pint=0;
		  rec_bb_flag=0;
		  light_run(1);
			}
		else
			{
			fputc(0xcc,output);
			reset_lcd();
			rec_bb_flag=0;
			MIC_run(1);	
			swich_gpio_set(0,0,1,1);
		}
		clean_men_flag=0;


}

void call_snj_precess()
{
	pthread_t pid_snj;


	if (pthread_create(&pid_snj,NULL,call_snj_fun,NULL)!=0) {
				   printf("init_time_check_kapd Create thread error!\n");
				  return ;
			   }
	pthread_detach(pid_snj);

}


void clean_lcd_fun()
{
	clean_lcd_flag=1;

	reset_lcd();
	MIC_run(1);	
	clean_lcd_flag=0;

}

void clean_lcd_precess()
{
	pthread_t pid_clean;


	if (pthread_create(&pid_clean,NULL, clean_lcd_fun,NULL)!=0) {
				   printf("init_time_check_kapd Create thread error!\n");
				   return ;
			   }
	pthread_detach(pid_clean);

}



void test_fun()
{   
char tmp_call[30];
int i;
int ret;

	while(1)
		{
			sleep(6);
		
																call_occupy_flag=1;
		
																	pthread_mutex_lock(&Device_mutex);
																lcd_clean() ;
																disp_hanzi(0x80,"	 接通中    ");	
															  //disp_hanzi(0x80,"	 通话中    ");			
															  fist_line=1;
															  pthread_mutex_unlock(&Device_mutex);
															  
																memset(tmp_call,0,sizeof(tmp_call));
																/*
																for(i=1;i<=glj_num;i++)
																	{
																	
																	sprintf(tmp_call,"192.168.7.20%d",i);
																	printf("tmp_call is %s\n",tmp_call);
																	ret=call_glj_check(tmp_call);
																	if(ret==0)
																		{
																			break;
																		}
																	}
		
																	if(i==glj_num+1)
																	{
																		pthread_mutex_lock(&Device_mutex);
																		lcd_clean() ;
																		disp_hanzi(0x80,"网络繁忙请稍候");	
																		fist_line=1;
																		pthread_mutex_unlock(&Device_mutex);
																		//linphone_core_play_named_tone(linphonec,LinphoneToneBusy);
																		buzzer_run(1);
																		sleep(1);
																		buzzer_run(0);
																		sleep(1);
		
																		buzzer_run(1);
																		sleep(1);
																		buzzer_run(0);
		
																		
																		//sleep(2);
																		reset_lcd();
																		call_occupy_flag=0;
																	//	break; 
																	//goto START;
																	}
		*/
																light_run(1);
																memset(tmp_call,0,sizeof(tmp_call));	
																sprintf(tmp_call,"call sip:192.168.7.20%d",i);
																printf("tmp_call is %s\n",tmp_call);
																swich_gpio_set(1,1,0,1);
																MIC_run(1);
																ortp_mutex_lock(&prompt_mutex);
																
																strcpy(received_prompt,tmp_call);
																//strcpy(received_prompt,"status register\0");
																//	 printf("Receiving command '%s'\n",received_prompt);kfflush(stdout);
																have_prompt=TRUE;
																ortp_mutex_unlock(&prompt_mutex);
																time_keeping_fun(60);
																call_state=2;
																//call_occupy_flag=1;
		
																memset(data_num,0,sizeof(data_num));
															
															  data_pint=0;
															
				sleep(6);

			ortp_mutex_lock(&prompt_mutex);
							 
							 strcpy(received_prompt,"terminate all");
							 //strcpy(received_prompt,"status register\0");
							 //   printf("Receiving command '%s'\n",received_prompt);kfflush(stdout);
							 have_prompt=TRUE;
							 ortp_mutex_unlock(&prompt_mutex);
								 call_state=0;
				

		}

}

void test_precess()
{
	pthread_t pid_test;


	if (pthread_create(&pid_test,NULL, test_fun,NULL)!=0) {
				   printf("init_time_check_kapd Create thread error!\n");
				   return ;
			   }
	//pthread_detach(pid_test);

}



int kpd_check()
{
	struct input_event data;
	  int num[12]={3,6,9,11,
				   2,5,8,0,
				   1,4,7,10};
	  int num_ch=0;
	  char num_change;
	  int put_time=0;
	  
	  int refresh_flag=0;
	  
	  
	  int manage_password[6]={1,1,1,1,1,1};
	  int door_password[6]={1,1,1,1,1,1};
	  int macadd_num[2]={1,1};
	 
	  uchar command_data;
	  int i;
	  int set_num=10;
	  
	  int no_show_flag=0;
	  char tmp_gate[20];
	  char tmp_ip[20];
	  char tmp_mac[50];
	   char tmp_call[30];
	    char tmp_show[50];
	   char tmp_passwd[50];
	  int ret;
	  int max_num=0;
	  FILE*fp_cm,*fp_do,*fp_ip,*fp_num,*fp_mac,*fp_lock,*fp_door_num;
	  FILE* fpr_op, *fpr_cmd,*fp_pd;
	  int f_num=0;
	  struct  timeval  start;
	  struct  timeval  end;
	  unsigned int timer;
	  int button_up_flag=0;
	  int button_buzzer=0;
	  int door_num_read[4];
	//  char buf_tmp[30];
	 // FILE *fp_ctf;
	  
	  if(init_device()<0) {
		printf("init error\n");
		return -1;
	  }
	  	pthread_mutex_init(&Device_mutex,NULL);
		gpio_init();
		
		swich_gpio_init();
		//jimmy ++ set gpio normal stand
		swich_gpio_set(0,0,1,1);
		load_sound_init() ;
		  lcd_init() ; 
		uart_init();
		buzzer_gpio_init();
		MIC_gpio_init();
		light_gpio_init();
		lock_gpio_init();
		read_gpio_config();
		init_time_check_kapd();
		heart_search_init();

		 fputc(buff_4[0],output);
		 
		  usleep(50000);
		  fputc(buff_4[1],output);
		  usleep(50000);

		  
		  fputc(buff_4[0],output);
		  
		   usleep(50000);
		   fputc(buff_4[1],output);
		   usleep(50000);


		fputc(buff_4[0],output);

		 usleep(50000);
		 fputc(buff_4[1],output);
		 usleep(50000);

		
//		lock_flag==0
	
	 // swich_gpio_set(1,1,0,1);
	 // swich_gpio_set(1,0,1,0);
		reset_lcd();
	//	fp_ctf = fdopen(fd, "r+");

	//test_precess();//jimmy test
	  for(;;){
		START:
		read(fd,&data,sizeof(data));
		//fflush(fp_ctf);
		//printf("fflush!!\n");
		//printf("read\n");
		/*
		 if(timekeeping_start == 1)
		   	{
		   	  printf("start timekeep!\n");
 			  gettimeofday(&start,NULL);
			  timekeeping_start=2;
			  timer=0;
			}
		   else if(timekeeping_start==2)
		   	{
				gettimeofday(&end,NULL);
			 // timer = 1000000 * (end.tv_sec-start.tv_sec)+ end.tv_usec-start.tv_usec;
			 timer = (end.tv_sec-start.tv_sec);
			 //sleep(1);
			  printf("timer = %d s\n",timer);
			 if(timer >=90)
			 	{
			 	//huang up
				 for(f_num=0;f_num<2;f_num++)
				 {
				 printf("start wirte\n");
				 printf("buff_4 is %d\n",buff_4[f_num]);
				 fputc(buff_4[f_num],output);
				 }
				 call_state=0;
				 timekeeping_start=0;

			    }
		    }
		   */

		
		if(data.type == 0 || clean_men_flag==1  || clean_lcd_flag==1)
			continue;
		
if(vice_used==0)
{
	if(state_monitor_flag==0  && (call_ins_flag!=1) && (ins_to_flag!=1) )
	{
		if(call_occupy_flag==0)
			{
			
			if(kpd_state != 1)
			{
			 if(put_time==0)
			 	{
			   		buzzer_run(1);
					usleep(50000);
					buzzer_run(0);
			 	}
			}
		
		//   printf("%d.%06d ", data.time.tv_sec, data.time.tv_usec);
		//   printf("%d %d %d %s\n", data.type, data.code, data.value, (data.value == 0) ? "up" : "down");
		//   printf("num is %d\n",num[data.code-1]);
		//   printf("directe is %s\n",(data.value == 0) ? "up" : "down");

		   time_ckeck_kapd_num=30;

		  

		   
		   if(data.value)
			{
				num_ch=num[data.code-1];
				put_time++;
				if(put_time>8)
					{
						printf("pass %d  loog time push\n",num_ch);
						if(kpd_state == 0)
							{
								if(num_ch == 10)
									{
									lcd_clean() ;
									disp_hanzi(0x80,"请输入密码");
									fist_line=1;
									kpd_state = 3;
									max_num=8;
									}
							}
						else if(kpd_state == 1)
							{
								if(num_ch == 1)
									{
										
										kpd_state=2;
										refresh_flag=1;
										lcd_clean() ;
										disp_hanzi(0x80,"修改管理密码");
										fist_line=1;
										set_num=1;
										no_show_flag=1;
										max_num=4;
										printf(" state %d \n",kpd_state);
									}
									else if(num_ch == 2)
									{
										
										kpd_state=2;
										refresh_flag=1;
										lcd_clean() ;
										disp_hanzi(0x80,"修改开锁密码");
										fist_line=1;
										set_num=2;
										no_show_flag=1;
										max_num=4;
										printf(" state %d \n",kpd_state);
									}
									
										else if(num_ch == 5)
									{
										
										kpd_state=2;
										refresh_flag=1;
										lcd_clean() ;
										disp_hanzi(0x80,"设置门口机号码");
										fist_line=1;
										set_num=3;
										no_show_flag=1;
										max_num=3;
										printf(" state %d \n",kpd_state);
									}
									else if(num_ch == 3)
									{
										
										kpd_state=2;
										refresh_flag=1;
										lcd_clean() ;
										disp_hanzi(0x80,"设置用户开门密码");
										fist_line=1;
										set_num=4;
										no_show_flag=1;
										max_num=8;
										printf(" state %d \n",kpd_state);
									}
									else if(num_ch == 6)
									{
										
										kpd_state=2;
										refresh_flag=1;
										lcd_clean() ;
										disp_hanzi(0x80,"设置物理地址");
										
										fist_line=1;
										set_num=5;
										no_show_flag=1;
										max_num=3;
										printf(" state %d \n",kpd_state);
									}
									else if(num_ch == 7)
									{
										
										kpd_state=2;
										refresh_flag=1;
										lcd_clean() ;
										disp_hanzi(0x80,"设置解码器");
										
										fist_line=1;
										set_num=6;
										no_show_flag=1;
										max_num=8;
										printf(" state %d \n",kpd_state);
									}
									else if(num_ch == 4)
									{
										kpd_state=2;
										refresh_flag=1;
										lcd_clean() ;
										disp_hanzi(0x80,"删除用户开门密码");
										fist_line=1;
										set_num=9;
										no_show_flag=1;
										max_num=4;
										printf(" state %d \n",kpd_state);
									}
										else if(num_ch == 9)
									{
										
										kpd_state=2;
										refresh_flag=1;
										lcd_clean() ;
									
										disp_hanzi(0x80,"电控磁力锁选择");
										fist_line=1;
										set_num=8;
										no_show_flag=1;
										max_num=1;
										printf(" state %d \n",kpd_state);
									}
									else if(num_ch==8)
										{
										
										kpd_state=2;
										refresh_flag=1;
										lcd_clean() ;
										disp_hanzi(0x80,"查询门牌号码");
										//disp_hanzi(0x80,"获取物理地址");
										fist_line=1;
										set_num=7;
										no_show_flag=1;
										max_num=4;
										printf(" state %d \n",kpd_state);
											

									}
									else if(num_ch == 0)
									{
									if((fp_door_num=fopen("/mnt/nand1-1/config/door_num","r"))==NULL)
											{
											printf("can not open command passwd the file.\n");
											//return NULL;
											goto START;
											}
										fscanf(fp_door_num,"%d,%d,%d",&door_num_read[0],&door_num_read[1],&door_num_read[2]);
										fclose(fp_door_num);
	
										printf("data_num is %d %d %d!!\n",door_num_read[0],door_num_read[1],door_num_read[2]);
										memset(tmp_show,0,sizeof(tmp_show));
									   
										lcd_clean() ;
										sprintf(tmp_show,"192.168.7.%d%d%d",door_num_read[0],door_num_read[1],door_num_read[2]);
										disp_hanzi(0x80,"IP地址");
										send_command(0x90);
										for(i=0;tmp_show[i]!='\0';i++)
											{
  										  	send_data(tmp_show[i]);
											}
					//disp_hanzi(0x90,tmp_show);
										sleep(2);
										if((fp_door_num=fopen("/mnt/nand1-1/config/macadress","r"))==NULL)
											{
											printf("can not open command passwd the file.\n");
											//return NULL;
											goto START;
											}
										fgets( tmp_show  ,	sizeof(tmp_show)  , fp_door_num );
										fclose(fp_door_num);
										printf("tmp_show is %s!\n",tmp_show);
										
									   
										lcd_clean() ;
										
										disp_hanzi(0x80,"MAC 地址");
										send_command(0x90);
										for(i=26;tmp_show[i]!='\0';i++)
											{
  										  	send_data(tmp_show[i]);
											}

										sleep(2);
										if((fp_door_num=fopen("/mnt/nand1-1/version","r"))==NULL)
											{
											printf("can not open command passwd the file.\n");
											//return NULL;
											goto START;
											}
										fgets( tmp_show  ,	sizeof(tmp_show)  , fp_door_num );
										fclose(fp_door_num);
										printf("tmp_show is %s!\n",tmp_show);
										
									   
										lcd_clean() ;
										
										disp_hanzi(0x80,"版本号");
										send_command(0x90);
										for(i=7;tmp_show[i]!='\0';i++)
											{
  										  	send_data(tmp_show[i]);
											}
										

										 kpd_state=2;
										refresh_flag=1;
										fist_line=1;
										set_num=0;
										no_show_flag=1;
										
										printf(" state %d \n",kpd_state);
									}
									 if(data.value==0)
			 						{
									buzzer_run(1);
									usleep(50000);
									buzzer_run(0);
									 }
								
							}
						else if(kpd_state == 2)
							{
								
							}
					}
			}
			else if(data.value ==0)
			{
				
				if(num_ch==num[data.code-1])
				{
					button_up_flag=0;
					printf("pass %d\n",num_ch);
					//send_command(0x90);
					
					
				if(num_ch == 10 )
					{
						//send_data('*');
			
						if(put_time<=8)
							{
							//lcd_clean();
							memset(data_num,0,sizeof(data_num));
							data_pint=0;
							
								if(kpd_state==0)
									{
										//kpd_state=0;
										//reset_lcd();
										clean_lcd_precess();
									}
								else if(kpd_state==1)
									{
										//kpd_state=0;
										reset_lcd();
										printf("fputc(buff_4[0],output);\n");
										fputc(buff_4[0],output);

										usleep(50000);
										fputc(buff_4[1],output);
										usleep(50000);


										fputc(buff_4[0],output);

										usleep(50000);
										fputc(buff_4[1],output);
										usleep(50000);


										fputc(buff_4[0],output);

										usleep(50000);
										fputc(buff_4[1],output);
										usleep(50000);
									}
								else if(kpd_state==2)
									{
										kpd_state=1;
										lcd_clean() ;
										disp_hanzi(0x80,"请输入设置项");	
										fist_line=1;
										set_num=10;
									}
								else if(kpd_state==3)
									{
										//kpd_state=0;
										reset_lcd();
										
										fputc(buff_4[0],output);

										usleep(50000);
										fputc(buff_4[1],output);
										usleep(50000);


										fputc(buff_4[0],output);

										usleep(50000);
										fputc(buff_4[1],output);
										usleep(50000);


										fputc(buff_4[0],output);

										usleep(50000);
										fputc(buff_4[1],output);
										usleep(50000);
									}
								put_time=0;
							goto START; 
							}

						
						
							
						
					}
				else if(num_ch == 11 )
					{
						//send_data('#');
							for(i=0;i<data_pint;i++)
						{
							printf("data_num %d\n",data_num[i]);
						}

					if(kpd_state==3)
						{
	
						printf("data_pint=%d",data_pint);
						if(data_pint==4)
							{
								if((fpr_cmd=fopen("/mnt/nand1-1/config/command_pw","r"))==NULL)
											{
											printf("can not open the file.\n");
											return -1;
											}
											
											fscanf(fpr_cmd,"%d,%d,%d,%d",&manage_password[0],
											&manage_password[1],&manage_password[2],&manage_password[3]);
											fclose(fpr_cmd);
								for(i=0;i<4;i++)
								{
									if(data_num[i]!=manage_password[i])
										{
										printf("data_num[i]=%d",data_num[i]);
										printf("manage_password[i]=%d",manage_password[i]);
									  goto NEXT;	
										}
								}
								
								kpd_state=1;
								refresh_flag=1;
								lcd_clean() ;
								disp_hanzi(0x80,"进入设置模式");
								fist_line=1;
								set_num = 10;
								//put_time=0;
								printf(" state %d \n",kpd_state);
								 goto START;
					
					
						NEXT:
							if((fpr_op=fopen("/mnt/nand1-1/config/door_pw","r"))==NULL)
								{
								printf("can not open the file.\n");
								return -1;
								}
								
								fscanf(fpr_op,"%d,%d,%d,%d",&door_password[0],
								&door_password[1],&door_password[2],&door_password[3]);
								fclose(fpr_op);
								for(i=0;i<4;i++)
								{
									if(data_num[i]!=door_password[i])
										{
										printf("data_num[i]=%d",data_num[i]);
										printf("door_password[i]=%d",door_password[i]);
										lcd_clean() ;
										disp_hanzi(0x80,"请输入密码");
										fist_line=1;
									
										memset(data_num,0,sizeof(data_num));
										data_pint=0;
									  goto START;	
										}
								}
					
								printf("open door\n");
								if(button_up_flag==0)
									{
									lock_open_precess();
									button_up_flag=1;
									
									reset_lcd();
									}
						
							}	
						else if(data_pint==8)
							{
							memset(tmp_passwd,0,sizeof(tmp_passwd));
							sprintf(tmp_passwd,"/mnt/nand1-1/password/%d%d%d%d",data_num[0],data_num[1],data_num[2],data_num[3]);

							if((fpr_op=fopen(tmp_passwd,"r"))==NULL)
								{
								printf("can not open the file.\n");
								lcd_clean() ;
										disp_hanzi(0x80,"请输入密码");
										fist_line=1;
										memset(data_num,0,sizeof(data_num));
										data_pint=0;
									  goto START;	
								}
								
								fscanf(fpr_op,"%d,%d,%d,%d,",&door_password[0],
								&door_password[1],&door_password[2],&door_password[3]);
								fclose(fpr_op);
								for(i=0;i<4;i++)
								{
									if(data_num[i+4]!=door_password[i])
										{
										printf("data_num[i]=%d\n",data_num[i]);
										printf("door_password[i]=%d\n",door_password[i]);
										lcd_clean() ;
										disp_hanzi(0x80,"请输入密码");
										fist_line=1;
										memset(data_num,0,sizeof(data_num));
										data_pint=0;
									  goto START;	
										}
								}
					
								printf("open door\n");
								if(button_up_flag==0)
									{
									lock_open_precess();
									button_up_flag=1;
									
									reset_lcd();
									}

						}
						else
							{
							lcd_clean() ;
							disp_hanzi(0x80,"请输入密码");
							memset(data_num,0,sizeof(data_num));
							data_pint=0;
							fist_line=1;
							}
						}
					else if(kpd_state==2)
						{
							switch (set_num){
							case 1:
								printf("command passwd\n");
								if(data_pint==4)
								{
								if((fpr_op=fopen("/mnt/nand1-1/config/door_pw","r"))==NULL)
								{
								printf("can not open the file.\n");
								return -1;
								}
								
								fscanf(fpr_op,"%d,%d,%d,%d",&door_password[0],
								&door_password[1],&door_password[2],&door_password[3]);
								fclose(fpr_op);
								
								if(door_password[0]==data_num[0] && door_password[1]==data_num[1] &&
									door_password[2]==data_num[2] && door_password[3]==data_num[3] )
									{
									lcd_clean();
									disp_hanzi(0x80,"设置错误");
									fist_line=1;	
									break;
	
									}

								
								if((fp_cm=fopen("/mnt/nand1-1/config/command_pw","w"))==NULL)
											{
											printf("can not open command passwd the file.\n");
											return -1;
											}
	
											fprintf(fp_cm,"%d,%d,%d,%d",data_num[0],
											data_num[1],data_num[2],data_num[3]
											);
					 		    fclose(fp_cm);
								lcd_clean();
								disp_hanzi(0x80,"设置成功");
								fist_line=1;	
										}
								else
								{
								lcd_clean();
								disp_hanzi(0x80,"设置错误");
								fist_line=1;	
								}
											
								break;
							case 2:
								printf("door passwd\n");
								if(data_pint==4)
								{
								if((fpr_cmd=fopen("/mnt/nand1-1/config/command_pw","r"))==NULL)
											{
											printf("can not open the file.\n");
											return -1;
											}
											
											fscanf(fpr_cmd,"%d,%d,%d,%d",&manage_password[0],
											&manage_password[1],&manage_password[2],&manage_password[3]);
											fclose(fpr_cmd);
											
								if(manage_password[0]==data_num[0] && manage_password[1]==data_num[1] &&
									manage_password[2]==data_num[2] && manage_password[3]==data_num[3] )
									{
									lcd_clean();
									disp_hanzi(0x80,"设置错误");
									fist_line=1;	
									break;

									}
								
								if((fp_do=fopen("/mnt/nand1-1/config/door_pw","w"))==NULL)
											{
											printf("can not open command passwd the file.\n");
											return -1;
											}
	
											fprintf(fp_do,"%d,%d,%d,%d,",data_num[0],
											data_num[1],data_num[2],data_num[3]);
											fclose(fp_do);
											lcd_clean();
											disp_hanzi(0x80,"设置成功");
											fist_line=1;	
										}
										else
										{
										lcd_clean();
										disp_hanzi(0x80,"设置错误");
							        	fist_line=1;	
										}
								break;
							case 3:
								printf("set door num\n");
								if(data_pint==3)
								{
								if((fp_num=fopen("/mnt/nand1-1/config/door_num","w"))==NULL)
											{
											printf("can not open command passwd the file.\n");
											return -1;
											}
	
								fprintf(fp_num,"%d,%d,%d",data_num[0],
								data_num[1],data_num[2]);
					 			  fclose(fp_num);
								  
								  sprintf(tmp_ip,"192.168.7.%d%d%d",data_num[0],
									  data_num[1],data_num[2]);
								  printf("tmp_ip is  %s\n",tmp_ip);

								  ret=SetIfAddr("eth0",tmp_ip,"255.255.255.0","192.168.7.0");	
								  if(ret>=0)
					 				{
								if((fp_ip=fopen("/mnt/nand1-1/route.sh","w"))==NULL)
											{
											printf("can not open command passwd the file.\n");
											return -1;
											}
								fprintf(fp_ip,"#!/bin/sh\n");
								fprintf(fp_ip,"usleep 60000\n");
								
								fprintf(fp_ip,"/sbin/ifconfig eth0 192.168.7.%d%d%d up\n",data_num[0],
								data_num[1],data_num[2]);
								//fprintf(fp_ip,"/sbin/route add -net 224.0.0.0 netmask 240.0.0.0 dev eth0\n");
								fprintf(fp_ip,"/sbin/route add -net 192.168.0.0 netmask 255.255.0.0 dev eth0\n");
								fprintf(fp_ip,"/sbin/route add default gw \"192.168.7.1\" dev eth0\n");
								fprintf(fp_ip,"/sbin/route\n");
								fprintf(fp_ip,"/sbin/ifconfig eth0 192.168.7.%d%d%d up\n",data_num[0],
								data_num[1],data_num[2]);

								fprintf(fp_ip,"/sbin/ifconfig eth0\n");
								fclose(fp_ip);
									lcd_clean();
								  disp_hanzi(0x80,"重启中请稍候");
								fist_line=1;	
								system("reboot");    
										}	
								  else
										{
										lcd_clean();
										disp_hanzi(0x80,"设置错误");
								fist_line=1;	
										}
								
										}
										else
										{
										lcd_clean();
										disp_hanzi(0x80,"设置错误");
								fist_line=1;	
										}
								break;
							case 4:
								printf("set password\n");
								if(data_pint==8)
									{
									memset(tmp_passwd,0,sizeof(tmp_passwd));
									sprintf(tmp_passwd,"/mnt/nand1-1/password/%d%d%d%d",data_num[0],data_num[1],data_num[2],data_num[3]);
									printf("tmp_passwd is %s\n",tmp_passwd);


									if((fp_pd=fopen(tmp_passwd,"w"))==NULL)
									{
									printf("can not open command passwd the file.\n");
									return -1;
									}

									fprintf(fp_pd,"%d,%d,%d,%d,",data_num[4],data_num[5],data_num[6],data_num[7]);
									fclose(fp_pd);

										if((fp_door_num=fopen("/mnt/nand1-1/config/door_num","r"))==NULL)
											{
											printf("can not open command passwd the file.\n");
											//return NULL;
											goto START;
											}
										fscanf(fp_door_num,"%d,%d,%d",&door_num_read[0],&door_num_read[1],&door_num_read[2]);
										fclose(fp_door_num);
	
										printf("data_num is %d %d %d!!\n",door_num_read[0],door_num_read[1],door_num_read[2]);
										
									memset(tmp_passwd,0,sizeof(tmp_passwd));
									sprintf(tmp_passwd,"chat sip:192.168.8.%d%d%d  set_password %d%d%d%d%d%d%d%d",door_num_read[0],door_num_read[1],door_num_read[2],
																									data_num[0],data_num[1],data_num[2],
																									data_num[3],data_num[4],data_num[5],data_num[6],data_num[7]);
									 ortp_mutex_lock(&prompt_mutex);
								
									//strcpy(received_prompt,"chat sip:192.168.7.201 ins_call_command %c,%c",buff[1],buff[2]);
									sprintf(received_prompt,tmp_passwd);
									//strcpy(received_prompt,"status register\0");
									//	 printf("Receiving command '%s'\n",received_prompt);kfflush(stdout);
									have_prompt=TRUE;
									ortp_mutex_unlock(&prompt_mutex);

									usleep(500000);

									memset(tmp_passwd,0,sizeof(tmp_passwd));
									sprintf(tmp_passwd,"chat sip:192.168.9.%d%d%d  set_password %d%d%d%d%d%d%d%d",door_num_read[0],door_num_read[1],door_num_read[2],
																									data_num[0],data_num[1],data_num[2],
																									data_num[3],data_num[4],data_num[5],data_num[6],data_num[7]);
									 ortp_mutex_lock(&prompt_mutex);
								
									//strcpy(received_prompt,"chat sip:192.168.7.201 ins_call_command %c,%c",buff[1],buff[2]);
									sprintf(received_prompt,tmp_passwd);
									//strcpy(received_prompt,"status register\0");
									//	 printf("Receiving command '%s'\n",received_prompt);kfflush(stdout);
									have_prompt=TRUE;
									ortp_mutex_unlock(&prompt_mutex);
									
					
									printf("tmp_passwd is %s\n",tmp_passwd);
									
									lcd_clean();
									disp_hanzi(0x80,"设置成功");
									fist_line=1;	

									}
									else
									{
									lcd_clean();
									disp_hanzi(0x80,"设置错误");
									fist_line=1;	
									}
								break;
							case 5:
								printf("set macaddress\n");
							if(data_pint==3)
								{
								sprintf(tmp_mac,"ifconfig eth0 hw ether 00:02:AC:55:8%d:%d%d",data_num[0],
								data_num[1],data_num[2]);
								printf("tmp_mac is %s\n",tmp_mac);
								system(tmp_mac);
								if((fp_mac=fopen("/mnt/nand1-1/config/macadress","w"))==NULL)
											{
											printf("can not open command passwd the file.\n");
											return -1;
											}
	
								fprintf(fp_mac,"ifconfig eth0 hw ether 00:02:AC:55:8%d:%d%d",data_num[0],
								data_num[1],data_num[2]);
								fclose(fp_mac);
								lcd_clean();
								disp_hanzi(0x80,"设置成功");
								fist_line=1;	
								}
								else
								{
								lcd_clean();
								disp_hanzi(0x80,"设置错误");
								fist_line=1;	
								}
								break;	
							case 6:
							
								printf("set decoder\n");
								if(data_pint==8)
								{
								
									if(data_num[0]*10+data_num[1] >63)
										{
											lcd_clean();
											disp_hanzi(0x80,"设置错误");
											fist_line=1;	
											break;
										}

									if(data_num[2]*10+data_num[3] >4)
										{
											lcd_clean();
											disp_hanzi(0x80,"设置错误");
											fist_line=1;	
											break;
										}
									

				
									
								#ifdef Set_door_address6
									buff_1[1]=data_num[0]*16+data_num[1];
								
									if(buff_1[1]==0x11)
										{
										buff_1[1]=0x91;
										}
									else if(buff_1[1]==0x13)
										{
										buff_1[1]=0x93;
										}
									
									buff_1[2]=data_num[2]*16+data_num[3];
									buff_1[3]=data_num[4]*16+data_num[5];
									if(buff_1[3]==0x11)
										{
										buff_1[3]=0x91;
										}
									else if(buff_1[3]==0x13)
										{
										buff_1[3]=0x93;
										}

										
									buff_1[4]=data_num[6]*16+data_num[7];
									if(buff_1[4]==0x11)
										{
										buff_1[4]=0x91;
										}
									else if(buff_1[4]==0x13)
										{
										buff_1[4]=0x93;
										}
								#else
									buff_1[0]=data_num[0]*16+data_num[1];
									buff_1[1]=data_num[2]*16+data_num[3];
									buff_1[2]=data_num[4]*16+data_num[5];
									if(buff_1[2]==0x11)
										{
										buff_1[2]=0x91;
										}
									else if(buff_1[2]==0x13)
										{
										buff_1[2]=0x93;
										}

										
									buff_1[3]=data_num[6]*16+data_num[7];
									if(buff_1[3]==0x11)
										{
										buff_1[3]=0x91;
										}
									else if(buff_1[3]==0x13)
										{
										buff_1[3]=0x93;
										}
								#endif
									
									printf("start wirte\n");
									af_no_self=1;
								#ifdef Set_door_address6
									for(f_num=0;f_num<6;f_num++)
							    #else
									for(f_num=0;f_num<5;f_num++)
								#endif
									{
									
									printf("buff_1 is %d\n",buff_1[f_num]);
									fputc(buff_1[f_num],output);
									
									}

									rec_bb_flag=1;
									sleep(1);
									if(rec_bb_flag==2)
										{
									
									  rec_bb_flag=0;

									  lcd_clean();
								   	disp_hanzi(0x80,"设置成功");
										}
									else
										{
										lcd_clean();
										disp_hanzi(0x80,"设置错误");
										fist_line=1;	
										rec_bb_flag=0;
										//break;

									}
														
									
									/*
									fist_line=1;	
									sleep(1);
									lcd_clean() ;
									disp_hanzi(0x80,"设置解码器");*/
								}
								else
								{
								lcd_clean();
								disp_hanzi(0x80,"设置错误");
								fist_line=1;	
								}
								break;
								
							case 7:
								printf("check door num\n");
								if(data_pint==4)
								{

									if(data_num[0]*10+data_num[1] >63)
										{
											lcd_clean();
											disp_hanzi(0x80,"设置错误");
											fist_line=1;	
											break;
										}

									if(data_num[2]*10+data_num[3] >4)
										{
											lcd_clean();
											disp_hanzi(0x80,"设置错误");
											fist_line=1;	
											break;
										}
									
									#ifdef Set_door_address6
									buff_3[1]=data_num[0]*16+data_num[1];
									
									//buff_1[1]=data_num[2]*16+data_num[3];
										if(buff_3[1]==0x11)
										{
										buff_3[1]=0x91;
										}
									else if(buff_3[1]==0x13)
										{
										buff_3[1]=0x93;
										}
									
									buff_3[3]=data_num[2]*16+data_num[3];
										if(buff_3[3]==0x11)
										{
										buff_3[3]=0x91;
										}
									else if(buff_3[3]==0x13)
										{
										buff_3[3]=0x93;
										}
									//buff_1[3]=data_num[6]*16+data_num[7];
									af_no_self=1;
									for(f_num=0;f_num<6;f_num++)
									{
									//printf("start wirte\n");
									printf("buff_1 is %d\n",buff_3[f_num]);
									fputc(buff_3[f_num],output);
										
									}
									#else
										buff_3[0]=data_num[0]*16+data_num[1];
									
									//buff_1[1]=data_num[2]*16+data_num[3];
										if(buff_3[0]==0x11)
										{
										buff_3[0]=0x91;
										}
									else if(buff_3[0]==0x13)
										{
										buff_3[0]=0x93;
										}
									
									buff_3[2]=data_num[2]*16+data_num[3];
										if(buff_3[2]==0x11)
										{
										buff_3[2]=0x91;
										}
									else if(buff_3[2]==0x13)
										{
										buff_3[2]=0x93;
										}
									//buff_1[3]=data_num[6]*16+data_num[7];
									
									for(f_num=0;f_num<5;f_num++)
									{
									//printf("start wirte\n");
									printf("buff_1 is %d\n",buff_3[f_num]);
									fputc(buff_3[f_num],output);
										
									}
									#endif
								}
								
								else
										{
									lcd_clean();
									disp_hanzi(0x80,"设置错误");
									fist_line=1;	
										}
									break;
							case 8:
								printf("lock type\n");
								if(data_pint==1)
								{
									if(data_num[0]==0 || data_num[0]==1)
										{
										lock_flag=data_num[0];
										if((fp_lock=fopen("/mnt/nand1-1/config/lock_gpio","w"))==NULL)
										{
										printf("can not open command passwd the file.\n");
										return -1;
										}

										fprintf(fp_lock,"%d",data_num[0]);
										fclose(fp_lock);
										read_gpio_config();
										lcd_clean();
										disp_hanzi(0x80,"设置成功");
										fist_line=1;	
										}
									else
										{
										lcd_clean();
										disp_hanzi(0x80,"设置错误");
										fist_line=1;	
										}
								}
								
								else
								{
								lcd_clean();
								disp_hanzi(0x80,"设置错误");
								fist_line=1;	
								}
							break;
							case 9:
							
							printf("dele password\n");
								if(data_pint==4)
									{
									memset(tmp_passwd,0,sizeof(tmp_passwd));
									sprintf(tmp_passwd,"/mnt/nand1-1/password/%d%d%d%d",data_num[0],data_num[1],data_num[2],data_num[3]);
									printf("tmp_passwd is %s\n",tmp_passwd);


									if((fp_pd=fopen(tmp_passwd,"w"))==NULL)
									{
									printf("can not open command passwd the file.\n");
									return -1;
									}

									fprintf(fp_pd,"%d,%d,%d,%d,",10,10,10,10);
									fclose(fp_pd);

										if((fp_door_num=fopen("/mnt/nand1-1/config/door_num","r"))==NULL)
											{
											printf("can not open command passwd the file.\n");
											//return NULL;
											goto START;
											}
										fscanf(fp_door_num,"%d,%d,%d",&door_num_read[0],&door_num_read[1],&door_num_read[2]);
										fclose(fp_door_num);
	
										printf("data_num is %d %d %d!!\n",door_num_read[0],door_num_read[1],door_num_read[2]);
										
									memset(tmp_passwd,0,sizeof(tmp_passwd));
									sprintf(tmp_passwd,"chat sip:192.168.8.%d%d%d  dele_password %d%d%d%d",door_num_read[0],door_num_read[1],door_num_read[2],
																									data_num[0],data_num[1],data_num[2],
																									data_num[3]);
									 ortp_mutex_lock(&prompt_mutex);
								
									//strcpy(received_prompt,"chat sip:192.168.7.201 ins_call_command %c,%c",buff[1],buff[2]);
									sprintf(received_prompt,tmp_passwd);
									//strcpy(received_prompt,"status register\0");
									//	 printf("Receiving command '%s'\n",received_prompt);kfflush(stdout);
									have_prompt=TRUE;
									ortp_mutex_unlock(&prompt_mutex);

									usleep(500000);

									memset(tmp_passwd,0,sizeof(tmp_passwd));
									sprintf(tmp_passwd,"chat sip:192.168.9.%d%d%d  dele_password %d%d%d%d",door_num_read[0],door_num_read[1],door_num_read[2],
																									data_num[0],data_num[1],data_num[2],
																									data_num[3]);
									 ortp_mutex_lock(&prompt_mutex);
								
									//strcpy(received_prompt,"chat sip:192.168.7.201 ins_call_command %c,%c",buff[1],buff[2]);
									sprintf(received_prompt,tmp_passwd);
									//strcpy(received_prompt,"status register\0");
									//	 printf("Receiving command '%s'\n",received_prompt);kfflush(stdout);
									have_prompt=TRUE;
									ortp_mutex_unlock(&prompt_mutex);
									
					
									printf("tmp_passwd is %s\n",tmp_passwd);
									
									lcd_clean();
									disp_hanzi(0x80,"设置成功");
									fist_line=1;	

									}
									else
									{
									lcd_clean();
									disp_hanzi(0x80,"设置错误");
									fist_line=1;	
									}
			
								break;
						case 0:
							printf("show\n");
							kpd_state=1;
							lcd_clean() ;
							disp_hanzi(0x80,"请输入设置项");	
							fist_line=1;
							set_num = 10;
							goto START;
							break;
							default:printf("error\n");
																		}

							if(set_num==7)
								{
							sleep(2);
								}
							else
								{
								sleep(1);

								}
							memset(data_num,0,sizeof(data_num));
							data_pint=0;
							lcd_clean() ;

							if(set_num == 1)
									{
										
										disp_hanzi(0x80,"修改管理密码");
										

									}
									else if(set_num == 2)
									{
										
										disp_hanzi(0x80,"修改开锁密码");
									}
									
										else if(set_num == 3)
									{
										
										disp_hanzi(0x80,"设置门口机号码");

									}
									else if(set_num == 4)
									{
										
										disp_hanzi(0x80,"设置用户开门密码");

									}
									else if(set_num == 5)
									{

										disp_hanzi(0x80,"设置物理地址");

									}
									else if(set_num == 6)
									{
										
										disp_hanzi(0x80,"设置解码器");

									}
									else if(set_num == 7)
									{

										disp_hanzi(0x80,"查询门牌号码");

									}
										else if(set_num == 8)
									{					
										disp_hanzi(0x80,"电控磁力锁选择");
	
									}
										else if(set_num == 9)
									{	

										disp_hanzi(0x80,"删除用户开门密码");
									}
										fist_line=1;
							//	}
								//set_num=0;										
						}
						else if(kpd_state==0)
							{
								//call
								//if(data_num[0]==2 || call_state==2 || call_state==3)
								#ifdef jimmy_debug_print
								printf("call_ins_flag is %d\n",call_ins_flag);
								printf("ins_to_flag is %d\n",ins_to_flag);
								printf("call_state is %d\n",call_state);
								printf("data_pint is %d\n",data_pint);
								#endif
								
							if((call_ins_flag!=1) && (ins_to_flag!=1) )	
									{
										if(data_pint==0 || call_state==2 || call_state==3)
										{
											
											
											printf("call phone\n");
											
												if(call_state==0)
												{
												if(call_occupy_flag==0)
													{
														call_occupy_flag=1;

															pthread_mutex_lock(&Device_mutex);
														lcd_clean() ;
														disp_hanzi(0x80,"    接通中    ");	
													  //disp_hanzi(0x80,"    通话中    ");			
													  fist_line=1;
													  pthread_mutex_unlock(&Device_mutex);
													  
														memset(tmp_call,0,sizeof(tmp_call));
														
														for(i=1;i<=glj_num;i++)
															{
															
															sprintf(tmp_call,"192.168.7.20%d",i);
															printf("tmp_call is %s\n",tmp_call);
															ret=call_glj_check(tmp_call);
															if(ret==0)
																{
																	break;
																}
															}

															if(i==glj_num+1)
															{
																pthread_mutex_lock(&Device_mutex);
																lcd_clean() ;
																disp_hanzi(0x80,"网络繁忙请稍候");	
																fist_line=1;
																pthread_mutex_unlock(&Device_mutex);
																//linphone_core_play_named_tone(linphonec,LinphoneToneBusy);
																buzzer_run(1);
																sleep(1);
																buzzer_run(0);
																sleep(1);

																buzzer_run(1);
																sleep(1);
																buzzer_run(0);

																
																//sleep(2);
																reset_lcd();
																call_occupy_flag=0;
															//	break; 
															goto START;
															}
														

														light_run(1);
														memset(tmp_call,0,sizeof(tmp_call));	
														sprintf(tmp_call,"call sip:192.168.7.20%d",i);
														printf("tmp_call is %s\n",tmp_call);
														swich_gpio_set(1,1,0,1);
														//jm
														MIC_run(0);
														ortp_mutex_lock(&prompt_mutex);
														
														strcpy(received_prompt,tmp_call);
														//strcpy(received_prompt,"status register\0");
														//	 printf("Receiving command '%s'\n",received_prompt);kfflush(stdout);
														have_prompt=TRUE;
														ortp_mutex_unlock(&prompt_mutex);
														time_keeping_fun(60);
														call_state=2;
														//call_occupy_flag=1;

														memset(data_num,0,sizeof(data_num));
													
													  data_pint=0;
											  

											  		#ifdef Doorphone_compile
													call_glj_flag=1;
													#endif
													}
												}
												/*
												else if(call_state==2)
												{
												ortp_mutex_lock(&prompt_mutex);
												
												strcpy(received_prompt,"terminate all");
												//strcpy(received_prompt,"status register\0");
												//	 printf("Receiving command '%s'\n",received_prompt);kfflush(stdout);
												have_prompt=TRUE;
												ortp_mutex_unlock(&prompt_mutex);
													call_state=0;
												}*/
												else if(call_state==3)
												{
												ortp_mutex_lock(&prompt_mutex);
												
												strcpy(received_prompt,"answer");
												//strcpy(received_prompt,"status register\0");
												//	 printf("Receiving command '%s'\n",received_prompt);kfflush(stdout);
												have_prompt=TRUE;
												ortp_mutex_unlock(&prompt_mutex);
													call_state=2;
												}
										}
										//else if(data_num[0]==1 || call_state==1)
										if(data_pint==4 || call_state==1)
											{
											
											if(call_state==0)
												{

												if(call_occupy_flag==0)
													{
													buff_2[1]=data_num[0]*16+data_num[1];
													
														if(buff_2[1]==0x11)
														{
														buff_2[1]=0x91;
														}
													else if(buff_2[1]==0x13)
														{
														buff_2[1]=0x93;
														}

													
													buff_2[2]=data_num[2]*16+data_num[3];
													
													if(buff_2[2]==0x11)
														{
														buff_2[2]=0x91;
														}
													else if(buff_2[2]==0x13)
														{
														buff_2[2]=0x93;
														}
													
													for(f_num=0;f_num<4;f_num++)
															{
														tmp_buf_logs[f_num]=data_num[f_num];
														}
													tmp_buf_logs[4]='\0';
													swich_gpio_set(1,0,1,0);
													MIC_run(0);
													printf("MIC_run(0)!!!!\n");
													printf("start wirte\n");
														for(f_num=0;f_num<5;f_num++)
															{
															
															printf("buff_2 is %d\n",buff_2[f_num]);
															fputc(buff_2[f_num],output);
															}
														call_state=1;
														//start  timekeeping
														time_keeping_fun(60);
														timekeeping_start=1;
														call_occupy_flag=1;
														printf("timekeeping_starts\n");
														memset(data_num,0,sizeof(data_num));
														pthread_mutex_lock(&Device_mutex);
														lcd_clean() ;
			
													  disp_hanzi(0x80,"    通话中    ");			
													  fist_line=1;
													  pthread_mutex_unlock(&Device_mutex);
													  data_pint=0;
													}
												}
											else
												{
												for(f_num=0;f_num<2;f_num++)
													{
													printf("start wirte\n");
													printf("buff_4 is %d\n",buff_4[f_num]);
													fputc(buff_4[f_num],output);
													}
												call_state=0;
												timekeeping_start=0;
												if(times_set>0)
												{	
												times_no_do=1;
												times_set=0;
													}
												}
										    }
										
							
										}
							}
					}
					else
						{
						
									num_change=num_ch+'0';
									
									if(no_show_flag==1)
									{
										no_show_flag=0;
									}
									else
									{
									
										if(kpd_state==0)
										{

										
										if(fist_line==1)
										{
										
										reset_lcd();
										sleep(1);
										}
										

										if(data_pint>2)
			       							{
			       							
			       							//reset_lcd();
			       							if(num_ch>=0 && num_ch<=9)
					       					{
					       					data_num[data_pint]=num_ch;
					       					data_pint++;
					       					}
											pthread_mutex_lock(&Device_mutex);
											send_data(num_change);
											pthread_mutex_unlock(&Device_mutex);
											if(call_state==0)
												{

												if(call_occupy_flag==0)
													{
													buff_2[1]=data_num[0]*16+data_num[1];
													
														if(buff_2[1]==0x11)
														{
														buff_2[1]=0x91;
														}
													else if(buff_2[1]==0x13)
														{
														buff_2[1]=0x93;
														}

													
													buff_2[2]=data_num[2]*16+data_num[3];
													
													if(buff_2[2]==0x11)
														{
														buff_2[2]=0x91;
														}
													else if(buff_2[2]==0x13)
														{
														buff_2[2]=0x93;
														}
													
													for(f_num=0;f_num<4;f_num++)
															{
														tmp_buf_logs[f_num]=data_num[f_num];
														}
													//jjj
													clean_men_flag=1;
													call_snj_precess();
													/*
													tmp_buf_logs[4]='\0';
													swich_gpio_set(1,0,1,0);
													MIC_run(0);
													printf("start wirte\n");
														for(f_num=0;f_num<5;f_num++)
															{
															
															printf("buff_2 is %d\n",buff_2[f_num]);
															fputc(buff_2[f_num],output);
															}
														rec_bb_flag=1;
														sleep(1);
														//usleep(50000);
														if(rec_bb_flag==2)
															{
														call_state=1;
														//start  timekeeping
														time_keeping_fun(60);
														timekeeping_start=1;
														call_occupy_flag=1;
														memkou_call_sn=1;
														printf("timekeeping_starts\n");
														
														memset(data_num,0,sizeof(data_num));
														pthread_mutex_lock(&Device_mutex);
														lcd_clean() ;
														//send_data('1');
														disp_hanzi(0x80,"    接通中    ");	
														//disp_hanzi(0x80,"    通话中    ");		
														  fist_line=1;
														pthread_mutex_unlock(&Device_mutex);
														  data_pint=0;
														  rec_bb_flag=0;
														  light_run(1);
															}
														else
															{
															fputc(0xcc,output);
															reset_lcd();
															rec_bb_flag=0;
															
														}
														*/
													}
												}
											put_time=0;
											goto START;
											}
										
										}
										else if(kpd_state==1)
										{
											put_time=0;
											goto START;
										}
										else
										{
											if(set_num==0)
												{
												kpd_state=1;
												lcd_clean() ;
												disp_hanzi(0x80,"请输入设置项");	
												fist_line=1;
												set_num = 10;
												goto START;
												}
											if(data_pint>max_num-1)
											{
												lcd_clean() ;
												if(kpd_state==3)
													{
													//lcd_clean() ;
													disp_hanzi(0x80,"请输入密码");
													fist_line=1;
													kpd_state = 3;
													max_num=8;
													}
												else
													{
														if(set_num == 1)
														{

														disp_hanzi(0x80,"修改管理密码");
														}
														else if(set_num == 2)
														{

														disp_hanzi(0x80,"修改开锁密码");
														}

														else if(set_num == 3)
														{

														disp_hanzi(0x80,"设置门口机号码");

														}
														else if(set_num == 4)
														{

														disp_hanzi(0x80,"设置用户开门密码");

														}
														else if(set_num == 5)
														{

														disp_hanzi(0x80,"设置物理地址");

														}
														else if(set_num == 6)
														{

														disp_hanzi(0x80,"设置解码器");

														}
														else if(set_num == 7)
														{

														disp_hanzi(0x80,"查询门牌号码");

														}
														else if(set_num == 8)
														{					
														disp_hanzi(0x80,"电控锁磁力锁选择");

														}
													
														else if(set_num == 9)
														{	

															disp_hanzi(0x80,"删除用户开门密码");
														}
														fist_line=1;
														
													}
													send_command(0x90);
													memset(data_num,0,sizeof(data_num));
		       										data_pint=0;
												goto START;
										}

										if(fist_line==1)
											{
											
											//reset_lcd();
											send_command(0x90);
											fist_line=0;
											}

										}

									if(num_ch>=0 && num_ch<=9)
			       					{
			       					data_num[data_pint]=num_ch;
			       					data_pint++;
			       					}
									
									pthread_mutex_lock(&Device_mutex);
									send_data(num_change);
								    pthread_mutex_unlock(&Device_mutex);
								}
						}
						
					
					//num_change=num_ch+'0';
				
						/*
					if(data_pint>=29)
						{
							lcd_clean();
							memset(data_num,0,sizeof(data_num));
							data_pint=0;
						}
					else
						{
					  if(data_pint==14)
						{
							send_command(0x90);
						}
						if(num_ch>=0 && num_ch<=9)
							{
							data_num[data_pint]=num_ch;
							data_pint++;
							}
						}
					*/
					/*
					if(data_pint>=14)
						{
							lcd_clean();
							memset(data_num,0,sizeof(data_num));
							data_pint=0;
						}
						else
						{
	
						
						if(num_ch>=0 && num_ch<=9)
							{
							data_num[data_pint]=num_ch;
							data_pint++;
							}
						}
					*/		
					
					put_time=0;
					if(refresh_flag==1)
						{
							printf("clean the lcd\n");
						//	lcd_clean();
							refresh_flag=0;
							memset(data_num,0,sizeof(data_num));
							data_pint=0;
						}
					
					
					
				}
			}
		 }
		else
			{
				printf("in the else \n");
				num_ch=num[data.code-1];
				if(data.value ==0)
					{
						if(num_ch == 10 )
							{
							
								 if(data.value==0)
			 						{
									buzzer_run(1);
									usleep(50000);
									buzzer_run(0);
									 }
							//jimmyb
							if(call_state==1)
							{
							for(f_num=0;f_num<2;f_num++)
								{
								printf("start wirte\n");
								printf("buff_4 is %d\n",buff_4[f_num]);
								fputc(buff_4[f_num],output);
								}
							call_state=0;
							timekeeping_start=0;
							
							if(times_set>0)
							{
							times_no_do=1;
							times_set=0;
								}
							//jimmy mark
							//sleep(2);
							call_occupy_flag=0;
							swich_gpio_set(0,0,1,1);
							}
							else //if(call_state==2 || call_state==3)
							 {
							 
							 ortp_mutex_lock(&prompt_mutex);
							 
							 strcpy(received_prompt,"terminate all");
							 //strcpy(received_prompt,"status register\0");
							 //   printf("Receiving command '%s'\n",received_prompt);kfflush(stdout);
							 have_prompt=TRUE;
							 ortp_mutex_unlock(&prompt_mutex);
								 call_state=0;
						
							 
							 }
							/*
							//jimmy +++ state 0??  tonghuazhong bug
								else 	{
									
									reset_lcd();
									
									light_run(0);
									swich_gpio_set(0,0,1,1);			

									//sleep(2); ???
									call_state=0;
									ins_to_flag=0;
									call_occupy_flag=0;
									state_monitor_flag=0;
							}
							*/
							

							}
						else if(num_ch == 11 )
						 	{
						 

						 
							 if(call_state==3)
							 {
							 if(data.value==0)
			 						{
									buzzer_run(1);
									usleep(50000);
									buzzer_run(0);
									 }
							 MIC_run(1);
							 ortp_mutex_lock(&prompt_mutex);
							 
							 strcpy(received_prompt,"answer");
							 //strcpy(received_prompt,"status register\0");
							 //   printf("Receiving command '%s'\n",received_prompt);kfflush(stdout);
							 have_prompt=TRUE;
							 ortp_mutex_unlock(&prompt_mutex);
								 call_state=2;
							 }

						 	}
					}

			}
	  }


		}
		else
		{
			num_ch=num[data.code-1];
			if(data.value ==0)
			{
				if(num_ch == 10 )
				{

					 if(data.value==0)
			 						{
									buzzer_run(1);
									usleep(50000);
									buzzer_run(0);
									 }

			if(call_state==2 || call_state==3)
				 {
				 ortp_mutex_lock(&prompt_mutex);
				 
				 strcpy(received_prompt,"terminate all");
				 //strcpy(received_prompt,"status register\0");
				 //   printf("Receiving command '%s'\n",received_prompt);kfflush(stdout);
				 have_prompt=TRUE;
				 ortp_mutex_unlock(&prompt_mutex);
					 call_state=0;
				 }



				}
				else if(num_ch == 11 )
				{
					 if(data.value==0)
			 						{
									buzzer_run(1);
									usleep(50000);
									buzzer_run(0);
									 }
				
				if(call_state==0)
				{
				if(call_occupy_flag==0)
					{
						call_occupy_flag=1;
						pthread_mutex_lock(&Device_mutex);
						lcd_clean() ;
						disp_hanzi(0x80,"    接通中    ");	
					  //disp_hanzi(0x80,"    通话中    ");			
					  fist_line=1;
					  pthread_mutex_unlock(&Device_mutex);
					  
						memset(tmp_call,0,sizeof(tmp_call));
														
						for(i=1;i<=glj_num;i++)
							{
							
							sprintf(tmp_call,"192.168.7.20%d",i);
							printf("tmp_call is %s\n",tmp_call);
							ret=call_glj_check(tmp_call);
							if(ret==0)
								{
									break;
								}
							}

							if(i==glj_num+1)
							{
								pthread_mutex_lock(&Device_mutex);
								lcd_clean() ;
								disp_hanzi(0x80,"占线中请稍候");	
								fist_line=1;
								pthread_mutex_unlock(&Device_mutex);
								buzzer_run(1);
								sleep(1);
								buzzer_run(0);
								sleep(1);

								buzzer_run(1);
								sleep(1);
								buzzer_run(0);
								//sleep(2);
								reset_lcd();
								call_occupy_flag=0;
							//	break; 
							goto START;
							}

															
						light_run(1);

						memset(tmp_call,0,sizeof(tmp_call));	
						sprintf(tmp_call,"call sip:192.168.7.20%d",i);
						printf("tmp_call is %s\n",tmp_call);
						
					
						swich_gpio_set(1,1,0,1);
						//jm
						MIC_run(0);
						ortp_mutex_lock(&prompt_mutex);
						
						strcpy(received_prompt,tmp_call);
						//strcpy(received_prompt,"status register\0");
						//	 printf("Receiving command '%s'\n",received_prompt);kfflush(stdout);
						have_prompt=TRUE;
						ortp_mutex_unlock(&prompt_mutex);
						time_keeping_fun(60);
						call_state=2;
						

						memset(data_num,0,sizeof(data_num));
						
					  data_pint=0;
					}
				}
				
				 if(call_state==3)
				 {
				 	 if(data.value==0)
						{
						buzzer_run(1);
						usleep(50000);
						buzzer_run(0);
						 }
					  MIC_run(1);
				 ortp_mutex_lock(&prompt_mutex);
				 
				 strcpy(received_prompt,"answer");
				 //strcpy(received_prompt,"status register\0");
				 //   printf("Receiving command '%s'\n",received_prompt);kfflush(stdout);
				 have_prompt=TRUE;
				 ortp_mutex_unlock(&prompt_mutex);
					 call_state=2;
				 }

				}
			}

		}
	 }


pthread_mutex_destroy(&Device_mutex);
return 0;

}
static void *pipe_thread(void*p){

watch_dog();

kpd_check();
//printf("in while 1\n");
//while(1);

}
#else

	void Ui_func()
		{
			printf("main Ui_func \n");
			//system("./mnt/nand1-1/mtest2");
			system("../nand1-1/mtest2 ");
			printf("main time_func \n");
		}



	void launch_ui_fun()
{
	pthread_t pid_ui;
//	func_set =func;


		
	printf("main launch_ui_fun \n");

	if (pthread_create(&pid_ui,NULL,(void*)Ui_func,NULL)!=0) {
				   printf("Create thread error!\n");
				   return ;
			   }
	pthread_detach(pid_ui);

}


static void *pipe_thread(void*p){

	char tmp[250];
	int bytes;
	int ret=-1;
		  LinphoneCall *call=NULL;
	  LinphoneCore *lc=NULL;
	 
	  
			//system("cd /mnt/nand1-1/");
			watch_dog();

			create_server_for_UI();
			
			//jimmy +++  launch ui 
			printf("main pipe_thread \n");
			launch_ui_fun();
			//system("cd /mnt/videodoor/");

			create_accept_for_UI();

			
			while(pipe_reader_run){
				/*
					while(client_sockfd!=ORTP_PIPE_INVALID){ 
#ifndef WIN32
					usleep(20000);
#else
					Sleep(20);
#endif
					}
                   
	*/			  
					memset(tmp,0,sizeof(tmp));
					if ((bytes = read (client_sockfd, (char *)tmp,sizeof(tmp)-1)) == -1) {
							perror ("read");
							//exit (EXIT_FAILURE);
					}
				if( (strstr(tmp, "answer_call")) != NULL)
					{
							 printf("call_id is %d\n",call_id);
							call=linphonec_get_call(call_id);
							if(call !=NULL)
								{
								printf("linphonec_get_call ok!\n");
								lc=call->core;
								if(lc !=NULL)
									{
										printf("call->core ok!\n");
										if (lc->ringstream!=NULL) {
												ms_message("stop ringing");
												ring_stop(lc->ringstream);
												ms_message("ring stopped");
												lc->ringstream=NULL;
												//was_ringing=TRUE;
											}
										if (call->ringing_beep){
												linphone_core_stop_dtmf(lc);
												call->ringing_beep=FALSE;
										 }
										memset(tmp,0,sizeof(tmp));

										linphone_call_start_audio_stream(call,"sip:root@192.168.7.201",0,0,0);
										//printf("from is %s\n",linphonec_get_caller());
										sscanf(linphonec_get_caller(),"%*[^@]@%[^>]",tmp);
										printf("tmp is %s\n",tmp);
										strcat(tmp," glj_get_call");
										//printf("tmp is %s\n",tmp);
										
										//printf("received_prompt is %s\n",received_prompt);

										
										
										   ortp_mutex_lock(&prompt_mutex);
											sprintf(received_prompt,"chat sip:%s",tmp);

											have_prompt=TRUE;
											ortp_mutex_unlock(&prompt_mutex);
										
									}
								else
									{
									printf("call->core no!\n");

									}
								}
							else
								{
								printf("linphonec_get_call no!\n");

								}
								
					}
				else
					{
			       ortp_mutex_lock(&prompt_mutex);
					tmp[bytes]='\0';
					strcpy(received_prompt,tmp);
					//strcpy(received_prompt,"status register\0");
					//	 printf("Receiving command '%s'\n",received_prompt);kfflush(stdout);
					have_prompt=TRUE;
					ortp_mutex_unlock(&prompt_mutex);
					
				/*
				if(start_voip)
					{
					
					//Draw_Ui(1);
					//sleep(1); 
					
					ortp_mutex_lock(&prompt_mutex);
					//tmp[bytes]='\0';
				    //strcpy(received_prompt,tmp);
					strcpy(received_prompt,"register sip:001001@192.168.0.240 sip:192.168.0.240 abc123\0");
					//   printf("Receiving command '%s'\n",received_prompt);kfflush(stdout);
					have_prompt=TRUE;
					ortp_mutex_unlock(&prompt_mutex);

					sleep(1); 

					
					ortp_mutex_lock(&prompt_mutex);
					//tmp[bytes]='\0';
					//strcpy(received_prompt,tmp);
					strcpy(received_prompt,"status register\0");
					//	 printf("Receiving command '%s'\n",received_prompt);kfflush(stdout);
					have_prompt=TRUE;
					ortp_mutex_unlock(&prompt_mutex);


					sleep(1); 

				    ortp_mutex_lock(&prompt_mutex);
					//tmp[bytes]='\0';
					//strcpy(received_prompt,tmp);
					strcpy(received_prompt,"autoanswer enable\0");
					//	 printf("Receiving command '%s'\n",received_prompt);kfflush(stdout);
					have_prompt=TRUE;
					ortp_mutex_unlock(&prompt_mutex);

					start_voip=FALSE;

				    }
				
					if((ret=button_one_check()) == 0 || (ret=button_two_check()) == 0)
					{
					
					ortp_mutex_lock(&prompt_mutex);
					//tmp[bytes]='\0';
					//strcpy(received_prompt,tmp);
					strcpy(received_prompt,"call sip:001000@192.168.0.240\0");
				 //   printf("Receiving command '%s'\n",received_prompt);kfflush(stdout);
					have_prompt=TRUE;
					ortp_mutex_unlock(&prompt_mutex);
					
					}*/
					sleep(1); 			
					printf("Receiving command '%s'\n",tmp);fflush(stdout);
					}
		}
	ms_message("Exiting pipe_reader_thread.");
	fflush(stdout);
	return NULL;

}

#endif

static void start_pipe_reader(void){
	ms_mutex_init(&prompt_mutex,NULL);
	pipe_reader_run=TRUE;
	ortp_thread_create(&pipe_reader_th,NULL,pipe_thread,NULL);
}

static void stop_pipe_reader(void){
	pipe_reader_run=FALSE;
	linphonec_command_finished();
	close (server_sockfd);
	close (client_sockfd);
    unlink ("/tmp/server_socket");
	//ortp_server_pipe_close(server_sock);
	ortp_thread_join(pipe_reader_th,NULL);
}
#endif /*_WIN32_WCE*/

#ifdef HAVE_READLINE
#define BOOL_HAVE_READLINE 1
#else
#define BOOL_HAVE_READLINE 0
#endif


char *linphonec_readline(char *prompt){ 
    // printf("linphonec_readline    ~~~\n");
	if (1){
	//if (unix_socket || !BOOL_HAVE_READLINE ){
		static bool_t prompt_reader_started=FALSE;
		static bool_t pipe_reader_started=FALSE;

		if (!prompt_reader_started){
			start_prompt_reader();
			prompt_reader_started=TRUE;
		}
		//printf("pipe_reader_started 111\n");
		if (unix_socket && !pipe_reader_started){
#if !defined(_WIN32_WCE)
			printf("pipe_reader_started222\n");

			start_pipe_reader();
			pipe_reader_started=TRUE;
#endif /*_WIN32_WCE*/
		}
		//printf("pipe_reader_started333\n");
		fprintf(stdout,"%s",prompt);
		fflush(stdout);
		while(1){
			ms_mutex_lock(&prompt_mutex);
			if (have_prompt){
				char *ret=strdup(received_prompt);
				printf("ret is %s\n",ret);
				have_prompt=FALSE;
				ms_mutex_unlock(&prompt_mutex);
				return ret;
			}
			ms_mutex_unlock(&prompt_mutex);
			linphonec_idle_call();
#ifdef WIN32
			Sleep(20);
			/* Following is to get the video window going as it
				 should. Maybe should we only have this on when the option -V
				 or -D is on? */
			MSG msg;
	
			if (PeekMessage(&msg, NULL, 0, 0,1)) {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
#else
			usleep(20000);
#endif
		}
		printf("pipe_reader_started444\n");
	}else{
#ifdef HAVE_READLINE
		char* ret=readline(prompt);
 //       printf("ret is %s",ret);
		return ret;
#endif
	}
}
//jimmy linphonec_out
#ifdef Doorphone_compile
void linphonec_out(const char *fmt,...){
	char *res;
	va_list args;
	va_start (args, fmt);
	res=ortp_strdup_vprintf(fmt,args);
	va_end (args);
	printf("%s",res);
	fflush(stdout);
#if !defined(_WIN32_WCE)
	if (client_sock!=ORTP_PIPE_INVALID){
		if (ortp_pipe_write(client_sock,(uint8_t*)res,strlen(res))==-1){
			fprintf(stderr,"Fail to send output via pipe: %s",strerror(errno));
		}
	}
#endif 
	ortp_free(res);
}

#else

void linphonec_out(const char *fmt,...){
	//printf("linphonec_out!!!\n");
/*
	char *res;
	va_list args;
	va_start (args, fmt);
	res=ortp_strdup_vprintf(fmt,args);
	va_end (args);
//	printf("%s",res);
//	printf("linphonec_out!!!\n");
	fflush(stdout);
*/
		printf("linphonec_out is %s\n",fmt);

#if !defined(_WIN32_WCE)
	/*if (client_sock!=ORTP_PIPE_INVALID){
		
		if (ortp_pipe_write(client_sock,(uint8_t*)res,strlen(res))==-1){
			fprintf(stderr,"Fail to send output via pipe: %s",strerror(errno));
		}
		
	}*/
	//printf("res is %s\n",res);
	if (client_sockfd!=ORTP_PIPE_INVALID){
		printf("linphonec_out client_sockfd =%d\n",client_sockfd);
	     if (write(client_sockfd, (char *)fmt, strlen(fmt)) == -1) {
	            perror ("write");
	           // exit (EXIT_FAILURE);
	        }
		}
	
#endif 
//	ortp_free(res);


}
#endif 

/*
 * Linphone core callback
 */
 //jimmy chat  recive
static void
linphonec_text_received(LinphoneCore *lc, LinphoneChatRoom *cr,
		const LinphoneAddress *from, const char *msg)
{
	char * pHeadPtr;
	char num_buf[3];
	char ip_buf[20];
	int mount=0 ,day=0,i=0;
	int num=0;
	int foor_shi,foor_ge,hu_shi,hu_ge;
	char * ip_point;
	FILE* fp_logs;
	char log_file_name[30];
	int alarm_num=0;
	char *fglj_point;
	
	char fglj_to_buf[10]={0};//call 
	//char fglj_to_buf_2[10]={0};//call 
	char fglj_men[3]={0};//call 
	char fglj_room[3]={0};//call 
	int fglj_wqj_flag=0;
	int mts_wqj_flag=0;
	int door_alarm_flag=0;
	static	int no_rec_flag=0;
	char wqj_buf_call[50];
	char wqj_num_rec;
	 LinphoneCall *call_mkj=NULL;
	 char *from_url=NULL;
	//jimmy ??
	memset(buf_print,0,sizeof(buf_print));
	sprintf(buf_print,"Message received from %s: %s\n", linphone_address_as_string(from), msg);
	linphonec_out(buf_print);

	// TODO: provide mechanism for answering.. ('say' command?)
	//printf("msg is %s\n\n", msg);
	//printf("linphonec_text_received\n");
	#ifdef Doorphone_compile
	/*
	if( (pHeadPtr=strstr(msg, "call_logs_get ")) != NULL)
		{
		   	pHeadPtr += strlen("call_logs_get ");
			while(*pHeadPtr>='0' && *pHeadPtr<='9')
				{
					num_buf[i]=*pHeadPtr;
					pHeadPtr++;
					i++;
			    }
			num_buf[i]='\0';
			mount=atoi(num_buf);
			i=0;
            pHeadPtr++;
			
			while(*pHeadPtr>='0' && *pHeadPtr<='9')
				{
					num_buf[i]=*pHeadPtr;
					pHeadPtr++;
					i++;
			    }
			num_buf[i]='\0';
			day=atoi(num_buf);
			printf("mount %d  day %d\n",mount,day);
			i=0;

			printf("linphone_address %s\n",linphone_address_as_string(from));
			pHeadPtr=strstr(linphone_address_as_string(from), "@");
			pHeadPtr++;
			while(*pHeadPtr !='>' && *pHeadPtr !='\0')
				{
					ip_buf[i]=*pHeadPtr;
					i++;
					pHeadPtr++;
			    }
			ip_buf[i]='\0';
			
			printf("ip_buf %s\n",ip_buf);
			call_logs_get_func(mount,day,ip_buf);

		}
	else*/ if( (pHeadPtr=strstr(msg, "call_ins_command")) != NULL)
		{
		call_ins_flag=1;
		ip_point=linphone_address_as_string(from);
		printf("ip_point is %s",ip_point);
		printf("msg is %s",msg);
		for(i=0;i<3;i++)
			{
				while(*ip_point!='.')
					{
					ip_point++;

					}
				ip_point++;
				
			}

		if(ip_point[1] == '>')
			{

			
		}
		else if(ip_point[2] == '>')
			{

			
			}
		else
			{
			
			
			 if(ip_point[0]=='2' && ip_point[1]=='1')
			 	{
			  		wqj_num=ip_point[2];
			 	}

			}
					
		if(call_occupy_flag==0)
			{
			
			sleep(1);
			 sscanf(msg,"call_ins_command,%d,%d,%d,%d",&foor_shi,&foor_ge,&hu_shi,&hu_ge);
			 
			 buff_2[1]=foor_shi*16+foor_ge;
			 
			 if(buff_2[1]==0x11)
			{
			buff_2[1]=0x91;
			}
		else if(buff_2[1]==0x13)
			{
			buff_2[1]=0x93;
			}
			 buff_2[2]=hu_shi*16+hu_ge;
			 if(buff_2[2]==0x11)
			{
			buff_2[2]=0x91;
			}
		else if(buff_2[2]==0x13)
			{
			buff_2[2]=0x93;
			} 
		if(vice_used==0)
			{
			
			 for(num=0;num<5;num++)
			{
			printf("start wirte\n");
			printf("buff_2 is %d\n",buff_2[num]);
			fputc(buff_2[num],output);
			}

			rec_bb_flag=1;

			sleep(1);
			if(rec_bb_flag==2)
				{
			
			  rec_bb_flag=0;
			  swich_gpio_set(0,1,1,1);

			  memset(data_num,0,sizeof(data_num));
			  pthread_mutex_lock(&Device_mutex);
			  lcd_clean() ;
			  disp_hanzi(0x80,"占用中请稍候");	
			  fist_line=1;
			  pthread_mutex_unlock(&Device_mutex);
			  data_pint=0;
			  time_keeping_fun(60);
				}
			else
				{
				 sleep(3);
				 ortp_mutex_lock(&prompt_mutex);
				 
				 strcpy(received_prompt,"terminate all");
				 //strcpy(received_prompt,"status register\0");
				 //   printf("Receiving command '%s'\n",received_prompt);kfflush(stdout);
				 have_prompt=TRUE;
				 ortp_mutex_unlock(&prompt_mutex);
				rec_bb_flag=0;
				//break;

			}


									
			
			}
	
			}
		}
		else if( (pHeadPtr=strstr(msg, "glj_open_door")) != NULL)
		{
		// if(call_state==2 || ( (call_ins_flag!=1) && (ins_to_flag!=1) ))
		if((call_ins_flag!=1) && (ins_to_flag!=1) )
		 	{
				lock_open_precess();

					 ortp_mutex_lock(&prompt_mutex);
								
					//strcpy(received_prompt,"chat sip:192.168.7.201 ins_call_command %c,%c",buff[1],buff[2]);
					sprintf(received_prompt,"chat sip:192.168.7.201 answer_glj_open");
					//strcpy(received_prompt,"status register\0");
					//	 printf("Receiving command '%s'\n",received_prompt);kfflush(stdout);
					have_prompt=TRUE;
					ortp_mutex_unlock(&prompt_mutex);
		 	}
		}
		else if( (pHeadPtr=strstr(msg, "glj_get_call")) != NULL)
			{
							 printf("call_id is %d\n",call_id);
							call_mkj=linphonec_get_call(call_id);
							if(call_mkj !=NULL)
								{
								printf("linphonec_get_call ok!\n");
								//lc=call->core;
								if(lc !=NULL)
									{
										printf("call->core ok!\n");
										if (lc->ringstream!=NULL) {
												ms_message("stop ringing");
												ring_stop(lc->ringstream);
												ms_message("ring stopped");
												lc->ringstream=NULL;
												//was_ringing=TRUE;
											}
										if (call_mkj->ringing_beep){
												linphone_core_stop_dtmf(lc);
												call_mkj->ringing_beep=FALSE;
										 }
	
										
									   from_url=linphone_address_as_string(call_mkj->log->from);
									   printf("from_url is %s\n",from_url);

										//linphone_call_start_audio_stream(call_mkj,"sip:root@192.168.7.121",0,0,0);
										linphone_call_start_audio_stream(call_mkj,from_url,0,0,0);
										MIC_run(1);
										//printf("from is %s\n",linphonec_get_caller());
										
										
									}
								else
									{
									printf("call->core no!\n");

									}
								}
							else
								{
								printf("linphonec_get_call no!\n");

								}
								
					}
	#endif

	if( (pHeadPtr=strstr(msg, "ins_call_command")) != NULL)
		{
			sscanf(msg,"ins_call_command %c,%c",&buff_rec[0],&buff_rec[1]);
			if(buff_rec[0]==0x93)
				{
				buff_rec[0]=0x13;

				}
			else if(buff_rec[0]==0x91)
				{
				buff_rec[0]=0x11;

				}

				if(buff_rec[1]==0x93)
				{
				buff_rec[1]=0x13;

				}
			else if(buff_rec[1]==0x91)
				{
				buff_rec[1]=0x11;

				}
			printf("%02x,%02x",buff_rec[0],buff_rec[1]);
			ins_call_flag=1;
			
			
		}
	else if( (pHeadPtr=strstr(msg, "fglj_call")) != NULL)
		{
				fglj_point=pHeadPtr;
				for(i=0;i<3;i++)
				{
					while(*fglj_point!='.' && *fglj_point !='\0')
						{
						fglj_point++;
		
						}
					
					fglj_point++;
					
					if(i==1)
					{
					fglj_mkj_num=*fglj_point-'0';

					}
					//DEBUG("pHeadPtr is %c\n",*pHeadPtr);
				}
		
			if(fglj_point[1] == '>')
				{

				printf("from is %c %c \n",fglj_point[0],fglj_point[1]);
				fglj_men[0]=0x00;
				fglj_men[1]=fglj_point[0]-'0';
				printf("%02x,%02x\n",fglj_men[0],fglj_men[1]);
				fglj_point=fglj_point+2;
			}
			else if(fglj_point[2] == '>')
				{
				printf("from is %c %c %c \n",fglj_point[0],fglj_point[1],fglj_point[2]);
				fglj_men[0]=0x00;
				fglj_men[1]=(fglj_point[0]-'0')*16+fglj_point[1]-'0';
				printf("%02x,%02x\n",fglj_men[0],fglj_men[1]);
				fglj_point=fglj_point+3;
			}
			else
				{
				
				
				 printf("from is %c %c %c \n",fglj_point[0],fglj_point[1],fglj_point[2]);
				 fglj_men[0]=fglj_point[0]-'0';
				 fglj_men[1]=(fglj_point[1]-'0')*16+fglj_point[2]-'0';
				
				 printf("%02x,%02x\n",fglj_men[0],fglj_men[1]);
				 if(fglj_point[0]=='2' && fglj_point[1]=='1')
					{
					 fglj_wqj_flag=fglj_point[2]-'0';
					}
				 fglj_point=fglj_point+4;
				

				}
			printf("fglj_wqj_flag is %d\n",fglj_wqj_flag);
			if(*fglj_point==',')
				{
				
				fglj_room[0]=(fglj_point[1]-'0')*16+(fglj_point[2]-'0');
				fglj_room[1]=(fglj_point[4]-'0')*16+(fglj_point[5]-'0');
				printf("%c%c%c%c%c%c%c%c%c%c\n",0xAF,0xAF,0xAF,0xAF,fglj_men[0],fglj_men[1],fglj_room[0],fglj_room[1],0xAA,0XFF);
				//printf("%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x\n",0xAF,0xAF,0xAF,0xAF,fglj_men[0],fglj_men[1],fglj_room[0],fglj_room[1],0xAA,0XFF);	
				}
			else
				{
				if(fglj_mkj_num==7)
					{
					if(fglj_wqj_flag>0)
						{
					printf("%c%c%c%c%c%c%c%c%c%c\n",0xAF,0xAF,0xAF,0xAF,fglj_men[0],fglj_men[1],0x00,0x00,0xAA,0xA0+fglj_wqj_flag);
				//	printf("%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x\n",0xAF,0xAF,0xAF,0xAF,fglj_men[0],fglj_men[1],0x00,0x00,0xAA,0xA0+fglj_wqj_flag);
					fglj_wqj_flag=0;
						}
					else
						{
					printf("%c%c%c%c%c%c%c%c%c%c\n",0xAF,0xAF,0xAF,0xAF,fglj_men[0],fglj_men[1],0x00,0x00,0xAA,0XFF);
				//	printf("%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x\n",0xAF,0xAF,0xAF,0xAF,fglj_men[0],fglj_men[1],0x00,0x00,0xAA,0XFF);

						}
					}
				else if(fglj_mkj_num==8)
					{
					printf("%c%c%c%c%c%c%c%c%c%c\n",0xAF,0xAF,0xAF,0xAF,fglj_men[0],fglj_men[1],0x00,0x00,0xAA,0X01);
				//	printf("%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x\n",0xAF,0xAF,0xAF,0xAF,fglj_men[0],fglj_men[1],0x00,0x00,0xAA,0X01);

					}
				else if(fglj_mkj_num==9)
					{
					printf("%c%c%c%c%c%c%c%c%c%c\n",0xAF,0xAF,0xAF,0xAF,fglj_men[0],fglj_men[1],0x00,0x00,0xAA,0X02);
				//	printf("%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x\n",0xAF,0xAF,0xAF,0xAF,fglj_men[0],fglj_men[1],0x00,0x00,0xAA,0X02);

					}
				}


			
			
		}
	else if( (pHeadPtr=strstr(msg, "fglj_to_men")) != NULL)
		{
			
		
			sscanf(msg,"fglj_to_men <%s>",fglj_to_buf);
			printf("fglj_to_buf is %s\n",fglj_to_buf);
			
			
			 fglj_men[0]=fglj_to_buf[0]-'0';
			 fglj_men[1]=(fglj_to_buf[1]-'0')*16+fglj_to_buf[2]-'0';
			 printf("%c%c%c%c%c%c%c%c%c%c\n",0xAF,0xAF,0xAF,0xAF,fglj_men[0],fglj_men[1],0x00,0x00,0xBB,0XFF);
		   //  printf("%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x\n",0xAF,0xAF,0xAF,0xAF,fglj_men[0],fglj_men[1],0x00,0x00,0xBB,0XFF);


			
			
		}
	else if( (pHeadPtr=strstr(msg, "fglj_to_sn")) != NULL)
		{
				sscanf(msg,"fglj_to_sn <%s,%s>",fglj_to_buf);
			printf("fglj_to_buf_2 is %s\n",fglj_to_buf);
			
			
			 fglj_men[0]=fglj_to_buf[0]-'0';
			 fglj_men[1]=(fglj_to_buf[1]-'0')*16+fglj_to_buf[2]-'0';

			 fglj_room[0]=(fglj_to_buf[4]-'0')*16+fglj_to_buf[5]-'0';
			 fglj_room[1]=(fglj_to_buf[6]-'0')*16+fglj_to_buf[7]-'0';
			 printf("%c%c%c%c%c%c%c%c%c%c\n",0xAF,0xAF,0xAF,0xAF,fglj_men[0],fglj_men[1],fglj_room[0],fglj_room[1],0xBB,0XFF);
		    // printf("%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x\n",0xAF,0xAF,0xAF,0xAF,fglj_men[0],fglj_men[1],fglj_room[0],fglj_room[1],0xBB,0XFF);
			
			
		}
	else if( (pHeadPtr=strstr(msg, "men_to_sn")) != NULL)
		{
		
		printf("men_to_sn\n");
		
		       ip_point=linphone_address_as_string(from);
			   printf("ip_point is %s",ip_point);
			   printf("msg is %s",msg);
				for(i=0;i<3;i++)
						{
							while(*ip_point!='.')
								{
								ip_point++;

								}
							ip_point++;
							if(i==1)
							{
							mts_mkj_num=*ip_point-'0';

							}
						}

				if(ip_point[1] == '>')
						{

						printf("from is %c %c \n",ip_point[0],ip_point[1]);
						buff_men_call_sn_men[0]=0x00;
						buff_men_call_sn_men[1]=ip_point[0]-'0';
						printf("%02x,%02x\n",buff_men_call_sn_men[0],buff_men_call_sn_men[1]);
						sprintf(log_file_name,"/mnt/nand1-3/00%c\0",ip_point[0]);
					}
					else if(ip_point[2] == '>')
						{
						printf("from is %c %c %c \n",ip_point[0],ip_point[1],ip_point[2]);
						buff_men_call_sn_men[0]=0x00;
						buff_men_call_sn_men[1]=(ip_point[0]-'0')*16+ip_point[1]-'0';
						printf("%02x,%02x\n",buff_men_call_sn_men[0],buff_men_call_sn_men[1]);
						sprintf(log_file_name,"/mnt/nand1-3/0%c%c\0",ip_point[0],ip_point[1]);

						
						}
					else
						{
						
						
						  printf("from is %c %c %c \n",ip_point[0],ip_point[1],ip_point[2]);
						buff_men_call_sn_men[0]=ip_point[0]-'0';
						 buff_men_call_sn_men[1]=(ip_point[1]-'0')*16+ip_point[2]-'0';
						
						 printf("%02x,%02x\n",buff_men_call_sn_men[0],buff_men_call_sn_men[1]);
						 sprintf(log_file_name,"/mnt/nand1-3/%c%c%c\0",ip_point[0],ip_point[1],ip_point[2]);
						 if(ip_point[0]=='2' && ip_point[1]=='1')
						 	{
						 mts_wqj_flag=ip_point[2]-'0';
							wgj_call_sn_num=mts_wqj_flag;
						 	}

						}

		if(mts_wqj_flag>0)
		{
			sscanf(msg,"men_to_sn %c,%c,%c,%c",&buff_men_call_sn_men[0],&buff_men_call_sn_men[1],&buff_men_call_sn[0],&buff_men_call_sn[1]);
				//buff_men_call_sn
					if(buff_men_call_sn[0]==0x93)
						{
						buff_men_call_sn[0]=0x13;
			
						}
					else if(buff_men_call_sn[0]==0x91)
						{
						buff_men_call_sn[0]=0x11;
			
						}
			
						if(buff_men_call_sn[1]==0x93)
						{
						buff_men_call_sn[1]=0x13;
			
						}
					else if(buff_men_call_sn[1]==0x91)
						{
						buff_men_call_sn[1]=0x11;
						}
					//wgj_call_sn_falg=1;
					
					wgj_call_sn_open[0]=buff_men_call_sn_men[0];
					if(wgj_call_sn_open[0]==0x30)
								{
								wgj_call_sn_open[0]=0x00;

								}
					wgj_call_sn_open[1]=buff_men_call_sn_men[1];

		}
	else
		{
			sscanf(msg,"men_to_sn %c,%c",&buff_men_call_sn[0],&buff_men_call_sn[1]);
		//buff_men_call_sn
			if(buff_men_call_sn[0]==0x93)
				{
				buff_men_call_sn[0]=0x13;

				}
			else if(buff_men_call_sn[0]==0x91)
				{
				buff_men_call_sn[0]=0x11;

				}

				if(buff_men_call_sn[1]==0x93)
				{
				buff_men_call_sn[1]=0x13;

				}
			else if(buff_men_call_sn[1]==0x91)
				{
				buff_men_call_sn[1]=0x11;
				}

		}
					if(no_rec_flag!=1 || mts_wqj_flag>0)
					{
					if(mts_mkj_num==7)
						{
						if(mts_wqj_flag>0)
						{
							if(buff_men_call_sn_men[0]==0x30)
								{
								buff_men_call_sn_men[0]=0x00;

								}
							printf("%c%c%c%c%c%c%c%c%c%c\n",0xAF,0xAF,0xAF,0xAF,buff_men_call_sn_men[0],buff_men_call_sn_men[1],buff_men_call_sn[0],buff_men_call_sn[1],0xAB,0xA0+mts_wqj_flag); 
							//printf("%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x\n",0xAF,0xAF,0xAF,0xAF,buff_men_call_sn_men[0],buff_men_call_sn_men[1],buff_men_call_sn[0],buff_men_call_sn[1],0xAB,0xA0+mts_wqj_flag);
							no_rec_flag=1;

						}
						else
							{
							printf("%c%c%c%c%c%c%c%c%c%c\n",0xAF,0xAF,0xAF,0xAF,buff_men_call_sn_men[0],buff_men_call_sn_men[1],buff_men_call_sn[0],buff_men_call_sn[1],0xAB,0XFF);	
							//printf("%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x\n",0xAF,0xAF,0xAF,0xAF,buff_men_call_sn_men[0],buff_men_call_sn_men[1],buff_men_call_sn[0],buff_men_call_sn[1],0xAB,0XFF);
							}
					}
					else 	if(mts_mkj_num==8)
						{
						printf("%c%c%c%c%c%c%c%c%c%c\n",0xAF,0xAF,0xAF,0xAF,buff_men_call_sn_men[0],buff_men_call_sn_men[1],buff_men_call_sn[0],buff_men_call_sn[1],0xAB,0X01);	
						//printf("%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x\n",0xAF,0xAF,0xAF,0xAF,buff_men_call_sn_men[0],buff_men_call_sn_men[1],buff_men_call_sn[0],buff_men_call_sn[1],0xAB,0X01);
						}

					else 	if(mts_mkj_num==9)
						{
						printf("%c%c%c%c%c%c%c%c%c%c\n",0xAF,0xAF,0xAF,0xAF,buff_men_call_sn_men[0],buff_men_call_sn_men[1],buff_men_call_sn[0],buff_men_call_sn[1],0xAB,0X02);	
						//printf("%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x\n",0xAF,0xAF,0xAF,0xAF,buff_men_call_sn_men[0],buff_men_call_sn_men[1],buff_men_call_sn[0],buff_men_call_sn[1],0xAB,0X02);
						}
					
					
				cur_time(buf_showtime);
				printf("buf_showtime is %s",buf_showtime);
				printf("log_file_name is %s",log_file_name+13);
				//if((fp_logs=fopen("/mnt/nand1-1/121","a+"))==NULL)
				if((fp_logs=fopen(log_file_name,"a+"))==NULL)
				{
				printf("can not open the file.\n");
			//	return -1;
				}
				else
					{
				fprintf(fp_logs,"%s 门口机%s 拨打 室内机%c%c%c%c\n",buf_showtime,log_file_name+13,buff_men_call_sn[0]/16+'0',buff_men_call_sn[0]%16+'0',buff_men_call_sn[1]/16+'0',buff_men_call_sn[1]%16+'0');
				fclose (fp_logs);
					}
			}
			else
				{
				no_rec_flag=0;

			}
		
		}
		else if( (pHeadPtr=strstr(msg, "mkj_open_door")) != NULL)
		{
			printf("mkj_open_door\n");
			 ip_point=linphone_address_as_string(from);
					   printf("ip_point is %s",ip_point);
					   
						for(i=0;i<3;i++)
								{
									while(*ip_point!='.')
										{
										ip_point++;
		
										}
									ip_point++;
								}
		
						if(ip_point[1] == '>')
								{
		
								printf("from is %c %c \n",ip_point[0],ip_point[1]);
								buff_mkj_open_door[0]=0x00;
								buff_mkj_open_door[1]=ip_point[0]-'0';
								printf("%02x,%02x\n",buff_mkj_open_door[0],buff_mkj_open_door[1]);
							}
							else if(ip_point[2] == '>')
								{
								printf("from is %c %c %c \n",ip_point[0],ip_point[1],ip_point[2]);
								buff_mkj_open_door[0]=0x00;
								buff_mkj_open_door[1]=(ip_point[0]-'0')*16+ip_point[1]-'0';
								printf("%02x,%02x\n",buff_mkj_open_door[0],buff_mkj_open_door[1]);
		
								
								}
							else
								{
								
								
								  printf("from is %c %c %c \n",ip_point[0],ip_point[1],ip_point[2]);
								buff_mkj_open_door[0]=ip_point[0]-'0';
								 buff_mkj_open_door[1]=(ip_point[1]-'0')*16+ip_point[2]-'0';
								
								 printf("%02x,%02x\n",buff_mkj_open_door[0],buff_mkj_open_door[1]);
		
								}
			 printf("%c%c%c%c%c%c%c%c%c%c\n",0xAF,0xAF,0xAF,0xAF,buff_mkj_open_door[0],buff_mkj_open_door[1],0x00,0x00,0xEE,0XFF);	
			 
		}
		
		else if( (pHeadPtr=strstr(msg, "door_alarm")) != NULL)
		{
			printf("door_alarm\n");
			 ip_point=linphone_address_as_string(from);
					   printf("ip_point is %s",ip_point);
					   
						for(i=0;i<3;i++)
								{
									while(*ip_point!='.')
										{
										ip_point++;
		
										}
									ip_point++;
									if(i==1)
									{
									alarm_num=*ip_point-'0';

									}
								}
						i=0;
						// printf("alarm_num is %d\n",alarm_num);
						if(ip_point[1] == '>')
								{
		
								printf("from is %c %c \n",ip_point[0],ip_point[1]);
								buff_alarm[0]=0x00;
								buff_alarm[1]=ip_point[0]-'0';
								printf("%02x,%02x\n",buff_alarm[0],buff_alarm[1]);
							}
							else if(ip_point[2] == '>')
								{
								printf("from is %c %c %c \n",ip_point[0],ip_point[1],ip_point[2]);
								buff_alarm[0]=0x00;
								buff_alarm[1]=(ip_point[0]-'0')*16+ip_point[1]-'0';
								printf("%02x,%02x\n",buff_alarm[0],buff_alarm[1]);
		
								
								}
							else
								{
								
								
								  printf("from is %c %c %c \n",ip_point[0],ip_point[1],ip_point[2]);
								buff_alarm[0]=ip_point[0]-'0';
								 buff_alarm[1]=(ip_point[1]-'0')*16+ip_point[2]-'0';
								
								 printf("%02x,%02x\n",buff_alarm[0],buff_alarm[1]);


									if(ip_point[0]=='2' && ip_point[1]=='1')
									{
									door_alarm_flag=ip_point[2]-'0';
									
									}

		
								}
				if(alarm_num==7)
				 	{
				 	if(door_alarm_flag>0)
				 		{
				 		printf("%c%c%c%c%c%c%c%c%c%c\n",0xAF,0xAF,0xAF,0xAF,buff_alarm[0],buff_alarm[1],0x00,0x00,0x09,0XA0+door_alarm_flag);	
						//printf("%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x\n",0xAF,0xAF,0xAF,0xAF,buff_alarm[0],buff_alarm[1],0xFF,0xFF,0x09,0XA0+door_alarm_flag);	
							 	
						door_alarm_flag=0;
						}
					else
						{
						printf("%c%c%c%c%c%c%c%c%c%c\n",0xAF,0xAF,0xAF,0xAF,buff_alarm[0],buff_alarm[1],0x00,0x00,0x09,0xFF);	
						}
				 	}
				 else if(alarm_num==8)
				 	{
					printf("%c%c%c%c%c%c%c%c%c%c\n",0xAF,0xAF,0xAF,0xAF,buff_alarm[0],buff_alarm[1],0x00,0x00,0x09,0X01);	
				 	}
				 else if(alarm_num==9)
				 	{
					printf("%c%c%c%c%c%c%c%c%c%c\n",0xAF,0xAF,0xAF,0xAF,buff_alarm[0],buff_alarm[1],0x00,0x00,0x09,0X02);	
				 	}
				

		}
	else if( (pHeadPtr=strstr(msg, "ins_open_door")) != NULL)
		{
				printf("ins_open_door\n");
				if( (pHeadPtr=strstr(msg, "ins_open_door_wqj")) != NULL)
						{
						printf("ins_open_door wqj\n");
						sscanf(msg,"ins_open_door_wqj %c,%c,%c",&buff_open_door[0],&buff_open_door[1],&wqj_num_rec);
						wgj_call_sn_falg=1;
						}
					
				else
					{
				
				sscanf(msg,"ins_open_door %c,%c",&buff_open_door[0],&buff_open_door[1]);
				//buff_men_call_sn
					
					}
				if(buff_open_door[0]==0x93)
						{
						buff_open_door[0]=0x13;
		
						}
					else if(buff_open_door[0]==0x91)
						{
						buff_open_door[0]=0x11;
		
						}
		
						if(buff_open_door[1]==0x93)
						{
						buff_open_door[1]=0x13;
		
						}
					else if(buff_open_door[1]==0x91)
						{
						buff_open_door[1]=0x11;
						}
					   ip_point=linphone_address_as_string(from);
					   printf("ip_point is %s",ip_point);
					   
						for(i=0;i<3;i++)
								{
									while(*ip_point!='.')
										{
										ip_point++;
		
										}
									ip_point++;
									if(i==1)
									{
									alarm_num=*ip_point-'0';

									}
								}
		
						if(ip_point[1] == '>')
								{
		
								printf("from is %c %c \n",ip_point[0],ip_point[1]);
								buff_open_door_men[0]=0x00;
								buff_open_door_men[1]=ip_point[0]-'0';
								printf("%02x,%02x\n",buff_open_door_men[0],buff_open_door_men[1]);
							}
							else if(ip_point[2] == '>')
								{
								printf("from is %c %c %c \n",ip_point[0],ip_point[1],ip_point[2]);
								buff_open_door_men[0]=0x00;
								buff_open_door_men[1]=(ip_point[0]-'0')*16+ip_point[1]-'0';
								printf("%02x,%02x\n",buff_open_door_men[0],buff_open_door_men[1]);
		
								
								}
							else
								{
								
								
								  printf("from is %c %c %c \n",ip_point[0],ip_point[1],ip_point[2]);
								buff_open_door_men[0]=ip_point[0]-'0';
								 buff_open_door_men[1]=(ip_point[1]-'0')*16+ip_point[2]-'0';
								
								 printf("%02x,%02x\n",buff_open_door_men[0],buff_open_door_men[1]);
		
								}
							
							if(wgj_call_sn_falg==1 )
								{
								
								 printf("%c%c%c%c%c%c%c%c%c%c\n",0xAF,0xAF,0xAF,0xAF,buff_open_door_men[0],buff_open_door_men[1],buff_open_door[0],buff_open_door[1],0xDD,0XA0+(wqj_num_rec-'0'));
								 //printf("%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x\n",0xAF,0xAF,0xAF,0xAF,buff_open_door_men[0],buff_open_door_men[1],buff_open_door[0],buff_open_door[1],0xDD,0XA0+wgj_call_sn_num);

								wgj_call_sn_falg=0;

								 
								}
							else
								{
									if(alarm_num==7)
							 	{
											 printf("%c%c%c%c%c%c%c%c%c%c\n",0xAF,0xAF,0xAF,0xAF,buff_open_door_men[0],buff_open_door_men[1],buff_open_door[0],buff_open_door[1],0xDD,0XFF);	
											// printf("%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x\n",0xAF,0xAF,0xAF,0xAF,buff_open_door_men[0],buff_open_door_men[1],buff_open_door[0],buff_open_door[1],0xDD,0XFF);	
							 	}
							 else if(alarm_num==8)
							 	{
											printf("%c%c%c%c%c%c%c%c%c%c\n",0xAF,0xAF,0xAF,0xAF,buff_open_door_men[0],buff_open_door_men[1],buff_open_door[0],buff_open_door[1],0xDD,0X01);	
											// printf("%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x\n",0xAF,0xAF,0xAF,0xAF,buff_open_door_men[0],buff_open_door_men[1],buff_open_door[0],buff_open_door[1],0xDD,0X01);
							 	}
							 else if(alarm_num==9)
							 	{
											printf("%c%c%c%c%c%c%c%c%c%c\n",0xAF,0xAF,0xAF,0xAF,buff_open_door_men[0],buff_open_door_men[1],buff_open_door[0],buff_open_door[1],0xDD,0X02);	
											// printf("%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x\n",0xAF,0xAF,0xAF,0xAF,buff_open_door_men[0],buff_open_door_men[1],buff_open_door[0],buff_open_door[1],0xDD,0X02);	
							 	}

								}
				}
		else if( (pHeadPtr=strstr(msg, "answer_glj_open")) != NULL)
		{
				printf("answer_glj_open\n");
				
					   ip_point=linphone_address_as_string(from);
					 //  printf("ip_point is %s",ip_point);
					   /*
						for(i=0;i<3;i++)
								{
									while(*ip_point!='.')
										{
										ip_point++;
		
										}
									ip_point++;
								}
		
						if(ip_point[1] == '>')
								{
		
								printf("from is %c %c \n",ip_point[0],ip_point[1]);
								buff_glj_open_door[0]=0x00;
								buff_glj_open_door[1]=ip_point[0]-'0';
								printf("%02x,%02x\n",buff_glj_open_door[0],buff_glj_open_door[1]);
							}
							else if(ip_point[2] == '>')
								{
								printf("from is %c %c %c \n",ip_point[0],ip_point[1],ip_point[2]);
								buff_glj_open_door[0]=0x00;
								buff_glj_open_door[1]=(ip_point[0]-'0')*16+ip_point[1]-'0';
								printf("%02x,%02x\n",buff_glj_open_door[0],buff_glj_open_door[1]);
		
								
								}
							else
								{
								
								
								  printf("from is %c %c %c \n",ip_point[0],ip_point[1],ip_point[2]);
								buff_glj_open_door[0]=ip_point[0]-'0';
								 buff_glj_open_door[1]=(ip_point[1]-'0')*16+ip_point[2]-'0';
								
								 printf("%02x,%02x\n",buff_glj_open_door[0],buff_glj_open_door[1]);
		
								}*/
								 printf("ip_point is  %02x,%02x!!\n",buff_glj_open_door[0],buff_glj_open_door[1]);
					  printf("%c%c%c%c%c%c%c%c%c%c\n",0xAF,0xAF,0xAF,0xAF,buff_glj_open_door[0],buff_glj_open_door[1],0x00,0x00,0xDE,0XFF);	
				}
		else if( (pHeadPtr=strstr(msg, "set_wgj_call_sn_falg")) != NULL)
				{
			ip_point=linphone_address_as_string(from);
								   printf("ip_point is %s",ip_point);
								   
									for(i=0;i<3;i++)
											{
												while(*ip_point!='.')
													{
													ip_point++;
					
													}
												ip_point++;
											}
					
								
											
											  printf("from is %c %c %c \n",ip_point[0],ip_point[1],ip_point[2]);
											wgj_huang[0]=ip_point[0]-'0';
											 wgj_huang[1]=(ip_point[1]-'0')*16+ip_point[2]-'0';
											
											 printf("%02x,%02x\n",wgj_huang[0],wgj_huang[1]);
					
											

				/*
				if(wgj_call_sn_falg==1 && (ip_point[2]-'0'==wgj_call_sn_num))
						{
					wgj_call_sn_falg=0;

						}*/
				}

	
}


void linphonec_command_finished(void){
#if !defined(_WIN32_WCE)
/*
	if (client_sock!=ORTP_PIPE_INVALID){
		ortp_server_pipe_close_client(client_sock);
		client_sock=ORTP_PIPE_INVALID;
	}
	*/
#endif /*_WIN32_WCE*/
}

void linphonec_set_autoanswer(bool_t enabled){
	auto_answer=enabled;
}

bool_t linphonec_get_autoanswer(){
	return auto_answer;
}

LinphoneCoreVTable linphonec_vtable={0};

/***************************************************************************/
/*
 * Main
 *
 * Use globals:
 *
 *	- char *histfile_name
 *	- FILE *mylogfile
 */
#if defined (_WIN32_WCE)

char **convert_args_to_ascii(int argc, _TCHAR **wargv){
	int i;
	char **result=malloc(argc*sizeof(char*));
	char argtmp[128];
	for(i=0;i<argc;++i){
		wcstombs(argtmp,wargv[i],sizeof(argtmp));
		result[i]=strdup(argtmp);
	}
	return result;
}

int _tmain(int argc, _TCHAR* wargv[]) {
	char **argv=convert_args_to_ascii(argc,wargv);
	trace_level=6;

#else
int
main (int argc, char *argv[]) {
#endif
/*
#ifdef Doorphone_compile
	
		sensor_gpio_init();
		sensor_run(0);
		sleep(1);
		sensor_run(1);
#endif
*/

	linphonec_vtable.call_state_changed=linphonec_call_state_changed;
	linphonec_vtable.notify_presence_recv = linphonec_notify_presence_received;
	linphonec_vtable.new_subscription_request = linphonec_new_unknown_subscriber;
	linphonec_vtable.auth_info_requested = linphonec_prompt_for_auth;
	linphonec_vtable.display_status = linphonec_display_status;
	linphonec_vtable.display_message=linphonec_display_something;
	linphonec_vtable.display_warning=linphonec_display_warning;
	linphonec_vtable.display_url=linphonec_display_url;
	linphonec_vtable.text_received=linphonec_text_received;
	linphonec_vtable.dtmf_received=linphonec_dtmf_received;
	linphonec_vtable.refer_received=linphonec_display_refer;
	linphonec_vtable.notify_recv=linphonec_notify_received;
	linphonec_vtable.call_encryption_changed=linphonec_call_encryption_changed;
	
	if (! linphonec_init(argc, argv) ) exit(EXIT_FAILURE);
#if defined(__ENABLE_KEYPAD__)
//jimmy close for keypad
//	if ( init_keydevice() < 0 ) exit(EXIT_FAILURE);
#endif
#if defined(__ENABLE_VKEYPAD__)
//jimmy close for keypad
//	if ( init_vkeydevice() < 0 ) exit(EXIT_FAILURE);
#endif

    printf("go to the linphonec_main_loop\n");
	linphonec_main_loop (linphonec);
    printf("out of the linphonec_main_loop\n");
	linphonec_finish(EXIT_SUCCESS);
	printf("out of the linphonec_finish\n");

	exit(EXIT_SUCCESS); /* should never reach here */
}

/*
 * Initialize linphonec
 */
static int
linphonec_init(int argc, char **argv)
{

	//g_mem_set_vtable(&dbgtable);

	/*
	 * Set initial values for global variables
	 */
	mylogfile = NULL;
	
	
#ifndef _WIN32
	//config.vdr is a config txt
	snprintf(configfile_name, PATH_MAX, "%smnt/nand1-2/config.vdr",
			getenv("HOME"));
	snprintf(zrtpsecrets, PATH_MAX, "%smnt/nand1-2/videodoor-zidcache",
			getenv("HOME"));
#elif defined(_WIN32_WCE)
	strncpy(configfile_name,PACKAGE_DIR "\\videodoorc",PATH_MAX);
	mylogfile=fopen(PACKAGE_DIR "\\" "videodoor.log","w");
	printf("Logs are redirected in" PACKAGE_DIR "\\videodoor.log");
#else
	snprintf(configfile_name, PATH_MAX, "%s/Videodoor/videodoor",
			getenv("APPDATA"));
	snprintf(zrtpsecrets, PATH_MAX, "%s/Videodoor/videodoor-zidcache",
			getenv("APPDATA"));
#endif
	/* Handle configuration filename changes */
	switch (handle_configfile_migration())
	{
		case -1: /* error during file copies */
			fprintf(stderr,
				"Error in configuration file migration\n");
			break;

		case 0: /* nothing done */
		case 1: /* migrated */
		default:
			break;
	}

#ifdef ENABLE_NLS
	if (NULL == bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR))
		perror ("bindtextdomain failed");
#ifndef __ARM__
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
#endif
	textdomain (GETTEXT_PACKAGE);
#else
	printf ("NLS disabled.\n");
#endif
	//set argv like -C -D
	linphonec_parse_cmdline(argc, argv);  

	if (trace_level > 0)
	{
		if (logfile_name != NULL)
			mylogfile = fopen (logfile_name, "w+");

		if (mylogfile == NULL)
		{
			mylogfile = stdout;
			fprintf (stderr,
				 "INFO: no logfile, logging to stdout\n");
		}
		linphone_core_enable_logs(mylogfile);
	}
	else
	{
		linphone_core_disable_logs();
	}
	/*
	 * Initialize auth stack
	 */
	auth_stack.nitems=0;

	/*
	 * Initialize linphone core
	 */
	linphonec=linphone_core_new (&linphonec_vtable, configfile_name, factory_configfile_name, NULL);
	linphone_core_set_zrtp_secrets_file(linphonec,zrtpsecrets);
	linphone_core_enable_video(linphonec,vcap_enabled,display_enabled);
	if (display_enabled && window_id != 0) 
	{
		printf ("Setting window_id: 0x%x\n", window_id);
		linphone_core_set_native_video_window_id(linphonec,window_id);
	}

	linphone_core_enable_video_preview(linphonec,preview_enabled);
	if (!(vcap_enabled || display_enabled)) printf("Warning: video is disabled in linphonec, use -V or -C or -D to enable.\n");
#ifdef HAVE_READLINE
	/*
	 * Initialize readline
	 */
	linphonec_initialize_readline();
#endif
#if !defined(_WIN32_WCE)
	/*
	 * Initialize signal handlers
	 */
	signal(SIGTERM, linphonec_finish);
	signal(SIGINT, linphonec_finish);
#endif /*_WIN32_WCE*/
	return 1;
}


void linphonec_main_loop_exit(void){
	linphonec_running=FALSE;
}

/*
 * Close linphonec, cleanly terminating
 * any pending call
 */
void
linphonec_finish(int exit_status)
{
	// Do not allow concurrent destroying to prevent glibc errors
	static bool_t terminating=FALSE;
	if (terminating) return; 
	terminating=TRUE;
	//linphonec_out("Terminating...\n");
    printf("linphonec_finish!!!!!!!!!!!\n");
	/* Terminate any pending call */
	linphone_core_terminate_all_calls(linphonec);
#ifdef HAVE_READLINE
	linphonec_finish_readline();
#endif
#if !defined(_WIN32_WCE)
	if (pipe_reader_run)
		stop_pipe_reader();
#endif /*_WIN32_WCE*/

	linphone_core_destroy (linphonec);

	if (mylogfile != NULL && mylogfile != stdout)
	{
		fclose (mylogfile);
	}
	printf("\n");
	exit(exit_status);

}

/*
 * This is called from idle_call() whenever
 * pending_auth != NULL.
 *
 * It prompts user for a password.
 * Hitting ^D (EOF) would make this function
 * return 0 (Cancel).
 * Any other input would try to set linphone core
 * auth_password for the pending_auth, add the auth_info
 * and return 1.
 */
int
linphonec_prompt_for_auth_final(LinphoneCore *lc)
{
	static int reentrancy=0;
	char *input, *iptr;
	char auth_prompt[256];
#ifdef HAVE_READLINE
	rl_hook_func_t *old_event_hook;
#endif

	if (reentrancy!=0) return 0;
	
	reentrancy++;
	
	LinphoneAuthInfo *pending_auth=auth_stack.elem[auth_stack.nitems-1];

	snprintf(auth_prompt, 256, "Password for %s on %s: ",
		pending_auth->username, pending_auth->realm);

	printf("\n");
#ifdef HAVE_READLINE
	/*
	 * Disable event hook to avoid entering an
	 * infinite loop. This would prevent idle_call
	 * from being called during authentication reads.
	 * Note that it might be undesiderable...
	 */
	old_event_hook=rl_event_hook;
	rl_event_hook=NULL;
#endif

	while (1)
	{
		input=linphonec_readline(auth_prompt);

		/*
		 * If EOF (^D) is sent you probably don't want
		 * to provide an auth password... should give up
		 * the operation, but there's no mechanism to
		 * send this info back to caller currently...
		 */
		if ( ! input )
		{
			printf("Cancel requested, but not implemented.\n");
			continue;
		}

		/* Strip blanks */
		iptr=lpc_strip_blanks(input);

		/*
		 * Only blanks, continue asking
		 */
		if ( ! *iptr )
		{
			free(input);
			continue;
		}

		/* Something typed, let's try */
		break;
	}

	/*
	 * No check is done here to ensure password is correct.
	 * I guess password will be asked again later.
	 */
	linphone_auth_info_set_passwd(pending_auth, input);
	linphone_core_add_auth_info(lc, pending_auth);
	linphone_auth_info_destroy(pending_auth);
	auth_stack.elem[auth_stack.nitems-1]=0;
	--(auth_stack.nitems);
#ifdef HAVE_READLINE
	/*
	 * Reset line_buffer, to avoid the password
	 * to be used again from outer readline
	 */
	rl_line_buffer[0]='\0';
	rl_event_hook=old_event_hook;
#endif
	return 1;
}

void
print_usage (int exit_status)
{
	fprintf (stdout, "\n"
"usage: linphonec [-c file] [-s sipaddr] [-a] [-V] [-d level ] [-l logfile]\n"
       "linphonec -v\n"
"\n"
"  -b  file             specify path of readonly factory configuration file.\n"
"  -c  file             specify path of configuration file.\n"
"  -d  level            be verbose. 0 is no output. 6 is all output\n"
"  -l  logfile          specify the log file for your SIP phone\n"
"  -s  sipaddress       specify the sip call to do at startup\n"
"  -a                   enable auto answering for incoming calls\n"
"  -V                   enable video features globally (disabled by default)\n"
"  -C                   enable video capture only (disabled by default)\n"
"  -D                   enable video display only (disabled by default)\n"
"  -S                   show general state messages (disabled by default)\n"
"  --wid  windowid      force embedding of video window into provided windowid (disabled by default)\n"
"  -v or --version      display version and exits.\n");
    printf("print_usage!!!\n");
  	exit(exit_status);
}

#ifdef VIDEO_ENABLED

#ifdef HAVE_X11_XLIB_H
static void x11_apply_video_params(VideoParams *params, Window window){
	XWindowChanges wc;
	unsigned int flags=0;
	static Display *display = NULL;
	const char *dname=getenv("DISPLAY");

	if (display==NULL && dname!=NULL){
		XInitThreads();
		display=XOpenDisplay(dname);
	}

	if (display==NULL){
		ms_error("Could not open display %s",dname);
		return;
	}
	memset(&wc,0,sizeof(wc));
	wc.x=params->x;
	wc.y=params->y;
	wc.width=params->w;
	wc.height=params->h;
	if (params->x!=-1 ){
		flags|=CWX|CWY;
	}
	if (params->w!=-1){
		flags|=CWWidth|CWHeight;
	}
	/*printf("XConfigureWindow x=%i,y=%i,w=%i,h=%i\n",
	       wc.x, wc.y ,wc.width, wc.height);*/
	XConfigureWindow(display,window,flags,&wc);
	if (params->show)
		XMapWindow(display,window);
	else
		XUnmapWindow(display,window);
	XSync(display,FALSE);
}
#endif


static void lpc_apply_video_params(){
	static unsigned long old_wid=0,old_pwid=0;
	unsigned long wid=linphone_core_get_native_video_window_id (linphonec);
	unsigned long pwid=linphone_core_get_native_preview_window_id (linphonec);

	if (wid!=0 && (lpc_video_params.refresh || old_wid!=wid)){
		lpc_video_params.refresh=FALSE;
#ifdef HAVE_X11_XLIB_H
		if (lpc_video_params.wid==0){  // do not manage window if embedded
			x11_apply_video_params(&lpc_video_params,wid);
		} else {
		        linphone_core_show_video(linphonec, lpc_video_params.show);
		}
#endif
	}
	old_wid=wid;
	if (pwid!=0 && (lpc_preview_params.refresh || old_pwid!=pwid)){
		lpc_preview_params.refresh=FALSE;
#ifdef HAVE_X11_XLIB_H
		/*printf("wid=%lu pwid=%lu\n",wid,pwid);*/
		if (lpc_preview_params.wid==0){  // do not manage window if embedded
			printf("Refreshing\n");
			x11_apply_video_params(&lpc_preview_params,pwid);
		}
#endif
	}
	old_pwid=pwid;
}

#endif


/*
 *
 * Called every second from main read loop.
 *
 * Will use the following globals:
 *
 *  - LinphoneCore linphonec
 *  - LPC_AUTH_STACK auth_stack;
 *
 */
static int
linphonec_idle_call ()
{
	LinphoneCore *opm=linphonec;

	/* Uncomment the following to verify being called */
	/* printf(".\n"); */

	linphone_core_iterate(opm);
	if (answer_call){
		fprintf (stdout, "-------auto answering to call-------\n" );
		linphone_core_accept_call(opm,NULL);
		answer_call=FALSE;
	}
	/* auto call handling */
	if (sip_addr_to_call != NULL )
	{
		char buf[512];
		snprintf (buf, sizeof(buf),"call %s", sip_addr_to_call);
		sip_addr_to_call=NULL;
		linphonec_parse_command_line(linphonec, buf);
	}

#if defined(__ENABLE_KEYPAD__) || defined(__ENABLE_VKEYPAD__)
	/* auto call terminate */
	if (sip_terminate_call != NULL )
	{
		char buffer[512];
		snprintf (buffer, sizeof(buffer),"%s", sip_terminate_call);
		sip_terminate_call=NULL;
		linphonec_parse_command_line(linphonec, buffer);
	}

	/* auto call answer */
	if (sip_answer_call != NULL )
	{
		char buffer[512];
		snprintf (buffer, sizeof(buffer),"%s", sip_answer_call);
		sip_answer_call=NULL;
		linphonec_parse_command_line(linphonec, buffer);
	}

	/* auto call handling */
	if (sip_addrurl_call != NULL )
	{
		char buffer[512];
		snprintf (buffer, sizeof(buffer),"call %s", sip_addrurl_call);
		sip_addrurl_call=NULL;
		linphonec_parse_command_line(linphonec, buffer);
	}
#endif

	if ( auth_stack.nitems )
	{
		/*
		 * Inhibit command completion
		 * during password prompts
		 */
#ifdef HAVE_READLINE
		rl_inhibit_completion=1;
#endif
		linphonec_prompt_for_auth_final(opm);
#ifdef HAVE_READLINE
		rl_inhibit_completion=0;
#endif
	}

#ifdef VIDEO_ENABLED
	lpc_apply_video_params();
#endif

	return 0;
}

#ifdef HAVE_READLINE
/*
 * Use globals:
 *
 *	- char *histfile_name (also sets this)
 *      - char *last_in_history (allocates it)
 */
static int
linphonec_initialize_readline()
{
	/*rl_bind_key('\t', rl_insert);*/

	/* Allow conditional parsing of ~/.inputrc */
	rl_readline_name = "linphonec";

	/* Call idle_call() every second */
	rl_set_keyboard_input_timeout(LPC_READLINE_TIMEOUT);
	rl_event_hook=linphonec_idle_call;

	/* Set history file and read it */
	histfile_name = ms_strdup_printf ("%s/.linphonec_history",
		getenv("HOME"));
	read_history(histfile_name);

	/* Initialized last_in_history cache*/
	last_in_history[0] = '\0';

	/* Register a completion function */
	rl_attempted_completion_function = linephonec_readline_completion;

	/* printf("Readline initialized.\n"); */
        setlinebuf(stdout);
	return 0;
}

/*
 * Uses globals:
 *
 *	- char *histfile_name (writes history to file and frees it)
 *	- char *last_in_history (frees it)
 *
 */
static int
linphonec_finish_readline()
{

	stifle_history(HISTSIZE);
	write_history(histfile_name);
	free(histfile_name);
	histfile_name=NULL;
	return 0;
}

#endif

static void print_prompt(LinphoneCore *opm){
#ifdef IDENTITY_AS_PROMPT
	snprintf(prompt, PROMPT_MAX_LEN, "%s> ",
		linphone_core_get_primary_contact(opm));
#else
	snprintf(prompt, PROMPT_MAX_LEN, "videobell : ");
#endif
}

static int
linphonec_main_loop (LinphoneCore * opm)
{
	char *input;

	print_prompt(opm);

#if defined(__ENABLE_KEYPAD__)
	keypads_thread_create();
#endif
#if defined(__ENABLE_VKEYPAD__)
	vkeypads_thread_create();
#endif
	while (linphonec_running && (input=linphonec_readline(prompt)))
	{
		char *iptr; /* input and input pointer */
		size_t input_len;
		//printf("start of the linphonec_main_loop\n");

		/* Strip blanks */
		iptr=lpc_strip_blanks(input);

		input_len = strlen(iptr);

		/*
		 * Do nothing but release memory
		 * if only blanks are read
		 */
		if ( ! input_len )
		{
			free(input);
			continue;
		}
#ifdef HAVE_READLINE
		/*
		 * Only add to history if not already
		 * last item in it, and only if the command
		 * doesn't start with a space (to allow for
		 * hiding passwords)
		 */
		if ( iptr == input && strcmp(last_in_history, iptr) )
		{
			strncpy(last_in_history,iptr,sizeof(last_in_history));
			last_in_history[sizeof(last_in_history)-1]='\0';
			add_history(iptr);
		}
#endif
   //     printf("iptr is %s\n",iptr);
		linphonec_parse_command_line(linphonec, iptr);
		//linphonec_command_finished();
		//printf("out of the linphonec_command_finished\n");
		free(input);
		//printf("out of the free\n");
		//unix_socket=FALSE;
	}
#if defined(__ENABLE_KEYPAD__)
	keypads_thread_destroy();
#endif
#if defined(__ENABLE_VKEYPAD__)
	vkeypads_thread_destroy();
#endif
	return 0;
}

/*
 *  Parse command line switches
 *
 *  Use globals:
 *
 *	- int trace_level
 *	- char *logfile_name
 *	- char *configfile_name
 *	- char *sipAddr
 */
static int
linphonec_parse_cmdline(int argc, char **argv)
{
	int arg_num=1;

	while (arg_num < argc)
	{
		int old_arg_num = arg_num;
		if (strncmp ("-d", argv[arg_num], 2) == 0)
		{
			arg_num++;
			if (arg_num < argc)
				trace_level = atoi (argv[arg_num]);
			else
				trace_level = 1;
		}
		else if (strncmp ("-l", argv[arg_num], 2) == 0)
		{
			arg_num++;
			if (arg_num < argc)
				logfile_name = argv[arg_num];
		}
		else if (strncmp ("-c", argv[arg_num], 2) == 0)
		{
			if ( ++arg_num >= argc ) print_usage(EXIT_FAILURE);
		#if !defined(_WIN32_WCE)
			if (access(argv[arg_num],F_OK)!=0 )
			{
				fprintf (stderr,
					"Cannot open config file %s.\n",
					 argv[arg_num]);
				exit(EXIT_FAILURE);
			}
		#endif /*_WIN32_WCE*/
			snprintf(configfile_name, PATH_MAX, "%s", argv[arg_num]);
		}
		else if (strncmp ("-b", argv[arg_num], 2) == 0)
		{
			if ( ++arg_num >= argc ) print_usage(EXIT_FAILURE);
		#if !defined(_WIN32_WCE)
			if (access(argv[arg_num],F_OK)!=0 )
			{
				fprintf (stderr,
					"Cannot open config file %s.\n",
					 argv[arg_num]);
				exit(EXIT_FAILURE);
			}
		#endif /*_WIN32_WCE*/
			factory_configfile_name = argv[arg_num];
		}
		else if (strncmp ("-s", argv[arg_num], 2) == 0)
		{
			arg_num++;
			if (arg_num < argc)
				sip_addr_to_call = argv[arg_num];
		}
                else if (strncmp ("-a", argv[arg_num], 2) == 0)
                {
                        auto_answer = TRUE;
                }
		else if (strncmp ("-C", argv[arg_num], 2) == 0)
                {
                        vcap_enabled = TRUE;
                }
		else if (strncmp ("-D", argv[arg_num], 2) == 0)
                {
                        display_enabled = TRUE;
                }
		else if (strncmp ("-V", argv[arg_num], 2) == 0)
                {
                        display_enabled = TRUE;
			vcap_enabled = TRUE;
			preview_enabled=TRUE;
                }
		else if ((strncmp ("-v", argv[arg_num], 2) == 0)
			 ||
			 (strncmp
			  ("--version", argv[arg_num],
			   strlen ("--version")) == 0))
		{
		#if !defined(_WIN32_WCE)
			printf ("version: " LINPHONE_VERSION "\n");
		#endif
			exit (EXIT_SUCCESS);
		}
		else if (strncmp ("-S", argv[arg_num], 2) == 0)
		{
			show_general_state = TRUE;
		}
		else if (strncmp ("--pipe", argv[arg_num], 6) == 0)
		{
			unix_socket=1;
		}
		else if (strncmp ("--wid", argv[arg_num], 5) == 0)
		{
			arg_num++;
			if (arg_num < argc) {
				char *tmp;
				window_id = strtol( argv[arg_num], &tmp, 0 );
				lpc_video_params.wid = window_id;
			}
		}
		else if (old_arg_num == arg_num)
		{
			fprintf (stderr, "ERROR: bad arguments\n");
			print_usage (EXIT_FAILURE);
		}
		arg_num++;
	}

	return 1;
}

/*
 * Up to version 1.2.1 linphone used ~/.linphonec for
 * CLI and ~/.gnome2/linphone for GUI as configuration file.
 * In newer version both interfaces will use ~/.linphonerc.
 *
 * This function helps transparently migrating from one
 * to the other layout using the following heuristic:
 *
 *	IF new_config EXISTS => do nothing
 *	ELSE IF old_cli_config EXISTS => copy to new_config
 *	ELSE IF old_gui_config EXISTS => copy to new_config
 *
 * Returns:
 *	 0 if it did nothing
 *	 1 if it migrated successfully
 *	-1 on error
 */
static int
handle_configfile_migration(void)
{
#if !defined(_WIN32_WCE)
	char *old_cfg_gui;
	char *old_cfg_cli;
	char *new_cfg;
#if !defined(_WIN32_WCE)
	const char *home = getenv("HOME");
#else
	const char *home = ".";
#endif /*_WIN32_WCE*/
	new_cfg = ms_strdup_printf("%smnt/nand1-2/config.vdr", home);

	/*
	 * If the *NEW* configuration already exists
	 * do nothing.
	 */
	if (access(new_cfg,F_OK)==0)
	{
		free(new_cfg);
		return 0;
	}

	old_cfg_cli = ms_strdup_printf("%smnt/nand1-2/config.vdr", home);

	/*
	 * If the *OLD* CLI configurations exist copy it to
	 * the new file and make it a symlink.
	 */
	if (access(old_cfg_cli, F_OK)==0)
	{
		if ( ! copy_file(old_cfg_cli, new_cfg) )
		{
			free(old_cfg_cli);
			free(new_cfg);
			return -1;
		}
		printf("%s copied to %s\n", old_cfg_cli, new_cfg);
		free(old_cfg_cli);
		free(new_cfg);
		return 1;
	}

	free(old_cfg_cli);
	old_cfg_gui = ms_strdup_printf("%s/.gnome2/linphone", home);

	/*
	 * If the *OLD* GUI configurations exist copy it to
	 * the new file and make it a symlink.
	 */
	if (access(old_cfg_gui, F_OK)==0)
	{
		if ( ! copy_file(old_cfg_gui, new_cfg) )
		{
			exit(EXIT_FAILURE);
			free(old_cfg_gui);
			free(new_cfg);
			return -1;
		}
		printf("%s copied to %s\n", old_cfg_gui, new_cfg);
		free(old_cfg_gui);
		free(new_cfg);
		return 1;
	}

	free(old_cfg_gui);
	free(new_cfg);
#endif /*_WIN32_WCE*/
	return 0;
}
#if !defined(_WIN32_WCE)
/*
 * Copy file "from" to file "to".
 * Destination file is truncated if existing.
 * Return 1 on success, 0 on error (printing an error).
 */
static int
copy_file(const char *from, const char *to)
{
	char message[256];
	FILE *in, *out;
	char buf[256];
	size_t n;

	/* Open "from" file for reading */
	in=fopen(from, "r");
	if ( in == NULL )
	{
		snprintf(message, 255, "Can't open %s for reading: %s\n",
			from, strerror(errno));
		fprintf(stderr, "%s", message);
		return 0;
	}

	/* Open "to" file for writing (will truncate existing files) */
	out=fopen(to, "w");
	if ( out == NULL )
	{
		snprintf(message, 255, "Can't open %s for writing: %s\n",
			to, strerror(errno));
		fprintf(stderr, "%s", message);
		fclose(in);
		return 0;
	}

	/* Copy data from "in" to "out" */
	while ( (n=fread(buf, 1, sizeof buf, in)) > 0 )
	{
		if ( ! fwrite(buf, 1, n, out) )
		{
			fclose(in);
			fclose(out);
			return 0;
		}
	}

	fclose(in);
	fclose(out);

	return 1;
}
#endif /*_WIN32_WCE*/

#ifdef HAVE_READLINE
static char **
linephonec_readline_completion(const char *text, int start, int end)
{
	char **matches = NULL;

	/*
	 * Prevent readline from falling
	 * back to filename-completion
	 */
	rl_attempted_completion_over=1;

	/*
	 * If this is the start of line we complete with commands
	 */
	if ( ! start )
	{
		return rl_completion_matches(text, linphonec_command_generator);
	}

	/*
	 * Otherwise, we should peek at command name
	 * or context to implement a smart completion.
	 * For example: "call .." could return
	 * friends' sip-uri as matches
	 */

	return matches;
}

#endif

/*
 * Strip blanks from a string.
 * Return a pointer into the provided string.
 * Modifies input adding a NULL at first
 * of trailing blanks.
 */
char *
lpc_strip_blanks(char *input)
{
	char *iptr;

	/* Find first non-blank */
	while(*input && isspace(*input)) ++input;

	/* Find last non-blank */
	iptr=input+strlen(input);
	if (iptr > input) {
		while(isspace(*--iptr));
		*(iptr+1)='\0';
	}

	return input;
}

/****************************************************************************
 *
 * $Log: linphonec.c,v $
 * Revision 1.57  2007/11/14 13:40:27  smorlat
 * fix --disable-video build.
 *
 * Revision 1.56  2007/09/26 14:07:27  fixkowalski
 * - ANSI/C++ compilation issues with non-GCC compilers
 * - Faster epm-based packaging
 * - Ability to build & run on FC6's eXosip/osip
 *
 * Revision 1.55  2007/09/24 16:01:58  smorlat
 * fix bugs.
 *
 * Revision 1.54  2007/08/22 14:06:11  smorlat
 * authentication bugs fixed.
 *
 * Revision 1.53  2007/02/13 21:31:01  smorlat
 * added patch for general state.
 * new doxygen for oRTP
 * gtk-doc removed.
 *
 * Revision 1.52  2007/01/10 14:11:24  smorlat
 * add --video to linphonec.
 *
 * Revision 1.51  2006/08/21 12:49:59  smorlat
 * merged several little patches.
 *
 * Revision 1.50  2006/07/26 08:17:28  smorlat
 * fix bugs.
 *
 * Revision 1.49  2006/07/17 18:45:00  smorlat
 * support for several event queues in ortp.
 * glib dependency removed from coreapi/ and console/
 *
 * Revision 1.48  2006/04/09 12:45:32  smorlat
 * linphonec improvements.
 *
 * Revision 1.47  2006/04/04 08:04:34  smorlat
 * switched to mediastreamer2, most bugs fixed.
 *
 * Revision 1.46  2006/03/16 17:17:40  smorlat
 * fix various bugs.
 *
 * Revision 1.45  2006/03/12 21:48:31  smorlat
 * gcc-2.95 compile error fixed.
 * mediastreamer2 in progress
 *
 * Revision 1.44  2006/03/04 11:17:10  smorlat
 * mediastreamer2 in progress.
 *
 * Revision 1.43  2006/02/13 09:50:50  strk
 * fixed unused variable warning.
 *
 * Revision 1.42  2006/02/02 15:39:18  strk
 * - Added 'friend list' and 'friend call' commands
 * - Allowed for multiple DTFM send in a single line
 * - Added status-specific callback (bare version)
 *
 * Revision 1.41  2006/02/02 13:30:05  strk
 * - Padded vtable with missing callbacks
 *   (fixing a segfault on friends subscription)
 * - Handled friends notify (bare version)
 * - Handled text messages receive (bare version)
 * - Printed message on subscription request (bare version)
 *
 * Revision 1.40  2006/01/26 09:48:05  strk
 * Added limits.h include
 *
 * Revision 1.39  2006/01/26 02:11:01  strk
 * Removed unused variables, fixed copyright date
 *
 * Revision 1.38  2006/01/25 18:33:02  strk
 * Removed the -t swich, terminate_on_close made the default behaviour
 *
 * Revision 1.37  2006/01/20 14:12:34  strk
 * Added linphonec_init() and linphonec_finish() functions.
 * Handled SIGINT and SIGTERM to invoke linphonec_finish().
 * Handling of auto-termination (-t) moved to linphonec_finish().
 * Reworked main (input read) loop to not rely on 'terminate'
 * and 'run' variable (dropped). configfile_name allocated on stack
 * using PATH_MAX limit. Changed print_usage signature to allow
 * for an exit_status specification.
 *
 * Revision 1.36  2006/01/18 09:25:32  strk
 * Command completion inhibited in proxy addition and auth request prompts.
 * Avoided use of readline's internal filename completion.
 *
 * Revision 1.35  2006/01/14 13:29:32  strk
 * Reworked commands interface to use a table structure,
 * used by command line parser and help function.
 * Implemented first level of completion (commands).
 * Added notification of invalid "answer" and "terminate"
 * commands (no incoming call, no active call).
 * Forbidden "call" intialization when a call is already active.
 * Cleaned up all commands, adding more feedback and error checks.
 *
 * Revision 1.34  2006/01/13 13:00:29  strk
 * Added linphonec.h. Code layout change (added comments, forward decl,
 * globals on top, copyright notices and Logs). Handled out-of-memory
 * condition on history management. Removed assumption on sizeof(char).
 * Fixed bug in authentication prompt (introduced by readline).
 * Added support for multiple authentication requests (up to MAX_PENDING_AUTH).
 *
 *
 ****************************************************************************/

