#include "folder.h"
#include "host.h"
#include "icons.h"

namespace mcx {

namespace {

class FactoryImpl : public IClassFactory {
public:
    virtual ~FactoryImpl() = default;

    IFACEMETHODIMP QueryInterface(REFIID riid, void** out) override {
        static const QITAB table[] = {QITABENT(FactoryImpl, IClassFactory), {}};
        return QISearch(this, table, riid, out);
    }

    IFACEMETHODIMP CreateInstance(IUnknown* outer, REFIID riid, void** out) override {
        return guard("Factory::CreateInstance", [&]() -> HRESULT {
            *out = nullptr;
            if (outer) return CLASS_E_NOAGGREGATION;
            auto folder = new Com<FolderImpl>();
            HRESULT hr = folder->QueryInterface(riid, out);
            folder->Release();
            return hr;
        });
    }

    IFACEMETHODIMP LockServer(BOOL lock) override {
        lock ? objects++ : objects--;
        return S_OK;
    }
};

std::wstring classKey() {
    return std::format(L"Software\\Classes\\CLSID\\{}", clsidText);
}

std::wstring junctionKey() {
    return std::format(L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\MyComputer\\NameSpace\\{}", clsidText);
}

void setValue(const std::wstring& key, const wchar_t* name, DWORD type, const void* data, DWORD size) {
    if (LSTATUS error = RegSetKeyValueW(HKEY_CURRENT_USER, key.c_str(), name, type, data, size)) throw std::runtime_error(std::format("RegSetKeyValue failed {}", error));
}

void setText(const std::wstring& key, const wchar_t* name, const std::wstring& value, DWORD type = REG_SZ) {
    setValue(key, name, type, value.c_str(), DWORD((value.size() + 1) * sizeof(wchar_t)));
}

void removeKey(const std::wstring& key) {
    LSTATUS error = RegDeleteTreeW(HKEY_CURRENT_USER, key.c_str());
    if (error && error != ERROR_FILE_NOT_FOUND) throw std::runtime_error(std::format("RegDeleteTree failed {}", error));
}

}

}

using namespace mcx;

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, void*) {
    if (reason == DLL_PROCESS_ATTACH) {
        mcx::module = instance;
        DisableThreadLibraryCalls(instance);
    }
    return TRUE;
}

STDAPI DllGetClassObject(REFCLSID requested, REFIID riid, void** out) {
    return guard("DllGetClassObject", [&]() -> HRESULT {
        *out = nullptr;
        if (requested != clsid) return CLASS_E_CLASSNOTAVAILABLE;
        auto factory = new Com<FactoryImpl>();
        HRESULT hr = factory->QueryInterface(riid, out);
        factory->Release();
        return hr;
    });
}

STDAPI DllCanUnloadNow() {
    return objects == 0 && !Host::everStarted() ? S_OK : S_FALSE;
}

STDAPI DllRegisterServer() {
    return guard("DllRegisterServer", [] {
        wchar_t path[MAX_PATH];
        GetModuleFileNameW(mcx::module, path, MAX_PATH);
        DWORD attributes = SFGAO_FOLDER | SFGAO_HASSUBFOLDER;
        setText(classKey(), nullptr, folderName);
        setText(classKey() + L"\\InprocServer32", nullptr, path);
        setText(classKey() + L"\\InprocServer32", L"ThreadingModel", L"Apartment");
        setText(classKey() + L"\\DefaultIcon", nullptr, stockIconLocation(SIID_SERVER));
        setValue(classKey() + L"\\ShellFolder", L"Attributes", REG_DWORD, &attributes, sizeof attributes);
        setText(junctionKey(), nullptr, folderName);
        SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
        return S_OK;
    });
}

STDAPI DllUnregisterServer() {
    return guard("DllUnregisterServer", [] {
        removeKey(junctionKey());
        removeKey(classKey());
        SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
        return S_OK;
    });
}
