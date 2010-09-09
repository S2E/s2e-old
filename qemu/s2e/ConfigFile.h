#ifndef S2E_CONFIG_FILE_H
#define S2E_CONFIG_FILE_H

#include <vector>
#include <string>
#include <inttypes.h>

struct lua_State;

namespace s2e {
class S2EExecutionState;

#define LUAS2E "LuaS2E"

//Copied from lua-gd
/* Emulates lua_(un)boxpointer from Lua 5.0 (don't exists on Lua 5.1) */
#define boxptr(L, p)   (*(void**)(lua_newuserdata(L, sizeof(void*))) = (p))
#define unboxptr(L, i) (*(void**)(lua_touserdata(L, i)))


class ConfigFile
{
public:
    enum LuaPrivateDataType {
        STATE
    };

    struct LuaPrivateData {
        LuaPrivateDataType type;
        void *opaque;
    };

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
    static LuaPrivateData* luaS2ECheck(lua_State *L, int index);
    static int luaS2EDestroy(lua_State *L);
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


    void fcnRegister(const char *name, int (*callback)(void *), void *context);
    static int fcnGetArgumentCount(void *s);
    static void *fcnGetContext(void *s);
    static bool fcnGetStringArg(void *s, int index, std::string &ret);
    void fcnExecute(const char *cmd);

    void invokeAnnotation(const std::string &annotation, S2EExecutionState *param);
    static int s2e_get_param(lua_State *L);
    static int s2e_write_mem_symb(lua_State *L);
    static int s2e_write_register(lua_State *L);
};

} // namespace s2e

#endif
