//
//  main.mm
//  bypass
//
//  Created by Jon Gabilondo on 9/29/14.
//

#include <stdio.h>
#include "codesign.hpp"
#include "injection.hpp"
#include "packaging.hpp"
#include "plist.hpp"


int main(int argc, const char * argv[])
{
    int err                     = 0;
    bool doInjectFramework      = false;
    bool doResign               = false;
    bool copyEntitlements       = false;
    bool useResourceRules       = false;
    bool removeEntitlements     = false;
    bool removeCodeSignature    = false;
    bool createNewUrlScheme     = true;
    bool useOriginalResRules    = false;
    bool useGenericResRules     = false;
    bool forceResRules          = false;
    bool input_file_is_ipa      = false;
    bool input_file_is_mac      = true;

    boost::filesystem::path argInputFilePath;
    boost::filesystem::path argFrameworkPath;
    boost::filesystem::path argCertificatePath;
    boost::filesystem::path argProvisionPath;
    boost::filesystem::path argEntitlementsPath;
    boost::filesystem::path argResourceRulesPath;
    boost::filesystem::path argDestinationPath;
    boost::filesystem::path argNewName;
    boost::filesystem::path repackedAppPath;
    
    boost::program_options::options_description desc("Allowed options", 200); // 200 cols
    boost::program_options::variables_map vm;
    add_options_description(desc);
    err = parse_options(argc, argv, desc, vm);
    if (err) {
        exit(err);
    }
    
    if (vm.count("help")) {
        std::cout << "Usage: bypass --inject_framework framework --codesign certificate -p provision [-v] [-h] [-V]  ipa_file \n";
        std::cout << desc << "\n";
    }
    if (vm.count("version")) {
        print_version();
        return 0;
    }
    if (vm.count("verbose")) {
        verbose = true;
        silenceCmdOutput = "";
    }
    if (vm.count("input-file")) {
        argInputFilePath = vm["input-file"].as<std::string>();
    }
    if (vm.count("inject-framework")) {
        argFrameworkPath = vm["inject-framework"].as<std::string>();
        doInjectFramework = true;
    }
    if (vm.count("certificate")) {
        argCertificatePath = vm["certificate"].as<std::string>();
        doResign = true;
    }
    if (vm.count("provision")) {
        argProvisionPath = vm["provision"].as<std::string>();
    }
    if (vm.count("entitlements")) {
        argEntitlementsPath = vm["entitlements"].as<std::string>();
        copyEntitlements = true;
    }
    if (vm.count("resource-rules")) {
        argResourceRulesPath = vm["resource-rules"].as<std::string>();
        useResourceRules = true;
    }
    if (vm.count("output-dir")) {
        argDestinationPath = vm["output-dir"].as<std::string>();
    }
    if (vm.count("output-name")) {
        argNewName = vm["output-name"].as<std::string>();
    }
    if (vm.count("no-url-scheme")) {
        createNewUrlScheme = false;
    }
    if (vm.count("original-res-rules")) {
        useOriginalResRules = true;
    }
    if (vm.count("generic-res-rules")) {
        useGenericResRules = true;
    }
    if (vm.count("force-res-rules")) {
        forceResRules = true;
    }
    if (vm.count("remove-entitlements")) {
        removeEntitlements = true;
    }
    if (vm.count("remove-code-signature")) {
        removeCodeSignature = true;
    }
    
    //------
    // Check arguments
    //------
    
    if (!argInputFilePath.empty()) {
        if (!boost::filesystem::exists(argInputFilePath)) {
            ORGLOG("Error: input file not found at: " << argInputFilePath);
            exit (ERR_Bad_Arguments);
        }
    } else {
        ORGLOG("Error: Missing input file.");
        exit (ERR_Bad_Arguments);
    }
    
    input_file_is_ipa = file_is_ipa(argInputFilePath);
    input_file_is_mac = file_is_mac_app(argInputFilePath);
    
    if (doInjectFramework) {
        if (!boost::filesystem::exists(argFrameworkPath)) {
            ORGLOG("Framework file not found at: " << argFrameworkPath);
            exit (ERR_Bad_Arguments);
        }
    }
    
    if (!argProvisionPath.empty()) {
        if (!boost::filesystem::exists(argProvisionPath)) {
            ORGLOG("Provision Profile not found at: " << argProvisionPath);
            exit (ERR_Bad_Arguments);
        }
    }
    
    //------
    // Preparations
    //------
    
    // Create temp folder path
    boost::filesystem::path tempDirectoryPath = boost::filesystem::temp_directory_path() / "bypassTempFolder";

    // Remove old temp if exist
    std::string systemCmd = (std::string)"rm -rf " + tempDirectoryPath.string();
    err = system(systemCmd.c_str());
    
    boost::filesystem::path appPath;

    if (input_file_is_ipa) {
        // Remove ipa
        boost::filesystem::path tempDirectoryIPA = tempDirectoryPath;
        tempDirectoryIPA.replace_extension("ipa");
        systemCmd = (std::string)"rm -rf " + tempDirectoryIPA.string();
        err = system(systemCmd.c_str());

        // Unzip ipa file into temp folder
        std::string tempDirectoryStd = tempDirectoryPath.string();
        ORGLOG_V("Unziping IPA into: " + tempDirectoryStd);
        if (unzip_ipa(argInputFilePath.c_str(), tempDirectoryStd)) {
            std::cout << "Error: Failed to unzip ipa.\n";
            exit (ERR_General_Error);
        }
    } else {
        // Copy app into temp folder
        if (!boost::filesystem::create_directory(tempDirectoryPath)) {
            ORGLOG("Error creating App temp folder: " << tempDirectoryPath);
            return ERR_General_Error;
        }
        systemCmd = (std::string)"cp -a " + argInputFilePath.string() + " " + tempDirectoryPath.string();
        err = system(systemCmd.c_str());
        if (err) {
            std::cout << "Error: Failed to copy input file to temp folder.\n";
            exit (ERR_General_Error);
        }
    }
    
    if (input_file_is_ipa) {
        systemCmd = (std::string)"ls -d " + tempDirectoryPath.string() + "/Payload/*.app";
        std::string cmdResult = run_command(systemCmd);
        boost::filesystem::path appName = boost::filesystem::path(cmdResult).filename();
        boost::filesystem::path appNameWithoutExtension = boost::filesystem::path(cmdResult).filename().replace_extension("");
        appPath = tempDirectoryPath;
        appPath.append("Payload").append(appName.string());
    } else {
        appPath = tempDirectoryPath / argInputFilePath.filename();
    }
    
    //------
    // Injection
    //------
    
    if (doInjectFramework) {
        
        ORGLOG("Injecting framework");
        
        std::string space = " ";
        std::string quote = "\"";
        boost::filesystem::path frameworksFolderPath = appPath;
        if (input_file_is_mac) {
            frameworksFolderPath.append("Contents"); // os x
        }
        frameworksFolderPath.append("Frameworks");
        
        // Create destination folder.
        if (boost::filesystem::exists(frameworksFolderPath) == false) {
            if (!boost::filesystem::create_directory(frameworksFolderPath)) {
                ORGLOG("Error creating JS Framework folder in the IPA: " << frameworksFolderPath);
                return ERR_General_Error;
            }
        }
            
        ORGLOG_V("Copying framework into app. " + systemCmd);
        systemCmd = "cp -a " + quote + (std::string)argFrameworkPath.c_str() + quote + space + quote + frameworksFolderPath.c_str() + quote;
        err = system((const char*)systemCmd.c_str());
        if (err) {
            ORGLOG("Failed copying the framework into the app.");
            err = ERR_General_Error;
            goto CLEAN_EXIT;
        }
        
        boost::filesystem::path pathToInfoplist = appPath;
        if (input_file_is_mac) {
            pathToInfoplist.append("Contents"); // os x
        }
        pathToInfoplist.append("Info.plist");

        std::string binaryFileName = get_app_binary_file_name(pathToInfoplist);
        
        if (!binaryFileName.empty())  {
            boost::filesystem::path pathToAppBinary = appPath;
            if (input_file_is_mac) {
                pathToAppBinary.append("Contents"); // os x
                pathToAppBinary.append("MacOS"); // os x
            }
            pathToAppBinary.append(binaryFileName);

            err = patch_binary(pathToAppBinary, argFrameworkPath, true, input_file_is_mac);
            if (err) {
                ORGLOG("Failed patching app binary");
                err = ERR_Injection_Failed;
                goto CLEAN_EXIT;
            }
        } else {
            ORGLOG("Failed retrieving app binary file name");
            err = ERR_Injection_Failed;
            goto CLEAN_EXIT;
        }
    }
    
    //------
    // Codesign
    //------
    
    if (doResign) {
        
        ORGLOG("Codesigning app ...");
        
        {
            if (!argProvisionPath.empty()) {
                err = copy_provision_file(appPath, argProvisionPath);
                if (err) {
                    err = ERR_General_Error;
                    goto CLEAN_EXIT;
                }
            }
            
            if (removeEntitlements) {
                err = remove_entitlements(appPath);
            }
            
            if (removeCodeSignature) {
                err = remove_codesign(appPath);
            }
            
            systemCmd = (std::string)"/usr/bin/codesign -f --deep -s '" + (std::string)argCertificatePath.c_str() + (std::string)"' " ;
            
            // Entitlements
            boost::filesystem::path entitlementsFilePath;
            if (input_file_is_ipa) {
                // Important entitlements info:
                // no entitlements provided in arguments, extract them from mobile provision
                // the provision file could be given in argument, if not extract it from the embeded.mobileprovision
                // if no embeded.mobileprovision then we can't do anything .. the codesign willprobably not build a good ipa, this will be more visible in 8.1.3
                
                if (copyEntitlements) {
                    entitlementsFilePath = argEntitlementsPath;
                } else {
                    std::string pathToProvision;
                    
                    if (!argProvisionPath.empty()) {
                        pathToProvision = argProvisionPath.c_str();
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
                resRulesFile = argResourceRulesPath.c_str();
            } else {
                resRulesFile = resource_rules_file(forceResRules, useOriginalResRules, useGenericResRules, appPath, tempDirectoryPath);
            }
            
            if (resRulesFile.length()) {
                ORGLOG_V("Codesign using resource rules file :" << (std::string)resRulesFile);
                systemCmd += (std::string)" --resource-rules='" + resRulesFile + "'";
            }
            
            systemCmd += " \"" + appPath.string() + "\"";
            
            // Codesign dylibs
            if (codesign_at_path(appPath, argCertificatePath, entitlementsFilePath)) {
                ORGLOG_V("Dylibs codesign sucessful!");
            } else {
                err = ERR_Dylib_Codesign_Failed;
                ORGLOG("Codesigning one or more dylibs failed");
            }
            
            // Codesigning extensions under plugins folder
            boost::filesystem::path extensionsFolderPath = appPath;
            extensionsFolderPath.append("PlugIns");
            if (codesign_at_path(extensionsFolderPath, argCertificatePath, entitlementsFilePath)) {
                ORGLOG_V("Extensions codesign sucessful!");
            }
            
            // Codesigning frameworks
            boost::filesystem::path frameworksFolderPath = appPath;
            if (input_file_is_mac) {
                frameworksFolderPath.append("Contents"); // os x
            }
            frameworksFolderPath.append("Frameworks");
            if (codesign_at_path(frameworksFolderPath, argCertificatePath, entitlementsFilePath)) {
                ORGLOG_V("Frameworks codesign sucessful!");
            }
            
            // silence output
            //systemCmd += " " + silenceCmdOutput;
            
            // execute codesign
            err = system((const char*)systemCmd.c_str());
            if (err) {
                err = ERR_App_Codesign_Failed;
                ORGLOG("Codesigning app failed.");
                goto CLEAN_EXIT;
            }
        }
    }
    
    if (input_file_is_ipa) {
        // Repack instrumented ipa
        err = repack(tempDirectoryPath, repackedAppPath);
        
        if (err) {
            ORGLOG("Error. Failed to zip ipa.");
        } else {
            
            boost::filesystem::path newAppPath;
            int err = deployToNewIPAtoDestination(repackedAppPath, argInputFilePath, argNewName.c_str(), argDestinationPath, doInjectFramework, newAppPath);
            if (err) {
                ORGLOG("Failed to move the new ipa to destination: " << newAppPath);
            } else {
                ORGLOG("\nSUCCESS!");
                ORGLOG("\nNew IPA path: " << newAppPath);
            }
        }
    } else {
        boost::filesystem::path newAppPath;
        int err = deployToNewAppToDestination(appPath, argInputFilePath, argNewName.c_str(), argDestinationPath, newAppPath);
        if (err) {
            ORGLOG("Failed to move the new ipa to destination: " << newAppPath);
        } else {
            ORGLOG("SUCCESS!");
            ORGLOG("New App path: " << newAppPath);
        }
    }
    
CLEAN_EXIT:
    
    if (!tempDirectoryPath.empty()) {
        systemCmd = "rm -rf " + tempDirectoryPath.string();
        if (system(systemCmd.c_str())) {
            ORGLOG("Failed to delete temp folder on clean exit");
        }
    }
    
    if (boost::filesystem::exists(repackedAppPath)) {
        boost::filesystem::remove(repackedAppPath);
    }
    
    return err;
}

