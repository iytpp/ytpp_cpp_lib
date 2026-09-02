#define WIN32_LEAN_AND_MEAN
#define NOMINMAX


#include "sys_core/machine_feature.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <Windows.h>
#include <iphlpapi.h>

#include <intrin.h>


#include <algorithm>
#include <array>
#include <cctype>
#include <ctime>
#include <cwctype>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>
#include <iostream>
#include <wbemidl.h>
#include <comdef.h>


namespace ytpp {
	namespace sys_core {

		namespace
		{
			std::string WideToUtf8(const std::wstring& text)
			{
				if (text.empty())
					return {};

				const int size = WideCharToMultiByte(
					CP_UTF8,
					0,
					text.data(),
					static_cast<int>(text.size()),
					nullptr,
					0,
					nullptr,
					nullptr
				);

				if (size <= 0)
					return {};

				std::string result(static_cast<std::size_t>(size), '\0');

				WideCharToMultiByte(
					CP_UTF8,
					0,
					text.data(),
					static_cast<int>(text.size()),
					result.data(),
					size,
					nullptr,
					nullptr
				);

				return result;
			}

			std::wstring ReadRegistryString(
				HKEY root,
				const wchar_t* subKey,
				const wchar_t* valueName)
			{
				DWORD type = 0;
				DWORD size = 0;

				const LONG queryResult = RegGetValueW(
					root,
					subKey,
					valueName,
					RRF_RT_REG_SZ,
					&type,
					nullptr,
					&size
				);

				if (queryResult != ERROR_SUCCESS || size == 0)
					return {};

				std::wstring value(
					static_cast<std::size_t>(size / sizeof(wchar_t)),
					L'\0'
				);

				const LONG readResult = RegGetValueW(
					root,
					subKey,
					valueName,
					RRF_RT_REG_SZ,
					&type,
					value.data(),
					&size
				);

				if (readResult != ERROR_SUCCESS)
					return {};

				while (!value.empty() && value.back() == L'\0')
					value.pop_back();

				return value;
			}

			DWORD ReadRegistryDWORD(
				HKEY root,
				const wchar_t* subKey,
				const wchar_t* valueName)
			{
				DWORD value = 0;
				DWORD size = sizeof(value);

				const LONG result = RegGetValueW(
					root,
					subKey,
					valueName,
					RRF_RT_REG_DWORD,
					nullptr,
					&value,
					&size
				);

				if (result != ERROR_SUCCESS)
					return 0;

				return value;
			}

			bool RegistryKeyExists(
				HKEY root,
				const wchar_t* subKey)
			{
				HKEY key = nullptr;

				const LONG result = RegOpenKeyExW(
					root,
					subKey,
					0,
					KEY_READ,
					&key
				);

				if (result != ERROR_SUCCESS)
					return false;

				RegCloseKey(key);
				return true;
			}

			std::wstring ToLower(std::wstring text)
			{
				std::transform(
					text.begin(),
					text.end(),
					text.begin(),
					[](wchar_t ch)
				{
					return static_cast<wchar_t>(std::towlower(ch));
				}
				);

				return text;
			}

			bool ContainsAny(
				const std::wstring& value,
				const std::vector<std::wstring>& needles)
			{
				if (value.empty())
					return false;

				const std::wstring lower = ToLower(value);

				for (const auto& needle : needles)
				{
					if (lower.find(ToLower(needle)) != std::wstring::npos)
						return true;
				}

				return false;
			}

			std::string GetDeviceName()
			{
				wchar_t buffer[256]{};
				DWORD size = static_cast<DWORD>(std::size(buffer));

				if (!GetComputerNameExW(
					ComputerNamePhysicalDnsHostname,
					buffer,
					&size))
				{
					return {};
				}

				return WideToUtf8(std::wstring(buffer, size));
			}

			std::string GetProcessorName()
			{
				return WideToUtf8(
					ReadRegistryString(
					HKEY_LOCAL_MACHINE,
					L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
					L"ProcessorNameString"
				)
				);
			}

			std::string GetInstalledRam()
			{
				MEMORYSTATUSEX memoryStatus{};
				memoryStatus.dwLength = sizeof(memoryStatus);

				if (!GlobalMemoryStatusEx(&memoryStatus))
					return {};

				const double gb =
					static_cast<double>(memoryStatus.ullTotalPhys) /
					1024.0 /
					1024.0 /
					1024.0;

				std::ostringstream stream;
				stream << std::fixed << std::setprecision(2) << gb << " GB";

				return stream.str();
			}

			std::string GetSystemType()
			{
				SYSTEM_INFO systemInfo{};
				GetNativeSystemInfo(&systemInfo);

				switch (systemInfo.wProcessorArchitecture)
				{
				case PROCESSOR_ARCHITECTURE_AMD64:
					return "64 位操作系统，基于 x64 的处理器";

				case PROCESSOR_ARCHITECTURE_ARM64:
					return "64 位操作系统，基于 ARM64 的处理器";

				case PROCESSOR_ARCHITECTURE_INTEL:
					return "32 位操作系统，基于 x86 的处理器";

				default:
					return "未知";
				}
			}

			std::string GetPenAndTouch()
			{
				const int digitizer = GetSystemMetrics(SM_DIGITIZER);

				if (digitizer == 0)
					return "没有可用于此显示器的笔或触控输入";

				const bool pen =
					(digitizer & NID_INTEGRATED_PEN) != 0 ||
					(digitizer & NID_EXTERNAL_PEN) != 0;

				const bool touch =
					(digitizer & NID_INTEGRATED_TOUCH) != 0 ||
					(digitizer & NID_EXTERNAL_TOUCH) != 0;

				if (pen && touch)
					return "支持笔和触控输入";

				if (pen)
					return "支持笔输入";

				if (touch)
					return "支持触控输入";

				return "检测到数字化设备";
			}

			std::string FormatInstallDate(DWORD unixTimestamp)
			{
				if (unixTimestamp == 0)
					return {};

				const std::time_t timestamp =
					static_cast<std::time_t>(unixTimestamp);

				std::tm localTime{};

				if (localtime_s(&localTime, &timestamp) != 0)
					return {};

				std::ostringstream stream;
				stream << std::put_time(&localTime, "%Y-%m-%d");

				return stream.str();
			}

			bool CpuReportsHypervisor()
			{
#if defined(_M_IX86) || defined(_M_X64)
				int regs[4]{};
				__cpuid(regs, 1);

				// CPUID.01H:ECX[31] == 1 表示存在 hypervisor。
				// 注意：物理机启用 Hyper-V/VBS 时同样可能为 1，
				// 因此这里只作为弱特征使用。
				return (static_cast<unsigned int>(regs[2]) & (1u << 31)) != 0;
#else
				return false;
#endif
			}

			bool HasVirtualMachineSmbiosSignature()
			{
				constexpr const wchar_t* BiosKey =
					L"HARDWARE\\DESCRIPTION\\System\\BIOS";

				const std::wstring systemManufacturer =
					ReadRegistryString(
					HKEY_LOCAL_MACHINE,
					BiosKey,
					L"SystemManufacturer"
					);

				const std::wstring systemProductName =
					ReadRegistryString(
					HKEY_LOCAL_MACHINE,
					BiosKey,
					L"SystemProductName"
					);

				const std::wstring biosVendor =
					ReadRegistryString(
					HKEY_LOCAL_MACHINE,
					BiosKey,
					L"BIOSVendor"
					);

				const std::wstring biosVersion =
					ReadRegistryString(
					HKEY_LOCAL_MACHINE,
					BiosKey,
					L"BIOSVersion"
					);

				const std::vector<std::wstring> strongVmMarkers =
				{
					L"vmware",
					L"virtualbox",
					L"virtual machine",
					L"virtual pc",
					L"kvm",
					L"qemu",
					L"xen",
					L"hvm domu",
					L"parallels",
					L"bochs",
					L"bhyve"
				};

				if (ContainsAny(systemManufacturer, strongVmMarkers))
					return true;

				if (ContainsAny(systemProductName, strongVmMarkers))
					return true;

				if (ContainsAny(biosVendor, strongVmMarkers))
					return true;

				if (ContainsAny(biosVersion, strongVmMarkers))
					return true;

				// Microsoft Corporation 本身不能单独作为虚拟机特征，
				// Surface 等真实设备也可能使用 Microsoft 作为制造商。
				// 只有产品名明确为 Virtual Machine 时才计入。
				const std::wstring manufacturerLower =
					ToLower(systemManufacturer);

				const std::wstring productLower =
					ToLower(systemProductName);

				if (manufacturerLower.find(L"microsoft") != std::wstring::npos &&
					productLower.find(L"virtual machine") != std::wstring::npos)
				{
					return true;
				}

				return false;
			}

			bool HasVirtualMachineGuestDrivers()
			{
				// 这里只检查较有代表性的 Guest Additions / Tools 驱动，
				// 不检查 Hyper-V vmic*，避免物理宿主机启用 Hyper-V 时误判。
				constexpr std::array<const wchar_t*, 13> GuestServiceKeys =
				{
					L"SYSTEM\\CurrentControlSet\\Services\\VBoxGuest",
					L"SYSTEM\\CurrentControlSet\\Services\\VBoxMouse",
					L"SYSTEM\\CurrentControlSet\\Services\\VBoxSF",
					L"SYSTEM\\CurrentControlSet\\Services\\VBoxVideo",

					L"SYSTEM\\CurrentControlSet\\Services\\vmhgfs",
					L"SYSTEM\\CurrentControlSet\\Services\\vmmouse",
					L"SYSTEM\\CurrentControlSet\\Services\\vm3dmp",
					L"SYSTEM\\CurrentControlSet\\Services\\vmrawdsk",
					L"SYSTEM\\CurrentControlSet\\Services\\VMTools",

					L"SYSTEM\\CurrentControlSet\\Services\\xenbus",
					L"SYSTEM\\CurrentControlSet\\Services\\xenvbd",

					L"SYSTEM\\CurrentControlSet\\Services\\qemufwcfg",
					L"SYSTEM\\CurrentControlSet\\Services\\qemu-ga"
				};

				for (const wchar_t* key : GuestServiceKeys)
				{
					if (RegistryKeyExists(HKEY_LOCAL_MACHINE, key))
						return true;
				}

				return false;
			}

			bool IsKnownVirtualMacPrefix(
				const unsigned char* mac,
				ULONG length)
			{
				if (mac == nullptr || length < 3)
					return false;

				struct Oui
				{
					unsigned char a;
					unsigned char b;
					unsigned char c;
				};

				// 常见虚拟化平台使用的 OUI。
				constexpr std::array<Oui, 8> VirtualOuis =
				{ {
					{0x00, 0x05, 0x69}, // VMware
					{0x00, 0x0C, 0x29}, // VMware
					{0x00, 0x1C, 0x14}, // VMware
					{0x00, 0x50, 0x56}, // VMware
					{0x08, 0x00, 0x27}, // VirtualBox
					{0x00, 0x15, 0x5D}, // Hyper-V
					{0x00, 0x16, 0x3E}, // Xen
					{0x52, 0x54, 0x00}  // QEMU/KVM
				} };

				for (const auto& oui : VirtualOuis)
				{
					if (mac[0] == oui.a &&
						mac[1] == oui.b &&
						mac[2] == oui.c)
					{
						return true;
					}
				}

				return false;
			}

			bool HasVirtualMachineMacAddress()
			{
				ULONG bufferSize = 16 * 1024;
				std::vector<unsigned char> buffer(bufferSize);

				auto* adapters =
					reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());

				ULONG result = GetAdaptersAddresses(
					AF_UNSPEC,
					GAA_FLAG_SKIP_ANYCAST |
					GAA_FLAG_SKIP_MULTICAST |
					GAA_FLAG_SKIP_DNS_SERVER,
					nullptr,
					adapters,
					&bufferSize
				);

				if (result == ERROR_BUFFER_OVERFLOW)
				{
					buffer.resize(bufferSize);

					adapters =
						reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());

					result = GetAdaptersAddresses(
						AF_UNSPEC,
						GAA_FLAG_SKIP_ANYCAST |
						GAA_FLAG_SKIP_MULTICAST |
						GAA_FLAG_SKIP_DNS_SERVER,
						nullptr,
						adapters,
						&bufferSize
					);
				}

				if (result != NO_ERROR)
					return false;

				for (auto* adapter = adapters;
					adapter != nullptr;
					adapter = adapter->Next)
				{
					if (adapter->PhysicalAddressLength < 3)
						continue;

					if (IsKnownVirtualMacPrefix(
						adapter->PhysicalAddress,
						adapter->PhysicalAddressLength))
					{
						return true;
					}
				}

				return false;
			}

			bool DetectVirtualMachine()
			{
				// 多维度评分：
				//
				// 1. SMBIOS / 系统厂商与产品名：强特征        +4
				// 2. Guest Additions / Tools 驱动：强特征      +3
				// 3. 虚拟网卡 OUI：中等特征                   +2
				// 4. CPUID Hypervisor Present：弱特征          +1
				//
				// 阈值设为 5：
				// 不会因为单一特征就认定为 VM。
				//
				// 示例：
				// VMware SMBIOS + Hypervisor bit = 5 -> VM
				// VirtualBox Guest 驱动 + 虚拟 MAC = 5 -> VM
				// 物理机仅启用 Hyper-V/VBS = 1 -> 非 VM
				// 物理机装了 Hyper-V 虚拟网卡 + VBS = 3 -> 非 VM

				int score = 0;

				if (HasVirtualMachineSmbiosSignature())
					score += 4;

				if (HasVirtualMachineGuestDrivers())
					score += 3;

				if (HasVirtualMachineMacAddress())
					score += 2;

				if (CpuReportsHypervisor())
					score += 1;

				return score >= 5;
			}
		}

		// 获取系统关于信息
		SystemAboutInfo GetSystemAboutInfo()
		{
			SystemAboutInfo info;

			constexpr const wchar_t* WindowsCurrentVersionKey =
				L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";

			// =========================
			// 设备规格
			// =========================

			info.deviceName = GetDeviceName();

			info.processor = GetProcessorName();

			info.installedRam = GetInstalledRam();

			// 这里使用 MachineGuid 作为 Windows 安装实例标识。
			// 它并不是永久硬件 ID。
			info.deviceId = WideToUtf8(
				ReadRegistryString(
				HKEY_LOCAL_MACHINE,
				L"SOFTWARE\\Microsoft\\Cryptography",
				L"MachineGuid"
			)
			);

			info.productId = WideToUtf8(
				ReadRegistryString(
				HKEY_LOCAL_MACHINE,
				WindowsCurrentVersionKey,
				L"ProductId"
			)
			);

			info.systemType = GetSystemType();

			info.penAndTouch = GetPenAndTouch();

			// =========================
			// Windows 规格
			// =========================

			info.edition = WideToUtf8(
				ReadRegistryString(
				HKEY_LOCAL_MACHINE,
				WindowsCurrentVersionKey,
				L"ProductName"
			)
			);

			info.version = WideToUtf8(
				ReadRegistryString(
				HKEY_LOCAL_MACHINE,
				WindowsCurrentVersionKey,
				L"DisplayVersion"
			)
			);

			info.installDate = FormatInstallDate(
				ReadRegistryDWORD(
				HKEY_LOCAL_MACHINE,
				WindowsCurrentVersionKey,
				L"InstallDate"
			)
			);

			const std::wstring build = ReadRegistryString(
				HKEY_LOCAL_MACHINE,
				WindowsCurrentVersionKey,
				L"CurrentBuildNumber"
			);

			const DWORD ubr = ReadRegistryDWORD(
				HKEY_LOCAL_MACHINE,
				WindowsCurrentVersionKey,
				L"UBR"
			);

			if (!build.empty())
			{
				info.osBuild = WideToUtf8(build);

				if (ubr != 0)
				{
					info.osBuild += ".";
					info.osBuild += std::to_string(ubr);
				}
			}

			// Windows“体验/功能包”没有一个稳定的一一对应 Win32 API。
			// 暂时保留字段，获取不到时为空字符串。
			info.experience.clear();

			// =========================
			// 虚拟机判断
			// =========================

			info.isVirtualMachine = DetectVirtualMachine();

			return info;
		}



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
			bool needCoUninitialize = false;
			if (hr == S_OK)
			{
				// 当前线程第一次初始化 COM
				needCoUninitialize = true;
			}
			else if (hr == S_FALSE)
			{
				// 当前线程已经初始化过 COM
				// 这次调用仍然算成功，后面仍需配对 CoUninitialize
				needCoUninitialize = true;
			}
			else if (hr == RPC_E_CHANGED_MODE)
			{
				// 当前线程已经被初始化成别的模型（通常是 STA）
				// 对 DLL 场景来说，这不是致命错误，可以继续做 WMI
				needCoUninitialize = false;
			}
			else
			{
				// 其他失败才是真失败
				return "";
			}

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
					//result << _bstr_t(vtProp.bstrVal); // C++20会报错
					result << static_cast<const char*>(_bstr_t(vtProp.bstrVal)); // 从C++17升级到C++20，解决兼容性问题
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


		// A版本：GetUserNameA，返回 std::string
		std::string get_usernameA()
		{
			DWORD bufSize = 256;
			char szBuffer[256]{};

			if (!::GetUserNameA(szBuffer, &bufSize))
			{
				return "UserNameA failed";
			}
			return std::string(szBuffer);
		}

		// W版本：GetUserNameW，返回 std::wstring
		std::wstring get_usernameW()
		{
			DWORD bufSize = 256;
			WCHAR szBuffer[256]{};

			if (!::GetUserNameW(szBuffer, &bufSize))
			{
				return L"UserNameA failed";
			}
			return std::wstring(szBuffer);
		}



	}
}
