#ifndef LOG_H
#define LOG_H

#include "LogManager.h"

/**
 * Logging Macros (F007)
 *
 * Convenience macros that route through LogManager singleton.
 * All logging is runtime-configurable via LogManager.
 *
 * Usage:
 *   LOG_INFO(System, "Message");
 *   LOG_INFOF(MQTT, "Connected to %s", server);
 */

// Get LogManager singleton
#define _LOG LogManager::getInstance()

// Error level
#define LOG_ERROR(comp, msg) _LOG.error(LogComponent::comp, "%s", msg)
#define LOG_ERRORF(comp, fmt, ...) _LOG.error(LogComponent::comp, fmt, ##__VA_ARGS__)

// Warning level
#define LOG_WARN(comp, msg) _LOG.warn(LogComponent::comp, "%s", msg)
#define LOG_WARNF(comp, fmt, ...) _LOG.warn(LogComponent::comp, fmt, ##__VA_ARGS__)

// Info level
#define LOG_INFO(comp, msg) _LOG.info(LogComponent::comp, "%s", msg)
#define LOG_INFOF(comp, fmt, ...) _LOG.info(LogComponent::comp, fmt, ##__VA_ARGS__)

// Debug level
#define LOG_DEBUG(comp, msg) _LOG.debug(LogComponent::comp, "%s", msg)
#define LOG_DEBUGF(comp, fmt, ...) _LOG.debug(LogComponent::comp, fmt, ##__VA_ARGS__)

// Raw output (no prefix/newline)
#define LOG_RAW(msg) _LOG.raw("%s", msg)
#define LOG_RAWF(fmt, ...) _LOG.raw(fmt, ##__VA_ARGS__)

#endif // LOG_H
