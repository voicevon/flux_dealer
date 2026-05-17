#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>

// ============================================================================
// Logger — 日志分级宏
//
// 使用方式：
//   LOG_D("调试信息 x=%d", x);
//   LOG_I("初始化完成");
//   LOG_W("队列将满 size=%d", size);
//   LOG_E("归零超时！");
//
// 在 config.h 或编译选项中 #define LOG_LEVEL 来控制输出级别。
// ============================================================================

#define LOG_LEVEL_DEBUG 0
#define LOG_LEVEL_INFO  1
#define LOG_LEVEL_WARN  2
#define LOG_LEVEL_ERROR 3
#define LOG_LEVEL_NONE  4

#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_LEVEL_INFO
#endif

#define LOG_D(fmt, ...) do { if (LOG_LEVEL <= LOG_LEVEL_DEBUG) Serial.printf("[D] " fmt "\n", ##__VA_ARGS__); } while(0)
#define LOG_I(fmt, ...) do { if (LOG_LEVEL <= LOG_LEVEL_INFO)  Serial.printf("[I] " fmt "\n", ##__VA_ARGS__); } while(0)
#define LOG_W(fmt, ...) do { if (LOG_LEVEL <= LOG_LEVEL_WARN)  Serial.printf("[W] " fmt "\n", ##__VA_ARGS__); } while(0)
#define LOG_E(fmt, ...) do { if (LOG_LEVEL <= LOG_LEVEL_ERROR) Serial.printf("[E] " fmt "\n", ##__VA_ARGS__); } while(0)

#endif // LOGGER_H
