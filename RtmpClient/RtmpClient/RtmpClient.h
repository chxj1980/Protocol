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

	int SendPacket(RTMPPacket *packet, bool bInQueue);

	void Close();

protected:
	bool InitSocket();
	bool UnInitSocket();

	// 解析rtmp地址
	int ParseRtmpURL(OUT int& iProtocol,
		OUT AVal& strHost,
		OUT unsigned int& iPort,
		OUT AVal& strPlayPath,
		OUT AVal& strApp,
		IN const char* strUrl);

	int ParseRtmpURL2(OUT int& iProtocol,
		OUT AVal& strHost,
		OUT unsigned int& iPort,
		OUT AVal& strApp,
		IN const char* strUrl);

	bool ConnectSocket();
	bool HandShake();
	bool ConnectRtmp();
	bool ConnectStream();

	// 控制消息;
	// type: 0x0001		set chunk size
	bool SendSetChunkSize(int iChunkSize);

	// type: 0x0014 命令消息
	// connect
	bool SendConnect();

private:

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
	//RTMP_METHOD *m_methodCalls;	/* remote method calls queue */

	int m_channelsAllocatedIn;
	int m_channelsAllocatedOut;
	//RTMPPacket **m_vecChannelsIn;
	RTMPPacket **m_vecChannelsOut;
	int *m_channelTimestamp;	/* abs timestamp of last packet */

	double m_fAudioCodecs;	/* audioCodecs for the connect packet */
	double m_fVideoCodecs;	/* videoCodecs for the connect packet */
	double m_fEncoding;		/* AMF0 or AMF3 */

	double m_fDuration;		/* duration of stream in seconds */

	int m_msgCounter;		/* RTMPT stuff */
	int m_polling;
	int m_resplen;
	int m_unackd;
	AVal m_clientID;

	//RTMP_READ m_read;
	//RTMPPacket m_write;
	RTMPSockBuf m_sb;
	RTMP_LNK	Link;		// 连接服务器信息;
};

#endif