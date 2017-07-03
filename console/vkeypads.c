/****************************************************************************
 *
 * FILENAME
 *     vkeypads.c
 *
 * DESCRIPTION
 *     'vkey, virtual key for minigui
 *
 ***************************************************************************/

#include<stdio.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/ipc.h>
#include<sys/ioctl.h>
#include<pthread.h>
#include<fcntl.h>
#include<linux/input.h>
#include<string.h>
#include<strings.h>

#include <keypads.h>

#include "linphonec.h"

//#define	DEBUG
#define	test_bit(bit, array)	(array[bit/8] & (1<<(bit%8)))
#define	RETRY_NUM		4

char		vdevice[] = "/dev/input/event2";
int		vfd = -1;
pthread_t	vkeypads;

static bool_t	vkeypadrun=FALSE;
static bool_t	vkeypads_locks=FALSE;
static bool_t	vkeytoterminate=FALSE;
static bool_t	isKeyaction=FALSE;

void videodoorc_out(const char *fmt,...);

#ifdef	DEBUG
	#define	DEBUG_INFO	linphonec_out
#else
	#define	DEBUG_INFO
#endif

int init_vkeydevice(void)
{
	char devname[256];
	static char vkeypads[] = "Keypad";
 
	//DEBUG_INFO("trying to open %s ...\n", device);
	if ((vfd=open(vdevice,O_RDONLY|O_NONBLOCK)) < 0) {
      		//return -1;
	}
	memset(devname, 0x0, sizeof(devname));
	if (ioctl(vfd, EVIOCGNAME(sizeof(devname)), devname) < 0) {
		//DEBUG_INFO("%s evdev ioctl error!\n", device);
		//return -1;
	} else {
		if (strstr(devname, vkeypads) != NULL) {
			//DEBUG_INFO("Keypad device is %s\n", device);
		}
	}
	return 0;
}


void *vkeypads_pthread(void *arg)
{
	struct input_event data;

     	while(vkeypadrun) 
        {
		if( read(vfd, &data, sizeof(data)) == sizeof(data) ) {
			if( (vkeypads_locks == FALSE)&& (data.type != 0) && (data.value == 1) )		//key down
			{
				if (data.code == KEYPAD_CALLID && vkeytoterminate == FALSE) {
				#if defined(__VIDEODOOR_S__) || (defined(__VIDEODOOR_DUALVIDEO__) && !defined(__KEYPAD_ENABLE__))
					sip_addrurl_call = "sip:192.168.2.20"; 
				#else
					sip_addrurl_call = "sip:192.168.2.10";
				#endif
					vkeytoterminate = TRUE;
					isKeyaction = TRUE;
				} else if (data.code == KEYPAD_CALLID && vkeytoterminate == TRUE) {
					sip_terminate_call = "terminate";
					vkeytoterminate = FALSE;
					isKeyaction = FALSE;
				} else if (data.code == KEYPAD_ANSWER && isKeyaction == TRUE) {
					sip_answer_call = "answer";
					vkeytoterminate = TRUE;
				} else if (data.code == KEYPAD_ANSWER && isKeyaction == FALSE) {
					sip_answer_call = "answer";
					vkeytoterminate = FALSE;
				} 				
				vkeypads_locks = TRUE;
			} else {
				usleep(80000);		/*avoid 100%cpu loop for nothing*/
			}
			 
		} else  {
			vkeypads_locks = FALSE;
			usleep(80000);		/*avoid 100%cpu loop for nothing*/
		}
	}
	close(vfd);
	return NULL;
}

int vkeypads_thread_create(void)
{
	vkeypadrun = TRUE;
	return pthread_create(&vkeypads, NULL, vkeypads_pthread, NULL);
}

int vkeypads_thread_destroy(void)
{
	vkeypadrun = FALSE;
	vkeytoterminate = FALSE;
	pthread_join(vkeypads, NULL);
	return 0;
}


