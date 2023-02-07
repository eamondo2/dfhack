// Includes from automelt
#include "Debug.h"
#include "LuaTools.h"
#include "PluginManager.h"
#include "TileTypes.h"

#include "modules/Buildings.h"
#include "modules/Maps.h"
#include "modules/Items.h"
#include "modules/World.h"
#include "modules/Designations.h"
#include "modules/Persistence.h"
#include "modules/Units.h"
#include "modules/Screen.h"
#include "modules/Gui.h"

#include "df/world.h"
#include "df/building.h"
#include "df/world_raws.h"
#include "df/building_def.h"
#include "df/viewscreen_dwarfmodest.h"
#include "df/building_stockpilest.h"
#include "df/plotinfost.h"
#include "df/item_quality.h"

// Includes from original stockpiles
#include "Core.h"
#include "Console.h"
#include "Export.h"
#include "DataFuncs.h"
#include "DataDefs.h"

#include "StockpileSerializer.h"
#include "StockpileUtils.h"

#include "df/world_data.h"
#include "df/stockpile_settings.h"
#include "df/global_objects.h"

#include "modules/Filesystem.h"
#include "modules/Gui.h"
#include "modules/Filesystem.h"

// Generic includes
#include <functional>
#include <vector>

#include "StockpileIO.h"

#include <map>
#include <unordered_map>

using df::building_stockpilest;
using std::map;
using std::multimap;
using std::pair;
using std::string;
using std::unordered_map;
using std::vector;

using namespace DFHack;
using namespace df::enums;

using std::endl;
using namespace google::protobuf;
using namespace dfstockpiles;

DFHACK_PLUGIN("stockpiles");
DFHACK_PLUGIN_IS_ENABLED(is_enabled);
REQUIRE_GLOBAL(gps);
REQUIRE_GLOBAL(world);
REQUIRE_GLOBAL(cursor);
REQUIRE_GLOBAL(plotinfo);
REQUIRE_GLOBAL(selection_rect);

namespace DFHack
{
    DBG_DECLARE(stockpiles, status, DebugCategory::LINFO);
    DBG_DECLARE(stockpiles, cycle, DebugCategory::LINFO);
    DBG_DECLARE(stockpiles, perf, DebugCategory::LINFO);
}

static const string CONFIG_KEY = string(plugin_name) + "/config";
static const string STOCKPILE_CONFIG_KEY_PREFIX = string(plugin_name) + "/stockpile/";
static PersistentDataItem config;


static unordered_map<int32_t, PersistentDataItem> watched_stockpiles;


enum StockpileConfigValues
{
    STOCKPILE_CONFIG_ID = 0,
    STOCKPILE_CONFIG_MONITORED = 1,
    STOCKPILE_CONFIG_MELT = 2,
    STOCKPILE_CONFIG_TRADE =3,

};

static int get_config_val(PersistentDataItem &c, int index)
{
    if (!c.isValid())
        return -1;
    return c.ival(index);
}

static bool get_config_bool(PersistentDataItem &c, int index)
{
    return get_config_val(c, index) == 1;
}

static void set_config_val(PersistentDataItem &c, int index, int value)
{
    if (c.isValid())
        c.ival(index) = value;
}

static void set_config_bool(PersistentDataItem &c, int index, bool value)
{
    set_config_val(c, index, value ? 1 : 0);
}

static PersistentDataItem &ensure_stockpile_config(color_ostream &out, int id)
{
    DEBUG(cycle,out).print("ensuring stockpile config id=%d\n", id);
    if (watched_stockpiles.count(id)){
        DEBUG(cycle,out).print("stockpile exists in watched_indices\n");
        return watched_stockpiles[id];
    }

    string keyname = STOCKPILE_CONFIG_KEY_PREFIX + int_to_string(id);
    DEBUG(status,out).print("creating new persistent key for stockpile %d\n", id);
    watched_stockpiles.emplace(id, World::GetPersistentData(keyname, NULL));
    return watched_stockpiles[id];
}

static void remove_stockpile_config(color_ostream &out, int id)
{
    if (!watched_stockpiles.count(id))
        return;
    DEBUG(status, out).print("removing persistent key for stockpile %d\n", id);
    World::DeletePersistentData(watched_stockpiles[id]);
    watched_stockpiles.erase(id);
}

static void validate_stockpile_configs(color_ostream &out)
{
    for (auto &c : watched_stockpiles) {
        int id = get_config_val(c.second, STOCKPILE_CONFIG_ID);
        if (!df::building::find(id)){
            remove_stockpile_config(out, id);
        }
    }
}

static const int32_t CYCLE_TICKS = 1200;
static int32_t cycle_timestamp = 0; // world->frame_counter at last cycle

static command_result do_command(color_ostream &out, vector<string> &parameters);
static int32_t do_cycle(color_ostream &out);

DFhackCExport command_result plugin_init(color_ostream &out, std::vector<PluginCommand> &commands)
{
    DEBUG(status, out).print("initializing %s\n", plugin_name);

    // provide a configuration interface for the plugin
    commands.push_back(PluginCommand(
        plugin_name,
        "Manage stockpile behavior",
        do_command));
    // TODO: Implement these via LUA and cb to c++ bindings
    // commands.push_back(PluginCommand(
    //     "copystock",
    //     "Copy stockpile under cursor.",
    //     copystock,
    //     copystock_guard));
    // commands.push_back(PluginCommand(
    //     "savestock",
    //     "Save the active stockpile's settings to a file.",
    //     savestock,
    //     savestock_guard));
    // commands.push_back(PluginCommand(
    //     "loadstock",
    //     "Load and apply stockpile settings from a file.",
    //     loadstock,
    //     loadstock_guard));

    return CR_OK;
}

DFhackCExport command_result plugin_enable(color_ostream &out, bool enable)
{
    if (enable != is_enabled)
    {
        is_enabled = enable;
        DEBUG(status, out).print("%s from the API; persisting\n", is_enabled ? "enabled" : "disabled");
    }
    else
    {
        DEBUG(status, out).print("%s from the API, but already %s; no action\n", is_enabled ? "enabled" : "disabled", is_enabled ? "enabled" : "disabled");
    }
    return CR_OK;
}

DFhackCExport command_result plugin_shutdown(color_ostream &out)
{
    DEBUG(status, out).print("shutting down %s\n", plugin_name);

    return CR_OK;
}

DFhackCExport command_result plugin_load_data(color_ostream &out)
{
    config = World::GetPersistentData(CONFIG_KEY);

    if (!config.isValid())
    {
        DEBUG(status, out).print("no config found in this save; initializing\n");
        config = World::AddPersistentData(CONFIG_KEY);
    }

    DEBUG(status, out).print("loading persisted enabled state: %s\n", is_enabled ? "true" : "false");
    vector<PersistentDataItem> loaded_persist_data;
    World::GetPersistentData(&loaded_persist_data, STOCKPILE_CONFIG_KEY_PREFIX, true);
    watched_stockpiles.clear();
    const size_t num_watched_stockpiles = loaded_persist_data.size();
    for (size_t idx = 0; idx < num_watched_stockpiles; ++idx)
    {
        auto &c = loaded_persist_data[idx];
        watched_stockpiles.emplace(get_config_val(c, STOCKPILE_CONFIG_ID), c);
    }
    validate_stockpile_configs(out);

    return CR_OK;
}

DFhackCExport command_result plugin_onstatechange(color_ostream &out, state_change_event event)
{
    if (event == DFHack::SC_WORLD_UNLOADED)
    {
        if (is_enabled)
        {
            DEBUG(status, out).print("world unloaded; disabling %s\n", plugin_name);
            is_enabled = false;
        }
    }

    return CR_OK;
}

DFhackCExport command_result plugin_onupdate(color_ostream &out)
{
    if (!Core::getInstance().isWorldLoaded())
        return CR_OK;
    if (is_enabled && world->frame_counter - cycle_timestamp >= CYCLE_TICKS)
    {
        int32_t marked = do_cycle(out);
        if (0 < marked)
            out.print("automelt: marked %d item(s) for melting\n", marked);
    }
    return CR_OK;
}

static bool call_stockpiles_lua(color_ostream *out, const char *fn_name,
        int nargs = 0, int nres = 0,
        Lua::LuaLambda && args_lambda = Lua::DEFAULT_LUA_LAMBDA,
        Lua::LuaLambda && res_lambda = Lua::DEFAULT_LUA_LAMBDA) {
    DEBUG(status).print("calling stockpiles lua function: '%s'\n", fn_name);

    CoreSuspender guard;

    auto L = Lua::Core::State;
    Lua::StackUnwinder top(L);

    if (!out)
        out = &Core::getInstance().getConsole();

    return Lua::CallLuaModuleFunction(*out, L, "plugins.stockpiles", fn_name,
            nargs, nres,
            std::forward<Lua::LuaLambda&&>(args_lambda),
            std::forward<Lua::LuaLambda&&>(res_lambda));
}

DFHACK_PLUGIN_LUA_FUNCTIONS
{
    DFHACK_LUA_FUNCTION ( stockpiles_load ),
    DFHACK_LUA_FUNCTION ( stockpiles_save ),
    DFHACK_LUA_END
};

DFHACK_PLUGIN_LUA_COMMANDS
{
    DFHACK_LUA_COMMAND ( stockpiles_list_settings ),
    DFHACK_LUA_END
};