#include "functions.h"
#include <string>
#include <iostream>
#include <array>

int log::min_log_level = 1;

void log::save_config(int config_min_log_level) {
	//min_log_level
	if (config_min_log_level >= 1 && config_min_log_level <= 4) {
		min_log_level = config_min_log_level;
	}
	else {
		min_log_level = 3;
		write(3, "log: save_config: invalid min_log_level value. continuing with level 3 warn.");
	}
}

void log::write(int log_level, std::string log_message) {
	// log_level: 1 = debug, 2 = info, 3 = warn, 4 = critical
	if (min_log_level <= log_level) {
		if (log_level >= 1 && log_level <= 4) {
			std::cout << log_level_text[log_level - 1] << log_message << "\n";
		}
		else {
			std::cout << "[INVALID LOG_LEVEL]: " << log_message << "\n";
		}
	}
}
