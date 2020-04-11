
#include <float4.h>
#include <math.h>

float4 f4_set3(float x, float y, float z)
{
    float4 r;
    r.x = x;
    r.y = y;
    r.z = z;
    r.w = 1;
    return r;
}

float4 f4_zero()
{
    float4 r;
    r.x = 0;
    r.y = 0;
    r.z = 0;
    r.w = 1;
    return r;
}

float4 f4_add(float4 a, float4 b)
{
    float4 r;
    r.x = a.x + b.x;
    r.y = a.y + b.y;
    r.z = a.z + b.z;
    r.w = a.w + b.w;
    return r;
};

float4 f4_sub(float4 a, float4 b)
{
    float4 r;
    r.x = a.x - b.x;
    r.y = a.y - b.y;
    r.z = a.z - b.z;
    r.w = a.w - b.w;
    return r;
};

float4 f4_mul(float4 a, float4 b)
{
    float4 r;
    r.x = a.x * b.x;
    r.y = a.y * b.y;
    r.z = a.z * b.z;
    r.w = a.w * b.w;
    return r;
};

float4 f4_mul_f(float4 a, float b)
{
    float4 r;
    r.x = a.x * b;
    r.y = a.y * b;
    r.z = a.z * b;
    r.w = a.w * b;
    return r;
};

float4 f4_div_f(float4 a, float b)
{
    float4 r;
    r.x = a.x / b;
    r.y = a.y / b;
    r.z = a.z / b;
    r.w = a.w / b;
    return r;
};

float4 ang2f4(float ang_x, float ang_y)
{
    float4 r;
    r.x = cosf(ang_x)*cosf(ang_y);
    r.y = sinf(ang_x);
    r.z = cosf(ang_x)*sinf(ang_y);
    r.w = 1;
    return r;
};


float f4_dot3(float4 a, float4 b)
{
    return (a.x*b.x)+(a.y*b.y)+(a.z*b.z); 
}
float f4_dot4(float4 a, float4 b)
{
    return (a.x*b.x)+(a.y*b.y)+(a.z*b.z)+(a.w*b.w); 
}

float f4_distance(float4 a, float4 b)
{
    return f4_length(f4_sub(a,b));
}
float f4_length(float4 v)
{
    return sqrtf( (v.x*v.x)+(v.y*v.y)+(v.z*v.z) );
}
float4 f4_normal(float4 v)
{
    float4 temp;
	float mag=f4_length(v);
	if(mag==0) return f4_zero();
	temp.x=(v.x/mag);
	temp.y=(v.y/mag);
	temp.z=(v.z/mag);
	temp.w=1;

	return temp;
}
float4 f4_cross(float4 a, float4 b)
{
    float4 ret;
	ret.x = (a.y*b.z) - (a.z*b.y);
	ret.y = (a.z*b.x) - (a.x*b.z);
	ret.z = (a.x*b.y) - (a.y*b.x);
	ret.w = 1;
	return ret;
}

float4 f4_calcnormal(float4 v1, float4 v2, float4 v3)
{    
    float4 ret;
	float4 rv1,rv2;

	rv1.x = v1.x - v2.x;
	rv1.y = v1.y - v2.y;
	rv1.z = v1.z - v2.z;
	rv1 = f4_normal(rv1);
	rv2.x = v2.x - v3.x;
	rv2.y = v2.y - v3.y;
	rv2.z = v2.z - v3.z;
	rv2 = f4_normal(rv2);
	
	ret = f4_cross(rv1,rv2);
	
	return f4_normal(ret);
}

float4 f4_create_plane(float4 normal, float4 position)
{
    float4 ret = normal;
    ret.w = -f4_dot3(normal,position);
    return ret;
}


float f4_plane_test(float4 plane, float4 point)
{
    return (plane.x*point.x + plane.y*point.y + plane.z*point.z) + plane.w;
}


float4 f4_persp(float4 v)
{
    float4 r;
    float _w = 1.0f;
    if(v.w > 0.001f) _w = 1.0f / v.w;
    r.x = v.x * _w;
    r.y = v.y * _w;
    r.z = v.z * _w;
    r.w = _w;
    return r;
}