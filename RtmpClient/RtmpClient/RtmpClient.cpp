#include "RtmpClient.h"
#include <winsock2.h>

#ifdef WIN32
#pragma comment(lib,"ws2_32.lib")
#endif

#ifdef _MSC_VER	/* MSVC */
#define snprintf		_snprintf
#define strcasecmp		_stricmp
#define strncasecmp		_strnicmp
#define vsnprintf		_vsnprintf
#endif

static const int sg_iChunkSize = 128;
static const int packetSize[] = { 12, 8, 4, 1 };

const AVal sg_av_connect			= AVC("connect");
const AVal sg_av_app				= AVC("app");						// 字符串  客户端连接到的服务器端应用的名字
const AVal sg_av_type				= AVC("type");
const AVal sg_av_nonprivate			= AVC("nonprivate");				
const AVal sg_av_flashVer			= AVC("flashVer");					// 字符串  Flash Player版本号
const AVal sg_av_swfUrl				= AVC("swfUrl");					// 字符串  进行当前连接的SWF文件源地址
const AVal sg_av_tcUrl				= AVC("tcUrl");						// 字符串  服务器 URL
const AVal sg_av_fpad				= AVC("fpad");						// 布尔    如果使用了代理就是true
const AVal sg_av_audioCodecs		= AVC("audioCodecs");				// 数字    表明客户端所支持的音频编码
const AVal sg_av_videoCodecs		= AVC("videoCodecs");				// 数字    表明支持的视频编码
const AVal sg_av_videoFunction		= AVC("videoFunction");				// 数字    表明所支持的特殊视频方法
const AVal sg_av_pageUrl			= AVC("pageUrl");					// 字符串  SWF文件所加载的网页URL
const AVal sg_av_objectEncoding		= AVC("objectEncoding");			// 数字    AMF 编码方法 AMF3

static int DecodeInt32LE(const char *data)
{
	unsigned char *c = (unsigned char *)data;
	unsigned int val;

	val = (c[3] << 24) | (c[2] << 16) | (c[1] << 8) | c[0];
	return val;
}

static int EncodeInt32LE(char *output, int nVal)
{
	output[0] = nVal;
	nVal >>= 8;
	output[1] = nVal;
	nVal >>= 8;
	output[2] = nVal;
	nVal >>= 8;
	output[3] = nVal;
	return 4;
}

CRtmpClient::CRtmpClient()
{
	memset(&Link, 0, sizeof(Link));

	m_vecChannelsOut = NULL; 
	m_channelsAllocatedOut = 0;

	m_outChunkSize = 128;
}

CRtmpClient::~CRtmpClient()
{
}

bool CRtmpClient::Init()
{
	if (!InitSocket())
	{
		return false;
	}

	return true;
}

bool CRtmpClient::UnInit()
{
	if (!UnInitSocket())
	{
		return false;
	}

	return true;
}

int CRtmpClient::SetRtmpURL(IN const char* strRtmpUrl)
{
	char* strUrl = (char*)strRtmpUrl;
	AVal opt, arg;
	char *p1, *p2, *ptr = strchr(strUrl, ' ');
	int ret, len;
	unsigned int port = 0;

	if (ptr)
	{
		*ptr = '\0';
	}

	len = (int)strlen(strUrl);
	// 将url解析后设置到r;
	ret = ParseRtmpURL(Link.protocol, Link.hostname, port, Link.playpath0, Link.app, strUrl);
	if (!ret)
	{
		return ret;
	}
	Link.port = port;
	Link.playpath = Link.playpath0;

	while (ptr)
	{
		*ptr++ = '\0';
		p1 = ptr;
		p2 = strchr(p1, '=');
		if (!p2)
		{
			break;
		}
		opt.av_val = p1;
		opt.av_len = p2 - p1;
		*p2++ = '\0';
		arg.av_val = p2;
		ptr = strchr(p2, ' ');
		if (ptr)
		{
			*ptr = '\0';
			arg.av_len = ptr - p2;
			/* skip repeated spaces */
			while (ptr[1] == ' ')
			{
				*ptr++ = '\0';
			}
		}
		else
		{
			arg.av_len = (int)strlen(p2);
		}

		/* unescape */
		port = arg.av_len;
		for (p1 = p2; port >0;)
		{
			if (*p1 == '\\')
			{
				unsigned int c;
				if (port < 3)
				{
					return FALSE;
				}
				sscanf(p1 + 1, "%02x", &c);
				*p2++ = c;
				port -= 3;
				p1 += 3;
			}
			else
			{
				*p2++ = *p1++;
				port--;
			}
		}
		arg.av_len = p2 - arg.av_val;

		/*ret = RTMP_SetOpt(r, &opt, &arg);
		if (!ret)
		{
			return ret;
		}*/
	}

	if (!Link.tcUrl.av_len)
	{
		Link.tcUrl.av_val = strUrl;
		if (Link.app.av_len)
		{
			if (Link.app.av_val < strUrl + len)
			{
				/* if app is part of original url, just use it */
				Link.tcUrl.av_len = Link.app.av_len + (Link.app.av_val - strUrl);
			}
			else
			{
				/*len = Link.hostname.av_len + Link.app.av_len + sizeof("rtmpte://:65535/");
				Link.tcUrl.av_val = (char*)malloc(len);
				Link.tcUrl.av_len = snprintf(Link.tcUrl.av_val, len,
					"%s://%.*s:%d/%.*s",
					RTMPProtocolStringsLower[Link.protocol],
					Link.hostname.av_len, Link.hostname.av_val,
					Link.port,
					Link.app.av_len, Link.app.av_val);
				Link.lFlags |= RTMP_LF_FTCU;*/
			}
		}
		else
		{
			Link.tcUrl.av_len = (int)strlen(strUrl);
		}
	}

#ifdef CRYPTO
	if ((Link.lFlags & RTMP_LF_SWFV) && Link.swfUrl.av_len)
		RTMP_HashSWF(Link.swfUrl.av_val, &Link.SWFSize,
		(unsigned char *)Link.SWFHash, Link.swfAge);
#endif

	//SocksSetup(r, &Link.sockshost);

	if (Link.port == 0)
	{
		if (Link.protocol & RTMP_FEATURE_SSL)
		{
			Link.port = 443;
		}
		else if (Link.protocol & RTMP_FEATURE_HTTP)
		{
			Link.port = 80;
		}
		else
		{
			Link.port = 1935;
		}
	}
	return TRUE;
}

int CRtmpClient::SetRtmpURL2(IN const char* strUrl, IN const char* strPlayPath)
{
	return 1;
}

void CRtmpClient::EnablePushed(bool bPushed)
{
	if (bPushed)
	{
		Link.protocol |= RTMP_FEATURE_WRITE;
	}
}

bool CRtmpClient::Connect()
{
	// tcp 三次握手;
	if (!ConnectSocket())
	{
		return false;
	}

	// rtmp 握手;
	if (!HandShake())
	{
		return false;
	}

	// rtmp 连接
	if (!ConnectRtmp())
	{
		return false;
	}

	// rtmp 通道
	if (!ConnectStream())
	{
		return false;
	}

	return true;
}

int CRtmpClient::SendPacket(RTMPPacket * packet, bool bInQueue)
{
	const RTMPPacket *prevPacket;
	uint32_t last = 0;
	int nSize;
	int hSize, cSize;
	char *header, *hptr, *hend, hbuf[RTMP_MAX_HEADER_SIZE], c;
	uint32_t t;
	char *buffer, *tbuf = NULL, *toff = NULL;
	int nChunkSize;
	int tlen;

	if (packet->m_nChannel >= m_channelsAllocatedOut)
	{
		int n = packet->m_nChannel + 10;
		RTMPPacket **packets = (RTMPPacket **)realloc(m_vecChannelsOut, sizeof(RTMPPacket*) * n);
		if (!packets)
		{
			free(m_vecChannelsOut);
			m_vecChannelsOut = NULL;
			m_channelsAllocatedOut = 0;
			return FALSE;
		}
		m_vecChannelsOut = packets;
		memset(m_vecChannelsOut + m_channelsAllocatedOut, 0, sizeof(RTMPPacket*) * (n - m_channelsAllocatedOut));
		m_channelsAllocatedOut = n;
	}

	// 前一个packet存在且不是完整的ChunkMsgHeader，因此有可能需要调整块消息头的类型;
	// fmt字节;
	// case 0: chunk msg header 长度为11;
	// case 1: chunk msg header 长度为7;
	// case 2: chunk msg header 长度为3;
	// case 3: chunk msg header 长度为0;
	prevPacket = m_vecChannelsOut[packet->m_nChannel];
	if (prevPacket && packet->m_headerType != BASICHEADER_FMT_ZREO)
	{
		/* compress a bit by using the prev packet's attributes */
		// 获取ChunkMsgHeader类型，前一个Chunk与当前Chunk比较;
		if (prevPacket->m_nBodySize == packet->m_nBodySize
			&& prevPacket->m_packetType == packet->m_packetType
			&& packet->m_headerType == BASICHEADER_FMT_ONE)
		{
			// 如果前后两个块的大小、包类型都相同，则将块头类型fmt设为2;  
			// 即可省略消息长度、消息类型id、消息流id; 
			// 可以参考官方协议：流的分块 --- 6.1.2.3节;
			packet->m_headerType = BASICHEADER_FMT_TWO;
		}

		if (prevPacket->m_nTimeStamp == packet->m_nTimeStamp
			&& packet->m_headerType == BASICHEADER_FMT_TWO)
		{
			// 前后两个块的时间戳相同，且块头类型fmt为2，则相应的时间戳也可省略，因此将块头类型置为3;
			// 可以参考官方协议：流的分块 --- 6.1.2.4节; 
			packet->m_headerType = BASICHEADER_FMT_THREE;
		}
		last = prevPacket->m_nTimeStamp;
	}

	// 块头类型fmt取值0、1、2、3; 超过3就表示出错(fmt占二个字节);
	if (packet->m_headerType > 3)	/* sanity */
	{
		// RTMP_Log(RTMP_LOGERROR, "sanity failed!! trying to send header of type: 0x%02x.", (unsigned char)packet->m_headerType);
		return FALSE;
	}

	// 块头初始大小 = 基本头(1字节) + 块消息头大小(11/7/3/0) = [12, 8, 4, 1]; 
	// 块基本头是1-3字节，因此用变量cSize来表示剩下的0-2字节;
	// nSize 表示块头初始大小， hSize表示块头大小;  
	nSize = packetSize[packet->m_headerType];
	hSize = nSize;
	cSize = 0;
	t = packet->m_nTimeStamp - last;	// 时间戳增量;

	if (packet->m_body)
	{
		// m_body是指向负载数据首地址的指针; "-"号用于指针前移;
		// header块头的首指针; hend块头的尾指针;
		header = packet->m_body - nSize;
		hend = packet->m_body;
	}
	else
	{
		header = hbuf + 6;
		hend = hbuf + sizeof(hbuf);
	}

	if (packet->m_nChannel > 319)
	{
		// 块流id(cs id)大于319，则块基本头占3个字节;
		cSize = 2;
	}
	else if (packet->m_nChannel > 63)
	{
		// 块流id(cs id)在64与319之间，则块基本头占2个字节;
		cSize = 1;
	}

	// ChunkBasicHeader的长度比初始长度还要长;
	if (cSize)
	{
		// header指向块头;
		header -= cSize;
		// hSize加上ChunkBasicHeader的长度(比初始长度多出来的长度);  
		hSize += cSize;
	}

	// nSize>1表示块消息头至少有3个字节,即存在timestamp字段;
	// 相对TimeStamp大于0xffffff，此时需要使用ExtendTimeStamp;
	if (nSize > 1 && t >= 0xffffff)
	{
		header -= 4;
		hSize += 4;
	}

	hptr = header;
	// 把ChunkBasicHeader的Fmt类型左移6位;
	c = packet->m_headerType << 6;
	// 设置basic header的第一个字节值,前两位为fmt;
	// 可以参考官方协议：流的分块 --- 6.1.1节;
	switch (cSize)
	{
	case 0:
	{
		// 把ChunkBasicHeader的低6位设置成ChunkStreamID( cs id ) 
		c |= packet->m_nChannel;
	}
	break;
	case 1:
	{
		// 同理 但低6位设置成000000;
	}
	break;
	case 2:
	{
		// 同理 但低6位设置成000001;
		c |= 1;
	}
	break;
	}

	// 可以拆分成两句*hptr=c; hptr++;
	// 此时hptr指向第2个字节;
	*hptr++ = c;

	// 设置basic header的第二(三)个字节值;
	if (cSize)
	{
		// 将要放到第2字节的内容tmp;
		int tmp = packet->m_nChannel - 64;
		// 获取低位存储与第2字节;
		*hptr++ = tmp & 0xff;
		if (cSize == 2)
		{
			// ChunkBasicHeader是最大的3字节时,获取高位存储于最后1个字节（注意：排序使用大端序列，和主机相反）;
			*hptr++ = tmp >> 8;
		}
	}

	// ChunkMsgHeader长度为11、7、3 都含有timestamp(3字节);
	if (nSize > 1)
	{
		// 将时间戳(相对或绝对)转化为3个字节存入hptr 如果时间戳超过0xffffff 则后面还要填入Extend Timestamp;
		hptr = AMFObject::EncodeInt24(hptr, hend, t > 0xffffff ? 0xffffff : t);
	}

	// ChunkMsgHeader长度为11、7，都含有 msg length + msg type id;
	if (nSize > 4)
	{
		// 将消息长度(msg length)转化为3个字节存入hptr;
		hptr = AMFObject::EncodeInt24(hptr, hend, packet->m_nBodySize);
		*hptr++ = packet->m_packetType;
	}

	// ChunkMsgHeader长度为11 含有msg stream id(小端);
	if (nSize > 8)
	{
		hptr += EncodeInt32LE(hptr, packet->m_nInfoField2);
	}

	if (nSize > 1 && t >= 0xffffff)
	{
		hptr = AMFObject::EncodeInt32(hptr, hend, t);
	}

	// 到此为止 已经将块头填写好了;  
	// 此时nSize表示负载数据的长度 buffer是指向负载数据区的指针;
	nSize = packet->m_nBodySize;
	buffer = packet->m_body;
	nChunkSize = m_outChunkSize;	//Chunk大小 默认是128字节;

	//RTMP_Log(RTMP_LOGDEBUG2, "%s: fd=%d, size=%d", __FUNCTION__, m_sb.sb_socket, nSize);
	/* send all chunks in one HTTP request  使用HTTP协议 */
	if (Link.protocol & RTMP_FEATURE_HTTP)
	{
		// nSize: Message负载长度; nChunkSize：Chunk长度;  
		// 例nSize: 307  nChunkSize: 128 ;  
		// 可分为(307 + 128 - 1)/128 = 3个;
		// 为什么加 nChunkSize - 1？ 因为除法会只取整数部分！;
		int chunks = (nSize + nChunkSize - 1) / nChunkSize;
		// Chunk个数超过一个;
		if (chunks > 1)
		{
			// 注意: ChunkBasicHeader的长度 = cSize + 1;
			// 消息分n块后总的开销; 
			// n个ChunkBasicHeader 1个ChunkMsgHeader 1个Message负载;
			// 实际上只有第一个Chunk是完整的，剩下的只有ChunkBasicHeader;
			tlen = chunks * (cSize + 1) + nSize + hSize;
			tbuf = (char*)malloc(tlen);
			if (!tbuf)
			{
				return FALSE;
			}
			toff = tbuf;
		}
	}

	// 消息的负载 + 头;
	while (nSize + hSize)
	{
		// 消息负载大小 < Chunk大小(不用分块);
		if (nSize < nChunkSize)
		{
			// Chunk可能小于设定值;
			nChunkSize = nSize;
		}

		// 如果Link.protocol采用Http协议 则将RTMP包数据封装成多个Chunk 然后一次性发送;  
		// 否则每封装成一个块，就立即发送出去;
		if (tbuf)
		{
			// 将从Chunk头开始的nChunkSize + hSize个字节拷贝至toff中;
			// 这些拷贝的数据包括块头数据(hSize字节)和nChunkSize个负载数据;
			memcpy(toff, header, nChunkSize + hSize);
			toff += nChunkSize + hSize;
		}
		else
			// 负载数据长度不超过设定的块大小 不需要分块; 因此tbuf为NULL或者Link.protocol不采用Http;
		{
			// 直接将负载数据和块头数据发送出去;
			int iLen = send(m_sb.sb_socket, header, nChunkSize + hSize, 0);
			if (iLen != nChunkSize + hSize)
			{
				return false;
			}

			//RTMP_LogHexString(RTMP_LOGDEBUG2, (uint8_t *)header, hSize);
			//RTMP_LogHexString(RTMP_LOGDEBUG2, ((uint8_t *)header) + hSize, nChunkSize);
		}
		nSize -= nChunkSize;	// 消息负载长度 - Chunk负载长度;
		buffer += nChunkSize;	// buffer指针后移1个Chunk负载长度;
		hSize = 0;

		// 如果消息负载数据还没有发完 准备填充下一个块的块头数据; 
		if (nSize > 0)
		{
			header = buffer - 1;
			hSize = 1;
			if (cSize)
			{
				header -= cSize;
				hSize += cSize;
			}
			*header = (0xc0 | c);
			if (cSize)
			{
				int tmp = packet->m_nChannel - 64;
				header[1] = tmp & 0xff;
				if (cSize == 2)
				{
					header[2] = tmp >> 8;
				}
			}
		}
	}
	if (tbuf)
	{
		int iLen = send(m_sb.sb_socket, tbuf, toff - tbuf, 0);
		if (iLen != toff - tbuf)
		{
			return false;
		}

		free(tbuf);
		tbuf = NULL;
	}

	/* we invoked a remote method */
	if (packet->m_packetType == MSGHEADER_TYPE_TYPEINVOKE)
	{
		AVal method;
		char *ptr;
		ptr = packet->m_body + 1;
		AMFObject::DecodeString(method, ptr);
		//RTMP_Log(RTMP_LOGDEBUG, "Invoking %s", method.av_val);
		/* keep it in call queue till result arrives */
		if (bInQueue)
		{
			int txn;
			ptr += 3 + method.av_len;
			txn = (int)AMFObject::DecodeNumber(ptr);
			//AV_queue(&m_methodCalls, &m_numCalls, &method, txn);
		}
	}

	if (!m_vecChannelsOut[packet->m_nChannel])
	{
		m_vecChannelsOut[packet->m_nChannel] = (RTMPPacket*)malloc(sizeof(RTMPPacket));
	}
	memcpy(m_vecChannelsOut[packet->m_nChannel], packet, sizeof(RTMPPacket));
	return TRUE;

	return 1;
}

void CRtmpClient::Close()
{

}

// 解析rtmp地址
int CRtmpClient::ParseRtmpURL(OUT int& iProtocol,
	OUT AVal& strHost,
	OUT unsigned int& iPort,
	OUT AVal& strPlayPath,
	OUT AVal& strApp,
	IN const char* strRtmpUrl)
{
	char* strUrl = (char*)strRtmpUrl;
	char *p, *end, *col, *ques, *slash;

	////RTMP_Log(//RTMP_LogDEBUG, "RTMP_ParseURL");

	iProtocol = RTMP_PROTOCOL_RTMP;
	iPort = 0;
	strPlayPath.av_len = 0;
	strPlayPath.av_val = NULL;
	strApp.av_len = 0;
	strApp.av_val = NULL;

	/* Old School Parsing */

	/* look for usual :// pattern */
	p = strstr(strUrl, "://");
	if (!p)
	{
		////RTMP_Log(//RTMP_LogERROR, "RTMP URL: No :// in url!");
		return FALSE;
	}
	{
		int len = (int)(p - strUrl);

		if (len == 4 && strncasecmp(strUrl, "rtmp", 4) == 0)
		{
			iProtocol = RTMP_PROTOCOL_RTMP;
		}
		else if (len == 5 && strncasecmp(strUrl, "rtmpt", 5) == 0)
		{
			iProtocol = RTMP_PROTOCOL_RTMPT;
		}
		else if (len == 5 && strncasecmp(strUrl, "rtmps", 5) == 0)
		{
			iProtocol = RTMP_PROTOCOL_RTMPS;
		}
		else if (len == 5 && strncasecmp(strUrl, "rtmpe", 5) == 0)
		{
			iProtocol = RTMP_PROTOCOL_RTMPE;
		}
		else if (len == 5 && strncasecmp(strUrl, "rtmfp", 5) == 0)
		{
			iProtocol = RTMP_PROTOCOL_RTMFP;
		}
		else if (len == 6 && strncasecmp(strUrl, "rtmpte", 6) == 0)
		{
			iProtocol = RTMP_PROTOCOL_RTMPTE;
		}
		else if (len == 6 && strncasecmp(strUrl, "rtmpts", 6) == 0)
		{
			iProtocol = RTMP_PROTOCOL_RTMPTS;
		}
		else
		{
			////RTMP_Log(//RTMP_LogWARNING, "Unknown protocol!\n");
			goto parsehost;
		}
	}

	////RTMP_Log(//RTMP_LogDEBUG, "Parsed protocol: %d", iProtocol);

parsehost:
	/* let's get the hostname */
	p += 3;

	/* check for sudden death */
	if (*p == 0)
	{
		//RTMP_Log(//RTMP_LogWARNING, "No hostname in URL!");
		return FALSE;
	}

	end = p + strlen(p);
	col = strchr(p, ':');
	ques = strchr(p, '?');
	slash = strchr(p, '/');

	{
		int hostlen;
		if (slash)
		{
			hostlen = slash - p;
		}
		else
		{
			hostlen = end - p;
		}

		if (col && col - p < hostlen)
		{
			hostlen = col - p;
		}

		if (hostlen < 256)
		{
			strHost.av_val = p;
			strHost.av_len = hostlen;
			//RTMP_Log(//RTMP_LogDEBUG, "Parsed host    : %.*s", hostlen, host->av_val);
		}
		else
		{
			//RTMP_Log(//RTMP_LogWARNING, "Hostname exceeds 255 characters!");
		}

		p += hostlen;
	}

	/* get the port number if available */
	if (*p == ':')
	{
		unsigned int p2;
		p++;
		p2 = atoi(p);
		if (p2 > 65535)
		{
			//RTMP_Log(//RTMP_LogWARNING, "Invalid port number!");
		}
		else
		{
			iPort = p2;
		}
	}

	if (!slash)
	{
		//RTMP_Log(//RTMP_LogWARNING, "No application or playpath in URL!");
		return TRUE;
	}
	p = slash + 1;

	{
		/* parse application
		*
		* rtmp://host[:port]/app[/appinstance][/...]
		* application = app[/appinstance]
		*/

		char *slash2, *slash3 = NULL, *slash4 = NULL;
		int applen, appnamelen;

		slash2 = strchr(p, '/');
		if (slash2)
		{
			slash3 = strchr(slash2 + 1, '/');
		}
		if (slash3)
		{
			slash4 = strchr(slash3 + 1, '/');
		}

		applen = end - p; /* ondemand, pass all parameters as app */
		appnamelen = applen; /* ondemand length */

		if (ques && strstr(p, "slist="))   /* whatever it is, the '?' and slist= means we need to use everything as app and parse plapath from slist= */
		{
			appnamelen = ques - p;
		}
		else if (strncmp(p, "ondemand/", 9) == 0)
		{
			/* app = ondemand/foobar, only pass app=ondemand */
			applen = 8;
			appnamelen = 8;
		}
		else   /* app!=ondemand, so app is app[/appinstance] */
		{
			if (slash4)
			{
				appnamelen = slash4 - p;
			}
			else if (slash3)
			{
				appnamelen = slash3 - p;
			}
			else if (slash2)
			{
				appnamelen = slash2 - p;
			}

			applen = appnamelen;
		}

		strApp.av_val = p;
		strApp.av_len = applen;
		//RTMP_Log(//RTMP_LogDEBUG, "Parsed app     : %.*s", applen, p);

		p += appnamelen;
	}

	if (*p == '/')
	{
		p++;
	}

	if (end - p)
	{
		AVal av = { p, end - p };
		//RTMP_ParsePlaypath(&av, strPlayPath);
	}

	return 1;
}

int CRtmpClient::ParseRtmpURL2(OUT int& iProtocol,
	OUT AVal& strHost,
	OUT unsigned int& iPort,
	OUT AVal& strApp,
	IN const char* strUrl)
{
	return 1;
}

bool CRtmpClient::InitSocket()
{
#ifdef WIN32
	WORD	wdVersion;
	WSADATA wsaData;
	wdVersion = MAKEWORD(2, 2);
	if (WSAStartup(wdVersion, &wsaData) != 0)
	{
		return false;
	}
#endif
	return true;
}

bool CRtmpClient::UnInitSocket()
{
#ifdef WIN32
	WSACleanup();
#endif
	return true;
}

bool CRtmpClient::ConnectSocket()
{
	m_sb.sb_socket = socket(AF_INET, SOCK_STREAM, 0);
	char chHost[100] = { 0 };
	memcpy(chHost, Link.hostname.av_val, Link.hostname.av_len);

	HOSTENT* pHost = gethostbyname(chHost);
	if (!pHost)
	{
		printf("error in gethostbyname: %d\n", WSAGetLastError());
		return false;
	}
	else
	{
		printf("name: %s\naddrtype; %d\naddrlength: %d\n", pHost->h_name, pHost->h_addrtype, pHost->h_length);
		printf("ip address: %s\n", inet_ntoa(*(struct in_addr*)pHost->h_addr_list[0]));
	}

	SOCKADDR_IN addrSrv;
	addrSrv.sin_addr = (*(struct in_addr*)(pHost->h_addr_list[0]));
	addrSrv.sin_family = AF_INET;
	addrSrv.sin_port = htons(Link.port);
	int iRes = connect(m_sb.sb_socket, (SOCKADDR*)&addrSrv, sizeof(SOCKADDR));
	if (0 != iRes)
	{
		return false;
	}

	return true;
}

bool CRtmpClient::HandShake()
{
	uint32_t uptime = 0, suptime = 0;
	char chVersion;
	char strC[RTMP_SIG_SIZE + 1];	// C0+C1;
	char* strC1 = strC + 1;			// C1;
	char strC2[RTMP_SIG_SIZE];		// C2;
	char strS2[RTMP_SIG_SIZE];		// S2

	// RTMP协议版本号为0x03, 即C0数据;
	strC[0] = 0x03;
	// 获取系统时间（毫秒为单位），将其写入到C1中，占4个字节;
	//uptime = htonl(RTMP_GetTime());
	uptime = htonl(0);
	memcpy(strC1, &uptime, 4);
	// 上次对方返回请求的时间（毫秒为单位），将其写入到C1中;
	memset(&strC1[4], 0, 4);
	// 使用rand()循环生成1528个伪随机数;
	for (int i = 8; i < RTMP_SIG_SIZE; i++)
	{
		strC1[i] = (char)(rand() % 256);
	}

	// 发送握手数据C0和C1
	int iLen = send(m_sb.sb_socket, strC, RTMP_SIG_SIZE + 1, 0);
	if (iLen != RTMP_SIG_SIZE + 1)
	{
		return false;
	}

	// 读取数据报，长度为1，存入type中;
	// 此处读取的是服务器端发送来的S0，表示服务器使用的Rtmp版本;
	iLen = recv(m_sb.sb_socket, &chVersion, 1, 0);
	if (iLen != 1)
	{
		return false;
	}
	// 客户端要求的版本与服务器端提供的版本不一样;
	if (chVersion != strC[0])
	{
		return false;
	}

	// 读取服务器端发送过来的S1数据赋值给C2，并判断随机序列长度是否相同;
	iLen = recv(m_sb.sb_socket, strC2, RTMP_SIG_SIZE, 0);
	if (iLen != RTMP_SIG_SIZE)
	{
		return false;
	}

	/* decode server response */
	// 把serversig的前4个字节赋值给suptime;
	// S1中的time与C2中的time应该相同;
	memcpy(&suptime, strC2, 4);
	// 发送握手数据C2（1536个字节）给服务器;
	iLen = send(m_sb.sb_socket, strC2, RTMP_SIG_SIZE, 0);
	if (iLen != RTMP_SIG_SIZE)
	{
		return false;
	}

	Sleep(500);
	// 读取从服务器发送过来的握手数据S2（1536个字节）;
	iLen = recv(m_sb.sb_socket, strS2, RTMP_SIG_SIZE, 0);
	if (iLen != RTMP_SIG_SIZE)
	{
		return false;
	}

	// 比较客户端C1和服务器端S2的1536个数是否匹配;
	int iRes = memcmp(strS2, strC1, RTMP_SIG_SIZE);
	if(iRes != 0)
	{
		//RTMP_Log(RTMP_LOGWARNING, "%s, client signature does not match!", __FUNCTION__);
		return false;
	}

	return true;
}

bool CRtmpClient::ConnectRtmp()
{
	if (!SendSetChunkSize(sg_iChunkSize))
	{
		return false;
	}

	if (!SendConnect())
	{
		return false;
	}

	return true;
}

bool CRtmpClient::ConnectStream()
{
	return true;
}

bool CRtmpClient::SendSetChunkSize(int iChunkSize)
{
	char pBuff[4096] = { 0 };
	char* pEnd = pBuff + sizeof(pBuff);

	if ((Link.protocol & RTMP_FEATURE_WRITE) && m_bSendChunkSizeInfo)
	{
		RTMPPacket packet;
		packet.m_nChannel = 0x02;
		packet.m_headerType = BASICHEADER_FMT_ZREO;
		packet.m_packetType = MSGHEADER_TYPE_SET_CHUNK_SIZE;
		packet.m_nTimeStamp = 0;
		packet.m_nInfoField2 = 0;
		packet.m_hasAbsTimestamp = 0;
		packet.m_body = pBuff + RTMP_MAX_HEADER_SIZE;
		packet.m_nBodySize = 4;

		char* pBodyEn = packet.m_body;
		AMFObject::EncodeInt32(pBodyEn, pEnd, iChunkSize);
		// 发送 set chunk size;
		if (!SendPacket(&packet, FALSE))
		{
			return false;
		}
	}
	return true;
}

bool CRtmpClient::SendConnect()
{
	char pBuff[4096] = { 0 };
	char* pEnd = pBuff + sizeof(pBuff);

	RTMPPacket packet;
	packet.m_nChannel = 0x03;	/* control channel (invoke) */
	packet.m_headerType = BASICHEADER_FMT_ZREO;
	packet.m_packetType = MSGHEADER_TYPE_TYPEINVOKE;
	packet.m_nTimeStamp = 0;
	packet.m_nInfoField2 = 0;
	packet.m_hasAbsTimestamp = 0;
	packet.m_body = pBuff + RTMP_MAX_HEADER_SIZE;

	char* pBodyEn = packet.m_body;
	// Command Name: "connect"
	pBodyEn = AMFObject::EncodeString(pBodyEn, pEnd, sg_av_connect);
	// Transaction ID: "connect"
	pBodyEn = AMFObject::EncodeNumber(pBodyEn, pEnd, 1);

	// Command Object 
	{
		AMFObject objCommand;
		if(Link.app.av_len)
		{
			AMFObjectProperty objTemp;
			objTemp.SetName(sg_av_app);
			objTemp.SetType(AMF_STRING);
			objTemp.SetString(Link.app);
			objCommand.AddProp(&objTemp);
		}
		if (Link.protocol & RTMP_FEATURE_WRITE)
		{
			AMFObjectProperty objTemp;
			objTemp.SetName(sg_av_type);
			objTemp.SetType(AMF_STRING);
			objTemp.SetString(sg_av_nonprivate);
			objCommand.AddProp(&objTemp);
		}
		if (Link.flashVer.av_len)
		{
			AMFObjectProperty objTemp;
			objTemp.SetName(sg_av_flashVer);
			objTemp.SetType(AMF_STRING);
			objTemp.SetString(Link.flashVer);
			objCommand.AddProp(&objTemp);
		}
		if (Link.swfUrl.av_len)
		{
			AMFObjectProperty objTemp;
			objTemp.SetName(sg_av_swfUrl);
			objTemp.SetType(AMF_STRING);
			objTemp.SetString(Link.swfUrl);
			objCommand.AddProp(&objTemp);
		}
		if (Link.tcUrl.av_len)
		{
			AMFObjectProperty objTemp;
			objTemp.SetName(sg_av_tcUrl);
			objTemp.SetType(AMF_STRING);
			objTemp.SetString(Link.tcUrl);
			objCommand.AddProp(&objTemp);
		}

		pBodyEn = objCommand.Encode(pBodyEn, pEnd);
	}

	//if (!(r->Link.protocol & RTMP_FEATURE_WRITE))
	//{
	//	pBodyEn = AMF_EncodeNamedBoolean(pBodyEn, pEnd, &av_fpad, FALSE);
	//	if (!pBodyEn)
	//	{
	//		return FALSE;
	//	}
	//	pBodyEn = AMF_EncodeNamedNumber(pBodyEn, pEnd, &av_capabilities, 15.0);
	//	if (!pBodyEn)
	//	{
	//		return FALSE;
	//	}
	//	pBodyEn = AMF_EncodeNamedNumber(pBodyEn, pEnd, &av_audioCodecs, r->m_fAudioCodecs);
	//	if (!pBodyEn)
	//	{
	//		return FALSE;
	//	}
	//	pBodyEn = AMF_EncodeNamedNumber(pBodyEn, pEnd, &av_videoCodecs, r->m_fVideoCodecs);
	//	if (!pBodyEn)
	//	{
	//		return FALSE;
	//	}
	//	pBodyEn = AMF_EncodeNamedNumber(pBodyEn, pEnd, &av_videoFunction, 1.0);
	//	if (!pBodyEn)
	//	{
	//		return FALSE;
	//	}
	//	if (r->Link.pageUrl.av_len)
	//	{
	//		pBodyEn = AMF_EncodeNamedString(pBodyEn, pEnd, &av_pageUrl, &r->Link.pageUrl);
	//		if (!pBodyEn)
	//		{
	//			return FALSE;
	//		}
	//	}
	//}
	//if (r->m_fEncoding != 0.0 || r->m_bSendEncoding)
	//{
	//	/* AMF0, AMF3 not fully supported yet */
	//	pBodyEn = AMF_EncodeNamedNumber(pBodyEn, pEnd, &av_objectEncoding, r->m_fEncoding);
	//	if (!pBodyEn)
	//	{
	//		return FALSE;
	//	}
	//}
	//if (pBodyEn + 3 >= pEnd)
	//{
	//	return FALSE;
	//}
	//*pBodyEn++ = 0;
	//*pBodyEn++ = 0;			/* end of object - 0x00 0x00 0x09 */
	//*pBodyEn++ = AMF_OBJECT_END;

	///* add auth string */
	//if (r->Link.auth.av_len)
	//{
	//	pBodyEn = AMF_EncodeBoolean(pBodyEn, pEnd, r->Link.lFlags & RTMP_LF_AUTH);
	//	if (!pBodyEn)
	//	{
	//		return FALSE;
	//	}
	//	pBodyEn = AMF_EncodeString(pBodyEn, pEnd, &r->Link.auth);
	//	if (!pBodyEn)
	//	{
	//		return FALSE;
	//	}
	//}
	//if (r->Link.extras.o_num)
	//{
	//	int i;
	//	for (i = 0; i < r->Link.extras.o_num; i++)
	//	{
	//		pBodyEn = AMFProp_Encode(&r->Link.extras.o_props[i], pBodyEn, pEnd);
	//		if (!pBodyEn)
	//		{
	//			return FALSE;
	//		}
	//	}
	//}
	packet.m_nBodySize = pBodyEn - packet.m_body;

	// 发送 connect(streamid);
	if (!SendPacket(&packet, TRUE))
	{
		return false;
	}

	return false;
}
