#ifndef vcFileDialog_h__
#define vcFileDialog_h__

#include <stddef.h>
#include "udMath.h"
#include "udCallback.h"

struct vcProgramState;
using vcFileDialogCallback = udCallback<void(void)>;

static const char *SupportedFileTypes_Images[] = { ".jpg", ".png", ".tga", ".bmp", ".gif" };
static const char *SupportedFileTypes_Projects[] = { ".json", ".udp" };
static const char *SupportedFileTypes_SceneItems[] = { ".uds", ".ssf", ".udg" };

static const char *SupportedTileTypes_ConvertExport[] = { ".uds" };
static const char *SupportedFileTypes_ConvertImport[] = { ".uds", ".ssf", ".udg", ".las", ".laz", ".e57", ".txt", ".csv", ".asc", ".pts", ".ptx", ".xyz", ".obj"
#ifdef FBXSDK_ON
  , ".fbx"
#endif
};

struct vcFileDialog
{
  bool showDialog;
  bool folderOnly;
  bool openFile; //Alternative is saveFile
  char *pPath;
  size_t pathLen;

  const char **ppExtensions; // Should be static arrays only
  size_t numExtensions;

  vcFileDialogCallback onSelect;
};

void vcFileDialog_Show(vcFileDialog *pDialog, char *pPath, size_t pathLen, const char **ppExtensions, size_t numExtensions, bool openFile, vcFileDialogCallback callback);

template <size_t N> inline void vcFileDialog_Show(vcFileDialog *pDialog, char(&path)[N], const char **ppExtensions, size_t numExtensions, bool openFile, vcFileDialogCallback callback)
{
  vcFileDialog_Show(pDialog, path, N, ppExtensions, numExtensions, openFile, callback);
}

template <size_t M> inline void vcFileDialog_Show(vcFileDialog *pDialog, char *pPath, size_t pathLen, const char *(&extensions)[M], bool openFile, vcFileDialogCallback callback)
{
  vcFileDialog_Show(pDialog, pPath, pathLen, extensions, M, openFile, callback);
}

template <size_t N, size_t M> inline void vcFileDialog_Show(vcFileDialog *pDialog, char(&path)[N], const char *(&extensions)[M], bool openFile, vcFileDialogCallback callback)
{
  vcFileDialog_Show(pDialog, path, N, extensions, M, openFile, callback);

  // These unused are required for compiling to work
  udUnused(SupportedFileTypes_Images);
  udUnused(SupportedFileTypes_Projects);
  udUnused(SupportedFileTypes_SceneItems);
  udUnused(SupportedTileTypes_ConvertExport);
  udUnused(SupportedFileTypes_ConvertImport);
}

bool vcFileDialog_DrawImGui(char *pPath, size_t pathLength, bool loadOnly = true, const char **ppExtensions = nullptr, size_t extensionCount = 0);

#endif //vcMenuButtons_h__
