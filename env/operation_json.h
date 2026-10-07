#pragma once
#include "env/operation.h"
#include "env/snapshot_json.h"
namespace sts2 {
Operation ParseOperationDocument(const snapshot_detail::Json& document);
}
