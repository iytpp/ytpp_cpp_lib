#include "sys_core/machine_feature.h"
#include "sys_core/sys_core.h"

#include <sstream>
#include <iostream>
#include <string>
#include <Windows.h>
#include <wbemidl.h>
#include <comdef.h>


namespace ytpp {
	namespace sys_core {

		/// <summary>
		/// 用wmi进行查询
		/// </summary>
		/// <param name="wql"></param>
		/// <param name="field"></param>
		/// <returns></returns>
		std::string wmi_query(const std::string& wql, const std::string& field) {
			HRESULT hr;
			IWbemLocator* pLoc = nullptr;
			IWbemServices* pSvc = nullptr;
			IEnumWbemClassObject* pEnumerator = nullptr;
			IWbemClassObject* pclsObj = nullptr;
			ULONG uReturn = 0;
			std::ostringstream result;

			hr = CoInitializeEx(0, COINIT_MULTITHREADED);
			if (FAILED(hr)) return "";

			hr = CoInitializeSecurity(nullptr, -1, nullptr, nullptr,
				RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE,
				nullptr, EOAC_NONE, nullptr);
			if (FAILED(hr)) return "";

			hr = CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER,
				IID_IWbemLocator, (LPVOID*)&pLoc);
			if (FAILED(hr)) return "";

			hr = pLoc->ConnectServer(
				BSTR(L"ROOT\\CIMV2"), nullptr, nullptr, 0, 0, 0, 0, &pSvc);
			if (FAILED(hr)) return "";

			hr = CoSetProxyBlanket(
				pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
				RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);
			if (FAILED(hr)) return "";

			hr = pSvc->ExecQuery(
				BSTR(L"WQL"), BSTR(std::wstring(wql.begin(), wql.end()).c_str()),
				WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &pEnumerator);
			if (FAILED(hr)) return "";

			while (pEnumerator) {
				hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
				if (0 == uReturn) break;

				VARIANT vtProp;
				hr = pclsObj->Get(BSTR(std::wstring(field.begin(), field.end()).c_str()), 0, &vtProp, 0, 0);
				if (SUCCEEDED(hr) && (vtProp.vt == VT_BSTR)) {
					result << _bstr_t(vtProp.bstrVal);
				}
				VariantClear(&vtProp);
				pclsObj->Release();
			}

			pSvc->Release();
			pLoc->Release();
			pEnumerator->Release();
			CoUninitialize();
			return result.str();
		}

		/// <summary>
		/// 获取机器特征
		/// </summary>
		/// <returns></returns>
		std::string get_machine_features(bool cup, bool baseBoard, bool diskDrive, bool gpu, bool physicalMemory, bool mac) {
			std::ostringstream features;
			if(cup)features << "CPU:" << wmi_query("SELECT ProcessorId FROM Win32_Processor", "ProcessorId");
			if (baseBoard)features << "\n主板:" << wmi_query("SELECT SerialNumber FROM Win32_BaseBoard", "SerialNumber");
			if (diskDrive)features << "\n硬盘:" << wmi_query("SELECT SerialNumber FROM Win32_DiskDrive", "SerialNumber");
			if (gpu)features << "\nGPU:" << wmi_query("SELECT PNPDeviceID FROM Win32_VideoController", "PNPDeviceID");
			if (physicalMemory)features << "\nRAM:" << wmi_query("SELECT Capacity FROM Win32_PhysicalMemory", "Capacity");
			if (mac)features << "\n网络:" << wmi_query("SELECT MACAddress FROM Win32_NetworkAdapter WHERE MACAddress IS NOT NULL", "MACAddress");
			return features.str();
		}



		/// <summary>
		/// 获取机器码
		/// </summary>
		/// <returns></returns>
		std::string get_machineCode(std::string signature, bool cup, bool baseBoard, bool diskDrive, bool gpu, bool physicalMemory, bool mac, HashType hashType)
		{
			std::string machine_features = get_machine_features(cup, baseBoard, diskDrive, gpu, physicalMemory, mac);
			if (!signature.empty()) {
				machine_features += signature;
			}
			std::string machine_code = str_toupper(get_hash(machine_features, hashType));
			//把machine_code按照5个为一组进行分割，每组之间用-连接
			//for (int i = 5; i < machine_code.length(); i += 6) {
			//	machine_code.insert(i, "-");
			//}
			return machine_code;
		}



	}
}
