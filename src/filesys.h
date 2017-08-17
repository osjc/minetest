/*
Minetest
Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#pragma once

#include <string>
#include <vector>
#include "exceptions.h"

#ifdef _WIN32 // WINDOWS
#define DIR_DELIM "\\"
#define DIR_DELIM_CHAR '\\'
#define FILESYS_CASE_INSENSITIVE 1
#define PATH_DELIM ";"
#else // POSIX
#define DIR_DELIM "/"
#define DIR_DELIM_CHAR '/'
#define FILESYS_CASE_INSENSITIVE 0
#define PATH_DELIM ":"
#endif

namespace fs
{

struct DirListNode
{
	std::string name;
	bool dir;
};

std::vector<DirListNode> GetDirListing(const std::string &path);

// Returns true if already exists
bool CreateDir(const std::string &path);

bool PathExists(const std::string &path);

bool IsPathAbsolute(const std::string &path);

bool IsDir(const std::string &path);

bool IsDirDelimiter(char c);

// Only pass full paths to this one. True on success.
// NOTE: The WIN32 version returns always true.
bool RecursiveDelete(const std::string &path);

bool DeleteSingleFileOrEmptyDirectory(const std::string &path);

// Returns path to temp directory, can return "" on error
std::string TempPath();

/* Multiplatform */

// The path itself not included
void GetRecursiveSubPaths(const std::string &path, std::vector<std::string> &dst);

// Tries to delete all, returns false if any failed
bool DeletePaths(const std::vector<std::string> &paths);

// Only pass full paths to this one. True on success.
bool RecursiveDeleteContent(const std::string &path);

// Create all directories on the given path that don't already exist.
bool CreateAllDirs(const std::string &path);

// Copy a regular file
bool CopyFileContents(const std::string &source, const std::string &target);

// Copy directory and all subdirectories
// Omits files and subdirectories that start with a period
bool CopyDir(const std::string &source, const std::string &target);

// Check if one path is prefix of another
// For example, "/tmp" is a prefix of "/tmp" and "/tmp/file" but not "/tmp2"
// Ignores case differences and '/' vs. '\\' on Windows
bool PathStartsWith(const std::string &path, const std::string &prefix);

// Remove last path component and the dir delimiter before and/or after it,
// returns "" if there is only one path component.
// removed: If non-NULL, receives the removed component(s).
// count: Number of components to remove
std::string RemoveLastPathComponent(const std::string &path,
               std::string *removed = NULL, int count = 1);

// Remove "." and ".." path components and for every ".." removed, remove
// the last normal path component before it. Unlike AbsolutePath,
// this does not resolve symlinks and check for existence of directories.
std::string RemoveRelativePathComponents(std::string path);

// Returns the absolute path for the passed path, with "." and ".." path
// components and symlinks removed.  Returns "" on error.
std::string AbsolutePath(const std::string &path);

// Returns the filename from a path or the entire path if no directory
// delimiter is found.
const char *GetFilenameFromPath(const char *path);

bool safeWriteToFile(const std::string &path, const std::string &content);

bool Rename(const std::string &from, const std::string &to);

} // namespace fs
