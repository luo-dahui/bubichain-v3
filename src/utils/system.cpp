#include "system.h"

#ifdef WIN32
#else
#include <mntent.h>
#endif // WIN32


namespace utils{
	SystemProcessor::SystemProcessor() {
		core_count_ = 0;
		cpu_type_ = "";
		user_time_ = 0;
		nice_time_ = 0;
		system_time_ = 0;
		idle_time_ = 0;
		io_wait_time_ = 0;
		irq_time_ = 0;
		soft_irq_time_ = 0;
		usage_percent_ = 0;

		utime_ = 0;
		stime_ = 0;
		cutime_ = 0;
		cstime_ = 0;
		usage_current_percent_ = 0;
	}
	SystemProcessor::~SystemProcessor() {

	}

	uint64_t SystemProcessor::GetTotalTime() {
#ifdef WIN32
		return user_time_ + system_time_;
#else
		return user_time_+nice_time_+system_time_+idle_time_+io_wait_time_+irq_time_+soft_irq_time_;
#endif
	}
	uint64_t SystemProcessor::GetUsageTime() {
#ifdef WIN32
		return user_time_ + system_time_ - idle_time_;
#else
		return user_time_+nice_time_+system_time_;
#endif
	}

	uint64_t SystemProcessor::GetCurrentUsageTime() {
#ifdef WIN32
		return utime_ + stime_;
#else
		return utime_ + stime_ + cutime_ + cstime_;
#endif
	}

	System::System(bool with_processors) {
		with_processors_ = with_processors;
		processor_list_ = NULL;

#if WIN32
		if (with_processors) {
			HMODULE hmodule = LoadLibrary("Ntdll.dll");
			pfn_nt_query_system_information_ = NULL;
			if (hmodule != NULL) {
				pfn_nt_query_system_information_ = (PROCNTQSI)GetProcAddress(hmodule, "NtQuerySystemInformation");
			}
		}
#endif
	}

	System::~System() {
		delete processor_list_;
	}

	bool System::UpdateProcessor() {
		SystemProcessor nold_processer = processor_;
		SystemProcessorVector old_process_list;

		if (with_processors_) {
			if (NULL != processor_list_) {
				old_process_list.assign(processor_list_->begin(), processor_list_->end());
				processor_list_->clear();
			}
			else {
				processor_list_ = new SystemProcessorVector();
			}
		}

#ifdef WIN32
		//get cpu information
		SYSTEM_INFO system_info;
		GetSystemInfo(&system_info);

		processor_.core_count_ = system_info.dwNumberOfProcessors;
		switch (system_info.wProcessorArchitecture) {
		case PROCESSOR_ARCHITECTURE_ALPHA:
			processor_.cpu_type_ = "Intel";
			break;
		case PROCESSOR_ARCHITECTURE_MIPS:
			processor_.cpu_type_ = "MIPS";
			break;
		case PROCESSOR_ARCHITECTURE_ARM:
			processor_.cpu_type_ = "ARM";
			break;
		case PROCESSOR_ARCHITECTURE_AMD64:
			processor_.cpu_type_ = "AMD64";
		}


		FILETIME idle_time_, nkernel_time, user_time;
		if (!GetSystemTimes(&idle_time_, &nkernel_time, &user_time))
			return false;

		processor_.user_time_ = (int64_t)((((uint64_t)user_time.dwHighDateTime) << 32) + (uint64_t)user_time.dwLowDateTime);
		processor_.system_time_ = (int64_t)((((uint64_t)nkernel_time.dwHighDateTime) << 32) + (uint64_t)nkernel_time.dwLowDateTime);
		processor_.idle_time_ = (int64_t)((((uint64_t)idle_time_.dwHighDateTime) << 32) + (uint64_t)idle_time_.dwLowDateTime);
		processor_.nice_time_ = 0;
		processor_.io_wait_time_ = 0;
		processor_.irq_time_ = 0;
		processor_.soft_irq_time_ = 0;

		if (with_processors_ && NULL != pfn_nt_query_system_information_) {
			ULONG uOutLength = 0;
			ULONG uInBufferLength = sizeof(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION)* processor_.core_count_;

			unsigned char *pBuffer = (unsigned char*)malloc(uInBufferLength);
			DWORD dwCode = (pfn_nt_query_system_information_)(SystemProcessorPerformanceInformation, pBuffer, uInBufferLength, &uOutLength);
			if (dwCode == 0) {
				SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION *pInfo = (SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION*)(pBuffer);
				for (size_t n = 0; n < processor_.core_count_; ++n) {
					SystemProcessor processorItem;
					processorItem.idle_time_ = pInfo->IdleTime.QuadPart;
					processorItem.user_time_ = pInfo->UserTime.QuadPart;
					processorItem.system_time_ = pInfo->KernelTime.QuadPart;
					++pInfo;
				}
			}
			free(pBuffer);
		}

		HANDLE hprocess = GetCurrentProcess();
		FILETIME nCreationTime, nExitTime, nKernelTime, nUserTime;
		if (GetProcessTimes(hprocess, &nCreationTime, &nExitTime, &nKernelTime, &nUserTime)) {
			processor_.utime_ = ((uint64_t)nUserTime.dwHighDateTime) << 32 | nUserTime.dwLowDateTime;
			processor_.stime_ = ((uint64_t)nKernelTime.dwHighDateTime) << 32 | nKernelTime.dwLowDateTime;
		}
		CloseHandle(hprocess);
#else
		File proce_file;

		if (!proce_file.Open("/proc/stat", File::FILE_M_READ))
			return false;

		std::string strline;
		StringVector values;

		if (!proce_file.ReadLine(strline, 1024)){
			proce_file.Close();
			return false;
		}

		values = String::split(strline, " ");
		if (values.size() < 8){
			proce_file.Close();
			return false;
		}

		processor_.user_time_ = String::Stoi64(values[1]);
		processor_.nice_time_ = String::Stoi64(values[2]);
		processor_.system_time_ = String::Stoi64(values[3]);
		processor_.idle_time_ = String::Stoi64(values[4]);
		processor_.io_wait_time_ = String::Stoi64(values[5]);
		processor_.irq_time_ = String::Stoi64(values[6]);
		processor_.soft_irq_time_ = String::Stoi64(values[7]);
		processor_.core_count_ = 0;

		strline = "";
		while (proce_file.ReadLine(strline, 1024)) {
			values = String::split(strline, " ");
			if (values.size() < 8)
				break;
			if (std::string::npos == values[0].find("cpu"))
				break;

			processor_.core_count_++;
		}
		proce_file.Close();

		uint32_t process_id = getpid();
		std::string stat_name = utils::String::Format("/proc/%d/stat", process_id);

		if (!proce_file.Open(stat_name, File::FILE_M_READ))
			return false;

		strline = "";
		if (!proce_file.ReadLine(strline, 1024)){
			proce_file.Close();
			return false;
		}

		values.clear();
		values = String::split(strline, " ");
		if (values.size() < 44){
			proce_file.Close();
			return false;
		}

		processor_.utime_ = String::Stoi64(values[13]);
		processor_.stime_ = String::Stoi64(values[14]);
		processor_.cutime_ = String::Stoi64(values[15]);
		processor_.cstime_ = String::Stoi64(values[16]);
		proce_file.Close();
#endif
		if (nold_processer.system_time_ > 0) {
			int64_t total_time1 = nold_processer.GetTotalTime();
			int64_t total_time2 = processor_.GetTotalTime();
			int64_t usage_time1 = nold_processer.GetUsageTime();
			int64_t usage_time2 = processor_.GetUsageTime();
			int64_t current_usage_time1 = nold_processer.GetCurrentUsageTime();
			int64_t current_usage_time2 = processor_.GetCurrentUsageTime();
			if (total_time2 > total_time1 && usage_time2 > usage_time1) {
				processor_.usage_percent_ = double(usage_time2 - usage_time1) / double(total_time2 - total_time1)*100.0;
			}
			else {
				processor_.usage_percent_ = 0;
			}

			if (total_time2 > total_time1 && current_usage_time2 > current_usage_time1) {
				processor_.usage_current_percent_ = double(current_usage_time2 - current_usage_time1) / double(total_time2 - total_time1)*100.0;
			}
			else {
				processor_.usage_current_percent_ = 0;
			}
		}
		else {
			processor_.usage_percent_ = double(processor_.GetUsageTime()) / double(processor_.GetTotalTime()) * 100;
			processor_.usage_current_percent_ = double(processor_.GetCurrentUsageTime()) / double(processor_.GetTotalTime()) * 100;
		}

		return true;
	}

	bool System::GetPhysicalDisk(const std::string &path, PhysicalDisk &disk) {
#ifdef WIN32
		ULARGE_INTEGER available, total, free;
		if (!GetDiskFreeSpaceExA(path.c_str(), &available, &total, &free)) {
			return false;
		}

		disk.total_bytes_ = total.QuadPart;
		disk.free_bytes_ = free.QuadPart;
		disk.available_bytes_ = available.QuadPart;
#else
		struct statfs ndisk_stat;

		if (statfs(path.c_str(), &ndisk_stat) != 0) {
			return false;
		}

		disk.total_bytes_ = (uint64_t)(ndisk_stat.f_blocks) * (uint64_t)(ndisk_stat.f_frsize);
		disk.available_bytes_ = (uint64_t)(ndisk_stat.f_bavail) * (uint64_t)(ndisk_stat.f_bsize);
		// default as root
		disk.free_bytes_ = disk.available_bytes_;
#endif
		if (disk.total_bytes_ > disk.free_bytes_) {
			disk.usage_percent_ = double(disk.total_bytes_ - disk.free_bytes_) / double(disk.total_bytes_) * 100.0;
		}

		return true;
	}

	bool System::GetPhysicalMemory(PhysicalMemory &memory) {
#ifdef WIN32
		MEMORYSTATUSEX status;
		status.dwLength = sizeof(MEMORYSTATUSEX);

		::GlobalMemoryStatusEx(&status);

		memory.total_bytes_ = status.ullTotalPhys;
		memory.free_bytes_ = status.ullAvailPhys;
		memory.available_bytes_ = status.ullTotalPhys - status.ullAvailPhys;
		memory.cached_bytes_ = 0;
		memory.buffers_bytes_ = 0;

		HANDLE hprocess = GetCurrentProcess();
		PROCESS_MEMORY_COUNTERS nMemoryInfo = { 0 };
		nMemoryInfo.cb = sizeof(nMemoryInfo);
		if (GetProcessMemoryInfo(hprocess, &nMemoryInfo, sizeof(nMemoryInfo))) {
			memory.physical_memory_size_ = nMemoryInfo.PeakWorkingSetSize;
			memory.virtual_memory_size_ = nMemoryInfo.WorkingSetSize;
		}

		CloseHandle(hprocess);
#else
		File proc_file;

		if (!proc_file.Open("/proc/meminfo", File::FILE_M_READ)) {
			return false;
		}

		std::string strline;
		while (proc_file.ReadLine(strline, 1024)) {
			StringVector values;
			values=String::split(strline,":");
			String::Trim(values[0]);
			String::Trim(values[1]);
			const std::string &strkey = values[0];
			const std::string &strvalue = values[1];
			if ( strkey.compare("MemTotal")==0) {
				memory.total_bytes_=String::Stoui64( values[1])*1024;
			}
			else if (strkey.compare("MemFree") == 0) {
				memory.free_bytes_=String::Stoui64( values[1])*1024;
			}
			else if (strkey.compare("Buffers") == 0) {
				memory.buffers_bytes_=String::Stoui64( values[1])*1024;
			}
			else if (strkey.compare("Cached") == 0) {
				memory.cached_bytes_=String::Stoui64( values[1])*1024;
			}
		}
		proc_file.Close();

		uint32_t process_id = getpid();

		std::string stat_name = utils::String::Format("/proc/%d/statm", process_id);
		if (!proc_file.Open(stat_name, File::FILE_M_READ))
			return false;

		strline = "";
		if (!proc_file.ReadLine(strline, 1024)){
			proc_file.Close();
			return false;
		}
		StringVector values = String::split(strline, " ");
		if (values.size() < 2){
			proc_file.Close();
			return false;
		}

		proc_file.Close();

		memory.virtual_memory_size_ = String::Stoi64(values[0]) * 4 * 1024;
		memory.physical_memory_size_ = String::Stoi64(values[1]) * 4 * 1024;
#endif
		memory.available_bytes_ = memory.free_bytes_ + memory.buffers_bytes_ + memory.cached_bytes_;

		if (memory.total_bytes_ > memory.available_bytes_) {
			memory.usage_percent_ = double(memory.total_bytes_ - memory.available_bytes_) / double(memory.total_bytes_) * (double)100.0;
		}

		if (memory.total_bytes_ > memory.physical_memory_size_) {
			memory.current_usage_percent_ = double(memory.physical_memory_size_) / double(memory.total_bytes_) * (double)100.0;
		}

		return true;
	}

	time_t System::GetStartupTime(time_t time_now) {
		time_t startup_time = 0;
		if (0 == time_now) {
			time_now = time(NULL);
		}
#ifdef WIN32
		LARGE_INTEGER count, freq;

		if (!QueryPerformanceCounter(&count) || !QueryPerformanceFrequency(&freq) || 0 == freq.QuadPart) {
			return 0;
		}
		startup_time = time_now - (time_t)(count.QuadPart / freq.QuadPart);
#else
		struct sysinfo info;

		memset(&info, 0, sizeof(info));
		sysinfo(&info);
		startup_time = time_now - (time_t)info.uptime;
#endif
		return startup_time;

	}

	size_t System::GetCpuCoreCount() {
		size_t core_count = 1;
#if defined(WIN32)
		SYSTEM_INFO nSystemInfo;
		GetSystemInfo(&nSystemInfo);
		core_count = nSystemInfo.dwNumberOfProcessors;
#else
		core_count = get_nprocs();
#endif
		return core_count;
	}

	bool System::GetHardwareAddress(std::string& hard_address, char* out_msg) {
		bool bret = false;
		do {
			std::string cpu_id;
			if (!GetCpuId(cpu_id)) {
				strcpy(out_msg, "get cpu id failed");
				break;
			}
			std::string mac;
			if (!GetMac(mac)) {
				strcpy(out_msg, "get mac address failed");
				break;
			}
			std::string hard_info = cpu_id + mac;
			hard_address = utils::MD5::GenerateMD5(hard_info);
			bret = true;
		} while (false);
		return bret;
	}

	bool System::GetCpuId(std::string& cpu_id) {
		bool bret = false;
#ifdef WIN32
		HANDLE hReadPipe = NULL; // pipe for read
		HANDLE hWritePipe = NULL; // pipe for write
		PROCESS_INFORMATION pi;
		do  {
			const long MAX_COMMAND_SIZE = 10000; // command buffer size
			char fetch_cmd[] = "wmic cpu get processorid";
			const std::string en_search = "ProcessorId"; // search for this info
			STARTUPINFO	si;	  // command windows attribute
			SECURITY_ATTRIBUTES sa;

			char buffer[MAX_COMMAND_SIZE + 1] = { 0 }; // ouput command
			unsigned long count = 0;
			long ipos = 0;
			memset(&pi, 0, sizeof(pi));
			memset(&si, 0, sizeof(si));
			memset(&sa, 0, sizeof(sa));
			pi.hProcess = NULL;
			pi.hThread = NULL;
			si.cb = sizeof(STARTUPINFO);
			sa.nLength = sizeof(SECURITY_ATTRIBUTES);
			sa.lpSecurityDescriptor = NULL;
			sa.bInheritHandle = TRUE;
			// create pipe
			bret = CreatePipe(&hReadPipe, &hWritePipe, &sa, 0) ? true : false;
			if (!bret) break;
			// set the pipe for command info
			GetStartupInfo(&si);
			si.hStdError = hWritePipe;
			si.hStdOutput = hWritePipe;
			si.wShowWindow = SW_HIDE; // hide command window
			si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
			// create command process
			bret = CreateProcessA(NULL, fetch_cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi) ? true : false;
			if (!bret) break;
			// read data
			WaitForSingleObject(pi.hProcess, 500);
			bret = ReadFile(hReadPipe, buffer, MAX_COMMAND_SIZE, &count, 0) ? true : false;
			if (!bret) break;
			// search cpu serial
			bret = FALSE;
			cpu_id = buffer;
			ipos = cpu_id.find(en_search);
			if (ipos < 0) break;
			cpu_id = cpu_id.substr(ipos + en_search.length());
			cpu_id.erase(std::remove_if(cpu_id.begin(), cpu_id.end(), isspace), cpu_id.end()); // delete space
			bret = true;
		} while (false);
		// �ر����еľ��
		if (hWritePipe) CloseHandle(hWritePipe);
		if (hReadPipe) CloseHandle(hReadPipe);
		if (pi.hProcess) CloseHandle(pi.hProcess);
		if (pi.hThread) CloseHandle(pi.hThread);
#else
		do {
			FILE* p = popen("dmidecode -t 4 | grep ID", "r"); // need root
			if (p == NULL) break;
			const int MAXLINE = 256;
			char result_buf[MAXLINE] = { 0 };
			if (fgets(result_buf, MAXLINE, p) != NULL) {
				if (result_buf[strlen(result_buf) - 1] == '\n') {
					result_buf[strlen(result_buf) - 1] = '\0';
				}
			}
			int rc = pclose(p);
			if (rc == -1)  break;
			cpu_id.assign(result_buf);
			cpu_id.erase(std::remove_if(cpu_id.begin(), cpu_id.end(), isspace), cpu_id.end()); // delete space
			cpu_id.erase(0, 3); // delete ID:
			bret = true;
		} while (false);
#endif
		return bret;

	}

	bool System::GetMac(std::string& mac) {
		bool bret = false;
		std::list<std::string> macs;
#ifdef WIN32
		do {
			char ac_mac[32] = { 0 };
			ULONG ulSize = sizeof(IP_ADAPTER_INFO);
			PIP_ADAPTER_INFO pinfo = NULL;
			// get necessary size
			GetAdaptersInfo(pinfo, &ulSize);
			pinfo = (PIP_ADAPTER_INFO)malloc(ulSize);
			if (NULL == pinfo) break;
			if (GetAdaptersInfo(pinfo, &ulSize) != NO_ERROR) break;

			while (pinfo) {
				do  {
					if (strstr(pinfo->Description, "Virtual") || strstr(pinfo->Description, "VMware") ||
						strstr(pinfo->Description, "Tunnel") || strstr(pinfo->Description, "Tunneling") ||
						strstr(pinfo->Description, "Pseudo") || strstr(pinfo->Description, "VirtualBox")) {
						break;
					}
					sprintf(ac_mac, "%02x%02x%02x%02x%02x%02x",
						int(pinfo->Address[0]),
						int(pinfo->Address[1]),
						int(pinfo->Address[2]),
						int(pinfo->Address[3]),
						int(pinfo->Address[4]),
						int(pinfo->Address[5]));
					//mac += ac_mac;
					macs.push_back(std::string(ac_mac));
					memset(ac_mac, 0, sizeof(ac_mac));
					bret = true;
				} while (false);
				pinfo = pinfo->Next;
			}
		} while (false);
#else
		int fd;
		do {
			int interfaceNum = 0;
			struct ifreq buf[16];
			struct ifconf ifc;
			if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) break;
			ifc.ifc_len = sizeof(buf);
			ifc.ifc_buf = (caddr_t)buf;
			if (ioctl(fd, SIOCGIFCONF, (char *)&ifc)) break;
			interfaceNum = ifc.ifc_len / sizeof(struct ifreq);
			while (interfaceNum-- > 0) {
				char ac_mac[32] = { 0 };
				struct ifreq ifrcopy;
				//ignore the interface that not up or not runing
				ifrcopy = buf[interfaceNum];
				if (ioctl(fd, SIOCGIFFLAGS, &ifrcopy)) continue;
				//get the mac of this interface
				if (ioctl(fd, SIOCGIFHWADDR, (char *)(&buf[interfaceNum]))) continue;
				snprintf(ac_mac, sizeof(ac_mac), "%02x%02x%02x%02x%02x%02x",
					(unsigned char)buf[interfaceNum].ifr_hwaddr.sa_data[0],
					(unsigned char)buf[interfaceNum].ifr_hwaddr.sa_data[1],
					(unsigned char)buf[interfaceNum].ifr_hwaddr.sa_data[2],
					(unsigned char)buf[interfaceNum].ifr_hwaddr.sa_data[3],
					(unsigned char)buf[interfaceNum].ifr_hwaddr.sa_data[4],
					(unsigned char)buf[interfaceNum].ifr_hwaddr.sa_data[5]);
				//mac += ac_mac;
				macs.push_back(std::string(ac_mac));
				memset(ac_mac, 0, sizeof(ac_mac));
				bret = true;
			}
		} while (false);
		close(fd);
#endif
		std::list<std::string>::iterator iter;
		for (iter = macs.begin(); iter != macs.end(); iter++) {
			mac += iter->c_str();
		}
		return bret;
	}

	std::string System::GetHostName() {
		char host_name[128];
		if (gethostname(host_name, 128) != 0)
			host_name[0] = '\0';
		return std::string(host_name);
	}


	std::string System::GetOsVersion() {
		std::string os_version;
#ifdef WIN32
		OSVERSIONINFOEX osvi;
		BOOL os_version_info_ex = false;
		const DWORD product_buffer_size = 1024;

		SYSTEM_INFO info;
		GetSystemInfo(&info);

		ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
		osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
		if (!(os_version_info_ex = GetVersionEx((OSVERSIONINFO*)&osvi))) {
			osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
			if (!GetVersionEx((OSVERSIONINFO*)&osvi)) {
				return os_version;
			}
		}

		switch (osvi.dwPlatformId) {
			// Test for the Windows NT product family.
		case VER_PLATFORM_WIN32_NT:

			// Test for the specific product.
			if (osvi.dwMajorVersion == 10 && osvi.dwMinorVersion == 0 && osvi.wProductType == VER_NT_WORKSTATION)
				os_version = "Windows 10 ";
			else if (osvi.dwMajorVersion == 6 && osvi.dwMinorVersion == 3) {
				if (osvi.wProductType == VER_NT_WORKSTATION)
					os_version = "Windows 8.1 ";
				else
					os_version = "Windows Server 2012 r2 ";
			}
			else if (osvi.dwMajorVersion == 6 && osvi.dwMinorVersion == 2) {
				if (osvi.wProductType == VER_NT_WORKSTATION)
					os_version = "Windows 8 ";
				else
					os_version = "Windows Server 2012 ";
			}
			else if (osvi.dwMajorVersion == 6 && osvi.dwMinorVersion == 1) {
				if (osvi.wProductType == VER_NT_WORKSTATION)
					os_version = "Windows 7 ";
				else
					os_version = "Windows Server 2008 R2 ";
			}
			else if (osvi.dwMajorVersion == 6 && osvi.dwMinorVersion == 0) {
				if (osvi.wProductType == VER_NT_WORKSTATION)
					os_version = "Windows Vista ";
				else
					os_version = "Windows Server 2008 ";
			}
			else if (osvi.dwMajorVersion == 5 && osvi.dwMinorVersion == 2) {
				if (osvi.wProductType == VER_NT_WORKSTATION && info.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64)
					os_version = "Windows xp Pro x64 Edition ";
				else if (GetSystemMetrics(SM_SERVERR2) == 0)
					os_version = "Windows Server 2003 ";
				else if (GetSystemMetrics(SM_SERVERR2) != 0)
					os_version = "Windows Server 2003 R2 ";
			}
			else if (osvi.dwMajorVersion == 5 && osvi.dwMinorVersion == 1) {
				if (osvi.wSuiteMask & VER_SUITE_EMBEDDEDNT)
					os_version = "Windows XP Embedded ";
				else
					os_version = "Windows XP ";
			}

			else if (osvi.dwMajorVersion == 5 && osvi.dwMinorVersion == 0)
				os_version = "Windows 2000 ";
			else
				os_version = String::Format("Windows NT(%u.%u)", osvi.dwMajorVersion, osvi.dwMinorVersion);

			// Test for specific product on Windows NT 4.0 SP6 and later.
			if (os_version_info_ex) {
				// Test for the workstation type.
				switch (osvi.dwMajorVersion){
				case 4:
					if (osvi.wProductType == VER_NT_WORKSTATION)
						os_version += "Workstation 4.0 ";
					else if (osvi.wSuiteMask & VER_SUITE_ENTERPRISE)
						os_version += "Server 4.0 Enterprise Edition ";
					else
						os_version += "Server 4.0 ";
				case 5:
					switch (osvi.dwMinorVersion){
					case 0:                  //Windows 2000 
						if (osvi.wSuiteMask == VER_SUITE_ENTERPRISE)
							os_version += "Advanced Server ";
						break;
					case 1:                  //Windows XP 
						if (osvi.wSuiteMask == VER_SUITE_EMBEDDEDNT)
							os_version += "Embedded ";
						else if (osvi.wSuiteMask == VER_SUITE_PERSONAL)
							os_version += "Home Edition ";
						else
							os_version += "Professional ";
						break;
					case 2:
						if (GetSystemMetrics(SM_SERVERR2) == 0 && osvi.wSuiteMask == VER_SUITE_BLADE)  //Windows Server 2003 
							os_version += "Web Edition ";
						else if (GetSystemMetrics(SM_SERVERR2) == 0 && osvi.wSuiteMask == VER_SUITE_COMPUTE_SERVER)
							os_version += "Compute Cluster Edition ";
						else if (GetSystemMetrics(SM_SERVERR2) == 0 && osvi.wSuiteMask == VER_SUITE_STORAGE_SERVER)
							os_version += "Storage Server ";
						else if (GetSystemMetrics(SM_SERVERR2) == 0 && osvi.wSuiteMask == VER_SUITE_DATACENTER)
							os_version += "DataCenter Edition ";
						else if (GetSystemMetrics(SM_SERVERR2) == 0 && osvi.wSuiteMask == VER_SUITE_ENTERPRISE)
							os_version += "Enterprise Edition ";
						else if (GetSystemMetrics(SM_SERVERR2) != 0 && osvi.wSuiteMask == VER_SUITE_STORAGE_SERVER)  // Windows Server 2003 R2
							os_version += "Storage Server ";
						break;
					}
					break;
				case 6:
					if (osvi.wProductType != VER_NT_WORKSTATION && osvi.wSuiteMask == VER_SUITE_DATACENTER)
						os_version += "DataCenter Server ";
					else if (osvi.wProductType != VER_NT_WORKSTATION && osvi.wSuiteMask == VER_SUITE_ENTERPRISE)
						os_version += "Enterprise ";
					else if (osvi.wProductType == VER_NT_WORKSTATION && osvi.wSuiteMask != VER_SUITE_ENTERPRISE)
						os_version += "Home Edition ";
					else if (osvi.wProductType == VER_NT_WORKSTATION && osvi.wSuiteMask == VER_SUITE_ENTERPRISE)
						os_version += "Enterprise ";
					break;
				case 10:
					if (osvi.wProductType == VER_NT_WORKSTATION && osvi.wSuiteMask == VER_SUITE_ENTERPRISE)
						os_version += "Enterprise ";
					else if (osvi.wProductType == VER_NT_WORKSTATION && osvi.wSuiteMask != VER_SUITE_ENTERPRISE)
						os_version += "Home Edition ";
					break;
				default:
					os_version += "";
				}
			}
			// Test for specific product on Windows NT 4.0 SP5 and earlier
			else {
				HKEY hkey;
				CHAR product_type[product_buffer_size];
				DWORD dwbuf_len = product_buffer_size;
				LONG lRet = 0;

				memset(product_type, 0, sizeof(product_type));

				if (ERROR_SUCCESS == RegOpenKeyEx(HKEY_LOCAL_MACHINE,
					"SYSTEM\\CurrentControlSet\\Control\\ProductOptions",
					0, KEY_QUERY_VALUE, &hkey)) {
					if (ERROR_SUCCESS == RegQueryValueExA(hkey, "ProductType", NULL, NULL,
						(LPBYTE)product_type, &dwbuf_len)) {
						if (stricmp("WINNT", product_type) == 0)
							os_version += "Workstation ";
						if (stricmp("LANMANNT", product_type) == 0)
							os_version += "Server ";
						if (stricmp("SERVERNT", product_type) == 0)
							os_version += "Advanced Server ";
					}
					RegCloseKey(hkey);
				}

				String::AppendFormat(os_version, "%u.%u", osvi.dwMajorVersion, osvi.dwMinorVersion);
			}

			// Display service pack (if any) and build number.
			if (osvi.dwMajorVersion == 4 &&
				stricmp(osvi.szCSDVersion, "Service Pack 6") == 0) {
				HKEY hkey = NULL;
				LONG lRet = 0;

				// Test for SP6 versus SP6a.
				if (ERROR_SUCCESS == RegOpenKeyExA(HKEY_LOCAL_MACHINE,
					"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Hotfix\\Q246009",
					0, KEY_QUERY_VALUE, &hkey)) {
					String::AppendFormat(os_version, "Service Pack 6a (Build %d)", osvi.dwBuildNumber & 0xFFFF);
					RegCloseKey(hkey);
				}
				else {// Windows NT 4.0 prior to SP6a
					String::AppendFormat(os_version, "%s (Build %d)",
						osvi.szCSDVersion,
						osvi.dwBuildNumber & 0xFFFF);
				}
			}
			else {// not Windows NT 4.0 
				String::AppendFormat(os_version, "%s (Build %d)",
					osvi.szCSDVersion,
					osvi.dwBuildNumber & 0xFFFF);
			}
			break;
		}
#else
		struct utsname unix_name;

		memset(&unix_name, 0, sizeof(unix_name));
		if (uname(&unix_name) != 0) {
			os_version = "Unknown";
		}
		else {
			os_version = String::Format("%s %s %s %s",
				unix_name.sysname,
				unix_name.release,
				unix_name.machine,
				unix_name.version);
		}
#endif
		return os_version;
	}

	std::string System::GetOsBits(){
		std::string os_bit = "";

#ifdef WIN32
		BOOL bis_wow64 = FALSE;

		typedef BOOL(WINAPI *LPFN_ISWOW64PROCESS) (HANDLE, PBOOL);
		LPFN_ISWOW64PROCESS fn_is_wow64_process;

		fn_is_wow64_process = (LPFN_ISWOW64PROCESS)GetProcAddress(
			GetModuleHandle("kernel32"), "IsWow64Process");
		if (NULL != fn_is_wow64_process) {
			fn_is_wow64_process(GetCurrentProcess(), &bis_wow64);
		}

		if (bis_wow64) {
			os_bit = "64";
		}
		else {
			os_bit = "32";
		}
#else
		FILE *fstream=NULL;
		char buff[1024];
		memset(buff, 0, sizeof(buff));
		if (NULL == (fstream = popen("getconf LONG_BIT", "r"))) {
			os_bit = "error";
			return os_bit;
		}
		if (NULL != fgets(buff, sizeof(buff), fstream)) {
			buff[2] = '\0';
			os_bit = buff;
		}
		else {
			os_bit = "error";
		}
		pclose(fstream);

#endif
		return os_bit;
	}

	uint64_t System::GetLogsSize(const std::string path) {

		uint64_t log_size = 0;

		std::string out_file_name = path + "-out";
		std::string err_file_name = path + "-err";

		std::string file_ext = File::GetExtension(path);
		if (file_ext.size() > 0 && (file_ext.size() + 1) < path.size())
		{
			std::string sub_path = path.substr(0, path.size() - file_ext.size() - 1);

			out_file_name = String::Format("%s-out.%s", sub_path.c_str(), file_ext.c_str());
			err_file_name = String::Format("%s-err.%s", sub_path.c_str(), file_ext.c_str());
		}

		log_size = GetLogSize(out_file_name.c_str());
		log_size += GetLogSize(err_file_name.c_str());

		return log_size;
	}

	uint64_t System::GetLogSize(const char* path) {

		uint64_t log_size = 0;

		struct stat buff;
		if (stat(path, &buff) < 0) {
			return log_size;
		}
		else {
			log_size = buff.st_size;
		}

		return log_size;
	}

	bool System::GetPhysicalPartition(uint64_t &total_bytes, PhysicalPartitionVector &nPartitionList) {
		total_bytes = 0;
		nPartitionList.clear();

#ifdef WIN32
		int nPartitionCount = 0;
		DWORD dwCode = GetLogicalDriveStringsA(0, NULL);
		char *pBuffer = (char *)malloc(sizeof(char)* dwCode + 1);
		memset(pBuffer, 0, dwCode + 1);
		dwCode = GetLogicalDriveStringsA(dwCode + 1, pBuffer);
		for (char *pszTmp = pBuffer; *pszTmp && nPartitionCount < 100; pszTmp += strlen(pszTmp) + 1, nPartitionCount++) {
			std::string strPartitionRoot = pszTmp;
			if (GetDriveTypeA(strPartitionRoot.c_str()) != DRIVE_FIXED) {
				continue;
			}

			ULARGE_INTEGER nAvailable, nTotal, nFree;
			if (!GetDiskFreeSpaceExA(strPartitionRoot.c_str(), &nAvailable, &nTotal, &nFree)) {
				break;
			}

			PhysicalPartition nPartition;
			nPartition.total_bytes_ = nTotal.QuadPart;
			nPartition.free_bytes_ = nFree.QuadPart;
			nPartition.available_bytes_ = nAvailable.QuadPart;
			nPartition.describe_ = strPartitionRoot;
			if (nPartition.total_bytes_ > nPartition.free_bytes_) {
				nPartition.usage_percent_ = double(nPartition.total_bytes_ - nPartition.free_bytes_) / double(nPartition.total_bytes_) * 100.0;
			}
			nPartitionList.push_back(nPartition);
			total_bytes += nPartition.total_bytes_;
		}
#else
		FILE* mount_table;
		struct mntent *mount_entry;
		struct statfs s;
		unsigned long blocks_used;
		unsigned blocks_percent_used;
		const char *disp_units_hdr = NULL;
		mount_table = NULL;
		mount_table = setmntent("/etc/mtab", "r");
		if (!mount_table) {
			fprintf(stderr, "set mount entry error\n");
			return false;
		}
		PhysicalPartition nPartition;
		while (1) {
			const char *device;
			const char *mount_point;
			if (mount_table) {
				mount_entry = getmntent(mount_table);
				if (!mount_entry) {
					endmntent(mount_table);
					break;
				}
			} 
			else
				continue;
			device = mount_entry->mnt_fsname;
			mount_point = mount_entry->mnt_dir;
			if (statfs(mount_point, &s) != 0)  {
				fprintf(stderr, "statfs failed!\n");
				continue;
			}
			if ((s.f_blocks > 0) || !mount_table )  {
				blocks_used = s.f_blocks - s.f_bfree;
				blocks_percent_used = 0;
				if (blocks_used + s.f_bavail) {
					blocks_percent_used = (blocks_used * 100ULL
						+ (blocks_used + s.f_bavail) / 2
						) / (blocks_used + s.f_bavail);
				}
				if (strcmp(device, "rootfs") == 0)
					continue;
				char s1[20];
				char s2[20];
				char s3[20];
				nPartition.total_bytes_ = s.f_blocks * s.f_bsize;
				nPartition.free_bytes_ = s.f_bfree * s.f_bsize;
				nPartition.available_bytes_ = s.f_bavail * s.f_bsize;
				nPartition.describe_ = mount_point;
				nPartition.usage_percent_ = blocks_percent_used;

				nPartitionList.push_back(nPartition);
				total_bytes += nPartition.total_bytes_;
			}
		}
#endif // WIN32

		return true;
	}
}