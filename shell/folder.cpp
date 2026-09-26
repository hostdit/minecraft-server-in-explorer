#include "folder.h"

#include "host.h"
#include "icons.h"

#include <mcx/blocks.h>

#include <propkey.h>
#include <propvarutil.h>

#include <algorithm>
#include <array>
#include <cmath>

namespace mcx {

namespace {

const std::array<PROPERTYKEY, 5> columns = {PKEY_ItemNameDisplay, PKEY_ItemTypeText, PKEY_DateModified, PKEY_Size, PKEY_Comment};
const std::array<const wchar_t*, 5> headers = {L"Name", L"Type", L"Date modified", L"Size", L"Comment"};
const std::array<SHCOLSTATEF, 5> columnStates = {SHCOLSTATE_TYPE_STR, SHCOLSTATE_TYPE_STR, SHCOLSTATE_TYPE_DATE, SHCOLSTATE_TYPE_INT, SHCOLSTATE_TYPE_STR};
const wchar_t chatPrompt[] = L"Type here to chat";
const char timePrefix[] = "Time - ";
const char weatherPrefix[] = "Weather - ";

struct Value {
    enum { none, text, number, time } type = none;
    std::wstring words;
    std::uint64_t count = 0;
    FILETIME stamp{};
};

Value text(std::wstring words) {
    return {Value::text, std::move(words)};
}

Value number(std::uint64_t count) {
    return {Value::number, {}, count};
}

Value when(Time time) {
    auto ticks = std::chrono::duration_cast<std::chrono::microseconds>(time.time_since_epoch()).count() * 10 + 116444736000000000LL;
    Value value{Value::time};
    value.stamp.dwLowDateTime = DWORD(ticks);
    value.stamp.dwHighDateTime = DWORD(std::uint64_t(ticks) >> 32);
    return value;
}

std::wstring itemName(const Item& item) {
    switch (item.kind) {
    case ItemKind::world: return L"World";
    case ItemKind::players: return L"Players";
    case ItemKind::chat: return L"Chat";
    case ItemKind::chunk: return std::format(L"chunk {}, {}", item.x, item.z);
    case ItemKind::block: return widen(fileName(item.state, {item.x, item.y, item.z}));
    case ItemKind::player:
    case ItemKind::chatMessage: return widen(item.text);
    case ItemKind::chatInput: return chatPrompt;
    case ItemKind::herobrine: return widen(std::string(herobrineName));
    case ItemKind::time: return widen(timePrefix + item.text);
    case ItemKind::weather: return widen(weatherPrefix + item.text);
    case ItemKind::status:
        switch (Status(item.id)) {
        case Status::running: return std::format(L"Server running on {} (pid {})", serverPort, GetCurrentProcessId());
        case Status::stopped: return L"Start server";
        case Status::inUse: return std::format(L"Port {} is in use - open to retry", serverPort);
        case Status::elsewhere: return L"Open this folder in File Explorer to start the server";
        }
    default: throw std::logic_error(std::format("no name for item kind {}", int(item.kind)));
    }
}

std::string_view withoutPrefix(std::string_view text, std::string_view prefix) {
    if (text.size() >= prefix.size() && _strnicmp(text.data(), prefix.data(), prefix.size()) == 0) text.remove_prefix(prefix.size());
    return text;
}

std::wstring parsingName(const Item& item) {
    if (item.kind == ItemKind::chatMessage) return std::format(L"#{}", item.id);
    if (item.kind == ItemKind::status) return L"status";
    if (item.kind == ItemKind::time) return L"time";
    if (item.kind == ItemKind::weather) return L"weather";
    return itemName(item);
}

SFGAOF attributesOf(const Item& item) {
    switch (item.kind) {
    case ItemKind::world: return SFGAO_FOLDER | SFGAO_HASSUBFOLDER;
    case ItemKind::players:
    case ItemKind::chat: return SFGAO_FOLDER;
    case ItemKind::chunk: return SFGAO_FOLDER | SFGAO_CANDELETE;
    case ItemKind::block: return SFGAO_CANDELETE | SFGAO_CANRENAME;
    case ItemKind::player: return SFGAO_CANDELETE | SFGAO_CANRENAME;
    case ItemKind::herobrine: return SFGAO_CANDELETE | SFGAO_HIDDEN | SFGAO_SYSTEM;
    case ItemKind::chatInput:
    case ItemKind::time:
    case ItemKind::weather: return SFGAO_CANRENAME;
    case ItemKind::status: return Status(item.id) == Status::running ? SFGAO_CANDELETE : 0;
    default: return 0;
    }
}

std::vector<Item> children(const std::vector<Item>& path, bool opening) {
    Host& host = Host::get();
    if (!host.active()) return path.empty() ? std::vector<Item>{statusItem(Status::elsewhere)} : std::vector<Item>{};
    if (opening) host.open();
    if (path.empty()) {
        std::vector<Item> root = {{ItemKind::world}, {ItemKind::players}, {ItemKind::chat}};
        if (host.hasWorld()) {
            root.push_back(timeItem(host.server().time()));
            root.push_back(weatherItem(host.server().weather()));
        }
        root.push_back(statusItem(host.status()));
        return root;
    }
    if (!host.hasWorld()) return {};
    const Item& here = path.back();
    std::vector<Item> found;
    switch (here.kind) {
    case ItemKind::world:
        for (int chunkZ = -worldRadius; chunkZ <= worldRadius; chunkZ++)
            for (int chunkX = -worldRadius; chunkX <= worldRadius; chunkX++)
                if (host.world().blockCount(chunkX, chunkZ)) found.push_back(chunkItem(chunkX, chunkZ));
        break;
    case ItemKind::chunk:
        for (const Block& block : host.world().blocks(here.x, here.z)) found.push_back(blockItem(block.position, block.state));
        break;
    case ItemKind::players:
        for (const PlayerInfo& player : host.server().players()) found.push_back(playerItem(player));
        found.push_back({ItemKind::herobrine});
        break;
    case ItemKind::chat:
        found.push_back({ItemKind::chatInput});
        for (const ChatLine& line : host.server().chat()) found.push_back(chatItem(line));
        break;
    default: break;
    }
    return found;
}

std::optional<Item> parseChild(const std::vector<Item>& path, const std::wstring& name) {
    Host& host = Host::get();
    if (!path.empty() && path.back().kind == ItemKind::world) {
        for (int chunkZ = -worldRadius; chunkZ <= worldRadius; chunkZ++)
            for (int chunkX = -worldRadius; chunkX <= worldRadius; chunkX++)
                if (_wcsicmp(itemName(chunkItem(chunkX, chunkZ)).c_str(), name.c_str()) == 0) return chunkItem(chunkX, chunkZ);
        return std::nullopt;
    }
    if (!path.empty() && path.back().kind == ItemKind::chunk) {
        auto named = parseFileName(narrow(name));
        if (!named || floorDiv(named->position.x, 16) != path.back().x || floorDiv(named->position.z, 16) != path.back().z) return std::nullopt;
        if (!host.hasWorld()) return blockItem(named->position, named->state);
        auto block = host.world().block(named->position);
        if (!block) return std::nullopt;
        return blockItem(block->position, block->state);
    }
    for (const Item& child : children(path, false))
        if (_wcsicmp(parsingName(child).c_str(), name.c_str()) == 0) return child;
    return std::nullopt;
}

Value detail(const Item& item, std::size_t column) {
    Host& host = Host::get();
    if (column == 0) return text(itemName(item));
    switch (item.kind) {
    case ItemKind::block: {
        if (column == 1) return text(widen(displayName(item.state)));
        if (column == 3) return number(2);
        auto block = host.world().block({item.x, item.y, item.z});
        if (!block) return {};
        if (column == 2) return when(block->modified);
        return text(widen(block->author));
    }
    case ItemKind::chunk: {
        if (column == 1) return text(L"Chunk");
        std::size_t count = host.world().blockCount(item.x, item.z);
        if (column == 3) return number(count * 2);
        if (column == 4) return text(std::format(L"{} blocks", count));
        auto blocks = host.world().blocks(item.x, item.z);
        if (blocks.empty()) return {};
        return when(std::max_element(blocks.begin(), blocks.end(), [](const Block& a, const Block& b) { return a.modified < b.modified; })->modified);
    }
    case ItemKind::world:
        if (column == 1) return text(L"Folder");
        if (column == 3 && host.hasWorld()) return number(host.world().count() * 2);
        return {};
    case ItemKind::players:
    case ItemKind::chat: return column == 1 ? text(L"Folder") : Value{};
    case ItemKind::player: {
        if (column == 1) return text(L"Player");
        if (column != 4) return {};
        auto player = host.player(std::int32_t(item.id));
        if (!player) return {};
        int x = int(std::floor(player->x));
        int z = int(std::floor(player->z));
        return text(std::format(L"at {} {} {} in chunk {}, {}", x, int(std::floor(player->y)), z, floorDiv(x, 16), floorDiv(z, 16)));
    }
    case ItemKind::chatInput: return column == 1 ? text(L"Chat") : Value{};
    case ItemKind::herobrine: return column == 1 ? text(L"Player") : column == 4 ? text(L"Removed") : Value{};
    case ItemKind::time:
        if (column == 1) return text(L"Time");
        return column == 4 ? text(L"Rename to sunrise, day, noon, sunset, night, midnight or 0 to 23999") : Value{};
    case ItemKind::weather:
        if (column == 1) return text(L"Weather");
        return column == 4 ? text(L"Rename to clear, rain or thunder") : Value{};
    case ItemKind::chatMessage: return column == 1 ? text(L"Chat message") : Value{};
    case ItemKind::status:
        if (column == 1) return text(L"Server");
        if (column == 4 && Status(item.id) == Status::running) return text(widen(host.server().motd()));
        return {};
    default: return {};
    }
}

int compareValues(const Value& a, const Value& b) {
    if (a.type != b.type) return a.type < b.type ? -1 : 1;
    switch (a.type) {
    case Value::text: return StrCmpLogicalW(a.words.c_str(), b.words.c_str());
    case Value::number: return a.count < b.count ? -1 : a.count > b.count ? 1 : 0;
    case Value::time: return CompareFileTime(&a.stamp, &b.stamp);
    default: return 0;
    }
}

HRESULT toVariant(const Value& value, VARIANT* out) {
    switch (value.type) {
    case Value::text: return InitVariantFromString(value.words.c_str(), out);
    case Value::number: return InitVariantFromUInt64(value.count, out);
    case Value::time: return InitVariantFromFileTime(&value.stamp, out);
    default: return E_FAIL;
    }
}

HRESULT orderResult(int order) {
    return MAKE_HRESULT(SEVERITY_SUCCESS, 0, USHORT(short(order < 0 ? -1 : order > 0 ? 1 : 0)));
}

std::vector<Item> selected(IDataObject* data) {
    std::vector<Item> items;
    if (!data) return items;
    FORMATETC format = {CLIPFORMAT(RegisterClipboardFormatW(CFSTR_SHELLIDLIST)), nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
    STGMEDIUM medium;
    if (FAILED(data->GetData(&format, &medium))) return items;
    if (auto ida = static_cast<const CIDA*>(GlobalLock(medium.hGlobal))) {
        for (UINT i = 0; i < ida->cidl; i++) {
            auto child = reinterpret_cast<PCUIDLIST_RELATIVE>(reinterpret_cast<const BYTE*>(ida) + ida->aoffset[i + 1]);
            if (auto item = readChild(child)) items.push_back(*item);
        }
        GlobalUnlock(medium.hGlobal);
    }
    ReleaseStgMedium(&medium);
    return items;
}

void remove(const Item& item) {
    Host& host = Host::get();
    auto now = std::chrono::system_clock::now();
    switch (item.kind) {
    case ItemKind::block: host.world().set({item.x, item.y, item.z}, 0, explorerName, now); return;
    case ItemKind::chunk: host.world().clearChunk(item.x, item.z, explorerName, now); return;
    case ItemKind::player: host.server().kick(std::int32_t(item.id), "You were deleted by File Explorer"); return;
    case ItemKind::status:
        if (Status(item.id) == Status::running) host.setRunning(false);
        return;
    default: return;
    }
}

class EnumImpl : public IEnumIDList {
public:
    explicit EnumImpl(std::vector<Item> items) : items(std::move(items)) {}

    IFACEMETHODIMP QueryInterface(REFIID riid, void** out) override {
        static const QITAB table[] = {QITABENT(EnumImpl, IEnumIDList), {}};
        return QISearch(this, table, riid, out);
    }

    IFACEMETHODIMP Next(ULONG wanted, PITEMID_CHILD* out, ULONG* fetched) override {
        return guard("Enum::Next", [&] {
            ULONG got = 0;
            for (; got < wanted && next < items.size(); got++) out[got] = makeChild(items[next++]);
            if (fetched) *fetched = got;
            return got == wanted ? S_OK : S_FALSE;
        });
    }

    IFACEMETHODIMP Skip(ULONG count) override {
        next = std::min(items.size(), next + count);
        return S_OK;
    }

    IFACEMETHODIMP Reset() override {
        next = 0;
        return S_OK;
    }

    IFACEMETHODIMP Clone(IEnumIDList** out) override {
        *out = nullptr;
        return E_NOTIMPL;
    }

private:
    std::vector<Item> items;
    std::size_t next = 0;
};

class MenuImpl : public IContextMenuCB {
public:
    IFACEMETHODIMP QueryInterface(REFIID riid, void** out) override {
        static const QITAB table[] = {QITABENT(MenuImpl, IContextMenuCB), {}};
        return QISearch(this, table, riid, out);
    }

    IFACEMETHODIMP CallBack(IShellFolder*, HWND window, IDataObject* data, UINT message, WPARAM wParam, LPARAM lParam) override {
        return guard("Menu::CallBack", [&]() -> HRESULT {
            auto items = selected(data);
            bool status = items.size() == 1 && items[0].kind == ItemKind::status && Status(items[0].id) != Status::elsewhere;
            if (message == DFM_MERGECONTEXTMENU) {
                if (!status) return S_OK;
                bool running = Status(items[0].id) == Status::running;
                auto info = reinterpret_cast<QCMINFO*>(lParam);
                InsertMenuW(info->hmenu, info->indexMenu, MF_BYPOSITION | MF_STRING, info->idCmdFirst, running ? L"Stop server" : L"Start server");
                if (!running) SetMenuDefaultItem(info->hmenu, info->idCmdFirst, FALSE);
                info->idCmdFirst++;
                return S_OK;
            }
            if (message != DFM_INVOKECOMMAND) return E_NOTIMPL;
            if (UINT(wParam) == DFM_CMD_DELETE) {
                log(std::format("deleting {} items", items.size()));
                for (const Item& item : items) remove(item);
                if (std::any_of(items.begin(), items.end(), [](const Item& item) { return item.kind == ItemKind::herobrine; }))
                    MessageBoxW(window, L"You require permission from Herobrine to make changes to this file.", L"File Access Denied", MB_OK | MB_ICONERROR);
                return S_OK;
            }
            if (wParam == 0 && status) {
                Host::get().setRunning(Status(items[0].id) != Status::running);
                return S_OK;
            }
            return S_FALSE;
        });
    }
};

class ViewImpl : public IShellFolderViewCB, public IFolderViewSettings {
public:
    explicit ViewImpl(bool chunk) : chunk(chunk) {}

    IFACEMETHODIMP QueryInterface(REFIID riid, void** out) override {
        static const QITAB table[] = {QITABENT(ViewImpl, IShellFolderViewCB), QITABENT(ViewImpl, IFolderViewSettings), {}};
        return QISearch(this, table, riid, out);
    }

    IFACEMETHODIMP MessageSFVCB(UINT, WPARAM, LPARAM) override { return E_NOTIMPL; }
    IFACEMETHODIMP GetColumnPropertyList(REFIID, void** out) override {
        *out = nullptr;
        return E_NOTIMPL;
    }
    IFACEMETHODIMP GetGroupByProperty(PROPERTYKEY*, BOOL*) override { return E_NOTIMPL; }
    IFACEMETHODIMP GetIconSize(UINT*) override { return E_NOTIMPL; }
    IFACEMETHODIMP GetFolderFlags(FOLDERFLAGS*, FOLDERFLAGS*) override { return E_NOTIMPL; }
    IFACEMETHODIMP GetGroupSubsetCount(UINT*) override { return E_NOTIMPL; }

    IFACEMETHODIMP GetViewMode(FOLDERLOGICALVIEWMODE* mode) override {
        if (!chunk) return E_NOTIMPL;
        *mode = FLVM_DETAILS;
        return S_OK;
    }

    IFACEMETHODIMP GetSortColumns(SORTCOLUMN* sort, UINT size, UINT* used) override {
        if (!chunk || size < 1) return E_NOTIMPL;
        sort[0] = {PKEY_DateModified, SORT_DESCENDING};
        *used = 1;
        return S_OK;
    }

private:
    bool chunk;
};

}

FolderImpl::FolderImpl(Pidl self, std::vector<Item> path) : self(std::move(self)), path(std::move(path)) {}

IFACEMETHODIMP FolderImpl::QueryInterface(REFIID riid, void** out) {
    static const QITAB table[] = {
        QITABENT(FolderImpl, IShellFolder),
        QITABENT(FolderImpl, IShellFolder2),
        QITABENT(FolderImpl, IPersist),
        QITABENT(FolderImpl, IPersistFolder),
        QITABENT(FolderImpl, IPersistFolder2),
        {},
    };
    return QISearch(this, table, riid, out);
}

IFACEMETHODIMP FolderImpl::GetClassID(CLSID* out) {
    *out = clsid;
    return S_OK;
}

IFACEMETHODIMP FolderImpl::Initialize(PCIDLIST_ABSOLUTE pidl) {
    return guard("Folder::Initialize", [&] {
        self.reset(ILCloneFull(pidl));
        if (!self) throw std::bad_alloc();
        if (path.empty()) Host::get().setRoot(pidl);
        return S_OK;
    });
}

IFACEMETHODIMP FolderImpl::GetCurFolder(PIDLIST_ABSOLUTE* out) {
    *out = self ? ILCloneFull(reinterpret_cast<PCIDLIST_ABSOLUTE>(self.get())) : nullptr;
    return *out ? S_OK : E_FAIL;
}

IFACEMETHODIMP FolderImpl::ParseDisplayName(HWND hwnd, IBindCtx* context, PWSTR name, ULONG* eaten, PIDLIST_RELATIVE* out, ULONG* attributes) {
    return guard("Folder::ParseDisplayName", [&]() -> HRESULT {
        *out = nullptr;
        if (!name) return E_INVALIDARG;
        std::wstring_view whole = name;
        auto slash = whole.find(L'\\');
        std::wstring component(whole.substr(0, slash));
        if (auto child = parseChild(path, component)) {
            Pidl first(makeChild(*child));
            if (slash == std::wstring_view::npos || slash + 1 == whole.size()) {
                if (attributes) *attributes &= attributesOf(*child);
                if (eaten) *eaten = ULONG(whole.size());
                *out = reinterpret_cast<PIDLIST_RELATIVE>(first.release());
                return S_OK;
            }
            IShellFolder* inner;
            HRESULT hr = BindToObject(reinterpret_cast<PCUIDLIST_RELATIVE>(first.get()), context, IID_PPV_ARGS(&inner));
            if (FAILED(hr)) return hr;
            PIDLIST_RELATIVE rest = nullptr;
            std::wstring remainder(whole.substr(slash + 1));
            hr = inner->ParseDisplayName(hwnd, context, remainder.data(), nullptr, &rest, attributes);
            inner->Release();
            if (FAILED(hr)) return hr;
            *out = ILCombine(reinterpret_cast<PCIDLIST_ABSOLUTE>(first.get()), rest);
            CoTaskMemFree(rest);
            if (eaten) *eaten = ULONG(whole.size());
            return *out ? S_OK : E_OUTOFMEMORY;
        }
        return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
    });
}

IFACEMETHODIMP FolderImpl::EnumObjects(HWND, SHCONTF flags, IEnumIDList** out) {
    return guard("Folder::EnumObjects", [&] {
        *out = nullptr;
        std::vector<Item> wanted;
        for (Item& child : children(path, true)) {
            bool folder = isFolder(child.kind);
            if (child.kind == ItemKind::herobrine && !(flags & SHCONTF_INCLUDEHIDDEN)) continue;
            if ((folder && (flags & SHCONTF_FOLDERS)) || (!folder && (flags & SHCONTF_NONFOLDERS))) wanted.push_back(std::move(child));
        }
        *out = new Com<EnumImpl>(std::move(wanted));
        return S_OK;
    });
}

IFACEMETHODIMP FolderImpl::BindToObject(PCUIDLIST_RELATIVE pidl, IBindCtx* context, REFIID riid, void** out) {
    return guard("Folder::BindToObject", [&]() -> HRESULT {
        *out = nullptr;
        auto child = readChild(pidl);
        if (!child) return E_INVALIDARG;
        if (!isFolder(child->kind)) return E_NOINTERFACE;
        std::vector<Item> deeper = path;
        deeper.push_back(*child);
        Pidl first(ILCloneFirst(pidl));
        Pidl combined(ILCombine(reinterpret_cast<PCIDLIST_ABSOLUTE>(self.get()), reinterpret_cast<PCUIDLIST_RELATIVE>(first.get())));
        if (!combined) throw std::bad_alloc();
        auto folder = new Com<FolderImpl>(std::move(combined), std::move(deeper));
        PCUIDLIST_RELATIVE rest = ILNext(pidl);
        HRESULT hr = ILIsEmpty(rest) ? folder->QueryInterface(riid, out) : folder->BindToObject(rest, context, riid, out);
        folder->Release();
        return hr;
    });
}

IFACEMETHODIMP FolderImpl::BindToStorage(PCUIDLIST_RELATIVE, IBindCtx*, REFIID, void** out) {
    *out = nullptr;
    return E_NOTIMPL;
}

IFACEMETHODIMP FolderImpl::CompareIDs(LPARAM how, PCUIDLIST_RELATIVE a, PCUIDLIST_RELATIVE b) {
    return guard("Folder::CompareIDs", [&]() -> HRESULT {
        auto first = readChild(a);
        auto second = readChild(b);
        if (!first || !second) return E_INVALIDARG;
        int order;
        if (how & SHCIDS_CANONICALONLY) order = canonicalOrder(*first, *second);
        else if (how & SHCIDS_ALLFIELDS) order = fullOrder(*first, *second);
        else {
            auto column = std::size_t(how & SHCIDS_COLUMNMASK);
            order = column < columns.size() ? compareValues(detail(*first, column), detail(*second, column)) : 0;
            if (!order) order = canonicalOrder(*first, *second);
        }
        if (order) return orderResult(order);
        PCUIDLIST_RELATIVE restA = ILNext(a);
        PCUIDLIST_RELATIVE restB = ILNext(b);
        if (ILIsEmpty(restA) || ILIsEmpty(restB)) return orderResult(ILIsEmpty(restA) ? (ILIsEmpty(restB) ? 0 : -1) : 1);
        IShellFolder* inner;
        Pidl head(ILCloneFirst(a));
        HRESULT hr = BindToObject(reinterpret_cast<PCUIDLIST_RELATIVE>(head.get()), nullptr, IID_PPV_ARGS(&inner));
        if (FAILED(hr)) return hr;
        hr = inner->CompareIDs(how & ~SHCIDS_COLUMNMASK, restA, restB);
        inner->Release();
        return hr;
    });
}

IFACEMETHODIMP FolderImpl::CreateViewObject(HWND hwnd, REFIID riid, void** out) {
    return guard("Folder::CreateViewObject", [&]() -> HRESULT {
        *out = nullptr;
        if (riid == IID_IShellView) {
            bool chunk = !path.empty() && path.back().kind == ItemKind::chunk;
            auto callback = new Com<ViewImpl>(chunk);
            SFV_CREATE create = {sizeof create, this, nullptr, callback};
            HRESULT hr = SHCreateShellFolderView(&create, reinterpret_cast<IShellView**>(out));
            callback->Release();
            return hr;
        }
        if (riid == IID_IContextMenu) {
            DEFCONTEXTMENU menu = {hwnd, nullptr, reinterpret_cast<PCIDLIST_ABSOLUTE>(self.get()), this, 0, nullptr, nullptr, 0, nullptr};
            return SHCreateDefaultContextMenu(&menu, riid, out);
        }
        return E_NOINTERFACE;
    });
}

IFACEMETHODIMP FolderImpl::GetAttributesOf(UINT count, PCUITEMID_CHILD_ARRAY items, SFGAOF* inOut) {
    return guard("Folder::GetAttributesOf", [&]() -> HRESULT {
        if (!count) {
            *inOut &= SFGAO_FOLDER | SFGAO_HASSUBFOLDER;
            return S_OK;
        }
        SFGAOF allowed = ~SFGAOF(0);
        for (UINT i = 0; i < count; i++) {
            auto item = readChild(items[i]);
            if (!item) return E_INVALIDARG;
            allowed &= attributesOf(*item);
        }
        *inOut &= allowed;
        return S_OK;
    });
}

IFACEMETHODIMP FolderImpl::GetUIObjectOf(HWND hwnd, UINT count, PCUITEMID_CHILD_ARRAY items, REFIID riid, UINT*, void** out) {
    return guard("Folder::GetUIObjectOf", [&]() -> HRESULT {
        *out = nullptr;
        std::vector<Item> chosen;
        for (UINT i = 0; i < count; i++) {
            auto item = readChild(items[i]);
            if (!item) return E_INVALIDARG;
            chosen.push_back(*item);
        }
        bool folders = !chosen.empty() && std::all_of(chosen.begin(), chosen.end(), [](const Item& item) { return isFolder(item.kind); });
        auto folderSelf = reinterpret_cast<PCIDLIST_ABSOLUTE>(self.get());
        if (riid == IID_IContextMenu) {
            IQueryAssociations* associations = nullptr;
            if (folders) {
                ASSOCIATIONELEMENT element = {ASSOCCLASS_FOLDER, nullptr, nullptr};
                HRESULT hr = AssocCreateForClasses(&element, 1, IID_PPV_ARGS(&associations));
                if (FAILED(hr)) return hr;
            }
            auto callback = new Com<MenuImpl>();
            DEFCONTEXTMENU menu = {hwnd, callback, folderSelf, this, count, items, associations, 0, nullptr};
            HRESULT hr = SHCreateDefaultContextMenu(&menu, riid, out);
            callback->Release();
            if (associations) associations->Release();
            return hr;
        }
        if (riid == IID_IDataObject) return SHCreateDataObject(folderSelf, count, items, nullptr, riid, out);
        if (riid == IID_IQueryAssociations && folders) {
            ASSOCIATIONELEMENT element = {ASSOCCLASS_FOLDER, nullptr, nullptr};
            return AssocCreateForClasses(&element, 1, riid, out);
        }
        if (riid == IID_IExtractIconW && count == 1) return iconFor(chosen[0], riid, out);
        return E_NOINTERFACE;
    });
}

IFACEMETHODIMP FolderImpl::GetDisplayNameOf(PCUITEMID_CHILD pidl, SHGDNF flags, STRRET* out) {
    return guard("Folder::GetDisplayNameOf", [&]() -> HRESULT {
        auto item = readChild(pidl);
        if (!item) return E_INVALIDARG;
        if (flags & SHGDN_FORPARSING) {
            if (flags & SHGDN_INFOLDER) return toStrRet(parsingName(*item), out);
            PWSTR base;
            HRESULT hr = SHGetNameFromIDList(reinterpret_cast<PCIDLIST_ABSOLUTE>(self.get()), (flags & SHGDN_FORADDRESSBAR) ? SIGDN_DESKTOPABSOLUTEEDITING : SIGDN_DESKTOPABSOLUTEPARSING, &base);
            if (FAILED(hr)) return hr;
            std::wstring full = std::format(L"{}\\{}", base, parsingName(*item));
            CoTaskMemFree(base);
            return toStrRet(full, out);
        }
        if (flags & SHGDN_FOREDITING) {
            if (item->kind == ItemKind::block) return toStrRet(widen(typeName(item->state)), out);
            if (item->kind == ItemKind::chatInput) return toStrRet(L"", out);
            if (item->kind == ItemKind::time || item->kind == ItemKind::weather) return toStrRet(widen(item->text), out);
        }
        return toStrRet(itemName(*item), out);
    });
}

IFACEMETHODIMP FolderImpl::SetNameOf(HWND, PCUITEMID_CHILD pidl, PCWSTR name, SHGDNF, PITEMID_CHILD* out) {
    return guard("Folder::SetNameOf", [&]() -> HRESULT {
        if (out) *out = nullptr;
        auto item = readChild(pidl);
        if (!item || !name) return E_INVALIDARG;
        Host& host = Host::get();
        std::string text = narrow(name);
        if (item->kind == ItemKind::block) {
            auto state = parseType(text);
            if (!state) {
                log(std::format("rejected rename to {}", text));
                return HRESULT_FROM_WIN32(ERROR_INVALID_NAME);
            }
            Item renamed = blockItem({item->x, item->y, item->z}, *state);
            host.world().set({item->x, item->y, item->z}, *state, explorerName, std::chrono::system_clock::now());
            if (out) *out = makeChild(renamed);
            return S_OK;
        }
        if (item->kind == ItemKind::chatInput) {
            if (!text.empty() && text != narrow(chatPrompt)) host.server().say(std::format("[{}] {}", explorerName, text));
            if (out) *out = makeChild(*item);
            return S_OK;
        }
        if (item->kind == ItemKind::player) {
            switch (host.server().rename(std::int32_t(item->id), text)) {
            case RenameResult::renamed:
                if (out) *out = makeChild({ItemKind::player, 0, 0, 0, 0, item->id, text});
                return S_OK;
            case RenameResult::taken: return HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS);
            case RenameResult::invalid: return HRESULT_FROM_WIN32(ERROR_INVALID_NAME);
            case RenameResult::missing: return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
            }
        }
        if (item->kind == ItemKind::time) {
            auto ticks = parseTime(withoutPrefix(text, timePrefix));
            if (!ticks) return HRESULT_FROM_WIN32(ERROR_INVALID_NAME);
            host.setTime(*ticks);
            if (out) *out = makeChild(timeItem(*ticks));
            return S_OK;
        }
        if (item->kind == ItemKind::weather) {
            auto weather = parseWeather(withoutPrefix(text, weatherPrefix));
            if (!weather) return HRESULT_FROM_WIN32(ERROR_INVALID_NAME);
            host.setWeather(*weather);
            if (out) *out = makeChild(weatherItem(*weather));
            return S_OK;
        }
        return E_FAIL;
    });
}

IFACEMETHODIMP FolderImpl::GetDefaultSearchGUID(GUID*) {
    return E_NOTIMPL;
}

IFACEMETHODIMP FolderImpl::EnumSearches(IEnumExtraSearch** out) {
    *out = nullptr;
    return E_NOINTERFACE;
}

IFACEMETHODIMP FolderImpl::GetDefaultColumn(DWORD, ULONG* sort, ULONG* display) {
    *sort = 0;
    *display = 0;
    return S_OK;
}

IFACEMETHODIMP FolderImpl::GetDefaultColumnState(UINT column, SHCOLSTATEF* flags) {
    if (column >= columns.size()) return E_INVALIDARG;
    *flags = columnStates[column] | SHCOLSTATE_ONBYDEFAULT;
    return S_OK;
}

IFACEMETHODIMP FolderImpl::GetDetailsEx(PCUITEMID_CHILD pidl, const PROPERTYKEY* key, VARIANT* out) {
    return guard("Folder::GetDetailsEx", [&]() -> HRESULT {
        auto item = readChild(pidl);
        if (!item) return E_INVALIDARG;
        for (std::size_t column = 0; column < columns.size(); column++)
            if (IsEqualPropertyKey(*key, columns[column])) return toVariant(detail(*item, column), out);
        return E_FAIL;
    });
}

IFACEMETHODIMP FolderImpl::GetDetailsOf(PCUITEMID_CHILD pidl, UINT column, SHELLDETAILS* details) {
    return guard("Folder::GetDetailsOf", [&]() -> HRESULT {
        if (column >= columns.size()) return E_FAIL;
        details->fmt = column == 3 ? LVCFMT_RIGHT : LVCFMT_LEFT;
        details->cxChar = column == 0 ? 32 : 20;
        if (!pidl) return toStrRet(headers[column], &details->str);
        auto item = readChild(pidl);
        if (!item) return E_INVALIDARG;
        VARIANT value;
        HRESULT hr = toVariant(detail(*item, column), &value);
        if (FAILED(hr)) return toStrRet(L"", &details->str);
        PROPVARIANT property;
        hr = VariantToPropVariant(&value, &property);
        VariantClear(&value);
        if (FAILED(hr)) return hr;
        PWSTR formatted;
        hr = PSFormatForDisplayAlloc(columns[column], property, PDFF_DEFAULT, &formatted);
        PropVariantClear(&property);
        if (FAILED(hr)) return hr;
        hr = toStrRet(formatted, &details->str);
        CoTaskMemFree(formatted);
        return hr;
    });
}

IFACEMETHODIMP FolderImpl::MapColumnToSCID(UINT column, PROPERTYKEY* key) {
    if (column >= columns.size()) return E_FAIL;
    *key = columns[column];
    return S_OK;
}

}
