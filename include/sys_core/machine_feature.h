#ifndef _MACHINE_FEATURE_H_
#define _MACHINE_FEATURE_H_

#include <string>
#include "sys_core/sys_core.h"

namespace ytpp {
	namespace sys_core {

		/// <summary>
		/// »ñÈ¡»úÆ÷Âë
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
	
	
	}
}










#endif /* _MACHINE_FEATURE_H_ */