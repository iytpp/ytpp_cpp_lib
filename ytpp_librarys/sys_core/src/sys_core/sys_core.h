#pragma once

/// @file sys_core.h
/// @brief sys_core 全功能入口；包含所有子模块并显式导出其公共符号。

#include "sys_core/date_time.h"
#include "sys_core/disk_manipulation.h"
#include "sys_core/encoding.h"
#include "sys_core/encryption.h"
#include "sys_core/environment.h"
#include "sys_core/hash.h"
#include "sys_core/log.hpp"
#include "sys_core/machine_feature.h"
#include "sys_core/string_ex.h"
#include "sys_core/sys_processing.h"

namespace ytpp::sys_core {

using namespace date_time;
using namespace disk_manipulation;
using namespace encoding;
using namespace encryption;
using namespace environment;
using namespace hash;
using namespace machine_feature;
using namespace string_ex;
using namespace sys_processing;

} // namespace ytpp::sys_core
