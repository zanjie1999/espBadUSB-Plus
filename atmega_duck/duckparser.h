#pragma once

#include <stddef.h> // size_t

namespace duckparser {
    void parse(const char* str, size_t len);
    int getRepeats();
    unsigned int getDelayTime();
};