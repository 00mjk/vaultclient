#include "vcAddSphereFilter.h"

#include "vcState.h"
#include "vcStrings.h"
#include "vcQueryNode.h"

#include "udStringUtil.h"

#include "imgui.h"

vcAddSphereFilter vcAddSphereFilter::m_instance;

void vcAddSphereFilter::SceneUI(vcState *pProgramState)
{
  udProjectNode *pItem = pProgramState->sceneExplorer.clickedItem.pItem;
  if (pItem == nullptr)
  {
    ImGui::PushFont(pProgramState->pMidFont);
    ImGui::TextWrapped("%s", vcString::Get("toolFilterSphereStart"));
    ImGui::PopFont();
  }
  else if (pItem != nullptr && pItem->itemtype == udPNT_QueryFilter && pProgramState->sceneExplorer.clickedItem.pItem->pUserData != nullptr)
  {
    ImGui::PushFont(pProgramState->pMidFont);
    ImGui::TextWrapped("%s", vcString::Get("toolFilterSphereContinue"));
    ImGui::PopFont();

    ImGui::Separator();

    vcSceneItem *pSceneItem = (vcSceneItem *)pProgramState->sceneExplorer.selectedItems[0].pItem->pUserData;
    pSceneItem->HandleSceneEmbeddedUI(pProgramState);
  }
}

void vcAddSphereFilter::HandlePicking(vcState *pProgramState, vcRenderData & /*renderData*/, const vcRenderPickResult & /*pickResult*/)
{
  if (!pProgramState->pActiveViewport->isMouseOver)
    return;

  if (pProgramState->sceneExplorer.clickedItem.pItem != nullptr && pProgramState->sceneExplorer.clickedItem.pItem->itemtype == udPNT_QueryFilter)
  {
    vcQueryNode *pQueryNode = (vcQueryNode *)pProgramState->sceneExplorer.clickedItem.pItem->pUserData;
    pQueryNode->EndQuery(pProgramState, pProgramState->pActiveViewport->worldMousePosCartesian);
  }
  else
  {
    vcProject_ClearSelection(pProgramState, false);
    udProjectNode *pNode = nullptr;
    if (udProjectNode_Create(pProgramState->activeProject.pProject, &pNode, pProgramState->activeProject.pRoot, "QFilter", vcString::Get("sceneExplorerFilterSphereDefaultName"), nullptr, nullptr) == udE_Success)
    {
      vcProject_UpdateNodeGeometryFromCartesian(&pProgramState->activeProject, pNode, pProgramState->geozone, udPGT_Point, &pProgramState->pActiveViewport->worldMousePosCartesian, 1);
      udProjectNode_SetMetadataString(pNode, "shape", "sphere");
      udStrcpy(pProgramState->sceneExplorer.selectUUIDWhenPossible, pNode->UUID);
    }
  }
}

void vcAddSphereFilter::PreviewPicking(vcState *pProgramState, vcRenderData & /*renderData*/, const vcRenderPickResult & /*pickResult*/)
{
  if (!pProgramState->pActiveViewport->isMouseOver)
    return;

  udProjectNode *pItem = pProgramState->sceneExplorer.clickedItem.pItem;
  if (pItem != nullptr && pItem->itemtype == udPNT_QueryFilter)
  {
    vcSceneItem *pSceneItem = (vcSceneItem *)pProgramState->sceneExplorer.clickedItem.pItem->pUserData;
    if (!pSceneItem->m_visible)
      pProgramState->activeTool = vcActiveTool_Select;

    vcQueryNode *pTool = (vcQueryNode *)pProgramState->sceneExplorer.clickedItem.pItem->pUserData;
    pTool->EndQuery(pProgramState, pProgramState->pActiveViewport->worldMousePosCartesian, true);
  }
}
