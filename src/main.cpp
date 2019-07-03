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
        err = inject_framework(appPath, argFrameworkPath, input_file_is_mac);
        if (err) {
            goto CLEAN_EXIT;
        }
    }
    
    //------
    // Codesign
    //------
    
    if (doResign) {
        ORGLOG("Codesigning app ...");
        err = codesign_app(appPath, argProvisionPath,  argResourceRulesPath, argCertificatePath, argEntitlementsPath, tempDirectoryPath, removeEntitlements, removeCodeSignature, input_file_is_ipa, input_file_is_mac, copyEntitlements, useResourceRules, forceResRules, useOriginalResRules, useGenericResRules);
        
        if (err) {
            goto CLEAN_EXIT;
        }
    }
    
    //------
    // Deploy to destination
    //------
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
            ORGLOG("Failed to move the new app to destination: " << newAppPath);
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

