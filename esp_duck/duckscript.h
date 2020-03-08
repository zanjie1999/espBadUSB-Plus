#pragma once

#include <Arduino.h> // String

namespace duckscript {
    void runTest();
    void run(String fileName);

    void nextLine();
    void repeat();
    void stopAll();
    void stop(String fileName);

    bool isRunning();
    String currentScript();
};