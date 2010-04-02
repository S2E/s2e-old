#include "S2E.h"

#include <s2e/Plugin.h>
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/ConfigFile.h>
#include <s2e/Utils.h>

#include <s2e/s2e_qemu.h>

#include <llvm/System/Path.h>

#include <klee/Interpreter.h>

#include <iostream>
#include <sstream>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>

#include <sys/stat.h>

namespace s2e {

using namespace std;

/** A streambuf that writes both to parent streambuf and cerr */
class WarningsStreamBuf : public streambuf
{
    streambuf* m_parent;
public:
    WarningsStreamBuf(streambuf* parent): m_parent(parent) {}
    streamsize xsputn(const char* s, streamsize n) {
        cerr.rdbuf()->sputn(s, n);
        return m_parent->sputn(s, n);
    }
    int overflow(int c = EOF ) {
        cerr.rdbuf()->sputc(c);
        m_parent->sputc(c);
        return 1;
    }
    int sync() {
        cerr.rdbuf()->pubsync();
        return m_parent->pubsync();
    }
};

/** Handler required for KLEE interpreter */
class KleeHandler : public klee::InterpreterHandler
{
private:
    S2E* m_s2e;
    unsigned m_testIndex;  // number of tests written so far
    unsigned m_pathsExplored; // number of paths explored so far

public:
    KleeHandler(S2E* s2e) : m_s2e(s2e) {}

    std::ostream &getInfoStream() const { return m_s2e->getInfoStream(); }
    std::string getOutputFilename(const std::string &fileName) {
        return m_s2e->getOutputFilename(fileName);
    }
    std::ostream *openOutputFile(const std::string &fileName) {
        return m_s2e->openOutputFile(fileName);
    }

    /* klee-related function */
    void incPathsExplored() { m_pathsExplored++; }

    /* klee-related function */
    void processTestCase(const klee::ExecutionState &state,
                         const char *err, const char *suffix) {
        m_s2e->getWarningsStream() << "Terminating state '" << (&state)
               << "with error message '" << (err ? err : "") << "'" << std::endl;
    }
};

S2E::S2E(const std::string &configFileName, const std::string &outputDirectory)
{
    /* Open output directory. Do it at the very begining so that
       other init* functions can use it. */
    initOutputDirectory(outputDirectory);

    /* Parse configuration file */
    m_configFile = new s2e::ConfigFile(configFileName);

    /* Initialize KLEE objects */
    initKlee();

    /* Load and initialize plugins */
    initPlugins();
}

S2E::~S2E()
{
    foreach(Plugin* p, m_activePluginsList)
        delete p;

    delete m_pluginsFactory;

    //delete m_kleeInterpreter;
    //delete m_kleeHandler;

    delete m_configFile;

    delete m_infoFile;
    delete m_messagesFile;
    delete m_warningsFile;
    delete m_warningsStreamBuf;
}

Plugin* S2E::getPlugin(const std::string& name) const
{
    map<string, Plugin*>::const_iterator it = m_activePluginsMap.find(name);
    if(it != m_activePluginsMap.end())
        return const_cast<Plugin*>(it->second);
    else
        return NULL;
}

std::string S2E::getOutputFilename(const std::string &fileName)
{
    llvm::sys::Path filePath(m_outputDirectory);
    filePath.appendComponent(fileName);
    return filePath.toString();
}

std::ostream* S2E::openOutputFile(const std::string &fileName)
{
    std::ios::openmode io_mode = std::ios::out | std::ios::trunc
                                  | std::ios::binary;

    std::string path = getOutputFilename(fileName);
    std::ostream *f = new std::ofstream(path.c_str(), io_mode);
    if (!f) {
        std::cerr << "ERROR: out of memory" << std::endl;
        exit(1);
    } else if (!f->good()) {
        std::cerr << "ERROR: can not open file '" << path << "'" << std::endl;
        exit(1);
    }

    return f;
}

void S2E::initOutputDirectory(const string& outputDirectory)
{
    if (outputDirectory.empty()) {
        llvm::sys::Path cwd(".");

        for (int i = 0; ; i++) {
            ostringstream dirName;
            dirName << "s2e-out-" << i;

            llvm::sys::Path dirPath(cwd);
            dirPath.appendComponent(dirName.str());

            if(!dirPath.exists()) {
                m_outputDirectory = dirPath.toString();
                break;
            }
        }

#ifndef _WIN32
        llvm::sys::Path s2eLast(cwd);
        s2eLast.appendComponent("s2e-last");

        if ((unlink(s2eLast.c_str()) < 0) && (errno != ENOENT)) {
            perror("ERRPR: Cannot unlink s2e-last");
            exit(1);
        }

        if (symlink(m_outputDirectory.c_str(), s2eLast.c_str()) < 0) {
            perror("ERROR: Cannot make symlink s2e-last");
            exit(1);
        }
#endif

    } else {
        m_outputDirectory = outputDirectory;
    }

    std::cout << "S2E: output directory = \"" << m_outputDirectory << "\"\n";

    if(mkdir(m_outputDirectory.c_str(), 0775) < 0) {
        perror("ERROR: Unable to create output directory");
        exit(1);
    }

    m_infoFile = openOutputFile("info");
    m_messagesFile = openOutputFile("messages.txt");
    m_warningsFile = openOutputFile("warnings.txt");

    m_warningsStreamBuf = new WarningsStreamBuf(m_warningsFile->rdbuf());
    m_warningsFile->rdbuf(m_warningsStreamBuf);
}

void S2E::initKlee()
{
#if 0
    m_kleeHandler = new KleeHandler(this);
    klee::Interpreter::InterpreterOptions IOpts;
    m_kleeInterpreter = klee::Interpreter::create(IOpts, m_kleeHandler.get());

    klee::Interpreter::ModuleOptions MOpts(KLEE_LIBRARY_DIR,
                        /* Optimize= */ false, /* CheckDivZero= */ false);
#endif
}

void S2E::initPlugins()
{
    m_pluginsFactory = new PluginsFactory();

    m_corePlugin = dynamic_cast<CorePlugin*>(
            m_pluginsFactory->createPlugin(this, "CorePlugin"));
    assert(m_corePlugin);

    m_activePluginsList.push_back(m_corePlugin);
    m_activePluginsMap.insert(
            make_pair(m_corePlugin->getPluginInfo()->name, m_corePlugin));
    if(!m_corePlugin->getPluginInfo()->functionName.empty())
        m_activePluginsMap.insert(
            make_pair(m_corePlugin->getPluginInfo()->functionName, m_corePlugin));

    vector<string> pluginNames = getConfig()->getStringList("plugins");

    /* Check and load plugins */
    foreach(const string& pluginName, pluginNames) {
        const PluginInfo* pluginInfo = m_pluginsFactory->getPluginInfo(pluginName);
        if(!pluginInfo) {
            std::cerr << "ERROR: plugin '" << pluginName
                      << "' does not exists in this S2E installation" << std::endl;
            exit(1);
        } else if(getPlugin(pluginInfo->name)) {
            std::cerr << "ERROR: plugin '" << pluginInfo->name
                      << "' was already loaded "
                      << "(is it enabled multiple times ?)" << std::endl;
            exit(1);
        } else if(!pluginInfo->functionName.empty() &&
                    getPlugin(pluginInfo->functionName)) {
            std::cerr << "ERROR: plugin '" << pluginInfo->name
                      << "' with function '" << pluginInfo->functionName
                      << "' can not be loaded because" << std::endl
                      <<  "    this function is already provided by '"
                      << getPlugin(pluginInfo->functionName)->getPluginInfo()->name
                      << "' plugin" << std::endl;
            exit(1);
        } else {
            Plugin* plugin = m_pluginsFactory->createPlugin(this, pluginName);
            assert(plugin);

            m_activePluginsList.push_back(plugin);
            m_activePluginsMap.insert(
                    make_pair(plugin->getPluginInfo()->name, plugin));
            if(!plugin->getPluginInfo()->functionName.empty())
                m_activePluginsMap.insert(
                    make_pair(plugin->getPluginInfo()->functionName, plugin));
        }
    }

    /* Check dependencies */
    foreach(Plugin* p, m_activePluginsList) {
        foreach(const string& name, p->getPluginInfo()->dependencies) {
            if(!getPlugin(name)) {
                std::cerr << "ERROR: plugin '" << p->getPluginInfo()->name
                          << "' depends on plugin '" << name
                          << "' which is not enabled in config" << std::endl;
                exit(1);
            }
        }
    }

    /* Initialize plugins */
    foreach(Plugin* p, m_activePluginsList) {
        p->initialize();
    }
}

} // namespace s2e

/******************************/
/* Functions called from QEMU */

extern "C" {

S2E* g_s2e = NULL;

S2E* s2e_initialize(const char* s2e_config_file,  const char* s2e_output_dir)
{
    return new S2E(s2e_config_file ? s2e_config_file : "",
                   s2e_output_dir  ? s2e_output_dir  : "");
}

void s2e_close(S2E *s2e)
{
    delete s2e;
}

} // extern "C"
