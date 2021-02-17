#ifndef LINMATHVK_H_INCLUDED
#define LINMATHVK_H_INCLUDED

#include "linmath/linmath.h"

LINMATH_H_FUNC void mat4x4_look_at_vk(mat4x4 m, vec3 const eye, vec3 const center, vec3 const up)
{
	/* Adapted from linmath.h, flipped y and z coordinate to comply with vulkan's coordinate system */
	vec3 f;
	vec3_sub(f, center, eye);
	vec3_norm(f, f);

	vec3 s;
	vec3_mul_cross(s, f, up);
	vec3_norm(s, s);

	vec3 t;
	vec3_mul_cross(t, s, f);

	m[0][0] =  s[0];
	m[0][1] = -t[0];
	m[0][2] =  f[0];
	m[0][3] =   0.f;

	m[1][0] =  s[1];
	m[1][1] = -t[1];
	m[1][2] =  f[1];
	m[1][3] =   0.f;

	m[2][0] =  s[2];
	m[2][1] = -t[2];
	m[2][2] =  f[2];
	m[2][3] =   0.f;

	m[3][0] =  0.f;
	m[3][1] =  0.f;
	m[3][2] =  0.f;
	m[3][3] =  1.f;

	mat4x4_translate_in_place(m, -eye[0], -eye[1], -eye[2]);
}

LINMATH_H_FUNC void mat4x4_perspective_vk(mat4x4 m, float y_fov, float aspect, float n, float f)
{
	/* NOTE: Degrees are an unhandy unit to work with.
	 * linmath.h uses radians for everything! */
	float const a = 1.f / tanf(y_fov / 2.f);

	m[0][0] = a / aspect;
	m[0][1] = 0.f;
	m[0][2] = 0.f;
	m[0][3] = 0.f;

	m[1][0] = 0.f;
	m[1][1] = a;
	m[1][2] = 0.f;
	m[1][3] = 0.f;

	m[2][0] = 0.f;
	m[2][1] = 0.f;
	m[2][2] = ((f + n) / (f - n));
	m[2][3] = 1.f;

	m[3][0] = 0.f;
	m[3][1] = 0.f;
	m[3][2] = -((2.f * f * n) / (f - n));
	m[3][3] = 0.f;
}

#endif // LINMATHVK_H_INCLUDED
