//
//  file.hpp
//  bypass
//
//  Created by Jon Gabilondo on 28/02/2017.
//

#ifndef file_hpp
#define file_hpp

#include <stdio.h>

bool createFile(const boost::filesystem::path & filePath, const std::string & content, bool removeFirst);

#endif /* file_hpp */
