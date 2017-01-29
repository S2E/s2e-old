/*
 * FileUtil.h
 *
 *  Created on: 2015年12月23日
 *      Author: Epeius
 */

#ifndef FILEUTIL_H_
#define FILEUTIL_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <fstream>
#include <vector>
#include <iostream>
#include <sstream>
#include <map>
#include <set>
#include <vector>
class FileUtil {
public:
	FileUtil();
	virtual ~FileUtil();
	static int exists(const char *filename);
	static int CreateParentDir(const char *filename);
	static int CreateMultiDir(const char *sPathName);
	static int remove_file(const char* path);
	static int count_dir( const char* path, std::set<std::string>& dirs);
	static int count_file(const char* path, std::vector<std::string>& files);
	static bool copyfile(const char* fromfile, const char* tofile, bool deletesrc,bool trunc);
	static bool renamefile(const char* fromfile, const char* tofile);
	static bool cleardir(const char* dir);
};

#endif /* FILEUTIL_H_ */
