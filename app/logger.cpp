#include <string>
#include <iostream>
#include <array>

class log {
	private:
		constexpr std::array<std::string_view, 4> log_level_text = {"[DEBUG]: ", "[INFO]: ", "[WARN]: ", "[CRITICAL]: "};	
	public:
		void write(int log_level, std::string log_message);
};

void log::write(int log_level, std::string log_message) {
	// log_level: 1 = debug, 2 = info, 3 = warn, 4 = critical
	if (log_level =< 4 || log_level >= 1) {
		std::cout << log_level_text[log_level] << log_message << "\n";
	}
	std::cout << "[INVALID LOG_LEVEL]: " << log_message << "\n";
}
