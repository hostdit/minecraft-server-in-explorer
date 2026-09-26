#include <mcx/blocks.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <format>

namespace mcx {

namespace {

struct Variant {
    const char* name;
    const char* display;
};

using Family = std::vector<Variant>;

const Family colours = {{"white", "White"}, {"orange", "Orange"}, {"magenta", "Magenta"}, {"light_blue", "Light Blue"}, {"yellow", "Yellow"}, {"lime", "Lime"}, {"pink", "Pink"}, {"gray", "Gray"}, {"silver", "Light Gray"}, {"cyan", "Cyan"}, {"purple", "Purple"}, {"blue", "Blue"}, {"brown", "Brown"}, {"green", "Green"}, {"red", "Red"}, {"black", "Black"}};
const Family woods = {{"oak", "Oak"}, {"spruce", "Spruce"}, {"birch", "Birch"}, {"jungle", "Jungle"}, {"acacia", "Acacia"}, {"dark_oak", "Dark Oak"}};
const Family oldWoods = {{"oak", "Oak"}, {"spruce", "Spruce"}, {"birch", "Birch"}, {"jungle", "Jungle"}};
const Family newWoods = {{"acacia", "Acacia"}, {"dark_oak", "Dark Oak"}};
const Family stones = {{"stone", "Stone"}, {"granite", "Granite"}, {"smooth_granite", "Polished Granite"}, {"diorite", "Diorite"}, {"smooth_diorite", "Polished Diorite"}, {"andesite", "Andesite"}, {"smooth_andesite", "Polished Andesite"}};
const Family dirts = {{"dirt", "Dirt"}, {"coarse_dirt", "Coarse Dirt"}, {"podzol", "Podzol"}};
const Family sands = {{"sand", "Sand"}, {"red_sand", "Red Sand"}};
const Family sponges = {{"dry", "Sponge"}, {"wet", "Wet Sponge"}};
const Family sandstones = {{"default", "Sandstone"}, {"chiseled", "Chiseled Sandstone"}, {"smooth", "Smooth Sandstone"}};
const Family redSandstones = {{"default", "Red Sandstone"}, {"chiseled", "Chiseled Red Sandstone"}, {"smooth", "Smooth Red Sandstone"}};
const Family grasses = {{"dead_bush", "Shrub"}, {"tall_grass", "Grass"}, {"fern", "Fern"}};
const Family flowers = {{"poppy", "Poppy"}, {"blue_orchid", "Blue Orchid"}, {"allium", "Allium"}, {"houstonia", "Azure Bluet"}, {"red_tulip", "Red Tulip"}, {"orange_tulip", "Orange Tulip"}, {"white_tulip", "White Tulip"}, {"pink_tulip", "Pink Tulip"}, {"oxeye_daisy", "Oxeye Daisy"}};
const Family slabs = {{"stone", "Stone"}, {"sandstone", "Sandstone"}, {"wood_old", "Wooden"}, {"cobblestone", "Cobblestone"}, {"brick", "Bricks"}, {"stone_brick", "Stone Bricks"}, {"nether_brick", "Nether Brick"}, {"quartz", "Quartz"}};
const Family redSlabs = {{"red_sandstone", "Red Sandstone"}};
const Family eggs = {{"stone", "Stone"}, {"cobblestone", "Cobblestone"}, {"stone_brick", "Stone Brick"}, {"mossy_brick", "Mossy Stone Brick"}, {"cracked_brick", "Cracked Stone Brick"}, {"chiseled_brick", "Chiseled Stone Brick"}};
const Family bricks = {{"default", "Stone Bricks"}, {"mossy", "Mossy Stone Bricks"}, {"cracked", "Cracked Stone Bricks"}, {"chiseled", "Chiseled Stone Bricks"}};
const Family walls = {{"normal", "Cobblestone Wall"}, {"mossy", "Mossy Cobblestone Wall"}};
const Family quartzes = {{"default", "Block of Quartz"}, {"chiseled", "Chiseled Quartz Block"}, {"lines", "Pillar Quartz Block"}};
const Family prismarines = {{"prismarine", "Prismarine"}, {"prismarine_bricks", "Prismarine Bricks"}, {"dark_prismarine", "Dark Prismarine"}};
const Family plants = {{"sunflower", "Sunflower"}, {"syringa", "Lilac"}, {"double_grass", "Double Tallgrass"}, {"double_fern", "Large Fern"}, {"double_rose", "Rose Bush"}, {"paeonia", "Peony"}};

struct Kind {
    const char* name;
    const char* display;
    int mask = 0;
    const Family* family = nullptr;
};

const std::array<Kind, maxBlockId + 1> kinds = {{
    {"air", "Air"},
    {"stone", "{}", 7, &stones},
    {"grass", "Grass Block"},
    {"dirt", "{}", 3, &dirts},
    {"cobblestone", "Cobblestone"},
    {"planks", "{} Wood Planks", 7, &woods},
    {"sapling", "{} Sapling", 7, &woods},
    {"bedrock", "Bedrock"},
    {"flowing_water", "Flowing Water"},
    {"water", "Water"},
    {"flowing_lava", "Flowing Lava"},
    {"lava", "Lava"},
    {"sand", "{}", 1, &sands},
    {"gravel", "Gravel"},
    {"gold_ore", "Gold Ore"},
    {"iron_ore", "Iron Ore"},
    {"coal_ore", "Coal Ore"},
    {"log", "{} Wood", 3, &oldWoods},
    {"leaves", "{} Leaves", 3, &oldWoods},
    {"sponge", "{}", 1, &sponges},
    {"glass", "Glass"},
    {"lapis_ore", "Lapis Lazuli Ore"},
    {"lapis_block", "Lapis Lazuli Block"},
    {"dispenser", "Dispenser"},
    {"sandstone", "{}", 3, &sandstones},
    {"noteblock", "Note Block"},
    {"bed", "Bed"},
    {"golden_rail", "Powered Rail"},
    {"detector_rail", "Detector Rail"},
    {"sticky_piston", "Sticky Piston"},
    {"web", "Cobweb"},
    {"tallgrass", "{}", 3, &grasses},
    {"deadbush", "Dead Bush"},
    {"piston", "Piston"},
    {"piston_head", "Piston Head"},
    {"wool", "{} Wool", 15, &colours},
    {"piston_extension", "Moving Piston"},
    {"yellow_flower", "Dandelion"},
    {"red_flower", "{}", 15, &flowers},
    {"brown_mushroom", "Brown Mushroom"},
    {"red_mushroom", "Red Mushroom"},
    {"gold_block", "Block of Gold"},
    {"iron_block", "Block of Iron"},
    {"double_stone_slab", "Double {} Slab", 7, &slabs},
    {"stone_slab", "{} Slab", 7, &slabs},
    {"brick_block", "Bricks"},
    {"tnt", "TNT"},
    {"bookshelf", "Bookshelf"},
    {"mossy_cobblestone", "Moss Stone"},
    {"obsidian", "Obsidian"},
    {"torch", "Torch"},
    {"fire", "Fire"},
    {"mob_spawner", "Monster Spawner"},
    {"oak_stairs", "Oak Wood Stairs"},
    {"chest", "Chest"},
    {"redstone_wire", "Redstone Wire"},
    {"diamond_ore", "Diamond Ore"},
    {"diamond_block", "Block of Diamond"},
    {"crafting_table", "Crafting Table"},
    {"wheat", "Crops"},
    {"farmland", "Farmland"},
    {"furnace", "Furnace"},
    {"lit_furnace", "Burning Furnace"},
    {"standing_sign", "Sign"},
    {"wooden_door", "Oak Door"},
    {"ladder", "Ladder"},
    {"rail", "Rail"},
    {"stone_stairs", "Cobblestone Stairs"},
    {"wall_sign", "Wall Sign"},
    {"lever", "Lever"},
    {"stone_pressure_plate", "Stone Pressure Plate"},
    {"iron_door", "Iron Door"},
    {"wooden_pressure_plate", "Wooden Pressure Plate"},
    {"redstone_ore", "Redstone Ore"},
    {"lit_redstone_ore", "Glowing Redstone Ore"},
    {"unlit_redstone_torch", "Redstone Torch (off)"},
    {"redstone_torch", "Redstone Torch"},
    {"stone_button", "Stone Button"},
    {"snow_layer", "Snow"},
    {"ice", "Ice"},
    {"snow", "Snow Block"},
    {"cactus", "Cactus"},
    {"clay", "Clay"},
    {"reeds", "Sugar Canes"},
    {"jukebox", "Jukebox"},
    {"fence", "Oak Fence"},
    {"pumpkin", "Pumpkin"},
    {"netherrack", "Netherrack"},
    {"soul_sand", "Soul Sand"},
    {"glowstone", "Glowstone"},
    {"portal", "Nether Portal"},
    {"lit_pumpkin", "Jack o'Lantern"},
    {"cake", "Cake"},
    {"unpowered_repeater", "Redstone Repeater"},
    {"powered_repeater", "Powered Redstone Repeater"},
    {"stained_glass", "{} Stained Glass", 15, &colours},
    {"trapdoor", "Wooden Trapdoor"},
    {"monster_egg", "{} Monster Egg", 7, &eggs},
    {"stonebrick", "{}", 3, &bricks},
    {"brown_mushroom_block", "Brown Mushroom Block"},
    {"red_mushroom_block", "Red Mushroom Block"},
    {"iron_bars", "Iron Bars"},
    {"glass_pane", "Glass Pane"},
    {"melon_block", "Melon"},
    {"pumpkin_stem", "Pumpkin Stem"},
    {"melon_stem", "Melon Stem"},
    {"vine", "Vines"},
    {"fence_gate", "Oak Fence Gate"},
    {"brick_stairs", "Brick Stairs"},
    {"stone_brick_stairs", "Stone Brick Stairs"},
    {"mycelium", "Mycelium"},
    {"waterlily", "Lily Pad"},
    {"nether_brick", "Nether Brick"},
    {"nether_brick_fence", "Nether Brick Fence"},
    {"nether_brick_stairs", "Nether Brick Stairs"},
    {"nether_wart", "Nether Wart"},
    {"enchanting_table", "Enchantment Table"},
    {"brewing_stand", "Brewing Stand"},
    {"cauldron", "Cauldron"},
    {"end_portal", "End Portal"},
    {"end_portal_frame", "End Portal Frame"},
    {"end_stone", "End Stone"},
    {"dragon_egg", "Dragon Egg"},
    {"redstone_lamp", "Redstone Lamp"},
    {"lit_redstone_lamp", "Lit Redstone Lamp"},
    {"double_wooden_slab", "Double {} Wood Slab", 7, &woods},
    {"wooden_slab", "{} Wood Slab", 7, &woods},
    {"cocoa", "Cocoa"},
    {"sandstone_stairs", "Sandstone Stairs"},
    {"emerald_ore", "Emerald Ore"},
    {"ender_chest", "Ender Chest"},
    {"tripwire_hook", "Tripwire Hook"},
    {"tripwire", "Tripwire"},
    {"emerald_block", "Block of Emerald"},
    {"spruce_stairs", "Spruce Wood Stairs"},
    {"birch_stairs", "Birch Wood Stairs"},
    {"jungle_stairs", "Jungle Wood Stairs"},
    {"command_block", "Command Block"},
    {"beacon", "Beacon"},
    {"cobblestone_wall", "{}", 1, &walls},
    {"flower_pot", "Flower Pot"},
    {"carrots", "Carrots"},
    {"potatoes", "Potatoes"},
    {"wooden_button", "Wooden Button"},
    {"skull", "Head"},
    {"anvil", "Anvil"},
    {"trapped_chest", "Trapped Chest"},
    {"light_weighted_pressure_plate", "Weighted Pressure Plate (Light)"},
    {"heavy_weighted_pressure_plate", "Weighted Pressure Plate (Heavy)"},
    {"unpowered_comparator", "Redstone Comparator"},
    {"powered_comparator", "Powered Redstone Comparator"},
    {"daylight_detector", "Daylight Sensor"},
    {"redstone_block", "Block of Redstone"},
    {"quartz_ore", "Nether Quartz Ore"},
    {"hopper", "Hopper"},
    {"quartz_block", "{}", 7, &quartzes},
    {"quartz_stairs", "Quartz Stairs"},
    {"activator_rail", "Activator Rail"},
    {"dropper", "Dropper"},
    {"stained_hardened_clay", "{} Stained Clay", 15, &colours},
    {"stained_glass_pane", "{} Stained Glass Pane", 15, &colours},
    {"leaves2", "{} Leaves", 3, &newWoods},
    {"log2", "{} Wood", 3, &newWoods},
    {"acacia_stairs", "Acacia Wood Stairs"},
    {"dark_oak_stairs", "Dark Oak Wood Stairs"},
    {"slime", "Slime Block"},
    {"barrier", "Barrier"},
    {"iron_trapdoor", "Iron Trapdoor"},
    {"prismarine", "{}", 3, &prismarines},
    {"sea_lantern", "Sea Lantern"},
    {"hay_block", "Hay Bale"},
    {"carpet", "{} Carpet", 15, &colours},
    {"hardened_clay", "Hardened Clay"},
    {"coal_block", "Block of Coal"},
    {"packed_ice", "Packed Ice"},
    {"double_plant", "{}", 7, &plants},
    {"standing_banner", "Banner"},
    {"wall_banner", "Wall Banner"},
    {"daylight_detector_inverted", "Inverted Daylight Sensor"},
    {"red_sandstone", "{}", 3, &redSandstones},
    {"red_sandstone_stairs", "Red Sandstone Stairs"},
    {"double_stone_slab2", "Double {} Slab", 7, &redSlabs},
    {"stone_slab2", "{} Slab", 7, &redSlabs},
    {"spruce_fence_gate", "Spruce Fence Gate"},
    {"birch_fence_gate", "Birch Fence Gate"},
    {"jungle_fence_gate", "Jungle Fence Gate"},
    {"dark_oak_fence_gate", "Dark Oak Fence Gate"},
    {"acacia_fence_gate", "Acacia Fence Gate"},
    {"spruce_fence", "Spruce Fence"},
    {"birch_fence", "Birch Fence"},
    {"jungle_fence", "Jungle Fence"},
    {"dark_oak_fence", "Dark Oak Fence"},
    {"acacia_fence", "Acacia Fence"},
    {"spruce_door", "Spruce Door"},
    {"birch_door", "Birch Door"},
    {"jungle_door", "Jungle Door"},
    {"acacia_door", "Acacia Door"},
    {"dark_oak_door", "Dark Oak Door"},
}};

const std::array<std::pair<int, int>, 25> itemBlocks = {{
    {295, 59}, {323, 63}, {324, 64}, {330, 71}, {331, 55}, {338, 83}, {354, 92}, {355, 26}, {356, 93},
    {361, 104}, {362, 105}, {372, 115}, {379, 117}, {380, 118}, {390, 140}, {391, 141}, {392, 142},
    {397, 144}, {404, 149}, {425, 176}, {427, 193}, {428, 194}, {429, 195}, {430, 196}, {431, 197},
}};

const Kind& kindOf(BlockState value) {
    int id = blockId(value);
    if (id > maxBlockId) throw std::out_of_range(std::format("block id {} is not in 1.8", id));
    return kinds[id];
}

const Variant* variantOf(const Kind& kind, BlockState value) {
    if (!kind.family) return nullptr;
    auto index = std::size_t(blockMeta(value) & kind.mask);
    return index < kind.family->size() ? &(*kind.family)[index] : nullptr;
}

std::string lower(std::string_view text) {
    std::string out(text);
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return out;
}

std::string_view trim(std::string_view text) {
    auto start = text.find_first_not_of(" \t");
    if (start == std::string_view::npos) return {};
    return text.substr(start, text.find_last_not_of(" \t") - start + 1);
}

std::optional<int> number(std::string_view text) {
    int value;
    auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc() || end != text.data() + text.size() || text.empty()) return std::nullopt;
    return value;
}

std::optional<int> variantIndex(const Kind& kind, std::string_view text) {
    if (!kind.family || text.empty()) return std::nullopt;
    auto size = int(kind.family->size());
    if (auto value = number(text)) return *value >= 0 && *value < size ? value : std::nullopt;
    for (int index = 0; index < size; index++)
        if (text == (*kind.family)[index].name) return index;
    return std::nullopt;
}

}

std::string typeName(BlockState value) {
    const Kind& kind = kindOf(value);
    const Variant* variant = variantOf(kind, value);
    if (!variant || variant == &kind.family->front()) return kind.name;
    return std::format("{}:{}", kind.name, variant->name);
}

std::string displayName(BlockState value) {
    const Kind& kind = kindOf(value);
    if (!kind.family) return kind.display;
    const Variant* variant = variantOf(kind, value);
    return std::vformat(kind.display, std::make_format_args((variant ? variant : &kind.family->front())->display));
}

std::optional<BlockState> parseType(std::string_view text) {
    std::string clean = lower(trim(text.substr(0, text.find(" @ "))));
    std::string_view name = clean;
    if (name.starts_with("minecraft:")) name.remove_prefix(10);
    auto colon = name.find(':');
    std::string_view base = name.substr(0, colon);
    for (int id = 1; id <= maxBlockId; id++) {
        if (base != kinds[id].name) continue;
        if (colon == std::string_view::npos) return state(id, 0);
        auto index = variantIndex(kinds[id], name.substr(colon + 1));
        if (!index) return std::nullopt;
        return state(id, *index);
    }
    return std::nullopt;
}

std::string fileName(BlockState value, Position position) {
    return std::format("{} @ {} {} {}", typeName(value), position.x, position.y, position.z);
}

std::optional<NamedBlock> parseFileName(std::string_view text) {
    auto at = text.find(" @ ");
    if (at == std::string_view::npos) return std::nullopt;
    auto type = parseType(text.substr(0, at));
    if (!type) return std::nullopt;
    std::string_view rest = text.substr(at + 3);
    std::array<int, 3> coordinates;
    for (int i = 0; i < 3; i++) {
        auto space = rest.find(' ');
        if ((i < 2) == (space == std::string_view::npos)) return std::nullopt;
        auto value = number(rest.substr(0, space));
        if (!value) return std::nullopt;
        coordinates[i] = *value;
        rest = i < 2 ? rest.substr(space + 1) : std::string_view{};
    }
    return NamedBlock{*type, {coordinates[0], coordinates[1], coordinates[2]}};
}

std::uint32_t colourOf(BlockState value) {
    static const std::array<std::uint32_t, maxBlockId + 1> base = {
        0x000000, 0x7D7D7D, 0x5D9B3A, 0x866043, 0x7A7A7A, 0x9C7F4E, 0x4A7A2A, 0x3C3C3C, 0x2F5AFF, 0x2F5AFF,
        0xD4590B, 0xD4590B, 0xDBD3A0, 0x857E7C, 0x8F8C7D, 0x877F79, 0x737373, 0x6B5433, 0x3D8A1E, 0xC3C24A,
        0xC0DDE3, 0x667087, 0x1D47A6, 0x6E6E6E, 0xD8CE9B, 0x6A4535, 0x8E1616, 0x9A8349, 0x7B6A5A, 0x8FA06B,
        0xDCDCDC, 0x5E9A3B, 0x946428, 0x9C8866, 0x9C8866, 0xE9ECEC, 0x9C8866, 0xF1F902, 0xC21E1E, 0x916D55,
        0xE21212, 0xF9EC4F, 0xDCDCDC, 0xA8A8A8, 0xA8A8A8, 0x96604F, 0xDB441A, 0x6C5835, 0x677A5E, 0x14121E,
        0xFFD84A, 0xE0AE15, 0x1B2A35, 0x9C7F4E, 0xA0782E, 0xAA0000, 0x7E9A9A, 0x62DBD5, 0x7A5A36, 0xA3993D,
        0x5E3E23, 0x6E6E6E, 0x7E6E5E, 0x9C7F4E, 0x87683A, 0x7C6034, 0x7E7362, 0x7A7A7A, 0x9C7F4E, 0x6E5A3C,
        0x7D7D7D, 0xC0C0C0, 0x9C7F4E, 0x846B6B, 0x9F6B6B, 0x5E1B0E, 0xFF2A0A, 0x7D7D7D, 0xF0FBFB, 0x7DADFF,
        0xF0FBFB, 0x0D6B18, 0x9FA4B1, 0x94C065, 0x6A4535, 0x9C7F4E, 0xC07615, 0x6F3634, 0x544033, 0xFBDA74,
        0x5A0ABF, 0xE3A22A, 0xE4CDCE, 0xA0A0A0, 0xB05050, 0xF4F5F5, 0x7E5D2D, 0x7D7D7D, 0x7A7A7A, 0x8D6A53,
        0xB62A27, 0x6D6C6A, 0xC0DDE3, 0x97A124, 0x6E9A2B, 0x6E9A2B, 0x3F7F1E, 0x9C7F4E, 0x96604F, 0x7A7A7A,
        0x6F6369, 0x208030, 0x2C161A, 0x2C161A, 0x2C161A, 0x8A1C1E, 0x6D2A2A, 0x7B6A4A, 0x3A3A3A, 0x0B0B1A,
        0x5B7962, 0xDDDFA5, 0x0C0910, 0x5F3B20, 0xC39C5E, 0x9C7F4E, 0x9C7F4E, 0x915B2B, 0xD8CE9B, 0x6D8074,
        0x2D3E3F, 0x7C7C7C, 0xDCDCDC, 0x51D975, 0x684E2F, 0xC4B27B, 0x9F714A, 0xB5886D, 0x75DDD7, 0x7A7A7A,
        0x7C4535, 0x3F9A2A, 0x4E9A2A, 0x9C7F4E, 0xC8C8C8, 0x444444, 0xA0782E, 0xF9EC4F, 0xDCDCDC, 0xA0A0A0,
        0xB05050, 0x837B63, 0xAB1B09, 0x7D5550, 0x3E3E3E, 0xECE6DF, 0xECE6DF, 0x7B5A4A, 0x6E6E6E, 0xBB9A8C,
        0xF4F5F5, 0x3D7A1E, 0x676157, 0xAD5D32, 0x3E2912, 0x78C865, 0xE33535, 0xC8C8C8, 0x63A38F, 0xACC8BE,
        0xA68A0C, 0xE9ECEC, 0x965C42, 0x121212, 0x8CB4FA, 0x5E9A3B, 0xE6E6E6, 0xE6E6E6, 0x6C7780, 0xA95821,
        0xA95821, 0xA95821, 0xA95821, 0x684E2F, 0xC4B27B, 0x9F714A, 0x3E2912, 0xAD5D32, 0x684E2F, 0xC4B27B,
        0x9F714A, 0x3E2912, 0xAD5D32, 0x684E2F, 0xC4B27B, 0x9F714A, 0xAD5D32, 0x3E2912,
    };
    static const std::array<std::uint32_t, 16> dyes = {0xE9ECEC, 0xF07613, 0xBD44B3, 0x3AAFD9, 0xF8C627, 0x70B919, 0xED8DAC, 0x3E4447, 0x8E8E86, 0x158991, 0x792AAC, 0x35399D, 0x724728, 0x546D1B, 0xA12722, 0x141519};
    static const std::array<std::uint32_t, 7> stoneColours = {0x7D7D7D, 0x9A6C5A, 0xA77562, 0xBCBCBC, 0xC5C5C7, 0x888888, 0x848685};
    static const std::array<std::uint32_t, 6> woodColours = {0x9C7F4E, 0x684E2F, 0xC4B27B, 0x9F714A, 0xAD5D32, 0x3E2912};
    static const std::array<std::uint32_t, 4> logs = {0x6B5433, 0x2E1D0C, 0xD8D8D3, 0x564419};
    auto mix = [](std::uint32_t colour, std::uint32_t with, int percent) {
        std::uint32_t out = 0;
        for (int shift = 0; shift < 24; shift += 8) out |= ((((colour >> shift) & 0xFF) * (100 - percent) + ((with >> shift) & 0xFF) * percent) / 100) << shift;
        return out;
    };
    int id = blockId(value);
    if (id > maxBlockId) throw std::out_of_range(std::format("block id {} is not in 1.8", id));
    const Kind& kind = kinds[id];
    auto index = std::size_t(blockMeta(value) & kind.mask);
    bool named = kind.family && index < kind.family->size();
    switch (id) {
    case 1: return named ? stoneColours[index] : base[id];
    case 3: return std::array<std::uint32_t, 3>{0x866043, 0x77553B, 0x5A3F1C}[named ? index : 0];
    case 5:
    case 125:
    case 126: return named ? woodColours[index] : base[id];
    case 12: return index ? 0xA95821 : base[id];
    case 17: return named ? logs[index] : base[id];
    case 162: return index ? 0x3E2912 : base[id];
    case 35:
    case 171: return dyes[index];
    case 95:
    case 160: return mix(dyes[index], 0xFFFFFF, 40);
    case 159: return mix(dyes[index], 0x965C42, 45);
    case 98: return std::array<std::uint32_t, 4>{0x7A7A7A, 0x6E7B5E, 0x767676, 0x777777}[index];
    case 168: return std::array<std::uint32_t, 3>{0x63A38F, 0x63AB9E, 0x335B4B}[named ? index : 0];
    default: return base[id];
    }
}

std::optional<BlockState> blockForItem(int itemId, int damage) {
    if (itemId >= 1 && itemId <= maxBlockId) {
        const Kind& kind = kinds[itemId];
        int meta = kind.family && (damage & kind.mask) < int(kind.family->size()) ? damage & kind.mask : 0;
        return state(itemId, meta);
    }
    for (auto [item, block] : itemBlocks)
        if (item == itemId) return state(block, 0);
    return std::nullopt;
}

}
