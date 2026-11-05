#include "include/logger.h"

// Definizione variabile statica
Logger::Level Logger::currentLevel = Logger::INFO;
char Logger::recentLogs[Logger::RECENT_LOG_CAPACITY][Logger::RECENT_LOG_LINE_SIZE] = {{0}};
uint8_t Logger::recentLogCount = 0;
uint8_t Logger::recentLogWriteIndex = 0;

void Logger::appendRecentLog(const char* line) {
	if (!line) {
		return;
	}

	strlcpy(recentLogs[recentLogWriteIndex], line, RECENT_LOG_LINE_SIZE);
	recentLogWriteIndex = (recentLogWriteIndex + 1) % RECENT_LOG_CAPACITY;
	if (recentLogCount < RECENT_LOG_CAPACITY) {
		recentLogCount++;
	}
}

String Logger::getRecentLogs(uint8_t maxLines) {
	if (recentLogCount == 0 || maxLines == 0) {
		return String();
	}

	uint8_t linesToRead = maxLines < recentLogCount ? maxLines : recentLogCount;
	uint8_t startIndex = (recentLogWriteIndex + RECENT_LOG_CAPACITY - linesToRead) % RECENT_LOG_CAPACITY;
	String result;

	for (uint8_t i = 0; i < linesToRead; i++) {
		uint8_t idx = (startIndex + i) % RECENT_LOG_CAPACITY;
		if (recentLogs[idx][0] == '\0') {
			continue;
		}
		if (result.length() > 0) {
			result += "\n";
		}
		result += recentLogs[idx];
	}

	return result;
}
