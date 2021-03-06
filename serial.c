#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <malloc.h>
#include <memory.h>
#include <pthread.h>
#include "serial.h"

enum {MAGIC=__LINE__};

typedef struct _CTX
{	int magic;
	int h;
	pthread_t th;
	volatile COMMCB rdcb;
	void * pParam;
}	CTX;
  
 static CTX * get_ctx (int h)
 {	 CTX * ctx = (CTX *) h;
	 if (ctx != 0 && ctx->magic == MAGIC) 
		return ctx;
	 return (CTX *) 0;}

void * comm_proc (void * ptr)
{	enum {BUFFSIZE = 8192};
	unsigned char buf [BUFFSIZE];
	static int retval;
	CTX * ctx;
	int size;
	retval = 0;
	ctx = (CTX *) ptr;
	if (ctx == 0)
		pthread_exit (&retval);
	while (ctx->rdcb != 0)
	{	size = BUFFSIZE;
		if (size != 0 && (size = comm_recv ((int) ctx, buf, size)) > 0)
		{	if (size != 0 && ctx->rdcb != 0)
				ctx->rdcb (ctx->pParam, buf, size);}}
	pthread_exit (&retval);}
 
int comm_open (const char * devname, unsigned int speed, unsigned int parity, COMMCB rdcb, void * pParam, unsigned int rdInterval, unsigned int rdTimeout, unsigned int wrTimeoput)
{	enum {OFLAGS = O_RDWR | O_NOCTTY | O_SYNC};
	//	enum {OFLAGS = O_RDONLY};
	struct termios tty;
	CTX * ctx;
	if (devname == 0) return 0;
	ctx= malloc (sizeof (CTX));
	if (ctx == 0) return 0;
	memset (ctx, 0, sizeof (CTX));
	ctx->h = open (devname, OFLAGS);
	if (ctx->h < 0)
	{	free ((void *) ctx);
		return 0;}
	tcgetattr (ctx->h, &tty);
//	cfsetospeed (&tty, speed);
//  cfsetispeed (&tty, speed);
	tty.c_cflag = speed | parity |CS8 | CLOCAL | CREAD;
	tty.c_iflag &= ~IGNBRK;         			// disable break processing
    tty.c_lflag = 0;                			// no signaling chars, no echo
	tty.c_oflag = 0;                			// no remapping, no delays
	tty.c_cc [VMIN]  = 1;            			// read doesn't block
	tty.c_cc [VTIME] = 1;            			// 1 second read timeout
	tty.c_iflag &= ~(IXON | IXOFF | IXANY); 	// shut off xon/xoff ctrl
	tcflush (ctx->h, TCIFLUSH);
	if (tcsetattr (ctx->h, TCSANOW, &tty) != 0)
	{	free ((void *) ctx);
		return 0;}
	ctx->rdcb = rdcb;
	ctx->pParam = pParam;
	ctx->magic = MAGIC;
	if (rdcb != 0)
		pthread_create (&ctx->th, NULL, comm_proc, (void *) ctx);
	return (int) ctx;}

int comm_recv (int h, void * buf, int size)
{	CTX * ctx = get_ctx (h);
	if (ctx != 0 && buf != 0 && size != 0)
		return read (ctx->h, buf, size);	
	return 0;}

void comm_close (int h)
{	CTX * ctx = (CTX *) get_ctx (h);
	void * msg;
	if (ctx) 
	{	ctx->rdcb = 0;
		pthread_join (ctx->th, &msg);
		free ((void *) ctx);
		ctx = 0;}}
