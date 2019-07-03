//
//  codesign.cpp
//  bypass
//
//  Created by Jon Gabilondo on 3/15/17.
//

#include "codesign.hpp"

const std::string sResourceRulesTemplate = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n\
<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n\
<plist version=\"1.0\">\n\
<dict>\n\
<key>rules</key>\n\
<dict>\n\
<key>.*</key>\n\
<true/>\n\
<key>Info.plist</key>\n\
<dict>\n\
<key>omit</key>\n\
<true/>\n\
<key>weight</key>\n\
<real>10</real>\n\
</dict>\n\
<key>ResourceRules.plist</key>\n\
<dict>\n\
<key>omit</key>\n\
<true/>\n\
<key>weight</key>\n\
<real>100</real>\n\
</dict>\n\
</dict>\n\
</dict>\n\
</plist>";

#pragma mark - Codesign functions

int codesign_app(const boost::filesystem::path &appPath, const boost::filesystem::path &provisionPath, const boost::filesystem::path & resourceRulesPath, const boost::filesystem::path &certificatePath, const boost::filesystem::path &entitlementsPath, const boost::filesystem::path &tempDirectoryPath, bool removeEntitlements, bool removeCodeSignature, bool input_file_is_ipa, bool input_file_is_mac, bool copyEntitlements, bool useResourceRules, bool forceResRules, bool useOriginalResRules, bool useGenericResRules)
{
    int err = 0;
    std::string systemCmd;
    
    if (!provisionPath.empty()) {
        err = copy_provision_file(appPath, provisionPath);
        if (err) {
            return ERR_General_Error;
        }
    }
    
    if (removeEntitlements) {
        err = remove_entitlements(appPath);
    }
    
    if (removeCodeSignature) {
        err = remove_codesign(appPath);
    }
    
    systemCmd = (std::string)"/usr/bin/codesign -f --deep -s '" + (std::string)certificatePath.c_str() + (std::string)"' " ;
    
    // Entitlements
    boost::filesystem::path entitlementsFilePath;
    if (input_file_is_ipa) {
        // Important entitlements info:
        // no entitlements provided in arguments, extract them from mobile provision
        // the provision file could be given in argument, if not extract it from the embeded.mobileprovision
        // if no embeded.mobileprovision then we can't do anything .. the codesign willprobably not build a good ipa, this will be more visible in 8.1.3
        
        if (copyEntitlements) {
            entitlementsFilePath = entitlementsPath;
        } else {
            std::string pathToProvision;
            
            if (!provisionPath.empty()) {
                pathToProvision = provisionPath.c_str();
            } else {
                pathToProvision = appPath.string() + "/" + "embedded.mobileprovision";
            }
            
            if (boost::filesystem::exists(pathToProvision)) {
                err = extract_entitlements_from_mobile_provision(pathToProvision, entitlementsFilePath, tempDirectoryPath);
            }
        }
        if (!entitlementsFilePath.empty()) {
            ORGLOG_V("Codesign using entitlements. " << entitlementsFilePath);
            systemCmd += (std::string)" --entitlements='" + entitlementsFilePath.string() + "'";
        } else {
            ORGLOG("Failed to extract entitlements from provision file, codesign will not use entitlements.");
        }
    }
    
    // Resource rules
    std::string resRulesFile;
    if (useResourceRules) {
        resRulesFile = resourceRulesPath.c_str();
    } else {
        resRulesFile = resource_rules_file(forceResRules, useOriginalResRules, useGenericResRules, appPath, tempDirectoryPath);
    }
    
    if (resRulesFile.length()) {
        ORGLOG_V("Codesign using resource rules file :" << (std::string)resRulesFile);
        systemCmd += (std::string)" --resource-rules='" + resRulesFile + "'";
    }
    
    systemCmd += " \"" + appPath.string() + "\"";
    
    // Codesign dylibs
    if (codesign_at_path(appPath, certificatePath, entitlementsFilePath)) {
        ORGLOG_V("Dylibs codesign sucessful!");
    } else {
        err = ERR_Dylib_Codesign_Failed;
        ORGLOG("Codesigning one or more dylibs failed");
    }
    
    // Codesigning extensions under plugins folder
    boost::filesystem::path extensionsFolderPath = appPath;
    extensionsFolderPath.append("PlugIns");
    if (codesign_at_path(extensionsFolderPath, certificatePath, entitlementsFilePath)) {
        ORGLOG_V("Extensions codesign sucessful!");
    }
    
    // Codesigning frameworks
    boost::filesystem::path frameworksFolderPath = appPath;
    if (input_file_is_mac) {
        frameworksFolderPath.append("Contents"); // os x
    }
    frameworksFolderPath.append("Frameworks");
    if (codesign_at_path(frameworksFolderPath, certificatePath, entitlementsFilePath)) {
        ORGLOG_V("Frameworks codesign sucessful!");
    }
    
    // silence output
    //systemCmd += " " + silenceCmdOutput;
    
    // execute codesign
    err = system((const char*)systemCmd.c_str());
    if (err) {
        err = ERR_App_Codesign_Failed;
        ORGLOG("Codesigning app failed.");
    }
    
    return err;
}

bool codesign_dylib(const boost::filesystem::path & appPath,
                    const std::string& dllName,
                    const boost::filesystem::path& certificateFilePath,
                    const boost::filesystem::path& entitlementsFilePath)
{
    bool result = false;
    
    if (certificateFilePath.string().length() && appPath.string().length() && dllName.length()) {
        std::string systemCmd = (std::string)"/usr/bin/codesign -f -s '" + certificateFilePath.string() + "'" + silenceCmdOutput;
        
        // add entitlements to cmd
        if (entitlementsFilePath.string().length()) {
            systemCmd += (std::string)" --entitlements='" + entitlementsFilePath.string() + "' ";
        }
        
        // add dllpath to cmd
        systemCmd += " \"" + appPath.string() + "/" + dllName + "\"";
        
        int err = system(systemCmd.c_str());
        result = err==noErr;
        
        if (result) {
            ORGLOG_V("Success Codesigning dylib");
        } else {
            ORGLOG_V("Error: Failed to Codesign dylib");
        }
    }
    
    return result;
}

bool codesign_at_path(const boost::filesystem::path & appPath,
                      const boost::filesystem::path&  certificateFilePath,
                      const boost::filesystem::path&  entitlementsFilePath)
{
    bool success = false;
    
    // Opening directory stream to app path
    DIR* dirFile = opendir(appPath.c_str());
    if (dirFile)
    {
        // Iterating over all files in app's path
        struct dirent* aFile;
        while ((aFile = readdir(dirFile)) != NULL)
        {
            // Ignoring current directory and parent directory names
            if (!strcmp(aFile->d_name, ".")) continue;
            if (!strcmp(aFile->d_name, "..")) continue;
            
            // Checking if file extension is 'dylib' OR 'framework'
            if (strstr(aFile->d_name, ".dylib") || strstr(aFile->d_name, ".framework") || strstr(aFile->d_name, ".appex")){
                
                // Found .dylib file, codesigning it
                codesign_dylib(appPath, aFile->d_name, certificateFilePath, entitlementsFilePath);
            }
        }
        
        // Done iterating, closing directory stream
        closedir(dirFile);
        
        // Assuming all went right
        success = true;
    }
    
    return success;
}

int remove_codesign(const boost::filesystem::path& appPath)
{
    ORGLOG_V("Removing Code Signature");
    
    int err = 0;
    std::string pathToCodeSignature = appPath.string() + "/_CodeSignature";
    std::string systemCmd = (std::string)"rm -rf '" + (std::string)pathToCodeSignature + (std::string)"'";
    err = system((const char*)systemCmd.c_str());
    if(err)std::cout << "Error: Failed to remove codesignature file.\n";
    
    return err;
}

#pragma mark - Entitlements functions

int remove_entitlements(const boost::filesystem::path& appPath)
{
    ORGLOG_V("Removing Entitlements");
    
    int err = 0;
    std::string pathToEntitlements = appPath.string() + "/Entitlements.plist";
    std::string systemCmd = (std::string)"rm '" + (std::string)pathToEntitlements + (std::string)"'";
    err = system((const char*)systemCmd.c_str());
    if (err)std::cout << "Error: Failed to remove entitlements file.\n";
    
    return  err;
}

bool extract_entitlements_from_mobile_provision(const std::string& provisionFile,
                                                boost::filesystem::path& outNewEntitlementsFile,
                                                const boost::filesystem::path & tempDirPath)
{
    bool success = false;
    
    std::string decodedProvision = tempDirPath.string() + "/../hpDecoded.mobileProvision"; // don't write it in our temp dir we need to zip all its contents
    
    ORGLOG_V("Getting entitlements from mobile provision: " << decodedProvision);
    
    std::string cmsCommand = (std::string)"security cms -D -i '" +  provisionFile + "' -o '" + decodedProvision + "'";
    int err = system((const char*)cmsCommand.c_str());
    if (err) {
        std::cout << "Error: Failed to decode mobile provision: " << provisionFile << " to: " <<  decodedProvision << std::endl;
    } else {
        // extract the entitlements
        
        std::string newEntitlements = tempDirPath.string() + "/../hpNewEntitlement.plist"; // don't write it in our temp dir we need to zip all its contents
        
        std::string plutilCommand = (std::string)"plutil -extract Entitlements xml1 -o " + newEntitlements + " " + decodedProvision;
        err = system((const char*)plutilCommand.c_str());
        if (err) {
            std::cout << "Error: Failed to extract entitlements from decoded mobile provision: " << decodedProvision << "to: " <<  newEntitlements << std::endl;
            success = false;
        } else {
            outNewEntitlementsFile = newEntitlements;
            success = true;
        }
    }
    return success;
}

bool extract_entitlements_from_app(const std::string& appPath, std::string& newEntitlementsFile, const std::string& tempDirPath)
{
    bool success = false;
    
    std::string entitlementsXml = tempDirPath + "/../hpNewEntitlement.xml";
    
    ORGLOG_V("Getting entitlements from app:" << appPath);
    
    std::string cmsCommand = (std::string)"/usr/bin/codesign -d --entitlements :- " + appPath + " > " + entitlementsXml;
    int err = system((const char*)cmsCommand.c_str());
    if (err) {
        std::cout << "Error: Failed to get entitlements from app file: " << appPath << std::endl;
    } else {
        newEntitlementsFile = entitlementsXml;
        success = true;
    }
    return success;
}

#pragma mark - Provision functions

int copy_provision_file(const boost::filesystem::path & appPath, const boost::filesystem::path& argProvision)
{
    ORGLOG_V("Adding provision file to app");
    
    int err = 0;
    std::string pathToProvision = appPath.string() + "/" + "embedded.mobileprovision";
    std::string systemCmd = (std::string)"cp -f \"" + argProvision.string() + "\" \"" + (std::string)pathToProvision + "\"";
    err = system((const char*)systemCmd.c_str());
    if(err)std::cout << "Error: Failed copying provision file. Profile:" << argProvision << " Destination:" << pathToProvision << std::endl;
    
    return err;
}

#pragma mark - Resource rules functions

std::string resource_rules_file(bool forceResRules,
                                bool useOriginalResRules,
                                bool useGenericResRules,
                                const boost::filesystem::path & appPath,
                                const boost::filesystem::path & tempDirPath)
{
    std::string resRulesFile;
    bool resRulesSet = false;
    
    if (forceResRules || useOriginalResRules) {
        // apply original res rules
        // check if ResourceRules.plist is the App
        std::string pathToResourceRules = appPath.string() + "/" + "ResourceRules.plist";
        
        boost::filesystem::path boostPathToResourceRules = boost::filesystem::path(pathToResourceRules);
        if (boost::filesystem::exists(boostPathToResourceRules)) {
            ORGLOG_V("Codesign using original resource rules file :" << (std::string)pathToResourceRules);
            resRulesFile = pathToResourceRules;
            resRulesSet = true;
        }
    }
    if (resRulesSet == false) {
        if (forceResRules || useGenericResRules) {
            // Apply a generic res rules, create a local file with the template ResourceRulesTemplate
            boost::filesystem::path resourceRulesFile = tempDirPath;
            resourceRulesFile.append("ResourceRules.plist");
            
            if (create_file(resourceRulesFile, sResourceRulesTemplate, true)) {
                ORGLOG_V("Codesign using general resource rules file :" << resourceRulesFile);
                resRulesFile = resourceRulesFile.string();
            }
        }
    }
    return resRulesFile;
}

#pragma mark - Validation functions

bool verify_app_correctness(const boost::filesystem::path& appPath, const std::string& dllName)
{
    bool result = false;
    
    // Get the TeamIdentifier of the binaries
    std::string cmd = (std::string)"/usr/bin/codesign -dv \"" +  appPath.string() + "\" 2>&1 | grep TeamIdentifier=";
    std::string cmdResult1 = run_command(cmd);
    
    cmd = (std::string)"/usr/bin/codesign -dv \"" +  appPath.string() + "/" + dllName + "\" 2>&1 | grep TeamIdentifier=";
    std::string cmdResult2 = run_command(cmd);
    
    ORGLOG("Validating team identifiers");
    ORGLOG_V("App " << cmdResult1 << "DYLIB " <<  cmdResult2);
    
    result = (cmdResult2!="TeamIdentifier=not set" && cmdResult1.length() && cmdResult2.length() && cmdResult1==cmdResult2);
    
    return result;
}

