#include "Etterna/Globals/global.h"
#include "LegacyDisplay.h"

enum DrawMode
{
	DrawMode_Invalid,
	DrawMode_Quads,
	DrawMode_QuadStrip,
	DrawMode_Fan,
	DrawMode_Strip,
	DrawMode_Triangles,
	DrawMode_SymmetricQuadStrip,
	DrawMode_CompiledGeometry,
};

enum CommandType
{
	Command_Invalid,
	Command_ClearZBuffer,
	Command_SetCullMode,
	Command_SetBlendMode,
	Command_SetZBias,
	Command_SetZTestMode,
	Command_SetZWrite,
	Command_SetAlphaTest,
	Command_SetTexture,
	Command_SetTextureMode,
	Command_SetTextureFiltering,
	Command_SetTextureWrapping,
	Command_Draw,
};

struct RenderState
{
	CullMode cullMode;
	ZTestMode zTestMode;
	BlendMode blendMode;
	float zBias;
	bool zWrite;
	bool alphaTest;
	bool textureWrapping[NUM_TextureUnit];
	bool textureFiltering[NUM_TextureUnit];
	uint8_t textureMode[NUM_TextureUnit];
	intptr_t textures[NUM_TextureUnit];
};

#define RENDERSTATE_STATS 1
#if defined(RENDERSTATE_STATS) && RENDERSTATE_STATS
#include <array>
#include <unordered_map>

struct RenderStateStats
{
	int32_t frames;
	int32_t cullMode;
	int32_t zTestMode;
	int32_t blendMode;
	int32_t zBias;
	int32_t zWrite;
	int32_t alphaTest;
	int32_t textureWrapping[NUM_TextureUnit];
	int32_t textureFiltering[NUM_TextureUnit];
	int32_t textureMode[NUM_TextureUnit];
	int32_t textures[NUM_TextureUnit];

	int32_t textureModeCounts[NUM_TextureMode];
	int32_t blendModeCounts[NUM_BlendMode];
	int32_t zTestModeCounts[NUM_ZTestMode];
	int32_t cullModeCounts[NUM_CullMode];
	std::array<int32_t, 64> textureCounts;
};

static RenderStateStats g_RenderStateStats = {};
static std::unordered_map<intptr_t, intptr_t> g_SequentialTextureIDs;
intptr_t SequentialTextureID(intptr_t id)
{
	// Terrible std api: this line is the insertion
	intptr_t result = g_SequentialTextureIDs[id];
	if (result == 0) {
		result = g_SequentialTextureIDs.size();
		g_SequentialTextureIDs[id] = result;
	}
	return result - 1;
}

/*
Loading the game and playing you suffer, exiting back to music wheel:
frames: 51847
cullMode: 0
zTestMode: 304
blendMode: 304
zBias: 304
zWrite: 304
alphaTest: 0
textureWrapping: {102112, 0, 0, 0, 0, 0, 0, 0}
textureFiltering: {0, 0, 0, 0, 0, 0, 0, 0}
textureMode: {108341, 0, 0, 0, 0, 0, 0, 0}
textures: {1257517, 0, 0, 0, 0, 0, 0, 0}
textureModeCounts: {80094, 28247, 0}
blendModeCounts: {152, 0, 0, 0, 0, 0, 0, 0, 0, 0, 152}
zTestModeCounts: {152, 152, 0}
cullModeCounts: {0, 0, 0}
textureCounts:
    [00]: 377337
    [01]: 411274
    [02]: 26
    [03]: 145066
    [04]: 2
    [05]: 51852
    [06]: 113082
    [07]: 89
    [08]: 89
    [09]: 165
    [10]: 1699
    [11]: 49369
    [12]: 49369
    [13]: 49368
    [14]: 4058
    [15]: 1290
    [16]: 1290
    [17]: 1290
    [18]: 307
    [19]: 307
    [20]: 188
    [21]: 0
*/

#define IncrementStatistic(field) g_RenderStateStats.field++
#else
#define IncrementStatistic(field) void
#endif

struct MatrixState
{
	RageMatrix projection;
	RageMatrix view;
	RageMatrix world;
	RageMatrix texture;
};

static RenderState g_RenderState;

struct Command
{
	CommandType type;
	int32_t sizeInBytes;
	union
	{
		CullMode cullMode;
		ZTestMode zTestMode;
		BlendMode blendMode;
		float zBias;
		bool zWrite;
		bool alphaTest;
		struct
		{
			TextureUnit handle;
			intptr_t value;
		} texture;
		DrawMode drawMode;
	};
};

struct CommandBuffer
{
	std::vector<uint8_t> buffer;
	size_t numCommands;

	void Push(Command command)
	{
		ASSERT_M(command.type != Command_Invalid, "No command set");
		ASSERT_M(command.type != Command_Draw, "Use PushDrawCommand to push Command_Draw");
		command.sizeInBytes = sizeof(Command);
		copyIntoBuffer((uint8_t *)&command, sizeof(Command));
		numCommands += 1;
	}

	void PushDrawCommand(DrawMode mode, MatrixState m, uint8_t* vertex_data, size_t length)
	{
		Command command = {};
		command.type = Command_Draw;
		command.sizeInBytes = sizeof(Command) + sizeof(MatrixState) + length;
		command.drawMode = mode;
		copyIntoBuffer((uint8_t *)&command, sizeof(Command));
		copyIntoBuffer((uint8_t *)&m, sizeof(MatrixState));
		copyIntoBuffer(vertex_data, length);
		numCommands += 1;
	}

	void Clear()
	{
		buffer.clear();

		numCommands = 0;
	}

	void copyIntoBuffer(uint8_t* buf, size_t len)
	{
		size_t end = buffer.size();
		buffer.resize(end + len);
		memcpy(&buffer[end], buf, len);
	}
};

static CommandBuffer g_CommandBuffer;

void LegacyDisplay::DumpMatrices(MatrixState& m)
{
	m.projection = *DISPLAY->GetProjectionTop();
	m.view = *DISPLAY->GetViewTop();
	m.world = *DISPLAY->GetWorldTop();
	m.texture = *DISPLAY->GetTextureTop();
}

auto
LegacyDisplay::BeginFrame()
	-> bool
{
	g_CommandBuffer.Clear();
	g_RenderState.cullMode = CULL_NONE;
	g_RenderState.zTestMode = ZTEST_OFF;
	g_RenderState.blendMode = BLEND_NORMAL;
	g_RenderState.zBias = 0.0f;
	g_RenderState.zWrite = false;
	g_RenderState.alphaTest = true;
	g_RenderState.textureFiltering[0] = true;
	g_RenderState.textureMode[0] = TextureMode_Invalid;
	g_RenderState.textureWrapping[0] = false;
	return m_display->BeginFrame();
}

void
LegacyDisplay::EndFrame()
{
	IncrementStatistic(frames);

	MatrixState m; DumpMatrices(m);

	uint8_t *cursor = &g_CommandBuffer.buffer[0];
	for (size_t i = 0; i < g_CommandBuffer.numCommands; i++) {
		ASSERT_M((cursor - &g_CommandBuffer.buffer[0]) < (ptrdiff_t)g_CommandBuffer.buffer.size(), "LegacyDisplay: Bad command buffer");
		Command *command = (Command *)cursor;

		switch (command->type) {
			case Command_ClearZBuffer: {
				m_display->ClearZBuffer();
			} break;
			case Command_SetCullMode: {
				m_display->SetCullMode(command->cullMode);
			} break;
			case Command_SetBlendMode: {
				m_display->SetBlendMode(command->blendMode);
			} break;
			case Command_SetZBias: {
				m_display->SetZBias(command->zBias);
			} break;
			case Command_SetZTestMode: {
				m_display->SetZTestMode(command->zTestMode);
			} break;
			case Command_SetZWrite: {
				m_display->SetZWrite(command->zWrite);
			} break;
			case Command_SetAlphaTest: {
				m_display->SetAlphaTest(command->alphaTest);
			} break;
			case Command_SetTexture: {
				m_display->SetTexture(command->texture.handle, command->texture.value);
			} break;
			case Command_SetTextureMode: {
				m_display->SetTextureMode(command->texture.handle, (TextureMode)command->texture.value);
			} break;
			case Command_SetTextureFiltering: {
				m_display->SetTextureFiltering(command->texture.handle, command->texture.value);
			} break;
			case Command_SetTextureWrapping: {
				m_display->SetTextureWrapping(command->texture.handle, command->texture.value);
			} break;
			case Command_Draw: {
				// Ugly but no point in doing better
				MatrixState *m = (MatrixState *)(cursor + sizeof(Command));
				int32_t numVertices = (command->sizeInBytes - sizeof(Command) - sizeof(MatrixState)) / sizeof(RageSpriteVertex);
				RageSpriteVertex *vertexData = (RageSpriteVertex *)(cursor + sizeof(Command) + sizeof(MatrixState));

				// We could push and pop here like good citizens, but we won't
				*(RageMatrix *)DISPLAY->GetProjectionTop() = m->projection;
				*(RageMatrix *)DISPLAY->GetViewTop() = m->view;
				*(RageMatrix *)DISPLAY->GetWorldTop() = m->world;
				*(RageMatrix *)DISPLAY->GetTextureTop() = m->texture;

				switch (command->drawMode) {
					case DrawMode_Quads: {
						m_display->DrawQuadsInternal(vertexData, numVertices);
					} break;
					case DrawMode_QuadStrip: {
						m_display->DrawQuadStripInternal(vertexData, numVertices);
					} break;
					case DrawMode_Fan: {
						m_display->DrawFanInternal(vertexData, numVertices);
					} break;
					case DrawMode_Strip: {
						m_display->DrawStripInternal(vertexData, numVertices);
					} break;
					case DrawMode_Triangles: {
						m_display->DrawTrianglesInternal(vertexData, numVertices);
					} break;
					case DrawMode_SymmetricQuadStrip: {
						m_display->DrawSymmetricQuadStripInternal(vertexData, numVertices);
					} break;
					case DrawMode_CompiledGeometry: {
						FAIL_M("Never used?");
					} break;
					default: {
						FAIL_M("LegacyDisplay: Bad command buffer");
					}
				}
			} break;
			default: {
				FAIL_M("LegacyDisplay: Bad command buffer");
			}
		}

		cursor += command->sizeInBytes;
	}

	*(RageMatrix *)DISPLAY->GetProjectionTop() = m.projection;
	*(RageMatrix *)DISPLAY->GetViewTop() = m.view;
	*(RageMatrix *)DISPLAY->GetWorldTop() = m.world;
	*(RageMatrix *)DISPLAY->GetTextureTop() = m.texture;
	return m_display->EndFrame();
}

void
LegacyDisplay::SetAlphaTest(bool b)
{
	if (g_RenderState.alphaTest != b) {
		IncrementStatistic(alphaTest);
		g_RenderState.alphaTest = b;
		Command command = {};
		command.type = Command_SetAlphaTest;
		command.alphaTest = b;
		g_CommandBuffer.Push(command);
	}
}

void
LegacyDisplay::SetBlendMode(BlendMode mode)
{
	if (g_RenderState.blendMode != mode) {
		IncrementStatistic(blendMode);
		IncrementStatistic(blendModeCounts[mode]);
		g_RenderState.blendMode = mode;
		Command command = {};
		command.type = Command_SetBlendMode;
		command.blendMode = mode;
		g_CommandBuffer.Push(command);
	}
}

void
LegacyDisplay::SetCullMode(CullMode mode)
{
	if (g_RenderState.cullMode != mode) {
		IncrementStatistic(cullMode);
		IncrementStatistic(cullModeCounts[mode]);
		g_RenderState.cullMode = mode;
		Command command = {};
		command.type = Command_SetCullMode;
		command.cullMode = mode;
		g_CommandBuffer.Push(command);
	}
}

void
LegacyDisplay::SetZBias(float f)
{
	if (g_RenderState.zBias != f) {
		IncrementStatistic(zBias);
		g_RenderState.zBias = f;
		Command command = {};
		command.type = Command_SetZBias;
		command.zBias = f;
		g_CommandBuffer.Push(command);
	}
}

void
LegacyDisplay::SetZWrite(bool b)
{
	if (g_RenderState.zWrite != b) {
		IncrementStatistic(zWrite);
		g_RenderState.zWrite = b;
		Command command = {};
		command.type = Command_SetZWrite;
		command.zWrite = b;
		g_CommandBuffer.Push(command);
	}
}

void
LegacyDisplay::SetZTestMode(ZTestMode mode)
{
	if (g_RenderState.zTestMode != mode) {
		IncrementStatistic(zTestMode);
		IncrementStatistic(zTestModeCounts[mode]);
		g_RenderState.zTestMode = mode;
		Command command = {};
		command.type = Command_SetZTestMode;
		command.zTestMode = mode;
		g_CommandBuffer.Push(command);
	}
}

void
LegacyDisplay::ClearZBuffer()
{
	Command command = {};
	command.type = Command_ClearZBuffer;
	g_CommandBuffer.Push(command);
}

void
LegacyDisplay::SetTexture(TextureUnit tu, intptr_t iTexture)
{
	if (g_RenderState.textures[tu] != iTexture) {
		IncrementStatistic(textures[tu]);
		IncrementStatistic(textureCounts[SequentialTextureID(iTexture)]);
		g_RenderState.textures[tu] = iTexture;
		Command command = {};
		command.type = Command_SetTexture;
		command.texture = { tu, iTexture };
		g_CommandBuffer.Push(command);
	}
}

void
LegacyDisplay::SetTextureMode(TextureUnit tu, TextureMode tm)
{
	if (g_RenderState.textureMode[tu] != tm) {
		IncrementStatistic(textureMode[tu]);
		IncrementStatistic(textureModeCounts[tm]);
		g_RenderState.textureMode[tu] = tm;
		Command command = {};
		command.type = Command_SetTextureMode;
		command.texture = { tu, tm };
		g_CommandBuffer.Push(command);
	}
}

void
LegacyDisplay::SetTextureFiltering(TextureUnit tu, bool b)
{
	if (g_RenderState.textureFiltering[tu] != b) {
		IncrementStatistic(textureFiltering[tu]);
		g_RenderState.textureFiltering[tu] = b;
		Command command = {};
		command.type = Command_SetTextureFiltering;
		command.texture = { tu, b };
		g_CommandBuffer.Push(command);
	}
}

void
LegacyDisplay::SetTextureWrapping(TextureUnit tu, bool b)
{
	if (g_RenderState.textureWrapping[tu] != b) {
		IncrementStatistic(textureWrapping[tu]);
		g_RenderState.textureWrapping[tu] = b;
		Command command = {};
		command.type = Command_SetTextureWrapping;
		command.texture = { tu, b };
		g_CommandBuffer.Push(command);
	}
}

void
LegacyDisplay::DrawQuadsInternal(const RageSpriteVertex v[], int iNumVerts)
{
	MatrixState m; DumpMatrices(m);
	g_CommandBuffer.PushDrawCommand(DrawMode_Quads, m, (uint8_t *)v, iNumVerts * sizeof(RageSpriteVertex));
}

void
LegacyDisplay::DrawQuadStripInternal(const RageSpriteVertex v[], int iNumVerts)
{
	MatrixState m; DumpMatrices(m);
	g_CommandBuffer.PushDrawCommand(DrawMode_QuadStrip, m, (uint8_t *)v, iNumVerts * sizeof(RageSpriteVertex));
}

void
LegacyDisplay::DrawSymmetricQuadStripInternal(const RageSpriteVertex v[], int iNumVerts)
{
	MatrixState m; DumpMatrices(m);
	g_CommandBuffer.PushDrawCommand(DrawMode_SymmetricQuadStrip, m, (uint8_t *)v, iNumVerts * sizeof(RageSpriteVertex));
}

void
LegacyDisplay::DrawFanInternal(const RageSpriteVertex v[], int iNumVerts)
{
	MatrixState m; DumpMatrices(m);
	g_CommandBuffer.PushDrawCommand(DrawMode_Fan, m, (uint8_t *)v, iNumVerts * sizeof(RageSpriteVertex));
}

void
LegacyDisplay::DrawStripInternal(const RageSpriteVertex v[], int iNumVerts)
{
	MatrixState m; DumpMatrices(m);
	g_CommandBuffer.PushDrawCommand(DrawMode_Strip, m, (uint8_t *)v, iNumVerts * sizeof(RageSpriteVertex));
}

void
LegacyDisplay::DrawTrianglesInternal(const RageSpriteVertex v[], int iNumVerts)
{
	MatrixState m; DumpMatrices(m);
	g_CommandBuffer.PushDrawCommand(DrawMode_Triangles, m, (uint8_t *)v, iNumVerts * sizeof(RageSpriteVertex));
}

void
LegacyDisplay::DrawCompiledGeometryInternal(const RageCompiledGeometry* p, int iMeshIndex)
{
	FAIL_M("Never called?");
}

auto
LegacyDisplay::IsZWriteEnabled() const
	-> bool
{
	FAIL_M("Never called?");
}

auto
LegacyDisplay::IsZTestEnabled() const
	-> bool
{
	FAIL_M("Never called?");
}

// All functions below are forwarded to the underlying display immediately.

auto LegacyDisplay::GetPixelFormatDesc(RagePixelFormat pf) const
	-> const RagePixelFormatDesc*
{
	return m_display->GetPixelFormatDesc(pf);
}

auto
LegacyDisplay::Init(const VideoModeParams& p, bool bAllowUnacceleratedRenderer)
	-> std::string
{
	return m_display->Init(p, bAllowUnacceleratedRenderer);
}

void
LegacyDisplay::GetDisplaySpecs(DisplaySpecs& out) const
{
	return m_display->GetDisplaySpecs(out);
}

auto
LegacyDisplay::TryVideoMode(const VideoModeParams& _p, bool& bNewDeviceOut)
  -> std::string
{
	return m_display->TryVideoMode(_p, bNewDeviceOut);
}

void
LegacyDisplay::ResolutionChanged()
{
	return m_display->ResolutionChanged();
}

auto
LegacyDisplay::GetMaxTextureSize() const
	-> int
{
	return m_display->GetMaxTextureSize();
}

auto
LegacyDisplay::SupportsTextureFormat(RagePixelFormat pixfmt, bool realtime)
	-> bool
{
	return m_display->SupportsTextureFormat(pixfmt, realtime);
}

auto
LegacyDisplay::SupportsThreadedRendering()
	-> bool
{
	return m_display->SupportsThreadedRendering();
}

auto
LegacyDisplay::CreateScreenshot()
	-> RageSurface*
{
	return m_display->CreateScreenshot();
}

auto
LegacyDisplay::GetActualVideoModeParams() const
	-> const ActualVideoModeParams*
{
	return m_display->GetActualVideoModeParams();
}

auto
LegacyDisplay::CreateCompiledGeometry()
	-> RageCompiledGeometry*
{
	FAIL_M("Never called?");
	return m_display->CreateCompiledGeometry();
}

void
LegacyDisplay::DeleteCompiledGeometry(RageCompiledGeometry* p)
{
	FAIL_M("Never called?");
	return m_display->DeleteCompiledGeometry(p);
}

void
LegacyDisplay::ClearAllTextures()
{
	FAIL_M("Never called?");
	return m_display->ClearAllTextures();
}

auto
LegacyDisplay::GetNumTextureUnits()
	-> int
{
	return m_display->GetNumTextureUnits();
}

void
LegacyDisplay::SetMaterial(const RageColor& emissive, const RageColor& ambient, const RageColor& diffuse, const RageColor& specular, float shininess)
{
	FAIL_M("Never called?");
}

void
LegacyDisplay::SetLighting(bool b)
{
	return m_display->SetLighting(b);
}

void
LegacyDisplay::SetLightOff(int index)
{
	return m_display->SetLightOff(index);
}

void
LegacyDisplay::SetLightDirectional(int index, const RageColor& ambient, const RageColor& diffuse, const RageColor& specular, const RageVector3& dir)
{
	FAIL_M("Never called?");
}

void
LegacyDisplay::DeleteTexture(intptr_t iTexHandle)
{
	return m_display->DeleteTexture(iTexHandle);
}

auto
LegacyDisplay::CreateTexture(RagePixelFormat pixfmt, RageSurface* img, bool bGenerateMipMaps)
	-> intptr_t
{
	return m_display->CreateTexture(pixfmt, img, bGenerateMipMaps);
}

void
LegacyDisplay::UpdateTexture(intptr_t uTexHandle, RageSurface* img, int xoffset, int yoffset, int width, int height)
{
	return m_display->UpdateTexture(uTexHandle, img, xoffset, yoffset, width, height);
}

auto
LegacyDisplay::CreateRenderTarget(const RenderTargetParam& param, int& iTextureWidthOut, int& iTextureHeightOut)
	-> intptr_t
{
	FAIL_M("Never called?");
	return m_display->CreateRenderTarget(param, iTextureWidthOut, iTextureHeightOut);
}

auto
LegacyDisplay::GetRenderTarget()
	-> intptr_t
{
	FAIL_M("Never called?");
	return m_display->GetRenderTarget();
}

void
LegacyDisplay::SetRenderTarget(intptr_t uTexHandle, bool bPreserveTexture)
{
	FAIL_M("Never called?");
	return m_display->SetRenderTarget(uTexHandle, bPreserveTexture);
}

void
LegacyDisplay::SetSphereEnvironmentMapping(TextureUnit tu, bool b)
{
	FAIL_M("Never called?");
	return m_display->SetSphereEnvironmentMapping(tu, b);
}

void
LegacyDisplay::SetCelShaded(int stage)
{
	FAIL_M("Never called?");
	return m_display->SetCelShaded(stage);
}

auto
LegacyDisplay::IsD3DInternal()
	-> bool
{
	FAIL_M("Never called?");
	return m_display->IsD3DInternal();
}
