#include <fgeom.h>

#define ASSERT_EQUAL_MAT4(a, b) ({ \
    for (int i = 0; i < 4; i++) \
    { \
        for (int j = 0; j < 4; j++) \
        { \
            ASSERT_EQUAL_FLOAT(a.m[i][j], b.m[i][j], "m[%d][%d] does not match!", i, j); \
        } \
    } \
})

void test_mat4_mul_two_identities(TestContext *ctx)
{
    fm_mat4_t a, b, c;

    fm_mat4_identity(&a);
    fm_mat4_identity(&b);
    fm_mat4_mul(&c, &a, &b);

    fm_mat4_t expected;
    fm_mat4_identity(&expected);
    
    ASSERT_EQUAL_MAT4(expected, c);
}

void test_mat4_mul_scale_translation(TestContext *ctx)
{
    fm_mat4_t a, b, c;

    fm_mat4_identity(&a);
    fm_mat4_translate(&a, &(fm_vec3_t){{1, 2, 3}});
    fm_mat4_identity(&b);
    fm_mat4_scale(&b, &(fm_vec3_t){{4, 4, 4}});
    fm_mat4_mul(&c, &a, &b);

    fm_mat4_t expected;
    fm_mat4_identity(&expected);
    fm_mat4_scale(&expected, &(fm_vec3_t){{4, 4, 4}});
    fm_mat4_translate(&expected, &(fm_vec3_t){{1, 2, 3}});
    
    ASSERT_EQUAL_MAT4(expected, c);
}
