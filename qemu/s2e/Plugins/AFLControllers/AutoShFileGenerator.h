/*
 * AutoShFileGenerator.h
 *
 *  Created on: 2015.12.23
 *      Author: Epeius
 */

#ifndef AUTOSHFILEGENERATOR_H_
#define AUTOSHFILEGENERATOR_H_
#include <string>
#include <s2e/Plugin.h>
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/S2EExecutionState.h>
namespace s2e {
namespace plugins {
/**
 * Generate an auto start shell script to avoid tedious manual work
 */
class AutoShFileGenerator: public Plugin {
	S2E_PLUGIN

    typedef std::pair<std::string, std::vector<unsigned char> > VarValuePair;
    typedef std::vector<VarValuePair> ConcreteInputs;
	std::string m_command_str;  //cmdline string
	std::string m_command_file; // where to generate the file
	std::string m_current_command;
	void onStateKill(S2EExecutionState* state) ;

	std::string m_randtemplate;

public:
	AutoShFileGenerator(S2E* s2e);
	void initialize();
	void generateCmdFile() ;
	virtual ~AutoShFileGenerator();
	std::string replace_randtemplate(std::string in_str);
	std::string getCurrentCommand(){
		return m_current_command;
	}
};

} /* namespace plugins */
} /* namespace s2e */

#endif /* !AUTOSHFILEGENERATOR_H_ */
