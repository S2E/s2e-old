/*
 * FileUtil.cpp
 *
 *  Created on: 2015年12月23日
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
		//把当前目录.，上一级目录.. 避免死循环遍历目录
		if (strncmp(file->d_name, ".", 1) == 0)
			continue;
		//判断该文件是否是目录
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
		//把当前目录.，上一级目录.. 避免死循环遍历目录
		if (strncmp(file->d_name, ".", 1) == 0)
			continue;
		//判断该文件是否是目录
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
		//把当前目录.，上一级目录.. 避免死循环遍历目录
		if (strncmp(file->d_name, ".", 1) == 0)
			continue;
		//判断该文件是否是目录
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

	/* 打开源文件 */
	if ((from_fd = open(fromfile, O_RDONLY)) == -1) /*open file readonly,返回-1表示出错，否则返回文件描述符*/
	{
		fprintf(stderr, "\nOpen %s Error:%s\n", fromfile, strerror(errno));
		return false;
	}
	/* 创建目的文件 */
	/* 使用了O_CREAT选项-创建文件,open()函数需要第3个参数,
	 mode=S_IRUSR|S_IWUSR表示S_IRUSR 用户可以读 S_IWUSR 用户可以写*/
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
	/* 以下代码是一个经典的拷贝文件的代码 */
	while ((bytes_read = read(from_fd, buffer, BUFFER_SIZE))) {
		/* 一个致命的错误发生了 */
		if ((bytes_read == -1) && (errno != EINTR))
			break;
		else if (bytes_read > 0) {
			ptr = buffer;
			while ((bytes_write = write(to_fd, ptr, bytes_read))) {
				/* 一个致命错误发生了 */
				if ((bytes_write == -1) && (errno != EINTR))
					break;
				/* 写完了所有读的字节 */
				else if (bytes_write == bytes_read)
					break;
				/* 只写了一部分,继续写 */
				else if (bytes_write > 0) {
					ptr += bytes_write;
					bytes_read -= bytes_write;
				}
			}
			/* 写的时候发生的致命错误 */
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
		//把当前目录.，上一级目录.. 避免死循环遍历目录
		if (strncmp(file->d_name, ".", 1) == 0)
			continue;
		//判断该文件是否是目录
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

