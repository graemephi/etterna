#include "Etterna/Globals/global.h"
#include "Display_VK.h"

#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#else
#error todo
#endif

#define VOLK_IMPLEMENTATION
#include "volk.h"
#include "vulkan/vulkan.h"

struct VulkanState
{
	VkInstance instance;
} g_vk;

auto
Display_VK::Init(const VideoModeParams& p, bool bAllowUnacceleratedRenderer)
  -> std::string
{
	VkResult rc = volkInitialize();

	if (rc != VK_SUCCESS) {
		return ssprintf("volkInitialize (0x%x)", rc);
	}

	// Dunno what a good version to target is. Gonna stick 1.1 in here for now,
	// 1.2 is quite new.
	VkApplicationInfo applicationInfo = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
	applicationInfo.apiVersion = VK_API_VERSION_1_1;

	VkInstanceCreateInfo createInfo = {
		VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO
	};
	createInfo.pApplicationInfo = &applicationInfo;

	rc = vkCreateInstance(&createInfo, 0, &g_vk.instance);

	if (rc != VK_SUCCESS) {
		return ssprintf("vkCreateInstance (0x%x)", rc);
	}

	return std::string{};
}

auto
Display_VK::GetPixelFormatDesc(RagePixelFormat pf) const
  -> const RagePixelFormatDesc*
{
	return 0;
}

void
Display_VK::GetDisplaySpecs(DisplaySpecs& out) const
{
}

auto
Display_VK::GetActualVideoModeParams() const -> const ActualVideoModeParams*
{
	return 0;
}

void
Display_VK::ResolutionChanged()
{
}

auto
Display_VK::TryVideoMode(const VideoModeParams& _p, bool& bNewDeviceOut)
  -> std::string
{
	return "";
}

auto
Display_VK::SupportsTextureFormat(RagePixelFormat pixfmt, bool realtime) -> bool
{
	return false;
}

auto
Display_VK::GetMaxTextureSize() const -> int
{
	return 0;
}

auto
Display_VK::CreateTexture(RagePixelFormat pixfmt,
						  RageSurface* img,
						  bool bGenerateMipMaps) -> intptr_t
{
	return 0;
}

void
Display_VK::DeleteTexture(intptr_t iTexHandle)
{
}

void
Display_VK::UpdateTexture(intptr_t uTexHandle,
						  RageSurface* img,
						  int xoffset,
						  int yoffset,
						  int width,
						  int height)
{
}

auto
Display_VK::BeginFrame() -> bool
{
	return false;
}

void
Display_VK::EndFrame()
{
}

void
Display_VK::ClearZBuffer()
{
}

void
Display_VK::SetAlphaTest(bool b)
{
}

void
Display_VK::SetBlendMode(BlendMode mode)
{
}

void
Display_VK::SetCullMode(CullMode mode)
{
}

void
Display_VK::SetZBias(float f)
{
}

void
Display_VK::SetZWrite(bool b)
{
}

void
Display_VK::SetZTestMode(ZTestMode mode)
{
}

void
Display_VK::SetTexture(TextureUnit tu, intptr_t iTexture)
{
}

void
Display_VK::SetTextureMode(TextureUnit tu, TextureMode tm)
{
}

void
Display_VK::SetTextureFiltering(TextureUnit tu, bool b)
{
}

void
Display_VK::SetTextureWrapping(TextureUnit tu, bool b)
{
}

void
Display_VK::DrawQuadsInternal(const RageSpriteVertex v[], int iNumVerts)
{
	FAIL_M("Not implemented");
}

void
Display_VK::DrawQuadStripInternal(const RageSpriteVertex v[], int iNumVerts)
{
	FAIL_M("Not implemented");
}

void
Display_VK::DrawSymmetricQuadStripInternal(const RageSpriteVertex v[],
										   int iNumVerts)
{
	FAIL_M("Not implemented");
}

void
Display_VK::DrawFanInternal(const RageSpriteVertex v[], int iNumVerts)
{
	FAIL_M("Not implemented");
}

void
Display_VK::DrawStripInternal(const RageSpriteVertex v[], int iNumVerts)
{
	FAIL_M("Not implemented");
}

void
Display_VK::DrawTrianglesInternal(const RageSpriteVertex v[], int iNumVerts)
{
	FAIL_M("Not implemented");
}

auto
Display_VK::IsZWriteEnabled() const -> bool
{
	FAIL_M("Never called?");
}

auto
Display_VK::IsZTestEnabled() const -> bool
{
	FAIL_M("Never called?");
}

void
Display_VK::PushQuads(RenderQuad q[], size_t numQuads)
{
}

auto
Display_VK::SupportsThreadedRendering() -> bool
{
	return false;
}

auto
Display_VK::CreateScreenshot() -> RageSurface*
{
	return 0;
}

///////////////////////////////////////////////////////////////////////////////
// Should probably be supported or the functionality removed from the rest of
// the code

auto
Display_VK::CreateRenderTarget(const RenderTargetParam& param,
							   int& iTextureWidthOut,
							   int& iTextureHeightOut) -> intptr_t
{
	FAIL_M("Never called?");
}

auto
Display_VK::GetRenderTarget() -> intptr_t
{
	FAIL_M("Never called?");
}

void
Display_VK::SetRenderTarget(intptr_t uTexHandle, bool bPreserveTexture)
{
	FAIL_M("Never called?");
}

auto
Display_VK::CreateCompiledGeometry() -> RageCompiledGeometry*
{
	FAIL_M("Never called?");
	return 0;
}

void
Display_VK::DeleteCompiledGeometry(RageCompiledGeometry* p)
{
	FAIL_M("Never called?");
}

void
Display_VK::DrawCompiledGeometryInternal(const RageCompiledGeometry* p,
										 int iMeshIndex)
{
	FAIL_M("Never called?");
}

void
Display_VK::ClearAllTextures()
{
	FAIL_M("Never called?");
}

auto
Display_VK::GetNumTextureUnits() -> int
{
	FAIL_M("Never called?");
}

///////////////////////////////////////////////////////////////////////////////
// Will never be supported, only make sense for ancient APIs

void
Display_VK::SetMaterial(const RageColor& emissive,
						const RageColor& ambient,
						const RageColor& diffuse,
						const RageColor& specular,
						float shininess)
{
	FAIL_M("Never called?");
}

void
Display_VK::SetLighting(bool b)
{
	FAIL_M("Never called?");
}

void
Display_VK::SetLightOff(int index)
{
	FAIL_M("Never called?");
}

void
Display_VK::SetLightDirectional(int index,
								const RageColor& ambient,
								const RageColor& diffuse,
								const RageColor& specular,
								const RageVector3& dir)
{
	FAIL_M("Never called?");
}

void
Display_VK::SetSphereEnvironmentMapping(TextureUnit tu, bool b)
{
	FAIL_M("Never called?");
}

void
Display_VK::SetCelShaded(int stage)
{
	FAIL_M("Never called?");
}

auto
Display_VK::IsD3DInternal() -> bool
{
	FAIL_M("Never called?");
}
