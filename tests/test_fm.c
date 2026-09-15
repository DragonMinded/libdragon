#include "fmath.h"
#include <math.h>

void test_fm_truncf(TestContext *ctx) {
    float vals[] = {
        1.0f, 1.2f, 1.5f, 1.7f, -1.0f, -1.2f, -1.5f, -1.7f,
    };
    for (int i = 0; i < 8; i++) {
        float x = vals[i];
        ASSERT_EQUAL_FLOAT(truncf(x), fm_truncf(x), "x=%f", x);
    }
}

void test_fm_ceilf(TestContext *ctx) {
    float vals[] = {
        1.0f, 1.2f, 1.5f, 1.7f, -1.0f, -1.2f, -1.5f, -1.7f,
    };
    for (int i = 0; i < 8; i++) {
        float x = vals[i];
        ASSERT_EQUAL_FLOAT(ceilf(x), fm_ceilf(x), "x=%f", x);
    }
}

void test_fm_floorf(TestContext *ctx) {
    float vals[] = {
        1.0f, 1.2f, 1.5f, 1.7f, -1.0f, -1.2f, -1.5f, -1.7f,
    };
    for (int i = 0; i < 8; i++) {
        float x = vals[i];
        ASSERT_EQUAL_FLOAT(floorf(x), fm_floorf(x), "x=%f", x);
    }
}

void test_fm_roundf(TestContext *ctx) {
    float vals[] = {
        1.0f, 1.2f, 1.5f, 1.7f, -1.0f, -1.2f, -1.5f, -1.7f,
    };
    for (int i = 0; i < 8; i++) {
        float x = vals[i];
        ASSERT_EQUAL_FLOAT(roundf(x), fm_roundf(x), "x=%f", x);
    }
}

void test_fm_fmodf(TestContext *ctx) {
    struct {
        float x, y;
    } vals[] = {
        {0.5f, 1.0f},
        {-0.5f, 1.0f},
        {0.5f, -1.0f},
        {-0.5f, -1.0f},
    };
    for (int i = 0; i < 4; i++) {
        float x = vals[i].x;
        float y = vals[i].y;
        ASSERT_EQUAL_FLOAT(fmodf(x, y), fm_fmodf(x, y), "x=%f y=%f", x, y);
    }
}

void test_fm_wrapf(TestContext *ctx) {
    struct {
        float x, y;
    } vals[] = {
        {0.5f, 1.0f},
        {-0.5f, 1.0f},
        {0.5f, -1.0f},
        {-0.5f, -1.0f},
    };
    for (int i = 0; i < 4; i++) {
        float x = vals[i].x;
        float y = vals[i].y;
        ASSERT_EQUAL_FLOAT(fmodf(fmodf(x, y) + y, y), fm_wrapf(x, y),
                           "x=%f y=%f", x, y);
    }
}

#define FM_TRIG_EPS 1e-3f

void test_fm_sinf(TestContext *ctx) {
    for (float x = -10.0f; x < 10; x += 0.2f) {
        ASSERT(fabsf(sinf(x) - fm_sinf(x)) < FM_TRIG_EPS, "x=%f", x);
    }
}

void test_fm_cosf(TestContext *ctx) {
    for (float x = -10.0f; x < 10; x += 0.2f) {
        ASSERT(fabsf(cosf(x) - fm_cosf(x)) < FM_TRIG_EPS, "x=%f", x);
    }
}

void test_fm_sincosf(TestContext *ctx) {
    for (float x = -10.0f; x < 10; x += 0.2f) {
        float s, c;
        fm_sincosf(x, &s, &c);
        ASSERT(fabsf(sinf(x) - s) < FM_TRIG_EPS, "x=%f", x);
        ASSERT(fabsf(cosf(x) - c) < FM_TRIG_EPS, "x=%f", x);
    }
}

void test_fm_atan2f(TestContext *ctx) {
    struct {
        float x, y;
    } vals[] = {
        {0.5f, 0.5f},   {0.5f, 1.0f},   {1.0f, 10.0f},   {10.0f, 1.0f},
        {-0.5f, 0.5f},  {-0.5f, 1.0f},  {-1.0f, 10.0f},  {-10.0f, 1.0f},
        {0.5f, -0.5f},  {0.5f, -1.0f},  {1.0f, -10.0f},  {10.0f, -1.0f},
        {-0.5f, -0.5f}, {-0.5f, -1.0f}, {-1.0f, -10.0f}, {-10.0f, -1.0f},
    };
    for (int i = 0; i < 16; i++) {
        float x = vals[i].x;
        float y = vals[i].y;
        ASSERT(fabsf(atan2f(x, y) - fm_atan2f(x, y)) < FM_TRIG_EPS, "x=%f y=%f",
               x, y);
    }
}

void test_fm_expf(TestContext *ctx) {
    for (float x = -10.0f; x < 10; x += 0.2f) {
        float ex = expf(x);
        ASSERT(fabsf(ex - fm_expf(x)) / ex < 0.03f, "x=%f", x);
    }
}

void test_fm_lerp_angle(TestContext *ctx) {
    struct {
        float a, b, t, f;
    } vals[] = {
        {0.0f, 1.0f, 0.5f, 0.5f},         {0.0f, 2 * M_PI, 0.5f, 0.0f},
        {0.0f, -2 * M_PI, 0.5f, 0.0f},    {1.0f, 0.0f, 0.5f, 0.5f},
        {2 * M_PI, 0.0f, 0.5f, 2 * M_PI}, {-2 * M_PI, 0.0f, 0.5f, -2 * M_PI},
    };
    for (int i = 0; i < 6; i++) {
        float fa = vals[i].a, fb = vals[i].b, t = vals[i].t, f = vals[i].f;
        ASSERT_EQUAL_FLOAT(fm_lerp_angle(fa, fb, t), f, "a=%f b=%f t=%f", a, b,
                           t);
    }
}

void test_fm_wrap_angle(TestContext *ctx) {
    for (float x = -10.0f; x < 10; x += 0.2f) {
        ASSERT(fabsf(fmodf(fmodf(x, 2 * M_PI) + 2 * M_PI, 2 * M_PI) -
                     fm_wrap_angle(x)) < 1e-6f,
               "x=%f", x);
    }
}
