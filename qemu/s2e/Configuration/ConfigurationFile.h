#ifndef _CONFIGURATION_READER_

#define _CONFIGURATION_READER_

#include <set>
#include <map>
#include <string>
#include <string>
#include <iostream>
#include <fstream>

#define SECTION_ENABLED_FUNCTION_HANDLERS "ENABLED_FUNCTION_HANDLERS"

class ConfigSection;

class ConfigurationFile
{
public:
  typedef std::map<std::string, ConfigSection*> Sections;

private:
  
  typedef enum _ELineType {
    UNKNOWN, BLANK, COMMENT, SECTION_HEADER, KEY_VALUE
  }ELineType;


  

  bool ReadLine(std::string &Line, ELineType &LineType);
  bool ReadSectionName(const std::string &Line, std::string &Name) const;
  void ReadKeyValue(const std::string &Line, std::string &Key, std::string &Value) const;

  ConfigSection *GetOrCreateSection(const std::string &SectionName);

  bool ParseConfigurationFile();

  std::string m_ConfigFileName;
  std::ifstream m_ConfigFile;
  Sections m_Sections;

public:
  ConfigurationFile(const std::string &ConfigFile);
  ~ConfigurationFile();

  const ConfigSection *GetSection(const std::string &Name) const;
  bool GetValue(const std::string &Section, const std::string &Key,
    std::string &Value)const;
};

class ConfigSection
{
public:
  typedef std::map<std::string, std::string> KeyValueMap;

private:
  KeyValueMap m_KeyValue;

public:
  ConfigSection();
  virtual ~ConfigSection();
  static ConfigSection *SectionFactory(const std::string &Name);
  void InsertKeyValue(const std::string &Key, const std::string &Value);
  bool HasKey(const std::string &Key) const;
  bool GetValue(const std::string &Key, std::string &Value) const;
};



#endif
