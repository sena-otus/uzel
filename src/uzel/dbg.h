#pragma once

#include <boost/log/trivial.hpp>


#define DBGOUT  __FILE_NAME__ << " " << "(" << __LINE__ << "): "
#define DBGOUTF  __FILE_NAME__ << "(" << __LINE__ << "): " << " " << __PRETTY_FUNCTION__ << ": "
