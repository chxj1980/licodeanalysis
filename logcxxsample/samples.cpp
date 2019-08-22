#include <iostream>
#include <log4cxx/logstring.h>
#include <log4cxx/logger.h>
#include <log4cxx/propertyconfigurator.h>
#include <log4cxx/helpers/exception.h>
#include <locale.h>
#include <log4cxx/basicconfigurator.h>

#pragma comment(lib, "log4cxx.lib")

using namespace log4cxx;
using namespace log4cxx::helpers;

int main() 
{
	//LoggerPtr logger(Logger::getLogger("LogTest1"));

	//BasicConfigurator::configure();
	//LOG4CXX_INFO(logger, "Hello ");
	//LOG4CXX_DEBUG(logger, "Log4Cxx");
	//printf("aaaaaaaaaaa");

	//����log4cxx�������ļ�������ʹ���������ļ�
	PropertyConfigurator::configure("log4cxx.properties");

	//���һ��Logger������ʹ����RootLogger
	LoggerPtr rootLogger = Logger::getRootLogger();

	//����INFO������������
	//LOG4CXX_INFO(rootLogger, _T("��־ϵͳ�����ˡ�������"));
	rootLogger->info("��־ϵͳ�����ˡ�������"); //�������Ǿ仰����һ��

	return 0;
}