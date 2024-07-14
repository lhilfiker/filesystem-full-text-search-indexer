#include "functions.h"
#include <string>
#include <iostream>
#include <array>

class log {
	private:
		int min_log_level = 1;
		constexpr std::array<std::string_view, 4> log_level_text = {"[DEBUG]: ", "[INFO]: ", "[WARN]: ", "[CRITICAL]: "};	
	public:
		void save_config(int config_min_log_level); 
		void write(int log_level, std::string log_message);
};

void save_config(int config_min_log_level) {
	//min_log_level
	if (config_min_log_level =< 4 || config_min_log_level => 1) {
		min_log_level = config_min_log_level;
	}
	else {
		min_log_level = 3;
		log.write(3, "log: save_config: invalid min_log_level value. continuing with level 3 warn.");
	}
}

void log::write(int log_level, std::string log_message) {
	// log_level: 1 = debug, 2 = info, 3 = warn, 4 = critical
	if (log_level =< 4 || log_level >= 1) {
		std::cout << log_level_text[log_level] << log_message << "\n";
	}
	std::cout << "[INVALID LOG_LEVEL]: " << log_message << "\n";
}
