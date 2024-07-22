#ifndef _SYS_CORE_SYS_PROCESSING_H_
#define _SYS_CORE_SYS_PROCESSING_H_

#include <windows.h>
#include <string>
#include "compatibility.h"

#ifdef _UNICODE

#define write_profile write_profileW
#define read_profile read_profileW
#define write_struct write_structW
#define read_struct read_structW

#else /* ANSI */

#define write_profile write_profileA
#define read_profile read_profileA
#define write_struct write_structA
#define read_struct read_structA

#endif /* _UNICODE */

namespace ytpp {
	using namespace std;

	/*
	* @brief д�����֧��ANSI��UTF8
	* @param [in] fileName: �ļ���
	* @param [in] section: ���ý����ƣ���������
	* @param [in] key�� ���������ƣ������ƣ�
	* @param [in] value�� ��д���ֵ
	* @return (bool) �Ƿ�д��ɹ�
	*/
	bool write_profileA(
		_In_ const string& fileName,
		_In_ const string& section,
		_In_ const string& key,
		_In_ const string& value 
	);
	/*
	* @brief д�����д������ANSI���룬��Ȼ��W�汾������д�����ļ�ȴ��ANSI���루Windowsϵͳ�����⣩������UNICODE֧�֣�����A�汾��ʹ��UTF8����
	* @param [in] fileName: �ļ���
	* @param [in] section: ���ý����ƣ���������
	* @param [in] key�� ���������ƣ������ƣ�
	* @param [in] value�� ��д���ֵ
	* @return (bool) �Ƿ�д��ɹ�
	*/
	bool write_profileW(
		_In_ const wstring& fileName,
		_In_ const wstring& section,
		_In_ const wstring& key,
		_In_ const wstring& value 
	);


	/*
	* @brief �������֧��ANSI��UTF8
	* @param [in] fileName: �ļ���
	* @param [in] section: ���ý����ƣ���������
	* @param [in] key�� ���������ƣ������ƣ�
	* @param [in] defaultValue�� Ĭ��ֵ�������������ʱ�����ش�ֵ
	* @param [in] defaultBufferSize: Ĭ�ϻ�������С��Ĭ��Ϊ256��ÿ�οռ䲻����ʱ����Զ�����256���ֽڡ�ʹ������ҪԤ����ֵ�ĳ��ȣ���ֹ���ݴ���������ɵ�ʱ�俪����
	* @return: (string)[ANSI] �������ֵ�����ʧ���򷵻ؿ��ַ���
	*/
	string read_profileA(
		_In_ const string& fileName,
		_In_ const string& section,
		_In_ const string& key,
		_In_ const string& defaultValue,
		_In_ DWORD defaultBufferSize = 256 
	);

	/*
	* @brief �������W�汾������� ANSI �� UTF-16 ������ļ������Ϊ UTF-16 ���룬��ʹ�� encoding_ ǰ׺�ı���ת������ת��Ϊ ANSI �� UTF8 ����
	* @param [in] fileName: �ļ���
	* @param [in] section: ���ý����ƣ���������
	* @param [in] key�� ���������ƣ������ƣ�
	* @param [in] defaultValue�� Ĭ��ֵ�������������ʱ�����ش�ֵ
	* @param [in] defaultBufferSize: Ĭ�ϻ�������С��Ĭ��Ϊ256��ÿ�οռ䲻����ʱ����Զ�����256���ֽڡ�ʹ������ҪԤ����ֵ�ĳ��ȣ���ֹ���ݴ���������ɵ�ʱ�俪����
	* @return: (string) �������ֵ�����ʧ���򷵻ؿ��ַ���
	*/
	wstring read_profileW(
		_In_ const wstring& fileName,
		_In_ const wstring& section,
		_In_ const wstring& key,
		_In_ const wstring& defaultValue,
		_In_ DWORD defaultBufferSize = 256 
	);


	/*
	* @brief д���ṹ������
	* @param [in] fileName: �ļ���
	* @param [in] section: ���ý����ƣ���������
	* @param [in] key�� ���������ƣ������ƣ�
	* @param [in] lpStruct: ��д��Ľṹ��ָ��
	* @param [in] uSizeStruct: �ṹ���С
	* @return: (bool) �Ƿ�д��ɹ�
	*/
	bool write_structA(
		_In_ const string& fileName,
		_In_ const string& section,
		_In_ const string& key,
		_In_ void* lpStruct,
		_In_ UINT uSizeStruct
	);

	/*
	* @brief д���ṹ������
	* @param [in] fileName: �ļ���
	* @param [in] section: ���ý����ƣ���������
	* @param [in] key�� ���������ƣ������ƣ�
	* @param [in] lpStruct: ��д��Ľṹ��ָ��
	* @param [in] uSizeStruct: �ṹ���С
	* @return: (bool) �Ƿ�д��ɹ�
	*/
	bool write_structW(
		_In_ const wstring& fileName,
		_In_ const wstring& section,
		_In_ const wstring& key,
		_In_ void* lpStruct,
		_In_ UINT uSizeStruct
	);

	/*
	* @brief ����ṹ������
	* @param [in] fileName: �ļ���
	* @param [in] section: ���ý����ƣ���������
	* @param [in] key�� ���������ƣ������ƣ�
	* @param [out] lpStruct: ������Ľṹ��ָ��
	* @param [in] uSizeStruct: �ṹ���С
	* @return: (bool) �Ƿ��ȡ�ɹ�
	*/
	bool read_structA(
		_In_ const string& fileName,
		_In_ const string& section,
		_In_ const string& key,
		_Out_ void* lpStruct,
		_In_ UINT uSizeStruct
	);

	/*
	* @brief ����ṹ������
	* @param [in] fileName: �ļ���
	* @param [in] section: ���ý����ƣ���������
	* @param [in] key�� ���������ƣ������ƣ�
	* @param [out] lpStruct: ������Ľṹ��ָ��
	* @param [in] uSizeStruct: �ṹ���С
	* @return: (bool) �Ƿ��ȡ�ɹ�
	*/
	bool read_structW(
		_In_ const wstring& fileName,
		_In_ const wstring& section,
		_In_ const wstring& key,
		_Out_ void* lpStruct,
		_In_ UINT uSizeStruct
	);

}





#endif /* _SYS_CORE_SYS_PROCESSING_H_ */