#pragma once

#include <cstdint>

// Stable C ABI discovery entry points. The descriptor pointer remains valid
// for the lifetime of the loaded sts2env library.
extern "C" {
uint32_t sts2_env_abi_version();
const char* sts2_env_api_descriptor();
}
