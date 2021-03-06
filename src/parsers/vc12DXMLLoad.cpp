#include "vc12DXML.h"
#include "vc12dxmlCommon.h"
#include "udStringUtil.h"
#include "udFile.h"

/*
*  Notes:
*  I am unsure if commands such as <null></null> and <colour></colour> can be placed anywhere in the xml
*  or must be called from inside a string. The spec is difficult to interpret. I have opted for the
*  following rule regarding commands
*     - Commands (null and colour at least) can only be called from within a string and should
*       only be called once. Position is irrelevant. The command will set the global value,
*       and will affect all subsequent strings.
*       WARNING: If udJSON does not preserve order when building arrays of objects, the spec will
*                not be obeyed!
*/

#define LOG_ERROR(...) do {}while(false)
#define LOG_WARNING(...) do {}while(false)
#define LOG_INFO(...) do {}while(false)

//----------------------------------------------------------------------------
// Unicode format detection. 
//----------------------------------------------------------------------------

#define MAX_BOM_BYTES 8

enum vcUnicodeEncoding
{
  vcUE_ANSI,
  vcUE_UTF8,
  vcUE_UTF8BOM,
  vcUE_UCS2BEBOM,
  vcUE_UCS2LEBOM,
  vcUE_UTF16BEBOM,
  vcUE_UTF16LEBOM,
  vcUE_Unknown
};

struct BOMBytes
{
  uint32_t nBytes;
  uint8_t bytes[MAX_BOM_BYTES];
};

static vcUnicodeEncoding vcUnicode_GetEncodingFromBOM(const BOMBytes &bom)
{
  static uint8_t UTF8BOM[3] = {0xEF, 0xBB, 0xBF};
  static uint8_t UTF16LEBOM[2] = {0xFF, 0xFE};
  static uint8_t UTF16BEBOM[2] = {0xFE, 0xFF};

  if (bom.nBytes >= 3 && memcmp(bom.bytes, UTF8BOM, 3) == 0)
    return vcUE_UTF8BOM;

  if (bom.nBytes >= 2 && memcmp(bom.bytes, UTF16LEBOM, 2) == 0)
    return vcUE_UTF16LEBOM;

  if (bom.nBytes >= 2 && memcmp(bom.bytes, UTF16BEBOM, 2) == 0)
    return vcUE_UTF16BEBOM;

  // Treat USC2 as UTF16 as UCS2 is a subset of UTF16 with identical BOMs

  return vcUE_Unknown;
}

udResult vcUnicode_DetermineFileEncoding(const char *pFilePath, vcUnicodeEncoding *pEncoding)
{
  udResult result;
  udFile *pFile = nullptr;
  int64_t fileLength = -1;
  size_t bytesRead = 0;
  BOMBytes bom = {};

  UD_ERROR_NULL(pFilePath, udR_InvalidParameter_);
  UD_ERROR_NULL(pEncoding, udR_InvalidParameter_);

  UD_ERROR_CHECK(udFile_Open(&pFile, pFilePath, udFOF_Read, &fileLength));
  UD_ERROR_CHECK(udFile_Read(pFile, bom.bytes, MAX_BOM_BYTES, 0, udFSW_SeekCur, &bytesRead));

  bom.nBytes = uint32_t(bytesRead);

  *pEncoding = vcUE_Unknown;

  do
  {
    *pEncoding = vcUnicode_GetEncodingFromBOM(bom);
    if (*pEncoding != vcUE_Unknown)
      break;

    // TODO From here we can try to probe the file to determine the encoding.

  } while (false);

  result = udR_Success;
epilogue:
  return result;
}

//----------------------------------------------------------------------------
// 12dxml file parsing
//----------------------------------------------------------------------------
static bool vc12DXML_IsGreyColour(const char *pStr, uint32_t *pOut)
{
  if (pStr == nullptr || pOut == nullptr)
    return false;

  bool success = false;

  if (udStrBeginsWithi(pStr, "grey "))
  {
    uint32_t scale = udStrAtou(pStr + 5); // Skip "grey "
    if (scale > 255)
      scale = 255;
    *pOut = 0xFF000000;
    *pOut |= (scale << 16);
    *pOut |= (scale << 8);
    *pOut |= (scale << 0);
    success = true;
  }
  return success;
}

static uint32_t vc12DXML_ToColour(const char *pStr)
{
  struct ColourPair
  {
    const char *pStr;
    uint32_t val;
  };

  static const ColourPair Colours[] =
  {
    // Standard colours
    {"red",     0xFFFF0000},
    {"green",   0xFF00FF00},
    {"blue",    0xFF0000FF},
    {"yellow",  0xFFFFFF00},
    {"cyan",    0xFF00FFFF},
    {"magenta", 0xFFFF00FF},
    {"black",   0xFF000000},
    {"white",   0xFFFFFFFF},
    {"orange",  0xFFFF9D00},
    {"purple",  0xFFB300FF},

    // Non standard colours
    {"brown",       0xFF964B00},
    {"grey",        0xFF7D7D7D},
    {"light red",   0xFFFF6464},
    {"light green", 0xFF64FF64},
    {"light blue",  0xFF6464FF},
    {"dark red",    0xFFBF0000},
    {"dark green",  0xFF00BF00},
    {"dark blue",   0xFF0000BF},
    {"dark grey",   0xFF646464},
    {"light grey",  0xFF5E5E5E},
    {"off yellow",  0xFFFAF3DC},
  };

  for (size_t i = 0; i < udLengthOf(Colours); ++i)
  {
    if (udStrcmpi(pStr, Colours[i].pStr) == 0)
      return Colours[i].val;
  }

  uint32_t grey = 0;
  if (vc12DXML_IsGreyColour(pStr, &grey))
    return grey;

  return 0xFFFF9D00;
}

static void SetGlobals(udJSON const *pNode, vc12DXML_ProjectGlobals *pGlobals)
{
  if (pNode == nullptr || pGlobals == nullptr)
    return;

  const udJSON *pItem = nullptr;

  pItem = &pNode->Get("null");
  if (!pItem->IsVoid())
    pGlobals->nullValue = pItem->AsDouble();

  pItem = &pNode->Get("colour");
  if (!pItem->IsVoid())
    pGlobals->colour = vc12DXML_ToColour(pItem->AsString());
}

static void vc12DXML_Ammend_data_3d(udJSON const *pNode, std::vector<udDouble3> *pPointList, const vc12DXML_ProjectGlobals *pGlobals)
{
  if (pNode == nullptr || pPointList == nullptr || pGlobals == nullptr)
    return;

  if (pNode->IsArray())
  {
    for (size_t i = 0; i < pNode->AsArray()->length; ++i)
      vc12DXML_Ammend_data_3d(pNode->AsArray()->GetElement(i), pPointList, pGlobals);
  }
  else
  {
    char str[128] = {};
    char *tokens[3] = {};
    udDouble3 point = {};
    udStrcpy(str, pNode->AsString());
    if (udStrTokenSplit(str, " ", tokens, 3) != 3)
    {
      LOG_WARNING("Malformed data_3d vertex item");
    }
    else
    {
      for (int p = 0; p < 3; ++p)
      {
        if (udStrcmp(tokens[p], "null") == 0)
          point[p] = pGlobals->nullValue;
        else
          point[p] = udStrAtof64(tokens[p]);
      }
      pPointList->push_back(point);
    }
  }
}

static void vc12DXML_Ammend_data_2d(udJSON const *pNode, std::vector<udDouble3> *pPointList, double z, const vc12DXML_ProjectGlobals *pGlobals)
{
  if (pNode == nullptr || pPointList == nullptr || pGlobals == nullptr)
    return;

  if (pNode->IsArray())
  {
    for (size_t i = 0; i < pNode->AsArray()->length; ++i)
      vc12DXML_Ammend_data_2d(pNode->AsArray()->GetElement(i), pPointList, z, pGlobals);
  }
  else
  {
    char str[128] = {};
    char *tokens[2] = {};
    udDouble3 point = {};
    point[2] = z;
    udStrcpy(str, pNode->AsString());
    if (udStrTokenSplit(str, " ", tokens, 2) != 2)
    {
      LOG_WARNING("Malformed data_2d vertex item");
    }
    else
    {
      for (int p = 0; p < 2; ++p)
      {
        if (udStrcmp(tokens[p], "null") == 0)
          point[p] = pGlobals->nullValue;
        else
          point[p] = udStrAtof64(tokens[p]);
      }
      pPointList->push_back(point);
    }
  }
}

vc12DXML_ProjectGlobals::vc12DXML_ProjectGlobals()
  : nullValue(-999.0)   // Default defined in the spec
  , colour(0xFFFF0000)  // red, default defined in the spec
{

}

vc12DXML_Item::~vc12DXML_Item()
{

}

vc12DXML_SuperString::vc12DXML_SuperString()
  : m_pName(nullptr)
  , m_isClosed(false)
  , m_weight(1.0)
  , m_colour(0xFFFF00FF)
{

}

vc12DXML_SuperString::~vc12DXML_SuperString()
{
  udFree(m_pName);
}

udResult vc12DXML_SuperString::Build(udJSON const *pNode, vc12DXML_ProjectGlobals *pGlobals)
{
  udResult result;
  const udJSON *pItem = nullptr;
  bool has_z = false;
  double z = 0.0;

  UD_ERROR_IF(pNode == nullptr, udR_InvalidParameter_);
  UD_ERROR_IF(pGlobals == nullptr, udR_InvalidParameter_);

  pItem = &pNode->Get("z");

  if (pItem->IsArray())
  {
    LOG_ERROR("More than one <z> tags in string");
    UD_ERROR_SET(udR_InvalidConfiguration); // Fail as we don't know which <z> to take.
  }
  
  if (!pItem->IsVoid())
  {
    has_z = true;
    z = pItem->AsDouble();
  }

  SetGlobals(pNode, pGlobals);

  pItem = &pNode->Get("name");
  if (!pItem->IsVoid())
  {
    udFree(m_pName);
    m_pName = udStrdup(pItem->AsString());
  }

  pItem = &pNode->Get("closed");
  if (!pItem->IsVoid())
    m_isClosed = pItem->AsBool();

  pItem = &pNode->Get("colour");
  if (!pItem->IsVoid())
    m_colour = vc12DXML_ToColour(pItem->AsString());
  else
    m_colour = pGlobals->colour;

  pItem = &pNode->Get("weight");
  if (!pItem->IsVoid())
    m_weight = pItem->AsDouble();

  pItem = &pNode->Get("data_3d");
  if (!pItem->IsVoid())
    vc12DXML_Ammend_data_3d(&pItem->Get("p"), &m_points, pGlobals);
  
  pItem = &pNode->Get("data_2d");
  if (!pItem->IsVoid())
  {
    if (!has_z)
      LOG_WARNING("data_2d present but <z> tag was not found");
    else
      vc12DXML_Ammend_data_2d(&pItem->Get("p"), &m_points, z, pGlobals);
  }

  result = udR_Success;
epilogue:
  return result;
}

vc12DXML_Model::vc12DXML_Model()
  : m_pName(nullptr)
{

}

vc12DXML_Model::~vc12DXML_Model()
{
  for (auto pItem : m_elements)
    delete pItem;

  udFree(m_pName);
}

udResult vc12DXML_Model::BuildChildren(udJSON const *pNode, vc12DXML_ProjectGlobals *pGlobals)
{
  udResult result;
  const udJSON *pItem = nullptr;

  UD_ERROR_IF(pNode == nullptr, udR_InvalidParameter_);
  UD_ERROR_IF(pGlobals == nullptr, udR_InvalidParameter_);

  pItem = &pNode->Get("string_super");
  if (!pItem->IsVoid())
  {
    if (pItem->IsArray())
    {
      for (size_t i = 0; i < pItem->ArrayLength(); ++i)
      {
        vc12DXML_SuperString *pSS = new vc12DXML_SuperString();
        udResult res = pSS->Build(pItem->AsArray()->GetElement(i), pGlobals);
        if (res == udR_Success)
          m_elements.push_back(pSS);
        else
          LOG_ERROR("Failed to load super string.");
      }
    }
    else
    {
      vc12DXML_SuperString *pSS = new vc12DXML_SuperString();
      udResult res = pSS->Build(pItem, pGlobals);
      if (res == udR_Success)
        m_elements.push_back(pSS);
      else
        LOG_ERROR("Failed to load super string.");
    }
  }

  result = udR_Success;
epilogue:
  return result;
}

udResult vc12DXML_Model::Build(udJSON const *pNode, vc12DXML_ProjectGlobals *pGlobals)
{
  udResult result;
  const udJSON *pItem = nullptr;

  UD_ERROR_IF(pNode == nullptr, udR_InvalidParameter_);
  UD_ERROR_IF(pGlobals == nullptr, udR_InvalidParameter_);

  // As per the spec, 'name' MUST be defined for models. But we don't check the format, just assume is correct.
  pItem = &pNode->Get("name");
  if (!pItem->IsVoid())
  {
    udFree(m_pName);
    m_pName = udStrdup(pItem->AsString());
  }

  pItem = &pNode->Get("children");
  if (!pItem->IsVoid())
    UD_ERROR_CHECK(BuildChildren(pItem, pGlobals));

  result = udR_Success;
epilogue:
  return result;
};

vc12DXML_Project::vc12DXML_Project()
  : m_pName(nullptr)
{

}

vc12DXML_Project::~vc12DXML_Project()
{
  for (auto pItem : m_models)
    delete pItem;

  udFree(m_pName);
}

void vc12DXML_Project::SetName(const char *pName)
{
  udFree(m_pName);
  m_pName = udStrdup(pName);
}

udResult vc12DXML_Project::AddProject(udJSON const *pNode, vc12DXML_ProjectGlobals *pGlobals)
{
  udResult result;
  std::vector<const udJSON *> modelNodes;
  const udJSON *pItem = nullptr;

  UD_ERROR_IF(pNode == nullptr, udR_InvalidParameter_);
  UD_ERROR_IF(pGlobals == nullptr, udR_InvalidParameter_);

  // Models
  pItem = &pNode->Get("model");
  if (!pItem->IsVoid())
    modelNodes.push_back(pItem);

  pItem = &pNode->Get("models");
  if (!pItem->IsVoid())
  {
    if (pItem->IsArray())
    {
      for (size_t i = 0; i < pItem->ArrayLength(); ++i)
      {
        const udJSON *pTempNode = &pItem->AsArray()->GetElement(i)->Get("model");
        if (!pTempNode->IsVoid())
          modelNodes.push_back(pTempNode);
      }
    }
    else
    {
      const udJSON *pTempNode = &pItem->Get("model");
      if (!pTempNode->IsVoid())
        modelNodes.push_back(pTempNode);
    }
  }

  for (auto pModelNode : modelNodes)
  {
    if (pModelNode->IsArray())
    {
      for (size_t i = 0; i < pModelNode->ArrayLength(); ++i)
      {
        vc12DXML_Model *pModel = new vc12DXML_Model();
        udResult res = pModel->Build(pModelNode->AsArray()->GetElement(i), pGlobals);
        if (res == udR_Success)
          m_models.push_back(pModel);
        else
          LOG_ERROR("Failed to load model.");
      }
    }
    else
    {
      vc12DXML_Model *pModel = new vc12DXML_Model();
      udResult res = pModel->Build(pModelNode, pGlobals);
      if (res == udR_Success)
        m_models.push_back(pModel);
      else
        LOG_ERROR("Failed to load model.");
    }
  }

  result = udR_Success;
epilogue:
  return result;
}

bool vc12DXML_Project::TryLoad(udJSON const *pNode, vc12DXML_ProjectGlobals *pGlobals)
{
  if (pNode == nullptr || pGlobals == nullptr)
    return false;

  bool result = false;
  if (!pNode->IsVoid())
  {
    result = true;

    // We could support multiple projects per file, but then should we support projects within projects?
    // For now just extract all models and elements and dump them into one project.
    // Perhaps we should replicate the xml structure in udStream as folders?
    if (pNode->IsArray())
    {
      for (size_t i = 0; i < pNode->ArrayLength(); ++i)
      {
        if (AddProject(pNode->AsArray()->GetElement(i), pGlobals) != udR_Success)
          LOG_WARNING("Failed to load a project.");
      }
    }
    else
    {
      if (AddProject(pNode, pGlobals) != udR_Success)
        LOG_WARNING("Failed to load project.");
    }
  }

  return result;
}

udResult vc12DXML_Project::Build(udJSON const *pNode, vc12DXML_ProjectGlobals *pGlobals)
{
  udResult result;

  UD_ERROR_IF(pNode == nullptr, udR_InvalidParameter_);

  // Try to find the root node...
  do
  {
    if (TryLoad(&pNode->Get("xml12d.project"), pGlobals))
      break;

    if (TryLoad(&pNode->Get("xml12d"), pGlobals))
      break;

    LOG_WARNING("Failed to load any projects.");

  } while (false);

  result = udR_Success;
epilogue:
  return result;
}

udResult vc12DXML_Project::BeginBuild(udJSON const *pNode)
{
  return Build(pNode, &m_globals);
}

udResult vc12DXML_LoadProject(vc12DXML_Project &project, const char *pFilePath)
{
  udResult result;
  udJSON xml;
  const char *pFileContents = nullptr;
  udJSON models;
  udFilename fileName;
  char projectName[256] = {};

  vcUnicodeEncoding encoding;

  UD_ERROR_CHECK(vcUnicode_DetermineFileEncoding(pFilePath, &encoding));
  UD_ERROR_IF(encoding == vcUE_UCS2LEBOM, udR_Unsupported);
  UD_ERROR_IF(encoding == vcUE_UCS2BEBOM, udR_Unsupported);
  UD_ERROR_IF(encoding == vcUE_UTF16LEBOM, udR_Unsupported);
  UD_ERROR_IF(encoding == vcUE_UTF16BEBOM, udR_Unsupported);

  UD_ERROR_NULL(pFilePath, udR_InvalidParameter_);

  fileName.SetFromFullPath("%s", pFilePath);
  fileName.ExtractFilenameOnly(projectName, 256);
  project.SetName(projectName);

  UD_ERROR_CHECK(udFile_Load(pFilePath, (void **)&pFileContents));

  if (encoding == vcUE_UTF8BOM)
    UD_ERROR_CHECK(xml.Parse(pFileContents + 3));
  else // Try to load as UTF8 or ANSI. Will fail here if anything else
    UD_ERROR_CHECK(xml.Parse(pFileContents));

  UD_ERROR_CHECK(project.BeginBuild(&xml));

epilogue:
  udFree(pFileContents);
  return result;
}

udResult vc12DXML_Load(vcState *pProgramState, const char *pFilename)
{
  udResult result;
  vc12DXML_Project project;

  UD_ERROR_CHECK(vc12DXML_LoadProject(project, pFilename));
  if (result == udR_Success)
  {
    udProjectNode *pParentNode = pProgramState->sceneExplorer.clickedItem.pItem != nullptr ? pProgramState->sceneExplorer.clickedItem.pItem : pProgramState->activeProject.pRoot;
    project.AddToProject(pProgramState, pParentNode);
  }

  result = udR_Success;
epilogue:
  return result;
}
