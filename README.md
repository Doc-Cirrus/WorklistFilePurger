### Modality Worklist Purger plugin for Orthanc DICOM Server
An Orthanc plugin that removes the Modality Worklist files of a study, once the study is received in the Orthanc PACS server.

#### Overview
When a study becomes stable in Orthanc (`StableStudy` change), this plugin reads its `StudyInstanceUID` and removes every matching worklist file from the worklist database folder.
The plugin reads that folder from the `Worklists` section of the Orthanc configuration (the section of the ModalityWorklists plugin), so this section is mandatory.

#### Configuration
```json
{
  "Worklists" : {
    "Enable" : true,
    "Database" : "/var/lib/orthanc/worklists"
  },
  "WorklistPurger" : {
    "Enable" : true
  }
}
```
The plugin does nothing unless both `Worklists.Enable` and `WorklistPurger.Enable` are `true`, and `Worklists.Database` is an existing folder.
A study counts as stable after `StableAge` seconds (60 by default) without a new instance.

#### Worklist file names
The plugin removes the files named `worklist_<StudyInstanceUID>_<anything>`, for example `worklist_1.2.840.10008.1.2_1696339200.wl`. The UID must match exactly. Files that follow another naming scheme are never removed.

#### Building the plugin
Orthanc 1.13.0 or later is required. The build downloads the Orthanc framework and the third-party libraries from `orthanc.uclouvain.be`; it needs `cmake`, `make`, `gcc-c++`, `python3`, `patch` and `libuuid-devel`.
```bash
mkdir Build
cd Build
cmake .. -DSTATIC_BUILD=ON -DALLOW_DOWNLOADS=ON -DCMAKE_BUILD_TYPE=Release
make
ctest
```
This builds `libWorklistPurger.so` and the unit tests (`-DBUILD_UNIT_TESTS=OFF` skips them). To build against an Orthanc source tree that is already unpacked, add `-DORTHANC_FRAMEWORK_SOURCE=path -DORTHANC_FRAMEWORK_ROOT=<Orthanc-1.13.0>/OrthancFramework/Sources`.

`Resources/SyncOrthancFolder.py` refreshes `Resources/Orthanc` (CMake helpers, C++ wrapper and plugin SDK) from an Orthanc release.

#### Callback function used
```bash
OrthancPluginRegisterOnChangeCallback
```
