#pragma once

#include "pidl.h"

#include <vector>

namespace mcx {

class FolderImpl : public IShellFolder2, public IPersistFolder2 {
public:
    FolderImpl() = default;
    FolderImpl(Pidl self, std::vector<Item> path);
    virtual ~FolderImpl() = default;

    IFACEMETHODIMP QueryInterface(REFIID riid, void** out) override;

    IFACEMETHODIMP GetClassID(CLSID* out) override;
    IFACEMETHODIMP Initialize(PCIDLIST_ABSOLUTE pidl) override;
    IFACEMETHODIMP GetCurFolder(PIDLIST_ABSOLUTE* out) override;

    IFACEMETHODIMP ParseDisplayName(HWND hwnd, IBindCtx* context, PWSTR name, ULONG* eaten, PIDLIST_RELATIVE* out, ULONG* attributes) override;
    IFACEMETHODIMP EnumObjects(HWND hwnd, SHCONTF flags, IEnumIDList** out) override;
    IFACEMETHODIMP BindToObject(PCUIDLIST_RELATIVE pidl, IBindCtx* context, REFIID riid, void** out) override;
    IFACEMETHODIMP BindToStorage(PCUIDLIST_RELATIVE pidl, IBindCtx* context, REFIID riid, void** out) override;
    IFACEMETHODIMP CompareIDs(LPARAM how, PCUIDLIST_RELATIVE a, PCUIDLIST_RELATIVE b) override;
    IFACEMETHODIMP CreateViewObject(HWND hwnd, REFIID riid, void** out) override;
    IFACEMETHODIMP GetAttributesOf(UINT count, PCUITEMID_CHILD_ARRAY items, SFGAOF* inOut) override;
    IFACEMETHODIMP GetUIObjectOf(HWND hwnd, UINT count, PCUITEMID_CHILD_ARRAY items, REFIID riid, UINT* reserved, void** out) override;
    IFACEMETHODIMP GetDisplayNameOf(PCUITEMID_CHILD pidl, SHGDNF flags, STRRET* out) override;
    IFACEMETHODIMP SetNameOf(HWND hwnd, PCUITEMID_CHILD pidl, PCWSTR name, SHGDNF flags, PITEMID_CHILD* out) override;

    IFACEMETHODIMP GetDefaultSearchGUID(GUID* out) override;
    IFACEMETHODIMP EnumSearches(IEnumExtraSearch** out) override;
    IFACEMETHODIMP GetDefaultColumn(DWORD reserved, ULONG* sort, ULONG* display) override;
    IFACEMETHODIMP GetDefaultColumnState(UINT column, SHCOLSTATEF* flags) override;
    IFACEMETHODIMP GetDetailsEx(PCUITEMID_CHILD pidl, const PROPERTYKEY* key, VARIANT* out) override;
    IFACEMETHODIMP GetDetailsOf(PCUITEMID_CHILD pidl, UINT column, SHELLDETAILS* details) override;
    IFACEMETHODIMP MapColumnToSCID(UINT column, PROPERTYKEY* key) override;

private:
    Pidl self;
    std::vector<Item> path;
};

}
