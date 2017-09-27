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

// ����ȫ�ֱ���;
#define SG_AVC(x)		static const AVal sg_av_##x = AVC(#x)			// �ȼ��� static const AVal sg_av_connect = AVC("connect")

SG_AVC(connect);
SG_AVC(app);						// �ַ���  �ͻ������ӵ��ķ�������Ӧ�õ�����
SG_AVC(type);
SG_AVC(nonprivate);
SG_AVC(flashVer);					// �ַ���  Flash Player�汾��
SG_AVC(swfUrl);						// �ַ���  ���е�ǰ���ӵ�SWF�ļ�Դ��ַ
SG_AVC(tcUrl);						// �ַ���  ������ URL
SG_AVC(fpad);						// ����    ���ʹ���˴������true
SG_AVC(audioCodecs);				// ����    �����ͻ�����֧�ֵ���Ƶ����
SG_AVC(videoCodecs);				// ����    ����֧�ֵ���Ƶ����
SG_AVC(videoFunction);				// ����    ������֧�ֵ�������Ƶ����
SG_AVC(pageUrl);					// �ַ���  SWF�ļ������ص���ҳURL
SG_AVC(objectEncoding);				// ����    AMF ���뷽�� AMF3
SG_AVC(_result);						// result
SG_AVC(capabilities);
SG_AVC(secureToken);
SG_AVC(secureTokenResponse);
SG_AVC(releaseStream);
SG_AVC(FCPublish);
SG_AVC(createStream);
SG_AVC(publish);
SG_AVC(live);
SG_AVC(set_playlist);
SG_AVC(0);
SG_AVC(play);
SG_AVC(onBWDone);
SG_AVC(onFCSubscribe);
SG_AVC(onFCUnsubscribe);
SG_AVC(pong);		// ?
SG_AVC(_onbwcheck);
SG_AVC(_onbwdone);
SG_AVC(_error);
SG_AVC(close);
SG_AVC(code);
SG_AVC(level);
SG_AVC(description);
SG_AVC(onStatus);
SG_AVC(playlist_ready);
SG_AVC(_checkbw);
SG_AVC(pause);
SG_AVC(deleteStream);
SG_AVC(FCSubscribe);
SG_AVC(FCUnpublish);


static const AVal sg_av_NetStream_Failed					= AVC("NetStream.Failed");
static const AVal sg_av_NetStream_Play_Failed				= AVC("NetStream.Play.Failed");
static const AVal sg_av_NetStream_Play_StreamNotFound		= AVC("NetStream.Play.StreamNotFound");
static const AVal sg_av_NetConnection_Connect_InvalidApp	= AVC("NetConnection.Connect.InvalidApp");
static const AVal sg_av_NetConnection_Connect_Closed		= AVC("NetConnection.Connect.Closed");
static const AVal sg_av_NetStream_Play_Start				= AVC("NetStream.Play.Start");
static const AVal sg_av_NetStream_Play_Complete				= AVC("NetStream.Play.Complete");
static const AVal sg_av_NetStream_Play_Stop					= AVC("NetStream.Play.Stop");
static const AVal sg_av_NetStream_Seek_Notify				= AVC("NetStream.Seek.Notify");
static const AVal sg_av_NetStream_Pause_Notify				= AVC("NetStream.Pause.Notify");
static const AVal sg_av_NetStream_Play_PublishNotify		= AVC("NetStream.Play.PublishNotify");
static const AVal sg_av_NetStream_Play_UnpublishNotify		= AVC("NetStream.Play.UnpublishNotify");
static const AVal sg_av_NetStream_Publish_Start				= AVC("NetStream.Publish.Start");
static const AVal sg_av_NetStream_Publish_Rejected			= AVC("NetStream.Publish.Rejected");
static const AVal sg_av_NetStream_Publish_Denied			= AVC("NetStream.Publish.Denied");
static const AVal sg_av_NetConnection_Connect_Rejected		= AVC("NetConnection.Connect.Rejected");
static const AVal sg_av_NetStream_Authenticate_UsherToken	= AVC("NetStream.Authenticate.UsherToken");

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
	memset(this, 0, sizeof(CRtmpClient));
	m_sb.sb_socket = -1;
	m_inChunkSize = RTMP_DEFAULT_CHUNKSIZE;
	m_outChunkSize = RTMP_DEFAULT_CHUNKSIZE;
	m_bSendChunkSizeInfo = 1;
	m_nBufferMS = 30000;
	m_nClientBW = 2500000;
	m_nClientBW2 = 2;
	m_nServerBW = 2500000;
	m_fAudioCodecs = 3191.0;
	m_fVideoCodecs = 252.0;
	Link.timeout = 30;
	Link.swfAge = 30;
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
	if (!ConnectStream(0))
	{
		return false;
	}

	return true;
}

bool CRtmpClient::IsConnected()
{
	return m_sb.sb_socket != -1;
}

bool CRtmpClient::SendPacket(IN RtmpMsg* const packet, IN bool bInQueue)
{
	const RtmpMsg *prevPacket;
	uint32_t last = 0;
	int nSize;
	int hSize, cSize;
	char *header, *hptr, *hend, hbuf[RTMP_MAX_HEADER_SIZE], c;
	uint32_t t;
	char *buffer, *tbuf = NULL, *toff = NULL;
	int nChunkSize;
	int tlen;

	if (packet->m_csid >= m_channelsAllocatedOut)
	{
		int n = packet->m_csid + 10;
		RtmpMsg **packets = (RtmpMsg **)realloc(m_vecChannelsOut, sizeof(RtmpMsg*) * n);
		if (!packets)
		{
			free(m_vecChannelsOut);
			m_vecChannelsOut = NULL;
			m_channelsAllocatedOut = 0;
			return false;
		}
		m_vecChannelsOut = packets;
		memset(m_vecChannelsOut + m_channelsAllocatedOut, 0, sizeof(RtmpMsg*) * (n - m_channelsAllocatedOut));
		m_channelsAllocatedOut = n;
	}

	// ǰһ��packet�����Ҳ���������ChunkMsgHeader������п�����Ҫ��������Ϣͷ������;
	// fmt�ֽ�;
	// case 0: chunk msg header ����Ϊ11;
	// case 1: chunk msg header ����Ϊ7;
	// case 2: chunk msg header ����Ϊ3;
	// case 3: chunk msg header ����Ϊ0;
	prevPacket = m_vecChannelsOut[packet->m_csid];
	if (prevPacket && packet->m_fmt != BASICHEADER_FMT_ZREO)
	{
		/* compress a bit by using the prev packet's attributes */
		// ��ȡChunkMsgHeader���ͣ�ǰһ��Chunk�뵱ǰChunk�Ƚ�;
		if (prevPacket->m_nBodySize == packet->m_nBodySize
			&& prevPacket->m_msgType == packet->m_msgType
			&& packet->m_fmt == BASICHEADER_FMT_ONE)
		{
			// ���ǰ��������Ĵ�С�������Ͷ���ͬ���򽫿�ͷ����fmt��Ϊ2;  
			// ����ʡ����Ϣ���ȡ���Ϣ����id����Ϣ��id; 
			// ���Բο��ٷ�Э�飺���ķֿ� --- 6.1.2.3��;
			packet->m_fmt = BASICHEADER_FMT_TWO;
		}

		if (prevPacket->m_nTimeStamp == packet->m_nTimeStamp
			&& packet->m_fmt == BASICHEADER_FMT_TWO)
		{
			// ǰ���������ʱ�����ͬ���ҿ�ͷ����fmtΪ2������Ӧ��ʱ���Ҳ��ʡ�ԣ���˽���ͷ������Ϊ3;
			// ���Բο��ٷ�Э�飺���ķֿ� --- 6.1.2.4��; 
			packet->m_fmt = BASICHEADER_FMT_THREE;
		}
		last = prevPacket->m_nTimeStamp;
	}

	// ��ͷ����fmtȡֵ0��1��2��3; ����3�ͱ�ʾ����(fmtռ�����ֽ�);
	if (packet->m_fmt > 3)	/* sanity */
	{
		// RTMP_Log(RTMP_LOGERROR, "sanity failed!! trying to send header of type: 0x%02x.", (unsigned char)packet->m_headerType);
		return FALSE;
	}

	// ��ͷ��ʼ��С = ����ͷ(1�ֽ�) + ����Ϣͷ��С(11/7/3/0) = [12, 8, 4, 1]; 
	// �����ͷ��1-3�ֽڣ�����ñ���cSize����ʾʣ�µ�0-2�ֽ�;
	// nSize ��ʾ��ͷ��ʼ��С�� hSize��ʾ��ͷ��С;  
	nSize = packetSize[packet->m_fmt];
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

	if (packet->m_csid > 319)
	{
		// ����id(cs id)����319��������ͷռ3���ֽ�;
		cSize = 2;
	}
	else if (packet->m_csid > 63)
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
	c = packet->m_fmt << 6;
	// ����basic header�ĵ�һ���ֽ�ֵ,ǰ��λΪfmt;
	// ���Բο��ٷ�Э�飺���ķֿ� --- 6.1.1��;
	switch (cSize)
	{
	case 0:
	{
		// ��ChunkBasicHeader�ĵ�6λ���ó�ChunkStreamID( cs id ) 
		c |= packet->m_csid;
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
		int tmp = packet->m_csid - 64;
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
		*hptr++ = packet->m_msgType;
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
				return false;
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
				printf("%d", WSAGetLastError());
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
				int tmp = packet->m_csid - 64;
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
	if (packet->m_msgType == MSGHEADER_TYPE_INVOKE)
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
			AV_queue(&m_methodCalls, &m_numCalls, &method, txn);
		}
	}

	if (!m_vecChannelsOut[packet->m_csid])
	{
		m_vecChannelsOut[packet->m_csid] = (RtmpMsg*)malloc(sizeof(RtmpMsg));
	}
	memcpy(m_vecChannelsOut[packet->m_csid], packet, sizeof(RtmpMsg));

	return true;
}

bool CRtmpClient::ReadPacket(OUT RtmpMsg * packet)
{
	uint8_t hbuf[RTMP_MAX_HEADER_SIZE] = { 0 };
	char *header = (char *)hbuf;
	int nSize, hSize, nToRead, nChunk;
	int didAlloc = FALSE;
	int extendedTimestamp = 0;

	//RTMP_Log(RTMP_LOGDEBUG2, "%s: fd=%d", __FUNCTION__, m_sb.sb_socket);

	int iLen = recv(m_sb.sb_socket, (char*)hbuf, 1, 0);
	if (iLen <= 0)
	{
		//RTMP_Log(RTMP_LOGDEBUG, "%s, failed to read RTMP packet header", __FUNCTION__);
		return false;
	}

	packet->m_fmt = (hbuf[0] & 0xc0) >> 6;
	packet->m_csid = (hbuf[0] & 0x3f);
	header++;
	if (packet->m_csid == 0)
	{
		iLen = recv(m_sb.sb_socket, (char *)&hbuf[1], 1, 0);
		//if (ReadN(r, (char *)&hbuf[1], 1) != 1)
		if (iLen != 1)
		{
			//RTMP_Log(RTMP_LOGERROR, "%s, failed to read RTMP packet header 2nd byte", __FUNCTION__);
			return FALSE;
		}
		packet->m_csid = hbuf[1];
		packet->m_csid += 64;
		header++;
	}
	else if (packet->m_csid == 1)
	{
		iLen = recv(m_sb.sb_socket, (char *)&hbuf[1], 2, 0);
		//if (ReadN(r, (char *)&hbuf[1], 2) != 2)
		if (iLen != 2)
		{
			//RTMP_Log(RTMP_LOGERROR, "%s, failed to read RTMP packet header 3nd byte", __FUNCTION__);
			return FALSE;
		}
		int tmp = (hbuf[2] << 8) + hbuf[1];
		packet->m_csid = tmp + 64;
		//RTMP_Log(RTMP_LOGDEBUG, "%s, m_nChannel: %0x", __FUNCTION__, packet->m_nChannel);
		header += 2;
	}
	nSize = packetSize[packet->m_fmt];

	if (packet->m_csid >= m_channelsAllocatedIn)
	{
		int n = packet->m_csid + 10;
		int* pTimestamp = (int*)realloc(m_channelTimestamp, sizeof(int) * n);
		RtmpMsg** packets = (RtmpMsg**)realloc(m_vecChannelsIn, sizeof(RtmpMsg*) * n);
		if (!pTimestamp)
		{
			free(m_channelTimestamp);
		}
		if (!packets)
		{
			free(m_vecChannelsIn);
		}

		m_channelTimestamp = pTimestamp;
		m_vecChannelsIn = packets;
		if (!pTimestamp || !packets)
		{
			m_channelsAllocatedIn = 0;
			return false;
		}
		memset(m_channelTimestamp + m_channelsAllocatedIn, 0, sizeof(int) * (n - m_channelsAllocatedIn));
		memset(m_vecChannelsIn + m_channelsAllocatedIn, 0, sizeof(RtmpMsg*) * (n - m_channelsAllocatedIn));
		m_channelsAllocatedIn = n;
	}

	if (nSize == RTMP_LARGE_HEADER_SIZE)	/* if we get a full header the timestamp is absolute */
	{
		packet->m_hasAbsTimestamp = TRUE;
	}
	else if (nSize < RTMP_LARGE_HEADER_SIZE)
	{
		/* using values from the last message of this channel */
		if (m_vecChannelsIn[packet->m_csid])
		{
			memcpy(packet, m_vecChannelsIn[packet->m_csid], sizeof(RtmpMsg));
		}
	}
	nSize--;

	if (nSize > 0 && recv(m_sb.sb_socket, header, nSize, 0) != nSize)
	{
		//RTMP_Log(RTMP_LOGERROR, "%s, failed to read RTMP packet header. type: %x", __FUNCTION__, (unsigned int)hbuf[0]);
		return FALSE;
	}
	hSize = nSize + (header - (char *)hbuf);

	printf("header : %02x ", hbuf[0]);
	for (int i = 0; i < nSize; i++)
	{
		printf("%02x ", (unsigned char)header[i]);
	}
	printf("\n");

	if (nSize >= 3)
	{
		packet->m_nTimeStamp = AMFObject::DecodeInt24(header);
		/*RTMP_Log(RTMP_LOGDEBUG, "%s, reading RTMP packet chunk on channel %x, headersz %i, timestamp %i, abs timestamp %i", __FUNCTION__, packet.m_nChannel, nSize, packet.m_nTimeStamp, packet.m_hasAbsTimestamp); */

		if (nSize >= 6)
		{
			packet->m_nBodySize = AMFObject::DecodeInt24(header + 3);
			packet->m_nBytesRead = 0;
			RTMPPacket_Free(packet);

			if (nSize > 6)
			{
				packet->m_msgType = header[6];

				if (nSize == 11)
				{
					packet->m_nInfoField2 = DecodeInt32LE(header + 7);
				}
			}
		}

		extendedTimestamp = (packet->m_nTimeStamp == 0xffffff);

		if (extendedTimestamp)
		{
			if (recv(m_sb.sb_socket, header + nSize, 4, 0) != 4)
			{
				//RTMP_Log(RTMP_LOGERROR, "%s, failed to read extended timestamp", __FUNCTION__);
				return FALSE;
			}
			packet->m_nTimeStamp = AMFObject::DecodeInt32(header + nSize);
			hSize += 4;
		}
	}

	//RTMP_LogHexString(RTMP_LOGDEBUG2, (uint8_t *)hbuf, hSize);

	if (packet->m_nBodySize > 0 && packet->m_body == NULL)
	{
		if (!RTMPPacket_Alloc(packet, packet->m_nBodySize))
		{
			//RTMP_Log(RTMP_LOGDEBUG, "%s, failed to allocate packet", __FUNCTION__);
			return FALSE;
		}
		didAlloc = TRUE;
		packet->m_fmt = (hbuf[0] & 0xc0) >> 6;
	}

	nToRead = packet->m_nBodySize - packet->m_nBytesRead;
	nChunk = m_inChunkSize;
	if (nToRead < nChunk)
	{
		nChunk = nToRead;
	}

	/* Does the caller want the raw chunk? */
	if (packet->m_chunk)
	{
		packet->m_chunk->c_headerSize = hSize;
		memcpy(packet->m_chunk->c_header, hbuf, hSize);
		packet->m_chunk->c_chunk = packet->m_body + packet->m_nBytesRead;
		packet->m_chunk->c_chunkSize = nChunk;
	}

	if (recv(m_sb.sb_socket, packet->m_body + packet->m_nBytesRead, nChunk, 0) != nChunk)
	{
		//RTMP_Log(RTMP_LOGERROR, "%s, failed to read RTMP packet body. len: %u", __FUNCTION__, packet->m_nBodySize);
		return FALSE;
	}

	{
		unsigned char* temp = (unsigned char*)(packet->m_body + packet->m_nBytesRead);
		printf("body : %02x ", hbuf[0]);
		for (int i = 0; i < nChunk; i++)
		{
			printf("%02x ", (unsigned char)(temp[i]));
		}
		printf("\n");
	}


	//RTMP_LogHexString(RTMP_LOGDEBUG2, (uint8_t *)packet->m_body + packet->m_nBytesRead, nChunk);
	packet->m_nBytesRead += nChunk;

	/* keep the packet as ref for other packets on this channel */
	if (!m_vecChannelsIn[packet->m_csid])
	{
		m_vecChannelsIn[packet->m_csid] = (RtmpMsg*)malloc(sizeof(RtmpMsg));
	}
	memcpy(m_vecChannelsIn[packet->m_csid], packet, sizeof(RtmpMsg));
	if (extendedTimestamp)
	{
		m_vecChannelsIn[packet->m_csid]->m_nTimeStamp = 0xffffff;
	}

	if (packet->m_nBytesRead == packet->m_nBodySize)
	{
		/* make packet's timestamp absolute */
		if (!packet->m_hasAbsTimestamp)
		{
			packet->m_nTimeStamp += m_channelTimestamp[packet->m_csid];	/* timestamps seem to be always relative!! */
		}

		m_channelTimestamp[packet->m_csid] = packet->m_nTimeStamp;

		/* reset the data from the stored packet. we keep the header since we may use it later if a new packet for this channel */
		/* arrives and requests to re-use some info (small packet header) */
		m_vecChannelsIn[packet->m_csid]->m_body = NULL;
		m_vecChannelsIn[packet->m_csid]->m_nBytesRead = 0;
		m_vecChannelsIn[packet->m_csid]->m_hasAbsTimestamp = FALSE;	/* can only be false if we reuse header */
	}
	else
	{
		packet->m_body = NULL;	/* so it won't be erased on free */
	}

	return true;
}

//int CRtmpClient::Read(OUT RTMPPacket * packet)
//{
//	int nRead = 0, total = 0;
//
//	/* can't continue */
//fail:
//	switch (m_read.status)
//	{
//	case READ_STATUS_EOF:
//	case READ_STATUS_COMPLETE:
//		return 0;
//	case READ_STATUS_ERROR:  /* corrupted stream, resume failed */
//		//SetSockError(EINVAL);
//		return -1;
//	default:
//		break;
//	}
//
//	/* first time thru */
//	//if (!(m_read.flags & READ_FLAG_HEADER))
//	//{
//	//	if (!(m_read.flags & READ_FLAG_RESUME))
//	//	{
//	//		// �����ڴ�,�ֱ�ָ�򻺳������ײ���β��;
//	//		char *mybuf = malloc(HEADERBUF), *end = mybuf + HEADERBUF;
//	//		int cnt = 0;
//	//		m_read.buf = mybuf;
//	//		m_read.buflen = HEADERBUF;
//
//	//		// ��Flv���ײ����Ƶ�mybufָ����ڴ�;
//	//		// RTMP���ݵĶ�ý�������ǡ���ͷ����FLV�ļ�;
//	//		memcpy(mybuf, flvHeader, sizeof(flvHeader));
//	//		m_read.buf += sizeof(flvHeader);			// m_read.bufָ�����flvheader����λ;
//	//		m_read.buflen -= sizeof(flvHeader);		// bufʣ�µĿռ��С����nRead 
//	//		cnt += sizeof(flvHeader);
//
//	//		// timestamp=0�����Ƕ�ý������;
//	//		while (m_read.timestamp == 0)
//	//		{
//	//			// ��ȡһ�� Packet ��m_read.buf;
//	//			// nReadΪ��ȡ������. ���nRead < 0,���ʾ��ȡ����;
//	//			nRead = Read_1_Packet(r, m_read.buf, m_read.buflen);
//	//			if (nRead < 0)
//	//			{
//	//				free(mybuf);
//	//				m_read.buf = NULL;
//	//				m_read.buflen = 0;
//	//				m_read.status = nRead;
//	//				goto fail;
//	//			}
//	//			/* buffer overflow, fix buffer and give up */
//	//			if (m_read.buf < mybuf || m_read.buf > end)
//	//			{
//	//				mybuf = realloc(mybuf, cnt + nRead);
//	//				memcpy(mybuf + cnt, m_read.buf, nRead);
//	//				free(m_read.buf);
//	//				m_read.buf = mybuf + cnt + nRead;
//	//				break;
//	//			}
//	//			cnt += nRead;					// ��¼��ȡ���ֽ���;
//	//			m_read.buf += nRead;			// m_read.bufָ�����nRead����λ;
//	//			m_read.buflen -= nRead;		 // bufʣ�µĿռ��С����nRead;
//
//	//											 // ��dataType = 00000101ʱ��������Ƶ����Ƶ; 
//	//											 // ˵���ж�ý��������;
//	//			if (m_read.dataType == 5)
//	//			{
//	//				break;
//	//			}
//	//		}
//
//	//		// ������������  ע�⣺mybufָ��λ��һֱû��;
//	//		// mybuf[4]�е�6λ��ʾ�Ƿ������ƵTag �� 8 λ��ʾ�Ƿ������ƵTag;
//	//		mybuf[4] = m_read.dataType;
//	//		m_read.buflen = m_read.buf - mybuf;	// ����ָ��֮��Ĳ�;
//	//		m_read.buf = mybuf;
//	//		m_read.bufpos = mybuf;					// ������Ҫ! ����memcopy;
//	//	}
//
//	//	// flags�����Ѿ��������ļ�ͷ;
//	//	m_read.flags |= RTMP_READ_HEADER;
//	//}
//
//	if ((m_read.flags & READ_FLAG_SEEKING) && m_read.buf)
//	{
//		/* drop whatever's here */
//		free(m_read.buf);
//		m_read.buf = NULL;
//		m_read.bufpos = NULL;
//		m_read.buflen = 0;
//	}
//
//	/* If there's leftover data buffered, use it up */
//	if (m_read.buf)
//	{
//		nRead = m_read.buflen;
//		if (nRead > size)
//		{
//			nRead = size;
//		}
//		// m_read.bufposָ��mybuf 
//		memcpy(buf, m_read.bufpos, nRead);
//		m_read.buflen -= nRead;
//		if (!m_read.buflen)
//		{
//			free(m_read.buf);
//			m_read.buf = NULL;
//			m_read.bufpos = NULL;
//		}
//		else
//		{
//			m_read.bufpos += nRead;
//		}
//		buf += nRead;
//		total += nRead;
//		size -= nRead;
//	}
//
//	// ���Ŷ�;
//	while (size > 0 && (nRead = Read_1_Packet(r, buf, size)) >= 0)
//	{
//		if (!nRead)
//		{
//			continue;
//		}
//		buf += nRead;
//		total += nRead;
//		size -= nRead;
//		break;
//	}
//	if (nRead < 0)
//	{
//		m_read.status = nRead;
//	}
//	if (size < 0)
//	{
//		total += size;
//	}
//
//	return total;
//}

int CRtmpClient::HandlePacket(IN RtmpMsg * const packet)
{
	int bHasMediaPacket = 0;
	switch (packet->m_msgType)
	{
	// RTMP��Ϣ����ID=1 ���ÿ��С;
	case MSGHEADER_TYPE_SET_CHUNK_SIZE:
		{
			/* chunk size */
			//RTMP_Log(RTMP_LOGDEBUG, "������Ϣ: chunk_size (m_packetType=1)\n");
			HandleSetChunkSize(packet);
		}
		break;
	// 2: abort msg 
	case MSGHEADER_TYPE_ABORT_MSG:
		{
		}
		break;
	// RTMP��Ϣ����ID=3 ��л;
	case MSGHEADER_TYPE_ACKNOWLEDGEMENT:
		{
			/* bytes read report */
			//RTMP_Log(RTMP_LOGDEBUG, "������Ϣ: bytes_read_report (m_packetType=3)\n ");
		}
		break;
	// RTMP��Ϣ����ID=4 �û�����;
	case MSGHEADER_TYPE_USER_CONTROL:
		{
			/* ctrl */
			//RTMP_Log(RTMP_LOGDEBUG, "������Ϣ: control (m_packetType=4)\n ");
			//HandleCtrl(r, packet);
		}
		break;
	// RTMP��Ϣ����ID=5  ;
	case MSGHEADER_TYPE_SERVER_BW:
		{
			/* server bw */
			//RTMP_Log(RTMP_LOGDEBUG, "������Ϣ: server_bw (m_packetType=5)\n ");
			HandleServerBW(packet);
		}
		break;
	// RTMP��Ϣ����ID=6;
	case MSGHEADER_TYPE_CLIENT_BW:
		{
			/* client bw */
			//RTMP_Log(RTMP_LOGDEBUG, "������Ϣ: client_bw (m_packetType=6)\n ");
			HandleClientBW(packet);
		}
		break;
	// RTMP��Ϣ����ID=8 ��Ƶ����;
	case MSGHEADER_TYPE_AUDIO:
		{
			/* audio data */
			//RTMP_Log(RTMP_LOGDEBUG, "������Ϣ: audio (m_packetType=8)\n ");
			//HandleAudio(r, packet);
			bHasMediaPacket = 1;
			if (!m_mediaChannel)
			{
				m_mediaChannel = packet->m_csid;
			}
			if (!m_pausing)
			{
				m_mediaStamp = packet->m_nTimeStamp;
			}
		}
		break;
	// RTMP��Ϣ����ID=9 ��Ƶ����;
	case MSGHEADER_TYPE_VIDEO:
		{
			/* video data */
			//RTMP_Log(RTMP_LOGDEBUG, "������Ϣ: video (m_packetType=9)\n ");
			//HandleVideo(r, packet);
			bHasMediaPacket = 1;
			if (!m_mediaChannel)
			{
				m_mediaChannel = packet->m_csid;
			}
			if (!m_pausing)
			{
				m_mediaStamp = packet->m_nTimeStamp;
			}
		}
		break;
	// RTMP��Ϣ����ID=15 AMF3���� ����; 
	case MSGHEADER_TYPE_FLEX_STREAM_SEND:
		{
			/* flex stream send */
			//RTMP_Log(RTMP_LOGDEBUG, "%s, flex stream send, size %u bytes, not supported, ignoring", __FUNCTION__, packet->m_nBodySize);
		}
		break;
	// RTMP��Ϣ����ID=16 AMF3���� ����;
	case MSGHEADER_TYPE_FLEX_SHARED_OBJECT:
		{
			/* flex shared object */
			//RTMP_Log(RTMP_LOGDEBUG, "%s, flex shared object, size %u bytes, not supported, ignoring", __FUNCTION__, packet->m_nBodySize);
		}
		break;
	// RTMP��Ϣ����ID=17 AMF3���� ���� 
	case MSGHEADER_TYPE_FLEX_MESSAGE:
		{
			/* flex message */
			//RTMP_Log(RTMP_LOGDEBUG, "%s, flex message, size %u bytes, not fully supported", __FUNCTION__, packet->m_nBodySize);
			/*RTMP_LogHex(packet.m_body, packet.m_nBodySize); */

			/* some DEBUG code */

			/*if (HandleInvoke(r, packet->m_body + 1, packet->m_nBodySize - 1) == 1)
			{
				bHasMediaPacket = 2;
			}*/
		}
		break;
	// RTMP��Ϣ����ID=18 AMF0���� ������Ϣ;
	case MSGHEADER_TYPE_INFO:
		{
			/* metadata (notify) */
			//RTMP_Log(RTMP_LOGDEBUG, "%s, received: notify %u bytes", __FUNCTION__, packet->m_nBodySize);

			// ����Ԫ����;
			/*if (HandleMetadata(r, packet->m_body, packet->m_nBodySize))
			{
				bHasMediaPacket = 1;
			}*/
		}
		break;
	// RTMP��Ϣ����ID=19 AMF0���룬����;
	case MSGHEADER_TYPE_SHARED_OBJECT:
		{
			//RTMP_Log(RTMP_LOGDEBUG, "%s, shared object, not supported, ignoring", __FUNCTION__);
		}
		break;
	// RTMP��Ϣ����ID=20 AMF0���룬������Ϣ 
	case MSGHEADER_TYPE_INVOKE:
		{
			/* invoke */
			//RTMP_Log(RTMP_LOGDEBUG, "%s, received: invoke %u bytes", __FUNCTION__, packet->m_nBodySize);
			/*RTMP_LogHex(packet.m_body, packet.m_nBodySize); */

			if (HandleInvoke(packet->m_body, packet->m_nBodySize) == 1)
			{
				bHasMediaPacket = 2;
			}
		}
		break;
	// RTMP��Ϣ����ID=22
	case MSGHEADER_TYPE_FLASH_VIDEO:
		{
			/* go through FLV packets and handle metadata packets */
			//unsigned int pos = 0;
			//uint32_t nTimeStamp = packet->m_nTimeStamp;

			//while (pos + 11 < packet->m_nBodySize)
			//{
			//	uint32_t dataSize = AMF_DecodeInt24(packet->m_body + pos + 1);	/* size without header (11) and prevTagSize (4) */

			//	if (pos + 11 + dataSize + 4 > packet->m_nBodySize)
			//	{
			//		RTMP_Log(RTMP_LOGWARNING, "Stream corrupt?!");
			//		break;
			//	}
			//	if (packet->m_body[pos] == 0x12)
			//	{
			//		HandleMetadata(r, packet->m_body + pos + 11, dataSize);
			//	}
			//	else if (packet->m_body[pos] == 8 || packet->m_body[pos] == 9)
			//	{
			//		nTimeStamp = AMF_DecodeInt24(packet->m_body + pos + 4);
			//		nTimeStamp |= (packet->m_body[pos + 7] << 24);
			//	}
			//	pos += (11 + dataSize + 4);
			//}
			//if (!m_pausing)
			//	m_mediaStamp = nTimeStamp;

			///* FLV tag(s) */
			///*RTMP_Log(RTMP_LOGDEBUG, "%s, received: FLV tag(s) %lu bytes", __FUNCTION__, packet.m_nBodySize); */
			//bHasMediaPacket = 1;
		}
		break;
	default:
		{
			//RTMP_Log(RTMP_LOGDEBUG, "%s, unknown packet type received: 0x%02x", __FUNCTION__, packet->m_packetType);
#ifdef _DEBUG
			//RTMP_LogHex(RTMP_LOGDEBUG, packet->m_body, packet->m_nBodySize);
#endif
		}
		break;
	}

	return bHasMediaPacket;
}

// type: 1
bool CRtmpClient::HandleSetChunkSize(const RtmpMsg *packet)
{
	if (packet->m_nBodySize == 4)
	{
		m_inChunkSize = AMFObject::DecodeInt32(packet->m_body);
		printf("received from server, set chunk size change to %d bytes. \n", m_inChunkSize);
	}
	return true;
}

// type: 5
bool CRtmpClient::HandleServerBW(const RtmpMsg *packet)
{
	m_nServerBW = AMFObject::DecodeInt32(packet->m_body);
	//RTMP_Log(RTMP_LOGDEBUG, "%s: server BW = %d", __FUNCTION__, m_nServerBW);

	return true;
}

// type: 6
bool CRtmpClient::HandleClientBW(const RtmpMsg *packet)
{
	m_nClientBW = AMFObject::DecodeInt32(packet->m_body);
	if (packet->m_nBodySize > 4)
	{
		m_nClientBW2 = packet->m_body[4];
	}
	else
	{
		m_nClientBW2 = -1;
	}
	// RTMP_Log(RTMP_LOGDEBUG, "%s: client BW = %d %d", __FUNCTION__, m_nClientBW, m_nClientBW2);

	return true;
}

#define HEX2BIN(a)	(((a)&0x40) ? ((a)&0xf)+9 : ((a)&0xf))
void CRtmpClient::DecodeTEA(AVal* key, AVal* text)
{
	uint32_t *v, k[4] = { 0 }, u;
	uint32_t z, y, sum = 0, e, DELTA = 0x9e3779b9;
	int32_t p, q;
	int i, n;
	unsigned char *ptr, *out;

	/* prep key: pack 1st 16 chars into 4 LittleEndian ints */
	ptr = (unsigned char *)key->av_val;
	u = 0;
	n = 0;
	v = k;
	p = key->av_len > 16 ? 16 : key->av_len;
	for (i = 0; i < p; i++)
	{
		u |= ptr[i] << (n * 8);
		if (n == 3)
		{
			*v++ = u;
			u = 0;
			n = 0;
		}
		else
		{
			n++;
		}
	}
	/* any trailing chars */
	if (u)
		*v = u;

	/* prep text: hex2bin, multiples of 4 */
	n = (text->av_len + 7) / 8;
	out = (unsigned char *)malloc(n * 8);
	ptr = (unsigned char *)text->av_val;
	v = (uint32_t *)out;
	for (i = 0; i < n; i++)
	{
		u = (HEX2BIN(ptr[0]) << 4) + HEX2BIN(ptr[1]);
		u |= ((HEX2BIN(ptr[2]) << 4) + HEX2BIN(ptr[3])) << 8;
		u |= ((HEX2BIN(ptr[4]) << 4) + HEX2BIN(ptr[5])) << 16;
		u |= ((HEX2BIN(ptr[6]) << 4) + HEX2BIN(ptr[7])) << 24;
		*v++ = u;
		ptr += 8;
	}
	v = (uint32_t *)out;

	/* http://www.movable-type.co.uk/scripts/tea-block.html */
#define MX (((z>>5)^(y<<2)) + ((y>>3)^(z<<4))) ^ ((sum^y) + (k[(p&3)^e]^z));
	z = v[n - 1];
	y = v[0];
	q = 6 + 52 / n;
	sum = q * DELTA;
	while (sum != 0)
	{
		e = sum >> 2 & 3;
		for (p = n - 1; p > 0; p--)
			z = v[p - 1], y = v[p] -= MX;
		z = v[n - 1];
		y = v[0] -= MX;
		sum -= DELTA;
	}

	text->av_len /= 2;
	memcpy(text->av_val, out, text->av_len);
	free(out);
}

// type: 20
int CRtmpClient::HandleInvoke(const char* body, unsigned int nBodySize)
{
	AMFObject obj;
	AVal method;
	double dTxn = 0;
	int ret = 0, nRes;
	if (body[0] != 0x02)		/* make sure it is a string method name we start with */
	{
		//RTMP_Log(RTMP_LOGWARNING, "%s, Sanity failed. no string method in invoke packet", __FUNCTION__);
		return 0;
	}

	nRes = obj.Decode(body, nBodySize, FALSE);
	if (nRes < 0)
	{
		//RTMP_Log(RTMP_LOGERROR, "%s, error decoding invoke packet", __FUNCTION__);
		return 0;
	}

	obj.Dump();
	AVal emptyVal = { NULL, 0 };
	obj.GetObjectProp(emptyVal, 0)->GetString(method);
	dTxn = obj.GetObjectProp(emptyVal, 1)->GetNumber();
	//RTMP_Log(RTMP_LOGDEBUG, "%s, server invoking <%s>", __FUNCTION__, method.av_val);

	if (AVMATCH(&method, &sg_av__result))
	{
		AVal methodInvoked = { 0 };
		for (int i = 0; i < m_numCalls; i++)
		{
			if (m_methodCalls[i].num == (int)dTxn)
			{
				methodInvoked = m_methodCalls[i].name;
				AV_erase(m_methodCalls, &m_numCalls, i, FALSE);
				break;
			}
		}
		if (!methodInvoked.av_val)
		{
			//RTMP_Log(RTMP_LOGDEBUG, "%s, received result id %f without matching request", __FUNCTION__, txn);
			goto leave;
		}

		//RTMP_Log(RTMP_LOGDEBUG, "%s, received result for method call <%s>", __FUNCTION__, methodInvoked.av_val);

		if (AVMATCH(&methodInvoked, &sg_av_connect))
		{
			if (Link.token.av_len)
			{
				AMFObjectProperty p;
				if (RTMP_FindFirstMatchingProperty(&obj, &sg_av_secureToken, &p))
				{
					DecodeTEA(&Link.token, &p.p_vu.p_aval);
					SendSecureTokenResponse(&p.p_vu.p_aval);
				}
			}
			if (Link.protocol & RTMP_FEATURE_WRITE)
			{
				SendReleaseStream();
				SendFCPublish();
			}
			else
			{
				SendServerBW();
				SendCtrl(3, 0, 300);
			}
			SendCreateStream();

			if (!(Link.protocol & RTMP_FEATURE_WRITE))
			{
				/* Authenticate on Justin.tv legacy servers before sending FCSubscribe */
				if (Link.usherToken.av_len)
				{
					SendUsherToken(Link.usherToken);
				}
				/* Send the FCSubscribe if live stream or if subscribepath is set */
				if (Link.subscribepath.av_len)
				{
					SendFCSubscribe(Link.subscribepath);
				}
				else if (Link.lFlags & RTMP_LF_LIVE)
				{
					SendFCSubscribe(Link.playpath);
				}
			}
		}
		else if (AVMATCH(&methodInvoked, &sg_av_createStream))
		{
			m_stream_id = (int)(obj.GetObjectProp(sg_emptyVal, 3)->GetNumber());

			if (Link.protocol & RTMP_FEATURE_WRITE)
			{
				SendPublish();
			}
			else
			{
				if (Link.lFlags & RTMP_LF_PLST)
				{
					SendPlaylist();
				}
				SendPlay();
				SendCtrl(3, m_stream_id, m_nBufferMS);
			}
		}
		else if (AVMATCH(&methodInvoked, &sg_av_play) || AVMATCH(&methodInvoked, &sg_av_publish))
		{
			m_bPlaying = TRUE;
		}
		free(methodInvoked.av_val);
	}
	else if (AVMATCH(&method, &sg_av_onBWDone))
	{
		if (!m_nBWCheckCounter)
			SendCheckBW();
	}
	else if (AVMATCH(&method, &sg_av_onFCSubscribe))
	{
		/* SendOnFCSubscribe(); */
	}
	else if (AVMATCH(&method, &sg_av_onFCUnsubscribe))
	{
		Close();
		ret = 1;
	}
	else if (AVMATCH(&method, &sg_av_pong))
	{
		SendPing(dTxn);
	}
	else if (AVMATCH(&method, &sg_av__onbwcheck))
	{
		SendCheckBWResult(dTxn);
	}
	else if (AVMATCH(&method, &sg_av__onbwdone))
	{
		for (int i = 0; i < m_numCalls; i++)
		{
			if (AVMATCH(&m_methodCalls[i].name, &sg_av__checkbw))
			{
				AV_erase(m_methodCalls, &m_numCalls, i, TRUE);
				break;
			}
		}
	}
	else if (AVMATCH(&method, &sg_av__error))
	{
#if defined(CRYPTO) || defined(USE_ONLY_MD5)
		AVal methodInvoked = { 0 };
		if (Link.protocol & RTMP_FEATURE_WRITE)
		{
			for (int i = 0; i<m_numCalls; i++)
			{
				if (m_methodCalls[i].num == txn)
				{
					methodInvoked = m_methodCalls[i].name;
					AV_erase(m_methodCalls, &m_numCalls, i, FALSE);
					break;
				}
			}
			if (!methodInvoked.av_val)
			{
				RTMP_Log(RTMP_LOGDEBUG, "%s, received result id %f without matching request",
					__FUNCTION__, txn);
				goto leave;
			}

			RTMP_Log(RTMP_LOGDEBUG, "%s, received error for method call <%s>", __FUNCTION__,
				methodInvoked.av_val);

			if (AVMATCH(&methodInvoked, &av_connect))
			{
				AMFObject obj2;
				AVal code, level, description;
				AMFProp_GetObject(GetObjectProp(&obj, NULL, 3), &obj2);
				AMFProp_GetString(GetObjectProp(&obj2, &av_code, -1), &code);
				AMFProp_GetString(GetObjectProp(&obj2, &av_level, -1), &level);
				AMFProp_GetString(GetObjectProp(&obj2, &av_description, -1), &description);
				RTMP_Log(RTMP_LOGDEBUG, "%s, error description: %s", __FUNCTION__, description.av_val);
				/* if PublisherAuth returns 1, then reconnect */
				PublisherAuth(r, &description);
			}
		}
		else
		{
			RTMP_Log(RTMP_LOGERROR, "rtmp server sent error");
		}
		free(methodInvoked.av_val);
#else
		//RTMP_Log(RTMP_LOGERROR, "rtmp server sent error");
#endif
	}
	else if (AVMATCH(&method, &sg_av_close))
	{
		//RTMP_Log(RTMP_LOGERROR, "rtmp server requested close");
		Close();
#if defined(CRYPTO) || defined(USE_ONLY_MD5)
		if ((Link.protocol & RTMP_FEATURE_WRITE) &&
			!(Link.pFlags & RTMP_PUB_CLEAN) &&
			(!(Link.pFlags & RTMP_PUB_NAME) ||
				!(Link.pFlags & RTMP_PUB_RESP) ||
				(Link.pFlags & RTMP_PUB_CLATE)))
		{
			/* clean later */
			if (Link.pFlags & RTMP_PUB_CLATE)
				Link.pFlags |= RTMP_PUB_CLEAN;
			RTMP_Log(RTMP_LOGERROR, "authenticating publisher");

			if (!RTMP_Connect(r, NULL) || !RTMP_ConnectStream(r, 0))
				goto leave;
		}
#endif
	}
	else if (AVMATCH(&method, &sg_av_onStatus))
	{
		AMFObject obj2;
		AVal code, level, description;
		obj.GetObjectProp(sg_emptyVal, 3)->GetObject(obj2);
		obj2.GetObjectProp(sg_av_code, -1)->GetString(code);
		obj2.GetObjectProp(sg_av_level, -1)->GetString(level);
		obj2.GetObjectProp(sg_av_description, -1)->GetString(description);

		//RTMP_Log(RTMP_LOGDEBUG, "%s, onStatus: %s", __FUNCTION__, code.av_val);
		if (   AVMATCH(&code, &sg_av_NetStream_Failed)
			|| AVMATCH(&code, &sg_av_NetStream_Play_Failed)
			|| AVMATCH(&code, &sg_av_NetStream_Play_StreamNotFound)
			|| AVMATCH(&code, &sg_av_NetConnection_Connect_InvalidApp)
			|| AVMATCH(&code, &sg_av_NetConnection_Connect_Closed)
			|| AVMATCH(&code, &sg_av_NetStream_Publish_Rejected)
			|| AVMATCH(&code, &sg_av_NetStream_Publish_Denied))
		{
			m_stream_id = -1;
			Close();

			if (description.av_len)
			{
				//RTMP_Log(RTMP_LOGERROR, "%s:\n%s (%s)", Link.tcUrl.av_val, code.av_val, description.av_val);
			}
			else
			{
				//RTMP_Log(RTMP_LOGERROR, "%s:\n%s", Link.tcUrl.av_val, code.av_val);
			}
		}

		else if (AVMATCH(&code, &sg_av_NetStream_Play_Start) || AVMATCH(&code, &sg_av_NetStream_Play_PublishNotify))
		{
			m_bPlaying = TRUE;
			for (int i = 0; i < m_numCalls; i++)
			{
				if (AVMATCH(&m_methodCalls[i].name, &sg_av_play))
				{
					AV_erase(m_methodCalls, &m_numCalls, i, TRUE);
					break;
				}
			}
		}

		else if (AVMATCH(&code, &sg_av_NetStream_Publish_Start))
		{
			m_bPlaying = TRUE;
			for (int i = 0; i < m_numCalls; i++)
			{
				if (AVMATCH(&m_methodCalls[i].name, &sg_av_publish))
				{
					AV_erase(m_methodCalls, &m_numCalls, i, TRUE);
					break;
				}
			}
		}
		/* Return 1 if this is a Play.Complete or Play.Stop */
		else if (AVMATCH(&code, &sg_av_NetStream_Play_Complete)
			|| AVMATCH(&code, &sg_av_NetStream_Play_Stop)
			|| AVMATCH(&code, &sg_av_NetStream_Play_UnpublishNotify))
		{
			Close();
			ret = 1;
		}
		else if (AVMATCH(&code, &sg_av_NetStream_Seek_Notify))
		{
			m_read.flags &= ~READ_FLAG_SEEKING;
		}

		else if (AVMATCH(&code, &sg_av_NetStream_Pause_Notify))
		{
			if (m_pausing == 1 || m_pausing == 2)
			{
				SendPause(FALSE, m_pauseStamp);
				m_pausing = 3;
			}
		}
	}
	else if (AVMATCH(&method, &sg_av_playlist_ready))
	{
		for (int i = 0; i < m_numCalls; i++)
		{
			if (AVMATCH(&m_methodCalls[i].name, &sg_av_set_playlist))
			{
				AV_erase(m_methodCalls, &m_numCalls, i, TRUE);
				break;
			}
		}
	}
	else
	{
	}

leave:
	obj.Reset();
	return ret;
}

int CRtmpClient::HandleMetadata(char *body, unsigned int len)
{
	return -1;
}

bool CRtmpClient::HandleAudio(const RtmpMsg *packet)
{
	return false;
}

bool CRtmpClient::HandleVideo(const RtmpMsg *packet)
{
	return false;
}

bool CRtmpClient::HandleCtrl(const RtmpMsg *packet)
{
	return false;
}

void CRtmpClient::Close()
{
	/*if (IsConnected())
	{
		if (m_stream_id > 0)
		{
			i = m_stream_id;
			m_stream_id = 0;
			if ((Link.protocol & RTMP_FEATURE_WRITE))
				SendFCUnpublish(r);
			SendDeleteStream(r, i);
		}
		if (m_clientID.av_val)
		{
			HTTP_Post(r, RTMPT_CLOSE, "", 1);
			free(m_clientID.av_val);
			m_clientID.av_val = NULL;
			m_clientID.av_len = 0;
		}
		RTMPSockBuf_Close(&m_sb);
	}

	m_stream_id = -1;
	m_sb.sb_socket = -1;
	m_nBWCheckCounter = 0;
	m_nBytesIn = 0;
	m_nBytesInSent = 0;

	if (m_read.flags & RTMP_READ_HEADER)
	{
		free(m_read.buf);
		m_read.buf = NULL;
	}
	m_read.dataType = 0;
	m_read.flags = 0;
	m_read.status = 0;
	m_read.nResumeTS = 0;
	m_read.nIgnoredFrameCounter = 0;
	m_read.nIgnoredFlvFrameCounter = 0;

	m_write.m_nBytesRead = 0;
	RTMPPacket_Free(&m_write);

	for (i = 0; i < m_channelsAllocatedIn; i++)
	{
		if (m_vecChannelsIn[i])
		{
			RTMPPacket_Free(m_vecChannelsIn[i]);
			free(m_vecChannelsIn[i]);
			m_vecChannelsIn[i] = NULL;
		}
	}
	free(m_vecChannelsIn);
	m_vecChannelsIn = NULL;
	free(m_channelTimestamp);
	m_channelTimestamp = NULL;
	m_channelsAllocatedIn = 0;
	for (i = 0; i < m_channelsAllocatedOut; i++)
	{
		if (m_vecChannelsOut[i])
		{
			free(m_vecChannelsOut[i]);
			m_vecChannelsOut[i] = NULL;
		}
	}
	free(m_vecChannelsOut);
	m_vecChannelsOut = NULL;
	m_channelsAllocatedOut = 0;
	AV_clear(m_methodCalls, m_numCalls);
	m_methodCalls = NULL;
	m_numCalls = 0;
	m_numInvokes = 0;

	m_bPlaying = FALSE;
	m_sb.sb_size = 0;

	m_msgCounter = 0;
	m_resplen = 0;
	m_unackd = 0;

	if (Link.lFlags & RTMP_LF_FTCU)
	{
		free(Link.tcUrl.av_val);
		Link.tcUrl.av_val = NULL;
		Link.lFlags ^= RTMP_LF_FTCU;
	}

#if defined(CRYPTO) || defined(USE_ONLY_MD5)
	if (!(Link.protocol & RTMP_FEATURE_WRITE) || (Link.pFlags & RTMP_PUB_CLEAN))
	{
		free(Link.playpath0.av_val);
		Link.playpath0.av_val = NULL;
	}
	if ((Link.protocol & RTMP_FEATURE_WRITE) &&
		(Link.pFlags & RTMP_PUB_CLEAN) &&
		(Link.pFlags & RTMP_PUB_ALLOC))
	{
		free(Link.app.av_val);
		Link.app.av_val = NULL;
		free(Link.tcUrl.av_val);
		Link.tcUrl.av_val = NULL;
	}
#elif defined(CRYPTO)
	if (Link.dh)
	{
		MDH_free(Link.dh);
		Link.dh = NULL;
	}
	if (Link.rc4keyIn)
	{
		RC4_free(Link.rc4keyIn);
		Link.rc4keyIn = NULL;
	}
	if (Link.rc4keyOut)
	{
		RC4_free(Link.rc4keyOut);
		Link.rc4keyOut = NULL;
	}
#else
	free(Link.playpath0.av_val);
	Link.playpath0.av_val = NULL;
#endif*/
}

bool CRtmpClient::RTMPPacket_Alloc(RtmpMsg * p, int nSize)
{
	char* ptr = (char*)calloc(1, nSize + RTMP_MAX_HEADER_SIZE);
	if (!ptr)
	{
		return false;
	}
	p->m_body = ptr + RTMP_MAX_HEADER_SIZE;
	p->m_nBytesRead = 0;
	return true;
}

void CRtmpClient::RTMPPacket_Free(RtmpMsg * p)
{
	if (p->m_body)
	{
		free(p->m_body - RTMP_MAX_HEADER_SIZE);
		p->m_body = NULL;
	}
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
		ParsePlaypath(&av, &strPlayPath);
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

void CRtmpClient::ParsePlaypath(AVal * in, AVal * out)
{
	int addMP4 = 0;
	int addMP3 = 0;
	int subExt = 0;
	const char *playpath = in->av_val;
	const char *temp, *q, *ext = NULL;
	const char *ppstart = playpath;
	char *streamname, *destptr, *p;

	int pplen = in->av_len;

	out->av_val = NULL;
	out->av_len = 0;

	if ((*ppstart == '?') && (temp = strstr(ppstart, "slist=")) != 0)
	{
		ppstart = temp + 6;
		pplen = (int)strlen(ppstart);

		temp = strchr(ppstart, '&');
		if (temp)
		{
			pplen = temp - ppstart;
		}
	}

	q = strchr(ppstart, '?');
	if (pplen >= 4)
	{
		if (q)
		{
			ext = q - 4;
		}
		else
		{
			ext = &ppstart[pplen - 4];
		}

		if ((strncmp(ext, ".f4v", 4) == 0) || (strncmp(ext, ".mp4", 4) == 0))
		{
			addMP4 = 1;
			subExt = 1;
			/* Only remove .flv from rtmp URL, not slist params */
		}
		else if ((ppstart == playpath) && (strncmp(ext, ".flv", 4) == 0))
		{
			subExt = 1;
		}
		else if (strncmp(ext, ".mp3", 4) == 0)
		{
			addMP3 = 1;
			subExt = 1;
		}
	}

	streamname = (char *)malloc((pplen + 4 + 1) * sizeof(char));
	if (!streamname)
	{
		return;
	}

	destptr = streamname;
	if (addMP4)
	{
		if (strncmp(ppstart, "mp4:", 4))
		{
			strcpy(destptr, "mp4:");
			destptr += 4;
		}
		else
		{
			subExt = 0;
		}
	}
	else if (addMP3)
	{
		if (strncmp(ppstart, "mp3:", 4))
		{
			strcpy(destptr, "mp3:");
			destptr += 4;
		}
		else
		{
			subExt = 0;
		}
	}

	for (p = (char *)ppstart; pplen >0;)
	{
		/* skip extension */
		if (subExt && p == ext)
		{
			p += 4;
			pplen -= 4;
			continue;
		}
		if (*p == '%')
		{
			unsigned int c;
			sscanf(p + 1, "%02x", &c);
			*destptr++ = c;
			pplen -= 3;
			p += 3;
		}
		else
		{
			*destptr++ = *p++;
			pplen--;
		}
	}
	*destptr = '\0';

	out->av_val = streamname;
	out->av_len = destptr - streamname;
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

	int tv = 5 * 100; // ��ʱʱ��;
	if (setsockopt(m_sb.sb_socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(tv)))
	{
		printf("set time out");
		return false;
	}

	if (!m_bUseNagle)
	{
		int on = 1;
		setsockopt(m_sb.sb_socket, IPPROTO_TCP, TCP_NODELAY, (char *)&on, sizeof(on));
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

	Sleep(500);
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

bool CRtmpClient::ConnectStream(IN const int iSeekTime)
{
	/* seekTime was already set by SetupStream / SetupURL.
	* This is only needed by ReconnectStream.
	*/
	if (iSeekTime > 0)
	{
		Link.seekTime = iSeekTime;
	}

	/*while (1)
	{
		unsigned char ch[11];
		int iLen = recv(m_sb.sb_socket, (char*)ch, 10, 0);
		if (iLen > 0)
		{
			for (int i = 0; i < iLen; i++)
			{
				printf("%02x ", ch[i]);
			}
		}
		else
		{
			Sleep(1);
		}
	}*/

	m_mediaChannel = 0;
	RtmpMsg packet = { 0 };
	while (!m_bPlaying && IsConnected() && ReadPacket(&packet))
	{
		if (packet.m_nBytesRead == packet.m_nBodySize)
		{
			if (!packet.m_nBodySize)
			{
				continue;
			}

			if ((packet.m_msgType == MSGHEADER_TYPE_AUDIO) ||
				(packet.m_msgType == MSGHEADER_TYPE_VIDEO) ||
				(packet.m_msgType == MSGHEADER_TYPE_INFO))
			{
				//RTMP_Log(RTMP_LOGWARNING, "Received FLV packet before play()! Ignoring.");
				//Free(&packet);
				continue;
			}

			HandlePacket(&packet);
			//RTMPPacket_Free(&packet);
		}
		memset(&packet, 0, sizeof(packet));
	}

//	return m_bPlaying;

	return true;
}

bool CRtmpClient::SendSetChunkSize(int iChunkSize)
{
	char pBuff[4096] = { 0 };
	char* pEnd = pBuff + sizeof(pBuff);

	if ((Link.protocol & RTMP_FEATURE_WRITE) && m_bSendChunkSizeInfo)
	{
		RtmpMsg packet;
		packet.m_csid = 0x02;
		packet.m_fmt = BASICHEADER_FMT_ZREO;
		packet.m_msgType = MSGHEADER_TYPE_SET_CHUNK_SIZE;
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

	RtmpMsg packet;
	packet.m_csid = 0x03;	/* control channel (invoke) */
	packet.m_fmt = BASICHEADER_FMT_ZREO;
	packet.m_msgType = MSGHEADER_TYPE_INVOKE;
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
			objCommand.AddProp(sg_av_app, Link.app);
		}
		if (Link.flashVer.av_len)
		{
			objCommand.AddProp(sg_av_flashVer, Link.flashVer);
		}
		if (Link.swfUrl.av_len)
		{
			objCommand.AddProp(sg_av_swfUrl, Link.swfUrl);
		}
		if (Link.tcUrl.av_len)
		{
			objCommand.AddProp(sg_av_tcUrl, Link.tcUrl);
		}

		if (!(Link.protocol & RTMP_FEATURE_WRITE))
		{
			objCommand.AddProp(sg_av_type, sg_av_nonprivate);
			objCommand.AddProp(sg_av_fpad, 0);
			//objCommand.AddProp(sg_av_capabilities, 15.0);
			objCommand.AddProp(sg_av_audioCodecs, m_fAudioCodecs);
			objCommand.AddProp(sg_av_videoCodecs, m_fVideoCodecs);
			objCommand.AddProp(sg_av_videoFunction, 1.0);
			if (Link.pageUrl.av_len)
			{
				objCommand.AddProp(sg_av_pageUrl, Link.pageUrl);
			}
			if (m_fEncoding != 0.0 || m_bSendEncoding)
			{
				/* AMF0, AMF3 not fully supported yet */
				objCommand.AddProp(sg_av_objectEncoding, m_fEncoding);
			}
		}
		pBodyEn = objCommand.Encode(pBodyEn, pEnd);
		assert(pBodyEn);
	}

	/* add auth string */
	if (Link.auth.av_len)
	{
		pBodyEn = AMFObject::EncodeBoolean(pBodyEn, pEnd, Link.lFlags & RTMP_LF_AUTH);
		assert(pBodyEn);
		pBodyEn = AMFObject::EncodeString(pBodyEn, pEnd, Link.auth);
		assert(pBodyEn);
	}
	if (Link.extras.o_num)
	{
		for (int i = 0; i < Link.extras.o_num; i++)
		{
			pBodyEn = Link.extras.o_props[i].Encode(pBodyEn, pEnd);
			assert(pBodyEn);
		}
	}
	packet.m_nBodySize = pBodyEn - packet.m_body;

	// ���� connect(streamid);
	if (!SendPacket(&packet, TRUE))
	{
		return false;
	}

	return true;
}

bool CRtmpClient::SendSecureTokenResponse(AVal * resp)
{
	RtmpMsg packet;
	char pBuff[1024] = {0};
	char* pEnd = pBuff + sizeof(pBuff);

	packet.m_csid = 0x03;	/* control channel (invoke) */
	packet.m_fmt = BASICHEADER_FMT_ONE;
	packet.m_msgType = MSGHEADER_TYPE_INVOKE;
	packet.m_nTimeStamp = 0;
	packet.m_nInfoField2 = 0;
	packet.m_hasAbsTimestamp = 0;
	packet.m_body = pBuff + RTMP_MAX_HEADER_SIZE;

	char* pBodyEn = packet.m_body;
	pBodyEn = AMFObject::EncodeString(pBodyEn, pEnd, sg_av_secureTokenResponse);
	pBodyEn = AMFObject::EncodeNumber(pBodyEn, pEnd, 0.0);
	*pBodyEn++ = AMF_NULL;
	pBodyEn = AMFObject::EncodeString(pBodyEn, pEnd, *resp);
	if (!pBodyEn)
	{
		return false;
	}
	packet.m_nBodySize = pBodyEn - packet.m_body;

	return SendPacket(&packet, false);
}

bool CRtmpClient::SendReleaseStream()
{
	RtmpMsg packet;
	char pBuff[1024] = {0};
	char* pEnd = pBuff + sizeof(pBuff);

	packet.m_csid = 0x03;	/* control channel (invoke) */
	packet.m_fmt = BASICHEADER_FMT_ONE;
	packet.m_msgType = MSGHEADER_TYPE_INVOKE;
	packet.m_nTimeStamp = 0;
	packet.m_nInfoField2 = 0;
	packet.m_hasAbsTimestamp = 0;
	packet.m_body = pBuff + RTMP_MAX_HEADER_SIZE;

	char* pBodyEn = packet.m_body;
	pBodyEn = AMFObject::EncodeString(pBodyEn, pEnd, sg_av_releaseStream);
	pBodyEn = AMFObject::EncodeNumber(pBodyEn, pEnd, ++m_numInvokes);
	*pBodyEn++ = AMF_NULL;
	pBodyEn = AMFObject::EncodeString(pBodyEn, pEnd, Link.playpath);
	if (!pBodyEn)
	{
		return false;
	}
	packet.m_nBodySize = pBodyEn - packet.m_body;

	return SendPacket(&packet, false);
}

bool CRtmpClient::SendFCPublish()
{
	RtmpMsg packet;
	char pBuff[1024] = {0};
	char* pEnd = pBuff + sizeof(pBuff);

	packet.m_csid = 0x03;	/* control channel (invoke) */
	packet.m_fmt = BASICHEADER_FMT_ONE;
	packet.m_msgType = MSGHEADER_TYPE_INVOKE;
	packet.m_nTimeStamp = 0;
	packet.m_nInfoField2 = 0;
	packet.m_hasAbsTimestamp = 0;
	packet.m_body = pBuff + RTMP_MAX_HEADER_SIZE;

	char* pBodyEn = packet.m_body;
	pBodyEn = AMFObject::EncodeString(pBodyEn, pEnd, sg_av_FCPublish);
	pBodyEn = AMFObject::EncodeNumber(pBodyEn, pEnd, ++m_numInvokes);
	*pBodyEn++ = AMF_NULL;
	pBodyEn = AMFObject::EncodeString(pBodyEn, pEnd, Link.playpath);
	if (!pBodyEn)
	{
		return false;
	}
	packet.m_nBodySize = pBodyEn - packet.m_body;

	return SendPacket(&packet, false);
}

bool CRtmpClient::SendServerBW()
{
	RtmpMsg packet;
	char pBuff[256] = {0};
	char* pEnd = pBuff + sizeof(pBuff);

	packet.m_csid = 0x02;	/* control channel (invoke) */
	packet.m_fmt = BASICHEADER_FMT_ZREO;
	packet.m_msgType = MSGHEADER_TYPE_SERVER_BW;
	packet.m_nTimeStamp = 0;
	packet.m_nInfoField2 = 0;
	packet.m_hasAbsTimestamp = 0;
	packet.m_body = pBuff + RTMP_MAX_HEADER_SIZE;

	AMFObject::EncodeInt32(packet.m_body, pEnd, m_nServerBW);
	packet.m_nBodySize = 4;

	return SendPacket(&packet, false);
}

bool CRtmpClient::SendClientBW()
{
	RtmpMsg packet;
	char pBuff[256] = {0};
	char* pEnd = pBuff + sizeof(pBuff);

	packet.m_csid = 0x02;	/* control channel (invoke) */
	packet.m_fmt = BASICHEADER_FMT_ZREO;
	packet.m_msgType = MSGHEADER_TYPE_CLIENT_BW;
	packet.m_nTimeStamp = 0;
	packet.m_nInfoField2 = 0;
	packet.m_hasAbsTimestamp = 0;
	packet.m_body = pBuff + RTMP_MAX_HEADER_SIZE;

	AMFObject::EncodeInt32(packet.m_body, pEnd, m_nClientBW);
	packet.m_body[4] = m_nClientBW2;
	packet.m_nBodySize = 5;

	return SendPacket(&packet, false);
}

bool CRtmpClient::SendCtrl(short nType, unsigned int nObject, unsigned int nTime)
{	
	//RTMP_Log(RTMP_LOGDEBUG, "sending ctrl. type: 0x%04x", (unsigned short)nType);
	
	RtmpMsg packet;
	char pBuff[256] = {0};
	char* pEnd = pBuff + sizeof(pBuff);

	packet.m_csid = 0x02;	/* control channel (ping) */
	packet.m_fmt = BASICHEADER_FMT_ONE;
	packet.m_msgType = MSGHEADER_TYPE_USER_CONTROL;
	packet.m_nTimeStamp = 0;	/* RTMP_GetTime(); */
	packet.m_nInfoField2 = 0;
	packet.m_hasAbsTimestamp = 0;
	packet.m_body = pBuff + RTMP_MAX_HEADER_SIZE;

	int iBodySize = 0;
	switch (nType)
	{
	case 0x03:
		iBodySize = 10;
		break;	/* buffer time */
	case 0x1A:
		iBodySize = 3;
		break;	/* SWF verify request */
	case 0x1B:
		iBodySize = 44;
		break;	/* SWF verify response */
	default:
		iBodySize = 6;
		break;
	}
	packet.m_nBodySize = iBodySize;

	char* pBodyEn = packet.m_body;
	pBodyEn = AMFObject::EncodeInt16(pBodyEn, pEnd, nType);
	if (nType == 0x1B)
	{
#ifdef CRYPTO
		memcpy(buf, r->Link.SWFVerificationResponse, 42);
		RTMP_Log(RTMP_LOGDEBUG, "Sending SWFVerification response: ");
		RTMP_LogHex(RTMP_LOGDEBUG, (uint8_t *)packet.m_body, packet.m_nBodySize);
#endif
	}
	else if (nType == 0x1A)
	{
		*pBodyEn = nObject & 0xff;
	}
	else
	{
		if (iBodySize > 2)
		{
			pBodyEn = AMFObject::EncodeInt32(pBodyEn, pEnd, nObject);
		}
		if (iBodySize > 6)
		{
			pBodyEn = AMFObject::EncodeInt32(pBodyEn, pEnd, nTime);
		}
	}

	return SendPacket(&packet, false);
}

bool CRtmpClient::SendCreateStream()
{
	RtmpMsg packet;
	char pBuff[256] = { 0 };
	char* pEnd = pBuff + sizeof(pBuff);

	packet.m_csid = 0x03;	/* control channel (invoke) */
	packet.m_fmt = BASICHEADER_FMT_ONE;
	packet.m_msgType = MSGHEADER_TYPE_INVOKE;
	packet.m_nTimeStamp = 0;
	packet.m_nInfoField2 = 0;
	packet.m_hasAbsTimestamp = 0;
	packet.m_body = pBuff + RTMP_MAX_HEADER_SIZE;

	char* pBodyEn = packet.m_body;
	pBodyEn = AMFObject::EncodeString(pBodyEn, pEnd, sg_av_createStream);
	pBodyEn = AMFObject::EncodeNumber(pBodyEn, pEnd, ++m_numInvokes);
	*pBodyEn++ = AMF_NULL;		/* NULL */
	packet.m_nBodySize = pBodyEn - packet.m_body;

	return SendPacket(&packet, true);
}

bool CRtmpClient::SendCheckBW()
{
	RtmpMsg packet;
	char pBuff[256] = { 0 };
	char* pend = pBuff + sizeof(pBuff);

	packet.m_csid = 0x03;	/* control channel (invoke) */
	packet.m_fmt = BASICHEADER_FMT_ZREO;
	packet.m_msgType = MSGHEADER_TYPE_INVOKE;
	packet.m_nTimeStamp = 0;	/* RTMP_GetTime(); */
	packet.m_nInfoField2 = 0;
	packet.m_hasAbsTimestamp = 0;
	packet.m_body = pBuff + RTMP_MAX_HEADER_SIZE;

	char* pBodyEn = packet.m_body;
	pBodyEn = AMFObject::EncodeString(pBodyEn, pend, sg_av__checkbw);
	pBodyEn = AMFObject::EncodeNumber(pBodyEn, pend, ++m_numInvokes);
	*pBodyEn++ = AMF_NULL;
	packet.m_nBodySize = pBodyEn - packet.m_body;

	/* triggers _onbwcheck and eventually results in _onbwdone */
	return SendPacket(&packet, false);
}

bool CRtmpClient::SendCheckBWResult(double dTxn)
{
	RtmpMsg packet;
	char pBuff[256] = {0};
	char* pEnd = pBuff + sizeof(pBuff);

	packet.m_csid = 0x03;	/* control channel (invoke) */
	packet.m_fmt = BASICHEADER_FMT_ONE;
	packet.m_msgType = MSGHEADER_TYPE_INVOKE;
	packet.m_nTimeStamp = 0x16 * m_nBWCheckCounter;	/* temp inc value. till we figure it out. */
	packet.m_nInfoField2 = 0;
	packet.m_hasAbsTimestamp = 0;
	packet.m_body = pBuff + RTMP_MAX_HEADER_SIZE;

	char* enc = packet.m_body;
	enc = AMFObject::EncodeString(enc, pEnd, sg_av__result);
	enc = AMFObject::EncodeNumber(enc, pEnd, dTxn);
	*enc++ = AMF_NULL;
	enc = AMFObject::EncodeNumber(enc, pEnd, m_nBWCheckCounter++);
	packet.m_nBodySize = enc - packet.m_body;

	return SendPacket(&packet, false);
}

bool CRtmpClient::SendDeleteStream(double dStreamId)
{
	RtmpMsg packet;
	char pBuff[256] = {0};
	char* pEnd = pBuff + sizeof(pBuff);

	packet.m_csid = 0x03;	/* control channel (invoke) */
	packet.m_fmt = BASICHEADER_FMT_ONE;
	packet.m_msgType = MSGHEADER_TYPE_INVOKE;
	packet.m_nTimeStamp = 0;
	packet.m_nInfoField2 = 0;
	packet.m_hasAbsTimestamp = 0;
	packet.m_body = pBuff + RTMP_MAX_HEADER_SIZE;

	char* pBodyEn = packet.m_body;
	pBodyEn = AMFObject::EncodeString(pBodyEn, pEnd, sg_av_deleteStream);
	pBodyEn = AMFObject::EncodeNumber(pBodyEn, pEnd, ++m_numInvokes);
	*pBodyEn++ = AMF_NULL;
	pBodyEn = AMFObject::EncodeNumber(pBodyEn, pEnd, dStreamId);

	packet.m_nBodySize = pBodyEn - packet.m_body;

	/* no response expected */
	return SendPacket(&packet, false);
}

bool CRtmpClient::SendFCSubscribe(const AVal& strSubscribePath)
{
	RtmpMsg packet;
	char pBuff[512] = { 0 };
	char* pEnd = pBuff + sizeof(pBuff);

	packet.m_csid = 0x03;	/* control channel (invoke) */
	packet.m_fmt = BASICHEADER_FMT_ONE;
	packet.m_msgType = MSGHEADER_TYPE_INVOKE;
	packet.m_nTimeStamp = 0;
	packet.m_nInfoField2 = 0;
	packet.m_hasAbsTimestamp = 0;
	packet.m_body = pBuff + RTMP_MAX_HEADER_SIZE;

	char* pBodyEn = packet.m_body;
	pBodyEn = AMFObject::EncodeString(pBodyEn, pEnd, sg_av_FCSubscribe);
	pBodyEn = AMFObject::EncodeNumber(pBodyEn, pEnd, ++m_numInvokes);
	*pBodyEn++ = AMF_NULL;
	pBodyEn = AMFObject::EncodeString(pBodyEn, pEnd, strSubscribePath);
	if (!pBodyEn)
	{
		return false;
	}
	packet.m_nBodySize = pBodyEn - packet.m_body;

	return SendPacket(&packet, true);
}

bool CRtmpClient::SendPlay()
{
	RtmpMsg packet;
	char pBuff[1024] = { 0 };
	char* pEnd = pBuff + sizeof(pBuff);

	packet.m_csid = 0x08;	/* we make 8 our stream channel */
	packet.m_fmt = BASICHEADER_FMT_ZREO;
	packet.m_msgType = MSGHEADER_TYPE_INVOKE;
	packet.m_nTimeStamp = 0;
	packet.m_nInfoField2 = m_stream_id;	/*0x01000000; */
	packet.m_hasAbsTimestamp = 0;
	packet.m_body = pBuff + RTMP_MAX_HEADER_SIZE;

	char *enc = packet.m_body;
	enc = AMFObject::EncodeString(enc, pEnd, sg_av_set_playlist);
	enc = AMFObject::EncodeNumber(enc, pEnd, 0);
	*enc++ = AMF_NULL;
	*enc++ = AMF_ECMA_ARRAY;
	*enc++ = 0;
	*enc++ = 0;
	*enc++ = 0;
	*enc++ = AMF_OBJECT;
	enc = AMFObject::EncodeNamedString(enc, pEnd, sg_av_0, Link.playpath);
	if (!enc)
	{
		return false;
	}
	if (enc + 3 >= pEnd)
	{
		return false;
	}
	*enc++ = 0;
	*enc++ = 0;
	*enc++ = AMF_OBJECT_END;
	packet.m_nBodySize = enc - packet.m_body;

	return SendPacket(&packet, true);
}

bool CRtmpClient::SendBytesReceived()
{
	RtmpMsg packet;
	char pBuff[256] = { 0 };
	char* pEnd = pBuff + sizeof(pBuff);

	packet.m_csid = 0x02;	/* control channel (invoke) */
	packet.m_fmt = BASICHEADER_FMT_ONE;
	packet.m_msgType = MSGHEADER_TYPE_ACKNOWLEDGEMENT;
	packet.m_nTimeStamp = 0;
	packet.m_nInfoField2 = 0;
	packet.m_hasAbsTimestamp = 0;
	packet.m_body = pBuff + RTMP_MAX_HEADER_SIZE;

	AMFObject::EncodeInt32(packet.m_body, pEnd, m_nBytesIn);	/* hard coded for now */
	packet.m_nBodySize = 4;
	m_nBytesInSent = m_nBytesIn;

	return SendPacket(&packet, false);
}

bool CRtmpClient::SendUsherToken(const AVal& strUsherToken)
{
	RtmpMsg packet;
	char pBuff[1024] = { 0 };
	char* pEnd = pBuff + sizeof(pBuff);

	packet.m_csid = 0x03;	/* control channel (invoke) */
	packet.m_fmt = BASICHEADER_FMT_ONE;
	packet.m_msgType = MSGHEADER_TYPE_INVOKE;
	packet.m_nTimeStamp = 0;
	packet.m_nInfoField2 = 0;
	packet.m_hasAbsTimestamp = 0;
	packet.m_body = pBuff + RTMP_MAX_HEADER_SIZE;

	char *enc = packet.m_body;
	enc = AMFObject::EncodeString(enc, pEnd, sg_av_NetStream_Authenticate_UsherToken);
	enc = AMFObject::EncodeNumber(enc, pEnd, ++m_numInvokes);
	*enc++ = AMF_NULL;
	enc = AMFObject::EncodeString(enc, pEnd, strUsherToken);
	if (!enc)
	{
		return false;
	}
	packet.m_nBodySize = enc - packet.m_body;

	return SendPacket(&packet, false);
}

bool CRtmpClient::SendFCUnpublish()
{
	RtmpMsg packet;
	char pBuff[1024] = { 0 };
	char* pEnd = pBuff + sizeof(pBuff);

	packet.m_csid = 0x03;	/* control channel (invoke) */
	packet.m_fmt = BASICHEADER_FMT_ONE;
	packet.m_msgType = MSGHEADER_TYPE_INVOKE;
	packet.m_nTimeStamp = 0;
	packet.m_nInfoField2 = 0;
	packet.m_hasAbsTimestamp = 0;
	packet.m_body = pBuff + RTMP_MAX_HEADER_SIZE;

	char* enc = packet.m_body;
	enc = AMFObject::EncodeString(enc, pEnd, sg_av_FCUnpublish);
	enc = AMFObject::EncodeNumber(enc, pEnd, ++m_numInvokes);
	*enc++ = AMF_NULL;
	enc = AMFObject::EncodeString(enc, pEnd, Link.playpath);
	if (!enc)
	{
		return false;
	}
	packet.m_nBodySize = enc - packet.m_body;

	return SendPacket(&packet, false);
}

bool CRtmpClient::SendPublish()
{
	RtmpMsg packet;
	char pBuff[1024] = {};
	char* pEnd = pBuff + sizeof(pBuff);

	packet.m_csid = 0x04;	/* source channel (invoke) */
	packet.m_fmt = BASICHEADER_FMT_ZREO;
	packet.m_msgType = MSGHEADER_TYPE_INVOKE;
	packet.m_nTimeStamp = 0;
	packet.m_nInfoField2 = m_stream_id;
	packet.m_hasAbsTimestamp = 0;
	packet.m_body = pBuff + RTMP_MAX_HEADER_SIZE;

	char* pBodyEn = packet.m_body;
	pBodyEn = AMFObject::EncodeString(pBodyEn, pEnd, sg_av_publish);
	pBodyEn = AMFObject::EncodeNumber(pBodyEn, pEnd, ++m_numInvokes);
	*pBodyEn++ = AMF_NULL;
	pBodyEn = AMFObject::EncodeString(pBodyEn, pEnd, Link.playpath);
	if (!pBodyEn)
	{
		return false;
	}
	/* FIXME: should we choose live based on Link.lFlags & RTMP_LF_LIVE? */
	pBodyEn = AMFObject::EncodeString(pBodyEn, pEnd, sg_av_live);
	if (!pBodyEn)
	{
		return false;
	}
	packet.m_nBodySize = pBodyEn - packet.m_body;

	return SendPacket(&packet, true);
}

bool CRtmpClient::SendPlaylist()
{
	RtmpMsg packet;
	char pBuff[1024] = {0};
	char* pEnd = pBuff + sizeof(pBuff);

	packet.m_csid = 0x08;	/* we make 8 our stream channel */
	packet.m_fmt = BASICHEADER_FMT_ZREO;
	packet.m_msgType = MSGHEADER_TYPE_INVOKE;
	packet.m_nTimeStamp = 0;
	packet.m_nInfoField2 = m_stream_id;	/*0x01000000; */
	packet.m_hasAbsTimestamp = 0;
	packet.m_body = pBuff + RTMP_MAX_HEADER_SIZE;

	char* enc = packet.m_body;
	enc = AMFObject::EncodeString(enc, pEnd, sg_av_set_playlist);
	enc = AMFObject::EncodeNumber(enc, pEnd, 0);
	*enc++ = AMF_NULL;
	*enc++ = AMF_ECMA_ARRAY;
	*enc++ = 0;
	*enc++ = 0;
	*enc++ = 0;
	*enc++ = AMF_OBJECT;
	enc = AMFObject::EncodeNamedString(enc, pEnd, sg_av_0, Link.playpath);
	if (!enc)
	{
		return false;
	}
	if (enc + 3 >= pEnd)
	{
		return false;
	}
	*enc++ = 0;
	*enc++ = 0;
	*enc++ = AMF_OBJECT_END;
	packet.m_nBodySize = enc - packet.m_body;

	return SendPacket(&packet, true);
}

bool CRtmpClient::SendPing(double dTxn)
{
	RtmpMsg packet;
	char pBuff[256] = {0};
	char* pEnd = pBuff + sizeof(pBuff);

	packet.m_csid = 0x03;	/* control channel (invoke) */
	packet.m_fmt = BASICHEADER_FMT_ONE;
	packet.m_msgType = MSGHEADER_TYPE_INVOKE;
	packet.m_nTimeStamp = 0x16 * m_nBWCheckCounter;	/* temp inc value. till we figure it out. */
	packet.m_nInfoField2 = 0;
	packet.m_hasAbsTimestamp = 0;
	packet.m_body = pBuff + RTMP_MAX_HEADER_SIZE;

	char* pBodyEn = packet.m_body;
	pBodyEn = AMFObject::EncodeString(pBodyEn, pEnd, sg_av_pong);
	pBodyEn = AMFObject::EncodeNumber(pBodyEn, pEnd, dTxn);
	*pBodyEn++ = AMF_NULL;
	packet.m_nBodySize = pBodyEn - packet.m_body;

	return SendPacket(&packet, false);
}

bool CRtmpClient::SendPause(bool bPause, int iTime)
{
	RtmpMsg packet;
	char pBuff[256] = {0};
	char* pEnd = pBuff + sizeof(pBuff);

	packet.m_csid = 0x08;	/* video channel */
	packet.m_fmt = BASICHEADER_FMT_ONE;
	packet.m_msgType = MSGHEADER_TYPE_INVOKE;
	packet.m_nTimeStamp = 0;
	packet.m_nInfoField2 = 0;
	packet.m_hasAbsTimestamp = 0;
	packet.m_body = pBuff + RTMP_MAX_HEADER_SIZE;

	char* pBodyEn = packet.m_body;
	pBodyEn = AMFObject::EncodeString(pBodyEn, pEnd, sg_av_pause);
	pBodyEn = AMFObject::EncodeNumber(pBodyEn, pEnd, ++m_numInvokes);
	*pBodyEn++ = AMF_NULL;
	pBodyEn = AMFObject::EncodeBoolean(pBodyEn, pEnd, bPause);
	pBodyEn = AMFObject::EncodeNumber(pBodyEn, pEnd, (double)iTime);
	packet.m_nBodySize = pBodyEn - packet.m_body;

	//RTMP_Log(RTMP_LOGDEBUG, "%s, %d, pauseTime=%d", __FUNCTION__, (int)bPause, iTime);
	return SendPacket(&packet, true);
}

void CRtmpClient::AV_queue(RTMP_METHOD ** vals, int * num, AVal * av, int txn)
{
	char *tmp;
	if (!(*num & 0x0f))
	{
		*vals = (RTMP_METHOD *)realloc(*vals, (*num + 16) * sizeof(RTMP_METHOD));
	}

	tmp = (char *)malloc(av->av_len + 1);
	memcpy(tmp, av->av_val, av->av_len);
	tmp[av->av_len] = '\0';
	(*vals)[*num].num = txn;
	(*vals)[*num].name.av_len = av->av_len;
	(*vals)[(*num)++].name.av_val = tmp;
}

void CRtmpClient::AV_erase(RTMP_METHOD * vals, int * num, int i, int freeit)
{
	if (freeit)
	{
		free(vals[i].name.av_val);
	}

	(*num)--;
	for (; i < *num; i++)
	{
		vals[i] = vals[i + 1];
	}
	vals[i].name.av_val = NULL;
	vals[i].name.av_len = 0;
	vals[i].num = 0;
}

int CRtmpClient::RTMP_FindFirstMatchingProperty(AMFObject* pObj, const AVal * name, AMFObjectProperty * p)
{
	/* this is a small object search to locate the "duration" property */
	for (int n = 0; n < pObj->o_num; n++)
	{
		AMFObjectProperty* pObjProp = pObj->GetObjectProp(sg_emptyVal, n);
		if (AVMATCH(&pObjProp->p_name, name))
		{
			memcpy(p, pObjProp, sizeof(*pObjProp));
			return true;
		}

		if (pObjProp->p_type == AMF_OBJECT || pObjProp->p_type == AMF_ECMA_ARRAY)
		{
			if (RTMP_FindFirstMatchingProperty(&pObjProp->p_vu.p_object, name, p))
			{
				return true;
			}
		}
	}
	return FALSE;
}
