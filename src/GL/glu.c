#include "GL/glu.h"
#include "gl_internal.h"

void gluLookAt(float eyex, float eyey, float eyez, 
               float centerx, float centery, float centerz,
               float upx, float upy, float upz)
{
    fm_vec3_t eye = {{eyex, eyey, eyez}};
    fm_vec3_t f = {{centerx - eyex, centery - eyey, centerz - eyez}};
    fm_vec3_t u = {{upx, upy, upz}};
    fm_vec3_t s;

    fm_vec3_norm(&f, &f);

    fm_vec3_cross(&s, &f, &u);
    fm_vec3_norm(&s, &s);

    fm_vec3_cross(&u, &s, &f);

    float m[4][4];
    
    m[0][0] = s.x;
    m[0][1] = u.x;
    m[0][2] = -f.x;
    m[0][3] = 0;

    m[1][0] = s.y;
    m[1][1] = u.y;
    m[1][2] = -f.y;
    m[1][3] = 0;

    m[2][0] = s.z;
    m[2][1] = u.z;
    m[2][2] = -f.z;
    m[2][3] = 0;

    m[3][0] = -fm_vec3_dot(&s, &eye);
    m[3][1] = -fm_vec3_dot(&u, &eye);
    m[3][2] = fm_vec3_dot(&f, &eye);
    m[3][3] = 1;

    glMultMatrixf(&m[0][0]);
};

void gluPerspective(float fovy, float aspect, float zNear, float zFar)
{
	float sine, cosine, cotangent, deltaZ;
	float radians = fovy / 2 * (float)M_PI / 180;
	deltaZ = zFar - zNear;
    fm_sincosf(radians, &sine, &cosine);
	if ((deltaZ == 0) || (sine == 0) || (aspect == 0))
	{
		return;
	}
	cotangent = cosine / sine;

	float m[4][4] = {
        {1,0,0,0},
        {0,1,0,0},
        {0,0,1,0},
        {0,0,0,1},
    };
	m[0][0] = cotangent / aspect;
	m[1][1] = cotangent;
	m[2][2] = -(zFar + zNear) / deltaZ;
	m[2][3] = -1;
	m[3][2] = -2 * zNear * zFar / deltaZ;
	m[3][3] = 0;

	glMultMatrixf(&m[0][0]);
}
