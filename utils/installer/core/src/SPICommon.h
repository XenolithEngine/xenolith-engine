#ifndef INSTALLER_CORE_SPICOMMON_H_
#define INSTALLER_CORE_SPICOMMON_H_

#include "SPCommon.h"
#include "SPMemory.h"
#include "SPString.h"
#include "SPData.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {
using mem_std::String;
using mem_std::Bytes;
using mem_std::Vector;
using mem_std::Function;
using Value = data::ValueTemplate<memory::StandartInterface>;

// mem_std::String is not implicitly constructible from a StringView; go through the Interface.
inline String toString(StringView sv) { return sv.str<memory::StandartInterface>(); }
inline String toString(const char *s) { return toString(StringView(s)); }
} // namespace stappler::xenolith::installer

#endif // INSTALLER_CORE_SPICOMMON_H_
