#include "pjsua_app.h"


#ifdef USELOG
#include "log/easylogging++.h"
INITIALIZE_EASYLOGGINGPP
#endif

//extern struct pjmedia_vid_codec_mgr;
//�Ựid
int acc_id;
int call_id;
int recv_index = 0;

#define CLIENT_RTP_PORT 6060
#define PROXY_RTP_PORT  6070

#if defined (_WIN32) && defined (_DEBUG) && defined(_X64)
#pragma comment(lib, "libpjproject-x86_64-x64-vc14-Debug.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "avcodec.lib")
#else
#pragma comment(lib, "libpjproject-i386-Win32-vc14-Debug.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "avcodec.lib")
#endif

void  OnRecvMsg(char* remote, char* msg, int len)
{
	//string szMsg = msg;
	//string temp = szMsg.substr(0, 4);
	//switch (m_MsgState)
	//{
	//case 0://�յ�����Ϣ,��Ҫ�ظ�
	//{
	//	if (szMsg.substr(0, 5) == "PLAY0")
	//	{
	//		StartRtsp(0);
	//		return;
	//	}

	//	if (szMsg.substr(0, 5) == "PLAY1")
	//	{
	//		StartRtsp(1);
	//		return;
	//	}

	//	if (szMsg.substr(0, 5) == "PLAY2")
	//	{
	//		StartRtsp(2);
	//		return;
	//	}

	//	if (szMsg.substr(0, 5) == "PLAY3")
	//	{
	//		StartRtsp(3);
	//		return;
	//	}

	//	if (szMsg.substr(0, 4) == "LIST")
	//	{
	//		if (szMsg.find("sip:hzjy.zjjy.cnjy@202.101.100.1") != string::npos)
	//		{
	//			//����ѧУ�б�
	//			string content = "KSLP/1.0 200 OK\r\n \
	//								  \r\n \
	//								  sip:yz.hzjy.zjjy.cnjy@202.101.101.1 һ��DO ON \r\n ";

	//			send_message(call_id, acc_id, remote, m_szServer.GetBuffer(256), content.c_str());

	//			return;
	//		}

	//		if (szMsg.find("sip:yz.hzjy.zjjy.cnjy@202.101.101.1") != string::npos)
	//		{
	//			//����һ��������б�
	//			string content = "KSLP/1.0 200 OK\r\n \
	//								  \r\n \
	//								  sip:yu00.yz.hzjy.zjjy.cnjy@202.101.102.1һ����EU ON|READY|AREC|VLOSS\r\n \
	//								  sip:yu01.yz.hzjy.zjjy.cnjy@202.101.102.1������EU ON|READY|AREC\r\n \
	//								  sip:yu02.yz.hzjy.zjjy.cnjy@202.101.102.1������EU ON|READY|AREC\r\n \
	//								  sip:yu03.yz.hzjy.zjjy.cnjy@202.101.102.1�Ŀ���EU ON|READY|AREC\r\n ";
	//			send_message(call_id, acc_id, remote, m_szServer.GetBuffer(256), content.c_str());

	//			return;
	//		}
	//	}
	//}
	//break;

	//case 1://�յ�ѧУ�б�ظ���Ϣ,�����ٻ���
	//{
	//	if (szMsg.find("KSLP/1.0 200 OK") != string::npos)
	//	{
	//		m_Tree.DeleteAllItems();
	//		HTREEITEM hRoot = m_Tree.InsertItem("����");

	//		evbuffer* pTemp = evbuffer_new();
	//		evbuffer_add(pTemp, msg, len);
	//		while (1)
	//		{
	//			char* line = evbuffer_readline(pTemp);
	//			if (line == NULL)
	//				break;
	//			string szLine = line;

	//			int nPos = szLine.find("sip:");
	//			if (nPos != string::npos)
	//			{
	//				szLine = szLine.substr(nPos);
	//				m_szScoolName = szLine.c_str();
	//				m_Tree.InsertItem(m_szScoolName, hRoot);
	//			}
	//		}

	//		evbuffer_free(pTemp);

	//		m_MsgState = 0;

	//		m_Tree.Expand(hRoot, TVE_EXPAND);
	//		return;
	//	}
	//}
	//break;

	//case 2:
	//{
	//	if (szMsg.find("KSLP/1.0 200 OK") != string::npos)
	//	{
	//		m_Tree.DeleteAllItems();
	//		HTREEITEM hRoot = m_Tree.InsertItem("����");
	//		HTREEITEM hSubRoot = m_Tree.InsertItem(m_szScoolName, hRoot);
	//		evbuffer* pTemp = evbuffer_new();
	//		evbuffer_add(pTemp, msg, len);

	//		int index = 0;
	//		while (1)
	//		{
	//			char* line = evbuffer_readline(pTemp);
	//			if (line == NULL)
	//				break;
	//			string szLine = line;

	//			int nPos = szLine.find("sip:");
	//			if (nPos != string::npos)
	//			{
	//				szLine = szLine.substr(nPos);
	//				m_szScoolName = szLine.c_str();
	//				HTREEITEM hItem = m_Tree.InsertItem(m_szScoolName, hSubRoot);
	//				m_Tree.SetItemData(hItem, index++);
	//			}
	//		}

	//		evbuffer_free(pTemp);
	//		m_MsgState = 0;

	//		m_Tree.Expand(hRoot, TVE_EXPAND);
	//		m_Tree.Expand(hSubRoot, TVE_EXPAND);
	//		return;
	//	}
	//}
	//break;
	//}
}

void UpdateStatus(int nEvent, int callid, char* local, char* remote)
{
	switch (nEvent)
	{
		case PJSUA_STATUS_CALLING:
		{
			LOG(INFO) << "PJSIP_INV_STATE_CALLING callid =" << callid << ",local=" << local << ",remote=" << remote;
		}
		break;
		case PJSUA_STATUS_DISCONNECTED:
		{
			LOG(INFO) << "PJSUA_STATUS_DISCONNECTED callid=" << callid << ",local=" << local << ",remote=" << remote;
			LOG(INFO) << "���ӶϿ�";
		}
		break;

	case PJSUA_STATUS_CONFIRMED:
	{
		LOG(INFO) << "PJSUA_STATUS_DISCONNECTED callid=" << callid << ",local=" << local << ",remote=" << remote;
		call_id = callid;
		//if (m_nOp == 1)
		//{
			//�������õ��Է�Ӧ�𣬿�ʼ����RTP SOCKET
			init_local_rtp();
		//}
		//else if (m_nOp == 2)
		//{
		//	//�յ����룬��������RTP
		//	init_local_rtp();
		//}
		LOG(INFO) << "���ӽ����ɹ�!\r\n";
	}
	break;
	case PJSUA_STATUS_INCOMING_CALL:
	{
		LOG(INFO) << "PJSUA_STATUS_INCOMING_CALL callid=" << callid << ",local=" << local << ",remote=" << remote;
		call_id = callid;

		LOG(INFO) << "�յ�����: callid=" << callid << ", local = " << local << ", remote = " << remote;
		//�Զ�Ӧ��,�����������
		//OnBnClickedButtonAnswer();
		//OnBnClickedButtonRtspStart();

		set_local_rtpport(PROXY_RTP_PORT);
		//m_nOp = 2;

		sip_answer(call_id, NULL);
		//m_bSendRtp = true;
	}
	break;
	case PJSUA_STATUS_RECVMSG:
		break;
	}
}

static void LogCB(int level, const char *data, int len, void* pParam)
{
	if (data == NULL)
		return;
	LOG(INFO) << data;
}


void UpdateListStatus(int accid, bool status)
{
	if (status)
	{
		acc_id = accid;
		LOG(INFO) << "�û���" << acc_id << "��¼�ɹ�";
	}
	else
	{
		LOG(INFO) << "�û���" << "**" << "��¼ʧ��";
	}
}

static void RecvMsgCB(int accid, char* remote, char* msg, int len, void* pParam)
{
	OnRecvMsg(remote, msg, len);
}

static void StatusCB(int status, pjsua_call_info *data, void* pParam)
{
	UpdateStatus(status, data->id, data->local_info.ptr, data->remote_info.ptr);
}

static void RegisterStatusCB(int accid, bool bRet, void* pParam)
{
	UpdateListStatus(accid, bRet);
}

void rtpDataCB(uint8_t* buff, int len)
{
	//int ret = m_pDecode->decode_rtsp_frame(buff, len, false);
	int ret = 0;
	LOG(INFO) << "decode = " << ret << "seq =" << recv_index++ << "len =" << len;
}

static void PR_RTP_DataCB(const char *data, int len, int ip, int port, void* pParam)
{
	rtpDataCB((uint8_t*)data, len);
}

static void on_rx_rtcp(void *user_data, void *pkt, pj_ssize_t size)
{
	//struct media_stream *strm;

	//strm = (media_stream*)user_data;

	///* Discard packet if media is inactive */
	//if (!strm->active)
	//	return;

	/* Check for errors */
	if (size < 0) {
		//app_perror(THIS_FILE, "Error receiving RTCP packet", (pj_status_t)-size);
		LOG(INFO) << "Error receiving RTCP packet" << (pj_status_t)-size;
		return;
	}

	///* Update RTCP session */
	//pjmedia_rtcp_rx_rtcp(&strm->rtcp, pkt, size);
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

	set_msg_callback(RecvMsgCB, NULL);
	set_callback(StatusCB, NULL);
	set_register_callback(RegisterStatusCB, NULL);
	set_log_callback(LogCB, NULL);
	app_init();

	Sleep(1000);
	pj_status_t status = sip_register("803", "803", "39.106.13.209", acc_id);
	if (status != PJ_SUCCESS)
	{
		LOG(ERROR) << "user reg fail!";
		return -1;
	}
	Sleep(1000);
	set_local_rtpport(CLIENT_RTP_PORT);
	set_data_callback(PR_RTP_DataCB, NULL);
	sip_call(acc_id, "802", "39.106.13.209", NULL);

	Sleep(4000);
	
	getchar();
}