//
//  injection.hpp
//  bypass
//
//  Created by Jon Gabilondo on 3/15/17.
//

#ifndef injection_hpp
#define injection_hpp

#include <stdio.h>

int inject_framework(const boost::filesystem::path &appPath, const boost::filesystem::path &argFrameworkPath, bool input_file_is_mac);
void inject_dylib(FILE* newFile, uint32_t top, const boost::filesystem::path& dylibPath);
int patch_binary(const boost::filesystem::path & binaryPath, const boost::filesystem::path & dllPath, bool isFramework, bool isMacFile);

#endif /* injection_hpp */
