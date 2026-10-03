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

namespace fs = boost::filesystem;

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

// The services of the Orthanc core that the plugin calls: only "RestApiGet"
// answers something, the other ones (logging, ...) do nothing
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
};

TEST_F(WorklistRemoverTest, getStudyUID) {
    const char* resourceId = "27f7126f-4f66fb14-03f4081b-f9341db2-53925988";
    std::string result;
    ASSERT_EQ(purger_->getStudyUID(resourceId, &result), OrthancPluginErrorCode_Success);
    ASSERT_EQ("12\\.234\\.1234\\.234561", result);
}

TEST_F(WorklistRemoverTest, verifyAndRemoveWorklist) {
    for (int i = 1; i < 10; i+=2) {
        std::string studyUID = "12.234.1234.23456" + std::to_string(i);
        std::string filename = "worklist_" + studyUID + "_unixtime.wl";

        ASSERT_TRUE(exists(filename));
        purger_->verifyAndRemoveWorklistFile(&studyUID);
        ASSERT_FALSE(exists(filename));
    }
}

int main(int argc, char **argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
