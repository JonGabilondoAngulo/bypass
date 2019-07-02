//
//  file.hpp
//  bypass
//
//  Created by Jon Gabilondo on 28/02/2017.
//

#ifndef file_hpp
#define file_hpp

#include <stdio.h>

bool create_file(const boost::filesystem::path & filePath, const std::string & content, bool removeFirst);
bool file_is_app(const boost::filesystem::path & filePath);
bool file_is_ipa(const boost::filesystem::path & filePath);
bool file_is_mac_app(const boost::filesystem::path & filePath);

#endif /* file_hpp */
