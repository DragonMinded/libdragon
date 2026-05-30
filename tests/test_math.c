#include <fgeom.h>
#include <math.h>

#define ASSERT_EQUAL_MAT4(a, b) ({ \
    for (int i = 0; i < 4; i++) \
    { \
        for (int j = 0; j < 4; j++) \
        { \
            ASSERT_EQUAL_FLOAT(a.m[i][j], b.m[i][j], "m[%d][%d] does not match!", i, j); \
        } \
    } \
})

#define ASSERT_EQUAL_MAT3(a, b) ({ \
    for (int i = 0; i < 3; i++) \
    { \
        for (int j = 0; j < 3; j++) \
        { \
            ASSERT_EQUAL_FLOAT(a.m[i][j], b.m[i][j], "m[%d][%d] does not match!", i, j); \
        } \
    } \
})

#define ASSERT_NEAR_FLOAT(_a, _b, _eps, msg, ...) ({ \
    float a = (_a); \
    float b = (_b); \
    float eps = (_eps); \
    float diff = fabsf(a - b); \
    if (diff > eps) { \
        ERR("ASSERTION FAILED (%s:%d):\n", __FILE__, __LINE__); \
        ERR("|%s - %s| <= %s (%f > %f)\n", #_a, #_b, #_eps, diff, eps); \
        ERR(msg "\n", ##__VA_ARGS__); \
        ctx->result = TEST_FAILED; \
        return; \
    } \
})

#define ASSERT_EQUAL_QUAT_NEAR(a, b, eps) ({ \
    ASSERT_NEAR_FLOAT((a).x, (b).x, eps, "x does not match!"); \
    ASSERT_NEAR_FLOAT((a).y, (b).y, eps, "y does not match!"); \
    ASSERT_NEAR_FLOAT((a).z, (b).z, eps, "z does not match!"); \
    ASSERT_NEAR_FLOAT((a).w, (b).w, eps, "w does not match!"); \
})

void test_mat3_mul_two_identities(TestContext *ctx)
{
    fm_mat3_t a, b, c;

    fm_mat3_identity(&a);
    fm_mat3_identity(&b);
    fm_mat3_mul(&c, &a, &b);

    fm_mat3_t expected;
    fm_mat3_identity(&expected);
    
    ASSERT_EQUAL_MAT3(expected, c);
}

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

void test_mat3_mul_scale_translation(TestContext *ctx)
{
    fm_mat3_t a, b, c;

    fm_mat3_identity(&a);
    fm_mat3_translate(&a, &(fm_vec2_t){{1, 2}});
    fm_mat3_identity(&b);
    fm_mat3_scale(&b, &(fm_vec2_t){{4, 4}});
    fm_mat3_mul(&c, &a, &b);

    fm_mat3_t expected;
    fm_mat3_identity(&expected);
    fm_mat3_scale(&expected, &(fm_vec2_t){{4, 4}});
    fm_mat3_translate(&expected, &(fm_vec2_t){{1, 2}});
    
    ASSERT_EQUAL_MAT3(expected, c);
}

void test_quat_from_euler_zero_identity(TestContext *ctx)
{
    fm_quat_t q;
    float euler[3] = {0.0f, 0.0f, 0.0f};

    fm_quat_from_euler(&q, euler);

    ASSERT_NEAR_FLOAT(q.x, 0.0f, 1e-6f, "identity quaternion x mismatch");
    ASSERT_NEAR_FLOAT(q.y, 0.0f, 1e-6f, "identity quaternion y mismatch");
    ASSERT_NEAR_FLOAT(q.z, 0.0f, 1e-6f, "identity quaternion z mismatch");
    ASSERT_NEAR_FLOAT(q.w, 1.0f, 1e-6f, "identity quaternion w mismatch");
}

void test_quat_from_euler_matches_zyx(TestContext *ctx)
{
    const float test_angles[][3] = {
        { 0.2f, -0.4f, 0.6f },
        { -1.0f, 0.3f, 2.2f },
        { FM_PI * 0.5f, -FM_PI * 0.25f, FM_PI * 0.75f },
    };

    for (int i = 0; i < 3; i++) {
        fm_quat_t qe, qzyx;
        float euler[3] = { test_angles[i][0], test_angles[i][1], test_angles[i][2] };

        fm_quat_from_euler(&qe, euler);
        fm_quat_from_euler_zyx(&qzyx, euler[0], euler[1], euler[2]);

        ASSERT_EQUAL_QUAT_NEAR(qe, qzyx, 1e-5f);
    }
}

void test_quat_slerp_same_quaternion(TestContext *ctx)
{
    fm_quat_t q;
    fm_quat_from_axis_angle(&q, &(fm_vec3_t){{0.0f, 1.0f, 0.0f}}, 0.73f);

    const float t_values[] = { 0.0f, 0.25f, 0.5f, 1.0f };
    for (int i = 0; i < 4; i++) {
        fm_quat_t out;
        fm_quat_slerp(&out, &q, &q, t_values[i]);

        ASSERT(isfinite(out.x) && isfinite(out.y) && isfinite(out.z) && isfinite(out.w),
               "slerp generated non-finite quaternion for t=%f", t_values[i]);
        ASSERT_EQUAL_QUAT_NEAR(out, q, 1e-5f);
    }
}

void test_quat_slerp_near_identical_is_finite(TestContext *ctx)
{
    fm_quat_t a, b, out;
    fm_quat_from_axis_angle(&a, &(fm_vec3_t){{0.0f, 0.0f, 1.0f}}, 1.1f);

    b = (fm_quat_t){{ a.x + 1e-6f, a.y - 2e-6f, a.z + 3e-6f, a.w - 1e-6f }};
    fm_quat_norm(&b, &b);

    fm_quat_slerp(&out, &a, &b, 0.5f);

    ASSERT(isfinite(out.x) && isfinite(out.y) && isfinite(out.z) && isfinite(out.w),
           "slerp generated non-finite quaternion for nearly-identical inputs");
    ASSERT_NEAR_FLOAT(fm_quat_dot(&out, &out), 1.0f, 1e-4f, "slerp output quaternion is not normalized");
}
