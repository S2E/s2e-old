/*
 * S2E Selective Symbolic Execution Framework
 *
 * Copyright (c) 2010, Dependable Systems Laboratory, EPFL
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Dependable Systems Laboratory, EPFL nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE DEPENDABLE SYSTEMS LABORATORY, EPFL BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Currently maintained by:
 *    Volodymyr Kuznetsov <vova.kuznetsov@epfl.ch>
 *    Vitaly Chipounov <vitaly.chipounov@epfl.ch>
 *
 * All contributors are listed in S2E-AUTHORS file.
 *
 */

#ifndef S2E_CONFIG_FILE_H
#define S2E_CONFIG_FILE_H

#include <vector>
#include <string>
#include <inttypes.h>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

namespace s2e {
class S2EExecutionState;

#define LUAS2E "LuaS2E"

//Copied from lua-gd
/* Emulates lua_(un)boxpointer from Lua 5.0 (don't exists on Lua 5.1) */
#define boxptr(L, p)   (*(void**)(lua_newuserdata(L, sizeof(void*))) = (p))
#define unboxptr(L, i) (*(void**)(lua_touserdata(L, i)))


template <typename T> class Lunar {
  typedef struct { T *pT; } userdataType;
public:
  typedef int (T::*mfp)(lua_State *L);
  typedef struct { const char *name; mfp mfunc; } RegType;

  static void Register(lua_State *L) {
    lua_newtable(L);
    int methods = lua_gettop(L);

    luaL_newmetatable(L, T::className);
    int metatable = lua_gettop(L);

    // store method table in globals so that
    // scripts can add functions written in Lua.
    lua_pushvalue(L, methods);
    set(L, LUA_GLOBALSINDEX, T::className);

    // hide metatable from Lua getmetatable()
    lua_pushvalue(L, methods);
    set(L, metatable, "__metatable");

    lua_pushvalue(L, methods);
    set(L, metatable, "__index");

    lua_pushcfunction(L, tostring_T);
    set(L, metatable, "__tostring");

    lua_pushcfunction(L, gc_T);
    set(L, metatable, "__gc");

    lua_newtable(L);                // mt for method table
    lua_pushcfunction(L, new_T);
    lua_pushvalue(L, -1);           // dup new_T function
    set(L, methods, "new");         // add new_T to method table
    set(L, -3, "__call");           // mt.__call = new_T
    lua_setmetatable(L, methods);

    // fill method table with methods from class T
    for (RegType *l = T::methods; l->name; l++) {
      lua_pushstring(L, l->name);
      lua_pushlightuserdata(L, (void*)l);
      lua_pushcclosure(L, thunk, 1);
      lua_settable(L, methods);
    }

    lua_pop(L, 2);  // drop metatable and method table
  }

  // call named lua method from userdata method table
  static int call(lua_State *L, const char *method,
                  int nargs=0, int nresults=LUA_MULTRET, int errfunc=0)
  {
    int base = lua_gettop(L) - nargs;  // userdata index
    if (!luaL_checkudata(L, base, T::className)) {
      lua_settop(L, base-1);           // drop userdata and args
      lua_pushfstring(L, "not a valid %s userdata", T::className);
      return -1;
    }

    lua_pushstring(L, method);         // method name
    lua_gettable(L, base);             // get method from userdata
    if (lua_isnil(L, -1)) {            // no method?
      lua_settop(L, base-1);           // drop userdata and args
      lua_pushfstring(L, "%s missing method '%s'", T::className, method);
      return -1;
    }
    lua_insert(L, base);               // put method under userdata, args

    int status = lua_pcall(L, 1+nargs, nresults, errfunc);  // call method
    if (status) {
      const char *msg = lua_tostring(L, -1);
      if (msg == NULL) msg = "(error with no message)";
      lua_pushfstring(L, "%s:%s status = %d\n%s",
                      T::className, method, status, msg);
      lua_remove(L, base);             // remove old message
      return -1;
    }
    return lua_gettop(L) - base + 1;   // number of results
  }

  // push onto the Lua stack a userdata containing a pointer to T object
  static int push(lua_State *L, T *obj, bool gc=false) {
    if (!obj) { lua_pushnil(L); return 0; }
    luaL_getmetatable(L, T::className);  // lookup metatable in Lua registry
    if (lua_isnil(L, -1)) luaL_error(L, "%s missing metatable", T::className);
    int mt = lua_gettop(L);
    subtable(L, mt, "userdata", "v");
    userdataType *ud =
      static_cast<userdataType*>(pushuserdata(L, obj, sizeof(userdataType)));
    if (ud) {
      ud->pT = obj;  // store pointer to object in userdata
      lua_pushvalue(L, mt);
      lua_setmetatable(L, -2);
      if (gc == false) {
        lua_checkstack(L, 3);
        subtable(L, mt, "do not trash", "k");
        lua_pushvalue(L, -2);
        lua_pushboolean(L, 1);
        lua_settable(L, -3);
        lua_pop(L, 1);
      }
    }
    lua_replace(L, mt);
    lua_settop(L, mt);
    return mt;  // index of userdata containing pointer to T object
  }

  // get userdata from Lua stack and return pointer to T object
  static T *check(lua_State *L, int narg) {
    userdataType *ud =
      static_cast<userdataType*>(luaL_checkudata(L, narg, T::className));
    if(!ud) {
        luaL_typerror(L, narg, T::className);
        return NULL;
    }
    return ud->pT;  // pointer to T object
  }

private:
  Lunar();  // hide default constructor

  // member function dispatcher
  static int thunk(lua_State *L) {
    // stack has userdata, followed by method args
    T *obj = check(L, 1);  // get 'self', or if you prefer, 'this'
    lua_remove(L, 1);  // remove self so member function args start at index 1
    // get member function from upvalue
    RegType *l = static_cast<RegType*>(lua_touserdata(L, lua_upvalueindex(1)));
    return (obj->*(l->mfunc))(L);  // call member function
  }

  // create a new T object and
  // push onto the Lua stack a userdata containing a pointer to T object
  static int new_T(lua_State *L) {
    lua_remove(L, 1);   // use classname:new(), instead of classname.new()
    T *obj = new T(L);  // call constructor for T objects
    push(L, obj, true); // gc_T will delete this object
    return 1;           // userdata containing pointer to T object
  }

  // garbage collection metamethod
  static int gc_T(lua_State *L) {
    if (luaL_getmetafield(L, 1, "do not trash")) {
      lua_pushvalue(L, 1);  // dup userdata
      lua_gettable(L, -2);
      if (!lua_isnil(L, -1)) return 0;  // do not delete object
    }
    userdataType *ud = static_cast<userdataType*>(lua_touserdata(L, 1));
    T *obj = ud->pT;
    if (obj) delete obj;  // call destructor for T objects
    return 0;
  }

  static int tostring_T (lua_State *L) {
    char buff[32];
    userdataType *ud = static_cast<userdataType*>(lua_touserdata(L, 1));
    T *obj = ud->pT;
    sprintf(buff, "%p", (void*)obj);
    lua_pushfstring(L, "%s (%s)", T::className, buff);

    return 1;
  }

  static void set(lua_State *L, int table_index, const char *key) {
    lua_pushstring(L, key);
    lua_insert(L, -2);  // swap value and key
    lua_settable(L, table_index);
  }

  static void weaktable(lua_State *L, const char *mode) {
    lua_newtable(L);
    lua_pushvalue(L, -1);  // table is its own metatable
    lua_setmetatable(L, -2);
    lua_pushliteral(L, "__mode");
    lua_pushstring(L, mode);
    lua_settable(L, -3);   // metatable.__mode = mode
  }

  static void subtable(lua_State *L, int tindex, const char *name, const char *mode) {
    lua_pushstring(L, name);
    lua_gettable(L, tindex);
    if (lua_isnil(L, -1)) {
      lua_pop(L, 1);
      lua_checkstack(L, 3);
      weaktable(L, mode);
      lua_pushstring(L, name);
      lua_pushvalue(L, -2);
      lua_settable(L, tindex);
    }
  }

  static void *pushuserdata(lua_State *L, void *key, size_t sz) {
    void *ud = 0;
    lua_pushlightuserdata(L, key);
    lua_gettable(L, -2);     // lookup[key]
    if (lua_isnil(L, -1)) {
      lua_pop(L, 1);         // drop nil
      lua_checkstack(L, 3);
      ud = lua_newuserdata(L, sz);  // create new userdata
      lua_pushlightuserdata(L, key);
      lua_pushvalue(L, -2);  // dup userdata
      lua_settable(L, -4);   // lookup[key] = userdata
    }
    return ud;
  }
};

#define LUNAR_DECLARE_METHOD(Class, Name) {#Name, &Class::Name}

class S2ELUAExecutionState
{
private:
    S2EExecutionState *m_state;
public:
  static const char className[];
  static Lunar<S2ELUAExecutionState>::RegType methods[];

  S2ELUAExecutionState(lua_State *L);
  S2ELUAExecutionState(S2EExecutionState *s);
  ~S2ELUAExecutionState();
  int writeRegister(lua_State *L);
  int writeRegisterSymb(lua_State *L);
  int readRegister(lua_State *L);
  int readParameter(lua_State *L);
  int writeParameter(lua_State *L);
  int writeMemorySymb(lua_State *L);
  int readMemory(lua_State *L);
  int writeMemory(lua_State *L);
};


class ConfigFile
{
private:
    lua_State *m_luaState;

    /* Called on errors during initial loading. Will terminate the program */
    void luaError(const char *fmt, ...);

    /* Called on errors that can be ignored */
    void luaWarning(const char *fmt, ...);

    /* Fake data type for list size */
    struct _list_size { int size; };

    /* Fake data type for table key list */
    struct _key_list { std::vector<std::string> keys; };

    /* Helper to get C++ type name */
    template<typename T>
    const char* getTypeName();

    /* Helper to get topmost value of lua stack as a C++ value */
    template<typename T>
    bool getLuaValue(T* res, const T& def, int index = -1);

    /* Universal implementation for getXXX functions */
    template<typename T>
    T getValueT(const std::string& expr, const T& def, bool *ok);

    int RegisterS2EApi();

public:
    ConfigFile(const std::string &configFileName);
    ~ConfigFile();

    /* Return value from configuration file.
  
       Example:
         width = getValueInt("window.width");
  
       Arguments:
         name  the name or the value (actually,
               any valid lua expression that will be
               prepended by "return ")
         def   default value to return on error
         ok    if non-null then will be false on error
    */
    bool getBool(const std::string& name, bool def = false, bool *ok = NULL);
    int64_t getInt(const std::string& name, int64_t def = 0, bool *ok = NULL);
    double getDouble(const std::string& name, double def = 0, bool *ok = NULL);
    std::string getString(const std::string& name,
                    const std::string& def = std::string(), bool *ok = NULL);
  
    typedef std::vector<std::string> string_list;
    string_list getStringList(const std::string& name,
                    const string_list& def = string_list(), bool *ok = NULL);

    typedef std::vector<uint64_t> integer_list;
    integer_list getIntegerList(
            const std::string& name, const integer_list& def = integer_list(), bool *ok = NULL);


    /* Return all string keys for a given table.
       Fails if some keys are not strings. */
    string_list getListKeys(const std::string& name, bool *ok = NULL);
    
    /* Return the size of the list. Works for all types of
       lua lists just like '#' operator in lua. */
    int getListSize(const std::string& name, bool *ok = NULL);

    /* Returns true if a config key exists */
    bool hasKey(const std::string& name);


    void invokeLuaCommand(const char *cmd);

    //void invokeAnnotation(const std::string &annotation, S2EExecutionState *param);

    lua_State* getState() const {
        return m_luaState;
    }

};

} // namespace s2e

#endif
