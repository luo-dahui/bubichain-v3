#ifndef UTILS_SYSTEM_H_
#define UTILS_SYSTEM_H_

#include "utils.h"
#include "file.h"
#include "strings.h"
#include "crypto.h"
#include <sys/stat.h>

#ifdef WIN32
#include <Iphlpapi.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <Winternl.h>

#include <Winnls.h>
#include <windows.h> 
#include <lm.h>
#else
#include <cstring>
#include <algorithm>
#include <net/if.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/statvfs.h>
#include <sys/statfs.h>
#include <sys/utsname.h>
#include <shadow.h>
#endif

namespace utils {
	class PhysicalMemory {
	public:
		uint64_t total_bytes_;
		uint64_t free_bytes_;
		uint64_t buffers_bytes_;
		uint64_t cached_bytes_;
		uint64_t available_bytes_;
		double	 usage_percent_;

		uint64_t physical_memory_size_;
		uint64_t virtual_memory_size_;
		double   current_usage_percent_;

		PhysicalMemory() {
			total_bytes_ = 0;
			free_bytes_ = 0;
			buffers_bytes_ = 0;
			cached_bytes_ = 0;
			available_bytes_ = 0;
			usage_percent_ = 0;

			physical_memory_size_ = 0;
			virtual_memory_size_ = 0;
			current_usage_percent_ = 0;
		}
	};

	class PhysicalDisk {
	public:
		uint64_t total_bytes_;
		uint64_t free_bytes_;
		uint64_t available_bytes_;
		double usage_percent_;

		PhysicalDisk() {
			total_bytes_ = 0;
			free_bytes_ = 0;
			available_bytes_ = 0;
			usage_percent_ = 0;
		}
	};

	class  PhysicalHDD {
	public:
		uint64_t total_bytes_;
		std::string describe_;

		PhysicalHDD() {
			total_bytes_ = 0;
		}
	};
	typedef std::vector<PhysicalHDD> PhysicalHDDVector;

	class PhysicalPartition {
	public:
		uint64_t total_bytes_;
		uint64_t free_bytes_;
		uint64_t available_bytes_;
		double usage_percent_;

		std::string describe_;

		PhysicalPartition() {
			usage_percent_ = 0;
			total_bytes_ = free_bytes_ = available_bytes_ = 0;
		}

	};
	typedef std::vector<PhysicalPartition> PhysicalPartitionVector;

	class SystemProcessor {
	public:
		SystemProcessor();
		~SystemProcessor();

		uint64_t GetTotalTime();
		uint64_t GetUsageTime();
		uint64_t GetCurrentUsageTime();

		size_t core_count_;
		std::string cpu_type_;
		int64_t user_time_;
		int64_t nice_time_;
		int64_t system_time_;
		int64_t idle_time_;
		int64_t io_wait_time_;
		int64_t irq_time_;
		int64_t soft_irq_time_;
		double  usage_percent_;

		int64_t utime_;
		int64_t stime_;
		int64_t cutime_;
		int64_t cstime_;
		double	usage_current_percent_;
	};

	class System {

	public:
		System(bool with_processors = false);
		virtual ~System();

		bool UpdateProcessor();
		inline const SystemProcessor &GetProcessor() const{ return processor_; };
		
		bool GetPhysicalPartition(uint64_t &total_bytes, PhysicalPartitionVector &nPartitionList);
		bool GetPhysicalDisk(const std::string &str_path,utils::PhysicalDisk &disk);
		bool GetPhysicalMemory(utils::PhysicalMemory &memory);
		std::string GetHostName();
		std::string GetOsVersion();
		std::string GetOsBits();
		uint64_t GetLogsSize(const std::string path);

		static time_t GetStartupTime(time_t time_now = 0);
		static size_t GetCpuCoreCount();
		bool GetHardwareAddress(std::string& hard_address, char* out_msg);
	private:
		uint64_t GetLogSize(const char* path);
		static bool GetCpuId(std::string& cpu_id);
		static bool GetMac(std::string& mac);
	protected:
		typedef std::vector<SystemProcessor> SystemProcessorVector;

		SystemProcessor  processor_;
		SystemProcessorVector *processor_list_;
	private:
		bool with_processors_;
#ifdef WIN32
		typedef NTSTATUS(WINAPI *PROCNTQSI)(UINT, PVOID, ULONG, PULONG);
		PROCNTQSI pfn_nt_query_system_information_;


#endif
	};
}
#endif