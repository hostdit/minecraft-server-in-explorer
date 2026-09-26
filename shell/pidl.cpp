#include "pidl.h"

#include <cstring>

namespace mcx {

PITEMID_CHILD makeChild(const Item& item) {
    Bytes payload = encodeItem(item);
    auto size = USHORT(sizeof(USHORT) + payload.size());
    auto memory = static_cast<std::uint8_t*>(CoTaskMemAlloc(size + sizeof(USHORT)));
    if (!memory) throw std::bad_alloc();
    std::memcpy(memory, &size, sizeof size);
    std::memcpy(memory + sizeof size, payload.data(), payload.size());
    std::memset(memory + size, 0, sizeof(USHORT));
    return reinterpret_cast<PITEMID_CHILD>(memory);
}

std::optional<Item> readChild(PCUIDLIST_RELATIVE pidl) {
    if (!pidl) return std::nullopt;
    USHORT size;
    std::memcpy(&size, pidl, sizeof size);
    if (size <= sizeof size) return std::nullopt;
    auto bytes = reinterpret_cast<const std::uint8_t*>(pidl) + sizeof size;
    return decodeItem(std::span(bytes, size - sizeof size));
}

Pidl absolute(PCIDLIST_ABSOLUTE root, std::span<const Item> path) {
    Pidl result(ILCloneFull(root));
    if (!result) throw std::bad_alloc();
    for (const Item& item : path) {
        Pidl child(makeChild(item));
        Pidl combined(ILCombine(reinterpret_cast<PCIDLIST_ABSOLUTE>(result.get()), reinterpret_cast<PCUIDLIST_RELATIVE>(child.get())));
        if (!combined) throw std::bad_alloc();
        result = std::move(combined);
    }
    return result;
}

bool isFolder(ItemKind kind) {
    return kind == ItemKind::world || kind == ItemKind::players || kind == ItemKind::chat || kind == ItemKind::chunk;
}

}
