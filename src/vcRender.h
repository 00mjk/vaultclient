#ifndef vcRender_h__
#define vcRender_h__

#include "vcState.h"
#include "vcModel.h"

#include "vdkRenderContext.h"
#include "vdkRenderView.h"

#include "gl/vcMesh.h"
#include "rendering/vcFenceRenderer.h"
#include "rendering/vcLabelRenderer.h"
#include "rendering/vcWaterRenderer.h"
#include "rendering/vcImageRenderer.h"
#include "rendering/vcCompass.h"
#include "rendering/vcPolygonModel.h"

struct vcRenderContext;
struct vcTexture;

struct vcRenderPolyInstance
{
  vcPolygonModel *pModel;
  udDouble4x4 worldMat; // will be converted to eye internally
};

struct vcRenderData
{
  vcGISSpace *pGISSpace;

  double deltaTime;

  udInt2 mouse;
  udDouble3 worldMousePos;

  udDouble3 *pWorldAnchorPos; // If this is not nullptr, this is the point to highlight
  bool pickingSuccess;
  vcTexture *pWatermarkTexture;

  udChunkedArray<vcModel*> models;
  udChunkedArray<vcFenceRenderer*> fences;
  udChunkedArray<vcLabelInfo*> labels;
  udChunkedArray<vcRenderPolyInstance> polyModels;
  udChunkedArray<vcWaterRenderer*> waterVolumes;
  udChunkedArray<vcImageRenderInfo*> images;

  vcCamera *pCamera;
  vcCameraSettings *pCameraSettings;
};

udResult vcRender_Init(vcRenderContext **ppRenderContext, vcSettings *pSettings, vcCamera *pCamera, const udUInt2 &windowResolution);
udResult vcRender_Destroy(vcRenderContext **pRenderContext);

udResult vcRender_SetVaultContext(vcRenderContext *pRenderContext, vdkContext *pVaultContext);

udResult vcRender_ResizeScene(vcRenderContext *pRenderContext, const uint32_t width, const uint32_t height);

vcTexture* vcRender_GetSceneTexture(vcRenderContext *pRenderContext);
void vcRender_RenderScene(vcRenderContext *pRenderContext, vcRenderData &renderData, vcFramebuffer *pDefaultFramebuffer);
void vcRender_vcRenderSceneImGui(vcRenderContext *pRenderContext, const vcRenderData &renderData);

void vcRender_ClearTiles(vcRenderContext *pRenderContext);
void vcRender_ClearPoints(vcRenderContext *pRenderContext);

#endif//vcRender_h__
