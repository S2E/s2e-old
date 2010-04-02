#ifndef S2E_H
#define S2E_H

#include <memory>
#include <fstream>
#include <string>
#include <map>

namespace klee {
    class Interpreter;
}

namespace s2e {

class Plugin;
class CorePlugin;
class ConfigFile;
class PluginsFactory;

class KleeHandler;
using klee::Interpreter;

class S2E
{
private:
  std::auto_ptr<ConfigFile> m_configFile;
  std::auto_ptr<PluginsFactory> m_pluginsFactory;

  CorePlugin* m_corePlugin;
  std::map<std::string, Plugin*> m_activePlugins;

  std::string m_outputDirectory;

  std::auto_ptr<std::ostream> m_infoFile;
  std::auto_ptr<std::ostream> m_messagesFile;
  std::auto_ptr<std::ostream> m_warningsFile;

  std::auto_ptr<KleeHandler> m_kleeHandler;
  std::auto_ptr<Interpreter> m_kleeInterpreter;

  void initOutputDirectory(const std::string& outputDirectory);
  void initKlee();
  void initPlugins();
  
public:
  /** Constructs S2E */
  explicit S2E(const std::string& configFileName,
               const std::string& outputDirectory);
  ~S2E();

  /** Get configuration file */
  ConfigFile* getConfig() const { return m_configFile.get(); }

  /** Get plugin by name of functionName */
  Plugin* getPlugin(const std::string& name) const;

  /** Get Core plugin */
  CorePlugin* getCorePlugin() const { return m_corePlugin; }

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

};

} // namespace s2e

#endif // S2E_H
