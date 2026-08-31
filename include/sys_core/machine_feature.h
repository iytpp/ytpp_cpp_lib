#ifndef _MACHINE_FEATURE_H_
#define _MACHINE_FEATURE_H_

#include <string>
#include "sys_core/sys_core.h"

namespace ytpp {
	namespace sys_core {


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