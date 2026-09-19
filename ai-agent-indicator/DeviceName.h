#pragma once

#include <cstddef>

constexpr size_t DEVICE_NAME_CAPACITY = 33U;

bool normalizeDeviceName(const char* input,
                         char friendly[DEVICE_NAME_CAPACITY],
                         char hostname[DEVICE_NAME_CAPACITY]);
