/****************************************************************************
 *
 * FILENAME
 *     keypads.c
 *
 * DESCRIPTION
 *     This file is a keypads program
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

char		device[] = "/dev/input/event0";
int		fd = -1;
pthread_t	keypads;

static bool_t	keypadrun=FALSE;
static bool_t	keypads_locks=FALSE;
static bool_t	keytoterminate=FALSE;
static bool_t	isKeyaction=FALSE;

void videodoorc_out(const char *fmt,...);

#ifdef	DEBUG
	#define	DEBUG_INFO	linphonec_out
#else
	#define	DEBUG_INFO
#endif

int init_keydevice(void)
{
	char devname[256];
	static char keypads[] = "Keypad";
 
	//DEBUG_INFO("trying to open %s ...\n", device);
	if ((fd=open(device,O_RDONLY|O_NONBLOCK)) < 0) {
      		return -1;
	}
	memset(devname, 0x0, sizeof(devname));
	if (ioctl(fd, EVIOCGNAME(sizeof(devname)), devname) < 0) {
		//DEBUG_INFO("%s evdev ioctl error!\n", device);
		return -1;
	} else {
		if (strstr(devname, keypads) != NULL) {
			//DEBUG_INFO("Keypad device is %s\n", device);
		}
	}
	return 0;
}


void *keypads_pthread(void *arg)
{
	struct input_event data;

     	while(keypadrun) 
        {
		if( read(fd, &data, sizeof(data)) == sizeof(data) ) {
			if( (keypads_locks == FALSE)&& (data.type != 0) && (data.value == 1) )		//key down
			{
				if (data.code == KEYPAD_CALLID && keytoterminate == FALSE) {
				#if defined(__VIDEODOOR_S__) || (defined(__VIDEODOOR_DUALVIDEO__) && !defined(__KEYPAD_ENABLE__))
					sip_addrurl_call = "sip:192.168.2.20"; 
				#else
					sip_addrurl_call = "sip:192.168.2.10";
				#endif
					keytoterminate = TRUE;
					isKeyaction = TRUE;
				} else if (data.code == KEYPAD_CALLID && keytoterminate == TRUE) {
					sip_terminate_call = "terminate";
					keytoterminate = FALSE;
					isKeyaction = FALSE;
				} else if (data.code == KEYPAD_ANSWER && isKeyaction == TRUE) {
					sip_answer_call = "answer";
					keytoterminate = TRUE;
				} else if (data.code == KEYPAD_ANSWER && isKeyaction == FALSE) {
					sip_answer_call = "answer";
					keytoterminate = FALSE;
				} 				
				keypads_locks = TRUE;
			} else {
				usleep(80000);		/*avoid 100%cpu loop for nothing*/
			}
			 
		} else  {
			keypads_locks = FALSE;
			usleep(80000);		/*avoid 100%cpu loop for nothing*/
		}
	}
	close(fd);
	return NULL;
}

//jimmy +++  keypads_thread
int keypads_thread_create(void)
{
	keypadrun = TRUE;
	return pthread_create(&keypads, NULL, keypads_pthread, NULL);
}

int keypads_thread_destroy(void)
{
	keypadrun = FALSE;
	keytoterminate = FALSE;
	pthread_join(keypads, NULL);
	return 0;
}


