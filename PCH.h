#pragma once

// This file is required.

#include <spdlog/sinks/basic_file_sink.h>

#include <algorithm>
#include <atomic>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"

namespace logger = SKSE::log;

using namespace std::literals;
