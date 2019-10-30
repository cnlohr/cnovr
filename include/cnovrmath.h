//Copyright 2013,2016,2019 <>< Charles Lohr licensable under the MIT/X11 or NewBSD licenses.
//Portions of cnovrmath.c were originally written by a variety of authors.  See cnovrmath.c for more details. 

#ifndef _CNOVRMATH_H
#define _CNOVRMATH_H

#include <stdbool.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

// Yes, I know it's kind of arbitrary.
#define DEFAULT_EPSILON 0.0000000001

// For printf
#define PFTHREE(x) (x)[0], (x)[1], (x)[2]
#define PFFOUR(x) (x)[0], (x)[1], (x)[2], (x)[3]

#define CNOVRPI ((FLT)3.141592653589)
#define CNOVRDEGRAD (CNOVRPI/180.)
#define CNOVRRADDEG (180./CNOVRPI)

#define cnovrmathprintf ovrprintf

#define USE_FLOAT

#ifdef USE_FLOAT

#define FLT float
#define FLT_SQRT sqrtf
#define FLT_TAN tanf
#define FLT_SIN sinf
#define FLT_COS cosf
#define FLT_ACOS acosf
#define FLT_ASIN asinf
#define FLT_ATAN2 atan2f
#ifdef __TINYC__
#define FLT_FABS__(x) (((x)<0)?(-(x)):(x))
#else
#define FLT_FABS__ fabsf
#endif
#define FLT_STRTO strtof
#define FLT_FLOOR(x) ((float)((int)(x)))
#else

#define USE_DOUBLE 1
#define FLT double
#define FLT_SQRT sqrt
#define FLT_TAN tan
#define FLT_SIN sin
#define FLT_COS cos
#define FLT_ACOS acos
#define FLT_ASIN asin
#define FLT_ATAN2 atan2
#define FLT_FABS__ fabs
#define FLT_STRTO strtod
#define FLT_FLOOR ((double)((int)(x)))

#endif

#ifdef TCC
#define FLT_FABS(x) (((x) < 0) ? (-(x)) : (x))
#else
#define FLT_FABS FLT_FABS__
#endif

typedef FLT cnovr_quat[4]; // This is the [wxyz] quaternion, in wxyz format.
typedef FLT cnovr_point4d[4];
typedef FLT cnovr_point2d[2];
typedef FLT cnovr_point3d[3];
typedef FLT cnovr_vec3d[3];
typedef FLT cnovr_euler_angle[3];
typedef FLT cnovr_aamag[3];

typedef struct cnovr_pose {
	cnovr_quat Rot;
	cnovr_point3d Pos;
	FLT Scale;
} cnovr_pose;

extern cnovr_quat cnovr_quat_identity;
extern cnovr_pose cnovr_pose_identity;

// General scalar operations
void cnovr_interpolate(FLT *out, int n, const FLT *A, const FLT *B, FLT t);
static inline FLT cnovr_lerp( FLT a, FLT b, FLT f ) { return (a) * ( 1. - (f) ) + (b) * (f); }
static inline FLT cnovr_enforce_range(FLT v, FLT mn, FLT mx) { return ((mn) < (v))?(mn):( ((mx)>(v))?(mx):(v) ); }
static inline FLT cnovr_max(FLT x, FLT y) { return ((x) > (y)) ? (x) : (y); }
static inline FLT cnovr_min(FLT x, FLT y) { return ((x) < (y)) ? (x) : (y); }

void cross3d(FLT *out, const FLT *a, const FLT *b);
void sub3d(FLT *out, const FLT *a, const FLT *b);
void add3d(FLT *out, const FLT *a, const FLT *b);
void mult3d(FLT * out, const FLT *a, const FLT *b);
void scale3d(FLT *out, const FLT *a, FLT scalar);
void invert3d(FLT *out, const FLT *a);
FLT mag3d(const FLT *in);
void normalize3d(FLT *out, const FLT *in);

// out_pts needs to be preallocated with 3 * num_pts FLT's; it doesn't need to be zeroed.
// out_mean can be null if you don't need the mean
void center3d(FLT *out_pts, FLT *out_mean, const FLT *pts, int num_pts);
void mean3d(cnovr_vec3d out, const FLT *pts, int num_pts);
FLT dot3d(const FLT *a, const FLT *b);

// Returns 0 if equal.  If either argument is null, 0 will ALWAYS be returned.
int compare3d(const FLT *a, const FLT *b, FLT epsilon);

void copy3d(FLT *out, const FLT *in);
FLT magnitude3d(const FLT *a);
FLT dist3d(const FLT *a, const FLT *b);
FLT anglebetween3d(FLT *a, FLT *b);
static inline void zero3d( FLT * out ) { out[0] = 0; out[1] = 0; out[2] = 0; }



void rotatearoundaxis(FLT *outvec3, FLT *invec3, FLT *axis, FLT angle);
void angleaxisfrom2vect(FLT *angle, FLT *axis, FLT *src, FLT *dest);
void axisanglefromquat(FLT *angle, FLT *axis, cnovr_quat quat);
// Quaternion things...

static inline void quatidentity( cnovr_quat qout ) { qout[0] = 1; qout[1] = 0; qout[2] = 0; qout[3] = 0; }
FLT quatdist(const cnovr_quat q1, const cnovr_quat q2);
FLT quatdifference(const cnovr_quat q1, const cnovr_quat q2);
void quatset(cnovr_quat q, FLT w, FLT x, FLT y, FLT z);
bool quatiszero(const cnovr_quat q);
void quatsetnone(cnovr_quat q);
void quatcopy(cnovr_quat q, const cnovr_quat qin);
void quatfromeuler(cnovr_quat q, const cnovr_euler_angle euler);
void quattoeuler(cnovr_euler_angle euler, const cnovr_quat q);
void quatfromaxisangle(cnovr_quat q, const FLT *axis, FLT radians);
void quatfromaxisanglemag(cnovr_quat q, const cnovr_aamag axisangle);
void quattoaxisanglemag(cnovr_aamag ang, const cnovr_quat q);
FLT quatmagnitude(const cnovr_quat q);
FLT quatinvsqmagnitude(const cnovr_quat q);
void quatnormalize(cnovr_quat qout, const cnovr_quat qin); // Safe for in to be same as out.
void quattomatrix(FLT *matrix44, const cnovr_quat q);
void quatfrommatrix(cnovr_quat q, const FLT *matrix44);
void quatgetconjugate(cnovr_quat qout, const cnovr_quat qin);
//Fast 180 degree, in place rotate.
void quatrotate180X( cnovr_quat q );
void quatrotate180Y( cnovr_quat q );
void quatrotate180Z( cnovr_quat q );

/***
 * Finds the nearest modulo 2*pi of a given axis angle representation that most neatly matches
 * another axis angle vector.
 *
 * @param out Output vector
 * @param inc Axis angle to find modulo answer for
 * @param match Comparison axis angle meant to match up to. Pass in null to match [0, 0, 0].
 */
void findnearestaxisanglemag(cnovr_aamag out, const cnovr_aamag inc,
											const cnovr_aamag match);
/***
 * Performs conjugation operation wherein we find
 *
 * q = r * v * r^-1
 *
 * This operation is useful to find equivalent rotations between different reference frames.
 * For instance, if 'v' is a particular rotation in frame A, and 'r' represents the translation
 * from frame 'A' to 'B', q represents that same rotation in frame B
 */
void quatconjugateby(cnovr_quat q, const cnovr_quat r, const cnovr_quat v);
void quatgetreciprocal(cnovr_quat qout, const cnovr_quat qin);
/***
 * Find q such that q * q0 = q1; where q0, q1, q are unit quaternions
 */
void quatfind(cnovr_quat q, const cnovr_quat q0, const cnovr_quat q1);
/***
 * Find q such that q0 * q1 = q; where q0, q1, q are unit quaternions
 *
 * same as quat multiply, not piecewise multiply.
 */
void quatrotateabout(cnovr_quat q, const cnovr_quat q0, const cnovr_quat q1);

/***
 * Finds q = qv*t. If 'qv' is thought of an angular velocity, and t is the scalar time span of rotation, q is the
 * arrived at rotation.
 */
void quatmultiplyrotation(cnovr_quat q, const cnovr_quat qv, FLT t);

/***
 * Peicewise scaling
 */
void quatscale(cnovr_quat qout, const cnovr_quat qin, FLT s);
/***
 * Peicewise division by scalar
 */
void quatdivs(cnovr_quat qout, const cnovr_quat qin, FLT s);
/***
 * Peicewise subtraction
 */
void quatsub(cnovr_quat qout, const cnovr_quat a, const cnovr_quat b);
/***
 * Peicewise addition
 */
void quatadd(cnovr_quat qout, const cnovr_quat a, const cnovr_quat b);
FLT quatinnerproduct(const cnovr_quat qa, const cnovr_quat qb);
void quatouterproduct(FLT *outvec3, cnovr_quat qa, cnovr_quat qb);
void quatevenproduct(cnovr_quat q, cnovr_quat qa, cnovr_quat qb);
void quatoddproduct(FLT *outvec3, cnovr_quat qa, cnovr_quat qb);
void quatslerp(cnovr_quat q, const cnovr_quat qa, const cnovr_quat qb, FLT t);
void quatrotatevector(FLT *vec3out, const cnovr_quat quat, const FLT *vec3in);
void eulerrotatevector(FLT *vec3out, const cnovr_euler_angle quat, const FLT *vec3in);
void quatfrom2vectors(cnovr_quat q, const FLT *src, const FLT *dest); //XXX I am suspicious this function is not good.
void eulerfrom2vectors(cnovr_euler_angle q, const FLT *src, const FLT *dest);

// This is the quat equivalent of 'pout = pose * pin' if pose were a 4x4 matrix in homogenous space
void apply_pose_to_point(cnovr_point3d pout, const cnovr_pose *pose, const cnovr_point3d pin);

// This is the quat equivalent of 'pout = lhs_pose * rhs_pose' if poses were a 4x4 matrix in homogenous space
void apply_pose_to_pose(cnovr_pose *pout, const cnovr_pose *lhs_pose, const cnovr_pose *rhs_pose);

void unapply_pose_from_pose(cnovr_pose *pout, const cnovr_pose *thing_youre_looking_at, const cnovr_pose *in_this_coordinate_frame);

// This is the quat equivlant of 'pose_in^-1'; so that ApplyPoseToPose(..., InvertPose(..., pose_in), pose_in) ==
// Identity ( [0, 0, 0], [1, 0, 0, 0] [1] )
// by definition.
static inline void pose_make_identity( cnovr_pose * poseout ) { zero3d( poseout->Pos ); quatsetnone( poseout->Rot ); poseout->Scale = 1;}
void pose_invert(cnovr_pose *poseout, const cnovr_pose *pose_in);
static inline cnovr_pose pose_invert_rtn(const cnovr_pose *pose_in) {cnovr_pose rtn;pose_invert(&rtn, pose_in);return rtn;}

void pose_to_matrix44(FLT *mat44, const cnovr_pose *pose_in);
void matrix44_to_pose(cnovr_pose * poseout, const FLT * m44 );	//HMD43-safe.

void matrix44copy(FLT *mout, const FLT *minm);
void matrix44transpose(FLT *mout, const FLT *minm);

//General Matrix Functions
void matrix44identity( FLT * f );
void matrix44zero( FLT * f );
void matrix44translate( FLT * f, FLT x, FLT y, FLT z );		//Operates ON f
void matrix44scale( FLT * f, FLT x, FLT y, FLT z );			//Operates ON f
void matrix44rotateaa( FLT * f, FLT angle, FLT x, FLT y, FLT z ); 	//Operates ON f
void matrix44rotatequat( FLT * f, FLT * quatwxyz ); 	//Operates ON f
void matrix44rotateea( FLT * f, FLT x, FLT y, FLT z );		//Operates ON f
void matrix44multiply( FLT * fout, FLT * fin1, FLT * fin2 );
void matrix34multiply( FLT * fout, FLT * fin1, FLT * fin2 );
void matrix44print( const FLT * f );

//Specialty Matrix Functions
void matrix44perspective( FLT * out, FLT fovy, FLT aspect, FLT zNear, FLT zFar ); //Sets, NOT OPERATES. (FOVX=degrees)
void matrix44lookat( FLT * m, FLT * eye, FLT * at, FLT * up );	//Operates ON m

//General point functions
#define matrix44pset( f, x, y, z ) { f[0] = x; f[1] = y; f[2] = z; }
void matrix44ptransform( FLT * pout, const FLT * pin, const FLT * f ); //Synonymous with 34p (I.e. doesn't use bottom row)
void matrix44vtransform( FLT * vout, const FLT * vin, const FLT * f ); //Synonymous with 34v (I.e. doesn't use bottom row) (Also doesn't use last column)
void matrix444transform( FLT * kout, const FLT * kin, const FLT * f );

//As one does.
FLT cnovr_perlin( FLT x, FLT y );


#ifdef __cplusplus
}
#endif

#endif
