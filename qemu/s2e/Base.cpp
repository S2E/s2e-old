#include <s2e/Base.h>

#include <llvm/System/Path.h>

#include <klee/Common.h>
#include <klee/ExecutionState.h>

#include <iostream>
#include <fstream>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>

extern "C" {
  /* command line arguments */
  const char* s2e_output_dir = NULL;
};

using namespace llvm;
using namespace klee;

BaseHandler::BaseHandler()
{
  std::string theDir;

  if (!s2e_output_dir) {
    llvm::sys::Path directory(".");
    std::string dirname = "";

    for (int i = 0; ; i++) {
      char buf[256], tmp[64];
      sprintf(tmp, "s2e-out-%d", i);
      dirname = tmp;
      sprintf(buf, "%s/%s", directory.c_str(), tmp);
      theDir = buf;

      if (DIR *dir = opendir(theDir.c_str())) {
        closedir(dir);
      } else {
        break;
      }
    }

    std::cerr << "S2E: output directory = \"" << dirname << "\"\n";

    llvm::sys::Path s2e_last(directory);
    s2e_last.appendComponent("s2e-last");

    if ((unlink(s2e_last.c_str()) < 0) && (errno != ENOENT)) {
      perror("Cannot unlink s2e-last");
      assert(0 && "exiting.");
    }

    if (symlink(dirname.c_str(), s2e_last.c_str()) < 0) {
      perror("Cannot make symlink");
      assert(0 && "exiting.");
    }
  } else {
    theDir = s2e_output_dir;
  }

  llvm::sys::Path p(theDir);
  if (!p.isAbsolute()) {
    sys::Path cwd = sys::Path::GetCurrentDirectory();
    cwd.appendComponent(theDir);
    p = cwd;
  }
  strcpy(m_outputDirectory, p.c_str());

  if (mkdir(m_outputDirectory, 0775) < 0) {
    std::cerr << "S2E: ERROR: Unable to make output directory: \""
               << m_outputDirectory
               << "\", refusing to overwrite.\n";
    exit(1);
  }

  char fname[1024];
  snprintf(fname, sizeof(fname), "%s/warnings.txt", m_outputDirectory);
  klee_warning_file = fopen(fname, "w");
  assert(klee_warning_file);

  snprintf(fname, sizeof(fname), "%s/messages.txt", m_outputDirectory);
  klee_message_file = fopen(fname, "w");
  assert(klee_message_file);

  m_infoFile = openOutputFile("info");
}

BaseHandler::~BaseHandler()
{
  delete m_infoFile;
}

std::string BaseHandler::getOutputFilename(const std::string &filename) {
  char outfile[1024];
  sprintf(outfile, "%s/%s", m_outputDirectory, filename.c_str());
  return outfile;
}

std::ostream *BaseHandler::openOutputFile(const std::string &filename) {
  std::ios::openmode io_mode = std::ios::out | std::ios::trunc
                             | std::ios::binary;
  std::ostream *f;
  std::string path = getOutputFilename(filename);
  f = new std::ofstream(path.c_str(), io_mode);
  if (!f) {
    klee_warning("out of memory");
  } else if (!f->good()) {
    klee_warning("error opening: %s", filename.c_str());
    delete f;
    f = NULL;
  }

  return f;
}

/* Outputs all files (.ktest, .pc, .cov etc.) describing a test case */
void BaseHandler::processTestCase(const ExecutionState &state,
                                  const char *errorMessage,
                                  const char *errorSuffix) {
  klee_warning("Terminating state %p with error message '%s'",
                &state, errorMessage ? errorMessage : "");
}
