#include "PagedImportSampleDialog.h"
#include "Application/Instruments/SampleInstrument.h"
#include "Application/Instruments/SamplePool.h"
#ifdef PICOBUILD
#include "pico/multicore.h"
#endif
#include <memory>

#define LIST_WIDTH 24

PagedImportSampleDialog::PagedImportSampleDialog(View &view) : ModalView(view) {
  fileList_.reserve(PAGED_PAGE_SIZE);
  Trace::Log("PAGEDIMPORT", "samplelib is:%s", SAMPLE_LIB_PATH);
}

PagedImportSampleDialog::~PagedImportSampleDialog() {
  Trace::Log("PAGEDIMPORT", "Destruct ===");
}

void PagedImportSampleDialog::DrawView() {
  Trace::Log("PAGEDIMPORT", "DrawView current:%d topIdx:%d", currentSample_,
             topIndex_);

  SetWindow(LIST_WIDTH, PAGED_PAGE_SIZE);
  GUITextProperties props;

// Draw title
#ifdef SHOW_MEM_USAGE
  char title[40];

  SetColor(CD_NORMAL);

  sprintf(title, "MEM [%d]", System::GetInstance()->GetMemoryUsage());
  GUIPoint pos = GUIPoint(0, 0);
  w_.DrawString(title, pos, props);

#endif
  // Draw samples
  int x = 0;
  int y = 0;

  int count = 0;
  char buffer[LIST_WIDTH + 1];
  for (const FileListItem &current : fileList_) {
    if (count == (currentSample_ % PAGED_PAGE_SIZE)) {
      SetColor(CD_HILITE2);
      props.invert_ = true;
    } else {
      SetColor(CD_NORMAL);
      props.invert_ = false;
    }
    if (!current.isDirectory) {
      strncpy(buffer, current.name.c_str(), LIST_WIDTH);
    } else {
      buffer[0] = '[';
      strncpy(buffer + 1, current.name.c_str(), LIST_WIDTH - 2);
      strcat(buffer, "]");
    }
    buffer[LIST_WIDTH] =
        0; // make sure if truncated the filename we have trailing null
    DrawString(x, y, buffer, props);
    y += 1;
    count++;
  };

  SetColor(CD_NORMAL);
};

void PagedImportSampleDialog::warpToNextSample(int direction) {
  int b_currentSample_ = currentSample_ + direction;
  int size = currentDir_->size();
  bool needPage = false;
  int b_topIndex_ = topIndex_;


  // wrap around from first entry to last entry
  if (b_currentSample_ < 0) {
    b_topIndex_ = (size / PAGED_PAGE_SIZE) * PAGED_PAGE_SIZE;
    b_currentSample_ = size - 1; // goto last entry
    needPage = true;
  }
  // wrap around from last entry to first entry
  if (b_currentSample_ >= size) {
    b_currentSample_ = 0;
    b_topIndex_ = 0;
    needPage = true;
  }

  // if we have scrolled off the bottom, page the file list down if not at end
  // of the list
  if ((b_currentSample_ >= (b_topIndex_ + PAGED_PAGE_SIZE)) &&
      ((b_topIndex_ + PAGED_PAGE_SIZE) < size)) {
    b_topIndex_ += PAGED_PAGE_SIZE;
    needPage = true;
  }

  // if we have scrolled off the top, page the file list up if not already at
  // very top of the list
  if (b_currentSample_ < b_topIndex_ && b_topIndex_ != 0) {
    b_topIndex_ -= PAGED_PAGE_SIZE;
    needPage = true;
  }

  //ignroe next page if sample playing , SD card freeze.
  if (Player::GetInstance()->IsPlaying() && needPage) {
      Player::GetInstance()->StopStreaming();
      return;
  }
  currentSample_=b_currentSample_;
  topIndex_=b_topIndex_;

  // need to fetch a new page of the file list of current directory
  if (needPage) {
    fileList_.clear();
    currentDir_->getFileList(topIndex_, &fileList_);
  }

  isDirty_ = true;
}

void PagedImportSampleDialog::OnPlayerUpdate(PlayerEventType,
                                             unsigned int currentTick){};

void PagedImportSampleDialog::OnFocus() {
  Path current(currentPath_);
  setCurrentFolder(&current);
  toInstr_ = viewData_->currentInstrument_;
};

void PagedImportSampleDialog::preview(Path &element) {
    if (Player::GetInstance()->IsPlaying()) {
      Player::GetInstance()->StopStreaming();
      if (currentSample_ != previewPlayingIndex_) {
        previewPlayingIndex_ = currentSample_;
        Player::GetInstance()->StartStreaming(element);
      }
    } else {
      Player::GetInstance()->StartStreaming(element);
      previewPlayingIndex_ = currentSample_;
    }
}

void PagedImportSampleDialog::import(Path &element) {
  //ignroe next page if sample playing , SD card freeze.
  if (Player::GetInstance()->IsPlaying()) {
      Player::GetInstance()->StopStreaming();
      return;
  }
  if(!element.IsFile())return;
  //----
  SamplePool *pool = SamplePool::GetInstance();

#ifdef PICOBUILD
  // Pause core1 in order to be able to write to flash and ensure core1 is
  // not reading from it, it also disables IRQs on it
  // https://www.raspberrypi.com/documentation/pico-sdk/high_level.html#multicore_lockout
  multicore_lockout_start_blocking();
#endif
  int sampleID = pool->ImportSample(element);
#ifdef PICOBUILD
  multicore_lockout_end_blocking();
#endif

  if (sampleID >= 0) {
    I_Instrument *instr =
        viewData_->project_->GetInstrumentBank()->GetInstrument(toInstr_);
    if (instr->GetType() == IT_SAMPLE) {
      SampleInstrument *sinstr = (SampleInstrument *)instr;
      sinstr->AssignSample(sampleID);
      toInstr_ = viewData_->project_->GetInstrumentBank()->GetNext();
    };
  } else {
    Trace::Error("failed to import sample");
  };
  isDirty_ = true;
};

void PagedImportSampleDialog::exit(){
    //ignroe next page if sample playing , SD card freeze.
    if (Player::GetInstance()->IsPlaying()) {
        Player::GetInstance()->StopStreaming();
        return;
    }
    // make sure we free mem from existing currentDir instance
    if (currentDir_ != NULL) {
      delete currentDir_;
    }
    EndModal(0);
}

void PagedImportSampleDialog::ProcessButtonMask(unsigned short mask,
                                                bool pressed) {

  if (!pressed)
    return;
  // make sure to index into the fileList with the offset from topIndex!
  FileListItem currentItem = fileList_[currentSample_ - topIndex_];

  std::string fullPathStr = std::string(currentPath_.GetPath());
  fullPathStr += "/";
  fullPathStr += currentItem.name;
  Path fullPath = Path{fullPathStr};

  if (mask & EPBM_START) { // preview command
    if (mask & EPBM_L) {
      Trace::Log("PAGEDIMPORT", "SHIFT play - import");
      import(fullPath);
    } else {
      if (currentItem.isDirectory) {
        if (currentItem.name == std::string("..")) {
          if (currentPath_.GetPath() == std::string(SAMPLE_LIB_PATH)) {
            exit();
          } else {
            Path parent = currentPath_.GetParent();
            setCurrentFolder(&parent);
          }
        } else {
          auto fullName = currentDir_->getFullName(currentItem.index);
          Path childDirPath = currentPath_.Descend(fullName);
          setCurrentFolder(&childDirPath);
        }
        isDirty_ = true;
        return;
      }else{
        preview(fullPath);
      }
    }
  } else if ((mask & EPBM_R)) { //page command
    //exit page
    if (mask & EPBM_LEFT){
      exit();
    }
    //page jump
    if(fullPath.IsFile() || fullPath.IsDirectory()){
      if (mask & EPBM_UP){
        warpToNextSample(-PAGED_PAGE_SIZE);
      }else if (mask & EPBM_DOWN){
        warpToNextSample(PAGED_PAGE_SIZE);
      }
    }
  } else if (mask & EPBM_B) { //exit
    if (currentPath_.GetPath() == std::string(SAMPLE_LIB_PATH)) {
      exit();
    }else{
      Path parent = currentPath_.GetParent();
      setCurrentFolder(&parent);
      isDirty_ = true;
      return;
    }
  } else if (mask & EPBM_A) {
      if (currentItem.isDirectory) {
        if (currentItem.name == std::string("..")) {
          if (currentPath_.GetPath() == std::string(SAMPLE_LIB_PATH)) {
            exit();
          } else {
            Path parent = currentPath_.GetParent();
            setCurrentFolder(&parent);
          }
        } else {
          auto fullName = currentDir_->getFullName(currentItem.index);
          Path childDirPath = currentPath_.Descend(fullName);
          setCurrentFolder(&childDirPath);
        }
        isDirty_ = true;
        return;
      }else{
          import(fullPath);
      }
  } else if (mask & EPBM_UP) {
      warpToNextSample(-1);
  } else if (mask & EPBM_DOWN) {
      warpToNextSample(1);
  }
}

void PagedImportSampleDialog::setCurrentFolder(Path *path) {
  //ignroe next page if sample playing , SD card freeze.
  if (Player::GetInstance()->IsPlaying()) {
      Player::GetInstance()->StopStreaming();
      return;
  }
  //

  Trace::Log("PAGEDIMPORT", "set Current Folder:%s", path->GetPath().c_str());
  Path formerPath(currentPath_);
  topIndex_ = 0;
  currentSample_ = 0;
  currentPath_ = Path(*path);
  fileList_.clear();
  if (path) {
    // make sure we free mem from existing currentDir instance
    if (currentDir_ != NULL) {
      delete currentDir_;
    }
    currentDir_ = FileSystem::GetInstance()->OpenPaged(path->GetPath().c_str());
    // TODO: show "Loading..." mesg in UI
    Trace::Log("PAGEDIMPORT", "Loading...");
    currentDir_->GetContent("*.wav");
    // TODO: hide "Loading..." mesg in UI
    Trace::Log("PAGEDIMPORT", "Finished Loading");
  }

  // load first page of file/subdirs
  fileList_.clear();
  currentDir_->getFileList(topIndex_, &fileList_);
}
