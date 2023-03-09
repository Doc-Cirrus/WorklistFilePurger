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
#include "Purger.h"
#include <Logging.h>
#include <boost/filesystem.hpp>
#include <json/value.h>
#include <json/reader.h>
#include <iostream>
#include <regex>
using namespace boost::filesystem;

namespace OrthancPlugins {
    WorklistPurger::WorklistPurger(OrthancPluginContext*  context__, std::string folder__) {
        folder_ = folder__;
        context_ = context__;
    };
    WorklistPurger::~WorklistPurger() {};

    OrthancPluginErrorCode WorklistPurger::getStudyUID(const char* resourceId, std::string *result) {
        OrthancPluginMemoryBuffer answerBody;
        std::string uri = std::string ("/studies/") + std::string (resourceId) ;

        OrthancPluginRestApiGet(context_, &answerBody, uri.c_str());

        // Parse JSON
        Json::Value studyJSON;
        char *begin, *end;
        begin = (char*)answerBody.data;
        std::string s((const char*)answerBody.data);
        end = begin + (s.length() - 1);

        Json::CharReaderBuilder builder {};

        auto reader = std::unique_ptr<Json::CharReader>( builder.newCharReader() );

        Json::Value parsed_study {};
        std::string errors {};
        const auto is_parsed = reader->parse(begin,end,&parsed_study,&errors );
        OrthancPluginFreeMemoryBuffer(context_, &answerBody);

        if ( is_parsed != 0 )
        {
            LOG(ERROR) << "Parsing API response was not successful";
            return OrthancPluginErrorCode_BadJson;
        }

        try {
            std::string studyUID = parsed_study["MainDicomTags"]["StudyInstanceUID"].asString();
            LOG(INFO) << "Study Instance UID of the stable study: " << studyUID;

            escapeDotsInFilename(&studyUID, result);
            return OrthancPluginErrorCode_Success;
        }
        catch ( const Json::Exception& error )
        {
            LOG(ERROR) << "Accessing Study Instance UID failed: " << error.what();
            return OrthancPluginErrorCode_InexistentTag;
        }
    };

    void WorklistPurger::escapeDotsInFilename(std::string* studyUID, std::string* escaped_studyUID) {
        for (auto it = (*studyUID).cbegin() ; it != (*studyUID).cend(); ++it) {
            if (*it == '.') {
                *escaped_studyUID += "\\";
            }
            *escaped_studyUID += *it;
        }
    }

    void WorklistPurger::verifyAndRemoveWorklistFile(std::string* escaped_studyUID) {
        path source(folder_);
        directory_iterator end;

        for (directory_iterator it(source); it != end; ++it) {
            std::string filename = it->path().filename().string();
            std::string match_string = "(worklist_" + (*escaped_studyUID) + "_)(.*)";

            if (std::regex_match(filename, std::regex(match_string))) {
                LOG(WARNING) << "Found a matching worklist file file for Study Instance UID " << *escaped_studyUID;
                if (remove(it->path())) {
                    LOG(WARNING) << "Worklist file was successfully deleted";
                } else {
                    LOG(ERROR) << "Unable to delete worklist file";
                };
                return;
            }
        }
        LOG(WARNING) << "No worklist file for Study Instance UID " << *escaped_studyUID << " found";
        return;
    };

    OrthancPluginErrorCode WorklistPurger:: OnChangeCallback(OrthancPluginChangeType changeType,OrthancPluginResourceType
                                                                           resourceType, const char* resourceId) {
        if (changeType == OrthancPluginChangeType_StableStudy ) {
            std::string studyUID;
            getStudyUID(resourceId, &studyUID);

            verifyAndRemoveWorklistFile(&studyUID);
        }

        return OrthancPluginErrorCode_Success;
    }
}
