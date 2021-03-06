#include <malloc.h>
#include <memory.h>
#include <string.h>
#include <stdio.h>
#include "protocol.h"

///////////////////////////////////////////////////////////////////////////////

typedef unsigned char MAGIC [8];

typedef struct _RADAR_HDR	//	36 bytes	header
{	MAGIC Magic;				//	8 bytes		-> header sync
	uint32_t Version;			//	4 bytes		-> check the version
	uint32_t PacketLength;		//	4 bytes		-> packet's totoal size
	uint32_t Platform;			//	4 bytes		= 0xA1443
	uint32_t FrameNumber;		//	4 bytes
	uint32_t ClockCounter;		//	4 bytes
	uint32_t NumberOfObjects;	//	4 bytes		unused?
	uint32_t NumberOfTLVs;		//	4 bytes		what's TLV?
}	RADAR_HDR;

typedef struct _RADAR_TLV	//	8 bytes TLV header
{	uint32_t Type;				//	4 bytes type
	uint32_t Length;			//	4 bytes length
}	RADAR_TLV;					//	follows by <Lenght> bytes data block

typedef struct _RADAR_DSC
{	uint32_t NumberOfObjects;	//	2 or 4? Same as before or different?
	uint32_t Qformat;			//	Fixed-point position
}	RADAR_DSC;

MAGIC magic = {0x02, 0x01, 0x04, 0x03, 0x06, 0x05, 0x08, 0x07};

///////////////////////////////////////////////////////////////////////////////

enum {BACKETTAG = __LINE__};

///////////////////////////////////////////////////////////////////////////////

typedef	struct _BACKET
{	uint32_t tag;
	uint32_t siz;		//	maximum size
	uint32_t bytes;	//	bytes loaded
	STATE state;		//	states
	char buf [4];		//	variable size data buffer
}	BACKET;

static uint32_t move_head (BACKET * backet, const uint32_t chunk);

///////////////////////////////////////////////////////////////////////////////
//	shift data block up

static uint32_t move_head (BACKET * backet, uint32_t chunk)
{	if (chunk > backet->bytes) chunk = backet->bytes;
	backet->bytes -= chunk;
	memcpy (&backet->buf [0], &backet->buf [chunk], backet->bytes);
	return chunk;}

///////////////////////////////////////////////////////////////////////////////
//	open the "backet"

int bopen (const int siz)
{	BACKET * backet;
	backet = (BACKET *) malloc (sizeof (*backet) - sizeof(backet->buf) + siz);
	if (backet)
	{	backet->tag = BACKETTAG;
		backet->state = NOSYNC;		//	state: no sync
		backet->siz = siz;			//	total size
		backet->bytes = 0U;}		//	data bytes
	return (int) backet;}

///////////////////////////////////////////////////////////////////////////////
//	close the "backet"

int bclose (const int h)
{	BACKET * backet = (BACKET *) h;
	if ((backet) && (backet->tag == BACKETTAG))
	{	free ((void *) backet);
		return 1;}
	return 0;}

///////////////////////////////////////////////////////////////////////////////
//	send (append) data bytes to the backet 

int	bsend (const int h, const char * buf, const uint32_t bytes)
{	BACKET * backet = (BACKET *) h;
	if ((backet) && (backet->tag == BACKETTAG))
	{	if (backet->bytes + bytes <= backet->siz)
		{	memcpy (&(backet->buf[backet->bytes]), buf, bytes);
			backet->bytes += bytes;}
		return 1;}
	return 0;}

///////////////////////////////////////////////////////////////////////////////
//	wait for the states' changes

STATE bwait (const int h)
{	BACKET * backet = (BACKET *) h;
	unsigned int bytes;
	if ((backet) && (backet->tag == BACKETTAG))
	{	switch (backet->state)
		{	default:
			case NOSYNC:
				//	printf ("NOSYNC %u\n", backet->bytes);
				while (backet->bytes >= sizeof(RADAR_HDR))
				{	unsigned int i;
					//	if 1st item is not magic [0] scan until it would be magic [0] or buffers ends ...
					if (backet->buf [0] != magic [0])
					{	for (i = 1; i < backet->bytes && backet->buf [i] != magic[0]; i++);
						move_head (backet, i);}
					// if 1st item is magic [0] scan for magic [i]
					else
					{	for (i = 1; i < sizeof (MAGIC) && backet->buf [i] == magic [i]; i++);						
						if (i < sizeof (MAGIC)) 
						{	move_head (backet, i);}
						else 
						{	//	entire magic [i] is matching
							backet->state = HDRRDY;
							break;}}}
				break;
			case HDRRDY:
				if (backet->bytes >= ((RADAR_HDR *) &(backet->buf[0]))->PacketLength)
					backet->state = DATRDY;
				break;
			case DATRDY:
				bytes = ((RADAR_HDR*)&(backet->buf[0]))->PacketLength;
				if (backet->bytes >= bytes)
				{	move_head (backet, bytes);
					backet->state = NOSYNC;}
				break;}
		return backet->state;}
	return UNDEF;}

///////////////////////////////////////////////////////////////////////////////
//	get header

void * get_header (const int h)
{	BACKET * backet = (BACKET *) h;
	if ((backet) && (backet->tag == BACKETTAG) && backet->state >= HDRRDY)
		return (void *) & (backet->buf[0]);
	return 0;}

///////////////////////////////////////////////////////////////////////////////
//	get version

uint32_t get_version (const int h)
{	BACKET * backet = (BACKET *) h;
	if ((backet) && (backet->tag == BACKETTAG) && backet->state >= HDRRDY)
		return ((RADAR_HDR *)&(backet->buf[0]))->Version;
	return 0;}

///////////////////////////////////////////////////////////////////////////////
//	get packet length

uint32_t get_packet_length (const int h)
{	BACKET * backet = (BACKET *) h;
	if ((backet) && (backet->tag == BACKETTAG) && backet->state >= HDRRDY)
		return ((RADAR_HDR *)&(backet->buf[0]))->PacketLength;
	return 0;}

///////////////////////////////////////////////////////////////////////////////
//	get packet lenght

uint32_t get_platform (const int h)
{	BACKET * backet = (BACKET *) h;
	if ((backet) && (backet->tag == BACKETTAG) && backet->state >= HDRRDY)
		return ((RADAR_HDR *)&(backet->buf[0]))->Platform;
	return 0;}

///////////////////////////////////////////////////////////////////////////////
//	get frame number

uint32_t get_frame_number (const int h)
{	BACKET * backet = (BACKET *) h;
	if ((backet) && (backet->tag == BACKETTAG) && backet->state >= HDRRDY)
		return ((RADAR_HDR *)&(backet->buf[0]))->FrameNumber;
	return 0;}

///////////////////////////////////////////////////////////////////////////////
//	get clock counter

uint32_t get_clock_counter (const int h)
{	BACKET * backet = (BACKET *) h;
	if ((backet) && (backet->tag == BACKETTAG) && backet->state >= HDRRDY)
		return ((RADAR_HDR *)&(backet->buf[0]))->ClockCounter;
	return 0;}

///////////////////////////////////////////////////////////////////////////////
//	get number of objects

uint32_t get_number_of_objects (const int h)
{	BACKET * backet = (BACKET *) h;
	if ((backet) && (backet->tag == BACKETTAG) && backet->state >= HDRRDY)
		return ((RADAR_HDR *)&(backet->buf[0]))->NumberOfObjects;
	return 0;}

///////////////////////////////////////////////////////////////////////////////
//	get number of TLVs

uint32_t get_number_of_tlvs (const int h)
{	BACKET * backet = (BACKET *) h;
	uint32_t n = 0; 
	if ((backet) && (backet->tag == BACKETTAG) && backet->state >= HDRRDY)
	{	n = ((RADAR_HDR *)&(backet->buf[0]))->NumberOfTLVs;}
	return n;}

///////////////////////////////////////////////////////////////////////////////
//	get data

void * get_data (const int h)
{	BACKET * backet = (BACKET *) h;
	if ((backet) && (backet->tag == BACKETTAG) && backet->state >= DATRDY)
		return (void *) ((unsigned char *) & (backet->buf[0]) + sizeof (RADAR_HDR));
	return 0;}

///////////////////////////////////////////////////////////////////////////////
//	get TLV type

uint32_t get_tlv_type (const int h, uint32_t idx)
{	BACKET * backet = (BACKET *) h;
	if ((backet) && (backet->tag == BACKETTAG) && backet->state == DATRDY && idx < get_number_of_tlvs(h))
	{	RADAR_TLV * ptlv = (RADAR_TLV *)((unsigned char *)&(backet->buf[0]) + sizeof (RADAR_HDR));
		unsigned int i;
		if (ptlv->Type > 3)
			return 0;
		for (i = 0; i < idx; i++)
		{	ptlv = (RADAR_TLV *)((unsigned char *) ptlv + sizeof (RADAR_TLV) + ptlv->Length);
			if (ptlv->Type > 3) 
				return 0;}
		return ptlv->Type;}
	return 0;}

///////////////////////////////////////////////////////////////////////////////
//	get TLV length

uint32_t get_tlv_length (const int h, uint32_t idx)
{	BACKET * backet = (BACKET *) h;
	if ((backet) && (backet->tag == BACKETTAG) && backet->state == DATRDY && idx < get_number_of_tlvs(h))
	{	RADAR_TLV * ptlv = (RADAR_TLV *)((unsigned char *)&(backet->buf[0]) + sizeof (RADAR_HDR));
		unsigned int i;
		if (ptlv->Type > 3)
			return 0;
		for (i = 0; i < idx; i++)
		{	ptlv = (RADAR_TLV *)((unsigned char *) ptlv + sizeof (RADAR_TLV) + ptlv->Length);
			if (ptlv->Type > 3)
				return 0;}
		return ptlv->Length;}
	return 0;}

///////////////////////////////////////////////////////////////////////////////
//	get TLV Data

void * get_tlv_data (const int h, uint32_t idx)
{	BACKET * backet = (BACKET *) h;
	if ((backet) && (backet->tag == BACKETTAG) && backet->state == DATRDY && idx < get_number_of_tlvs(h))
	{	RADAR_TLV * ptlv = (RADAR_TLV *)((unsigned char *)&(backet->buf[0]) + sizeof (RADAR_HDR));
		unsigned int i;
		if (ptlv->Type > 3)
			return 0;
		for (i = 0; i < idx; i++)
		{	ptlv = (RADAR_TLV *)((unsigned char *) ptlv + sizeof (RADAR_TLV) + ptlv->Length);
			if (ptlv->Type > 3) 
				return 0;}
		return (void *) ((unsigned char *) ptlv + sizeof (RADAR_TLV));}
	return 0;}
