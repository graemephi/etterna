#include "LegacyDisplay.h"

auto
LegacyDisplay::BeginFrame()
	-> bool
{
	return m_driver->BeginFrame();
}

void
LegacyDisplay::EndFrame()
{
	return m_driver->EndFrame();
}

void
LegacyDisplay::SetAlphaTest(bool b)
{
	return m_driver->SetAlphaTest(b);
}

void
LegacyDisplay::SetBlendMode(BlendMode mode)
{
	return m_driver->SetBlendMode(mode);
}

void
LegacyDisplay::SetCullMode(CullMode mode)
{
	return m_driver->SetCullMode(mode);
}

auto
LegacyDisplay::IsZWriteEnabled() const
	-> bool
{
	return m_driver->IsZWriteEnabled();
}

void
LegacyDisplay::SetZBias(float f)
{
	return m_driver->SetZBias(f);
}

auto
LegacyDisplay::IsZTestEnabled() const
	-> bool
{
	return m_driver->IsZTestEnabled();
}

void
LegacyDisplay::SetZWrite(bool b)
{
	return m_driver->SetZWrite(b);
}

void
LegacyDisplay::SetZTestMode(ZTestMode mode)
{
	return m_driver->SetZTestMode(mode);
}

void
LegacyDisplay::ClearZBuffer()
{
	return m_driver->ClearZBuffer();
}

void
LegacyDisplay::SetTexture(TextureUnit tu, intptr_t iTexture)
{
	return m_driver->SetTexture(tu, iTexture);
}

void
LegacyDisplay::SetTextureMode(TextureUnit tu, TextureMode tm)
{
	return m_driver->SetTextureMode(tu, tm);
}

void
LegacyDisplay::SetTextureFiltering(TextureUnit tu, bool b)
{
	return m_driver->SetTextureFiltering(tu, b);
}

void
LegacyDisplay::SetTextureWrapping(TextureUnit tu, bool b)
{
	return m_driver->SetTextureWrapping(tu, b);
}

void
LegacyDisplay::DrawQuadsInternal(const RageSpriteVertex v[], int iNumVerts)
{
	return m_driver->DrawQuadsInternal(v, iNumVerts);
}

void
LegacyDisplay::DrawQuadStripInternal(const RageSpriteVertex v[], int iNumVerts)
{
	return m_driver->DrawQuadStripInternal(v, iNumVerts);
}

void
LegacyDisplay::DrawSymmetricQuadStripInternal(const RageSpriteVertex v[], int iNumVerts)
{
	return m_driver->DrawSymmetricQuadStripInternal(v, iNumVerts);
}

void
LegacyDisplay::DrawFanInternal(const RageSpriteVertex v[], int iNumVerts)
{
	return m_driver->DrawFanInternal(v, iNumVerts);
}

void
LegacyDisplay::DrawStripInternal(const RageSpriteVertex v[], int iNumVerts)
{
	return m_driver->DrawStripInternal(v, iNumVerts);
}

void
LegacyDisplay::DrawTrianglesInternal(const RageSpriteVertex v[], int iNumVerts)
{
	return m_driver->DrawTrianglesInternal(v, iNumVerts);
}

void
LegacyDisplay::DrawCompiledGeometryInternal(const RageCompiledGeometry* p, int iMeshIndex)
{
	return m_driver->DrawCompiledGeometryInternal(p, iMeshIndex);
}

// All functions below are forwarded to the underlying driver immediately.

auto LegacyDisplay::GetPixelFormatDesc(RagePixelFormat pf) const
	-> const RagePixelFormatDesc*
{
	return m_driver->GetPixelFormatDesc(pf);
}

auto
LegacyDisplay::Init(const VideoModeParams& p, bool bAllowUnacceleratedRenderer)
	-> std::string
{
	return m_driver->Init(p, bAllowUnacceleratedRenderer);
}

void
LegacyDisplay::GetDisplaySpecs(DisplaySpecs& out) const
{
	return m_driver->GetDisplaySpecs(out);
}

auto
LegacyDisplay::TryVideoMode(const VideoModeParams& _p, bool& bNewDeviceOut)
  -> std::string
{
	return m_driver->TryVideoMode(_p, bNewDeviceOut);
}

void
LegacyDisplay::ResolutionChanged()
{
	return m_driver->ResolutionChanged();
}

auto
LegacyDisplay::GetMaxTextureSize() const
	-> int
{
	return m_driver->GetMaxTextureSize();
}

auto
LegacyDisplay::SupportsTextureFormat(RagePixelFormat pixfmt, bool realtime)
	-> bool
{
	return m_driver->SupportsTextureFormat(pixfmt, realtime);
}

auto
LegacyDisplay::SupportsThreadedRendering()
	-> bool
{
	return m_driver->SupportsThreadedRendering();
}

auto
LegacyDisplay::CreateScreenshot()
	-> RageSurface*
{
	return m_driver->CreateScreenshot();
}

auto
LegacyDisplay::GetActualVideoModeParams() const
	-> const ActualVideoModeParams*
{
	return m_driver->GetActualVideoModeParams();
}

auto
LegacyDisplay::CreateCompiledGeometry()
	-> RageCompiledGeometry*
{
	return m_driver->CreateCompiledGeometry();
}

void
LegacyDisplay::DeleteCompiledGeometry(RageCompiledGeometry* p)
{
	return m_driver->DeleteCompiledGeometry(p);
}

void
LegacyDisplay::ClearAllTextures()
{
	return m_driver->ClearAllTextures();
}

auto
LegacyDisplay::GetNumTextureUnits()
	-> int
{
	return m_driver->GetNumTextureUnits();
}

void
LegacyDisplay::SetMaterial(const RageColor& emissive, const RageColor& ambient, const RageColor& diffuse, const RageColor& specular, float shininess)
{
}

void
LegacyDisplay::SetLighting(bool b)
{
	return m_driver->SetLighting(b);
}

void
LegacyDisplay::SetLightOff(int index)
{
	return m_driver->SetLightOff(index);
}

void
LegacyDisplay::SetLightDirectional(int index, const RageColor& ambient, const RageColor& diffuse, const RageColor& specular, const RageVector3& dir)
{
}

void
LegacyDisplay::DeleteTexture(intptr_t iTexHandle)
{
	return m_driver->DeleteTexture(iTexHandle);
}

auto
LegacyDisplay::CreateTexture(RagePixelFormat pixfmt, RageSurface* img, bool bGenerateMipMaps)
	-> intptr_t
{
	return m_driver->CreateTexture(pixfmt, img, bGenerateMipMaps);

}

void
LegacyDisplay::UpdateTexture(intptr_t uTexHandle, RageSurface* img, int xoffset, int yoffset, int width, int height)
{
	return m_driver->UpdateTexture(uTexHandle, img, xoffset, yoffset, width, height);
}

auto
LegacyDisplay::GetOrthoMatrix(float l, float r, float b, float t, float zn, float zf)
	-> RageMatrix
{
	return m_driver->GetOrthoMatrix(l, r, b, t, zn, zf);
}

auto
LegacyDisplay::CreateRenderTarget(const RenderTargetParam& param, int& iTextureWidthOut, int& iTextureHeightOut)
	-> intptr_t
{
	return m_driver->CreateRenderTarget(param, iTextureWidthOut, iTextureHeightOut);
}

auto
LegacyDisplay::GetRenderTarget()
	-> intptr_t
{
	return m_driver->GetRenderTarget();
}

void
LegacyDisplay::SetRenderTarget(intptr_t uTexHandle, bool bPreserveTexture)
{
	return m_driver->SetRenderTarget(uTexHandle, bPreserveTexture);
}

void
LegacyDisplay::SetSphereEnvironmentMapping(TextureUnit tu, bool b)
{
	return m_driver->SetSphereEnvironmentMapping(tu, b);
}

void
LegacyDisplay::SetCelShaded(int stage)
{
	return m_driver->SetCelShaded(stage);
}

auto
LegacyDisplay::IsD3DInternal()
	-> bool
{
	return m_driver->IsD3DInternal();
}
