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

#include <gtest/gtest.h>

#include <Logging.h>

#include <boost/filesystem.hpp>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include "../Plugin/Purger.h"
#include "../Resources/Orthanc/Plugins/OrthancPluginCppWrapper.h"

namespace fs = boost::filesystem;

// The entry points of the plugin (Plugin.cpp)
extern "C" {
    int32_t OrthancPluginInitialize(OrthancPluginContext* context);
    void OrthancPluginFinalize();
}

static const char* const STUDY_ID = "27f7126f-4f66fb14-03f4081b-f9341db2-53925988";

// Answer of GET /studies/{id}, as Orthanc returns it: styled JSON, ending with a newline
static const char* const STUDY_JSON = R"({
   "ID" : "27f7126f-4f66fb14-03f4081b-f9341db2-53925988",
   "IsStable" : true,
   "MainDicomTags" : {
      "AccessionNumber" : "A1234",
      "StudyDate" : "20260101",
      "StudyInstanceUID" : "12.234.1234.234561"
   },
   "ParentPatient" : "6816cb19-844d5aee-85245eba-8a5e7f5d-6c8a2d31",
   "Series" : [ "3fd7ae4a-29b8d4c4-6e5c5d5f-5c2e4cdd-8d4b8e6f" ],
   "Type" : "Study"
}
)";

static std::string restAnswer_;
static OrthancPluginErrorCode restError_ = OrthancPluginErrorCode_Success;
static std::string configuration_;
static OrthancPluginOnChangeCallback onChange_ = NULL;

// The services of the Orthanc core that the plugin calls: "RestApiGet" answers restAnswer_,
// "GetConfiguration" answers configuration_, "RegisterOnChangeCallback" records the callback,
// and the other ones (logging, description, ...) do nothing
static OrthancPluginErrorCode ServiceMock(struct _OrthancPluginContext_t* context,
                                          _OrthancPluginService service,
                                          const void* params)
{
    if (service == _OrthancPluginService_RestApiGet)
    {
        if (restError_ != OrthancPluginErrorCode_Success)
        {
            return restError_;
        }

        const _OrthancPluginRestApiGet& p = *reinterpret_cast<const _OrthancPluginRestApiGet*>(params);
        p.target->size = restAnswer_.size();
        p.target->data = restAnswer_.empty() ? NULL : malloc(restAnswer_.size());
        if (!restAnswer_.empty())
        {
            memcpy(p.target->data, restAnswer_.data(), restAnswer_.size());
        }
    }
    else if (service == _OrthancPluginService_GetConfiguration)
    {
        *reinterpret_cast<const _OrthancPluginRetrieveDynamicString*>(params)->result = strdup(configuration_.c_str());
    }
    else if (service == _OrthancPluginService_RegisterOnChangeCallback)
    {
        onChange_ = reinterpret_cast<const _OrthancPluginOnChangeCallback*>(params)->callback;
    }

    return OrthancPluginErrorCode_Success;
}

class WorklistRemoverTest : public ::testing::Test {
    protected:
        OrthancPluginContext context_;
        std::unique_ptr<OrthancPlugins::WorklistPurger> purger_;
        std::string folder_;

    virtual void SetUp() {
        memset(&context_, 0, sizeof(context_));
        context_.orthancVersion = "1.13.0";
        context_.Free = &free;
        context_.InvokeService = &ServiceMock;
        Orthanc::Logging::InitializePluginContext(&context_, "worklist-file-purger");

        restAnswer_ = STUDY_JSON;
        restError_ = OrthancPluginErrorCode_Success;
        onChange_ = NULL;

        // A fresh folder per test, so that a crashed run leaves nothing behind for the next one
        fs::path p = fs::temp_directory_path() / fs::unique_path("worklist-purger-%%%%-%%%%-%%%%");
        fs::create_directories(p);
        folder_ = p.string();
        purger_.reset(new OrthancPlugins::WorklistPurger(&context_, folder_));

        //create worklist files
        for (int i = 0; i < 10; i++) {
            createFile("worklist_12.234.1234.23456" + std::to_string(i) + "_unixtime.wl");
        }
    }

    virtual void TearDown() {
        fs::remove_all(folder_);
    }

    void createFile(const std::string& name) {
        std::ofstream file((fs::path(folder_) / name).string());
        ASSERT_TRUE(file.good());
    }

    bool exists(const std::string& name) {
        return fs::exists(fs::path(folder_) / name);
    }

    size_t countFiles() {
        return std::distance(fs::directory_iterator(folder_), fs::directory_iterator());
    }

    // Loads the plugin as Orthanc does, with this configuration
    int32_t initializePlugin(const std::string& configuration) {
        configuration_ = configuration;
        OrthancPlugins::ResetGlobalContext();
        return OrthancPluginInitialize(&context_);
    }

    void finalizePlugin() {
        OrthancPluginFinalize();
        OrthancPlugins::ResetGlobalContext();
    }

    std::string enabledConfiguration() {
        return R"({ "Worklists" : { "Enable" : true, "Database" : ")" + folder_ +
            R"(" }, "WorklistPurger" : { "Enable" : true } })";
    }
};

TEST_F(WorklistRemoverTest, getStudyUID) {
    std::string result;
    ASSERT_EQ(purger_->getStudyUID(STUDY_ID, &result), OrthancPluginErrorCode_Success);
    ASSERT_EQ("12.234.1234.234561", result);
}

TEST_F(WorklistRemoverTest, getStudyUIDCompactAnswer) {
    restAnswer_ = R"({"MainDicomTags":{"StudyInstanceUID":"1.2.3"}})";
    std::string result;
    ASSERT_EQ(purger_->getStudyUID(STUDY_ID, &result), OrthancPluginErrorCode_Success);
    ASSERT_EQ("1.2.3", result);
}

TEST_F(WorklistRemoverTest, getStudyUIDRestApiFailure) {
    restError_ = OrthancPluginErrorCode_UnknownResource;
    std::string result;
    ASSERT_EQ(purger_->getStudyUID(STUDY_ID, &result), OrthancPluginErrorCode_UnknownResource);
    ASSERT_TRUE(result.empty());
}

TEST_F(WorklistRemoverTest, getStudyUIDEmptyAnswer) {
    restAnswer_ = "";
    std::string result;
    ASSERT_EQ(purger_->getStudyUID(STUDY_ID, &result), OrthancPluginErrorCode_BadJson);
    ASSERT_TRUE(result.empty());
}

TEST_F(WorklistRemoverTest, getStudyUIDNotAnObject) {
    std::string result;
    for (const char* answer : { "not json", "[ 1, 2 ]\n", "{ \"MainDicomTags\" : " }) {
        restAnswer_ = answer;
        ASSERT_EQ(purger_->getStudyUID(STUDY_ID, &result), OrthancPluginErrorCode_BadJson) << answer;
    }
    ASSERT_TRUE(result.empty());
}

TEST_F(WorklistRemoverTest, getStudyUIDWithoutStudyInstanceUID) {
    std::string result;
    for (const char* answer : { "{}", R"({"MainDicomTags":[]})", R"({"MainDicomTags":{}})",
                                R"({"MainDicomTags":{"StudyInstanceUID":12}})",
                                R"({"MainDicomTags":{"StudyInstanceUID":""}})" }) {
        restAnswer_ = answer;
        ASSERT_EQ(purger_->getStudyUID(STUDY_ID, &result), OrthancPluginErrorCode_InexistentTag) << answer;
    }
    ASSERT_TRUE(result.empty());
}

TEST_F(WorklistRemoverTest, verifyAndRemoveWorklist) {
    for (int i = 1; i < 10; i+=2) {
        std::string studyUID = "12.234.1234.23456" + std::to_string(i);
        std::string filename = "worklist_" + studyUID + "_unixtime.wl";

        ASSERT_TRUE(exists(filename));
        purger_->verifyAndRemoveWorklistFile(&studyUID);
        ASSERT_FALSE(exists(filename));
    }
    ASSERT_EQ(5u, countFiles());
}

TEST_F(WorklistRemoverTest, removesEveryMatchingFile) {
    createFile("worklist_12.234.1234.234561_second.wl");
    std::string studyUID = "12.234.1234.234561";
    purger_->verifyAndRemoveWorklistFile(&studyUID);
    ASSERT_FALSE(exists("worklist_12.234.1234.234561_unixtime.wl"));
    ASSERT_FALSE(exists("worklist_12.234.1234.234561_second.wl"));
    ASSERT_EQ(9u, countFiles());
}

TEST_F(WorklistRemoverTest, matchesTheWholeUID) {
    // A dot is not a wildcard, and a UID that is a prefix of another one matches nothing
    createFile("worklist_12x234x1234x234561_unixtime.wl");
    std::string studyUID = "12.234.1234.234561";
    purger_->verifyAndRemoveWorklistFile(&studyUID);
    ASSERT_TRUE(exists("worklist_12x234x1234x234561_unixtime.wl"));

    std::string prefixUID = "12.234.1234.23456";
    purger_->verifyAndRemoveWorklistFile(&prefixUID);
    ASSERT_EQ(10u, countFiles());

    // Special characters of regular expressions are plain characters
    createFile("worklist_1.2(3+_a.wl");
    std::string specialUID = "1.2(3+";
    purger_->verifyAndRemoveWorklistFile(&specialUID);
    ASSERT_FALSE(exists("worklist_1.2(3+_a.wl"));
}

TEST_F(WorklistRemoverTest, noMatchingFile) {
    std::string studyUID = "1.2.3";
    purger_->verifyAndRemoveWorklistFile(&studyUID);
    ASSERT_EQ(10u, countFiles());
}

TEST_F(WorklistRemoverTest, missingFolder) {
    fs::remove_all(folder_);
    std::string studyUID = "12.234.1234.234561";
    ASSERT_NO_THROW(purger_->verifyAndRemoveWorklistFile(&studyUID));
}

TEST_F(WorklistRemoverTest, onlyStableStudies) {
    for (OrthancPluginChangeType type : { OrthancPluginChangeType_NewStudy, OrthancPluginChangeType_StableSeries,
                                          OrthancPluginChangeType_StablePatient, OrthancPluginChangeType_UpdatedAttachment }) {
        ASSERT_EQ(OrthancPluginErrorCode_Success, purger_->OnChangeCallback(type, OrthancPluginResourceType_Study, STUDY_ID));
        ASSERT_EQ(10u, countFiles());
    }

    ASSERT_EQ(OrthancPluginErrorCode_Success,
              purger_->OnChangeCallback(OrthancPluginChangeType_StableStudy, OrthancPluginResourceType_Study, STUDY_ID));
    ASSERT_FALSE(exists("worklist_12.234.1234.234561_unixtime.wl"));
    ASSERT_EQ(9u, countFiles());
}

TEST_F(WorklistRemoverTest, nothingRemovedWithoutStudyInstanceUID) {
    restError_ = OrthancPluginErrorCode_UnknownResource;
    ASSERT_EQ(OrthancPluginErrorCode_Success,
              purger_->OnChangeCallback(OrthancPluginChangeType_StableStudy, OrthancPluginResourceType_Study, STUDY_ID));

    // An empty UID must not select the files named "worklist__*"
    createFile("worklist__empty.wl");
    restError_ = OrthancPluginErrorCode_Success;
    restAnswer_ = R"({"MainDicomTags":{"StudyInstanceUID":""}})";
    ASSERT_EQ(OrthancPluginErrorCode_Success,
              purger_->OnChangeCallback(OrthancPluginChangeType_StableStudy, OrthancPluginResourceType_Study, STUDY_ID));
    ASSERT_EQ(11u, countFiles());
}

TEST_F(WorklistRemoverTest, pluginEnabled) {
    ASSERT_EQ(0, initializePlugin(enabledConfiguration()));
    ASSERT_TRUE(onChange_ != NULL);

    ASSERT_EQ(OrthancPluginErrorCode_Success,
              onChange_(OrthancPluginChangeType_StableStudy, OrthancPluginResourceType_Study, STUDY_ID));
    ASSERT_FALSE(exists("worklist_12.234.1234.234561_unixtime.wl"));
    ASSERT_EQ(9u, countFiles());
    finalizePlugin();
}

TEST_F(WorklistRemoverTest, pluginDisabled) {
    for (const std::string& configuration : {
            std::string("{}"),
            R"({ "Worklists" : { "Enable" : true, "Database" : ")" + folder_ + R"(" } })",
            R"({ "Worklists" : { "Enable" : true, "Database" : ")" + folder_ + R"(" }, "WorklistPurger" : { "Enable" : false } })",
            R"({ "Worklists" : { "Enable" : false, "Database" : ")" + folder_ + R"(" }, "WorklistPurger" : { "Enable" : true } })",
            std::string(R"({ "Worklists" : { "Enable" : true }, "WorklistPurger" : { "Enable" : true } })"),
            std::string(R"({ "Worklists" : { "Enable" : true, "Database" : "/nonexistent" }, "WorklistPurger" : { "Enable" : true } })") }) {
        ASSERT_EQ(0, initializePlugin(configuration)) << configuration;
        ASSERT_TRUE(onChange_ == NULL) << configuration;
        finalizePlugin();
    }
}

TEST_F(WorklistRemoverTest, pluginWrongConfiguration) {
    // Orthanc must get an error code, not an exception
    ASSERT_EQ(-1, initializePlugin(R"({ "Worklists" : { "Enable" : true }, "WorklistPurger" : { "Enable" : "yes" } })"));
    ASSERT_TRUE(onChange_ == NULL);
    finalizePlugin();
}

int main(int argc, char **argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
