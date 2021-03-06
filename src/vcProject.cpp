#include "vcProject.h"

#include "vcSceneItem.h"
#include "vcState.h"
#include "vcRender.h"
#include "vcModals.h"
#include "vcQueryNode.h"

#include "udFile.h"
#include "udStringUtil.h"
#include "vcError.h"

const char *vcProject_ErrorToString(udError error)
{
  switch (error)
  {
  case udE_InvalidParameter:
    return vcString::Get("errorInvalidParameter");
  case udE_OpenFailure:
    return vcString::Get("errorOpenFailure");
  case udE_NotSupported:
    return vcString::Get("errorUnsupported");
  case udE_WriteFailure:
    return vcString::Get("errorFileExists");
  case udE_ExceededAllowedLimit:
    return vcString::Get("errorExceedsProjectLimit");
  case udE_Failure: // Falls through
  default:
    return vcString::Get("errorUnknown");
  }
}

void vcProject_UpdateProjectHistory(vcState *pProgramState, const char *pFilename, bool isServerProject);

void vcProject_InitScene(vcState *pProgramState, int srid)
{
  udGeoZone zone = {};
  vcGIS_ChangeSpace(&pProgramState->geozone, zone);

  for (int viewportIndex = 0; viewportIndex < pProgramState->settings.activeViewportCount; ++viewportIndex)
  {
    pProgramState->pViewports[viewportIndex].camera.position = udDouble3::zero();
    pProgramState->pViewports[viewportIndex].camera.headingPitch = udDouble2::zero();
  }

  pProgramState->sceneExplorer.selectedItems.clear();
  pProgramState->sceneExplorer.clickedItem = {};

  udProject_GetProjectRoot(pProgramState->activeProject.pProject, &pProgramState->activeProject.pRoot);

  pProgramState->activeProject.pFolder = new vcFolder(&pProgramState->activeProject, pProgramState->activeProject.pRoot, pProgramState);
  pProgramState->activeProject.pRoot->pUserData = pProgramState->activeProject.pFolder;
  pProgramState->activeProject.slideshow = false;
  pProgramState->activeProject.pSlideshowViewpoint = nullptr;

  udGeoZone_SetFromSRID(&pProgramState->activeProject.baseZone, srid);

  int cameraProjection = (srid == vcPSZ_StandardGeoJSON) ? vcPSZ_WGS84ECEF : srid; // use ECEF if its default, otherwise the requested zone

  if (cameraProjection != 0)
  {
    udGeoZone cameraZone = {};
    udGeoZone_SetFromSRID(&cameraZone, cameraProjection);

    if (vcGIS_ChangeSpace(&pProgramState->geozone, cameraZone))
      pProgramState->activeProject.pFolder->ChangeProjection(cameraZone);

    if (cameraProjection == vcPSZ_WGS84ECEF || cameraZone.latLongBoundMin == cameraZone.latLongBoundMax)
    {
      double locations[][5] = {
        { 309281.960926, 5640790.149293, 2977479.571028, 55.74, -32.45 }, // Mount Everest
        { 4443919.137517, 556287.927124, 4540116.021340, 21.07, -10.85 }, // Valley in France
        { 6390753.962424, 1173147.659817, 5866300.533479, 3.25, -76.07 }, // Europe High
        { -5345572.793165, 5951831.265765, -4079550.822723, 1.33, -84.59 }, // Australia High
      };

      uint64_t length = (uint64_t)udLengthOf(locations);
      uint64_t seed = udGetEpochMilliSecsUTCd();
      int randomIndex = (int)(seed % length);
      double *pPlace = locations[randomIndex];

      for (int viewportIndex = 0; viewportIndex < pProgramState->settings.activeViewportCount; ++viewportIndex)
      {
        pProgramState->pViewports[viewportIndex].camera.position = udDouble3::create(pPlace[0], pPlace[1], pPlace[2]);
        pProgramState->pViewports[viewportIndex].camera.headingPitch = { UD_DEG2RAD(pPlace[3]), UD_DEG2RAD(pPlace[4]) };
      }
    }
    else
    {
      for (int viewportIndex = 0; viewportIndex < pProgramState->settings.activeViewportCount; ++viewportIndex)
      {
        pProgramState->pViewports[viewportIndex].camera.position = udGeoZone_LatLongToCartesian(cameraZone, udDouble3::create((cameraZone.latLongBoundMin + cameraZone.latLongBoundMax) / 2.0, 10000.0));
        pProgramState->pViewports[viewportIndex].camera.headingPitch = { 0.0, UD_DEG2RAD(-80.0) };
      }
    }
  }

  udProjectNode_SetMetadataInt(pProgramState->activeProject.pRoot, "defaultcrs", pProgramState->geozone.srid);
  udProjectNode_SetMetadataInt(pProgramState->activeProject.pRoot, "projectcrs", srid);
  udProject_Save(pProgramState->activeProject.pProject);
}

bool vcProject_CreateBlankScene(vcState *pProgramState, const char *pName, int srid)
{
  if (pProgramState == nullptr || pName == nullptr)
    return false;

  udProject *pNewProject = nullptr;
  if (udProject_CreateInMemory(&pNewProject, pName) != udE_Success)
    return false;

  if (pProgramState->activeProject.pProject != nullptr)
    vcProject_Deinit(pProgramState, &pProgramState->activeProject);

  pProgramState->activeProject.pProject = pNewProject;
  vcProject_InitScene(pProgramState, srid);

  return true;
}

udError vcProject_CreateFileScene(vcState *pProgramState, const char *pFileName, const char *pProjectName, int srid)
{
  if (pProgramState == nullptr)
    return udE_Failure;
  if (pFileName == nullptr || pFileName[0] == '\0')
    return udE_InvalidParameter;
  if (pProjectName == nullptr || pProjectName[0] == '\0')
    return udE_InvalidParameter;

  udProject* pNewProject = nullptr;

  if (udFileExists(pFileName) == udR_Success)
  {
    if (!pProgramState->settings.window.useNativeUI)
    {
      if (vcModals_OverwriteExistingFile(pProgramState, pFileName))
        udFileDelete(pFileName);
      else
        return udE_WriteFailure;
    }
    else // File exists and Native UI
    {
      udFileDelete(pFileName);
    }
  }

  udError error = udProject_CreateInFile(&pNewProject, pProjectName, pFileName);
  if (error != udE_Success)
    return error;

  if (pProgramState->activeProject.pProject != nullptr)
    vcProject_Deinit(pProgramState, &pProgramState->activeProject);

  pProgramState->activeProject.pProject = pNewProject;
  vcProject_InitScene(pProgramState, srid);

  vcProject_UpdateProjectHistory(pProgramState, pFileName, false);

  return error;
}

udError vcProject_CreateServerScene(vcState *pProgramState, const char *pName, const char *pGroupUUID, int srid)
{
  if (pProgramState == nullptr || pName == nullptr || pGroupUUID == nullptr)
    return udE_InvalidParameter;

  udProject *pNewProject = nullptr;
  udError error = udProject_CreateInServer(pProgramState->pUDSDKContext, &pNewProject, pName, pGroupUUID);
  if (error != udE_Success)
    return error;

  if (pProgramState->activeProject.pProject != nullptr)
    vcProject_Deinit(pProgramState, &pProgramState->activeProject);

  pProgramState->activeProject.pProject = pNewProject;
  vcProject_InitScene(pProgramState, srid);

  const char *pProjectUUID = nullptr;
  udProject_GetProjectUUID(pNewProject, &pProjectUUID);
  vcProject_UpdateProjectHistory(pProgramState, pProjectUUID, true);

  return error;
}

bool vcProject_ExtractCameraRecursive(vcState *pProgramState, udProjectNode *pParentNode)
{
  udProjectNode *pNode = pParentNode->pFirstChild;
  while (pNode != nullptr)
  {
    if (pNode->itemtype == udPNT_Viewpoint)
    {
      udDouble3 position = udDouble3::zero();
      udDouble2 headingPitch = udDouble2::zero();

      udDouble3 *pPoint = nullptr;
      int numPoints = 0;

      vcProject_FetchNodeGeometryAsCartesian(&pProgramState->activeProject, pNode, pProgramState->geozone, &pPoint, &numPoints);
      if (numPoints == 1)
        position = pPoint[0];

      udProjectNode_GetMetadataDouble(pNode, "transform.heading", &headingPitch.x, 0.0);
      udProjectNode_GetMetadataDouble(pNode, "transform.pitch", &headingPitch.y, 0.0);

      for (int viewportIndex = 0; viewportIndex < pProgramState->settings.activeViewportCount; ++viewportIndex)
      {
        pProgramState->pViewports[viewportIndex].camera.position = position;
        pProgramState->pViewports[viewportIndex].camera.headingPitch = headingPitch;
      }

      // unset
      memset(pProgramState->sceneExplorer.movetoUUIDWithoutProjectionWhenPossible, 0, sizeof(pProgramState->sceneExplorer.movetoUUIDWithoutProjectionWhenPossible));
      return true;
    }
    else if (pNode->itemtype == udPNT_PointCloud)
    {
      udStrcpy(pProgramState->sceneExplorer.movetoUUIDWithoutProjectionWhenPossible, pNode->UUID);
    }

    if (vcProject_ExtractCameraRecursive(pProgramState, pNode))
      return true;

    pNode = pNode->pNextSibling;
  }

  return false;
}

// Try extract a valid viewpoint from the project, based on available nodes
void vcProject_ExtractCamera(vcState *pProgramState)
{
  for (int viewportIndex = 0; viewportIndex < pProgramState->settings.activeViewportCount; ++viewportIndex)
  {
    pProgramState->pViewports[viewportIndex].camera.position = udDouble3::zero();
    pProgramState->pViewports[viewportIndex].camera.headingPitch = udDouble2::zero();
  }

  vcProject_ExtractCameraRecursive(pProgramState, pProgramState->activeProject.pRoot);
}

void vcProject_UpdateProjectHistory(vcState *pProgramState, const char *pFilename, bool isServerProject)
{
  // replace '\\' with '/'
  char *pFormattedPath = udStrdup(pFilename);
  size_t index = 0;
  while (udStrchr(pFormattedPath, "\\", &index) != nullptr)
    pFormattedPath[index] = '/';

  for (size_t i = 0; i < pProgramState->settings.projectsHistory.projects.length; ++i)
  {
    if (udStrEqual(pFormattedPath, pProgramState->settings.projectsHistory.projects[i].pPath))
    {
      vcSettings_CleanupHistoryProjectItem(&pProgramState->settings.projectsHistory.projects[i]);
      pProgramState->settings.projectsHistory.projects.RemoveAt(i);
      break;
    }
  }

  while (pProgramState->settings.projectsHistory.projects.length >= vcMaxProjectHistoryCount)
  {
    vcSettings_CleanupHistoryProjectItem(&pProgramState->settings.projectsHistory.projects[pProgramState->settings.projectsHistory.projects.length - 1]);
    pProgramState->settings.projectsHistory.projects.PopBack();
  }

  const char *pProjectName = udStrdup(pProgramState->activeProject.pRoot->pName);
  pProgramState->settings.projectsHistory.projects.PushFront({ isServerProject, pProjectName, pFormattedPath });
}

void vcProject_RemoveHistoryItem(vcState *pProgramState, size_t itemPosition)
{
  if (pProgramState == nullptr || itemPosition >= pProgramState->settings.projectsHistory.projects.length)
    return;

  vcSettings_CleanupHistoryProjectItem(&pProgramState->settings.projectsHistory.projects[itemPosition]);
  pProgramState->settings.projectsHistory.projects.RemoveAt(itemPosition);
}

bool vcProject_LoadFromServer(vcState *pProgramState, const char *pProjectID)
{
  udProject *pProject = nullptr;
  udError vResult = udProject_LoadFromServer(pProgramState->pUDSDKContext, &pProject, pProjectID);
  if (vResult == udE_Success)
  {
    vcProject_Deinit(pProgramState, &pProgramState->activeProject);

    udGeoZone zone = {};
    vcGIS_ChangeSpace(&pProgramState->geozone, zone);

    pProgramState->sceneExplorer.selectedItems.clear();
    pProgramState->sceneExplorer.clickedItem = {};

    pProgramState->activeProject.pProject = pProject;
    udProject_GetProjectRoot(pProgramState->activeProject.pProject, &pProgramState->activeProject.pRoot);
    pProgramState->activeProject.pFolder = new vcFolder(&pProgramState->activeProject, pProgramState->activeProject.pRoot, pProgramState);
    pProgramState->activeProject.pRoot->pUserData = pProgramState->activeProject.pFolder;
    vcProject_GetNodeMetadata(pProgramState->activeProject.pRoot, "slideshow", &pProgramState->activeProject.slideshow, false);
    pProgramState->activeProject.pSlideshowViewpoint = nullptr;

    int32_t projectZone = vcPSZ_StandardGeoJSON; // LongLat
    udProjectNode_GetMetadataInt(pProgramState->activeProject.pRoot, "projectcrs", &projectZone, vcPSZ_StandardGeoJSON);
    if (projectZone > 0 && udGeoZone_SetFromSRID(&pProgramState->activeProject.baseZone, projectZone) != udR_Success)
      udGeoZone_SetFromSRID(&pProgramState->activeProject.baseZone, vcPSZ_StandardGeoJSON);

    pProgramState->activeProject.recommendedSRID = -1;
    if (udProjectNode_GetMetadataInt(pProgramState->activeProject.pRoot, "defaultcrs", &pProgramState->activeProject.recommendedSRID, pProgramState->activeProject.baseZone.srid) == udE_Success && pProgramState->activeProject.recommendedSRID >= 0 && ((udGeoZone_SetFromSRID(&zone, pProgramState->activeProject.recommendedSRID) == udR_Success) || pProgramState->activeProject.recommendedSRID == 0))
      vcGIS_ChangeSpace(&pProgramState->geozone, zone);

    const char *pInfo = nullptr;
    if (udProjectNode_GetMetadataString(pProgramState->activeProject.pRoot, "information", &pInfo, "") == udE_Success)
      vcModals_OpenModal(pProgramState, vcMT_ProjectInfo);

    vcProject_ExtractCamera(pProgramState);
    vcProject_UpdateProjectHistory(pProgramState, pProjectID, true);
  }
  else
  {
    ErrorItem projectError;
    projectError.source = vcES_ProjectChange;
    projectError.pData = udStrdup(pProjectID);

    if (vResult == udE_NotFound)
      projectError.resultCode = udR_ObjectNotFound;
    else if (vResult == udE_ParseError)
      projectError.resultCode = udR_ParseError;
    else
      projectError.resultCode = udR_Failure_;

    vcError_AddError(pProgramState, projectError);

    vcModals_OpenModal(pProgramState, vcMT_ProjectChange);
  }

  return (pProject != nullptr);
}

bool vcProject_LoadFromURI(vcState *pProgramState, const char *pFilename)
{
  bool success = false;
  udProject *pProject = nullptr;
  if (udProject_LoadFromFile(&pProject, pFilename) == udE_Success)
  {
    vcProject_Deinit(pProgramState, &pProgramState->activeProject);

    udGeoZone zone = {};
    vcGIS_ChangeSpace(&pProgramState->geozone, zone);

    pProgramState->sceneExplorer.selectedItems.clear();
    pProgramState->sceneExplorer.clickedItem = {};

    pProgramState->activeProject.pProject = pProject;
    udProject_GetProjectRoot(pProgramState->activeProject.pProject, &pProgramState->activeProject.pRoot);
    pProgramState->activeProject.pFolder = new vcFolder(&pProgramState->activeProject, pProgramState->activeProject.pRoot, pProgramState);
    pProgramState->activeProject.pRoot->pUserData = pProgramState->activeProject.pFolder;
    vcProject_GetNodeMetadata(pProgramState->activeProject.pRoot, "slideshow", &pProgramState->activeProject.slideshow, false);
    pProgramState->activeProject.pSlideshowViewpoint = nullptr;

    udFilename temp(pFilename);
    temp.SetFilenameWithExt("");
    pProgramState->activeProject.pRelativeBase = udStrdup(temp.GetPath());

    int32_t projectZone = vcPSZ_StandardGeoJSON; // LongLat
    udProjectNode_GetMetadataInt(pProgramState->activeProject.pRoot, "projectcrs", &projectZone, vcPSZ_StandardGeoJSON);
    if (projectZone > 0 && udGeoZone_SetFromSRID(&pProgramState->activeProject.baseZone, projectZone) != udR_Success)
      udGeoZone_SetFromSRID(&pProgramState->activeProject.baseZone, vcPSZ_StandardGeoJSON);

    pProgramState->activeProject.recommendedSRID = -1;
    if (udProjectNode_GetMetadataInt(pProgramState->activeProject.pRoot, "defaultcrs", &pProgramState->activeProject.recommendedSRID, pProgramState->activeProject.baseZone.srid) == udE_Success && pProgramState->activeProject.recommendedSRID >= 0 && ((udGeoZone_SetFromSRID(&zone, pProgramState->activeProject.recommendedSRID) == udR_Success) || pProgramState->activeProject.recommendedSRID == 0))
      vcGIS_ChangeSpace(&pProgramState->geozone, zone);

    const char *pInfo = nullptr;
    if (udProjectNode_GetMetadataString(pProgramState->activeProject.pRoot, "information", &pInfo, "") == udE_Success)
      vcModals_OpenModal(pProgramState, vcMT_ProjectInfo);

    vcProject_ExtractCamera(pProgramState);
    vcProject_UpdateProjectHistory(pProgramState, pFilename, false);

    success = true;
  }
  else
  {
    ErrorItem projectError;
    projectError.source = vcES_ProjectChange;
    projectError.pData = udStrdup(pFilename);
    projectError.resultCode = udR_ParseError;

    vcError_AddError(pProgramState, projectError);

    vcModals_OpenModal(pProgramState, vcMT_ProjectChange);
  }

  return success;
}

// This won't be required after destroy list works in udProject
void vcProject_RecursiveDestroyUserData(vcState *pProgramData, udProjectNode *pFirstSibling)
{
  udProjectNode *pNode = pFirstSibling;

  do
  {
    if (pNode->pFirstChild)
      vcProject_RecursiveDestroyUserData(pProgramData, pNode->pFirstChild);

    if (pNode->pUserData)
    {
      vcSceneItem *pItem = (vcSceneItem*)pNode->pUserData;

      if (pItem != nullptr)
      {
        if (pItem->m_loadStatus == vcSLS_Pending)
          udInterlockedCompareExchange(&pItem->m_loadStatus, vcSLS_Unloaded, vcSLS_Pending);

        while (pItem->m_loadStatus == vcSLS_Loading)
          udYield(); // Spin until other thread stops processing

        if (pItem->m_loadStatus == vcSLS_Loaded || pItem->m_loadStatus == vcSLS_OpenFailure || pItem->m_loadStatus == vcSLS_Failed)
        {
          pItem->Cleanup(pProgramData);
          delete pItem;
          pNode->pUserData = nullptr;
        }
      }
    }

    pNode = pNode->pNextSibling;
  } while (pNode != nullptr);
}

void vcProject_Deinit(vcState *pProgramData, vcProject *pProject)
{
  if (pProject == nullptr || pProject->pProject == nullptr)
    return;

  pProgramData->activeTool = vcActiveTool_Select;
  pProgramData->activeProject.pSlideshowViewpoint = nullptr;
  if (pProgramData->pActiveViewport)
    pProgramData->pActiveViewport->cameraInput.pAttachedToSceneItem = nullptr;
  udFree(pProject->pRelativeBase);
  vcProject_RecursiveDestroyUserData(pProgramData, pProject->pRoot);
  udProject_Release(&pProject->pProject);
}

bool vcProject_Save(vcState *pProgramState)
{
  if (pProgramState == nullptr)
    return false;

  udError status = udProject_Save(pProgramState->activeProject.pProject);

  if (status != udE_Success)
  {
    ErrorItem projectError = {};
    projectError.source = vcES_ProjectChange;
    projectError.pData = nullptr;

    if (status == udE_WriteFailure)
      projectError.resultCode = udR_WriteFailure;
    else
      projectError.resultCode = udR_Failure_;

    vcError_AddError(pProgramState, projectError);
    vcModals_OpenModal(pProgramState, vcMT_ProjectChange);
  }
  else
  {
    pProgramState->lastSuccessfulSave = udGetEpochSecsUTCf();
  }

  return (status == udE_Success);
}

void vcProject_AutoCompletedName(udFilename *exportFilename, const char *pFileName, const char *pDefaultName)
{
  udFindDir *pDir = nullptr;
  if (!udStrEquali(pFileName, "") && !udStrEndsWithi(pFileName, "/") && !udStrEndsWithi(pFileName, "\\") && udOpenDir(&pDir, pFileName) == udR_Success)
    exportFilename->SetFromFullPath("%s/%s.json", pFileName, pDefaultName);
  else if (exportFilename->HasFilename())
    exportFilename->SetExtension(".json");
  else
    exportFilename->SetFilenameWithExt(udTempStr("%s.json", pDefaultName));

  udCloseDir(&pDir);
}

bool vcProject_SaveAs(vcState *pProgramState, const char *pPath, bool allowOverride)
{
  if (pProgramState == nullptr || pPath == nullptr)
    return false;
  
  udFilename exportFilename(pPath);
  vcProject_AutoCompletedName(&exportFilename, pPath, pProgramState->activeProject.pRoot->pName);

  // Check if file path exists before writing to disk, and if so, the user will be presented with the option to overwrite or cancel
  if (!allowOverride && !vcModals_OverwriteExistingFile(pProgramState, exportFilename.GetPath()))
    return false;

  ErrorItem projectError = {};
  projectError.source = vcES_ProjectChange;
  projectError.pData = udStrdup(exportFilename.GetFilenameWithExt());

  if (udProject_SaveToFile(pProgramState->activeProject.pProject, exportFilename.GetPath()) == udE_Success)
    projectError.resultCode = udR_Success;
  else
    projectError.resultCode = udR_WriteFailure;

  if (projectError.resultCode != udR_Success)
  {
    pProgramState->errorItems.PushBack(projectError);
    vcModals_OpenModal(pProgramState, vcMT_ProjectChange);
  }

  vcProject_UpdateProjectHistory(pProgramState, exportFilename.GetPath(), false);

  return (projectError.resultCode == udR_Success);
}

udError vcProject_SaveAsServer(vcState *pProgramState, const char *pProjectID)
{
  if (pProgramState == nullptr || pProjectID == nullptr)
    return udE_InvalidParameter;

  ErrorItem projectError = {};
  projectError.source = vcES_ProjectChange;
  projectError.pData = udStrdup(pProjectID);

  udError result = udProject_SaveToServer(pProgramState->pUDSDKContext, pProgramState->activeProject.pProject, pProjectID);

  if (result == udE_Success)
    projectError.resultCode = udR_Success;
  else if (result == udE_ExceededAllowedLimit)
    projectError.resultCode = udR_ExceededAllowedLimit;
  else
    projectError.resultCode = udR_WriteFailure;

  vcError_AddError(pProgramState, projectError);

  vcModals_OpenModal(pProgramState, vcMT_ProjectChange);
  vcProject_UpdateProjectHistory(pProgramState, pProjectID, true);

  return result;
}

bool vcProject_AbleToChange(vcState *pProgramState)
{
  if (pProgramState == nullptr || !pProgramState->hasContext)
    return false;

  if (udProject_HasUnsavedChanges(pProgramState->activeProject.pProject) == udE_NotFound)
    return true;

  return vcModals_AllowDestructiveAction(pProgramState, vcString::Get("menuChangeScene"), vcString::Get("menuChangeSceneConfirm"));
}

void vcProject_RemoveItem(vcState *pProgramState, udProjectNode *pParent, udProjectNode *pNode)
{
  for (int32_t i = 0; i < (int32_t)pProgramState->sceneExplorer.selectedItems.size(); ++i)
  {
    if (pProgramState->sceneExplorer.selectedItems[i].pParent == pParent && pProgramState->sceneExplorer.selectedItems[i].pItem == pNode)
    {
      pProgramState->sceneExplorer.selectedItems.erase(pProgramState->sceneExplorer.selectedItems.begin() + i);
      --i;
    }
  }

  if (pNode == pProgramState->activeProject.pSlideshowViewpoint)
    pProgramState->activeProject.pSlideshowViewpoint = nullptr;

  if (pNode->pUserData == pProgramState->pActiveViewport->cameraInput.pAttachedToSceneItem)
    pProgramState->pActiveViewport->cameraInput.pAttachedToSceneItem = nullptr;

  vcSceneItem *pItem = pNode == nullptr ? nullptr : (vcSceneItem*)pNode->pUserData;

  if (pItem != nullptr)
  {
    if (pItem->m_loadStatus == vcSLS_Pending)
      udInterlockedCompareExchange(&pItem->m_loadStatus, vcSLS_Unloaded, vcSLS_Pending);

    while (pItem->m_loadStatus == vcSLS_Loading)
      udYield(); // Spin until other thread stops processing

    if (pItem->m_loadStatus == vcSLS_Loaded || pItem->m_loadStatus == vcSLS_OpenFailure || pItem->m_loadStatus == vcSLS_Failed)
    {
      pItem->Cleanup(pProgramState);
      delete pItem;
      pNode->pUserData = nullptr;
    }
  }

  udProjectNode_RemoveChild(pProgramState->activeProject.pProject, pParent, pNode);
}

void vcProject_RemoveSelectedFolder(vcState *pProgramState, udProjectNode *pFolderNode)
{
  udProjectNode *pNode = pFolderNode->pFirstChild;

  while (pNode != nullptr)
  {
    vcSceneItem *pItem = (vcSceneItem*)pNode->pUserData;

    if (pItem != nullptr && pItem->m_selected)
      vcProject_RemoveItem(pProgramState, pFolderNode, pNode);

    if (pNode->itemtype == udPNT_Folder)
      vcProject_RemoveSelectedFolder(pProgramState, pNode);

    pNode = pNode->pNextSibling;
  }
}

void vcProject_RemoveSelected(vcState *pProgramState)
{
  vcProject_RemoveSelectedFolder(pProgramState, pProgramState->activeProject.pRoot);

  pProgramState->sceneExplorer.selectStartItem = {};
  pProgramState->sceneExplorer.selectedItems.clear();
  pProgramState->sceneExplorer.clickedItem = {};
}

bool vcProject_ContainsItem(udProjectNode *pParentNode, udProjectNode *pItem)
{
  if (pParentNode == pItem)
    return true;

  udProjectNode *pNode = pParentNode->pFirstChild;

  while (pNode != nullptr)
  {
    if (pNode == pItem)
      return true;

    if (pNode->itemtype == udPNT_Folder && vcProject_ContainsItem(pNode, pItem))
      return true;

    pNode = pNode->pNextSibling;
  }

  return false;
}

static void vcProject_SelectRange(vcState *pProgramState, udProjectNode *pNode1, udProjectNode *pNode2)
{
  udChunkedArray<udProjectNode *> stack;
  stack.Init(32);
  stack.PushBack(pProgramState->activeProject.pRoot);

  udProjectNode *pChild = pProgramState->activeProject.pRoot->pFirstChild;
  bool select = false;
  do
  {
    // If the current node is either of the end nodes in the range, toggle the selection
    bool toggleSelect = (pChild == pNode1 || pChild == pNode2);
    if (toggleSelect)
      select = !select;

    // If selecting (or toggling to include last item), select the current node
    if (select || toggleSelect)
    {
      ((vcSceneItem *)pChild->pUserData)->m_selected = true;
      pProgramState->sceneExplorer.selectedItems.push_back({ stack[stack.length - 1], pChild });
    }

    // If no longer selecting, break out of the loop
    if (toggleSelect && !select)
      break;

    // Add child to the stack to simplify the code below
    stack.PushBack(pChild);

    // Depth first search for the nodes
    if (pChild->pFirstChild)
    {
      pChild = pChild->pFirstChild;
      continue;
    }

    // Try the sibling of the current node (pChild)
    if (pChild->pNextSibling)
    {
      pChild = pChild->pNextSibling;
      stack.PopBack();
      continue;
    }

    // There are no children or siblings, pop up the stack until there's a sibling
    do
    {
      stack.PopBack();
      pChild = stack[stack.length - 1];
    } while (pChild->pNextSibling == nullptr);

    // Pop the current node and try the sibling
    stack.PopBack();
    pChild = pChild->pNextSibling;
  } while (stack.length > 0);

  stack.Deinit();
}

void vcProject_SelectItem(vcState *pProgramState, udProjectNode *pParent, udProjectNode *pNode)
{
  vcSceneItem *pItem = (vcSceneItem*)pNode->pUserData;

  // If we're doing range selection, else normal selection
  if (pItem != nullptr && pProgramState->sceneExplorer.selectStartItem.pItem != nullptr && pProgramState->sceneExplorer.selectStartItem.pItem != pNode)
  {
    udProjectNode *pSelectNode = pProgramState->sceneExplorer.selectStartItem.pItem;
    vcProject_SelectRange(pProgramState, pNode, pSelectNode);
  }
  else if (pItem != nullptr && !pItem->m_selected)
  {
    pItem->m_selected = true;
    pProgramState->sceneExplorer.selectedItems.push_back({ pParent, pNode });
  }
}

void vcProject_UnselectItem(vcState *pProgramState, udProjectNode *pParent, udProjectNode *pNode)
{
  vcSceneItem *pItem = (vcSceneItem *)pNode->pUserData;

  if (pItem != nullptr && pItem->m_selected)
  {
    pItem->m_selected = false;
    for (int32_t i = 0; i < (int32_t)pProgramState->sceneExplorer.selectedItems.size(); ++i)
    {
      const vcSceneItemRef &item = pProgramState->sceneExplorer.selectedItems[i];
      if (item.pParent == pParent && item.pItem == pNode)
      {
        pProgramState->sceneExplorer.selectedItems.erase(pProgramState->sceneExplorer.selectedItems.begin() + i);
        --i;
      }
    }
  }
}

void vcProject_ClearSelection(udProjectNode *pParentNode)
{
  udProjectNode *pNode = pParentNode->pFirstChild;

  while (pNode != nullptr)
  {
    if (pNode->itemtype == udPNT_Folder)
      vcProject_ClearSelection(pNode);
    else if (pNode->pUserData != nullptr)
      ((vcSceneItem*)pNode->pUserData)->m_selected = false;

    pNode = pNode->pNextSibling;
  }

  if (pParentNode->pUserData != nullptr)
    ((vcSceneItem*)pParentNode->pUserData)->m_selected = false;
}

void vcProject_ClearSelection(vcState *pProgramState, bool clearToolState /*= true*/)
{
  vcProject_ClearSelection(pProgramState->activeProject.pRoot);
  pProgramState->sceneExplorer.selectedItems.clear();
  pProgramState->sceneExplorer.clickedItem = {};

  if (clearToolState)
    pProgramState->activeTool = vcActiveTool_Select;
}

bool vcProject_UseProjectionFromItem(vcState *pProgramState, vcSceneItem *pItem)
{
  if (pProgramState == nullptr || pItem == nullptr || pProgramState->programComplete || pItem->m_pPreferredProjection == nullptr)
    return false;

  if (vcGIS_ChangeSpace(&pProgramState->geozone, *pItem->m_pPreferredProjection))
    pProgramState->activeProject.pFolder->ChangeProjection(*pItem->m_pPreferredProjection);

  // move camera to the new item's position
  pItem->SetCameraPosition(pProgramState);

  return true;
}

bool vcProject_UpdateNodeGeometryFromCartesian(vcProject *pProject, udProjectNode *pNode, const udGeoZone &zone, udProjectGeometryType newType, udDouble3 *pPoints, int numPoints)
{
  if (pProject == nullptr || pNode == nullptr)
    return false;

  udDouble3 *pGeom = nullptr;

  udError result = udE_Failure;

  if (zone.srid != 0 && pProject->baseZone.srid != 0) //Geolocated
  {
    pGeom = udAllocType(udDouble3, numPoints, udAF_Zero);

    // Change all points from the projection
    for (int i = 0; i < numPoints; ++i)
      pGeom[i] = udGeoZone_TransformPoint(pPoints[i], zone, pProject->baseZone);

    result = udProjectNode_SetGeometry(pProject->pProject, pNode, newType, numPoints, (double*)pGeom);
    udFree(pGeom);
  }
  else
  {
    result = udProjectNode_SetGeometry(pProject->pProject, pNode, newType, numPoints, (double*)pPoints);
  }

  return (result == udE_Success);
}

bool vcProject_UpdateNodeGeometryFromLatLong(vcProject *pProject, udProjectNode *pNode, udProjectGeometryType newType, udDouble3 *pPoints, int numPoints)
{
  //TODO: Optimise this
  udGeoZone zone;
  udGeoZone_SetFromSRID(&zone, 4326);

  return vcProject_UpdateNodeGeometryFromCartesian(pProject, pNode, zone, newType, pPoints, numPoints);
}

bool vcProject_FetchNodeGeometryAsCartesian(vcProject *pProject, udProjectNode *pNode, const udGeoZone &zone, udDouble3 **ppPoints, int *pNumPoints)
{
  if (pProject == nullptr || pNode == nullptr)
    return false;

  udDouble3 *pPoints = udAllocType(udDouble3, pNode->geomCount, udAF_Zero);

  if (pNumPoints != nullptr)
    *pNumPoints = pNode->geomCount;

  if (zone.srid != 0 && pProject->baseZone.srid != 0) // Geolocated
  {
    // Change all points from the projection
    for (int i = 0; i < pNode->geomCount; ++i)
      pPoints[i] = udGeoZone_TransformPoint(((udDouble3*)pNode->pCoordinates)[i], pProject->baseZone, zone);
  }
  else
  {
    for (int i = 0; i < pNode->geomCount; ++i)
      pPoints[i] = ((udDouble3*)pNode->pCoordinates)[i];
  }

  *ppPoints = pPoints;

  return true;
}

void vcProject_ExtractAttributionText(udProjectNode *pFolderNode, const char **ppCurrentText)
{
  //TODO: Cache attribution text; if nothing was added/removed from the scene then it doesn't need to be updated
  udProjectNode *pNode = pFolderNode->pFirstChild;

  while (pNode != nullptr)
  {
    if (pNode->pUserData != nullptr && pNode->itemtype == udProjectNodeType::udPNT_PointCloud)
    {
      vcModel *pModel = nullptr;
      const char *pAttributionText = nullptr;

      pModel = (vcModel *)pNode->pUserData;

      //Priority: Author -> License -> Copyright
      pAttributionText = pModel->m_metadata.Get("Author").AsString(pModel->m_metadata.Get("License").AsString(pModel->m_metadata.Get("Copyright").AsString()));
      if (pAttributionText)
      {
        if (*ppCurrentText == nullptr || udStrstr(*ppCurrentText, 0, pAttributionText) == nullptr)
        {
          if (*ppCurrentText != nullptr)
            udSprintf(ppCurrentText, "%s, %s", *ppCurrentText, pAttributionText);
          else
            udSprintf(ppCurrentText, "%s", pAttributionText);
        }
      }
    }

    if (pNode->itemtype == udPNT_Folder)
      vcProject_ExtractAttributionText(pNode, ppCurrentText);

    pNode = pNode->pNextSibling;
  }

  
}
