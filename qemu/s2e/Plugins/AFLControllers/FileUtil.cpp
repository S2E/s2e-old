/*
 * FileUtil.cpp
 *
 *  Created on: 2015.12.24
 *      Author: Epeius
 */

#include "FileUtil.h"
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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/io.h>
#include <dirent.h>
#include <unistd.h>

FileUtil::FileUtil() {
}

FileUtil::~FileUtil() {
}
int FileUtil::exists(const char *filename){
return access(filename, F_OK);
}
int FileUtil::CreateParentDir(const char *sPathName) {
	char DirName[512];
	strcpy(DirName, sPathName);
	int len = strlen(DirName);
	do{
		len--;
		if (DirName[len] == '/'){
			DirName[len+1] = '\0';
			 len = strlen(DirName);
			break;
		}
	}while(len > 1);
return  FileUtil::CreateMultiDir(DirName);
}
int FileUtil::CreateMultiDir(const char *sPathName) {
	char DirName[512];
	strcpy(DirName, sPathName);
	int i, len = strlen(DirName);
	if (DirName[len - 1] != '/')
		strcat(DirName, "/");

	len = strlen(DirName);

	for (i = 1; i < len; i++) {
		if (DirName[i] == '/') {
			DirName[i] = 0;
			if (access(DirName, F_OK) != 0) {
				if (mkdir(DirName, S_IRWXU) < 0 && (errno != EEXIST)) {
					perror("mkdir   error");
					return -1;
				}
			}
			DirName[i] = '/';
		}
	}
	return 0;
}
int FileUtil::remove_file(const char* path) {
	int filecounter = 0;
	DIR *d;
	struct dirent *file;
	struct stat sb;

	if (!(d = opendir(path))) {
		printf("error opendir %s...\n", path);
		return -1;
	}
	while ((file = readdir(d)) != NULL) {
		if (strncmp(file->d_name, ".", 1) == 0)
			continue;
		//determine whether is a directory
		std::stringstream fullfilename;
		fullfilename << path << file->d_name;
		if (stat(fullfilename.str().c_str(), &sb) >= 0 && S_ISDIR(sb.st_mode)) {
		} else {
			remove(fullfilename.str().c_str());
			filecounter++;
		}
	}
	closedir(d);
	return filecounter;
}

int FileUtil::count_dir(const char* path, std::set<std::string>& dirs) {
	int dircounter = 0;
	DIR *d;
	struct dirent *file;
	struct stat sb;

	if (!(d = opendir(path))) {
		printf("error opendir %s...\n", path);
		return -1;
	}
	while ((file = readdir(d)) != NULL) {
		if (strncmp(file->d_name, ".", 1) == 0)
			continue;
		std::stringstream fullfilename;
		fullfilename << path << file->d_name;
		if (stat(fullfilename.str().c_str(), &sb) >= 0 && S_ISDIR(sb.st_mode)) {
			dirs.insert(fullfilename.str());
			dircounter++;
		}
	}
	closedir(d);
	return dircounter;
}
int FileUtil::count_file(const char* path, std::vector<std::string>& files) {
	int filecounter = 0;
	DIR *d;
	struct dirent *file;
	struct stat sb;

	if (!(d = opendir(path))) {
		printf("error opendir %s...\n", path);
		return -1;
	}
	while ((file = readdir(d)) != NULL) {
		if (strncmp(file->d_name, ".", 1) == 0)
			continue;
		std::stringstream fullfilename;
		fullfilename << path << file->d_name;
		if (stat(fullfilename.str().c_str(), &sb) >= 0 && S_ISDIR(sb.st_mode)) {
		} else {
			files.push_back(fullfilename.str());
			filecounter++;
		}
	}
	closedir(d);
	return filecounter;
}
#define BUFFER_SIZE 4
bool FileUtil::copyfile(const char* fromfile, const char* tofile,
		bool deletesrc,bool trunc) {
	int from_fd, to_fd;
	int bytes_read, bytes_write;
	char buffer[BUFFER_SIZE];
	char *ptr;

	/* open source */
	if ((from_fd = open(fromfile, O_RDONLY)) == -1) /*open file readonly*/
	{
		fprintf(stderr, "\nOpen %s Error:%s\n", fromfile, strerror(errno));
		return false;
	}
	if(trunc){
		if ((to_fd = open(tofile, O_WRONLY| O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR)) == -1) {
				fprintf(stderr, "\nOpen %s Error:%s\n", tofile, strerror(errno));
				return false;
			}
	}else{
		if ((to_fd = open(tofile, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR)) == -1) {
			fprintf(stderr, "\nOpen %s Error:%s\n", tofile, strerror(errno));
			return false;
		}
	}
	while ((bytes_read = read(from_fd, buffer, BUFFER_SIZE))) {
		if ((bytes_read == -1) && (errno != EINTR))
			break;
		else if (bytes_read > 0) {
			ptr = buffer;
			while ((bytes_write = write(to_fd, ptr, bytes_read))) {
				if ((bytes_write == -1) && (errno != EINTR))
					break;
				else if (bytes_write == bytes_read)
					break;
				else if (bytes_write > 0) {
					ptr += bytes_write;
					bytes_read -= bytes_write;
				}
			}
			if (bytes_write == -1)
				break;
		}
	}
	close(from_fd);
	close(to_fd);
	if (deletesrc) {
		if (remove(fromfile) == 0) {
			//printf("\nRemoved %s succeed.\n", fromfile);
		} else {
			//printf("\nRemoved %s failed.\n", fromfile);
		}
	}
	return true;
}
bool FileUtil::renamefile(const char* fromfile, const char* tofile){
	rename(fromfile,tofile);
	return true;
}
bool FileUtil::cleardir(const char* dir) {
	DIR *d;
	struct dirent *file;
	struct stat sb;

	if (!(d = opendir(dir))) {
		printf("error opendir %s...\n", dir);
		return false;
	}
	while ((file = readdir(d)) != NULL) {
		if (strncmp(file->d_name, ".", 1) == 0)
			continue;
		std::stringstream fullfilename;
		fullfilename << dir << file->d_name;
		if (stat(fullfilename.str().c_str(), &sb) >= 0 && S_ISDIR(sb.st_mode)) {
		} else {
			remove(fullfilename.str().c_str());
		}
	}
	closedir(d);
	return true;
}

