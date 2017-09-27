#include "RtmpClient.h"
#include <time.h>

#define HTON16(x)  ((x>>8&0xff)|(x<<8&0xff00))
#define HTON24(x)  ((x>>16&0xff)|(x<<16&0xff0000)|(x&0xff00))
#define HTON32(x)  ((x>>24&0xff)|(x>>8&0xff00)|(x << 8 & 0xff0000) | (x << 24 & 0xff000000))
#define HTONTIME(x) ((x>>16&0xff)|(x<<16&0xff0000)|(x&0xff00)|(x&0xff000000))

int ReadU8(uint32_t *u8, FILE*fp);
int ReadU16(uint32_t *u16, FILE*fp);
int ReadU24(uint32_t *u24, FILE*fp);
int ReadU32(uint32_t *u32, FILE*fp);
int PeekU8(uint32_t *u8, FILE*fp);
int ReadTime(uint32_t *utime, FILE*fp);


//char* rtmpurl = "rtmp://www.bj-mobiletv.com:8000/live/test";	//连接的URL;
char* rtmpurl = "rtmp://teacher.xescdn.com/live_bak/test12";	//连接的URL;
char* flvfilename = "test.flv";	// 读取的flv文件;

								// VLC拉流地址： rtmp://www.bj-mobiletv.com:8000/live/test;

int main()
{
	long start = 0;
	long perframetime = 0;
	long lasttime = 0;
	int bNextIsKey = 1;

	CRtmpClient* pClient = new CRtmpClient();
	pClient->Init();

	//pClient->SetRtmpURL("rtmp://teacher.xescdn.com/live_bak/test11");
	//pClient->SetRtmpURL("rtmp://www.bj-mobiletv.com:8000/live/tst100");
	pClient->SetRtmpURL("rtmp://10.99.1.140/live/test");
	pClient->EnablePushed(true);

	pClient->Connect();

	RtmpMsg* packet = new RtmpMsg();
	pClient->RTMPPacket_Alloc(packet, 1024 * 256);	// 给packet分配数据空间;
	//memset(packet-<bo)
	//pClient->RTMPPacket_Reset(packet);				// 重置packet状态;

	packet->m_hasAbsTimestamp = 0;	// 绝对时间戳;
	packet->m_csid = 0x09;		// 通道;
	packet->m_nInfoField2 = pClient->m_stream_id;

	FILE* pFileFlv = NULL;
	pFileFlv = fopen(flvfilename, "rb");
	if (pFileFlv == NULL)
	{
		printf("Open File:%s Err\n", flvfilename);
		return -1;
	}

	printf("rtmpurl:%s\nflvfile:%s\nsend data ...\n", rtmpurl, flvfilename);
	////////////////////////////////////////发送数据//////////////////////
	fseek(pFileFlv, 9, SEEK_SET);	// 跳过前9个字节;
	fseek(pFileFlv, 4, SEEK_CUR);	// 跳过4字节长度;
	start = time(NULL) - 1;
	perframetime = 0;		// 上一帧时间戳;
	while (TRUE)
	{
		uint32_t type = 0;			// 类型;
		uint32_t datalength = 0;	// 数据长度;
		uint32_t timestamp = 0;		// 时间戳;
		uint32_t streamid = 0;		// 流ID;
		uint32_t alldatalength = 0;	// 该帧总长度;

		if (((time(NULL) - start)<(perframetime / 1000)) && bNextIsKey)
		{
			// 发的太快就等一下;
			if (perframetime > lasttime)
			{
				printf("TimeStamp:%8lu ms\n", perframetime);
				lasttime = perframetime;
			}
#ifdef WIN32
			Sleep(1000);
#else			
			sleep(1);
#endif
			continue;
		}
		if (!ReadU8(&type, pFileFlv))
		{
			break;
		}
		if (!ReadU24(&datalength, pFileFlv))
		{
			break;
		}
		if (!ReadTime(&timestamp, pFileFlv))
		{
			break;
		}
		if (!ReadU24(&streamid, pFileFlv))
		{
			break;
		}

		if (type != 0x08 && type != 0x09)
		{
			// 跳过非音视频桢;
			fseek(pFileFlv, datalength + 4, SEEK_CUR);
			continue;
		}
		if (fread(packet->m_body, 1, datalength, pFileFlv) != datalength)
		{
			break;
		}

		//packet->m_headerType = BASICHEADER_FMT_ONE;
		packet->m_fmt = BASICHEADER_FMT_ZREO;
		packet->m_nTimeStamp = timestamp;
		packet->m_msgType = type;
		packet->m_nBodySize = datalength;

		if (!pClient->IsConnected())
		{
			printf("rtmp is not connect\n");
			break;
		}
		if (!pClient->SendPacket(packet, 0))
		{
			printf("Send Err\n");
			break;
		}
		if (!ReadU32(&alldatalength, pFileFlv))
		{
			break;
		}

		perframetime = timestamp;
		///////////////判断下一帧是否关键帧////////////////		
		bNextIsKey = 0;
		if (!PeekU8(&type, pFileFlv))
		{
			break;
		}

		if (type == 0x09)
		{
			if (fseek(pFileFlv, 11, SEEK_CUR) != 0)
			{
				break;
			}
			if (!PeekU8(&type, pFileFlv))
			{
				break;
			}
			if (type == 0x17)
			{
				bNextIsKey = 1;
			}
			fseek(pFileFlv, -11, SEEK_CUR);
		}
		////////////////////////////////////		
	}
	printf("\nSend Data Over\n");

	fclose(pFileFlv);

	pClient->UnInit();
	delete pClient;

	return 0;
}

int ReadU8(uint32_t *u8, FILE*fp)
{
	if (fread(u8, 1, 1, fp) != 1)
	{
		return 0;
	}
	return 1;
}
int ReadU16(uint32_t *u16, FILE*fp)
{
	if (fread(u16, 2, 1, fp) != 1)
	{
		return 0;
	}
	*u16 = HTON16(*u16);
	return 1;
}
int ReadU24(uint32_t *u24, FILE*fp)
{
	if (fread(u24, 3, 1, fp) != 1)
	{
		return 0;
	}
	*u24 = HTON24(*u24);
	return 1;
}
int ReadU32(uint32_t *u32, FILE*fp)
{
	if (fread(u32, 4, 1, fp) != 1)
	{
		return 0;
	}
	*u32 = HTON32(*u32);
	return 1;
}
int PeekU8(uint32_t *u8, FILE*fp)
{
	if (fread(u8, 1, 1, fp) != 1)
	{
		return 0;
	}
	fseek(fp, -1, SEEK_CUR);
	return 1;
}
int ReadTime(uint32_t *utime, FILE*fp)
{
	if (fread(utime, 4, 1, fp) != 1)
	{
		return 0;
	}
	*utime = HTONTIME(*utime);
	return 1;
}