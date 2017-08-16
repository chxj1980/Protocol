#include "RtmpClient.h"

int main()
{
	CRtmpClient* pClient = new CRtmpClient();
	pClient->Init();

	pClient->SetRtmpURL("rtmp://teacher.xescdn.com/live_bak/test12");
	pClient->EnablePushed(true);

	pClient->Connect();

	pClient->UnInit();
	delete pClient;

	return 1;
}