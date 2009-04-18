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
 * =========================================================================
 * File        : ogl_tex.h
 * Project     : 0 A.D.
 * Description : wrapper for all OpenGL texturing calls.
 *             : provides caching, hotloading and lifetime management.
 * =========================================================================
 */

// license: GPL; see lib/license.txt

/*

[KEEP IN SYNC WITH WIKI!]

Introduction
------------

This module simplifies use of textures in OpenGL. An easy-to-use
load/upload/bind/free API is provided, which completely replaces
direct access to OpenGL's texturing calls.

It basically wraps tex.cpp's texture info in a resource object
(see h_mgr.h) that maintains associated GL state and provides for
reference counting, caching, hotloading and safe access.
Additionally, the upload step provides for trading quality vs. speed
and works around older hardware/drivers.


Texture Parameters
------------------

OpenGL textures are conditioned on several parameters including
filter and wrap mode. These are typically set once when the texture is
created, but must survive reloads (1). To that end, all state (2) is
set via ogl_tex_set_* (instead of direct glTexParameter calls) and
re-applied after a reload.

(1) the purpose of hotloading is to permit artists to see their changes
    in-game without having to restart the map. reloads where the
    texture looks different due to changed state are useless.

(2) currently only filter and wrap mode. no other glTexParameter
    settings are used ATM; if that changes, add to OglTexState.


Uploading to OpenGL
-------------------

.. deserves some clarification. This entails calling glTexImage2D
(or its variants for mipmaps/compressed textures) and transfers
texture parameters and data from system memory to OpenGL
(and thereby usually video memory).

In so doing, choices are made as to the texture's internal representation
(how it is stored in vmem) - in particular, the bit depth.
This can trade performance (more/less data to copy) for quality
(fidelity to original).

We provide a mechanism that applies defaults to all uploads;
this allows a global "quality" setting that can boost performance on
older graphics cards without requiring anything else to be changed.
Textures with specific quality needs can override this via
ogl_tex_set_* or ogl_tex_upload parameters.

Finally, provision is made for coping with hardware/drivers lacking support
for S3TC decompression or mipmap generation: that can be done in software,
if necessary. This avoids the need for alternate asset formats and
lowers hardware requirements. While such cards probably won't run
the app very well (due to their age and lack of other capabilities),
this does make possible developing/testing on older machines/laptops.


Caching and Texture Instances
-----------------------------

Caching is both an advantage and drawback. When opening the same
texture twice without previously freeing it, a reference to the
first instance is returned. Therefore, be advised that concurrent use of the
same texture but with differing parameters (e.g. upload quality) followed by 
a reload of the first instance will result in using the wrong parameters.
For background and rationale why this is acceptable, see struct OglTex.


Example Usage
-------------

Note: to keep the examples simple, we leave out error handling by
ignoring all return values. Each function will still raise a warning
(assert) if it fails and passing e.g. invalid Handles will only cause
the next function to fail, but real apps should check and report errors.

1) Basic usage: load texture from file.

	Handle hTexture = ogl_tex_load("filename.dds");
	(void)ogl_tex_upload(hTexture);

	[when rendering:]
	(void)ogl_tex_bind(hTexture);
	[.. do something with OpenGL that uses the currently bound texture]

	[at exit:]
	* (done automatically, but this avoids it showing up as a leak)
	(void)ogl_tex_free(hTexture);


2) Advanced usage: wrap existing texture data, override filter,
   specify internal_format and use multitexturing.

	Tex t;
	const size_t flags = 0;	* image is plain RGB, default orientation
	void* data = [pre-existing image]
	(void)tex_wrap(w, h, 24, flags, data, &t);
	Handle hCompositeAlphaMap = ogl_tex_wrap(&t, "(alpha map composite)");
	(void)ogl_tex_set_filter(hCompositeAlphaMap, GL_LINEAR);
	(void)ogl_tex_upload(hCompositeAlphaMap, 0, 0, GL_INTENSITY);
	* (your responsibility! tex_wrap attaches a reference but it is
	* removed by ogl_tex_upload.)
	free(data);

	[when rendering:]
	(void)ogl_tex_bind(hCompositeAlphaMap, 1);
	[.. do something with OpenGL that uses the currently bound texture]

	[at exit:]
	* (done automatically, but this avoids it showing up as a leak)
	(void)ogl_tex_free(hCompositeAlphaMap);

*/

#ifndef INCLUDED_OGL_TEX
#define INCLUDED_OGL_TEX

#include "lib/res/handle.h"
#include "lib/ogl.h"
#include "lib/tex/tex.h"


//
// quality mechanism
//

/**
* Quality flags for texture uploads.
* Specify any of them to override certain aspects of the default.
*/
enum OglTexQualityFlags
{
	/**
	 * emphatically require full quality for this texture.
	 * (q_flags are invalid if this is set together with any other bit)
	 * rationale: the value 0 is used to indicate "use default flags" in
	 * ogl_tex_upload and ogl_tex_set_defaults, so this is the only
	 * way we can say "disregard default and do not reduce anything".
	 */
	OGL_TEX_FULL_QUALITY = 0x20,

	/**
	 * store the texture at half the normal bit depth
	 * (4 bits per pixel component, as opposed to 8).
	 * this increases performance on older graphics cards due to
	 * decreased size in vmem. it has no effect on
	 * compressed textures because they have a fixed internal format.
	 */
	OGL_TEX_HALF_BPP = 0x10,

	/**
	 * store the texture at half its original resolution.
	 * this increases performance on older graphics cards due to
	 * decreased size in vmem. 
	 * this is useful for also reducing quality of compressed textures,
	 * which are not affected by OGL_TEX_HALF_BPP.
	 * currently only implemented for images that contain mipmaps
	 * (otherwise, we'd have to resample, which is slow).
	 * note: scaling down to 1/4, 1/8, .. is easily possible without
	 * extra work, so we leave some bits free for that.
	 */
	OGL_TEX_HALF_RES = 0x01
};

/**
* Change default settings - these affect performance vs. quality.
* May be overridden for individual textures via parameter to
* ogl_tex_upload or ogl_tex_set_filter, respectively.
*
* @param q_flags quality flags. Pass 0 to keep the current setting
*        (initially OGL_TEX_FULL_QUALITY), or any combination of
*        OglTexQualityFlags.
* @param filter mag/minification filter. Pass 0 to keep the current setting
*        (initially GL_LINEAR), or any valid OpenGL minification filter.
*/
extern void ogl_tex_set_defaults(int q_flags, GLint filter);


//
// open/close
//

/**
* Load and return a handle to the texture.
*
* @param flags h_alloc flags.
* @return Handle to texture or negative LibError
* for a list of supported formats, see tex.h's tex_load.
*/
extern Handle ogl_tex_load(const VfsPath& pathname, size_t flags = 0);

/**
* Find and return an existing texture object, if it has already been
* loaded and is still in memory.
*
* @param fn VFS filename of texture.
* @return Handle to texture or negative LibError
*/
extern Handle ogl_tex_find(const VfsPath& pathname);

/**
* Make the Tex object ready for use as an OpenGL texture
* and return a handle to it. This will be as if its contents
* had been loaded by ogl_tex_load.
*
* @param t Texture object.
* @param fn filename or description of texture. not strictly needed,
*        but would allow h_filename to return meaningful info for
*        purposes of debugging.
* @return Handle to texture or negative LibError
*
* note: because we cannot guarantee that callers will pass distinct
* "filenames", caching is disabled for the created object. this avoids
* mistakenly reusing previous objects that share the same comment.
*
* we need only add bookkeeping information and "wrap" it in
* a resource object (accessed via Handle), hence the name.
*/
extern Handle ogl_tex_wrap(Tex* t, const char* fn = 0, size_t flags = 0);

/**
* Release this texture reference. When the count reaches zero, all of
* its associated resources are freed and further use made impossible.
*
* @param ht Texture handle.
* @return LibError
*/
extern LibError ogl_tex_free(Handle& ht);


//
// set texture parameters
//

// these must be called before uploading; this simplifies
// things and avoids calling glTexParameter twice.

/**
* Override default filter (see {@link #ogl_tex_set_defaults}) for
* this texture.
*
* @param filter OpenGL minification and magnification filter
*        (rationale: see {@link OglTexState})
* @return LibError
*
* Must be called before uploading (raises a warning if called afterwards).
*/
extern LibError ogl_tex_set_filter(Handle ht, GLint filter);

/**
* Override default wrap mode (GL_REPEAT) for this texture.
*
* @param wrap OpenGL wrap mode (for both S and T coordinates)
*        (rationale: see {@link OglTexState})
* @return LibError
*
* Must be called before uploading (raises a warning if called afterwards).
*/
extern LibError ogl_tex_set_wrap(Handle ht, GLint wrap);


//
// upload
//

enum OglTexOverrides
{
	OGL_TEX_S3TC,
	OGL_TEX_AUTO_MIPMAP_GEN
};

enum OglTexAllow
{
	OGL_TEX_DISABLE,
	OGL_TEX_ENABLE
};

/**
* Override the default decision and force/disallow use of the
* given feature. Typically called from ah_override_gl_upload_caps.
*
* @param what feature to influence
* @param allow disable/enable flag
*/
extern void ogl_tex_override(OglTexOverrides what, OglTexAllow allow);

/**
* Upload texture to OpenGL.
*
* @param fmt_ovr optional override for OpenGL format (e.g. GL_RGB),
*        which is decided from bpp / Tex flags
* @param q_flags_ovr optional override for global default
*        OglTexQualityFlags
* @param int_fmt_ovr optional override for OpenGL internal format
*        (e.g. GL_RGB8), which is decided from fmt / q_flags.
* @return LibError
* Side Effects:
* <UL>
*   <LI>enables texturing on TMU 0 and binds the texture to it;
*   <LI>frees the texel data! see ogl_tex_get_data.
* </UL>
*/
extern LibError ogl_tex_upload(const Handle ht, GLenum fmt_ovr = 0, int q_flags_ovr = 0, GLint int_fmt_ovr = 0);


//
// return information about the texture
//

/**
* Retrieve dimensions and bit depth of the texture.
*
* @param ht Texture handle
* @param w optional; will be filled with width
* @param h optional; will be filled with height
* @param bpp optional; will be filled with bits per pixel
* @return LibError
*/
extern LibError ogl_tex_get_size(Handle ht, size_t* w, size_t* h, size_t* bpp);

/**
* Retrieve pixel format of the texture.
*
* @param ht Texture handle
* @param flags optional; will be filled with TexFlags
* @param fmt optional; will be filled with GL format
*        (it is determined during ogl_tex_upload and 0 before then)
* @return LibError
*/
extern LibError ogl_tex_get_format(Handle ht, size_t* flags, GLenum* fmt);

/**
* Retrieve pixel data of the texture.
*
* @param ht Texture handle
* @param p will be filled with pointer to texels.
* @return LibError
*
* Note: this memory is freed after a successful ogl_tex_upload for
* this texture. After that, the pointer we retrieve is NULL but
* the function doesn't fail (negative return value) by design.
* If you still need to get at the data, add a reference before
* uploading it or read directly from OpenGL (discouraged).
*/
extern LibError ogl_tex_get_data(Handle ht, u8** p);


//
// misc
//

/**
* Bind texture to the specified unit in preparation for using it in
* rendering.
*
* @param ht Texture handle. If 0, texturing is disabled on this unit.
* @param unit Texture Mapping Unit number, typically 0 for the first.
* @return LibError
*
* Side Effects:
* - changes the active texture unit;
* - (if successful) texturing was enabled/disabled on that unit.
*
* Notes:
* - assumes multitexturing is available.
* - not necessary before calling ogl_tex_upload!
* - on error, the unit's texture state is unchanged; see implementation.
*/
extern LibError ogl_tex_bind(Handle ht, size_t unit = 0);

/**
* (partially) Transform pixel format of the texture.
*
* @param ht Texture handle
* @param flags the TexFlags that are to be <em>changed</em>
* @return LibError
* @see tex_transform
*
* Must be called before uploading (raises a warning if called afterwards).
*/
extern LibError ogl_tex_transform(Handle ht, size_t flags);

/**
* Transform pixel format of the texture.
*
* @param ht Texture handle
* @param flags desired new TexFlags indicating pixel format.
* @return LibError
* @see tex_transform
*
* Must be called before uploading (raises a warning if called afterwards).
*
* Note: this is equivalent to ogl_tex_transform(ht, ht_flags^new_flags).
*/
extern LibError ogl_tex_transform_to(Handle ht, size_t new_flags);

#endif	// #ifndef INCLUDED_OGL_TEX
