#ifndef _PAGED_IMPORT_SAMPLE_DIALOG_H_
#define _PAGED_IMPORT_SAMPLE_DIALOG_H_

#include "Application/Views/BaseClasses/ModalView.h"
#include "Foundation/T_SimpleList.h"
#include "System/FileSystem/FileSystem.h"
#include <string>
#include <vector>

class PagedImportSampleDialog : public ModalView {
public:
  PagedImportSampleDialog(View &view);
  virtual ~PagedImportSampleDialog();

  virtual void DrawView();
  virtual void OnPlayerUpdate(PlayerEventType, unsigned int currentTick);
  virtual void OnFocus();
  virtual void ProcessButtonMask(unsigned short mask, bool pressed);
  virtual void AnimationUpdate(){};

protected:
  void setCurrentFolder(Path *path);
  void warpToNextSample(int dir);
  void import(Path &element);
  void preview(Path &element);
  void exit();

private:
  std::vector<FileListItem> fileList_{};
  int currentSample_;
  int topIndex_ = 0;
  int toInstr_;
  int previewPlayingIndex_ = 0;
  Path currentPath_{SAMPLE_LIB_PATH};
  I_PagedDir *currentDir_{};
};

#endif
