#ifndef TIMER_H_
#define TIMER_H_

#include "singleton.h"
#include "thread.h"
#include "utils.h"
#include "timestamp.h"

namespace utils {
	class TimerElement {
	private:
		std::function<void(int64_t)> func_;

		int64_t id_;
		int64_t data_;
		int64_t expire_time_;
	public:
		TimerElement(int64_t id_, int64_t data_, int64_t expire_time_, std::function<void(int64_t)> const &func);
		~TimerElement() {};
		int64_t GetIndex();
		void Excute();
	};

	class Timer : public Singleton<Timer> {
		friend class Singleton<Timer>;
	private:
		std::multimap<int64_t, TimerElement> time_ele_;
		std::list<TimerElement> exeute_list_;
		utils::Mutex lock_;
		int64_t global_element_id_;
		int64_t check_interval_;
		int64_t last_check_time_;
	public:
		Timer();
		virtual ~Timer();
	public:
		bool Initialize();
		bool Exit();
		void OnTimer(int64_t current_time);

		int64_t AddTimer(int64_t micro_time, int64_t data, std::function<void(int64_t)> const &func); /* msec unit: millisecond (1/1000);*/
		bool DelTimer(int64_t index);
		void CheckExpire(int64_t cur_time);
	};
}
#endif 