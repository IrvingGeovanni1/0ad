/* Copyright (C) 2009 Wildfire Games.
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

/**
 * File        : Scene.h
 * Project     : graphics
 * Description : This file contains the interfaces that are used to send a
 *             : scene to the renderer, and for the renderer to query objects
 *             : in that scene.
 *
 * @note This file would fit just as well into the graphics/ subdirectory.
 **/

#ifndef INCLUDED_SCENE
#define INCLUDED_SCENE

class CFrustum;
class CModel;
class CPatch;

class SceneCollector;

/**
 * This interface describes a scene to the renderer.
 *
 * @see CRenderer::RenderScene
 */
class Scene {
public:
	/**
	 * Send all objects that can be seen when rendering the given frustum
	 * to the scene collector.
	 * @param frustum The frustum that will be used for rendering.
	 * @param c The scene collector that should receive objects inside the frustum
	 *  that are visible.
	 */
	virtual void EnumerateObjects(const CFrustum& frustum, SceneCollector* c) = 0;
};


/**
 * This interface accepts renderable objects.
 *
 * @see Scene::EnumerateObjects
 */
class SceneCollector {
public:
	/**
	 * Submit a terrain patch that is part of the scene.
	 */
	virtual void Submit(CPatch* patch) = 0;

	/**
	 * Submit a model that is part of the scene,
	 * without submitting attached models.
	 */
	virtual void SubmitNonRecursive(CModel* model) = 0;

	/**
	 * Submit a model that is part of the scene,
	 * including attached sub-models.
	 *
	 * @note This function is implemented using SubmitNonRecursive,
	 * so you shouldn't have to reimplement it.
	 */
	virtual void SubmitRecursive(CModel* model);
};


#endif // INCLUDED_SCENE
