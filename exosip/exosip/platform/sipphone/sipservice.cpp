#define _CRT_SECURE_NO_WARNINGS
//HAVE_OPENSSL_SSL_H��TSL_SUPPORT
#include <rtpsession.h>
#include <rtppacket.h>
#include "rtpudpv4transmitter.h"
#include "rtpipv4address.h"
#include "rtpsessionparams.h"
#include "rtperrors.h"
#include "rtpsourcedata.h"
#include "timestramp.h"

#ifndef WIN32
#include <netinet/in.h>
#include <arpa/inet.h>
#else
#include <winsock2.h>
#endif // WIN32
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <string>

#include "easylogging++.h"
#define USELOG
#ifdef USELOG
INITIALIZE_EASYLOGGINGPP
#endif

/*
����ϵͳͷ�ļ�
*/
#include <iostream>
#include <windows.h>

#ifdef INTHEPATH
#include <mxml/mxml.h>
#else
#include <mxml.h>
#endif

#include <time.h>
#include <process.h>
//���ر���ͷ�ļ�
#include "filenameio.h"
/*
���ù���ͷ�ļ�
*/
//#include <Video.h>

//ffmpeg�����
extern "C"
{
	#include "libavcodec/avcodec.h"
	#include "libavutil/avutil.h"
	#include "libswscale/swscale.h"
};


using namespace jrtplib;

#pragma  comment(lib,"avcodec.lib")
#pragma  comment(lib,"avutil.lib")
#pragma  comment(lib,"swscale.lib")

#ifdef _DEBUG
#pragma comment(lib, "jrtplib_d.lib") 
#pragma comment(lib,"jthread_d.lib")
#pragma comment(lib,"WS2_32.lib")
#else
#pragma comment(lib, "jrtplib.lib") 
#pragma comment(lib,"jthread.lib")
#pragma comment(lib,"WS2_32.lib")
#endif

//opencv�����
#include "opencv/cv.h"
#include "opencv/highgui.h"
#include "opencv2/imgproc/imgproc_c.h"
#pragma comment(lib,"opencv_world346d.lib")

//#pragma comment(lib,"opencv_core2413.lib")
//#pragma comment(lib,"opencv_highgui2413.lib")
//#pragma comment(lib,"opencv_imgproc2413.lib") 

#include <atlstr.h>
/*
����sip����ͷ�ļ��;�̬��
*/
#include <eXosip2/eXosip.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "mxml1.lib")
#pragma comment(lib, "eXosip2.lib")
#pragma comment(lib, "libcares.lib")
#pragma comment(lib, "osip2.lib")

//Dnsapi.lib;Iphlpapi.lib;ws2_32.lib;eXosip.lib;osip2.lib;osipparser2.lib;Qwave.lib;libcares.lib;delayimp.lib;
//���� libcmt.libĬ�Ͽ�
#pragma comment(lib, "Dnsapi.lib")
#pragma comment(lib, "Iphlpapi.lib")
#pragma comment(lib, "osipparser2.lib")
#pragma comment(lib, "Qwave.lib")
#pragma comment(lib, "delayimp.lib")


/*
	RTPתH264��ز���
*/
#ifndef uint16_t
typedef unsigned short uint16_t;
#endif
#ifndef uint32_t
typedef unsigned int uint32_t;
#endif
//#ifndef uint64_t
//typedef unsigned int uint64_t;
//#endif

typedef struct RTP_HEADER
{
	uint16_t cc : 4;
	uint16_t extbit : 1;
	uint16_t padbit : 1;
	uint16_t version : 2;
	uint16_t paytype : 7;  //��������
	uint16_t markbit : 1;  //1��ʾǰ��İ�Ϊһ�����뵥Ԫ,0��ʾ��ǰ���뵥Ԫδ����
	uint16_t seq_number;  //���
	uint32_t timestamp; //ʱ���
	uint32_t ssrc;  //ѭ��У����
	//uint32_t csrc[16];
} RTP_header_t;

typedef union littel_endian_size_s {
	unsigned short int	length;
	unsigned char		byte[2];
} littel_endian_size;

typedef struct pack_start_code_s {
	unsigned char start_code[3];
	unsigned char stream_id[1];
} pack_start_code;

typedef struct program_stream_pack_header_s {
	pack_start_code PackStart;// 4
	unsigned char Buf[9];
	unsigned char stuffinglen;
} program_stream_pack_header;

typedef struct program_stream_map_s {
	pack_start_code PackStart;
	littel_endian_size PackLength;//we mast do exchange
	//program_stream_info_length
	//info
	//elementary_stream_map_length
	//elem
} program_stream_map;

typedef struct program_stream_e_s {
	pack_start_code		PackStart;
	littel_endian_size	PackLength;//we mast do exchange
	char				PackInfo1[2];
	unsigned char		stuffing_length;
} program_stream_e;

//���������Ϣ
#define CAMERA_SUPPORT_MAX      500
#define RTP_MAXBUF          (100*4096)
#define PS_BUF_SIZE         (1024*1024*4)
#define H264_FRAME_SIZE_MAX (1024*1024*4)

typedef struct _SipParams{
	char platformSipId[MAX_PATH];
	char platformIpAddr[MAX_PATH];
	int platformSipPort;
	char localSipId[MAX_PATH];
	char localIpAddr[MAX_PATH];
	int localSipPort;
	int SN;
	struct eXosip_t *eCtx;
	int call_id;
	int dialog_id;
	int registerOk;
	int running;
	_SipParams() {
		registerOk = 0;
	}
} SipParams;

typedef struct {
	char sipId[MAX_PATH];
	char UserName[MAX_PATH];
	char UserPwd[MAX_PATH];
	int recvPort;
	int status;
	int statusErrCnt;
	FILE *fpH264;
	int running;
} CameraParams;

typedef struct _liveVideoStreamParams{
	int cameraNum;
	CameraParams *pCameraParams;
	SipParams gb28181Param;
	int stream_input_type;
	int running;
} liveVideoStreamParams;

#define MAX_TMP_BUF_SIZE 4*1024*1024

typedef void (*StreamCallback)(void*, void*, int,int);
typedef void(*RGBCallback)(void*, void*, int, int);
typedef void(*TrigCallback)(void*, void*, int, int);
typedef void(*PixBufCallback)(void*, void*, int, int);

/*�������ݽṹ*/
struct internal_handle
{
	StreamCallback OnStreamReady;/*ʵʱ��Ƶ���ص�*/
	RGBCallback OnRealRGBReady;  /*ʵʱ����ͼƬ�ص�*/
	RGBCallback OnTrigRGBReady;  /*�ⲿ����ץ��ͼƬ�ص�*/
	TrigCallback OnTrigRGBReadyEx;  /*�ⲿ����ץ��ͼƬ����Ϣ�ص�*/
	
	//Э����Ϣ�������Ϣ
	liveVideoStreamParams hliveVideoParams;

	std::string strServerIP;
	std::string strRemoteIP;
	std::string strUserName;
	int nServerPort;
	int nRemotePort;
	int nRTPRecvPort;

	//�������
	HANDLE hVideoChannel;
	bool  bRealCallback;
	bool  bTrigCallback;
	void* pUserData;
	SwsContext* pCtx;
	char* pRGBBuffer;
	long lUserHandle; //�û����    
	long lRealHandle; //ʵʱ�����	
	long lDecodePort; //����ͨ����
	LONG lAlarmHandle;//�����������
	long lChannel;
	long lCount;
	bool Gateflag;  //��բ״̬ true����  false:��
	int nCameraID;  //���ID
	//CvxText *pText;
	//	CVehicleDetect *pVehicleDetect;

	char frameData[MAX_TMP_BUF_SIZE];
	int   total;

	BYTE H264Buff[MAX_TMP_BUF_SIZE];
	internal_handle()
	{
		OnStreamReady = NULL;
		//OnRealRGBReady = NULL;
		//OnTrigRGBReady = NULL;
		//OnTrigRGBReadyEx = NULL;
		bRealCallback = false;
		bTrigCallback = false;
		pUserData = NULL;
		pCtx = NULL;
		pRGBBuffer = NULL;
		lUserHandle = 0; //�û����    
		lRealHandle = 0; //ʵʱ�����	
		lDecodePort = 0; //����ͨ����
		lAlarmHandle = 0;//�����������
		lChannel = 0;
		lCount = 0;
		//	pText = NULL;
		//	pVehicleDetect = NULL;
		Gateflag = false;
	}
};

typedef struct _VideoParam {
	long AddressNum;
	int nServerPort;
	std::string strServerIP;
	int nRTPRecvPort;
	std::string strRTPRecvIP;
	std::string strUserName;
	std::string strRemoteName;
}VideoParam;

typedef struct PES_HEADER_tag
{
	unsigned char packet_start_code_prefix[3];
	unsigned char stream_id;
	unsigned char PES_packet_length[2];

	unsigned char original_or_copy : 1;
	unsigned char copyright : 1;
	unsigned char data_alignment_indicator : 1;
	unsigned char PES_priority : 1;
	unsigned char PES_scrambling_control : 2;
	unsigned char fix_bit : 2;

	unsigned char PES_extension_flag : 1;
	unsigned char PES_CRC_flag : 1;
	unsigned char additional_copy_info_flag : 1;
	unsigned char DSM_trick_mode_flag : 1;
	unsigned char ES_rate_flag : 1;
	unsigned char ESCR_flag : 1;
	unsigned char PTS_DTS_flags : 2;

	unsigned char PES_header_data_length;
	PES_HEADER_tag()
	{
		packet_start_code_prefix[0] = 0x00;
		packet_start_code_prefix[1] = 0x00;
		packet_start_code_prefix[2] = 0x01;

		PES_packet_length[0] = 0x00;
		PES_packet_length[1] = 0x00;

		stream_id = 0xE0;
		fix_bit = 0x02;
	}

}*pPES_HEADER_tag;

//jrtp����պ���
//���
void checkerror(int rtperr)
{
	if (rtperr < 0)
	{
		std::cout << "ERROR: " << RTPGetErrorString(rtperr) << std::endl;
		LOG(INFO) << RTPGetErrorString(rtperr);
		exit(-1);
	}
}

class MyRTPSession : public RTPSession
{
protected:
	void OnNewSource(RTPSourceData *dat)
	{
		if (dat->IsOwnSSRC())
			return;

		uint32_t ip;
		uint16_t port;

		if (dat->GetRTPDataAddress() != 0)
		{
			const RTPIPv4Address *addr = (const RTPIPv4Address *)(dat->GetRTPDataAddress());
			ip = addr->GetIP();
			port = addr->GetPort();
		}
		else if (dat->GetRTCPDataAddress() != 0)
		{
			const RTPIPv4Address *addr = (const RTPIPv4Address *)(dat->GetRTCPDataAddress());
			ip = addr->GetIP();
			port = addr->GetPort() - 1;
		}
		else
			return;

		RTPIPv4Address dest(ip, port);
		AddDestination(dest);

		struct in_addr inaddr;
		inaddr.s_addr = htonl(ip);
		std::cout << "Adding destination " << std::string(inet_ntoa(inaddr)) << ":" << port << std::endl;
	}

	void OnBYEPacket(RTPSourceData *dat)
	{
		if (dat->IsOwnSSRC())
			return;

		uint32_t ip;
		uint16_t port;

		if (dat->GetRTPDataAddress() != 0)
		{
			const RTPIPv4Address *addr = (const RTPIPv4Address *)(dat->GetRTPDataAddress());
			ip = addr->GetIP();
			port = addr->GetPort();
		}
		else if (dat->GetRTCPDataAddress() != 0)
		{
			const RTPIPv4Address *addr = (const RTPIPv4Address *)(dat->GetRTCPDataAddress());
			ip = addr->GetIP();
			port = addr->GetPort() - 1;
		}
		else
			return;

		RTPIPv4Address dest(ip, port);
		DeleteDestination(dest);

		struct in_addr inaddr;
		inaddr.s_addr = htonl(ip);
		std::cout << "Deleting destination " << std::string(inet_ntoa(inaddr)) << ":" << port << std::endl;
	}

	void OnRemoveSource(RTPSourceData *dat)
	{
		if (dat->IsOwnSSRC())
			return;
		if (dat->ReceivedBYE())
			return;

		uint32_t ip;
		uint16_t port;

		if (dat->GetRTPDataAddress() != 0)
		{
			const RTPIPv4Address *addr = (const RTPIPv4Address *)(dat->GetRTPDataAddress());
			ip = addr->GetIP();
			port = addr->GetPort();
		}
		else if (dat->GetRTCPDataAddress() != 0)
		{
			const RTPIPv4Address *addr = (const RTPIPv4Address *)(dat->GetRTCPDataAddress());
			ip = addr->GetIP();
			port = addr->GetPort() - 1;
		}
		else
			return;

		RTPIPv4Address dest(ip, port);
		DeleteDestination(dest);

		struct in_addr inaddr;
		inaddr.s_addr = htonl(ip);
		std::cout << "Deleting destination " << std::string(inet_ntoa(inaddr)) << ":" << port << std::endl;
	}
};

/*
���ÿ�Դ��������ݻ�ȡ
*/
static unsigned __stdcall jrtplib_rtp_recv_thread(void* arg)
{
	internal_handle* ptrHandle = (internal_handle*)arg;
	int rtp_port = ptrHandle->hliveVideoParams.pCameraParams->recvPort;

	//����H264��Ƶ����ʾ
	//CH264Decoder HDecoder;

	//��ʼ��
	//HDecoder.Initial();

	//��ȡ�������
	//CameraParams *p = (CameraParams *)arg;

	//�����ʼ��
	//H264_Init();

#ifdef WIN32
	WSADATA dat;
	WSAStartup(MAKEWORD(2, 2), &dat);
#endif // WIN32

	MyRTPSession sess;
	uint16_t portbase;
	std::string ipstr;
	int status, i, num;

	RTPUDPv4TransmissionParams transparams;
	RTPSessionParams sessparams;

	transparams.SetRTPReceiveBuffer(1024 * 1024*1024);
	//transparams.SetRTCPReceiveBuffer(1400);
	//transparams.SetRTCPSendBuffer(1400);
	//ʱ���
	sessparams.SetOwnTimestampUnit(1.0 / 3600.0);

	portbase = rtp_port;

	sessparams.SetAcceptOwnPackets(true);
	sessparams.SetMaximumPacketSize(10 * 4096);
	transparams.SetPortbase(portbase);
	sessparams.SetUsePollThread(true);
	status = sess.Create(sessparams, &transparams);
	checkerror(status);

	//д����Ƶ�ļ�
	//��ȡ��ǰ����·��
	std::string strPath = GetMoudlePath();
	char filename[MAX_PATH];
	strPath += ptrHandle->hliveVideoParams.pCameraParams->sipId;
	_snprintf(filename, 128, "%s.264", strPath.c_str());
	ptrHandle->hliveVideoParams.pCameraParams->fpH264 = fopen(filename, "wb");
	if (ptrHandle->hliveVideoParams.pCameraParams->fpH264 == NULL)
	{
		LOG(INFO) << "fopen failed:" << filename;
		return NULL;
	}

	unsigned char* h264Buf;
	h264Buf = (unsigned char*)malloc(PS_BUF_SIZE);
	memset(h264Buf, '\0', PS_BUF_SIZE);
	int h264Len = 0;

#if 0
	char* h263buf;
	h263buf = (char*)malloc(1000);

	//�������ȡ�ļ�����ʾ
	while (!feof(ptrHandle->hliveVideoParams.pCameraParams->fpH264))
	{
		fread(h263buf, 1, 1000, ptrHandle->hliveVideoParams.pCameraParams->fpH264);
		ptrHandle->OnStreamReady(ptrHandle->pUserData, (void*)h263buf, 1000, 0);
		Sleep(100);
	}

	return 0;
#endif

	/*unsigned char* h100buf;
	h100buf = (unsigned char*)malloc(1000);
	memset(h100buf, '\0', 100);

	while (!feof(ptrHandle->hliveVideoParams.pCameraParams->fpH264))
	{
		fread(h100buf, 1, 100, ptrHandle->hliveVideoParams.pCameraParams->fpH264);
		ptrHandle->OnStreamReady(ptrHandle->pUserData, (void*)h264Buf, h264Len, 0);
	}*/

	/*
		�ӽ�����ر���
	*/
	//unsigned char* houtbuf;
	//houtbuf = (unsigned char*)malloc(PS_BUF_SIZE);
	//unsigned int noutsize;
	//int nwidth;
	//int nheight;

	//��ʼ��������
	while (ptrHandle->hliveVideoParams.pCameraParams->running)
	{
		sess.BeginDataAccess();

		// check incoming packets
		if (sess.GotoFirstSourceWithData())
		{
			do
			{
				RTPPacket *pack;

				while ((pack = sess.GetNextPacket()) != NULL)
				{
					//д���ļ�
					//fwrite(pack->GetPayloadData(), 1, pack->GetPayloadLength(), ptrHandle->hliveVideoParams.pCameraParams->fpH264);

					if (pack->HasMarker())				//�ж�β��
					{
						memcpy(h264Buf + h264Len, pack->GetPayloadData(), pack->GetPayloadLength());
						//LOG(INFO) << pack->GetSequenceNumber();
						h264Len += pack->GetPayloadLength();
						LOG(INFO) << "H264��С:" << h264Len;
						//д���¼�
						timestramp tim;

						//�����һ�����к��ƽ�ȥ
						//seqNum.push_back(pack->GetSequenceNumber());

						//�ж��Ƿ񶪰�
						//bool bIsWrong = true;
						//std::vector<int>::iterator iterSeq = seqNum.begin();
						//for (; iterSeq != seqNum.end(); ++iterSeq)
						//{
						//	*iterSeq += 1;
						//	if (++iterSeq == seqNum.end())
						//	{
						//		bIsWrong = false;
						//		seqNum.clear();
						//		break;
						//	}
						//	if (*iterSeq != *--iterSeq)
						//		break;
						//}

						fwrite(h264Buf, 1, h264Len, ptrHandle->hliveVideoParams.pCameraParams->fpH264);

						//���벢��ʾ
						//if (!bIsWrong)
						//H264_2_RGB(h264Buf, h264Len, houtbuf, &noutsize, &nwidth, &nheight);
						ptrHandle->OnStreamReady(ptrHandle->pUserData, (void*)h264Buf, h264Len, 0);
						//ptrHandle->OnRealRGBReady(ptrHandle->pUserData, (void*)houtbuf, nwidth, nwidth * 2, nheight);
						memset(h264Buf, 0, h264Len);		//��ջ���������һ�δ洢
						h264Len = 0;
					}
					else
					{
						memcpy(h264Buf + h264Len, pack->GetPayloadData(), pack->GetPayloadLength());
						h264Len += pack->GetPayloadLength();
						//LOG(INFO) << pack->GetSequenceNumber();

						//��ȡ���к�
						//seqNum.push_back(pack->GetSequenceNumber());

					}
					sess.DeletePacket(pack);
				}
			} while (sess.GotoNextSourceWithData());
		}

		sess.EndDataAccess();

#ifndef RTP_SUPPORT_THREAD
		status = sess.Poll();
		checkerror(status);
#endif // RTP_SUPPORT_THREAD

		RTPTime::Wait(RTPTime(0, 0));
	}

	sess.BYEDestroy(RTPTime(10, 0), 0, 0);

#ifdef WIN32
	WSACleanup();
#endif // WIN32

	return 0;
}

//��ʼ��udp�׽���
int init_udpsocket(int port, struct sockaddr_in *servaddr, char *mcast_addr);
void release_udpsocket(int socket_fd, char *mcast_addr);

// ��ini�ļ���ȡ���������Ϣ
static int ParserIniFile(void* ptrHandle, char *iniCfgFile)
{
	//��ӡ��־
	std::string strLogPath = GetMoudlePath();
	LOG(INFO) << "��ʼ���������ļ�";

	//liveVideoStreamParams *pHandle = (liveVideoStreamParams*)ptrHandle;
	internal_handle* pHandle = (internal_handle*)ptrHandle;
	::GetPrivateProfileString("sipinfo", "platform_id", "���", pHandle->hliveVideoParams.gb28181Param.platformSipId, MAX_PATH, iniCfgFile);	//��ȡƽ̨ID
	pHandle->hliveVideoParams.gb28181Param.platformSipPort = GetPrivateProfileInt("sipinfo", "platform_port", 0, iniCfgFile);					//��ȡƽ̨�˿�
	::GetPrivateProfileString("sipinfo", "platform_ip", "���", pHandle->hliveVideoParams.gb28181Param.platformIpAddr, MAX_PATH, iniCfgFile);	//��ȡƽ̨IP
	::GetPrivateProfileString("sipinfo", "local_id", "���", pHandle->hliveVideoParams.gb28181Param.localSipId, MAX_PATH, iniCfgFile);		//��ȡ����ID
	pHandle->hliveVideoParams.gb28181Param.localSipPort = GetPrivateProfileInt("sipinfo", "local_port", 0, iniCfgFile);						//��ȡ���ض˿�
	::GetPrivateProfileString("sipinfo", "local_ip", "���", pHandle->hliveVideoParams.gb28181Param.localIpAddr, MAX_PATH, iniCfgFile);		//��ȡƽ̨IP
	pHandle->hliveVideoParams.cameraNum = GetPrivateProfileInt("sipinfo", "camera_num", 0, iniCfgFile);										//�������

	if (pHandle->hliveVideoParams.cameraNum > 0 && pHandle->hliveVideoParams.cameraNum < CAMERA_SUPPORT_MAX) 
	{
		pHandle->hliveVideoParams.pCameraParams = (CameraParams *)malloc(sizeof(CameraParams)*pHandle->hliveVideoParams.cameraNum);
		if (pHandle->hliveVideoParams.pCameraParams == NULL) 
		{
			//APP_DEBUG("malloc failed");
			LOG(INFO) << "malloc, failed";
			return -1;
		}
		memset(pHandle->hliveVideoParams.pCameraParams, 0, sizeof(CameraParams)*pHandle->hliveVideoParams.cameraNum);
		CameraParams *p;
		//֧�ֶ�·���
		for (int i = 0; i < pHandle->hliveVideoParams.cameraNum; ++i)
		{
			p = pHandle->hliveVideoParams.pCameraParams + i;

			GetPrivateProfileString("sipinfo", "camera1_sip_id", "", p->sipId, MAX_PATH, iniCfgFile);
			p->recvPort = GetPrivateProfileInt("sipinfo", "camera1_recv_port", 0, iniCfgFile);

			//��ȡ�����¼��������
			GetPrivateProfileString("sipinfo", "UserPwd", "", p->UserPwd, MAX_PATH, iniCfgFile);
			GetPrivateProfileString("sipinfo", "UserName", "", p->UserName, MAX_PATH, iniCfgFile);
		}
	}

	pHandle->hliveVideoParams.gb28181Param.SN = 1;
	pHandle->hliveVideoParams.gb28181Param.call_id = -1;
	pHandle->hliveVideoParams.gb28181Param.dialog_id = -1;
	pHandle->hliveVideoParams.gb28181Param.registerOk = 0;
	pHandle->hliveVideoParams.running = 1;
	pHandle->hliveVideoParams.gb28181Param.running = 1;

	LOG(INFO) << "���������ļ����";

	return 0;
}

// ��ȡ�����ļ�·��
std::string RXGetModuleFilePath()
{
	std::string str;
	CHAR szFile[255] = { 0 };
	int len = 0;
	DWORD dwRet = GetModuleFileName(NULL, szFile, 255);
	if (dwRet != 0)
	{
		str = (szFile);
		int nPos = (int)str.rfind('\\');
		if (nPos != -1)
		{
			str = str.replace(nPos, str.length(), "\\sipinfo.ini");
		}
	}
	return str;
}

static void RegisterSuccess(struct eXosip_t * peCtx, eXosip_event_t *je)
{
	int iReturnCode = 0;
	osip_message_t * pSRegister = NULL;
	iReturnCode = eXosip_message_build_answer(peCtx, je->tid, 200, &pSRegister);
	if (iReturnCode == 0 && pSRegister != NULL)
	{
		eXosip_lock(peCtx);
		eXosip_message_send_answer(peCtx, je->tid, 200, pSRegister);
		eXosip_unlock(peCtx);
		//osip_message_free(pSRegister);
	}
}

void RegisterFailed(struct eXosip_t * peCtx, eXosip_event_t *je)
{
	int iReturnCode = 0;
	osip_message_t * pSRegister = NULL;
	iReturnCode = eXosip_message_build_answer(peCtx, je->tid, 401, &pSRegister);
	if (iReturnCode == 0 && pSRegister != NULL)
	{
		eXosip_lock(peCtx);
		eXosip_message_send_answer(peCtx, je->tid, 401, pSRegister);
		eXosip_unlock(peCtx);
	}
}

inline int pe2es(char * tmpBuf, int nDataSize, void *pUser)
{
	internal_handle *pih = (internal_handle*)pUser;

	int nSearchPos = 0;
	int nSavePos = 0;
	UINT piclen = 0;
	while (nSearchPos<(nDataSize - 12))
	{
		int nWritelen = 0;
		// Ѱ��pes
		BYTE* p = (BYTE*)tmpBuf + nSearchPos;
		if (p[0] == 0 && p[1] == 0 && p[2] == 1 && p[3] == 0xE0)
		{// �ҵ�pesͷ
			PES_HEADER_tag* pes_head = (PES_HEADER_tag*)p;
			unsigned short usPesHeadLen = pes_head->PES_header_data_length;
			unsigned short usPacketLen;
			char* p2 = (char*)&usPacketLen;
			p2[0] = pes_head->PES_packet_length[1];
			p2[1] = pes_head->PES_packet_length[0];
			BYTE* pH264Begin = p + 9 + usPesHeadLen;
			int   nH264Len = usPacketLen - 3 - usPesHeadLen;
			//nWritelen = fwrite(pH264Begin,1,nH264Len,ffh264);
			if (nH264Len >0)
			{
				memcpy(pih->H264Buff + nSavePos, pH264Begin, nH264Len);
				nSavePos += nH264Len;
				nSearchPos += usPesHeadLen + 6;//����λ��������ps��
				continue;
			}
		}
		nSearchPos++;
	}

	if (pih->OnStreamReady != NULL)//����׼��Ƶ��
	{
		//pih->OnStreamReady(pih->pUserData, (void*)pih->H264Buff, nSavePos, 0);
		fwrite(pih->H264Buff, 1, nSavePos, pih->hliveVideoParams.pCameraParams->fpH264);
	}
	return nSearchPos;

}

void  CALLBACK iveInternal_StreamReadyCB(int handle, const char* data, int size, void *pUser)
{
	internal_handle *pih = (internal_handle*)pUser;

	if (pih == NULL || !pih->bRealCallback)
	{
		return;
	}
	unsigned char *buffer = (unsigned char *)data;
	int header_position = -1;
	for (int i = 0; i<size - 4; i++)
	{
		//LOG(INFO) << "����Ҫ������� << buffer[i] << buffer[i + 1] << buffer[i + 2] << buffer[i + 3];
		if (buffer[i] == 0x00 && buffer[i + 1] == 0x00 && buffer[i + 2] == 0x01 && buffer[i + 3] == 0xBA)
		{
			header_position = i;
			break;
		}
	}

	if (header_position>-1)
	{
		if (header_position > 0)
		{
			memcpy(pih->frameData + pih->total, (char *)data, header_position);
			pih->total += header_position;
		}

		if (pih->total>0)
		{
			int nRet = pe2es(pih->frameData, pih->total, (void*)pih);
		}

		memset(pih->frameData, 0, MAX_TMP_BUF_SIZE);//�����һ֡FRAME����		

		memcpy(pih->frameData, (char *)data + header_position, size - header_position);
		pih->total = size - header_position;
	}
	else
	{
		if (pih->total > 0)
		{
			memcpy(pih->frameData + pih->total, data, size);
			pih->total += size;
		}
	}

	return;
}

/*
	�����ĸ�������RTPתH264�õ��ĺ���
*/
int inline ProgramStreamPackHeader(char* Pack, int length, char **NextPack, int *leftlength)
{
	//printf("[%s]%x %x %x %x\n", __FUNCTION__, Pack[0], Pack[1], Pack[2], Pack[3]);
	//ͨ�� 00 00 01 baͷ�ĵ�14���ֽڵ����3λ��ȷ��ͷ������˶����ֽ�
	program_stream_pack_header *PsHead = (program_stream_pack_header *)Pack;
	unsigned char pack_stuffing_length = PsHead->stuffinglen & '\x07';

	*leftlength = length - sizeof(program_stream_pack_header)-pack_stuffing_length;//��ȥͷ�������ֽ�
	*NextPack = Pack + sizeof(program_stream_pack_header)+pack_stuffing_length;
	if (*leftlength<4)
		return 0;

	return *leftlength;
}

inline int ProgramStreamMap(char* Pack, int length, char **NextPack, int *leftlength, char **PayloadData, int *PayloadDataLen)
{
	program_stream_map* PSMPack = (program_stream_map*)Pack;

	//no payload
	*PayloadData = 0;
	*PayloadDataLen = 0;

	if ((unsigned int)length < sizeof(program_stream_map)) return 0;

	littel_endian_size psm_length;
	psm_length.byte[0] = PSMPack->PackLength.byte[1];
	psm_length.byte[1] = PSMPack->PackLength.byte[0];

	*leftlength = length - psm_length.length - sizeof(program_stream_map);
	if (*leftlength <= 0) return 0;

	*NextPack = Pack + psm_length.length + sizeof(program_stream_map);

	return *leftlength;
}

inline int ProgramShHead(char* Pack, int length, char **NextPack, int *leftlength, char **PayloadData, int *PayloadDataLen)
{
	program_stream_map* PSMPack = (program_stream_map*)Pack;

	//no payload
	*PayloadData = 0;
	*PayloadDataLen = 0;

	if ((unsigned int)length < sizeof(program_stream_map)) return 0;

	littel_endian_size psm_length;
	psm_length.byte[0] = PSMPack->PackLength.byte[1];
	psm_length.byte[1] = PSMPack->PackLength.byte[0];

	*leftlength = length - psm_length.length - sizeof(program_stream_map);
	if (*leftlength <= 0)
		return 0;

	*NextPack = Pack + psm_length.length + sizeof(program_stream_map);

	return *leftlength;
}

//
inline int Pes(char* Pack, int length, char **NextPack, int *leftlength, char **PayloadData, int *PayloadDataLen)
{
	program_stream_e* PSEPack = (program_stream_e*)Pack;

	*PayloadData = 0;
	*PayloadDataLen = 0;

	if ((unsigned int)length < sizeof(program_stream_e)) return 0;

	littel_endian_size pse_length;
	pse_length.byte[0] = PSEPack->PackLength.byte[1];
	pse_length.byte[1] = PSEPack->PackLength.byte[0];

	*PayloadDataLen = pse_length.length - 2 - 1 - PSEPack->stuffing_length;
	if (*PayloadDataLen>0)
		*PayloadData = Pack + sizeof(program_stream_e)+PSEPack->stuffing_length;

	*leftlength = length - pse_length.length - sizeof(pack_start_code)-sizeof(littel_endian_size);
	if (*leftlength <= 0) return 0;

	*NextPack = Pack + sizeof(pack_start_code)+sizeof(littel_endian_size)+pse_length.length;

	return *leftlength;
}

bool GetH246FromPSEasy(IN BYTE* pBuffer, IN int nBufLenth, BYTE** pH264, int& nH264Lenth, BOOL& bVideo)
{
	if (!pBuffer || nBufLenth <= 0)
	{
		return FALSE;
	}

	BYTE* pH264Buffer = NULL;
	int nHerderLen = 0;

	if (pBuffer
		&& pBuffer[0] == 0x00
		&& pBuffer[1] == 0x00
		&& pBuffer[2] == 0x01
		&& pBuffer[3] == 0xE0)//E==��Ƶ����(�˴�E0��ʶΪ��Ƶ)
	{
		bVideo = TRUE;
		nHerderLen = 9 + (int)pBuffer[8];//9��Ϊ�̶������ݰ�ͷ���ȣ�pBuffer[8]Ϊ���ͷ���ֵĳ���
		pH264Buffer = pBuffer + nHerderLen;
		if (*pH264 == NULL)
		{
			*pH264 = new BYTE[nBufLenth];
		}
		if (*pH264&&pH264Buffer && (nBufLenth - nHerderLen)>0)
		{
			memcpy(*pH264, pH264Buffer, (nBufLenth - nHerderLen));
		}
		nH264Lenth = nBufLenth - nHerderLen;

		return TRUE;
	}
	else if (pBuffer
		&& pBuffer[0] == 0x00
		&& pBuffer[1] == 0x00
		&& pBuffer[2] == 0x01
		&& pBuffer[3] == 0xC0) //C==��Ƶ���ݣ�
	{
		*pH264 = NULL;
		nH264Lenth = 0;
		bVideo = FALSE;
	}
	else if (pBuffer
		&& pBuffer[0] == 0x00
		&& pBuffer[1] == 0x00
		&& pBuffer[2] == 0x01
		&& pBuffer[3] == 0xBA)//��Ƶ�����ݰ� ��ͷ
	{
		bVideo = TRUE;
		*pH264 = NULL;
		nH264Lenth = 0;
		return FALSE;
	}
	return FALSE;
}

//trp����h264��Ƶ��Ϣ
int inline GetH246FromPs(char* buffer, int length, char *h264Buffer, int *h264length, char *sipId)
{
	int leftlength = 0;
	char *NextPack = 0;

	*h264length = 0;

	if (ProgramStreamPackHeader(buffer, length, &NextPack, &leftlength) == 0)
		return 0;

	char *PayloadData = NULL;
	int PayloadDataLen = 0;

	while ((unsigned int)leftlength >= sizeof(pack_start_code))
	{
		PayloadData = NULL;
		PayloadDataLen = 0;

		if (NextPack && NextPack[0] == '\x00' && NextPack[1] == '\x00' && NextPack[2] == '\x01' && NextPack[3] == '\xE0')
		{
			//���ž���������˵���Ƿ�i֡
			if (Pes(NextPack, leftlength, &NextPack, &leftlength, &PayloadData, &PayloadDataLen))
			{
				if (PayloadDataLen)
				{
					if (PayloadDataLen + *h264length < H264_FRAME_SIZE_MAX)
					{
						memcpy(h264Buffer, PayloadData, PayloadDataLen);
						h264Buffer += PayloadDataLen;
						*h264length += PayloadDataLen;
					}
					else
					{
						LOG(INFO) << "h264 frame size exception!!" << PayloadDataLen<< ":" <<  *h264length;
					}
				}
			}
			else
			{
				if (PayloadDataLen)
				{
					if (PayloadDataLen + *h264length < H264_FRAME_SIZE_MAX)
					{
						memcpy(h264Buffer, PayloadData, PayloadDataLen);
						h264Buffer += PayloadDataLen;
						*h264length += PayloadDataLen;
					}
					else
					{
						LOG(INFO) << "h264 frame size exception!!" << PayloadDataLen << ":" << *h264length;
					}
				}
				break;
			}
		}
		else if (NextPack
			&& NextPack[0] == '\x00'
			&& NextPack[1] == '\x00'
			&& NextPack[2] == '\x01'
			&& NextPack[3] == '\xBB')
		{
			if (ProgramShHead(NextPack, leftlength, &NextPack, &leftlength, &PayloadData, &PayloadDataLen) == 0)
				break;
		}
		else if (NextPack
			&& NextPack[0] == '\x00'
			&& NextPack[1] == '\x00'
			&& NextPack[2] == '\x01'
			&& NextPack[3] == '\xBC')
		{
			if (ProgramStreamMap(NextPack, leftlength, &NextPack, &leftlength, &PayloadData, &PayloadDataLen) == 0)
				break;
		}
		else if (NextPack
			&& NextPack[0] == '\x00'
			&& NextPack[1] == '\x00'
			&& NextPack[2] == '\x01'
			&& (NextPack[3] == '\xC0' || NextPack[3] == '\xBD'))
		{
			LOG(INFO) << "audio ps frame, skip it\n";
			break;
		}
		else
		{
			LOG(INFO) << "no know :" << sipId <<  NextPack[0] << NextPack[1] << NextPack[2] << NextPack[3];
			break;
		}
	}

	return *h264length;
}

//������Ƶ��Ϣ��SDP��Ϣ
static int sendInvitePlay(char *playSipId, int rtp_recv_port, SipParams *p28181Params)
{
	char dest_call[256], source_call[256], subject[128];
	osip_message_t *invite = NULL;
	int ret;
	struct eXosip_t *peCtx = p28181Params->eCtx;

	_snprintf(dest_call, 256, "sip:%s@%s:%d", playSipId, p28181Params->platformIpAddr, p28181Params->platformSipPort);
	_snprintf(source_call, 256, "sip:%s@%s", p28181Params->localSipId, p28181Params->localIpAddr);
	_snprintf(subject, 128, "%s:0,%s:0", playSipId, p28181Params->localSipId);
	ret = eXosip_call_build_initial_invite(peCtx, &invite, dest_call, source_call, NULL, subject);
	if (ret != 0)
	{
		LOG(INFO) << "eXosip_call_build_initial_invite failed:" << dest_call << source_call << subject;
		return -1;
	}

	//sdp
	char body[1024];
#if 1
	int bodyLen = _snprintf(body, 1024, 
		"v=0\n"
		"o=%s 0 0 IN IP4 %s\n"
		"s=Play\n"
		"c=IN IP4 %s\n"
		"t=0 0\n"
		"m=video %d RTP/AVP 96 98 97\n"
		"a=recvonly\n"
		"a=rtpmap:96 PS/90000\n"
		"a=rtpmap:98 H264/90000\n"
		"a=rtpmap:97 MPEG4/90000\n"
		"y=0100000001\n",
		playSipId,
		p28181Params->localIpAddr,
		p28181Params->localIpAddr,
		rtp_recv_port);
#elif 0
	int bodyLen = _snprintf(body, 1024,
		"v=0\n"
		"o=%s 0 0 IN IP4 %s\n"
		"s=Play\n"
		"c=IN IP4 %s\n"
		"t=0 0\n"
		"m=video %d RTP/AVP 96 98\n"
		"a=recvonly\n"
		"a=rtpmap:96 PS/90000\n"
		"a=rtpmap:98 H264/90000\n",
		playSipId,
		p28181Params->localIpAddr,
		p28181Params->localIpAddr,
		rtp_recv_port);
#else
	int bodyLen = _snprintf(body, 1024,
	"v=0\r\n"
	"o=%s 0 0 IN IP4 %s\r\n"
	"s=Play\r\n"
	"c=IN IP4 %s\r\n"
	"t=0 0\r\n"
	"m=video %d RTP/AVP 96 97 98\r\n"
	"a=rtpmap:96 PS/90000\r\n"
	"a=rtpmap:97 MPEG4/90000\r\n"
	"a=rtpmap:98 H264/90000\r\n"
/*	"a=ptime:20"
	"a=framerate:1"
	"a=recvonly\r\n"
	"y=0100000001\n"*/, playSipId, p28181Params->localIpAddr,
	p28181Params->localIpAddr, rtp_recv_port);
#endif

	osip_message_set_body(invite, body, bodyLen);
	osip_message_set_content_type(invite, "APPLICATION/SDP");
	eXosip_lock(peCtx);
	eXosip_call_send_initial_invite(peCtx, invite);
	eXosip_unlock(peCtx);

	return 0;
}

//ֹͣ��Ƶ�ش�
static int sendPlayBye(SipParams *p28181Params)
{
	struct eXosip_t *peCtx = p28181Params->eCtx;

	eXosip_lock(peCtx);
	eXosip_call_terminate(peCtx, p28181Params->call_id, p28181Params->dialog_id);
	eXosip_unlock(peCtx);
	return 0;
}

//ֹͣ�������Ƶ�ش�
static int stopCameraRealStream(liveVideoStreamParams *pliveVideoParams)
{
	int i, tryCnt;
	CameraParams *p;
	SipParams *p28181Params = &(pliveVideoParams->gb28181Param);

	for (i = 0; i < pliveVideoParams->cameraNum; i++)
	{
		p = pliveVideoParams->pCameraParams + i;
		p28181Params->call_id = -1;
		sendInvitePlay(p->sipId, p->recvPort, p28181Params);
		//sendPlayBye(p28181Params);	//ֹͣ��Ƶ�ش�
		tryCnt = 10;
		while (tryCnt-- > 0)
		{
			if (p28181Params->call_id != -1)
			{
				break;
			}
			//Sleep(1000);
		}
		if (p28181Params->call_id == -1)
		{
			LOG(INFO) << "exception wait call_id:"<< p28181Params->call_id << "sipid:" << p->sipId;
		}
		sendPlayBye(p28181Params);

		p->running = 0;
	}

	return 0;
}

//����������ش���Ƶ
static int startCameraRealStream(liveVideoStreamParams *pliveVideoParams)
{
	int i;
	CameraParams *p;

	for (i = 0; i < pliveVideoParams->cameraNum; i++)
	{
		p = pliveVideoParams->pCameraParams + i;
		sendInvitePlay(p->sipId, p->recvPort, &(pliveVideoParams->gb28181Param));
	}

	return 0;
}

//��֤���״̬
static int checkCameraStatus(liveVideoStreamParams *pliveVideoParams)
{
	int i;
	CameraParams *p;
	SipParams *p28181Params = &(pliveVideoParams->gb28181Param);

	for (i = 0; i < pliveVideoParams->cameraNum; i++)
	{
		p = pliveVideoParams->pCameraParams + i;
		if (p->status == 0)
		{
			p->statusErrCnt++;
			if (p->statusErrCnt % 10 == 0)
			{
				LOG(INFO) << "camera is exception, restart it" << p->sipId;
				p28181Params->call_id = -1;
				sendInvitePlay(p->sipId, p->recvPort, p28181Params);
				p->statusErrCnt = 0;

			}
		}
		else
		{
			p->statusErrCnt = 0;
			p->status = 0;
		}
	}

	return 0;
}

//ֹͣ����
static int stopStreamRecv(liveVideoStreamParams *pliveVideoParams)
{
	int i;
	CameraParams *p;

	for (i = 0; i < pliveVideoParams->cameraNum; i++)
	{
		p = pliveVideoParams->pCameraParams + i;
		p->running = 0;
	}

	return 0;
}

const char *whitespace_cb(mxml_node_t *node, int where)
{
	return NULL;
}

//��������catalog��Ϣ
static int sendQueryCatalog(SipParams *p28181Params)
{
	char sn[32];
	int ret;
	mxml_node_t *tree, *query, *node;
	struct eXosip_t *peCtx = p28181Params->eCtx;
	char *deviceId = p28181Params->localSipId;

	tree = mxmlNewXML("1.0");
	if (tree != NULL)
	{
		query = mxmlNewElement(tree, "Query");
		if (query != NULL)
		{
			char buf[256] = { 0 };
			char dest_call[256], source_call[256];
			node = mxmlNewElement(query, "CmdType");
			mxmlNewText(node, 0, "Catalog");
			node = mxmlNewElement(query, "SN");
			_snprintf(sn, 32, "%d", p28181Params->SN++);
			mxmlNewText(node, 0, sn);
			node = mxmlNewElement(query, "DeviceID");
			mxmlNewText(node, 0, deviceId);
			mxmlSaveString(tree, buf, 256, whitespace_cb);
			//printf("send query catalog:%s\n", buf);
			osip_message_t *message = NULL;
			_snprintf(dest_call, 256, "sip:%s@%s:%d", p28181Params->platformSipId,
				p28181Params->platformIpAddr, p28181Params->platformSipPort);
			_snprintf(source_call, 256, "sip:%s@%s", p28181Params->localSipId, p28181Params->localIpAddr);
			ret = eXosip_message_build_request(peCtx, &message, "MESSAGE", dest_call, source_call, NULL);
			if (ret == 0 && message != NULL)
			{
				osip_message_set_body(message, buf, strlen(buf));
				osip_message_set_content_type(message, "Application/MANSCDP+xml");
				eXosip_lock(peCtx);
				eXosip_message_send_request(peCtx, message);
				eXosip_unlock(peCtx);
				LOG(INFO) << "xml:%s, dest_call:%s, source_call:%s, ok", buf, dest_call, source_call;
			}
			else
			{
				LOG(INFO) << "eXosip_message_build_request failed";
			}
		}
		else
		{
			LOG(INFO) << "mxmlNewElement Query failed";
		}
		mxmlDelete(tree);
	}
	else
	{
		LOG(INFO) << "mxmlNewXML failed";
	}

	return 0;
}

static unsigned __stdcall tcp_rtp_recv_thread(void* ptrParams)
{
	int socket_fd;
	internal_handle* ptrHandle = (internal_handle*)ptrParams;
	int rtp_port = ptrHandle->hliveVideoParams.pCameraParams->recvPort;
	struct sockaddr_in servaddr;

	socket_fd = init_udpsocket(rtp_port, &servaddr, NULL);


}

//��������ش���rtp��Ƶ��
static unsigned __stdcall rtp_recv_thread(void *ptrParams)
{
	//��ʼ��������
	//H264_Init();
	int socket_fd;
#if 1
	internal_handle* ptrHandle = (internal_handle*)ptrParams;
	int rtp_port = ptrHandle->hliveVideoParams.pCameraParams->recvPort;
#else
	CameraParams *p = (CameraParams*)ptrParams;
	int rtp_port = p->recvPort;
#endif

	struct sockaddr_in servaddr;

	socket_fd = init_udpsocket(rtp_port, &servaddr, NULL);
	if (socket_fd >= 0)
	{
		LOG(INFO) << "start socket port %d success\n", rtp_port;
	}

	char *buf = (char *)malloc(RTP_MAXBUF);
	if (buf == NULL)
	{
		LOG(INFO) << "malloc failed buf";
		return NULL;
	}
	char *psBuf = (char *)malloc(PS_BUF_SIZE);
	if (psBuf == NULL)
	{
		LOG(INFO) << "malloc failed";
		return NULL;
	}
	memset(psBuf, '\0', PS_BUF_SIZE);
	char *h264buf = (char *)malloc(H264_FRAME_SIZE_MAX);
	if (h264buf == NULL)
	{
		LOG(INFO) << "malloc failed";
		return NULL;
	}
	int recvLen;
	int addr_len = sizeof(struct sockaddr_in);
	int rtpHeadLen = sizeof(RTP_header_t);

#if 1
	//д����Ƶ�ļ�
	//��ȡ��ǰ����·��
	std::string strPath = GetMoudlePath();
	char filename[MAX_PATH];
	strPath += ptrHandle->hliveVideoParams.pCameraParams->sipId;
	_snprintf(filename, 128, "%s.264", strPath.c_str());
	ptrHandle->hliveVideoParams.pCameraParams->fpH264 = fopen(filename, "wb");
	if (ptrHandle->hliveVideoParams.pCameraParams->fpH264 == NULL)
	{
		LOG(INFO) << "fopen %s failed", filename;
		return NULL;
	}

	LOG(INFO) << "%s:%d starting ...", ptrHandle->hliveVideoParams.pCameraParams->sipId, ptrHandle->hliveVideoParams.pCameraParams->recvPort;
#endif
	int cnt = 0;
	int rtpPsLen, h264length, psLen = 0;

	char *ptr;
	memset(buf, 0, RTP_MAXBUF);
#if 1
	while (ptrHandle->hliveVideoParams.pCameraParams->running)
#else
	while (p->running)
#endif 
	{
		//���յ���rtp�����ݳ���
		recvLen = recvfrom(socket_fd, buf, RTP_MAXBUF, 0, (struct sockaddr*)&servaddr, (int*)&addr_len);
		LOG(INFO) << "%d\n", recvLen;
		//������յ����ֶγ��Ȼ�û��rtp����ͷ������ֱ�ӽ���������
		if (recvLen > rtpHeadLen)
		{
#if 1
			//д���ļ�
			ptr = psBuf + psLen;			//�������ݵ�ͷ
			rtpPsLen = recvLen - rtpHeadLen;
			if (psLen + rtpPsLen < PS_BUF_SIZE)
			{
				memcpy(ptr, buf + rtpHeadLen, rtpPsLen);
			}
			else
			{
				LOG(INFO) << "psBuf memory overflow, %d\n", psLen + rtpPsLen;
				psLen = 0;
				continue;
			}
			//��Ƶ����
			if (ptr[0] == 0x00 && ptr[1] == 0x00 && ptr[2] == 0x01 && ptr[3] == 0xffffffBA&& psLen > 0)
			{
				if (cnt % 10000 == 0)
				{
					LOG(INFO) << "rtpRecvPort:%d, cnt:%d, pssize:%d\n", rtp_port, cnt++, psLen;
				}
				if (cnt % 25 == 0)
				{
					ptrHandle->hliveVideoParams.pCameraParams->status = 1;
				}

				fwrite(psBuf, 1, psLen, ptrHandle->hliveVideoParams.pCameraParams->fpH264);

				//BOOL bVideo = true;
				//GetH246FromPSEasy((BYTE*)psBuf, psLen, &h264buf, h264length, bVideo);
				GetH246FromPs((char *)psBuf, psLen, h264buf, &h264length, ptrHandle->hliveVideoParams.pCameraParams->sipId);
				if (h264length > 0)
				{
					timestramp tim;

					//д���ļ�
					//fwrite(h264buf, 1, h264length, ptrHandle->hliveVideoParams.pCameraParams->fpH264);

					//ֱ�Ӳ���
					ptrHandle->OnStreamReady(ptrHandle->pUserData, (void*)h264buf, h264length, 0);
					memset(psBuf, 0, h264length);
				}
				memcpy(psBuf, ptr, rtpPsLen);
				psLen = 0;
				cnt++;
			}
			psLen += rtpPsLen;
#else

			iveInternal_StreamReadyCB(0, (const char*)buf, recvLen, (void*)ptrHandle);
#endif
		}
		else
		{
			LOG(INFO) << "recvfrom() udp";
		}

		if (recvLen > 1500)
		{
			LOG(INFO) << "udp frame exception, %d\n", recvLen;
		}
	}

	release_udpsocket(socket_fd, NULL);
	if (buf != NULL)
	{
		free(buf);
	}
	if (psBuf != NULL)
	{
		free(psBuf);
	}
	if (h264buf != NULL)
	{
		free(h264buf);
	}

	LOG(INFO) << "%s:%d run over", ptrHandle->hliveVideoParams.pCameraParams->sipId, ptrHandle->hliveVideoParams.pCameraParams->recvPort;

	return NULL;
}

static unsigned __stdcall stream_keep_alive_thread(void *arg)
{
	int socket_fd;
	CameraParams *p = (CameraParams *)arg;
	int rtcp_port = p->recvPort + 1;
	struct sockaddr_in servaddr;

	socket_fd = init_udpsocket(rtcp_port, &servaddr, NULL);
	if (socket_fd >= 0)
	{
		LOG(INFO) << "start socket port %d success\n", rtcp_port;
	}

	char *buf = (char *)malloc(1024);
	if (buf == NULL)
	{
		LOG(INFO) << "malloc failed buf";
		return NULL;
	}
	int recvLen;
	int addr_len = sizeof(struct sockaddr_in);

	LOG(INFO) << "%s:%d starting ...", p->sipId, rtcp_port;

	memset(buf, 0, 1024);
	while (p->running)
	{
		recvLen = recvfrom(socket_fd, buf, 1024, 0, (struct sockaddr*)&servaddr, (int*)&addr_len);
		if (recvLen > 0)
		{
			LOG(INFO) << "stream_keep_alive_thread, rtcp_port %d, recv %d bytes\n", rtcp_port, recvLen;
			recvLen = sendto(socket_fd, buf, recvLen, 0, (struct sockaddr*)&servaddr, sizeof(struct sockaddr_in));
			if (recvLen <= 0)
			{
				LOG(INFO) << "sendto %d failed", rtcp_port;
			}
		}
		else
		{
			LOG(INFO) << "recvfrom() alive";
		}
	}

	release_udpsocket(socket_fd, NULL);
	if (buf != NULL)
	{
		free(buf);
	}

	LOG(INFO) << "%s:%d run over", p->sipId, rtcp_port;

	return NULL;
}

//�������֡�Ƿ���ڶ�����������ڶ����Ͷ�����������֪ͨ�ش�
void GetCRCCodec(void* pH264Buf)
{

}

//��ʼ������Ƶ��
static int StartStreamRecv(void* ptrParams)
{
	int i;
	HANDLE hHandle;
	HANDLE hHandleAlive;
	CameraParams *p;
	internal_handle* ptrHandle = (internal_handle*)ptrParams;
	liveVideoStreamParams *pliveVideoParams = (liveVideoStreamParams *)&(ptrHandle->hliveVideoParams);

	//��Ҫ֧�ֶ����
	for (i = 0; i < 1; i++)
	{
		p = pliveVideoParams->pCameraParams + i;
		p->statusErrCnt = 0;
		p->running = 1;
#if 1
		if ((hHandle = (HANDLE)_beginthreadex(NULL, 0, jrtplib_rtp_recv_thread, (void*)ptrHandle, 0, NULL)) == INVALID_HANDLE_VALUE)
#else
		if ((hHandle = (HANDLE)_beginthreadex(NULL, 0, rtp_recv_thread, (void*)ptrHandle, 0, NULL)) == INVALID_HANDLE_VALUE)
#endif
		{
			LOG(INFO) << "pthread_create rtp_recv_thread err, %s:%d", p->sipId, p->recvPort;
		}
		else
		{
			CloseHandle(hHandle);
		}
#if 0
		if ((hHandleAlive = (HANDLE)_beginthreadex(NULL, 0, stream_keep_alive_thread, (void*)p, 0, NULL)) == INVALID_HANDLE_VALUE) {
			LOG(INFO) << "pthread_create stream_keep_alive_thread err, %s:%d", p->sipId, p->recvPort + 1;
		}
		else
		{
			CloseHandle(hHandleAlive);
		}
#endif
	}

	return 0;
}

//��ʼ��udp�׽���
int init_udpsocket(int port, struct sockaddr_in *servaddr, char *mcast_addr)
{
	int err = -1;
	int socket_fd;

	socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (socket_fd < 0)
	{
		LOG(INFO) << "socket failed, port:%d", port;
		return -1;
	}

	memset(servaddr, 0, sizeof(struct sockaddr_in));
	servaddr->sin_family = AF_INET;
	servaddr->sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr->sin_port = htons(port);

	err = bind(socket_fd, (struct sockaddr*)servaddr, sizeof(struct sockaddr_in));
	if (err < 0)
	{
		LOG(INFO) << "bind failed, port:%d", port;
		return -2;
	}

	/*set enable MULTICAST LOOP */
	int loop = 20*1024*1024;
	//char loop[10] = "0";
	//err = setsockopt(socket_fd, IPPROTO_IP, SO_RCVBUF, (const char*)&loop, sizeof(loop));
	//err = setsockopt(socket_fd, IP_MULTICAST_LOOP, 4, (const char*)&loop, sizeof(loop));
	err = setsockopt(socket_fd, SOL_SOCKET, SO_RCVBUF, (const char*)&loop, sizeof(loop));
	if (err < 0)
	{
		LOG(INFO) << "setsockopt IP_MULTICAST_LOOP failed, port:%d", port;
		return -3;
	}

	return socket_fd;
}

//�ر��׽���
void release_udpsocket(int socket_fd, char *mcast_addr)
{
	closesocket(socket_fd);
}

//�����������Ϣ���������߳�
static void *MessageThread(SipParams *p28181Params, void * pvSClientGB)
{
	char *p;
	int keepAliveFlag = 0;
	struct eXosip_t * peCtx = (struct eXosip_t *)pvSClientGB;

	//�������ظ��������Ϣ
	while (p28181Params->running)
	{
		eXosip_event_t *je = NULL;
		//�����¼�
		je = eXosip_event_wait(peCtx, 0, 4);
		if (je == NULL)
		{
			osip_usleep(100);
			continue;
		}

		switch (je->type)
		{
		case EXOSIP_MESSAGE_NEW:				//����Ϣ����
		{
			LOG(INFO) << "new msg method:%s\n", je->request->sip_method;
			if (MSG_IS_REGISTER(je->request))
			{
				LOG(INFO) << "recv Register";
				p28181Params->registerOk = 1;
			}
			else if (MSG_IS_MESSAGE(je->request))
			{
				osip_body_t *body = NULL;
				osip_message_get_body(je->request, 0, &body);
				if (body != NULL)
				{
					p = strstr(body->body, "Keepalive");
					if (p != NULL)
					{
						if (keepAliveFlag == 0)
						{
							LOG(INFO) << "msg body:%s\n", body->body;
							keepAliveFlag = 1;
							p28181Params->registerOk = 1;
						}
					}
					else
					{
						LOG(INFO) << "msg body:%s\n", body->body;
					}
				}
				else
				{
					LOG(INFO) << "get body failed";
				}
			}
			else if (strncmp(je->request->sip_method, "BYE", 4) != 0)
			{
				LOG(INFO) << "unsupport new msg method : %s", je->request->sip_method;
			}
			RegisterSuccess(peCtx, je);
			break;
		}
		case EXOSIP_MESSAGE_ANSWERED:				//��ѯ
		{
			LOG(INFO) << "answered method:%s\n", je->request->sip_method;
			RegisterSuccess(peCtx, je);
			break;
		}
		case EXOSIP_CALL_ANSWERED:
		{
			osip_message_t *ack = NULL;
			p28181Params->call_id = je->cid;
			p28181Params->dialog_id = je->did;
			LOG(INFO) << "call answered method:%s, call_id:%d, dialog_id:%d\n", je->request->sip_method, p28181Params->call_id, p28181Params->dialog_id;
			eXosip_call_build_ack(peCtx, je->did, &ack);
			eXosip_lock(peCtx);
			eXosip_call_send_ack(peCtx, je->did, ack);
			eXosip_unlock(peCtx);
			break;
		}
		case EXOSIP_CALL_PROCEEDING:
		{
			LOG(INFO) << "recv EXOSIP_CALL_PROCEEDING\n";
			RegisterSuccess(peCtx, je);
			break;
		}
		case EXOSIP_CALL_REQUESTFAILURE:
		{
			LOG(INFO) << "recv EXOSIP_CALL_REQUESTFAILURE\n";
			RegisterSuccess(peCtx, je);
			break;
		}
		case EXOSIP_CALL_MESSAGE_ANSWERED:
		{
			LOG(INFO) << "recv EXOSIP_CALL_MESSAGE_ANSWERED\n";
			RegisterSuccess(peCtx, je);
			break;
		}
		case EXOSIP_CALL_RELEASED:         //������Ƶ���ظ��ɹ�
		{
			LOG(INFO) << "recv EXOSIP_CALL_RELEASED\n";
			RegisterSuccess(peCtx, je);
			break;
		}
		case EXOSIP_CALL_CLOSED:
			LOG(INFO) << "recv EXOSIP_CALL_CLOSED\n";
			RegisterSuccess(peCtx, je);
			break;
		case EXOSIP_CALL_MESSAGE_NEW:
			LOG(INFO) << "recv EXOSIP_CALL_MESSAGE_NEW\n";
			RegisterSuccess(peCtx, je);
			break;
		default:
			LOG(INFO) << "##test,%s:%d, unsupport type:%d\n", __FILE__, __LINE__, je->type;
			RegisterSuccess(peCtx, je);
			break;
		}
		eXosip_event_free(je);
	}

	return NULL;
}

void Register(eXosip_t* sua)
{
	printf("1 ======  Register()\n");

	char fromuser[256];
	char proxy[256];
	char route[256];

	sprintf(fromuser, "sip:%s@%s", "803", "39.106.13.209:5060");
	//sprintf(proxy, "sip:%s@%s", "803", "39.106.13.209");
	sprintf(proxy, "sip:%s:%s", "39.106.13.209", "5060");
	sprintf(route, "<sip:%s:%d;lr>", "39.106.13.209", 5060);

	eXosip_clear_authentication_info(sua);
	osip_message_t *reg = NULL;
	int m_nregid = eXosip_register_build_initial_register(sua, fromuser, proxy, NULL, 3600, &reg);
	//��ǰ��������֤��Ϣ������ϢΪ401������eXosip_automatic_action()�Զ�����
	eXosip_add_authentication_info(sua, "803", "803", "803", "MD5", NULL);
	if (reg == NULL && m_nregid < 1)
	{
		LOG(INFO) << "Register Filaed";
		return;
	}
	osip_message_set_route(reg, route);
	if (eXosip_register_send_register(sua, m_nregid, reg) != 0)
		return;
}

/*
SIP�����߳�
*/
static unsigned __stdcall DaemonThread(void *ptrParams)
{
	int iReturnCode = 0;
	struct eXosip_t *eCtx;
	SipParams *ptrSipParams = (SipParams *)(ptrParams);

	//��ʼ��������Ϣ
	TRACE_INITIALIZE(6, NULL);

	//��ʼ��eXosip��osipջ
	eCtx = eXosip_malloc();
	iReturnCode = eXosip_init(eCtx);
	if (iReturnCode != OSIP_SUCCESS)
	{
		LOG(INFO) << "Can,t initialize, eXosip!";
		return NULL;
	}
	else
	{
		LOG(INFO) << "eXosip_init successfully!\n";
	}

	Register(eCtx);
	getchar();

	//��һ��UDP socket �����ź�
	iReturnCode = eXosip_listen_addr(eCtx, IPPROTO_UDP, NULL, ptrSipParams->localSipPort, AF_INET, 0);
	//iReturnCode = eXosip_listen_addr(eCtx, IPPROTO_TCP, NULL, ptrSipParams->localSipPort, AF_INET, 0);
	if (iReturnCode != OSIP_SUCCESS)
	{
		return NULL;
	}

	ptrSipParams->eCtx = eCtx;
	MessageThread(ptrSipParams, eCtx);

	eXosip_quit(eCtx);
	osip_free(eCtx);
	eCtx = NULL;
	ptrSipParams->eCtx = NULL;

	LOG(INFO) << "%s run over", __FUNCTION__;

	return 0;
}

/*
OSIP�������
*/
bool InitOsipService(void * ptrHandle)
{
	HANDLE hHandle;
	//1.���������ļ�
	//std::string strpath = RXGetModuleFilePath();
	//ParserIniFile((void*)ptrHandle, const_cast<char*>(strpath.c_str()));

	internal_handle* pHandle = (internal_handle*)ptrHandle;

	//�����������̣߳��������˿ڴ�����̣߳����������������������Ϣ
	if ((hHandle = (HANDLE)_beginthreadex(NULL, 0, DaemonThread, (void*)&(pHandle->hliveVideoParams.gb28181Param), 0, NULL)) == INVALID_HANDLE_VALUE)
	{
		LOG(INFO) << "error, pthread_create gb28181ServerThread err";
	}
	else
	{
		CloseHandle(hHandle);
	}

	int tmpCnt = 20;
	while ((!pHandle->hliveVideoParams.gb28181Param.registerOk) && (tmpCnt > 0))
	{
		LOG(INFO) << "waiting register ...\n" << tmpCnt--;
		Sleep(1000);
		if (tmpCnt == 0)
			MessageBox(NULL, _T("ע�ᳬʱ,����������"), _T("����"), MB_OK);
	}

	if (tmpCnt > 0)
	{
		//��������catalog��Ϣ
		sendQueryCatalog(&(pHandle->hliveVideoParams.gb28181Param));

		//������Ƶ��
		StartStreamRecv((void*)pHandle);
		Sleep(1000);

		//����������Ƶ��Ϣ
		startCameraRealStream(&pHandle->hliveVideoParams);
	}
	return true;
}

/*
�������ƣ�InitialVideoSource
����������������
ʵʱ��Ƶ���ص�
ʵʱ����ͼƬ�ص�
�ⲿ����ץ��ͼƬ�ص�
����˵������ʼ����ƵԴ������ص����ڻ�ȡ�������ⲿ����ץ�ĵ�ͼƬ
*/
void* InitialVideoSource(VideoParam *param, StreamCallback OnStreamReady, RGBCallback OnRealRGBReady, RGBCallback OnTrigRGBReady, void *pUserData)
{
	//��ʼ���ṹ��ָ��
	internal_handle *pih = new internal_handle;
	pih->OnStreamReady = OnStreamReady;
	pih->OnRealRGBReady = OnRealRGBReady;
	pih->OnTrigRGBReady = OnTrigRGBReady;
	//pih->bRealCallback = true;
	pih->pUserData = pUserData;
	pih->pRGBBuffer = NULL;
	pih->pCtx = NULL;
	pih->lRealHandle = 0;
	pih->lChannel = param->AddressNum;

	//��ֵ
	pih->strServerIP = param->strServerIP;
	pih->nServerPort = param->nServerPort;
	pih->strUserName = param->strUserName;

	strcpy(pih->hliveVideoParams.gb28181Param.platformIpAddr, param->strServerIP.c_str()); 
	strcpy(pih->hliveVideoParams.gb28181Param.platformSipId, param->strUserName.c_str());
	pih->hliveVideoParams.gb28181Param.platformSipPort = param->nServerPort;

	//��ʼ��SIP����
	InitOsipService((void *)pih);

	return (void*)pih;
}

/*
�������ƣ�InitialVideoSource3
��������������Ϣ
ʵʱ��Ƶ���ص�
������������ص�
ʵʱ����ͼƬ�ص�
�ⲿ����ץ��ͼƬ�ص�
�ⲿ����ץ��ͼƬ�ص�
����˵������ʼ����ƵԴ������ص����ڻ�ȡ�������ⲿ����ץ�ĵ�ͼƬ
*/
void* InitialVideoSource3(VideoParam *param, StreamCallback OnStreamReady, PixBufCallback OnPixBufReady, RGBCallback OnRealRGBReady, RGBCallback OnTrigRGBReady, TrigCallback OnTrigRGBReadyEx, void *pUserData)
{
	internal_handle *pih = NULL;
	char bufferUserPwd[20] = "";
	memset(bufferUserPwd, '\0', 20);
	//��ȡ�û���������
	char bufferUserName[20] = { 0 };
	memset(bufferUserName, 0, 20);
	std::string strPath = RXGetModuleFilePath();
	//��ȡ�˻�����
	::GetPrivateProfileString("sipinfo", "UserName", "admin", bufferUserPwd, MAX_PATH, strPath.c_str());
	::GetPrivateProfileString("sipinfo", "PassWord", "admin", bufferUserName, MAX_PATH, strPath.c_str());
	pih = (internal_handle*)InitialVideoSource(param, OnStreamReady, OnRealRGBReady, OnTrigRGBReady, pUserData);
	if (NULL != pih)
	{
		pih->OnTrigRGBReadyEx = OnTrigRGBReadyEx;
	}
	return (void*)pih;
}


/*��ȡ��ƵԴ��Ϣ������0ʧ�ܣ�1�ɹ�*/
 int  GetVideoInfo(void* hVS, int *decode, int *width, int *height, int *codec)
{
	internal_handle *pih = (internal_handle*)hVS;

	*decode = 0; //0-�ⲿ���룬1-�ڲ�����
	*width = 0;
	*height = 0;
	*codec = AV_CODEC_ID_H264;

	return 1;
}

/*������ƵԴ������0ʧ�ܣ�1�ɹ�*/
int  StartVideoSource(void* hVS)
{
	internal_handle *pih = (internal_handle*)hVS;
	pih->bRealCallback = true;
	pih->bTrigCallback = true;
	return 1;
}

/*��ͣ��ƵԴ�����øú�������Ȼ���Ե���StartVideoSource����*/
void  StopVideoSource(void* hVS)
{
	internal_handle *pih = (internal_handle*)hVS;
	pih->bRealCallback = false;
	pih->bTrigCallback = false;
	pih->hliveVideoParams.running = 0;
	stopCameraRealStream(&pih->hliveVideoParams);
	//Sleep(300);
	stopStreamRecv(&pih->hliveVideoParams);
	pih->hliveVideoParams.gb28181Param.running = 0;
	if (pih->hliveVideoParams.pCameraParams->fpH264 != NULL)
	{
		fclose(pih->hliveVideoParams.pCameraParams->fpH264);
		pih->hliveVideoParams.pCameraParams->fpH264 = NULL;
	}

	//H264_Release();
	//Sleep(1000);
}

/*��ֹ��ƵԴģ��*/
void FinializeVideoSource(void* hVS)
{
	internal_handle *pih = (internal_handle*)hVS;

	if (pih->hVideoChannel)
	{
		pih->hVideoChannel = NULL;
		Sleep(500);   //�ӳ�500,�ȴ����̹߳ҵ�
	}
	Sleep(500);   //�ӳ�500,�ȴ����̹߳ҵ�
	if (pih->pCtx != NULL)
	{
		sws_freeContext(pih->pCtx);
	}
	if (pih->pRGBBuffer != NULL)
	{
		delete[] pih->pRGBBuffer;
	}

	delete pih;
	pih = NULL;
}

void H264StreamCallback(void* pUserData, void* h264Buf, int nLen, int nType)
{

}

int main(int argc, char* argv[])
{
	// Load configuration from file
	el::Configurations conf("/path/to/my-conf.conf");
	// Reconfigure single logger
	el::Loggers::reconfigureLogger("default", conf);
	// Actually reconfigure all loggers instead
	el::Loggers::reconfigureAllLoggers(conf);

	el::Configurations defaultConf;
	defaultConf.setToDefault();
	// Values are always std::string
	defaultConf.set(el::Level::Info, el::ConfigurationType::Format, "%datetime %level %msg");
	// default logger uses default configurations
	el::Loggers::reconfigureLogger("default", defaultConf);
	LOG(INFO) << "Log using default file";
	// To set GLOBAL configurations you may use
	//defaultConf.setGlobally(el::ConfigurationType::Format, "%date %msg");
	defaultConf.setGlobally(el::ConfigurationType::ToFile, "true");
	defaultConf.setGlobally(el::ConfigurationType::ToStandardOutput, "true");
	el::Loggers::reconfigureLogger("default", defaultConf);

	// Now all the loggers will use configuration from file
	START_EASYLOGGINGPP(argc, argv);
	LOG(INFO) << "My first info log using default logger";
	VideoParam* pParam = new VideoParam;
	pParam->nRTPRecvPort = 6060;
	pParam->nServerPort = 5060;
	pParam->strRemoteName = "803";
	pParam->strRTPRecvIP = "39.106.13.209";
	pParam->strServerIP = "39.106.13.209";
	void* hHandle = InitialVideoSource(pParam, H264StreamCallback, NULL, NULL, NULL);
	return 0;
}