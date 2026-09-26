#include "icons.h"

#include "host.h"

#include <mcx/blocks.h>
#include <mcx/sky.h>

#include <array>

#include <strsafe.h>

namespace mcx {

namespace {

const wchar_t colourIcons[] = L"McExplorer.colour";

HICON square(int size, std::uint32_t colour) {
    BITMAPINFO info{};
    info.bmiHeader = {sizeof(BITMAPINFOHEADER), size, -size, 1, 32, BI_RGB};
    void* bits;
    HBITMAP colourBitmap = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!colourBitmap) throw std::runtime_error("CreateDIBSection failed");
    HBITMAP mask = CreateBitmap(size, size, 1, 1, nullptr);
    auto pixels = static_cast<std::uint32_t*>(bits);
    int margin = std::max(1, size / 8);
    int edge = std::max(1, size / 16);
    auto shade = [](std::uint32_t rgb, int percent) {
        std::uint32_t out = 0;
        for (int shift = 0; shift < 24; shift += 8) out |= std::min<std::uint32_t>(255, ((rgb >> shift) & 0xFF) * std::uint32_t(percent) / 100) << shift;
        return out;
    };
    for (int y = 0; y < size; y++)
        for (int x = 0; x < size; x++) {
            bool inside = x >= margin && y >= margin && x < size - margin && y < size - margin;
            bool border = inside && (x < margin + edge || y < margin + edge || x >= size - margin - edge || y >= size - margin - edge);
            std::uint32_t rgb = border ? shade(colour, 65) : colour;
            pixels[y * size + x] = inside ? 0xFF000000 | rgb : 0;
        }
    ICONINFO icon = {TRUE, 0, 0, mask, colourBitmap};
    HICON result = CreateIconIndirect(&icon);
    DeleteObject(colourBitmap);
    DeleteObject(mask);
    if (!result) throw std::runtime_error("CreateIconIndirect failed");
    return result;
}

class ColourIconImpl : public IExtractIconW {
public:
    explicit ColourIconImpl(std::uint32_t colour) : colour(colour) {}
    virtual ~ColourIconImpl() = default;

    IFACEMETHODIMP QueryInterface(REFIID riid, void** out) override {
        static const QITAB table[] = {QITABENT(ColourIconImpl, IExtractIconW), {}};
        return QISearch(this, table, riid, out);
    }

    IFACEMETHODIMP GetIconLocation(UINT, PWSTR file, UINT size, int* index, UINT* flags) override {
        *index = int(colour);
        *flags = GIL_NOTFILENAME | GIL_PERCLASS;
        return StringCchCopyW(file, size, colourIcons);
    }

    IFACEMETHODIMP Extract(PCWSTR, UINT, HICON* largeIcon, HICON* smallIcon, UINT sizes) override {
        return guard("ColourIcon::Extract", [&] {
            if (largeIcon) *largeIcon = square(LOWORD(sizes), colour);
            if (smallIcon) *smallIcon = square(HIWORD(sizes), colour);
            return S_OK;
        });
    }

private:
    std::uint32_t colour;
};

std::uint32_t skyColour(std::int64_t ticks) {
    if (ticks >= 1000 && ticks < 12000) return ticks == noon ? 0x6FB7FF : 0x87CEEB;
    if (ticks >= 13000 && ticks < 23000) return ticks == 18000 ? 0x0B1030 : 0x2B3A67;
    return 0xE8743B;
}

std::optional<std::uint32_t> colourFor(const Item& item) {
    switch (item.kind) {
    case ItemKind::block: return colourOf(item.state);
    case ItemKind::time:
        if (auto ticks = parseTime(item.text)) return skyColour(*ticks);
        return std::nullopt;
    case ItemKind::weather:
        if (auto weather = parseWeather(item.text)) return std::array<std::uint32_t, 3>{0xFFD83D, 0x4A6FA5, 0x3A3A48}[std::size_t(*weather)];
        return std::nullopt;
    default: return std::nullopt;
    }
}

SHSTOCKICONID stockFor(const Item& item) {
    switch (item.kind) {
    case ItemKind::world: return SIID_WORLD;
    case ItemKind::players:
    case ItemKind::player:
    case ItemKind::herobrine: return SIID_USERS;
    case ItemKind::chat: return SIID_INFO;
    case ItemKind::chunk: return SIID_FOLDER;
    case ItemKind::status:
        switch (Status(item.id)) {
        case Status::running: return SIID_SERVER;
        case Status::stopped: return SIID_NETWORKCONNECT;
        case Status::inUse: return SIID_WARNING;
        default: return SIID_INFO;
        }
    default: return SIID_DOCNOASSOC;
    }
}

}

HICON blockIcon(int size, BlockState state) {
    return square(size, colourOf(state));
}

std::wstring stockIconLocation(SHSTOCKICONID id) {
    SHSTOCKICONINFO info = {sizeof info};
    if (FAILED(SHGetStockIconInfo(id, SHGSI_ICONLOCATION, &info))) throw std::runtime_error("SHGetStockIconInfo failed");
    return std::format(L"{},{}", info.szPath, info.iIcon);
}

HRESULT iconFor(const Item& item, REFIID riid, void** out) {
    if (auto colour = colourFor(item)) {
        auto icon = new Com<ColourIconImpl>(*colour);
        HRESULT hr = icon->QueryInterface(riid, out);
        icon->Release();
        return hr;
    }
    SHSTOCKICONINFO info = {sizeof info};
    HRESULT hr = SHGetStockIconInfo(stockFor(item), SHGSI_ICONLOCATION, &info);
    if (FAILED(hr)) return hr;
    IDefaultExtractIconInit* icon;
    hr = SHCreateDefaultExtractIcon(IID_PPV_ARGS(&icon));
    if (FAILED(hr)) return hr;
    hr = icon->SetNormalIcon(info.szPath, info.iIcon);
    if (SUCCEEDED(hr)) hr = icon->QueryInterface(riid, out);
    icon->Release();
    return hr;
}

}
