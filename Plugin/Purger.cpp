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
#include <vector>
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

        OrthancPluginErrorCode code = OrthancPluginRestApiGet(context_, &answerBody, uri.c_str());
        if (code != OrthancPluginErrorCode_Success) {
            LOG(ERROR) << "Cannot read " << uri << " from the REST API (error " << code << ")";
            return code;
        }

        // The answer is not NUL-terminated: parse exactly "size" bytes
        Json::Value study;
        bool isParsed = OrthancPlugins::ReadJson(study, answerBody.data, answerBody.size);
        OrthancPluginFreeMemoryBuffer(context_, &answerBody);

        if (!isParsed || !study.isObject()) {
            LOG(ERROR) << "The answer of " << uri << " is not a JSON object";
            return OrthancPluginErrorCode_BadJson;
        }

        const Json::Value& tags = study["MainDicomTags"];
        if (!tags.isObject() || !tags["StudyInstanceUID"].isString() || tags["StudyInstanceUID"].asString().empty()) {
            LOG(ERROR) << "The study " << resourceId << " has no StudyInstanceUID";
            return OrthancPluginErrorCode_InexistentTag;
        }

        *result = tags["StudyInstanceUID"].asString();
        LOG(INFO) << "Study Instance UID of the stable study: " << *result;
        return OrthancPluginErrorCode_Success;
    };

    void WorklistPurger::verifyAndRemoveWorklistFile(std::string* studyUID) {
        // A plain prefix, not a regular expression: a UID with special characters can neither
        // match other files nor throw. The error_code overloads of Boost do not throw either,
        // as this runs inside a callback of Orthanc.
        const std::string prefix = "worklist_" + *studyUID + "_";
        boost::system::error_code ec;

        // Collected first, then removed: removing entries while iterating a folder is unspecified
        std::vector<path> matches;
        for (directory_iterator it(path(folder_), ec), end; !ec && it != end; it.increment(ec)) {
            if (it->path().filename().string().compare(0, prefix.size(), prefix) == 0) {
                matches.push_back(it->path());
            }
        }

        if (ec) {
            LOG(ERROR) << "Cannot list the worklist folder " << folder_ << ": " << ec.message();
        }

        if (matches.empty()) {
            LOG(WARNING) << "No worklist file for Study Instance UID " << *studyUID << " found";
            return;
        }

        for (const path& file : matches) {
            if (remove(file, ec)) {
                LOG(WARNING) << "Deleted the worklist file " << file.filename().string();
            } else {
                LOG(ERROR) << "Unable to delete the worklist file " << file.string() << ": " << ec.message();
            }
        }
    };

    OrthancPluginErrorCode WorklistPurger:: OnChangeCallback(OrthancPluginChangeType changeType,OrthancPluginResourceType
                                                                           resourceType, const char* resourceId) {
        if (changeType == OrthancPluginChangeType_StableStudy ) {
            std::string studyUID;
            if (getStudyUID(resourceId, &studyUID) == OrthancPluginErrorCode_Success) {
                verifyAndRemoveWorklistFile(&studyUID);
            }
        }

        return OrthancPluginErrorCode_Success;
    }
}
