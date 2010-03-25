#ifndef S2E_CONFIG_FILE_H
#define S2E_CONFIG_FILE_H

#include <vector>
#include <string>

typedef struct lua_State lua_State;

class ConfigFile
{
private:
    lua_State *m_luaState;

    /* Called on errors during initial loading. Will terminate the program */
    void luaError(const char *fmt, ...);

    /* Called on errors that can be ignored */
    void luaWarning(const char *fmt, ...);

    template<typename T>
    const char* getTypeName();

    template<typename T>
    bool getLuaValue(T* res, const T& def, int index = -1);

    template<typename T>
    T getValueT(const std::string& expr, const T& def, bool *ok);

public:
    ConfigFile(const std::string &ConfigFile);
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
};

#endif
