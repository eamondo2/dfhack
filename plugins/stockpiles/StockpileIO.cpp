#include "Core.h"
#include "Console.h"
#include "Export.h"
#include "PluginManager.h"
#include "DataFuncs.h"
#include "LuaTools.h"
#include "DataDefs.h"
#include "Debug.h"

#include "modules/Filesystem.h"
#include "modules/Filesystem.h"
#include "modules/Gui.h"
#include "modules/Filesystem.h"

#include "df/world.h"
#include "df/world_data.h"
#include "df/plotinfost.h"
#include "df/building_stockpilest.h"
#include "df/stockpile_settings.h"
#include "df/global_objects.h"
#include "df/viewscreen_dwarfmodest.h"

#include "StockpileUtils.h"
#include "StockpileSerializer.h"

//  stl
#include <functional>
#include <vector>

#include "StockpileIO.h"

#include "plugin.h"

using std::vector;
using std::string;
using std::endl;
using namespace DFHack;
using namespace df::enums;
using namespace google::protobuf;
using namespace dfstockpiles;


using df::building_stockpilest;
using std::placeholders::_1;

/**
 * 
 * TODO:
 * change read/write dir to dfhack-config/stockpiles
*/


command_result copystock ( color_ostream &out, vector <string> & parameters );

command_result savestock ( color_ostream &out, vector <string> & parameters );

command_result loadstock ( color_ostream &out, vector <string> & parameters );

bool copystock_guard ( color_ostream &out, df::viewscreen *top )
{
     if ( !Gui::dwarfmode_hotkey ( top ) )
        return false;

    df::building *cur_selected = Gui::getSelectedBuilding(out);
    
    if (!cur_selected || cur_selected->getType() != building_type::Stockpile) {
        return false;
    }

    return true;
}

bool savestock_guard ( color_ostream &out, df::viewscreen *top )
{

    if ( !Gui::dwarfmode_hotkey ( top ) )
        return false;

    df::building *cur_selected = Gui::getSelectedBuilding(out);
    
    if (!cur_selected || cur_selected->getType() != building_type::Stockpile) {
        return false;
    }

    return true;
    
}

bool loadstock_guard ( color_ostream &out, df::viewscreen *top )
{
     if ( !Gui::dwarfmode_hotkey ( top ) )
        return false;

    df::building *cur_selected = Gui::getSelectedBuilding(out);
    
    if (!cur_selected || cur_selected->getType() != building_type::Stockpile) {
        return false;
    }

    return true;
}

command_result copystock ( color_ostream &out, vector <string> & parameters )
{
    DEBUG(cycle, out).print("entering copystock\n");
    using df::global::plotinfo;
    using df::global::world;
    
    df::building *cur_selected = Gui::getSelectedBuilding(out);
    
    if (!cur_selected || cur_selected->getType() != building_type::Stockpile) {
        out.printerr ( "Selected building isn't a stockpile.\n" );
        return CR_WRONG_USAGE;
    }

    building_stockpilest *sp = virtual_cast<building_stockpilest> ( cur_selected );

    if ( !sp )
    {
        out.printerr ( "Selected building isn't a stockpile.\n" );
        return CR_WRONG_USAGE;
    }



    plotinfo->stockpile.custom_settings = sp->settings;
    plotinfo->main.mode = ui_sidebar_mode::Stockpiles;
    world->selected_stockpile_type = stockpile_category::Custom;

    out << "Stockpile options copied." << endl;
    return CR_OK;
}

//  exporting
command_result savestock ( color_ostream &out, vector <string> & parameters )
{

    using df::global::plotinfo;
    using df::global::world;

    building_stockpilest *sp = virtual_cast<building_stockpilest> ( world->selected_building );
    if ( !sp )
    {
        out.printerr ( "Selected building isn't a stockpile.\n" );
        return CR_WRONG_USAGE;
    }

    if ( parameters.size() > 2 )
    {
        out.printerr ( "Invalid parameters\n" );
        return CR_WRONG_USAGE;
    }

    bool debug = false;
    std::string file;
    for ( size_t i = 0; i < parameters.size(); ++i )
    {
        const std::string o = parameters.at ( i );
        if ( o == "--debug"  ||  o ==  "-d" )
            debug =  true;
        else  if ( !o.empty() && o[0] !=  '-' )
        {
            file = o;
        }
    }
    if ( file.empty() )
    {
        out.printerr ( "You must supply a valid filename.\n" );
        return CR_WRONG_USAGE;
    }

    StockpileSerializer cereal ( sp );
    if ( debug )
        cereal.enable_debug ( out );

    if ( !is_dfstockfile ( file ) ) file += ".dfstock";
    try
    {
        if ( !cereal.serialize_to_file ( file ) )
        {
            out.printerr ( "could not save to %s\n", file.c_str() );
            return CR_FAILURE;
        }
    }
    catch ( std::exception &e )
    {
        out.printerr ( "serialization failed: protobuf exception: %s\n", e.what() );
        return CR_FAILURE;
    }

    return CR_OK;
}

// importing
command_result loadstock ( color_ostream &out, vector <string> & parameters )
{

    using df::global::plotinfo;
    using df::global::world;

    building_stockpilest *sp = virtual_cast<building_stockpilest> ( world->selected_building );
    if ( !sp )
    {
        out.printerr ( "Selected building isn't a stockpile.\n" );
        return CR_WRONG_USAGE;
    }

    if ( parameters.size() < 1 ||  parameters.size() > 2 )
    {
        out.printerr ( "Invalid parameters\n" );
        return CR_WRONG_USAGE;
    }

    bool debug = false;
    std::string file;
    for ( size_t i = 0; i < parameters.size(); ++i )
    {
        const std::string o = parameters.at ( i );
        if ( o == "--debug"  ||  o ==  "-d" )
            debug =  true;
        else  if ( !o.empty() && o[0] !=  '-' )
        {
            file = o;
        }
    }
    if ( file.empty() ) {
        out.printerr ( "ERROR: missing .dfstock file parameter\n");
        return DFHack::CR_WRONG_USAGE;
    }
    if ( !is_dfstockfile ( file ) )
        file += ".dfstock";
    if ( !Filesystem::exists ( file ) )
    {
        out.printerr ( "ERROR: the .dfstock file doesn't exist: %s\n",  file.c_str());
        return CR_WRONG_USAGE;
    }

    StockpileSerializer cereal ( sp );
    if ( debug )
        cereal.enable_debug ( out );
    try
    {
        if ( !cereal.unserialize_from_file ( file ) )
        {
            out.printerr ( "unserialization failed: %s\n", file.c_str() );
            return CR_FAILURE;
        }
    }
    catch ( std::exception &e )
    {
        out.printerr ( "unserialization failed: protobuf exception: %s\n", e.what() );
        return CR_FAILURE;
    }
    return CR_OK;
}

/**
 * calls the lua function manage_settings() to kickoff the GUI
 */
bool manage_settings ( building_stockpilest *sp )
{
    auto L = Lua::Core::State;
    color_ostream_proxy out ( Core::getInstance().getConsole() );

    CoreSuspendClaimer suspend;
    Lua::StackUnwinder top ( L );

    if ( !lua_checkstack ( L, 2 ) )
        return false;

    if ( !Lua::PushModulePublic ( out, L, "plugins.stockpiles", "manage_settings" ) )
        return false;

    Lua::Push ( L, sp );

    if ( !Lua::SafeCall ( out, L, 1, 2 ) )
        return false;

    return true;
}

bool show_message_box ( const std::string & title,  const std::string & msg,  bool is_error = false )
{
    auto L = Lua::Core::State;
    color_ostream_proxy out ( Core::getInstance().getConsole() );

    CoreSuspendClaimer suspend;
    Lua::StackUnwinder top ( L );

    if ( !lua_checkstack ( L, 4 ) )
        return false;

    if ( !Lua::PushModulePublic ( out, L, "plugins.stockpiles", "show_message_box" ) )
        return false;

    Lua::Push ( L, title );
    Lua::Push ( L, msg );
    Lua::Push ( L, is_error );

    if ( !Lua::SafeCall ( out, L, 3, 0 ) )
        return false;

    return true;
}


std::vector<std::string> list_dir ( const std::string &path, bool recursive)
{
//     color_ostream_proxy out ( Core::getInstance().getConsole() );
    std::vector<std::string> files;
    std::stack<std::string> dirs;
    dirs.push(path);
//     out <<  "list_dir start" <<  endl;
    while (!dirs.empty() ) {
        const std::string current = dirs.top();
//         out <<  "\t walking " <<  current << endl;
        dirs.pop();
        std::vector<std::string> entries;
        const int res = DFHack::getdir(current,  entries);
        if ( res !=  0 )
            continue;
        for ( std::vector<std::string>::iterator it = entries.begin() ; it != entries.end(); ++it )
        {
            if ( (*it).empty() ||  (*it)[0] ==  '.' ) continue;
            //  shitty cross platform c++ we've got to construct the actual path manually
            std::ostringstream child_path_s;
            child_path_s <<  current <<  "/" <<  *it;
            const std::string child = child_path_s.str();
            if ( recursive && Filesystem::isdir ( child ) )
            {
//                 out <<  "\t\tgot child dir: " <<  child <<  endl;
                dirs.push ( child );
            }
            else if ( Filesystem::isfile ( child ) )
            {
                const std::string  rel_path ( child.substr ( std::string ( "./"+path).length()-1 ) );
//                 out <<  "\t\t adding file: " <<  child << "  as   " <<  rel_path  <<  endl;
                files.push_back ( rel_path );
            }
        }
    }
//     out <<  "listdir_stop" <<  endl;
    return files;
}

std::vector<std::string> clean_dfstock_list ( const std::string &path )
{
    if ( !Filesystem::exists ( path ) )
    {
        return std::vector<std::string>();
    }
    std::vector<std::string> files ( list_dir ( path,  true) );
    files.erase ( std::remove_if ( files.begin(), files.end(), [] ( const std::string &f )
    {
        return !is_dfstockfile ( f );
    } ),  files.end() );
    std::transform ( files.begin(), files.end(), files.begin(), [] ( const std::string &f )
    {
        return f.substr ( 0, f.find_last_of ( "." ) );
    } );
    std::sort ( files.begin(),files.end(), CompareNoCase );
    return files;
}

int stockpiles_list_settings ( lua_State *L )
{
    auto path = luaL_checkstring ( L, 1 );
    if ( Filesystem::exists ( path ) && !Filesystem::isdir ( path ) )
    {
        lua_pushfstring ( L,  "stocksettings path invalid: %s",  path );
        lua_error ( L );
        return 0;
    }
    std::vector<std::string> files = clean_dfstock_list ( path );
    Lua::PushVector ( L, files, true );
    return 1;
}

const std::string err_title = "Stockpile Settings Error";
const std::string err_help = "Does the folder exist?\nCheck the console for more information.";

void stockpiles_load ( color_ostream &out, std::string filename )
{
    std::vector<std::string> params;
    params.push_back ( filename );
    command_result r = loadstock ( out, params );
    if ( r !=  CR_OK )
        show_message_box ( err_title, "Couldn't load. " + err_help,  true );
}


void stockpiles_save ( color_ostream &out, std::string filename )
{
    std::vector<std::string> params;
    params.push_back ( filename );
    command_result r = savestock ( out, params );
    if ( r !=  CR_OK )
        show_message_box ( err_title, "Couldn't save. " + err_help,  true );
}

