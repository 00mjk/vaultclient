#ifndef vcOpenGL_h__
#define vcOpenGL_h__

#include "udPlatform.h"

#if UDPLATFORM_OSX
# include "OpenGL/gl3.h"
# include "OpenGL/gl3ext.h"
#elif UDPLATFORM_IOS || UDPLATFORM_IOS_SIMULATOR
# include "OpenGLES/ES3/gl.h"
# include "OpenGLES/ES3/glext.h"
#elif UDPLATFORM_LINUX
# define GL_GLEXT_PROTOTYPES 1
# include "GL/gl.h"
# include "GL/glu.h"
# include "GL/glext.h"
#elif UDPLATFORM_ANDROID
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>
#else
# include "GL/glew.h"
#endif

#ifndef VERIFY_GL
#ifdef _DEBUG
#define VERIFY_GL() if (int glError = glGetError()) { udDebugPrintf("GLError %d\n", glError); __debugbreak(); }
#else
#define VERIFY_GL()
#endif
#endif //VERIFY_GL

#include "vcGLState.h"
#include "vcTexture.h"
#include "vcMesh.h"

struct vcTexture
{
  vcTextureCreationFlags flags;
  vcTextureType type;

  GLuint id;

  vcTextureFormat format;
  int width, height, depth;

  vcTextureFilterMode filterMode;
  vcTextureWrapMode wrapMode;
  bool hasMipmaps;
  int aniFilter;

  // Optional if the texture is configured with the `vcTCF_AsynchronousRead` flag
  // Provides asynchronous transfer. Internally ping pongs between these between Begin()/End() calls
  uint32_t pboIndex;
  GLuint pbos[2];

  bool isTempClone;
};

struct vcFramebuffer
{
  GLuint id;

  int attachmentCount;
  vcTexture *attachments[2];
  vcTexture *pDepth; // optional
};

struct vcShaderConstantBuffer
{
  GLuint id;
  GLuint bindPoint;
  char name[32];
};

struct vcShaderSampler
{
  GLuint id;
};

struct vcShader
{
  GLuint programID;
  vcShaderConstantBuffer bufferObjects[16];
  int numBufferObjects;
  vcShaderSampler samplerIndexes[16];
  int numSamplerIndexes;

  vcShaderConstantBuffer *pCameraPlaneParams;
};

struct vcMesh
{
  GLuint vao;
  GLuint vbo;
  GLuint ibo;

  uint32_t vertexCount;
  uint32_t indexCount;

  GLenum drawType;
  GLenum indexType;
  int indexBytes;
  uint32_t vertexSize;
};

static const GLenum vcTTToGL[] = { GL_TEXTURE_2D, GL_TEXTURE_3D, GL_TEXTURE_2D_ARRAY }; // Texture Type
UDCOMPILEASSERT(udLengthOf(vcTTToGL) == vcTextureType_Count, "TextureTypes not equal size");

static const GLenum vcTFMToGL[] = { GL_NEAREST, GL_LINEAR }; // Filter Mode
UDCOMPILEASSERT(udLengthOf(vcTFMToGL) == vcTFM_Total, "TextureFilterModes not equal size");

static const GLenum vcTWMToGL[] = { GL_REPEAT, GL_CLAMP_TO_EDGE }; // Wrap Mode
UDCOMPILEASSERT(udLengthOf(vcTWMToGL) == vcTFM_Total, "TextureFilterModes not equal size");


#endif // vcOpenGL_h__
