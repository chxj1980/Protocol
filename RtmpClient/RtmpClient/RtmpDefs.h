#ifndef __RTMPDEFS_H__
#define __RTMPDEFS_H__
#include <winsock2.h>
#include "amf.h"

#define RTMP_MAX_HEADER_SIZE		18
#define RTMP_BUFFER_CACHE_SIZE		(16*1024)

#define RTMP_SIG_SIZE				1536
#define RTMP_LARGE_HEADER_SIZE		12

#define RTMP_DEFAULT_CHUNKSIZE		128

// rtmp特点
#define RTMP_FEATURE_HTTP	0x01
#define RTMP_FEATURE_ENC	0x02
#define RTMP_FEATURE_SSL	0x04
#define RTMP_FEATURE_MFP	0x08	/* not yet supported */
#define RTMP_FEATURE_WRITE	0x10	/* publish, not play */
#define RTMP_FEATURE_HTTP2	0x20	/* server-side rtmpt */


// 基本头-格式
enum BasicHeaderFmt
{
	BASICHEADER_FMT_ZREO	= 0,	// 11 bytes.
	BASICHEADER_FMT_ONE		= 1,	// 7 bytes.
	BASICHEADER_FMT_TWO		= 2,	// 3 bytes.
	BASICHEADER_FMT_THREE	= 3		// 0 bytes.
};

typedef struct RTMP_METHOD
{
	AVal name;
	int num;
} RTMP_METHOD;

// 消息头-类型
enum MsgHeaderType
{
	MSGHEADER_TYPE_SET_CHUNK_SIZE		= 0x01,		// set chunk size
	MSGHEADER_TYPE_ABORT_MSG			= 0x02,		// abort msg
	MSGHEADER_TYPE_ACKNOWLEDGEMENT		= 0x03,		// ack for set chunksize
	MSGHEADER_TYPE_USER_CONTROL			= 0x04,		// user control
	MSGHEADER_TYPE_SERVER_BW			= 0x05,		// 
	MSGHEADER_TYPE_CLIENT_BW			= 0x06,		// 
	// MSGHEADER_TYPE_TYPE_...				0x07 
	MSGHEADER_TYPE_AUDIO				= 0x08,		// audio
	MSGHEADER_TYPE_VIDEO				= 0x09,		// video
	/*      RTMP_PACKET_TYPE_...                0x0A */
	/*      RTMP_PACKET_TYPE_...                0x0B */
	/*      RTMP_PACKET_TYPE_...                0x0C */
	/*      RTMP_PACKET_TYPE_...                0x0D */
	/*      RTMP_PACKET_TYPE_...                0x0E */
	MSGHEADER_TYPE_FLEX_STREAM_SEND		= 0x0F,
	MSGHEADER_TYPE_FLEX_SHARED_OBJECT	= 0x10,
	MSGHEADER_TYPE_FLEX_MESSAGE			= 0x11,
	MSGHEADER_TYPE_INFO					= 0x12,
	MSGHEADER_TYPE_SHARED_OBJECT		= 0x13,
	MSGHEADER_TYPE_INVOKE				= 0x14,
	//RTMP_PACKET_TYPE_...                0x15 */
	MSGHEADER_TYPE_FLASH_VIDEO			= 0x16,
};

// rtmp协议类型
typedef enum RtmpProtocol
{
	RTMP_PROTOCOL_UNDEFINED = -1,
	RTMP_PROTOCOL_RTMP		= 0,
	RTMP_PROTOCOL_RTMPE		= RTMP_FEATURE_ENC,
	RTMP_PROTOCOL_RTMPT		= RTMP_FEATURE_HTTP,
	RTMP_PROTOCOL_RTMPS		= RTMP_FEATURE_SSL,
	RTMP_PROTOCOL_RTMPTE	= (RTMP_FEATURE_HTTP|RTMP_FEATURE_ENC),
	RTMP_PROTOCOL_RTMPTS	= (RTMP_FEATURE_HTTP|RTMP_FEATURE_SSL),
	RTMP_PROTOCOL_RTMFP		= RTMP_FEATURE_MFP

}RtmpProtocol_t;

enum ReadFlag
{
	READ_FLAG_HEADER		= 0x01,
	READ_FLAG_RESUME		= 0x02,
	READ_FLAG_NO_IGNORE		= 0x04,
	READ_FLAG_GOTKF			= 0x08,
	READ_FLAG_GOTFLVK		= 0x10,
	READ_FLAG_SEEKING		= 0x20,
};

enum ReadStatus
{
	READ_STATUS_COMPLETE	= -3,
	READ_STATUS_ERROR		= -2,
	READ_STATUS_EOF			= -1,
	READ_STATUS_IGNORE		= 0,
};

typedef struct RTMP_READ
{
	char *buf;
	char *bufpos;
	unsigned int buflen;
	uint32_t timestamp;
	uint8_t dataType;
	int flags;	// ReadFlag

	ReadStatus status;

	/* if bResume == TRUE */
	uint8_t initialFrameType;
	uint32_t nResumeTS;
	char *metaHeader;
	char *initialFrame;
	uint32_t nMetaHeaderSize;
	uint32_t nInitialFrameSize;
	uint32_t nIgnoredFrameCounter;
	uint32_t nIgnoredFlvFrameCounter;
} RTMP_READ;

struct RtmpChunk
{
	char c_header[RTMP_MAX_HEADER_SIZE];
	int c_headerSize;
	char *c_chunk;
	int c_chunkSize;
};

struct RtmpMsg
{
	uint8_t m_fmt;
	int m_csid;

	uint8_t m_msgType;
	uint8_t m_hasAbsTimestamp;	/* timestamp absolute or relative? */
	uint32_t m_nTimeStamp;		/* timestamp */
	int32_t m_nInfoField2;		/* last 4 bytes in a long header */
	uint32_t m_nBodySize;
	uint32_t m_nBytesRead;
	RtmpChunk *m_chunk;
	char *m_body;
};

//typedef struct RTMPChunk
//{
//	int c_headerSize;
//	int c_chunkSize;
//	char *c_chunk;
//	char c_header[RTMP_MAX_HEADER_SIZE];
//} RTMPChunk;
//
//typedef struct RTMPPacket
//{
//	uint8_t m_headerType;
//	uint8_t m_packetType;
//	uint8_t m_hasAbsTimestamp;	/* timestamp absolute or relative? */
//	int m_nChannel;
//	uint32_t m_nTimeStamp;		/* timestamp */
//	int32_t m_nInfoField2;		/* last 4 bytes in a long header */
//	uint32_t m_nBodySize;
//	uint32_t m_nBytesRead;
//	RTMPChunk *m_chunk;
//	char *m_body;
//} RTMPPacket;

typedef struct RTMPSockBuf
{
	SOCKET sb_socket;
	int sb_size;		/* number of unprocessed bytes in buffer */
	char *sb_start;		/* pointer into sb_pBuffer of next byte to process */
	char sb_buf[RTMP_BUFFER_CACHE_SIZE];	/* data read from socket */
	int sb_timedout;
	void *sb_ssl;
} RTMPSockBuf;

typedef struct RTMP_LNK
{
    AVal hostname;
    AVal sockshost;

    AVal playpath0;		/* parsed from URL */
	AVal playpath;		/* passed in explicitly */
    AVal tcUrl;
    AVal swfUrl;
    AVal pageUrl;
    AVal app;
    AVal auth;
    AVal flashVer;
    AVal subscribepath;
    AVal usherToken;
    AVal token;
    AVal pubUser;
    AVal pubPasswd;
    AMFObject extras;
    int edepth;

    int seekTime;
    int stopTime;

#define RTMP_LF_AUTH	0x0001	/* using auth param */
#define RTMP_LF_LIVE	0x0002	/* stream is live */
#define RTMP_LF_SWFV	0x0004	/* do SWF verification */
#define RTMP_LF_PLST	0x0008	/* send playlist before play */
#define RTMP_LF_BUFX	0x0010	/* toggle stream on BufferEmpty msg */
#define RTMP_LF_FTCU	0x0020	/* free tcUrl on close */
    int lFlags;

    int swfAge;

	int protocol;
    int timeout;		/* connection timeout in seconds */

#define RTMP_PUB_NAME   0x0001  /* send login to server */
#define RTMP_PUB_RESP   0x0002  /* send salted password hash */
#define RTMP_PUB_ALLOC  0x0004  /* allocated data for new tcUrl & app */
#define RTMP_PUB_CLEAN  0x0008  /* need to free allocated data for newer tcUrl & app at exit */
#define RTMP_PUB_CLATE  0x0010  /* late clean tcUrl & app at exit */
    int pFlags;

    unsigned short socksport;
    unsigned short port;

#ifdef CRYPTO
#define RTMP_SWF_HASHLEN	32
    void *dh;			/* for encryption */
    void *rc4keyIn;
    void *rc4keyOut;

    uint32_t SWFSize;
    uint8_t SWFHash[RTMP_SWF_HASHLEN];
    char SWFVerificationResponse[RTMP_SWF_HASHLEN+10];
#endif
} RTMP_LNK;


#endif