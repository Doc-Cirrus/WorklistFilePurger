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

#include <orthanc/OrthancCPlugin.h>
#include "../Resources/Orthanc/Plugins/OrthancPluginCppWrapper.h"

namespace OrthancPlugins {
    class WorklistPurger {
        private:
            OrthancPluginContext*  context_;
            std::string folder_;
        public:
            WorklistPurger(OrthancPluginContext*  context_, std::string folder_);
            virtual ~WorklistPurger();
            virtual OrthancPluginErrorCode getStudyUID(const char* resourceId, std::string *result);
            virtual void verifyAndRemoveWorklistFile(std::string* studyUID);
            virtual void escapeDotsInFilename(std::string* studyUID, std::string* escaped_studyUID);
            virtual OrthancPluginErrorCode OnChangeCallback(OrthancPluginChangeType changeType,OrthancPluginResourceType
                                                                                  resourceType, const char* resourceId);
    };
};