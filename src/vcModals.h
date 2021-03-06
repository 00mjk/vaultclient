#ifndef vcModals_h__
#define vcModals_h__

enum vcModalTypes
{
  // These are handled by DrawModals
  vcMT_LoggedOut,
  vcMT_AddSceneItem,
  vcMT_LoadProject,
  vcMT_CreateProject,
  vcMT_Welcome,
  vcMT_ExportProject,
  vcMT_ImageViewer,
  vcMT_ProjectSettings,
  vcMT_ProjectChange,
  vcMT_ProjectReadOnly,
  vcMT_ProjectInfo,
  vcMT_ErrorInformation,
  vcMT_UnsupportedEncoding,
  vcMT_Profile,
  vcMT_Convert,
  vcMT_ChangePassword,
  vcMT_ShowInputInfo,
  vcMT_ImportShapeFile,
  vcMT_FlythroughExport,
  vcMT_ImportAnnotations,
  vcMT_UserGuide,

  vcMT_Count
};

struct vcState;

void vcModals_OpenModal(vcState *pProgramState, vcModalTypes type);
void vcModals_CloseModal(vcState *pProgramState, vcModalTypes type);
void vcModals_DrawModals(vcState *pProgramState);

// Returns true if its safe to write- if exists the user is asked if it can be overriden
bool vcModals_OverwriteExistingFile(vcState *pProgramState, const char *pFilename, const char *pFileExistingMsg = nullptr);

// Returns true if user accepts action
bool vcModals_AllowDestructiveAction(vcState *pProgramState, const char *pTitle, const char *pMessage);

// Returns true if user accepts ending the session, `isQuit` is false when logging out
bool vcModals_ConfirmEndSession(vcState *pProgramState, bool isQuit);

// Returns true if user accepts Dropbox link change
bool vcModals_DropboxHelp(vcState *pProgramState, const char *pURL, const char *pNewURL);

#endif //vcModals_h__
