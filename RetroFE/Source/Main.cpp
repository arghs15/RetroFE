/* This file is part of RetroFE.
 *
 * RetroFE is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * RetroFE is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with RetroFE.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "Video/GStreamerVideo.h"
#include "Database/Configuration.h"
#include "Database/DB.h"
#include "Collection/CollectionInfoBuilder.h"
#include "Execute/Launcher.h"
#include "Utility/Log.h"
#include "Utility/Utils.h"
#include "RetroFE.h"
#include "Version.h"
#include "SDL.h"

#include "Database/GlobalOpts.h"

#include <cstdlib>
#include <fstream>
#include <time.h>
#include <locale>
#include <filesystem>
#include <string>
#include <fstream>
#include <vector>
#include <iostream>
#include <iomanip>
#ifdef WIN32
    #include <io.h>
#else
    #include <unistd.h>
#endif

namespace fs = std::filesystem;
static bool ImportConfiguration(Configuration* c);
std::vector<std::string> settingsFromCLI;

// Function to initialize the DB object
bool initializeDB(DB*& db, const std::string& dbPath) {
    db = new DB(dbPath);
    if (!db->initialize()) {
        LOG_ERROR("RetroFE", "Could not initialize database");
        return false;
    }
    return true;
}

// Function to initialize the MetadataDatabase object
bool initializeMetadataDatabase(MetadataDatabase*& metadb, DB* db, Configuration& config) {
    metadb = new MetadataDatabase(*db, config);
    if (!metadb->initialize()) {
        LOG_ERROR("RetroFE", "Could not initialize meta database");
        return false;
    }
    return true;
}

// Function to format and print contents of global_options::options_entry
void showUsage(const global_options::options_entry* options) {
    for (int i = 0; options[i].name || options[i].description; ++i) {
        if (options[i].name) {
            std::cout << "-" << std::setw(30) << std::left << options[i].name << options[i].description << std::endl;
        }
        else {
            // Category headers have nullptr names, so we print separate
            std::cout << "\n#\n# " << options[i].description << "\n#\n" << std::endl;
        }
    }
    std::cout << std::endl;
}

// Function to format and print contents of global_options::options_entry
void makeSettings(const global_options::options_entry* options) {
    std::string filename = Utils::combinePath(Configuration::absolutePath, "settings.conf");
    std::ofstream settingsFile;
    settingsFile.open(filename.c_str());
    for (int i = 0; options[i].name || options[i].description; ++i) {
        if (options[i].name) {
            settingsFile << options[i].name << "=" << options[i].defvalue << std::endl;
        }
        else {
            // Category headers have nullptr names, so we print separate
            settingsFile << "\n#\n# " << options[i].description << "\n#\n" << std::endl;
        }
    }
    settingsFile.close();
}

int main(int argc, char** argv)
{
    Configuration::initialize();
    Configuration config;
    std::string dbPath = Utils::combinePath(Configuration::absolutePath, "meta.db");
    
#ifdef WIN32
    std::string osType = "windows";
#elif __APPLE__
    std::string osType = "apple";
#else
    std::string osType = "linux";
#endif
    
    // Check to see if an argument was passed
    if (argc > 1)
    {
        std::string program = argv[0];
        std::string param = argv[1];
        
        if (param == "-createcollection" ||
            param == "--createcollection" ||
            param == "-cc")
        {
            std::string param = argv[1];
            std::string value = argv[2];
            if (argc == 3)
            {
                CollectionInfoBuilder::createCollectionDirectory(value, "", osType);
                return 0;
            }
            else if (argc == 4 and std::string(argv[3]) == "local")
            {
                CollectionInfoBuilder::createCollectionDirectory(value, argv[3], osType);
                return 0;
            }
            else
            {
                std::cout << "Expected 1 argument for -createcollection, got " << argc - 2 << std::endl;
                return 0;
            }
        }
        else if (param == "-version" ||
            param == "--version" ||
            param == "-v")
        {
            std::cout << "RetroFE version " << Version::getString() << std::endl << std::flush;
            return 0;
        }
        else if (param == "-showusage" ||
            param == "--showusage" ||
            param == "-su")
        {
            showUsage(global_options::s_option_entries);
            return 0;
        }
        else if (param == "-rebuilddatabase" ||
            param == "--rebuilddatabase" ||
            param == "-rebuilddb" ||
            param == "-rbdb" ||
            param == "-rdb")
        {
            DB* db = nullptr;
            MetadataDatabase* metadb = nullptr;
            if (!initializeDB(db, dbPath)) {
                // Handle initialization error, clean up if necessary
                return 0;
            }
            if (!initializeMetadataDatabase(metadb, db, config)) {
                // Handle initialization error, clean up if necessary
                delete db; // Clean up DB object
                return 0;
            }
            metadb->resetDatabase();
            return 0;
        }
        else if (param == "-showconfig" ||
            param == "--showconfig" ||
            param == "-sc")
        {
            ImportConfiguration(&config);
            config.printProperties();
            return 0;
        }
        else if (param == "-dumpproperties" ||
                 param == "--dumpproperties" ||
                 param == "-dump")
        {
            if (argc == 3)
            {
                ImportConfiguration(&config);
                // Need to get controls. prefix and launcher. prefix
                config.dumpPropertiesToFile(argv[2]);
                fprintf(stdout, "Dumping to: %s/%s\n", Configuration::absolutePath.c_str(), argv[2]);
                return 0;
            }
            else
            {
                std::cout << "Expected 1 argument for -dump, got " << argc - 2 << std::endl;
                return 0;
            }
        }
        else if (param == "-createconfig" ||
            param == "--createconfig" ||
            param == "-C")
        {
            // TODO; create default settings.conf
            makeSettings(global_options::s_option_entries);
            return 0;
        }
        else if ((argc % 2 != 0 or argc % 2 == 0) and param != "-help" and param != "-h")
        {
            // Pass global settings via CLI
            for (int i = 1; i <= argc - 1 ; i+=2) {
                // The odd argument should always be the key, and even will be value
                if (argv[i+1] == nullptr)
                {
                    // If you don't pass a value with a key we need catch that here
                    std::cout << "Expected 1 argument for " << argv[i] << " got " << 0 << std::endl;
                    return 0;
                }
                std::string CLIkey = argv[i];
                std::string CLIvalue = argv[i+1];
                if (Utils::startsWithAndStrip(CLIkey, "-") and ! Utils::startsWith(CLIvalue,"-"))
                {
                    if (CLIkey == OPTION_LOG)
                    {
                        config.setProperty(OPTION_LOG, CLIvalue);
                        config.StartLogging(&config);
                    }
                    settingsFromCLI.push_back(CLIkey + "=" + CLIvalue + "\n");
                }
                else if(Utils::startsWith(CLIvalue,"-"))
                {
                    std::cout << "Expected 1 argument for -" << CLIkey << " got " << 0 << std::endl;
                    return 0;
                }
                else
                {
                    std::cout << "To pass settings via CLI pairs use [-key] [value] format" << std::endl;
                    return 0;
                }
            }
        }
        else
        {
            // Display information about RetroFE
            std::cout << "Absolute Path: " << Configuration::absolutePath << std::endl;
            std::cout << "RetroFE Version: " << Version::getString() << std::endl;
            std::cout << std::endl;
            std::cout << "RetroFE is a cross-platform desktop frontend designed for MAME cabinets and game centers, with a focus on simplicity and customization." << std::endl;
            std::cout << "It's licensed under the terms of the GNU General Public License, version 3 or later (GPLv3)." << std::endl;
            std::cout << std::endl;

            // Display usage information
            std::cout << "Usage:" << std::endl;
            std::cout << "  -h   -help               Show this message" << std::endl;
            std::cout << "  -v   -version            Print the version of RetroFE" << std::endl;
            std::cout << std::endl;
            std::cout << "  -cc  -createcollection   Create a collection directory structure" << std::endl;
            std::cout << "  -rdb -rebuilddatabase    Rebuild the database from /meta subfolder" << std::endl;
            std::cout << "  -su  -showusage          Print a list of all global settings" << std::endl;
            std::cout << "  -sc  -showconfig         Print a list of current settings" << std::endl;
            std::cout << "  -C   -createconfig       Create a settings.conf with default values" << std::endl;
            std::cout << "       -dump               Dump current settings to a file" << std::endl;
            std::cout << std::endl;

            // Provide additional information and references
            std::cout << "For more information, visit" << std::endl;
            std::cout << "https://github.com/CoinOPS-Official/RetroFE/" << std::endl;
            std::cout << "http://retrofe.nl/" << std::endl;
            return 0;
        }
    }

    // Initialize locale language
    setlocale(LC_ALL, "");

    // Initialize random seed
    srand(static_cast<unsigned int>(time(nullptr)));

    gst_init(nullptr, nullptr);

    try {
        
        while (true)
        {
            if (!ImportConfiguration(&config))
            {
                // Exit with a heads up...
                std::string logFile = Utils::combinePath(Configuration::absolutePath, "log.txt");
                if(Utils::isOutputATerminal())
                {
                    fprintf(stderr, "RetroFE has failed to start due to a configuration error\nCheck the log for details: %s\n", logFile.c_str());
                }
                else
                {
                    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Configuration Error", ("RetroFE has failed to start due to a configuration error\nCheck the log for details: \n" + logFile).c_str(), NULL);
                }
                exit(EXIT_FAILURE);
            }
            RetroFE p(config);
            if (p.run()) // Check if we need to reboot after running
                config.clearProperties();
            else
                break;
        }
    }
    catch (std::exception& e)
    {
        LOG_ERROR("EXCEPTION", e.what());
    }

    Logger::deInitialize();

    return 0;
}

static bool ImportConfiguration(Configuration* c)
{
    std::string configPath = Configuration::absolutePath;

#ifdef WIN32
    std::string osType = "windows";
#elif __APPLE__
    std::string osType = "apple";
#else
    std::string osType = "linux";
#endif

    fs::path launchersPath = Utils::combinePath(Configuration::absolutePath, "launchers." + osType);

    fs::path collectionsPath = Utils::combinePath(Configuration::absolutePath, "collections");

    std::string settingsConfPath = Utils::combinePath(configPath, "settings");
    
    if(!fs::exists(Utils::combinePath(Configuration::absolutePath, "settings.conf")))
    {
        std::string logFile = "\nCheck the log for details: \n" + Utils::combinePath(Configuration::absolutePath, "log.txt");
        if(Utils::isOutputATerminal())
        {
            std::cout << "RetroFE failed to find a valid settings.conf in the current directory" + logFile << std::endl;
        }
        else
        {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Configuration", ("RetroFE failed to find a valid settings.conf in the current directory" + logFile).c_str(), NULL);
        }
        exit(EXIT_FAILURE);
    }
    
    if (!c->import("", settingsConfPath + ".conf"))
    {
        LOG_ERROR("RetroFE", "Could not import \"" + settingsConfPath + ".conf\"");
        return false;
    }
    for (int i = 1; i < 16; i++) {
        std::string settingsFile = settingsConfPath + std::to_string(i) + ".conf";
        if (fs::exists(settingsFile)) 
        {
            c->import("", "", settingsFile, false);
        }
    }
    std::string savedSettingsFile = settingsConfPath + "_saved.conf";
    if (fs::exists(savedSettingsFile)) 
    {
        c->import("", "", savedSettingsFile, false);
    }
    if (!settingsFromCLI.empty()) 
    {
        // If settingsFromCLI isn't empty let's do something with it
        std::string result;
        for (const auto& str : settingsFromCLI) {
            result += str;
        }
        c->import("", "CLI", result, false);
    }

    // log version
    LOG_INFO("RetroFE", "Version " + Version::getString() + " starting");

#ifdef WIN32
    LOG_INFO("RetroFE", "OS: Windows");
#elif __APPLE__
    LOG_INFO("RetroFE", "OS: Mac");
#else
    LOG_INFO("RetroFE", "OS: Linux");
#endif
    
    // Check if GStreamer initialization was successful
    if (gst_is_initialized())
    {
#ifdef WIN32
        std::string path = Utils::combinePath(Configuration::absolutePath, "retrofe");
        GstRegistry* registry = gst_registry_get();
        gst_registry_scan_path(registry, path.c_str());
#endif
        LOG_INFO("RetroFE", "GStreamer successfully initialized");
    }
    else
    {
        LOG_ERROR("RetroFE", "Failed to initialize GStreamer");
        return false;
    }

    LOG_INFO("RetroFE", "Absolute path: " + Configuration::absolutePath);

    // Process launchers
    if (fs::is_directory(launchersPath))
    {
        for (const auto& entry : fs::directory_iterator(launchersPath))
        {
            if (entry.is_regular_file())
            {
                fs::path filePath = entry.path();
                if (filePath.extension() == ".conf")
                {
                    std::string basename = filePath.stem().string();
                    std::string prefix = "launchers." + basename;
                    std::string importFile = filePath.string();

                    if (!c->import(prefix, importFile))
                    {
                        LOG_ERROR("RetroFE", "Could not import \"" + importFile + "\"");
                    }
                }
            }
        }
    }
    else
    {
        LOG_NOTICE("RetroFE", "Launchers directory does not exist or is not a directory: " + launchersPath.string());
    }


    // Process collections
    if (!fs::exists(collectionsPath) || !fs::is_directory(collectionsPath)) {
        LOG_ERROR("RetroFE", "Could not read directory \"" + collectionsPath.string() + "\"");
        return false;
    }

    for (const auto& entry : fs::directory_iterator(collectionsPath)) {
        std::string collection = entry.path().filename().string();
        if (fs::is_directory(entry) && !collection.empty() && collection[0] != '_' && collection != "." && collection != "..") {
            std::string prefix = "collections." + collection;
            bool settingsImported = false;
            std::string settingsFile = Utils::combinePath(collectionsPath, collection, "settings.conf");
            if (fs::exists(settingsFile)) {
                settingsImported |= c->import(collection, prefix, settingsFile, false);
            }

            for (int i = 1; i < 16; i++) {
                std::string numberedSettingsFile = Utils::combinePath(collectionsPath, collection, "settings" + std::to_string(i) + ".conf");
                if (fs::exists(numberedSettingsFile)) {
                    settingsImported |= c->import(collection, prefix, numberedSettingsFile, false);
                }
            }

            std::string infoFile = Utils::combinePath(collectionsPath, collection, "info.conf");
            if (fs::exists(infoFile)) {
                c->import(collection, prefix, infoFile, false);
            }

            // Process collection-specific launcher overrides
            std::string osSpecificLauncherFile = Utils::combinePath(collectionsPath, collection, "launcher." + osType + ".conf");
            std::string defaultLauncherFile = Utils::combinePath(collectionsPath, collection, "launcher.conf");
            std::string launcherKey = "collectionLaunchers." + collection; // Unique key for collection-specific launchers

            std::string importFile = fs::exists(osSpecificLauncherFile) ? osSpecificLauncherFile
                : fs::exists(defaultLauncherFile) ? defaultLauncherFile
                : "";

            // Import the launcher file if it exists under the unique key
            if (!importFile.empty()) {
                c->import(collection, launcherKey, importFile, false);
                LOG_INFO("RetroFE", "Imported collection-specific launcher for: " + collection);
                if (!c->propertyExists("collections." + collection + ".launcher"))
                {
                    c->setProperty("collections." + collection + ".launcher", collection);
                }
            }

            fs::path localLaunchersPath = Utils::combinePath(collectionsPath, collection, "launchers." + osType + ".local");
            fs::path defaultLocalLaunchersPath = Utils::combinePath(collectionsPath, collection, "launchers.local");

            // Check if OS-specific launchers directory exists, otherwise use the default launchers directory
            if (!fs::is_directory(localLaunchersPath) && fs::is_directory(defaultLocalLaunchersPath)) {
                localLaunchersPath = defaultLocalLaunchersPath;
            }

            if (fs::is_directory(localLaunchersPath))
            {
                for (const auto& launcherEntry : fs::directory_iterator(localLaunchersPath)) {
                    if (launcherEntry.is_regular_file()) {
                        fs::path filePath = launcherEntry.path();
                        if (filePath.extension() == ".conf") {
                            std::string basename = filePath.stem().string();
                            std::string prefix = "localLaunchers." + collection + "." + basename;
                            std::string importFile = filePath.string();

                            if (!c->import(collection, prefix, importFile)) {
                                LOG_ERROR("RetroFE", "Could not import local launcher \"" + importFile + "\" for collection \"" + collection + "\"");
                            }
                            else {
                                LOG_INFO("RetroFE", "Imported local launcher \"" + basename + "\" for collection \"" + collection + "\"");
                            }
                        }
                    }
                }
            }
            // Set the launcher property if it's not already set in settings.conf
            std::string launcherPropertyKey = "collections." + collection + ".launcher";
            if (!c->propertyExists(launcherPropertyKey) && !importFile.empty()) {
                c->setProperty(launcherPropertyKey, collection);
            }

            // Update collectionLaunchers property
            std::string collectionLaunchers = "collectionLaunchers";
            std::string launchers = "";
            c->getProperty(collectionLaunchers, launchers);
            if (!importFile.empty() && (collection.size() < 3 || collection.substr(collection.size() - 3) != "SUB")) {
                c->setProperty(collectionLaunchers, launchers + collection + ",");
            }



            if (!settingsImported) {
                LOG_ERROR("RetroFE", "Could not import any collection settings for " + collection);
            }
            else {
                LOG_INFO("RetroFE", "Imported settings for collection: " + collection);
            }

            if (!importFile.empty()) {
                LOG_INFO("RetroFE", "Imported launcher configuration for collection: " + collection);
            }
        }
    }

    LOG_INFO("RetroFE", "Imported configuration");
    bool dumpProperties = false;
    c->getProperty(OPTION_DUMPPROPERTIES, dumpProperties);
    if (dumpProperties) {
            c->dumpPropertiesToFile(Utils::combinePath(Configuration::absolutePath, "properties.txt"));
    }
    return true;
}
