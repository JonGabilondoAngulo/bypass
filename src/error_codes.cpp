//
//  error_codes.cpp
//  bypass
//
//  Created by Jon Gabilondo on 3/15/17.
//

#include <stdio.h>

#define ERR_General_Error 99 // General error, investigate logs
#define ERR_Bad_Arguments 100
#define ERR_Wrong_Use 101 // Wrong use of bypass
#define ERR_App_Codesign_Failed 103 // Codesign process failed, general cause
#define ERR_Dylib_Codesign_Failed 104 // Dylib Codesign process failed, general cause
#define ERR_Injection_Failed 105 // Dylib injection process failed
#define ERR_TeamIdentifier_Validation_Failed 106 // Team identifier might be empty / euqal to 'not set' / app binary and dll has different team identifiers
