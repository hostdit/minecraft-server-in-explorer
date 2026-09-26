#pragma once

#include "common.h"

#include <mcx/chunk.h>

namespace mcx {

HICON blockIcon(int size, BlockState state);
std::wstring stockIconLocation(SHSTOCKICONID id);
HRESULT iconFor(const Item& item, REFIID riid, void** out);

}
