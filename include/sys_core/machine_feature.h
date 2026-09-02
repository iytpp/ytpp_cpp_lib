#ifndef _MACHINE_FEATURE_H_
#define _MACHINE_FEATURE_H_

#include <string>
#include "sys_core/sys_core.h"

namespace ytpp {
	namespace sys_core {

		struct SystemAboutInfo
		{
			// 设备规格
			std::string deviceName;      // 设备名称
			std::string processor;       // 处理器
			std::string installedRam;    // 机带 RAM
			std::string deviceId;        // 设备 ID
			std::string productId;       // 产品 ID
			std::string systemType;      // 系统类型
			std::string penAndTouch;     // 笔和触控

			// Windows 规格
			std::string edition;         // 版本，例如 Windows 11 专业版
			std::string version;         // 版本号，例如 24H2
			std::string installDate;     // 安装日期
			std::string osBuild;         // 操作系统版本，例如 26100.4946
			std::string experience;      // 功能包

			// 环境判断
			bool isVirtualMachine = false; // 多维度综合判断是否运行于虚拟机
		};

		// 查询系统关于信息
		SystemAboutInfo GetSystemAboutInfo();


		/// <summary>
		/// 用wmi进行查询
		/// </summary>
		/// <param name="wql"></param>
		/// <param name="field"></param>
		/// <returns></returns>
		std::string wmi_query(const std::string& wql, const std::string& field);

		/// <summary>
		/// 获取机器码
		/// </summary>
		/// <returns></returns>
		std::string get_machineCode(
			std::string signature = "",
			bool cup = true, 
			bool baseBoard = true, 
			bool diskDrive = true, 
			bool gpu = true, 
			bool physicalMemory = true, 
			bool mac = true,
			HashType hashType = HashType::SHA256
		);

		/// <summary>
		/// 获取机器特征
		/// </summary>
		/// <returns></returns>
		std::string get_machine_features(bool cup, bool baseBoard, bool diskDrive, bool gpu, bool physicalMemory, bool mac);


		/// <summary>
		/// 获取用户名
		/// </summary>
		/// <returns></returns>
		std::string get_usernameA();


		/// <summary>
		/// 获取用户名
		/// </summary>
		/// <returns></returns>
		std::wstring get_usernameW();
	
	
	}
}










#endif /* _MACHINE_FEATURE_H_ */