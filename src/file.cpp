//
//  file.cpp
//  bypass
//
//  Created by Jon Gabilondo on 28/02/2017.
//

#include "file.hpp"

bool create_file(const boost::filesystem::path & filePath, const std::string & content, bool removeFirst)
{
    if (removeFirst && boost::filesystem::exists(filePath)) {
        boost::filesystem::remove(filePath);
    }
    
    boost::filesystem::ofstream fileStream(filePath);
    fileStream << content.data();
    
    return true;
}

bool file_is_app(const boost::filesystem::path & filePath)
{
    return (filePath.extension().compare(".app") == 0);
}

bool file_is_ipa(const boost::filesystem::path & filePath)
{
    return (filePath.extension().compare(".ipa") == 0);
}

bool file_is_mac_app(const boost::filesystem::path & filePath)
{
    if (!file_is_app(filePath)) {
        return false;
    }
    boost::filesystem::path macos_folder = filePath / "Contents" / "MacOS";
    return boost::filesystem::is_directory(macos_folder);
}


