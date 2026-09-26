#pragma once

#include "common.h"

#include <memory>
#include <span>
#include <type_traits>
#include <vector>

namespace mcx {

struct PidlFree {
    void operator()(void* pidl) const { CoTaskMemFree(pidl); }
};

using Pidl = std::unique_ptr<std::remove_pointer_t<PIDLIST_RELATIVE>, PidlFree>;

PITEMID_CHILD makeChild(const Item& item);
std::optional<Item> readChild(PCUIDLIST_RELATIVE pidl);
Pidl absolute(PCIDLIST_ABSOLUTE root, std::span<const Item> path);
bool isFolder(ItemKind kind);

}
