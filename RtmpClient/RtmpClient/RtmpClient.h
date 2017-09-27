#ifndef __RTMPCLIENT_H__
#define __RTMPCLIENT_H__
#include "RtmpDefs.h"
#include "amf.h"

class CRtmpClient
{
public:
	CRtmpClient();
	~CRtmpClient();

	// 初始化、反初始化
	bool Init();
	bool UnInit();

	// 设置rtmp地址
	int SetRtmpURL(IN const char* strUrl);
	int SetRtmpURL2(IN const char* strUrl, IN const char* strPlayPath);

	void EnablePushed(bool bPushed);

	bool Connect();

	bool IsConnected();
	bool SendPacket(IN RtmpMsg* const packet, bool bInQueue);
	bool ReadPacket(OUT RtmpMsg *packet);
	//int Read(OUT RTMPPacket *packet);

	int HandlePacket(IN RtmpMsg* const packet);

	// type: 1
	bool HandleSetChunkSize(const RtmpMsg *packet);
	// type: 5
	bool HandleServerBW(const RtmpMsg *packet);
	// type: 6
	bool HandleClientBW(const RtmpMsg *packet);

	int HandleMetadata(char *body, unsigned int len);
	
	bool HandleAudio(const RtmpMsg *packet);
	bool HandleVideo(const RtmpMsg *packet);
	bool HandleCtrl(const RtmpMsg *packet);
	// type: 20
	int HandleInvoke(const char *body, unsigned int nBodySize);
	

	void Close();

	bool RTMPPacket_Alloc(RtmpMsg *p, int nSize);
	void RTMPPacket_Free(RtmpMsg *p);

protected:
	bool InitSocket();
	bool UnInitSocket();

	// 解析rtmp地址
	static int ParseRtmpURL(OUT int& iProtocol,
		OUT AVal& strHost,
		OUT unsigned int& iPort,
		OUT AVal& strPlayPath,
		OUT AVal& strApp,
		IN const char* strUrl);

	static int ParseRtmpURL2(OUT int& iProtocol,
		OUT AVal& strHost,
		OUT unsigned int& iPort,
		OUT AVal& strApp,
		IN const char* strUrl);

	static void ParsePlaypath(AVal* in, AVal* out);

	bool ConnectSocket();
	bool HandShake();
	bool ConnectRtmp();
	bool ConnectStream(IN const int iSeekTime);

	// 控制消息;
	// type: 0x0001		set chunk size
	bool SendSetChunkSize(int iChunkSize);

	// type: 0x0014 命令消息
	// connect
	bool SendConnect();

	bool SendSecureTokenResponse(AVal* resp);

	bool SendReleaseStream();

	bool SendFCPublish();

	bool SendServerBW();
	bool SendClientBW();

	bool SendCtrl(short nType, unsigned int nObject, unsigned int nTime);

	bool SendCreateStream();

	bool SendCheckBW();
	bool SendCheckBWResult(double dTxn);
	bool SendDeleteStream(double dStreamId);
	bool SendFCSubscribe(const AVal& strSubscribePath);
	bool SendPlay();
	bool SendBytesReceived();
	bool SendUsherToken(const AVal& strUsherToken);
	bool SendFCUnpublish();

	bool SendPublish();

	bool SendPlaylist();
	bool SendPing(double dTxn);

	bool SendPause(bool bPause, int iTime);

	void AV_queue(RTMP_METHOD** vals, int *num, AVal* av, int txn);
	void AV_erase(RTMP_METHOD* vals, int *num, int i, int freeit);

	int RTMP_FindFirstMatchingProperty(AMFObject *obj, const AVal* name, AMFObjectProperty * p);

	static void CRtmpClient::DecodeTEA(AVal* key, AVal* text);

public:

	int m_inChunkSize;
	int m_outChunkSize;
	int m_nBWCheckCounter;
	int m_nBytesIn;
	int m_nBytesInSent;
	int m_nBufferMS;
	int m_stream_id;		/* returned in _result from createStream */
	int m_mediaChannel;
	uint32_t m_mediaStamp;
	uint32_t m_pauseStamp;
	int m_pausing;
	int m_nServerBW;
	int m_nClientBW;
	uint8_t m_nClientBW2;
	uint8_t m_bPlaying;
	uint8_t m_bSendEncoding;
	uint8_t m_bSendCounter;

	uint8_t m_bUseNagle;
	uint8_t m_bCustomSend;
	void*   m_customSendParam;
	//CUSTOMSEND m_customSendFunc;

	//RTMP_BINDINFO m_bindIP;

	uint8_t m_bSendChunkSizeInfo;

	int m_numInvokes;
	int m_numCalls;
	RTMP_METHOD *m_methodCalls;	/* remote method calls queue */

	int m_channelsAllocatedIn;
	int m_channelsAllocatedOut;
	RtmpMsg** m_vecChannelsIn;
	RtmpMsg** m_vecChannelsOut;
	int* m_channelTimestamp;	/* abs timestamp of last packet */

	double m_fAudioCodecs;	/* audioCodecs for the connect packet */
	double m_fVideoCodecs;	/* videoCodecs for the connect packet */
	double m_fEncoding;		/* AMF0 or AMF3 */

	double m_fDuration;		/* duration of stream in seconds */

	int m_msgCounter;		/* RTMPT stuff */
	int m_polling;
	int m_resplen;
	int m_unackd;
	AVal m_clientID;

	RTMP_READ m_read;
	RtmpMsg m_write;
	RTMPSockBuf m_sb;
	RTMP_LNK	Link;		// 连接服务器信息;
};

#endif