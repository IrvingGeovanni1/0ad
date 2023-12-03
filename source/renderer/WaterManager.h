/* Copyright (C) 2023 Wildfire Games.
 * This file is part of 0 A.D.
 *
 * 0 A.D. is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * 0 A.D. is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with 0 A.D.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Water settings (speed, height) and texture management
 */

#ifndef INCLUDED_WATERMANAGER
#define INCLUDED_WATERMANAGER

#include "graphics/Color.h"
#include "graphics/Texture.h"
#include "maths/Matrix3D.h"
#include "maths/Vector2D.h"
#include "renderer/backend/IDeviceCommandContext.h"
#include "renderer/backend/IFramebuffer.h"
#include "renderer/backend/IShaderProgram.h"
#include "renderer/backend/ITexture.h"
#include "renderer/VertexBufferManager.h"

#include <memory>
#include <vector>

class CFrustum;

struct WaveObject;

/**
 * Class WaterManager: Maintain rendering-related water settings and textures
 * Anything that affects gameplay should go in CcmpWaterManager.cpp and passed to this (possibly as copy).
 */

class WaterManager
{
public:
	CTexturePtr m_WaterTexture[60];
	CTexturePtr m_NormalMap[60];

	// How strong the waves are at point X. % of waviness.
	std::unique_ptr<float[]> m_WindStrength;
	// How far from the shore a point is. Manhattan.
	std::unique_ptr<float[]> m_DistanceHeightmap;

	// Waves vertex buffers
	// TODO: measure storing value instead of pointer.
	std::vector<std::unique_ptr<WaveObject>> m_ShoreWaves;
	// Waves indices buffer. Only one since All Wave Objects have the same.
	CVertexBufferManager::Handle m_ShoreWavesVBIndices;

	Renderer::Backend::IVertexInputLayout* m_ShoreVertexInputLayout = nullptr;

	size_t m_MapSize;
	ssize_t m_TexSize;

	CTexturePtr m_WaveTex;
	CTexturePtr m_FoamTex;

	std::unique_ptr<Renderer::Backend::ITexture> m_FancyTexture;
	std::unique_ptr<Renderer::Backend::ITexture> m_FancyTextureDepth;
	std::unique_ptr<Renderer::Backend::ITexture> m_ReflFboDepthTexture;
	std::unique_ptr<Renderer::Backend::ITexture> m_RefrFboDepthTexture;

	// used to know what to update when updating parts of the terrain only.
	u32 m_updatei0;
	u32 m_updatej0;
	u32 m_updatei1;
	u32 m_updatej1;

	bool m_RenderWater;

	// If disabled, force the use of the simple water shader for rendering.
	bool m_WaterEffects;
	// Those variables register the current quality level. If there is a change, I have to recompile the shader.
	// Use real depth or use the fake precomputed one.
	bool m_WaterRealDepth;
	// Use fancy shore effects and show trails behind ships
	bool m_WaterFancyEffects;
	// Use refractions instead of simply making the water more or less transparent.
	bool m_WaterRefraction;
	// Use complete reflections instead of showing merely the sky.
	bool m_WaterReflection;

	bool m_NeedsReloading;
	// requires also recreating the super fancy information.
	bool m_NeedInfoUpdate;

	float m_WaterHeight;

	double m_WaterTexTimer;
	float m_RepeatPeriod;

	// Reflection and refraction textures for fancy water
	std::unique_ptr<Renderer::Backend::ITexture> m_ReflectionTexture;
	std::unique_ptr<Renderer::Backend::ITexture> m_RefractionTexture;
	size_t m_RefTextureSize;

	// framebuffer objects
	std::unique_ptr<Renderer::Backend::IFramebuffer> m_RefractionFramebuffer;
	std::unique_ptr<Renderer::Backend::IFramebuffer> m_ReflectionFramebuffer;
	std::unique_ptr<Renderer::Backend::IFramebuffer> m_FancyEffectsFramebuffer;
	std::unique_ptr<Renderer::Backend::IFramebuffer> m_FancyEffectsOccludersFramebuffer;

	// Model-view-projection matrices for reflected & refracted cameras
	// (used to let the vertex shader do projective texturing)
	CMatrix3D m_ReflectionMatrix;
	CMatrix3D m_RefractionMatrix;
	CMatrix3D m_RefractionProjInvMatrix;
	CMatrix3D m_RefractionViewInvMatrix;

	// Water parameters
	std::wstring m_WaterType;		// Which texture to use.
	CColor m_WaterColor;	// Color of the water without refractions. This is what you're seeing when the water's deep or murkiness high.
	CColor m_WaterTint;		// Tint of refraction in the water.
	float m_Waviness;		// How big the waves are.
	float m_Murkiness;		// How murky the water is.
	float m_WindAngle;	// In which direction the water waves go.

public:
	WaterManager(Renderer::Backend::IDevice* device);
	~WaterManager();

	void Initialize();

	/**
	 * LoadWaterTextures: Load water textures from within the
	 * progressive load framework.
	 *
	 * @return 0 if loading has completed, a value from 1 to 100 (in percent of completion)
	 * if more textures need to be loaded and a negative error value on failure.
	 */
	int LoadWaterTextures();

	/**
	 * Recreates/loads needed water textures.
	 */
	void RecreateOrLoadTexturesIfNeeded();

	/**
	 * ReloadWaterNormalTextures: Reload the normal textures so that changing
	 * water type in Atlas will actually do the right thing.
	 */
	void ReloadWaterNormalTextures();

	/**
	 * UnloadWaterTextures: Free any loaded water textures and reset the internal state
	 * so that another call to LoadWaterTextures will begin progressive loading.
	 */
	void UnloadWaterTextures();

	/**
	 * RecomputeWaterData: calculates all derived data from the waterheight
	 */
	void RecomputeWaterData();

	/**
	 * RecomputeWindStrength: calculates the intensity of waves
	 */
	void RecomputeWindStrength();

	/**
	 * RecomputeDistanceHeightmap: recalculates (or calculates) the distance heightmap.
	 */
	void RecomputeDistanceHeightmap();

	/**
	 * CreateWaveMeshes: Creates the waves objects (and meshes).
	 */
	void CreateWaveMeshes();

	/**
	 * Updates the map size. Will trigger a complete recalculation of fancy water information the next turn.
	 */
	void SetMapSize(size_t size);

	/**
	 * Updates the settings to the one from the renderer, and sets m_NeedsReloading.
	 */
	void UpdateQuality();

	/**
	 * Returns true if fancy water shaders will be used (i.e. the hardware is capable
	 * and it hasn't been configured off)
	 */
	bool WillRenderFancyWater() const;

	void RenderWaves(
		Renderer::Backend::IDeviceCommandContext* deviceCommandContext,
		const CFrustum& frustrum);

	/**
	 * Returns an index of the current texture that should be used for rendering
	 * water.
	 */
	size_t GetCurrentTextureIndex(const double& period) const;
	size_t GetNextTextureIndex(const double& period) const;

private:
	Renderer::Backend::IDevice* m_Device = nullptr;
};


#endif // INCLUDED_WATERMANAGER
