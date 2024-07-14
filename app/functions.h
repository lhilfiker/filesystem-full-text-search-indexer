// functions.h
#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#include <string>
#include <filesystem>

// log.cpp
void log.save_config(int config_min_log_level);
void log.write(int log_level, std::string log_message);

// index.cpp
void index.save_config(std::filesystem::path config_index_path, int config_buffer_size);
int initialize();
int uninitialize();
