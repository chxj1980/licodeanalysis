// stdafx.h : ��׼ϵͳ�����ļ��İ����ļ���
// ���Ǿ���ʹ�õ��������ĵ�
// �ض�����Ŀ�İ����ļ�
//

#pragma once

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN             //  �� Windows ͷ�ļ����ų�����ʹ�õ���Ϣ
// Windows ͷ�ļ�: 
#include <windows.h>

/*
* ��libjpeg-turboΪvs2010����ʱ��vs2015�¾�̬����libjpeg-turbo�����ӳ���:�Ҳ���__iob_func,
* ����__iob_func��__acrt_iob_func��ת���������������,
* ��libjpeg-turbo��vs2015����ʱ������Ҫ�˲����ļ�
*/
//#if _MSC_VER>=1900
//#include "stdio.h" 
//_ACRTIMP_ALT FILE* __cdecl __acrt_iob_func(unsigned);
//#ifdef __cplusplus 
//extern "C"
//#endif 
//FILE* __cdecl __iob_func(unsigned i) {
//	return __acrt_iob_func(i);
//}
//#endif /* _MSC_VER>=1900 */

#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS      // ĳЩ CString ���캯��������ʽ��

#include <atlbase.h>
#include <atlstr.h>

// TODO:  �ڴ˴����ó�����Ҫ������ͷ�ļ�
#include <crtdbg.h>
