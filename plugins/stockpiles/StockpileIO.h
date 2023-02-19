#pragma once

#include "Core.h"
#include "Console.h"
#include "Export.h"
#include "PluginManager.h"

#include "DataFuncs.h"
#include "LuaTools.h"
#include "modules/Filesystem.h"

#include "../uicommon.h"

#include "StockpileUtils.h"
#include "StockpileSerializer.h"


#include "modules/Filesystem.h"
#include "modules/Gui.h"
#include "modules/Filesystem.h"

#include "df/world.h"
#include "df/world_data.h"

#include "DataDefs.h"
#include "df/plotinfost.h"
#include "df/building_stockpilest.h"
#include "df/stockpile_settings.h"
#include "df/global_objects.h"
#include "df/viewscreen_dwarfmodest.h"

#include <functional>
#include <vector>

using std::vector;
using std::string;
using std::endl;
using namespace DFHack;
using namespace df::enums;
using namespace google::protobuf;
using namespace dfstockpiles;


using df::building_stockpilest;
using std::placeholders::_1;

bool copystock_guard ( df::viewscreen *top );

command_result copystock ( color_ostream &out, vector <string> & parameters );

bool savestock_guard ( df::viewscreen *top );

bool loadstock_guard ( df::viewscreen *top );

command_result savestock ( color_ostream &out, vector <string> & parameters );

command_result loadstock ( color_ostream &out, vector <string> & parameters );

std::vector<std::string> list_dir ( const std::string &path, bool recursive = false );

std::vector<std::string> clean_dfstock_list ( const std::string &path );

int stockpiles_list_settings ( lua_State *L );

void stockpiles_load ( color_ostream &out, std::string filename );

void stockpiles_save ( color_ostream &out, std::string filename );