#include "USNParser.h"
#include "util.h"
#include <QDebug>

USNParser::USNParser(const char dl) {
  driver_letter = dl;
  char vol[10] = ".:\\";
  vol[0] = dl;
  char sysNameBuf[128];
  int status = GetVolumeInformationA(vol, NULL, 0, NULL, NULL, NULL, sysNameBuf, 128);
  if (status && strcmp(sysNameBuf, "NTFS") == 0) {
    getRootHandle();
  } else {
    qDebug() << "Only NTFS supported yet";
  }
  DWORD br;
  bool isSuccess = DeviceIoControl(root_handle, FSCTL_QUERY_USN_JOURNAL, NULL, 0, &journal, sizeof(journal), &br, NULL);
  if (!isSuccess) isSuccess = createUsnJournal();
  genEntries();
}

std::set<FileEntry*> USNParser::query(const std::wstring& pattern) {
  if (pattern.find(L"\\") == std::wstring::npos) {
    return querySimple(pattern);
  } else {
    return queryComplex(pattern);
  }
}
std::set<FileEntry*> USNParser::querySimple(const std::wstring& pattern) {
  std::set<FileEntry*> res;
  for (auto& kvp : all_entries) {
    if (kvp.second->file_name.find(pattern) != std::wstring::npos) {
      res.insert(kvp.second);
      if (kvp.second->is_folder && sub_entries.count(kvp.second->file_ref)) {
        recursiveAdd(kvp.second->file_ref, res);
      }
    }
  }
  for (auto ptr : res) ptr->genPath(all_entries);
  return res;
}

std::set<FileEntry*> USNParser::queryComplex(const std::wstring& pattern) {
  auto splited = pattern.substr(pattern.find_last_of(L"\\"));
  std::set<FileEntry*> res;
  for (auto& kvp : all_entries) {
    if (kvp.second->file_name.find(splited) != std::wstring::npos) {
      kvp.second->genPath(all_entries);
      if (kvp.second->full_path.find(pattern) != std::wstring::npos) {
        res.insert(kvp.second);
        if (kvp.second->is_folder && sub_entries.count(kvp.second->file_ref)) {
          recursiveAdd(kvp.second->file_ref, res);
        }
      }
    }
  }
  for (auto ptr : res) ptr->genPath(all_entries);
  return res;
}

void USNParser::recursiveAdd(FILEREF folder, std::set<FileEntry*>& res) {
  auto& childs = sub_entries[folder];
  for (auto child : childs) {
    res.insert(child);
    if (child->is_folder && sub_entries.count(child->file_ref)) {
      recursiveAdd(child->file_ref, res);
    }
  }
}

FILEREF USNParser::getFileRef(const std::wstring& path) {
  auto pattern = path.substr(path.find_last_of(L'\\') + 1);
  for (auto& kvp : all_entries) {
    if (kvp.second->file_name.find(pattern) != std::wstring::npos) {
      kvp.second->genPath(all_entries);
      if (kvp.second->full_path == path) return kvp.second->file_ref;
    }
  }
  return 0;
}

bool USNParser::createUsnJournal() {
  CREATE_USN_JOURNAL_DATA cujd;
  DWORD br;
  cujd.MaximumSize = 0;
  cujd.AllocationDelta = 0;
  auto status = DeviceIoControl(root_handle, FSCTL_CREATE_USN_JOURNAL, &cujd, sizeof(cujd), NULL, 0, &br, NULL);
  return status;
}

void USNParser::getRootHandle() {
  char vol[10] = "\\\\.\\C:";
  vol[4] = driver_letter;
  root_handle = CreateFileA(vol,
      GENERIC_READ | GENERIC_WRITE,
      FILE_SHARE_READ | FILE_SHARE_WRITE,
      NULL,
      OPEN_EXISTING,
      FILE_ATTRIBUTE_READONLY,
      NULL);

  if (root_handle == INVALID_HANDLE_VALUE) {
    qDebug() << "Fail to parse USN, please use administrator mode";
  }
}

void USNParser::genEntries() {
  constexpr int BUFLEN = 1 << 18;
  all_entries[ROOT_REFERENCE] = new FileEntry(driver_letter);
  MFT_ENUM_DATA_V0 mftEnumData;
  mftEnumData.StartFileReferenceNumber = 0;
  mftEnumData.LowUsn = 0;
  mftEnumData.HighUsn = journal.NextUsn;
  CHAR buffer[BUFLEN];
  DWORD usnDataSize;
  PUSN_RECORD UsnRecord;
  while (DeviceIoControl(root_handle, FSCTL_ENUM_USN_DATA, &mftEnumData, sizeof(mftEnumData), buffer, BUFLEN, &usnDataSize, NULL)) {
    DWORD dwRetBytes = usnDataSize - sizeof(USN);
    UsnRecord = (PUSN_RECORD)(((PCHAR)buffer) + sizeof(USN));
    while (dwRetBytes>0) {
      if (UsnRecord->FileName[0] != L'$' && UsnRecord->FileName[0] != L'~') {
        auto ptr = new FileEntry(UsnRecord);
        all_entries.insert({ UsnRecord->FileReferenceNumber, ptr });
        if (sub_entries.count(UsnRecord->ParentFileReferenceNumber) == 0)
          sub_entries.insert({ UsnRecord->ParentFileReferenceNumber , std::vector<FileEntry*>() });
        sub_entries[UsnRecord->ParentFileReferenceNumber].push_back(ptr);
      }
      DWORD recordLen = UsnRecord->RecordLength;
      dwRetBytes -= recordLen;
      UsnRecord = (PUSN_RECORD)(((PCHAR)UsnRecord) + recordLen);
    }
    mftEnumData.StartFileReferenceNumber = *(USN *)&buffer;
  }
  cleanHiddenEntries();
}

void USNParser::cleanHiddenEntries() {
  auto iter = all_entries.begin();
  while(iter != all_entries.end()) {
    if (all_entries.count(iter->second->parent_ref) == 0) {
      iter = all_entries.erase(iter);
      auto tmp = sub_entries.find(iter->second->parent_ref);
      if (tmp != sub_entries.end()) sub_entries.erase(tmp);
    } else {
      ++iter;
    }
  }
}

USNParser::~USNParser() {
  if (root_handle != NULL && root_handle != INVALID_HANDLE_VALUE)
      CloseHandle(root_handle);
}
