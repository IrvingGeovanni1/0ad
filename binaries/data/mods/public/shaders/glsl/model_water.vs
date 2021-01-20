#if USE_GPU_SKINNING
// Skinning requires GLSL 1.30 for ivec4 vertex attributes
#version 130
#else
#version 120
#endif

#include "common/los_vertex.h"
#include "common/shadows_vertex.h"

uniform mat4 transform;
uniform vec3 cameraPos;
uniform vec3 sunDir;
uniform vec3 sunColor;
uniform mat4 instancingTransform;

uniform float sim_time;
uniform vec2 translation;

attribute vec3 a_vertex;
attribute vec3 a_normal;
#if USE_INSTANCING
  attribute vec4 a_tangent;
#endif
attribute vec2 a_uv0;
attribute vec2 a_uv1;

#if USE_GPU_SKINNING
  const int MAX_INFLUENCES = 4;
  const int MAX_BONES = 64;
  uniform mat4 skinBlendMatrices[MAX_BONES];
  attribute ivec4 a_skinJoints;
  attribute vec4 a_skinWeights;
#endif

varying vec4 worldPos;
varying vec4 v_tex;

vec4 fakeCos(vec4 x)
{
	vec4 tri = abs(fract(x + 0.5) * 2.0 - 1.0);
	return tri * tri *(3.0 - 2.0 * tri);
}

void main()
{
	worldPos = instancingTransform * vec4(a_vertex, 1.0);

	v_tex.xy = (a_uv0 + worldPos.xz) / 5.0 + sim_time * translation;

	v_tex.zw = a_uv0;

	calculatePositionInShadowSpace(worldPos);

	calculateLOSCoordinates(worldPos.xz);

	gl_Position = transform * worldPos;
}
