#ifndef S2ETOOLS_STATIC_TRANSLATOR_H_

#define S2ETOOLS_STATIC_TRANSLATOR_H_

#include <ostream>
#include <fstream>

#include <lib/BinaryReaders/Library.h>


namespace s2etools
{

class InvalidAddressException {

};

class StaticTranslatorTool {
private:
    static bool s_translatorInited;
    BFDInterface *m_binary;

    //Outputs raw x86 translated code here
    std::ofstream *m_translatedCode;

public:
    StaticTranslatorTool();
    ~StaticTranslatorTool();
    void translate();
};

}

#endif
