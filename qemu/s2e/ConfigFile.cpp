#include "ConfigFile.h"

#include <s2e/Utils.h>
#include <ctype.h>
#include <stdlib.h>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

namespace s2e {
using namespace std;

ConfigFile::ConfigFile(const std::string &configFileName)
{
    m_luaState = lua_open();
    luaL_openlibs(m_luaState);
  
    if(!configFileName.empty()) {
        if(luaL_loadfile(m_luaState, configFileName.c_str()) ||
                    lua_pcall(m_luaState, 0, 0, 0)) {
            luaError("Can not run configuration file:\n    %s\n",
                    lua_tostring(m_luaState, -1));
        }
    }
}

ConfigFile::~ConfigFile()
{
    lua_close(m_luaState);
}

template<> inline
const char* ConfigFile::getTypeName<bool>() { return "boolean"; }

template<> inline
const char* ConfigFile::getTypeName<int64_t>() { return "integer"; }

template<> inline
const char* ConfigFile::getTypeName<double>() { return "double"; }

template<> inline
const char* ConfigFile::getTypeName<string>() { return "string"; }

template<> inline
const char* ConfigFile::getTypeName<ConfigFile::string_list>() {
  return "lua_list with only string values";
}

template<> inline
const char* ConfigFile::getTypeName<ConfigFile::_key_list>() {
  return "lua_table with only string keys";
}

template<> inline
const char* ConfigFile::getTypeName<ConfigFile::_list_size>() {
  return "lua_table";
}

template<> inline
bool ConfigFile::getLuaValue(bool *res, const bool& def, int index) {
    bool ok = lua_isboolean(m_luaState, index);
    *res = ok ? lua_toboolean(m_luaState, index) : def;
    return ok;
}

template<> inline
bool ConfigFile::getLuaValue(int64_t* res, const int64_t& def, int index) {
    bool ok = lua_isnumber(m_luaState, index);
    *res = ok ? lua_tointeger(m_luaState, index) : def;
    return ok;
}

template<> inline
bool ConfigFile::getLuaValue(double* res, const double& def, int index) {
    bool ok = lua_isnumber(m_luaState, index);
    *res = ok ? lua_tonumber(m_luaState, index) : def;
    return ok;
}

template<> inline
bool ConfigFile::getLuaValue(string* res, const string& def, int index) {
    bool ok = lua_isstring(m_luaState, index);
    *res = ok ? lua_tostring(m_luaState, index) : def;
    return ok;
}

template<> inline
bool ConfigFile::getLuaValue(string_list* res, const string_list& def, int index) {
    bool ok = lua_istable(m_luaState, index);
    if(!ok) { *res = def; return ok; }
  
    /* read table as array */
    for(int i=1; ; ++i) {
        lua_rawgeti(m_luaState, index, i);
        if(lua_isnil(m_luaState, -1)) {
            lua_pop(m_luaState, 1);
            break;
        }
        if(lua_isstring(m_luaState, -1)) {
            res->push_back(lua_tostring(m_luaState, -1));
            lua_pop(m_luaState, 1);
        } else {
            lua_pop(m_luaState, 1);
            *res = def;
            return false;
        }
    }
  
    return true;
}

template<> inline
bool ConfigFile::getLuaValue(_list_size* res, const _list_size& def, int index) {
    bool ok = lua_istable(m_luaState, index);
    if(!ok) { *res = def; return ok; }

    /* read table as array */
    res->size = 0;
    for(int i=1; ; ++i) {
        lua_rawgeti(m_luaState, index, i);
        if(lua_isnil(m_luaState, -1)) {
            lua_pop(m_luaState, 1);
            break;
        }
        res->size += 1;
        lua_pop(m_luaState, 1);
    }

    return true;
}

template<> inline
bool ConfigFile::getLuaValue(_key_list* res, const _key_list& def, int index) {
    bool ok = lua_istable(m_luaState, index);
    if(!ok) { *res = def; return ok; }

    lua_pushnil(m_luaState);  /* first key */

    /* table is in the stack at index-1 */
    while(lua_next(m_luaState, index-1) != 0) {
        /* uses 'key' (at index -2) and 'value' (at index -1) */

        if (!lua_isstring(m_luaState, -2)) {
            *res = def;
            return false;
        }

        res->keys.push_back(lua_tostring(m_luaState, -2));

        /* removes 'value'; keeps 'key' for next iteration */
        lua_pop(m_luaState, 1);
    }

    return true;
}

template<typename T> inline
T ConfigFile::getValueT(const std::string& name, const T& def, bool *ok)
{
	assert(name.size() != 0);    
    string expr = "return " + name;
  
    if(luaL_loadstring(m_luaState, expr.c_str()) ||
                    lua_pcall(m_luaState, 0, 1, 0)) {
        luaWarning("Can not get configuration value '%s':\n    %s\n",
                    name.c_str(), lua_tostring(m_luaState, -1));
        lua_pop(m_luaState, 1);
        if(ok) *ok = false;
        return def;
    }
  
    T res;
    bool _ok = getLuaValue(&res, def, -1);
    if(ok) *ok = _ok;
  
    if(!_ok) {
        luaWarning("Can not get configuration value '%s':\n    "
                "value of type %s can not be converted to %s\n",
                name.c_str(), lua_typename(m_luaState,
                    lua_type(m_luaState, -1)),
                getTypeName<T>());
    }
  
    lua_pop(m_luaState, 1);
    return res;
}

bool ConfigFile::getBool(const string& name, bool def, bool *ok)
{
    return getValueT(name, def, ok);
}

int64_t ConfigFile::getInt(const string& name, int64_t def, bool *ok)
{
    return getValueT(name, def, ok);
}

string ConfigFile::getString(
            const string& name, const string& def, bool *ok)
{
    return getValueT(name, def, ok);
}

ConfigFile::string_list ConfigFile::getStringList(
            const std::string& name, const string_list& def, bool *ok)
{
    return getValueT(name, def, ok);
}

int ConfigFile::getListSize(const std::string& name, bool *ok)
{
    static const _list_size l = { 0 };
    return getValueT(name, l, ok).size;
}

ConfigFile::string_list ConfigFile::getListKeys(const std::string& name, bool *ok)
{
    static const _key_list l;
    return getValueT(name, l, ok).keys;
}

void ConfigFile::luaError(const char *fmt, ...)
{
    fprintf(stderr, "ERROR: ");
    va_list v;
    va_start(v, fmt);
    vfprintf(stderr, fmt, v);
    va_end(v);
    lua_close(m_luaState);
    exit(1);
}

void ConfigFile::luaWarning(const char *fmt, ...)
{
    fprintf(stderr, "WARNING: ");
    va_list v;
    va_start(v, fmt);
    vfprintf(stderr, fmt, v);
    va_end(v);
}

} // namespace s2e

