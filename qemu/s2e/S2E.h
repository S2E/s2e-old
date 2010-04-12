#ifndef S2E_H
#define S2E_H

#include <fstream>
#include <string>
#include <vector>
#include <tr1/unordered_map>

namespace klee {
    class Interpreter;
}

class TCGLLVMContext;

namespace s2e {

class Plugin;
class CorePlugin;
class ConfigFile;
class PluginsFactory;

class S2EHandler;
class S2EExecutor;
class S2EExecutionState;

class S2E
{
protected:
  ConfigFile* m_configFile;
  PluginsFactory* m_pluginsFactory;

  CorePlugin* m_corePlugin;
  std::vector<Plugin*> m_activePluginsList;
  std::tr1::unordered_map<std::string, Plugin*> m_activePluginsMap;

  std::string m_outputDirectory;

  std::ostream* m_infoFile;
  std::ostream* m_messagesFile;
  std::ostream* m_warningsFile;
  std::streambuf* m_warningsStreamBuf;

  TCGLLVMContext *m_tcgLLVMContext;

  /* The following members are late-initialized when
     QEMU pc creation is complete */
  S2EHandler* m_s2eHandler;
  S2EExecutor* m_s2eExecutor;

  void initOutputDirectory(const std::string& outputDirectory);
  void initPlugins();

public:
  /** Construct S2E */
  explicit S2E(TCGLLVMContext* tcgLLVMContext,
               const std::string& configFileName,
               const std::string& outputDirectory);
  ~S2E();

  /** Initialize symbolic execution machinery */
  void initializeSymbolicExecution();

  /*****************************/
  /* Configuration and plugins */

  /** Get configuration file */
  ConfigFile* getConfig() const { return m_configFile; }

  /** Get plugin by name of functionName */
  Plugin* getPlugin(const std::string& name) const;

  /** Get Core plugin */
  CorePlugin* getCorePlugin() const { return m_corePlugin; }

  /*************************/
  /* Directories and files */

  /** Get output directory name */
  const std::string& getOutputDirectory() const { return m_outputDirectory; }

  /** Get a filename inside an output directory */
  std::string getOutputFilename(const std::string& fileName);

  /** Create output file in an output directory */
  std::ostream* openOutputFile(const std::string &filename);

  /** Get info stream (used by KLEE internals) */
  std::ostream& getInfoStream() const { return *m_infoFile; }

  /** Get messages stream (used for non-critical information) */
  std::ostream& getMessagesStream() const { return *m_messagesFile; }

  /** Get warnings stream (used for warnings, duplicated on the screen) */
  std::ostream& getWarningsStream() const { return *m_warningsFile; }

  /***********************/
  /* Runtime information */
  S2EExecutor* getExecutor() { return m_s2eExecutor; }
};

} // namespace s2e

#endif // S2E_H
