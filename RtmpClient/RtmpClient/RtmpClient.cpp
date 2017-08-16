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
const AVal sg_av_app				= AVC("app");						// �ַ���  �ͻ������ӵ��ķ�������Ӧ�õ�����
const AVal sg_av_type				= AVC("type");
const AVal sg_av_nonprivate			= AVC("nonprivate");				
const AVal sg_av_flashVer			= AVC("flashVer");					// �ַ���  Flash Player�汾��
const AVal sg_av_swfUrl				= AVC("swfUrl");					// �ַ���  ���е�ǰ���ӵ�SWF�ļ�Դ��ַ
const AVal sg_av_tcUrl				= AVC("tcUrl");						// �ַ���  ������ URL
const AVal sg_av_fpad				= AVC("fpad");						// ����    ���ʹ���˴������true
const AVal sg_av_audioCodecs		= AVC("audioCodecs");				// ����    �����ͻ�����֧�ֵ���Ƶ����
const AVal sg_av_videoCodecs		= AVC("videoCodecs");				// ����    ����֧�ֵ���Ƶ����
const AVal sg_av_videoFunction		= AVC("videoFunction");				// ����    ������֧�ֵ�������Ƶ����
const AVal sg_av_pageUrl			= AVC("pageUrl");					// �ַ���  SWF�ļ������ص���ҳURL
const AVal sg_av_objectEncoding		= AVC("objectEncoding");			// ����    AMF ���뷽�� AMF3

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
	// ��url���������õ�r;
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
	// tcp ��������;
	if (!ConnectSocket())
	{
		return false;
	}

	// rtmp ����;
	if (!HandShake())
	{
		return false;
	}

	// rtmp ����
	if (!ConnectRtmp())
	{
		return false;
	}

	// rtmp ͨ��
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

	// ǰһ��packet�����Ҳ���������ChunkMsgHeader������п�����Ҫ��������Ϣͷ������;
	// fmt�ֽ�;
	// case 0: chunk msg header ����Ϊ11;
	// case 1: chunk msg header ����Ϊ7;
	// case 2: chunk msg header ����Ϊ3;
	// case 3: chunk msg header ����Ϊ0;
	prevPacket = m_vecChannelsOut[packet->m_nChannel];
	if (prevPacket && packet->m_headerType != BASICHEADER_FMT_ZREO)
	{
		/* compress a bit by using the prev packet's attributes */
		// ��ȡChunkMsgHeader���ͣ�ǰһ��Chunk�뵱ǰChunk�Ƚ�;
		if (prevPacket->m_nBodySize == packet->m_nBodySize
			&& prevPacket->m_packetType == packet->m_packetType
			&& packet->m_headerType == BASICHEADER_FMT_ONE)
		{
			// ���ǰ��������Ĵ�С�������Ͷ���ͬ���򽫿�ͷ����fmt��Ϊ2;  
			// ����ʡ����Ϣ���ȡ���Ϣ����id����Ϣ��id; 
			// ���Բο��ٷ�Э�飺���ķֿ� --- 6.1.2.3��;
			packet->m_headerType = BASICHEADER_FMT_TWO;
		}

		if (prevPacket->m_nTimeStamp == packet->m_nTimeStamp
			&& packet->m_headerType == BASICHEADER_FMT_TWO)
		{
			// ǰ���������ʱ�����ͬ���ҿ�ͷ����fmtΪ2������Ӧ��ʱ���Ҳ��ʡ�ԣ���˽���ͷ������Ϊ3;
			// ���Բο��ٷ�Э�飺���ķֿ� --- 6.1.2.4��; 
			packet->m_headerType = BASICHEADER_FMT_THREE;
		}
		last = prevPacket->m_nTimeStamp;
	}

	// ��ͷ����fmtȡֵ0��1��2��3; ����3�ͱ�ʾ����(fmtռ�����ֽ�);
	if (packet->m_headerType > 3)	/* sanity */
	{
		// RTMP_Log(RTMP_LOGERROR, "sanity failed!! trying to send header of type: 0x%02x.", (unsigned char)packet->m_headerType);
		return FALSE;
	}

	// ��ͷ��ʼ��С = ����ͷ(1�ֽ�) + ����Ϣͷ��С(11/7/3/0) = [12, 8, 4, 1]; 
	// �����ͷ��1-3�ֽڣ�����ñ���cSize����ʾʣ�µ�0-2�ֽ�;
	// nSize ��ʾ��ͷ��ʼ��С�� hSize��ʾ��ͷ��С;  
	nSize = packetSize[packet->m_headerType];
	hSize = nSize;
	cSize = 0;
	t = packet->m_nTimeStamp - last;	// ʱ�������;

	if (packet->m_body)
	{
		// m_body��ָ���������׵�ַ��ָ��; "-"������ָ��ǰ��;
		// header��ͷ����ָ��; hend��ͷ��βָ��;
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
		// ����id(cs id)����319��������ͷռ3���ֽ�;
		cSize = 2;
	}
	else if (packet->m_nChannel > 63)
	{
		// ����id(cs id)��64��319֮�䣬������ͷռ2���ֽ�;
		cSize = 1;
	}

	// ChunkBasicHeader�ĳ��ȱȳ�ʼ���Ȼ�Ҫ��;
	if (cSize)
	{
		// headerָ���ͷ;
		header -= cSize;
		// hSize����ChunkBasicHeader�ĳ���(�ȳ�ʼ���ȶ�����ĳ���);  
		hSize += cSize;
	}

	// nSize>1��ʾ����Ϣͷ������3���ֽ�,������timestamp�ֶ�;
	// ���TimeStamp����0xffffff����ʱ��Ҫʹ��ExtendTimeStamp;
	if (nSize > 1 && t >= 0xffffff)
	{
		header -= 4;
		hSize += 4;
	}

	hptr = header;
	// ��ChunkBasicHeader��Fmt��������6λ;
	c = packet->m_headerType << 6;
	// ����basic header�ĵ�һ���ֽ�ֵ,ǰ��λΪfmt;
	// ���Բο��ٷ�Э�飺���ķֿ� --- 6.1.1��;
	switch (cSize)
	{
	case 0:
	{
		// ��ChunkBasicHeader�ĵ�6λ���ó�ChunkStreamID( cs id ) 
		c |= packet->m_nChannel;
	}
	break;
	case 1:
	{
		// ͬ�� ����6λ���ó�000000;
	}
	break;
	case 2:
	{
		// ͬ�� ����6λ���ó�000001;
		c |= 1;
	}
	break;
	}

	// ���Բ�ֳ�����*hptr=c; hptr++;
	// ��ʱhptrָ���2���ֽ�;
	*hptr++ = c;

	// ����basic header�ĵڶ�(��)���ֽ�ֵ;
	if (cSize)
	{
		// ��Ҫ�ŵ���2�ֽڵ�����tmp;
		int tmp = packet->m_nChannel - 64;
		// ��ȡ��λ�洢���2�ֽ�;
		*hptr++ = tmp & 0xff;
		if (cSize == 2)
		{
			// ChunkBasicHeader������3�ֽ�ʱ,��ȡ��λ�洢�����1���ֽڣ�ע�⣺����ʹ�ô�����У��������෴��;
			*hptr++ = tmp >> 8;
		}
	}

	// ChunkMsgHeader����Ϊ11��7��3 ������timestamp(3�ֽ�);
	if (nSize > 1)
	{
		// ��ʱ���(��Ի����)ת��Ϊ3���ֽڴ���hptr ���ʱ�������0xffffff ����滹Ҫ����Extend Timestamp;
		hptr = AMFObject::EncodeInt24(hptr, hend, t > 0xffffff ? 0xffffff : t);
	}

	// ChunkMsgHeader����Ϊ11��7�������� msg length + msg type id;
	if (nSize > 4)
	{
		// ����Ϣ����(msg length)ת��Ϊ3���ֽڴ���hptr;
		hptr = AMFObject::EncodeInt24(hptr, hend, packet->m_nBodySize);
		*hptr++ = packet->m_packetType;
	}

	// ChunkMsgHeader����Ϊ11 ����msg stream id(С��);
	if (nSize > 8)
	{
		hptr += EncodeInt32LE(hptr, packet->m_nInfoField2);
	}

	if (nSize > 1 && t >= 0xffffff)
	{
		hptr = AMFObject::EncodeInt32(hptr, hend, t);
	}

	// ����Ϊֹ �Ѿ�����ͷ��д����;  
	// ��ʱnSize��ʾ�������ݵĳ��� buffer��ָ������������ָ��;
	nSize = packet->m_nBodySize;
	buffer = packet->m_body;
	nChunkSize = m_outChunkSize;	//Chunk��С Ĭ����128�ֽ�;

	//RTMP_Log(RTMP_LOGDEBUG2, "%s: fd=%d, size=%d", __FUNCTION__, m_sb.sb_socket, nSize);
	/* send all chunks in one HTTP request  ʹ��HTTPЭ�� */
	if (Link.protocol & RTMP_FEATURE_HTTP)
	{
		// nSize: Message���س���; nChunkSize��Chunk����;  
		// ��nSize: 307  nChunkSize: 128 ;  
		// �ɷ�Ϊ(307 + 128 - 1)/128 = 3��;
		// Ϊʲô�� nChunkSize - 1�� ��Ϊ������ֻȡ�������֣�;
		int chunks = (nSize + nChunkSize - 1) / nChunkSize;
		// Chunk��������һ��;
		if (chunks > 1)
		{
			// ע��: ChunkBasicHeader�ĳ��� = cSize + 1;
			// ��Ϣ��n����ܵĿ���; 
			// n��ChunkBasicHeader 1��ChunkMsgHeader 1��Message����;
			// ʵ����ֻ�е�һ��Chunk�������ģ�ʣ�µ�ֻ��ChunkBasicHeader;
			tlen = chunks * (cSize + 1) + nSize + hSize;
			tbuf = (char*)malloc(tlen);
			if (!tbuf)
			{
				return FALSE;
			}
			toff = tbuf;
		}
	}

	// ��Ϣ�ĸ��� + ͷ;
	while (nSize + hSize)
	{
		// ��Ϣ���ش�С < Chunk��С(���÷ֿ�);
		if (nSize < nChunkSize)
		{
			// Chunk����С���趨ֵ;
			nChunkSize = nSize;
		}

		// ���Link.protocol����HttpЭ�� ��RTMP�����ݷ�װ�ɶ��Chunk Ȼ��һ���Է���;  
		// ����ÿ��װ��һ���飬���������ͳ�ȥ;
		if (tbuf)
		{
			// ����Chunkͷ��ʼ��nChunkSize + hSize���ֽڿ�����toff��;
			// ��Щ���������ݰ�����ͷ����(hSize�ֽ�)��nChunkSize����������;
			memcpy(toff, header, nChunkSize + hSize);
			toff += nChunkSize + hSize;
		}
		else
			// �������ݳ��Ȳ������趨�Ŀ��С ����Ҫ�ֿ�; ���tbufΪNULL����Link.protocol������Http;
		{
			// ֱ�ӽ��������ݺͿ�ͷ���ݷ��ͳ�ȥ;
			int iLen = send(m_sb.sb_socket, header, nChunkSize + hSize, 0);
			if (iLen != nChunkSize + hSize)
			{
				return false;
			}

			//RTMP_LogHexString(RTMP_LOGDEBUG2, (uint8_t *)header, hSize);
			//RTMP_LogHexString(RTMP_LOGDEBUG2, ((uint8_t *)header) + hSize, nChunkSize);
		}
		nSize -= nChunkSize;	// ��Ϣ���س��� - Chunk���س���;
		buffer += nChunkSize;	// bufferָ�����1��Chunk���س���;
		hSize = 0;

		// �����Ϣ�������ݻ�û�з��� ׼�������һ����Ŀ�ͷ����; 
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

// ����rtmp��ַ
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

	// RTMPЭ��汾��Ϊ0x03, ��C0����;
	strC[0] = 0x03;
	// ��ȡϵͳʱ�䣨����Ϊ��λ��������д�뵽C1�У�ռ4���ֽ�;
	//uptime = htonl(RTMP_GetTime());
	uptime = htonl(0);
	memcpy(strC1, &uptime, 4);
	// �ϴζԷ����������ʱ�䣨����Ϊ��λ��������д�뵽C1��;
	memset(&strC1[4], 0, 4);
	// ʹ��rand()ѭ������1528��α�����;
	for (int i = 8; i < RTMP_SIG_SIZE; i++)
	{
		strC1[i] = (char)(rand() % 256);
	}

	// ������������C0��C1
	int iLen = send(m_sb.sb_socket, strC, RTMP_SIG_SIZE + 1, 0);
	if (iLen != RTMP_SIG_SIZE + 1)
	{
		return false;
	}

	// ��ȡ���ݱ�������Ϊ1������type��;
	// �˴���ȡ���Ƿ������˷�������S0����ʾ������ʹ�õ�Rtmp�汾;
	iLen = recv(m_sb.sb_socket, &chVersion, 1, 0);
	if (iLen != 1)
	{
		return false;
	}
	// �ͻ���Ҫ��İ汾����������ṩ�İ汾��һ��;
	if (chVersion != strC[0])
	{
		return false;
	}

	// ��ȡ�������˷��͹�����S1���ݸ�ֵ��C2�����ж�������г����Ƿ���ͬ;
	iLen = recv(m_sb.sb_socket, strC2, RTMP_SIG_SIZE, 0);
	if (iLen != RTMP_SIG_SIZE)
	{
		return false;
	}

	/* decode server response */
	// ��serversig��ǰ4���ֽڸ�ֵ��suptime;
	// S1�е�time��C2�е�timeӦ����ͬ;
	memcpy(&suptime, strC2, 4);
	// ������������C2��1536���ֽڣ���������;
	iLen = send(m_sb.sb_socket, strC2, RTMP_SIG_SIZE, 0);
	if (iLen != RTMP_SIG_SIZE)
	{
		return false;
	}

	Sleep(500);
	// ��ȡ�ӷ��������͹�������������S2��1536���ֽڣ�;
	iLen = recv(m_sb.sb_socket, strS2, RTMP_SIG_SIZE, 0);
	if (iLen != RTMP_SIG_SIZE)
	{
		return false;
	}

	// �ȽϿͻ���C1�ͷ�������S2��1536�����Ƿ�ƥ��;
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
		// ���� set chunk size;
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

	// ���� connect(streamid);
	if (!SendPacket(&packet, TRUE))
	{
		return false;
	}

	return false;
}
