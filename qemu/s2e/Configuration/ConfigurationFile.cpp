/**
 *  The configuration file contains sections with pair/value keys
 *  [SECTION1]
 *  key1=val1
 *  ...
 *  [SECTION2]
 */

#include "ConfigurationFile.h"
#include <s2e/Utils.h>
#include <ctype.h>

using namespace std;


ConfigurationFile::ConfigurationFile(const std::string &File)
{
  m_ConfigFileName = File;
  ParseConfigurationFile();
}

ConfigurationFile::~ConfigurationFile()
{
  foreach (it, m_Sections.begin(), m_Sections.end()) {
    delete (*it).second;
  }
}

ConfigSection* ConfigurationFile::GetOrCreateSection(const std::string &SectionName)
{
  Sections::iterator it = m_Sections.find(SectionName);
  if (it != m_Sections.end()) {
    return (*it).second;
  }

  ConfigSection *Section = ConfigSection::SectionFactory(SectionName);
  if (Section) {
    m_Sections[SectionName] = Section;
  }
  return Section;
}

/**
 *  Returns a line and its type
 */
bool ConfigurationFile::ReadLine(std::string &Line, ELineType &LineType)
{
  if (m_ConfigFile.eof()) {
    return false;
  }

  std::getline(m_ConfigFile, Line);

  LineType = BLANK;
  for(unsigned i=0; i<Line.length(); i++) {
    if (isspace(Line[i])) {
      continue;
    }else if (Line[i] == '[') {
      LineType = SECTION_HEADER;
      return true;
    }else if (Line[i] == ';') {
      LineType = COMMENT;
      return true;
    }else {
      LineType = KEY_VALUE;
    }
  }

  return true;
}

bool ConfigurationFile::ReadSectionName(const std::string &Line, std::string &Name) const
{
  unsigned State = 0;
  Name = "";

  for (unsigned i=0; i<Line.length(); i++) {
    switch(State) {
      case 0:
          if (Line[i] == '[') {
            State = 1;
          }else if (!isspace(Line[i])) {
            return false;
          }
          break;
      case 1:
          if (Line[i] == ']') {
            return true;
          }
          Name += Line[i];
          break;
    }
  }
  return false;
}

void ConfigurationFile::ReadKeyValue(const std::string &Line, std::string &Key, std::string &Value) const
{
  unsigned State = 0;
  Key = "";
  Value = "";
  for (unsigned i=0; i<Line.length(); i++) {
    switch(State) {
      case 0: 
        if (!isspace(Line[i])) {
            State = 1;
            Key += Line[i];
        }
        break;

      case 1:
        if (Line[i] == '=') {
          State = 2;
        }else {
          Key += Line[i];
        }
        break;
      
      case 2:
        Value += Line[i];
        break;
    }
  }
}

bool ConfigurationFile::ParseConfigurationFile()
{
  std::string Line, SectionName, Key, Value;
  ELineType LineType = UNKNOWN;
  ConfigSection *CurrentSection = NULL;

  assert(!m_ConfigFile.is_open());

  m_ConfigFile.open(m_ConfigFileName.c_str());
  if (!m_ConfigFile.is_open()) {
    std::cerr << "Could not open configuration file " << m_ConfigFileName << std::endl;
    return false;
  }

  while(ReadLine(Line, LineType)) {
    switch (LineType) {
      case BLANK: 
      case COMMENT:      
        break;

      case SECTION_HEADER:
        ReadSectionName(Line, SectionName);
        CurrentSection = GetOrCreateSection(SectionName);
        break;

      case KEY_VALUE:
        if (CurrentSection) {
          ReadKeyValue(Line, Key, Value);
          CurrentSection->InsertKeyValue(Key, Value);
        }
        break;

      default:
        break;
    }
  }

  m_ConfigFile.close();

  return true;
}

bool ConfigurationFile::GetValue(const std::string &Section, const std::string &Key,
    std::string &Value) const
{
  const ConfigSection *S = GetSection(Section);
  if (!S) {
    return false;
  }

  return S->GetValue(Key, Value);
}

const ConfigSection *ConfigurationFile::GetSection(const std::string &Name) const
{
  Sections::const_iterator it = m_Sections.find(Name);
  if (it != m_Sections.end()) {
    return (*it).second;
  }
  return NULL;
}


ConfigSection::ConfigSection()
{

}

ConfigSection::~ConfigSection()
{

}

ConfigSection *ConfigSection::SectionFactory(const std::string &Name)
{
  /*if (!Name.compare(SECTION_ENABLED_FUNCTION_HANDLERS)) {
    return new ConfigFunctionHandlers();
  }*/
  return new ConfigSection();
}

bool ConfigSection::HasKey(const std::string &Key) const
{
  return m_KeyValue.find(Key) != m_KeyValue.end();
}


void ConfigSection::InsertKeyValue(const std::string &Key, const std::string &Value)
{
  m_KeyValue[Key] = Value;
}


bool ConfigSection::GetValue(const std::string &Key, std::string &Value) const
{
  KeyValueMap::const_iterator it = m_KeyValue.find(Key);
  if (it == m_KeyValue.end()) {
    return false;
  }

  Value = (*it).second;
  return true;
}
