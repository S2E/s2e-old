extern "C" {
#include <qemu-common.h>
#include <cpu-all.h>
#include <exec-all.h>
}


#include "ConfigFile.h"

#include <s2e/Utils.h>
#include <s2e/S2E.h>
#include <s2e/S2EExecutionState.h>
#include <ctype.h>
#include <stdlib.h>
#include <sstream>

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

        //Register S2E API
        RegisterS2EApi();
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
const char* ConfigFile::getTypeName<ConfigFile::integer_list>() {
  return "lua_list with only integer values";
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
bool ConfigFile::getLuaValue(integer_list* res, 
                             const integer_list& def, int index) {
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
            res->push_back(lua_tointeger(m_luaState, -1));
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

ConfigFile::integer_list ConfigFile::getIntegerList(
            const std::string& name, const integer_list& def, bool *ok)
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

bool ConfigFile::hasKey(const std::string& name)
{
	assert(name.size() != 0);
    string expr = "return " + name;

    if(luaL_loadstring(m_luaState, expr.c_str()) ||
                    lua_pcall(m_luaState, 0, 1, 0))
        return false;

    bool ok = !lua_isnil(m_luaState, -1);
    lua_pop(m_luaState, 1);

    return ok;
}


///////////////////////////////////////////////////////
//XXX: Maybe find a better place for the next functions.
//ConfigFile does not sound to be the right place

//Register a C function that will be called by lua
void ConfigFile::fcnRegister(const char *name, int (*callback)(void *), void *opaque)
{
    lua_pushinteger(m_luaState, (lua_Integer)opaque);
    lua_pushcclosure(m_luaState, (lua_CFunction)callback, 1);
    lua_setglobal(m_luaState, name);
}

int ConfigFile::fcnGetArgumentCount(void *s)
{
    lua_State *L = (lua_State *)s;
    return lua_gettop(L);
}

void *ConfigFile::fcnGetContext(void *s)
{
    lua_State *L = (lua_State *)s;
    bool ok = lua_isnumber(L, lua_upvalueindex(1));
    if (!ok) { return NULL; }
    return (void*)lua_tointeger(L, lua_upvalueindex(1));
}

bool ConfigFile::fcnGetStringArg(void *s, int index, std::string &ret)
{
    lua_State *L = (lua_State *)s;
    const char *str = lua_tostring(L, index);
    if (!str) {
        return false;
    }
    ret = str;
    return true;
}

void ConfigFile::fcnExecute(const char *cmd)
{
    if (luaL_dostring(m_luaState, cmd)) {
        luaWarning("Could not run '%s':\n    %s\n",
                    cmd, lua_tostring(m_luaState, -1));
        //lua_pop(m_luaState, 1);
    }
}

void ConfigFile::invokeAnnotation(const std::string &annotation, S2EExecutionState *param)
{
    LuaPrivateData *opaque = new LuaPrivateData();
    opaque->opaque = param;
    opaque->type = STATE;

    boxptr(m_luaState, opaque);
    luaL_getmetatable(m_luaState, LUAS2E);
    lua_setmetatable(m_luaState, -2);    /* Done */
    lua_call(m_luaState, 1, 0);
}

int ConfigFile::RegisterS2EApi()
{
    static const luaL_reg meta_methods[] = {
        {"__gc", ConfigFile::luaS2EDestroy },
        {0,0}
    };

    static const luaL_reg s2e_methods[] = {
        {"get_param",           ConfigFile::s2e_get_param },
        {"write_mem_symb", ConfigFile::s2e_write_mem_symb },
        {"write_register",          ConfigFile::s2e_write_register },
        {0,0}
    };
#if 0
    int metatable, methods;
    lua_State *L = m_luaState;


    luaL_newmetatable(L, LUAS2E);
    lua_pushliteral(L, "__index");
    lua_pushvalue(L, -3);
    lua_settable(L, -3);
    luaL_openlib(L, NULL, meta_methods, 0);
    lua_pop(L, 1);


    lua_pushliteral(L, LUAS2E);        /* name of LuaS2E table */
    methods   = newtable(L);           /* LuaS2E methods table */
    metatable = newtable(L);           /* LuaS2E metatable */

    lua_pushliteral(L, "__index");     /* add index event to metatable */
    lua_pushvalue(L, methods);
    lua_settable(L, metatable);        /* metatable.__index = methods */

    lua_pushliteral(L, "__metatable"); /* hide metatable */
    lua_pushvalue(L, methods);
    lua_settable(L, metatable);        /* metatable.__metatable = methods */

    luaL_openlib(L, 0, meta_methods,  0); /* fill metatable */
    luaL_openlib(L, 0, s2e_methods, 1); /* fill LuaS2E methods table */
    lua_settable(L, LUA_GLOBALSINDEX); /* add LuaS2E to globals */
#endif
    return 0;

}


ConfigFile::LuaPrivateData* ConfigFile::luaS2ECheck(lua_State *L, int i)
{
    if (luaL_checkudata(L, i, LUAS2E) != NULL) {
        LuaPrivateData *im = (LuaPrivateData*)unboxptr(L, i);
        if (im == NULL)
            luaL_error(L, "attempt to use an invalid " LUAS2E);
        return im;
    }
    luaL_typerror(L, i, LUAS2E);
    return NULL;
}

int ConfigFile::luaS2EDestroy(lua_State *L)
{
    LuaPrivateData *pd = (LuaPrivateData*)unboxptr(L, 1);
    if (pd)
        delete pd;
    return 0;
}

//Reads a concrete value from the stack
int ConfigFile::s2e_get_param(lua_State *L)
{
    int n = lua_gettop(L);    /* number of arguments */

    if (n != 2) {
        lua_pushstring(L, "incorrect argument to function s2e_get_param'");
        lua_error(L);
    }

    LuaPrivateData *pd = luaS2ECheck(L, 1);
    uint32_t param = luaL_checkint(L, 2);

    S2EExecutionState *state = static_cast<S2EExecutionState*>(pd->opaque);

    uint32_t val;
    bool b = state->readMemoryConcrete(state->getSp() + (param+1) * sizeof(uint32_t), &val, sizeof(val));
    if (!b) {
        lua_pushstring(L, "s2e_get_param: Could not read from memory");
        lua_error(L);
    }

    lua_pushnumber(L, val);        /* first result */
    return 1;
}

int ConfigFile::s2e_write_mem_symb(lua_State *L)
{
    int n = lua_gettop(L);    /* number of arguments */

    if (n != 3) {
        lua_pushstring(L, "incorrect argument to function s2e_write_mem_symb'");
        lua_error(L);
    }

    LuaPrivateData *pd = luaS2ECheck(L, 1);
    uint32_t address = luaL_checkint(L, 2);
    uint32_t size = luaL_checkint(L, 3);

    S2EExecutionState *state = static_cast<S2EExecutionState*>(pd->opaque);

    klee::Expr::Width width=klee::Expr::Int8;
    switch(size) {
        case 1: width = klee::Expr::Int8; break;
        case 2: width = klee::Expr::Int16; break;
        case 4: width = klee::Expr::Int32; break;
        case 8: width = klee::Expr::Int64; break;
        default:
            {
                std::stringstream ss;
                ss << "s2e_write_mem_symb: Invalid size " << size;
                lua_pushstring(L, ss.str().c_str());
                lua_error(L);
            }
            break;
    }

    klee::ref<klee::Expr> val = state->createSymbolicValue(width, "s2e_write_mem_symb");

    if (!state->writeMemory(address, val)) {
        std::stringstream ss;
        ss << "s2e_write_mem_symb: Could not write to memory at address 0x" << std::hex << address;
        lua_pushstring(L, ss.str().c_str());
        lua_error(L);
    }

    return 0;
}

int ConfigFile::s2e_write_register(lua_State *L)
{
    int n = lua_gettop(L);    /* number of arguments */

    if (n != 3) {
        lua_pushstring(L, "incorrect argument to function s2e_write_register'");
        lua_error(L);
    }
#if 0
    //XXX: check pointer validity!
    if (!lua_isnumber(L, 1)) {
        lua_pushstring(L, "First parameter must be the execution state");
        lua_error(L);
    }

    if (!lua_isstring(L, 2)) {
        lua_pushstring(L, "Second parameter must be a register name");
        lua_error(L);
    }

    if (!lua_isnumber(L, 3)) {
        lua_pushstring(L, "Third parameter must be the value to be written");
        lua_error(L);
    }
#endif
    LuaPrivateData *pd = luaS2ECheck(L, 1);
    std::string regstr = luaL_checkstring(L, 2);
    uint32_t value = luaL_checkint(L, 3);

    S2EExecutionState *state = static_cast<S2EExecutionState*>(pd->opaque);

    unsigned regIndex=0, size=0;

    if (regstr == "eax") {
        regIndex = R_EAX;
        size = 4;
    }else if (regstr == "ebx") {
        regIndex = R_EBX;
        size = 4;
    }else if (regstr == "ecx") {
        regIndex = R_ECX;
        size = 4;
    }else if (regstr == "edx") {
        regIndex = R_EDX;
        size = 4;
    }else if (regstr == "edi") {
        regIndex = R_EDI;
        size = 4;
    }else if (regstr == "esi") {
        regIndex = R_ESI;
        size = 4;
    }else if (regstr == "esp") {
        regIndex = R_ESP;
        size = 4;
    }else if (regstr == "ebp") {
        regIndex = R_EBP;
        size = 4;
    }else {
        std::stringstream ss;
        ss << "Invalid register " << regstr;
        lua_pushstring(L, ss.str().c_str());
        lua_error(L);
    }

    state->writeCpuRegisterConcrete(offsetof(CPUState, regs) + regIndex*4, &value, size);

    return 0;                   /* number of results */
}

///////////////////////////////////////////////////////
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

