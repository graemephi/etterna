#include "RageDisplay.h"

struct MatrixState;
class LegacyDisplay : public RageDisplay
{
    std::unique_ptr<RageDisplay> m_display;
  public:
	LegacyDisplay(RageDisplay* driver) : m_display(driver) {};
	~LegacyDisplay() override = default;
	auto Init(const VideoModeParams& p, bool bAllowUnacceleratedRenderer)
	  -> std::string override;

	[[nodiscard]] auto GetApiDescription() const -> std::string override
	{
		static std::string s = m_display->GetApiDescription() + " (Legacy)";
		return s;
	}
	virtual void GetDisplaySpecs(DisplaySpecs& out) const override;
	void ResolutionChanged() override;
	[[nodiscard]] auto GetPixelFormatDesc(RagePixelFormat pf) const
	  -> const RagePixelFormatDesc* override;

	auto BeginFrame() -> bool override;
	void EndFrame() override;
	[[nodiscard]] auto GetActualVideoModeParams() const
	  -> const ActualVideoModeParams* override;
	void SetBlendMode(BlendMode mode) override;
	auto SupportsTextureFormat(RagePixelFormat pixfmt, bool realtime = false)
	  -> bool override;
	auto SupportsThreadedRendering() -> bool override;
	auto SupportsPerVertexMatrixScale() -> bool override { return false; }
	auto CreateTexture(RagePixelFormat pixfmt,
					   RageSurface* img,
					   bool bGenerateMipMaps) -> intptr_t override;
	void UpdateTexture(intptr_t uTexHandle,
					   RageSurface* img,
					   int xoffset,
					   int yoffset,
					   int width,
					   int height) override;
	void DeleteTexture(intptr_t iTexHandle) override;
	void ClearAllTextures() override;
	auto GetNumTextureUnits() -> int override;
	void SetTexture(TextureUnit tu, intptr_t iTexture) override;
	void SetTextureMode(TextureUnit tu, TextureMode tm) override;
	void SetTextureWrapping(TextureUnit tu, bool b) override;
	[[nodiscard]] auto GetMaxTextureSize() const -> int override;
	void SetTextureFiltering(TextureUnit tu, bool b) override;
	[[nodiscard]] auto IsZWriteEnabled() const -> bool override;
	[[nodiscard]] auto IsZTestEnabled() const -> bool override;
	void SetZWrite(bool b) override;
	void SetZBias(float f) override;
	void SetZTestMode(ZTestMode mode) override;
	void ClearZBuffer() override;
	void SetCullMode(CullMode mode) override;
	void SetAlphaTest(bool b) override;
	void SetMaterial(const RageColor& emissive,
					 const RageColor& ambient,
					 const RageColor& diffuse,
					 const RageColor& specular,
					 float shininess) override;
	void SetLighting(bool b) override;
	void SetLightOff(int index) override;
	void SetLightDirectional(int index,
							 const RageColor& ambient,
							 const RageColor& diffuse,
							 const RageColor& specular,
							 const RageVector3& dir) override;

	auto CreateRenderTarget(const RenderTargetParam& param,
							int& iTextureWidthOut,
							int& iTextureHeightOut) -> intptr_t override;
	auto GetRenderTarget() -> intptr_t override;
	void SetRenderTarget(intptr_t uTexHandle, bool bPreserveTexture) override;

	void SetSphereEnvironmentMapping(TextureUnit tu, bool b) override;
	void SetCelShaded(int stage) override;

	auto IsD3DInternal() -> bool override;
	[[nodiscard]] auto SupportsFullscreenBorderlessWindow() const -> bool override
	{
		return m_display->SupportsFullscreenBorderlessWindow();
	}

	auto CreateCompiledGeometry() -> RageCompiledGeometry* override;
	void DeleteCompiledGeometry(RageCompiledGeometry* p) override;

  protected:
	void DrawQuadsInternal(const RageSpriteVertex v[], int iNumVerts) override;
	void DrawQuadStripInternal(const RageSpriteVertex v[], int iNumVerts) override;
	void DrawFanInternal(const RageSpriteVertex v[], int iNumVerts) override;
	void DrawStripInternal(const RageSpriteVertex v[], int iNumVerts) override;
	void DrawTrianglesInternal(const RageSpriteVertex v[], int iNumVerts) override;
	void DrawSymmetricQuadStripInternal(const RageSpriteVertex v[], int iNumVerts) override;
	void DrawCompiledGeometryInternal(const RageCompiledGeometry* p, int iMeshIndex) override;

	auto TryVideoMode(const VideoModeParams& p, bool& bNewDeviceOut) -> std::string override;
	auto CreateScreenshot() -> RageSurface* override;

	void DumpMatrices(MatrixState& m);
};
