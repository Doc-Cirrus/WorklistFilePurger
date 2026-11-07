/**
 * Worklist Purger Plugin - A plugin for Orthanc DICOM Server for removing worklist files for stable studies
 * Copyright (C) 2017 - 2023  (Doc Cirrus GmbH)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 **/

#include "Purger.h"
#include <orthanc/OrthancCPlugin.h>
#include "../Resources/Orthanc/Plugins/OrthancPluginCppWrapper.h"
#include <Logging.h>
#include <OrthancException.h>

#include <iostream>
#include <boost/filesystem.hpp>
using namespace boost::filesystem;

#define ORTHANC_PLUGIN_NAME "worklist-file-purger"

static OrthancPlugins::WorklistPurger* purger_;

OrthancPluginErrorCode OnChange(OrthancPluginChangeType changeType,OrthancPluginResourceType resourceType, const char*
                                                                                                           resourceId) {
    // No exception may leave this C callback
    try {
        purger_->OnChangeCallback(changeType, resourceType, resourceId);
    } catch (Orthanc::OrthancException& e) {
        LOG(ERROR) << "WorklistPurger: " << e.What();
    } catch (std::exception& e) {
        LOG(ERROR) << "WorklistPurger: " << e.what();
    } catch (...) {
        LOG(ERROR) << "WorklistPurger: unknown error";
    }
    return OrthancPluginErrorCode_Success;
};

extern "C" {
    ORTHANC_PLUGINS_API int32_t OrthancPluginInitialize(OrthancPluginContext* c) {
        OrthancPluginContext* context_ = c;
        OrthancPlugins::SetGlobalContext(context_, ORTHANC_PLUGIN_NAME);
        Orthanc::Logging::InitializePluginContext(context_, ORTHANC_PLUGIN_NAME);

        /* Check the version of the Orthanc core */
        if (OrthancPluginCheckVersion(context_) == 0) {
            LOG(ERROR) << "Your version of Orthanc " <<( OrthancPlugins::GetGlobalContext()->orthancVersion)
                       <<  " must be above "
                       <<  ORTHANC_PLUGINS_MINIMAL_MAJOR_NUMBER <<"." <<ORTHANC_PLUGINS_MINIMAL_MINOR_NUMBER <<"."
                       << ORTHANC_PLUGINS_MINIMAL_REVISION_NUMBER << " to run this plugin";
        return -1;
        }

        LOG(WARNING) << "WorklistFilePurger :  plugin is initializing";
        OrthancPluginSetDescription2(c, ORTHANC_PLUGIN_NAME, "Delete worklist file after receiving a matching instance in Orthanc PACS");


        // No exception may leave this C function: a configuration with a wrong type throws
        try {
            OrthancPlugins::OrthancConfiguration configuration;

            OrthancPlugins::OrthancConfiguration worklistsConf;
            OrthancPlugins::OrthancConfiguration worklistPurger;

            configuration.GetSection(worklistsConf, "Worklists");
            configuration.GetSection(worklistPurger, "WorklistPurger");

            bool serverEnabled = worklistsConf.GetBooleanValue("Enable", false);
            bool pluginEnabled = worklistPurger.GetBooleanValue("Enable", false);

            std::string folder_;
            worklistsConf.LookupStringValue(folder_, "Database");

            if (serverEnabled && pluginEnabled) {
                if (is_directory(path(folder_))) {
                    purger_ = new OrthancPlugins::WorklistPurger(context_, folder_);
                    OrthancPluginRegisterOnChangeCallback(context_, OnChange);
                } else {
                    LOG(WARNING) << "The configuration option \"Worklists.Database\" must contain a path to an existing folder in the Orthanc configuration file";
                }
            } else {
              LOG(WARNING) << "Worklist server or WorklistPurger plugin is disabled in the Orthanc configuration file";
            }
        } catch (Orthanc::OrthancException& e) {
            LOG(ERROR) << "WorklistPurger: " << e.What();
            return -1;
        } catch (std::exception& e) {
            LOG(ERROR) << "WorklistPurger: " << e.what();
            return -1;
        }
        return 0;
    }


  ORTHANC_PLUGINS_API void OrthancPluginFinalize()
  {
    LOG(WARNING) << "WorklistPurger plugin is finalizing";
    delete purger_;
    purger_ = NULL;
  }


  ORTHANC_PLUGINS_API const char* OrthancPluginGetName()
  {
    return ORTHANC_PLUGIN_NAME;
  }
	


  ORTHANC_PLUGINS_API const char* OrthancPluginGetVersion()
  {
    return ORTHANC_PLUGIN_VERSION;
  }
}

  


