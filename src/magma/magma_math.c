/**
 * @file magma_math.c
 * @author Dennis Heinze <dennis.heinze@mailbox.org>
 */
#include "magma_math.h"
#include "assert.h"

void mg_mat4_perspective(fm_mat4_t *out, float fovy, float aspect, float z_near, float z_far)
{
    assert(fovy != 0.0f);
    assert(aspect != 0.0f);
    assert(z_near != z_far);

	float sine, cosine, cotangent, deltaZ;
	float radians = fovy / 2;
	deltaZ = z_near - z_far;
    fm_sincosf(radians, &sine, &cosine);
	cotangent = cosine / sine;

    // Positive Y points downwards in screen space. Therefore we flip it here to make it point upwards in view space.
    *out = (fm_mat4_t){{
        { cotangent / aspect, 0, 0, 0 },
        { 0, -cotangent, 0, 0 },
        { 0, 0, z_far / deltaZ, -1 },
        { 0, 0, z_near * z_far / deltaZ, 0 },
    }};
}

void mg_mat4_ortho(fm_mat4_t *out, float l, float r, float b, float t, float n, float f)
{
    // Positive Y points downwards in screen space. Therefore we flip it here to make it point upwards in view space.
    *out = (fm_mat4_t){{
        { 2.0f/(r-l), 0, 0, 0 },
        { 0, -2.0f/(t-b), 0, 0 },
        { 0, 0, -2.0f/(f-n), 0 },
        { -(r+l)/(r-l), -(t+b)/(t-b), -(f+n)/(f-n), 1 }
    }};
}
