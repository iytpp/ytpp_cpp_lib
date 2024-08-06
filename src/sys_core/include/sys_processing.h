#ifndef _SYS_CORE_SYS_PROCESSING_H_
#define _SYS_CORE_SYS_PROCESSING_H_

#include <windows.h>
#include <string>

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
	namespace sys_core {
		using namespace std;



		/*
		* @brief ���ù��λ��
		* @param [in] x: ˮƽλ��
		* @param [in] y: ��ֱλ��
		* @return (bool) �Ƿ����óɹ�
		*/
		BOOL set_cursorPosOrig(
			_In_ int x, 
			_In_ int y
		);

		/*
		* @brief ���ù��λ�ã�����������ѡ������д�����ΪNULL���޸Ķ�Ӧ�Ĳ��
		* @brief ��������ǰ���ȡԭ����λ�ã��������ܷ��������ģ������ϣ���Զ���ȡԭλ�ã���ʹ��set_cursorPosOrig��ֱ�ӵ���ϵͳAPI��SetCursorPos
		* @param [in] x: ˮƽλ��
		* @param [in] y: ��ֱλ��
		* @return (bool) �Ƿ����óɹ�
		*/
		BOOL set_cursorPos(
			_In_ int x, 
			_In_ int y
		);

		/*
		* @brief ��ȡ���λ��
		* @param [out] x: ˮƽλ��
		* @param [out] y: ��ֱλ��
		* @return (bool) �Ƿ��ȡ�ɹ�
		*/
		BOOL get_cursorPos(
			_Out_ LONG* x, 
			_Out_ LONG* y
		);

		/*
		* @brief ��ȡ���Ĵ�ֱλ��
		* @return (int) ��괹ֱλ�ã������ȡʧ���򷵻� -1
		*/
		LONG get_cursorPosY();

		/*
		* @brief ��ȡ����ˮƽλ��
		* @return (int) ���ˮƽλ�ã������ȡʧ���򷵻� -1
		*/
		LONG get_cursorPosX();

		/*
		* @brief ��ȡ��Ļ���
		* @return (int) ��Ļ���
		*/
		int get_screenWidth();

		/*
		* @brief ��ȡ��Ļ�߶�
		* @return (int) ��Ļ�߶�
		*/
		int get_screenHeight();

		/* 2024��7��26�ա� */

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


	} /* namespace sys_core */
} /* namespace ytpp */





#endif /* _SYS_CORE_SYS_PROCESSING_H_ */