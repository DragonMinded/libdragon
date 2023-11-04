#ifndef __LIBDRAGON_MODEL64_CATMULL_H
#define __LIBDRAGON_MODEL64_CATMULL_H

static float catmull_calc(float p1, float p2, float p3, float p4, float t)
{
    float a = (-1*p1  +3*p2 -3*p3 + 1*p4) * t*t*t;
    float b = ( 2*p1  -5*p2 +4*p3 - 1*p4) * t*t;
    float c = (  -p1        +  p3)        * t;
    float d = 2*p2;
    return 0.5f * (a+b+c+d);
}

static void catmull_calc_vec(float *p1, float *p2, float *p3, float *p4, float *out, float t, int num_values)
{
    for(int i=0; i<num_values; i++) {
        out[i] = catmull_calc(p1[i], p2[i], p3[i], p4[i], t);
    }
}

#endif
