#include "vcSceneItem.h"

#include "vcState.h"
#include "vcRender.h"

udProjectNode* vcSceneItem_CreateNodeInProject(udProject *pProject, const char *pType, const char *pName)
{
  udProjectNode *pNode = nullptr;
  udProjectNode *pRootNode = nullptr;

  udProject_GetProjectRoot(pProject, &pRootNode);

  udProjectNode_Create(pProject, &pNode, pRootNode, pType, pName, nullptr, nullptr);

  UDASSERT(pNode != nullptr, "Remove Path to this- memory alloc failed.");

  return pNode;
}

vcSceneItem::vcSceneItem(vcProject *pProject, udProjectNode *pNode, vcState * /*pProgramState*/) :
  m_pProject(pProject),
  m_loadStatus(0),
  m_pActiveWarningStatus(nullptr),
  m_visible(true),
  m_selected(false),
  m_expanded(false),
  m_editName(false),
  m_pName(nullptr),
  m_nameCapacity(0),
  m_lastUpdateTime(0.0),
  m_pPreferredProjection(nullptr)
{
  m_metadata.SetVoid();
  m_pNode = pNode;
}

vcSceneItem::vcSceneItem(vcState *pProgramState, const char *pType, const char *pName) :
  vcSceneItem(&pProgramState->activeProject, vcSceneItem_CreateNodeInProject(pProgramState->activeProject.pProject, pType, pName), pProgramState)
{
  // Do nothing
}

vcSceneItem::~vcSceneItem()
{
  m_metadata.Destroy();
  udFree(m_pPreferredProjection);
}

void vcSceneItem::HandleSceneEmbeddedUI(vcState * /*pProgramState*/)
{
  ImGui::Text("%s (%s)", m_pNode->pName, m_pNode->itemtypeStr);
}

void vcSceneItem::HandleContextMenu(vcState * /*pProgramState*/)
{
  // No additional items
}

void vcSceneItem::HandleAttachmentUI(vcState * /*pProgramState*/)
{
  // No additional items
}

void vcSceneItem::HandleToolUI(vcState * /*pProgramState*/)
{
  // No additional items
}

vcMenuBarButtonIcon vcSceneItem::GetSceneExplorerIcon()
{
  return vcMBBI_None;
}

void vcSceneItem::SetCameraPosition(vcState *pProgramState)
{
  for (int viewportIndex = 0; viewportIndex < pProgramState->settings.activeViewportCount; ++viewportIndex)
    pProgramState->pViewports[viewportIndex].camera.position = GetWorldSpacePivot();
}

udDouble3 vcSceneItem::GetWorldSpacePivot()
{
  return (this->GetWorldSpaceMatrix() * udDouble4::create(this->GetLocalSpacePivot(), 1.0)).toVector3();
}

udDouble3 vcSceneItem::GetLocalSpacePivot()
{
  return udDouble3::zero(); //TODO: Consider somehow defaulting to Camera Position instead
}

udDouble4x4 vcSceneItem::GetWorldSpaceMatrix()
{
  return udDouble4x4::identity();
}

void vcSceneItem::UpdateNode(vcState *pProgramState)
{
  this->OnNodeUpdate(pProgramState);
  m_lastUpdateTime = m_pNode->lastUpdate;
};

void vcSceneItem::SelectSubitem(uint64_t internalId)
{
  // Does nothing
  udUnused(internalId);
}

bool vcSceneItem::IsSubitemSelected(uint64_t internalId)
{
  udUnused(internalId);
  return m_selected;
}

vcGizmoAllowedControls vcSceneItem::GetAllowedControls()
{
  return vcGAC_All;
}
