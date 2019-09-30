// Copyright 2009,2013,2016,2018,2019 <>< C. N. Lohr licensable under the MIT/X11 or NewBSD licenses.
// Portions copyright under the same license to Adam Lowman, Josua Allen and Charles Lohr

#include <cnovrmath.h>
#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <string.h>
#include <cnovr.h>
#include <stdio.h>

void cross3d(FLT *out, const FLT *a, const FLT *b) {
	FLT o0 = a[1] * b[2] - a[2] * b[1];
	FLT o1 = a[2] * b[0] - a[0] * b[2];
	out[2] = a[0] * b[1] - a[1] * b[0];
	out[1] = o1;
	out[0] = o0;
}

void sub3d(FLT *out, const FLT *a, const FLT *b) {
	out[0] = a[0] - b[0];
	out[1] = a[1] - b[1];
	out[2] = a[2] - b[2];
}

void add3d(FLT *out, const FLT *a, const FLT *b) {
	out[0] = a[0] + b[0];
	out[1] = a[1] + b[1];
	out[2] = a[2] + b[2];
}

void mult3d(FLT * out, const FLT *a, const FLT *b)
{
	out[0] = a[0] * b[0];
	out[1] = a[1] * b[1];
	out[2] = a[2] * b[2];
}

void invert3d(FLT *out, const FLT *a) {
	out[0] = 1. / a[0];
	out[1] = 1. / a[1];
	out[2] = 1. / a[2];
}
void scale3d(FLT *out, const FLT *a, FLT scalar) {
	out[0] = a[0] * scalar;
	out[1] = a[1] * scalar;
	out[2] = a[2] * scalar;
}

FLT mag3d(const FLT *in) { return FLT_SQRT(in[0] * in[0] + in[1] * in[1] + in[2] * in[2]); }

void normalize3d(FLT *out, const FLT *in) {
	FLT r = ((FLT)1.) / mag3d(in);
	out[0] = in[0] * r;
	out[1] = in[1] * r;
	out[2] = in[2] * r;
}

void cnovr_interpolate(FLT *out, int n, const FLT *A, const FLT *B, FLT t) {
	for (int i = 0; i < n; i++) {
		out[i] = A[i] + (B[i] - A[i]) * t;
	}
}

FLT dot3d(const FLT *a, const FLT *b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }

int compare3d(const FLT *a, const FLT *b, FLT epsilon) {
	if (!a || !b)
		return 0;
	if (a[2] - b[2] > epsilon)
		return 1;
	if (b[2] - a[2] > epsilon)
		return -1;
	if (a[1] - b[1] > epsilon)
		return 1;
	if (b[1] - a[1] > epsilon)
		return -1;
	if (a[0] - b[0] > epsilon)
		return 1;
	if (b[0] - a[0] > epsilon)
		return -1;
	return 0;
}

void copy3d(FLT *out, const FLT *in) {
	out[0] = in[0];
	out[1] = in[1];
	out[2] = in[2];
}

FLT magnitude3d(const FLT *a) { return FLT_SQRT(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]); }
FLT dist3d(const FLT *a, const FLT *b) {
	cnovr_point3d tmp;
	sub3d(tmp, a, b);
	return magnitude3d(tmp);
}

FLT anglebetween3d(FLT *a, FLT *b) {
	FLT an[3];
	FLT bn[3];
	normalize3d(an, a);
	normalize3d(bn, b);
	FLT dot = dot3d(an, bn);
	if (dot < -0.9999999)
		return CNOVRPI;
	if (dot > 0.9999999)
		return 0;
	return FLT_ACOS(dot);
}

void center3d(FLT *out_pts, FLT *out_mean, const FLT *pts, int num_pts) {
	FLT center[3];
	if (out_mean == 0)
		out_mean = center;
	mean3d(out_mean, pts, num_pts);

	for (int i = 0; i < num_pts; i++) {
		for (int j = 0; j < 3; j++) {
			out_pts[i * 3 + j] = pts[i * 3 + j] - out_mean[j];
		}
	}
}

void mean3d(cnovr_vec3d out, const FLT *pts, int num_pts) {
	for (int i = 0; i < 3; i++) {
		out[i] = 0;
	}

	for (int i = 0; i < num_pts; i++) {
		for (int j = 0; j < 3; j++) {
			out[j] += pts[i * 3 + j];
		}
	}

	for (int j = 0; j < 3; j++) {
		out[j] = out[j] / (FLT)num_pts;
	}
}

// algorithm found here: http://inside.mines.edu/fs_home/gmurray/ArbitraryAxisRotation/
void rotatearoundaxis(FLT *outvec3, FLT *invec3, FLT *axis, FLT angle) {
	// TODO: this really should be external.
	normalize3d(axis, axis);

	FLT s = FLT_SIN(angle);
	FLT c = FLT_COS(angle);

	FLT u = axis[0];
	FLT v = axis[1];
	FLT w = axis[2];

	FLT x = invec3[0];
	FLT y = invec3[1];
	FLT z = invec3[2];

	outvec3[0] = u * (u * x + v * y + w * z) * (1 - c) + x * c + (-w * y + v * z) * s;
	outvec3[1] = v * (u * x + v * y + w * z) * (1 - c) + y * c + (w * x - u * z) * s;
	outvec3[2] = w * (u * x + v * y + w * z) * (1 - c) + z * c + (-v * x + u * y) * s;
}

void angleaxisfrom2vect(FLT *angle, FLT *axis, FLT *src, FLT *dest) {
	FLT v0[3];
	FLT v1[3];
	normalize3d(v0, src);
	normalize3d(v1, dest);

	FLT d = dot3d(v0, v1); // v0.dotProduct(v1);

	// If dot == 1, vectors are the same
	// If dot == -1, vectors are opposite
	if (FLT_FABS(d - 1) < DEFAULT_EPSILON) {
		axis[0] = 0;
		axis[1] = 1;
		axis[2] = 0;
		*angle = 0;
		return;
	} else if (FLT_FABS(d + 1) < DEFAULT_EPSILON) {
		axis[0] = 0;
		axis[1] = 1;
		axis[2] = 0;
		*angle = CNOVRPI;
		return;
	}

	FLT v0Len = magnitude3d(v0);
	FLT v1Len = magnitude3d(v1);

	*angle = FLT_ACOS(d / (v0Len * v1Len));

	// cross3d(c, v0, v1);
	cross3d(axis, v1, v0);
}

void axisanglefromquat(FLT *angle, FLT *axis, FLT *q) {
	// this way might be fine, too.
	// FLT dist = FLT_SQRT((q[1] * q[1]) + (q[2] * q[2]) + (q[3] * q[3]));
	//
	//*angle = 2 * FLT_ATAN2(dist, q[0]);

	// axis[0] = q[1] / dist;
	// axis[1] = q[2] / dist;
	// axis[2] = q[3] / dist;

	// Good mathematical foundation for this algorithm found here:
	// http://www.euclideanspace.com/maths/geometry/rotations/conversions/quaternionToAngle/index.htm

	FLT tmp[4] = {q[0], q[1], q[2], q[3]};

	quatnormalize(tmp, q);

	if (FLT_FABS(q[0] - 1) < FLT_EPSILON) {
		// we have a degenerate case where we're rotating approx. 0 degrees
		*angle = 0;
		axis[0] = 1;
		axis[1] = 0;
		axis[2] = 0;
		return;
	}

	axis[0] = tmp[1] / FLT_SQRT(1 - (tmp[0] * tmp[0]));
	axis[1] = tmp[2] / FLT_SQRT(1 - (tmp[0] * tmp[0]));
	axis[2] = tmp[3] / FLT_SQRT(1 - (tmp[0] * tmp[0]));

	*angle = 2 * FLT_ACOS(tmp[0]);
}

/////////////////////////////////////QUATERNIONS//////////////////////////////////////////
// Originally from Mercury (Copyright (C) 2009 by Joshua Allen, Charles Lohr, Adam Lowman)
// Under the mit/X11 license.

FLT quatdifference(const cnovr_quat q1, const cnovr_quat q2) {
	cnovr_quat diff;
	quatfind(diff, q1, q2);
	return 1. - diff[0];
}
FLT quatdist(const FLT *q1, const FLT *q2) {
	FLT rtn = 0;
	for (int i = 0; i < 4; i++) {
		rtn += q1[i] * q2[i];
	}
	rtn = cnovr_max(1., cnovr_min(-1, rtn));
	return 2 * FLT_ACOS(FLT_FABS(rtn));
}

bool quatiszero(const cnovr_quat q) { return q[0] == 0 && q[1] == 0 && q[2] == 0 && q[3] == 0; }
void quatset(cnovr_quat q, FLT w, FLT x, FLT y, FLT z) {
	q[0] = w;
	q[1] = x;
	q[2] = y;
	q[3] = z;
}

void quatsetnone(cnovr_quat q) {
	q[0] = 1;
	q[1] = 0;
	q[2] = 0;
	q[3] = 0;
}

void quatcopy(cnovr_quat qout, const cnovr_quat qin) {
	qout[0] = qin[0];
	qout[1] = qin[1];
	qout[2] = qin[2];
	qout[3] = qin[3];
}

void quatfromeuler(cnovr_quat q, const cnovr_euler_angle euler) {
	FLT X = euler[0] / 2.0f; // roll
	FLT Y = euler[1] / 2.0f; // pitch
	FLT Z = euler[2] / 2.0f; // yaw

	FLT cx = FLT_COS(X);
	FLT sx = FLT_SIN(X);
	FLT cy = FLT_COS(Y);
	FLT sy = FLT_SIN(Y);
	FLT cz = FLT_COS(Z);
	FLT sz = FLT_SIN(Z);

	// Correct according to
	// http://en.wikipedia.org/wiki/Conversion_between_MQuaternions_and_Euler_angles
	q[0] = cx * cy * cz + sx * sy * sz; // q1
	q[1] = sx * cy * cz - cx * sy * sz; // q2
	q[2] = cx * sy * cz + sx * cy * sz; // q3
	q[3] = cx * cy * sz - sx * sy * cz; // q4
	quatnormalize(q, q);
}

void quattoeuler(cnovr_euler_angle euler, const cnovr_quat q) {
	// According to http://en.wikipedia.org/wiki/Conversion_between_quaternions_and_Euler_angles (Oct 26, 2009)
	euler[0] = FLT_ATAN2(2 * (q[0] * q[1] + q[2] * q[3]), 1 - 2 * (q[1] * q[1] + q[2] * q[2]));
	euler[1] = FLT_ASIN(2 * (q[0] * q[2] - q[3] * q[1]));
	euler[2] = FLT_ATAN2(2 * (q[0] * q[3] + q[1] * q[2]), 1 - 2 * (q[2] * q[2] + q[3] * q[3]));
}

void quatfromaxisanglemag(cnovr_quat q, const cnovr_aamag axisangle) {
	FLT radians = mag3d(axisangle);

	if (radians == 0.0) {
		quatcopy(q, cnovr_quat_identity);
	} else {
		FLT sn = FLT_SIN(radians / 2.0f);
		q[0] = FLT_COS(radians / 2.0f);
		q[1] = sn * axisangle[0] / radians;
		q[2] = sn * axisangle[1] / radians;
		q[3] = sn * axisangle[2] / radians;
		quatnormalize(q, q);
	}
}
void quatfromaxisangle(cnovr_quat q, const FLT *axis, FLT radians) {
	FLT v[3];
	normalize3d(v, axis);

	FLT sn = FLT_SIN(radians / 2.0f);
	q[0] = FLT_COS(radians / 2.0f);
	q[1] = sn * v[0];
	q[2] = sn * v[1];
	q[3] = sn * v[2];

	quatnormalize(q, q);
}

FLT quatmagnitude(const cnovr_quat q) {
	return FLT_SQRT((q[0] * q[0]) + (q[1] * q[1]) + (q[2] * q[2]) + (q[3] * q[3]));
}

FLT quatinvsqmagnitude(const cnovr_quat q) {
	return ((FLT)1.) / FLT_SQRT((q[0] * q[0]) + (q[1] * q[1]) + (q[2] * q[2]) + (q[3] * q[3]));
}

void quatnormalize(cnovr_quat qout, const cnovr_quat qin) {
	FLT norm = quatmagnitude(qin);
	quatdivs(qout, qin, norm);
}

void quattomatrix(FLT *matrix44, const cnovr_quat qin) {
	FLT q[4];
	quatnormalize(q, qin);

	// Reduced calculation for speed
	FLT xx = 2 * q[1] * q[1];
	FLT xy = 2 * q[1] * q[2];
	FLT xz = 2 * q[1] * q[3];
	FLT xw = 2 * q[1] * q[0];

	FLT yy = 2 * q[2] * q[2];
	FLT yz = 2 * q[2] * q[3];
	FLT yw = 2 * q[2] * q[0];

	FLT zz = 2 * q[3] * q[3];
	FLT zw = 2 * q[3] * q[0];

	// opengl major
	matrix44[0] = 1 - yy - zz;
	matrix44[1] = xy - zw;
	matrix44[2] = xz + yw;
	matrix44[3] = 0;

	matrix44[4] = xy + zw;
	matrix44[5] = 1 - xx - zz;
	matrix44[6] = yz - xw;
	matrix44[7] = 0;

	matrix44[8] = xz - yw;
	matrix44[9] = yz + xw;
	matrix44[10] = 1 - xx - yy;
	matrix44[11] = 0;

	matrix44[12] = 0;
	matrix44[13] = 0;
	matrix44[14] = 0;
	matrix44[15] = 1;
}


void quatfrommatrix(cnovr_quat q, const FLT *matrix44) {
	// Algorithm from http://www.euclideanspace.com/maths/geometry/rotations/conversions/matrixToQuaternion/
	FLT tr = matrix44[0] + matrix44[5] + matrix44[10];

	if (tr > 0) {
		FLT S = FLT_SQRT(tr + 1.0) * 2.; // S=4*qw
		q[0] = 0.25f * S;
		q[1] = (matrix44[9] - matrix44[6]) / S;
		q[2] = (matrix44[2] - matrix44[8]) / S;
		q[3] = (matrix44[4] - matrix44[1]) / S;
	} else if ((matrix44[0] > matrix44[5]) && (matrix44[0] > matrix44[10])) {
		FLT S = FLT_SQRT(1.0 + matrix44[0] - matrix44[5] - matrix44[10]) * 2.; // S=4*qx
		q[0] = (matrix44[9] - matrix44[6]) / S;
		q[1] = 0.25f * S;
		q[2] = (matrix44[1] + matrix44[4]) / S;
		q[3] = (matrix44[2] + matrix44[8]) / S;
	} else if (matrix44[5] > matrix44[10]) {
		FLT S = FLT_SQRT(1.0 + matrix44[5] - matrix44[0] - matrix44[10]) * 2.; // S=4*qy
		q[0] = (matrix44[2] - matrix44[8]) / S;
		q[1] = (matrix44[1] + matrix44[4]) / S;
		q[2] = 0.25f * S;
		q[3] = (matrix44[6] + matrix44[9]) / S;
	} else {
		FLT S = FLT_SQRT(1.0 + matrix44[10] - matrix44[0] - matrix44[5]) * 2.; // S=4*qz
		q[0] = (matrix44[4] - matrix44[1]) / S;
		q[1] = (matrix44[2] + matrix44[8]) / S;
		q[2] = (matrix44[6] + matrix44[9]) / S;
		q[3] = 0.25 * S;
	}
}

void quatgetconjugate(cnovr_quat qout, const cnovr_quat qin) {
	qout[0] = qin[0];
	qout[1] = -qin[1];
	qout[2] = -qin[2];
	qout[3] = -qin[3];
}

void quatgetreciprocal(cnovr_quat qout, const cnovr_quat qin) {
	FLT m = quatinvsqmagnitude(qin);
	quatgetconjugate(qout, qin);
	quatscale(qout, qout, m);
}

void quatconjugateby(cnovr_quat q, const cnovr_quat r, const cnovr_quat v) {
	cnovr_quat ir;
	quatgetconjugate(ir, r);
	quatrotateabout(q, ir, v);
	quatrotateabout(q, q, r);
	quatnormalize(q, q);
}
void quatsub(cnovr_quat qout, const FLT *a, const FLT *b) {
	qout[0] = a[0] - b[0];
	qout[1] = a[1] - b[1];
	qout[2] = a[2] - b[2];
	qout[3] = a[3] - b[3];
}

void quatadd(cnovr_quat qout, const FLT *a, const FLT *b) {
	qout[0] = a[0] + b[0];
	qout[1] = a[1] + b[1];
	qout[2] = a[2] + b[2];
	qout[3] = a[3] + b[3];
}

void quatfind(cnovr_quat q, const cnovr_quat q0, const cnovr_quat q1) {
	cnovr_quat iq0;
	quatgetconjugate(iq0, q0);
	quatrotateabout(q, q1, iq0);
}

void quatrotateabout(cnovr_quat qout, const cnovr_quat q1, const cnovr_quat q2) {
	// NOTE: Does not normalize
	cnovr_quat rtn;
	FLT *p = qout;
	bool aliased = q1 == qout || q2 == qout;
	if (aliased) {
		p = rtn;
	}

	p[0] = (q1[0] * q2[0]) - (q1[1] * q2[1]) - (q1[2] * q2[2]) - (q1[3] * q2[3]);
	p[1] = (q1[0] * q2[1]) + (q1[1] * q2[0]) + (q1[2] * q2[3]) - (q1[3] * q2[2]);
	p[2] = (q1[0] * q2[2]) - (q1[1] * q2[3]) + (q1[2] * q2[0]) + (q1[3] * q2[1]);
	p[3] = (q1[0] * q2[3]) + (q1[1] * q2[2]) - (q1[2] * q2[1]) + (q1[3] * q2[0]);

	if (aliased) {
		quatcopy(qout, rtn);
	}
}

void findnearestaxisanglemag(cnovr_aamag out, const cnovr_aamag inc,
									const cnovr_aamag match) {
	FLT norm_match = match ? mag3d(match) : 0;
	FLT norm_inc = mag3d(inc);
	FLT new_norm_inc = norm_inc;

	while (new_norm_inc > norm_match + M_PI) {
		new_norm_inc -= 2 * M_PI;
	}
	while (new_norm_inc + M_PI < norm_match) {
		new_norm_inc += 2 * M_PI;
	}
	scale3d(out, inc, new_norm_inc / norm_inc);
}

void quattoaxisanglemag(cnovr_aamag ang, const cnovr_quat q) {
	FLT axis_len = FLT_SQRT(q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
	FLT angle = 2. * FLT_ATAN2(axis_len, q[0]);
	scale3d(ang, q + 1, axis_len == 0 ? 0 : angle / axis_len);
}

void quatmultiplyrotation(cnovr_quat q, const cnovr_quat qv, FLT t) {
	cnovr_aamag aa;
	quattoaxisanglemag(aa, qv);
	scale3d(aa, aa, t);
	quatfromaxisanglemag(q, aa);
}

void quatscale(cnovr_quat qout, const cnovr_quat qin, FLT s) {
	qout[0] = qin[0] * s;
	qout[1] = qin[1] * s;
	qout[2] = qin[2] * s;
	qout[3] = qin[3] * s;
}

void quatdivs(cnovr_quat qout, const cnovr_quat qin, FLT s) {
	qout[0] = qin[0] / s;
	qout[1] = qin[1] / s;
	qout[2] = qin[2] / s;
	qout[3] = qin[3] / s;
}

FLT quatinnerproduct(const cnovr_quat qa, const cnovr_quat qb) {
	return (qa[0] * qb[0]) + (qa[1] * qb[1]) + (qa[2] * qb[2]) + (qa[3] * qb[3]);
}

void quatouterproduct(FLT *outvec3, cnovr_quat qa, cnovr_quat qb) {
	outvec3[0] = (qa[0] * qb[1]) - (qa[1] * qb[0]) - (qa[2] * qb[3]) + (qa[3] * qb[2]);
	outvec3[1] = (qa[0] * qb[2]) + (qa[1] * qb[3]) - (qa[2] * qb[0]) - (qa[3] * qb[1]);
	outvec3[2] = (qa[0] * qb[3]) - (qa[1] * qb[2]) + (qa[2] * qb[1]) - (qa[3] * qb[0]);
}

void quatevenproduct(cnovr_quat q, cnovr_quat qa, cnovr_quat qb) {
	q[0] = (qa[0] * qb[0]) - (qa[1] * qb[1]) - (qa[2] * qb[2]) - (qa[3] * qb[3]);
	q[1] = (qa[0] * qb[1]) + (qa[1] * qb[0]);
	q[2] = (qa[0] * qb[2]) + (qa[2] * qb[0]);
	q[3] = (qa[0] * qb[3]) + (qa[3] * qb[0]);
}

void quatoddproduct(FLT *outvec3, cnovr_quat qa, cnovr_quat qb) {
	outvec3[0] = (qa[2] * qb[3]) - (qa[3] * qb[2]);
	outvec3[1] = (qa[3] * qb[1]) - (qa[1] * qb[3]);
	outvec3[2] = (qa[1] * qb[2]) - (qa[2] * qb[1]);
}

void quatslerp(cnovr_quat q, const cnovr_quat qa, const cnovr_quat qb, FLT t) {
	FLT an[4];
	FLT bn[4];
	quatnormalize(an, qa);
	quatnormalize(bn, qb);

	// Switched implementation to match https://en.wikipedia.org/wiki/Slerp

    // Compute the cosine of the angle between the two vectors.
    double dot = dot3d(qa, qb);

	cnovr_quat nqb;

    // If the dot product is negative, slerp won't take
    // the shorter path. Note that v1 and -v1 are equivalent when
    // the negation is applied to all four components. Fix by 
    // reversing one quaternion.
    if (dot < 0.0f) {
		quatscale( nqb, qb, -1 );
        dot = -dot;
    } 
	else
	{
		quatscale( nqb, qb, 1 );
	}

    const double DOT_THRESHOLD = 0.9995;
    if (dot > DOT_THRESHOLD) {
        // If the inputs are too close for comfort, linearly interpolate
        // and normalize the result.

        //Quaternion result = v0 + t*(v1 - v0);
		cnovr_quat tmp;
		quatsub( tmp, nqb, qa );
		quatscale( tmp, tmp, t );
		quatadd( q, qa, tmp );
        quatnormalize( q, q );
		return;
    }

    // Since dot is in range [0, DOT_THRESHOLD], acos is safe
    double theta_0 = FLT_ACOS(dot);        // theta_0 = angle between input vectors
    double theta = theta_0*t;              // theta = angle between v0 and result
    double sin_theta = FLT_SIN(theta);     // compute this value only once
    double sin_theta_0 = FLT_SIN(theta_0); // compute this value only once

	double s0 = cos( theta ) - dot * sin_theta / sin_theta_0;  // == sin(theta_0 - theta) / sin(theta_0)
	double s1 = sin_theta / sin_theta_0;

	cnovr_quat aside;
	cnovr_quat bside;
	quatscale(bside, nqb, s1 );
	quatscale(aside, qa, s0 );
	quatadd(q, aside, bside);
	quatnormalize(q, q);
}

void quatrotate180X( cnovr_quat q )
{
	float tmp;
	tmp = q[0]; q[0] = -q[1]; q[1] = tmp;
	tmp = q[3]; q[3] = -q[2]; q[2] = tmp;
}


void quatrotate180Y( cnovr_quat q )
{
	float tmp;
	tmp = q[0]; q[0] = -q[2]; q[2] = tmp;
	tmp = q[1]; q[1] = -q[3]; q[3] = tmp;
}

void quatrotate180Z( cnovr_quat q )
{
	float tmp;
	tmp = q[0]; q[0] = -q[3]; q[3] = tmp;
	tmp = q[2]; q[2] = -q[1]; q[1] = tmp;
}

void eulerrotatevector(FLT *vec3out, const cnovr_euler_angle eulerAngle, const FLT *vec3in) {
	cnovr_quat q;
	quatfromeuler(q, eulerAngle);
	quatrotatevector(vec3out, q, vec3in);
}
void quatrotatevector(FLT *vec3out, const cnovr_quat quat, const FLT *vec3in) {
	// See: http://www.geeks3d.com/20141201/how-to-rotate-a-vertex-by-a-quaternion-in-glsl/

	FLT tmp[3];
	FLT tmp2[3];
	cross3d(tmp, &quat[1], vec3in);
	tmp[0] += vec3in[0] * quat[0];
	tmp[1] += vec3in[1] * quat[0];
	tmp[2] += vec3in[2] * quat[0];
	cross3d(tmp2, &quat[1], tmp);
	vec3out[0] = vec3in[0] + 2 * tmp2[0];
	vec3out[1] = vec3in[1] + 2 * tmp2[1];
	vec3out[2] = vec3in[2] + 2 * tmp2[2];
}


// This function based on code from Object-oriented Graphics Rendering Engine
// Copyright(c) 2000 - 2012 Torus Knot Software Ltd
// under MIT license
// http://www.ogre3d.org/docs/api/1.9/_ogre_vector3_8h_source.html

void eulerfrom2vectors(cnovr_euler_angle euler, const FLT *src, const FLT *dest) {
	cnovr_quat q;
	quatfrom2vectors(q, src, dest);
	quattoeuler(euler, q);
}
/** Gets the shortest arc quaternion to rotate this vector to the destination
vector.
@remarks
If you call this with a dest vector that is close to the inverse
of this vector, we will rotate 180 degrees around a generated axis if
since in this case ANY axis of rotation is valid.
*/
void quatfrom2vectors(FLT *q, const FLT *src, const FLT *dest) {
	// Based on Stan Melax's article in Game Programming Gems

	// Copy, since cannot modify local
	FLT v0[3];
	FLT v1[3];
	normalize3d(v0, src);
	normalize3d(v1, dest);

	FLT d = dot3d(v0, v1); // v0.dotProduct(v1);
	// If dot == 1, vectors are the same
	if (d >= 1.0f) {
		quatsetnone(q);
		return;
	}
	if (d < (1e-6f - 1.0f)) {
		// Generate an axis
		FLT unitX[3] = {1, 0, 0};
		FLT unitY[3] = {0, 1, 0};

		FLT axis[3];
		cross3d(axis, unitX, src);												  // pick an angle
		if ((axis[0] < 1.0e-35f) && (axis[1] < 1.0e-35f) && (axis[2] < 1.0e-35f)) // pick another if colinear
		{
			cross3d(axis, unitY, src);
		}
		normalize3d(axis, axis);
		quatfromaxisangle(q, axis, CNOVRPI);
	} else {
		FLT s = FLT_SQRT((1 + d) * 2);
		FLT invs = 1 / s;

		FLT c[3];
		cross3d(c, v0, v1);

		q[0] = s * 0.5f;
		q[1] = c[0] * invs;
		q[2] = c[1] * invs;
		q[3] = c[2] * invs;

		quatnormalize(q, q);
	}
}

void matrix44copy(FLT *mout, const FLT *minm) { memcpy(mout, minm, sizeof(FLT) * 16); }

void matrix44transpose(FLT *mout, const FLT *minm) {
	mout[0] = minm[0];
	mout[1] = minm[4];
	mout[2] = minm[8];
	mout[3] = minm[12];

	mout[4] = minm[1];
	mout[5] = minm[5];
	mout[6] = minm[9];
	mout[7] = minm[13];

	mout[8] = minm[2];
	mout[9] = minm[6];
	mout[10] = minm[10];
	mout[11] = minm[14];

	mout[12] = minm[3];
	mout[13] = minm[7];
	mout[14] = minm[11];
	mout[15] = minm[15];
}

void apply_pose_to_point(cnovr_point3d pout, const cnovr_pose *pose, const cnovr_point3d pin) {
	cnovr_point3d tmp;
	quatrotatevector(tmp, pose->Rot, pin);
	scale3d( tmp, tmp, pose->Scale );
	add3d( pout, tmp, pose->Pos);
}

void apply_pose_to_point_revorder(cnovr_point3d pout, const cnovr_pose *pose, const cnovr_point3d pin) {
	cnovr_point3d tmp;
	add3d( pout, tmp, pose->Pos);
	scale3d( tmp, tmp, pose->Scale );
	quatrotatevector(tmp, pose->Rot, pin);
}

void apply_pose_to_pose(cnovr_pose *pout, const cnovr_pose *lhs_pose, const cnovr_pose *rhs_pose) {
	apply_pose_to_point(pout->Pos, lhs_pose, rhs_pose->Pos);
	scale3d( pout->Pos, pout->Pos, rhs_pose->Scale );
	quatrotateabout(pout->Rot, lhs_pose->Rot, rhs_pose->Rot);
	pout->Scale = lhs_pose->Scale * rhs_pose->Scale;
}

void unapply_pose_from_pose(cnovr_pose *poseout, const cnovr_pose *in_this_coordinate_frame, const cnovr_pose *thing_youre_looking_at)
{
	cnovr_pose inv;
	//This would be the opposite of apply_pose_to_pose( orig, return of this, in_this_coordinate_frame );
	//Rules:
	//  (1) Assume "in_this_coordinate_frame" is 1, always.  This would be applied first, but because we ignore it, it's OK.
	//  (2) Unrotate quaternion.
	//  (3) Unapply pose from point.
	//No need to do any scaling difference.

	poseout->Scale = 1;
	quatgetreciprocal(inv.Rot, in_this_coordinate_frame->Rot); 
	quatrotateabout(poseout->Rot, inv.Rot, thing_youre_looking_at->Rot);
	sub3d( inv.Pos, thing_youre_looking_at->Pos, in_this_coordinate_frame->Pos );
	quatrotatevector(inv.Pos, inv.Rot, inv.Pos);
	apply_pose_to_point(poseout->Pos, &inv, thing_youre_looking_at->Pos );
}

void pose_invert(cnovr_pose *poseout, const cnovr_pose *pose) {
	quatgetreciprocal(poseout->Rot, pose->Rot);
	quatrotatevector(poseout->Pos, poseout->Rot, pose->Pos);
	poseout->Scale = 1. / pose->Scale;
	scale3d(poseout->Pos, poseout->Pos, -poseout->Scale);
}

void pose_to_matrix44(FLT *matrix44, const cnovr_pose *pose_in) {
	quattomatrix(matrix44, pose_in->Rot);
	scale3d( matrix44 + 0, matrix44 + 0, pose_in->Scale );
	scale3d( matrix44 + 4, matrix44 + 4, pose_in->Scale );
	scale3d( matrix44 + 8, matrix44 + 8, pose_in->Scale );
	matrix44[3] = pose_in->Pos[0];
	matrix44[7] = pose_in->Pos[1];
	matrix44[11] = pose_in->Pos[2];
	zero3d( matrix44 + 12 );
	matrix44[15] = 1;
}

void matrix44_to_pose(cnovr_pose * pose_out, const FLT * mat44 )
{
	quatfrommatrix(pose_out->Rot, mat44);
	pose_out->Pos[0] = mat44[3];
	pose_out->Pos[1] = mat44[7];
	pose_out->Pos[2] = mat44[11];
	pose_out->Scale = ( mag3d( mat44 + 0 ) + mag3d( mat44 + 4 ) + mag3d( mat44 + 8 ) ) / 3.0;
}



#define m00 0
#define m01 1
#define m02 2
#define m03 3
#define m10 4
#define m11 5
#define m12 6
#define m13 7
#define m20 8
#define m21 9
#define m22 10
#define m23 11
#define m30 12
#define m31 13
#define m32 14
#define m33 15

void matrix44identity( FLT * f )
{
	f[m00] = 1; f[m01] = 0; f[m02] = 0; f[m03] = 0;
	f[m10] = 0; f[m11] = 1; f[m12] = 0; f[m13] = 0;
	f[m20] = 0; f[m21] = 0; f[m22] = 1; f[m23] = 0;
	f[m30] = 0; f[m31] = 0; f[m32] = 0; f[m33] = 1;
}

void matrix44zero( FLT * f )
{
	f[m00] = 0; f[m01] = 0; f[m02] = 0; f[m03] = 0;
	f[m10] = 0; f[m11] = 0; f[m12] = 0; f[m13] = 0;
	f[m20] = 0; f[m21] = 0; f[m22] = 0; f[m23] = 0;
	f[m30] = 0; f[m31] = 0; f[m32] = 0; f[m33] = 0;
}

void matrix44translate( FLT * f, FLT x, FLT y, FLT z )
{
	f[m03] += x;
	f[m13] += y;
	f[m23] += z;
}

void matrix44scale( FLT * f, FLT x, FLT y, FLT z )
{
	FLT ftmp[16];
	matrix44identity(ftmp);
	ftmp[m00] *= x;
	ftmp[m11] *= y;
	ftmp[m22] *= z;
	matrix44multiply( f, f, ftmp );
}

void matrix44rotateaa( FLT * f, FLT angle, FLT ix, FLT iy, FLT iz )
{
	FLT ftmp[16];

	FLT c = FLT_COS( angle*CNOVRDEGRAD );
	FLT s = FLT_SIN( angle*CNOVRDEGRAD );
	FLT absin = FLT_SQRT( ix*ix + iy*iy + iz*iz );
	FLT x = ix/absin;
	FLT y = iy/absin;
	FLT z = iz/absin;

	ftmp[m00] = x*x*(1-c)+c;
	ftmp[m01] = x*y*(1-c)-z*s;
	ftmp[m02] = x*z*(1-c)+y*s;
	ftmp[m03] = 0;

	ftmp[m10] = y*x*(1-c)+z*s;
	ftmp[m11] = y*y*(1-c)+c;
	ftmp[m12] = y*z*(1-c)-x*s;
	ftmp[m13] = 0;

	ftmp[m20] = x*z*(1-c)-y*s;
	ftmp[m21] = y*z*(1-c)+x*s;
	ftmp[m22] = z*z*(1-c)+c;
	ftmp[m23] = 0;

	ftmp[m30] = 0;
	ftmp[m31] = 0;
	ftmp[m32] = 0;
	ftmp[m33] = 1;

	matrix44multiply( f, f, ftmp );
}

void matrix44rotatequat( FLT * f, FLT * quatwxyz )
{
	FLT ftmp[16];
	float qw = quatwxyz[0];
	float qx = quatwxyz[1];
	float qy = quatwxyz[2];
	float qz = quatwxyz[3];
	FLT qw2 = qw*qw;
	FLT qx2 = qx*qx;
	FLT qy2 = qy*qy;
	FLT qz2 = qz*qz;

	ftmp[m00] = 1 - 2*qy2 - 2*qz2;
	ftmp[m01] = 2*qx*qy - 2*qz*qw;
	ftmp[m02] = 2*qx*qz + 2*qy*qw;
	ftmp[m03] = 0;

	ftmp[m10] = 2*qx*qy + 2*qz*qw;
	ftmp[m11] = 1 - 2*qx2 - 2*qz2;
	ftmp[m12] = 2*qy*qz - 2*qx*qw;
	ftmp[m13] = 0;

	ftmp[m20] = 2*qx*qz - 2*qy*qw;
	ftmp[m21] = 2*qy*qz + 2*qx*qw;
	ftmp[m22] = 1 - 2*qx2 - 2*qy2;
	ftmp[m23] = 0;

	ftmp[m30] = 0;
	ftmp[m31] = 0;
	ftmp[m32] = 0;
	ftmp[m33] = 1;

	matrix44multiply( f, f, ftmp );
}

void matrix44rotateea( FLT * f, FLT x, FLT y, FLT z )
{
	FLT ftmp[16];

	//x,y,z must be negated for some reason
	FLT X = -x*2*CNOVRPI/360.; //Reduced calulation for speed
	FLT Y = -y*2*CNOVRPI/360.;
	FLT Z = -z*2*CNOVRPI/360.;
	FLT cx = FLT_COS(X);
	FLT sx = FLT_SIN(X);
	FLT cy = FLT_COS(Y);
	FLT sy = FLT_SIN(Y);
	FLT cz = FLT_COS(Z);
	FLT sz = FLT_SIN(Z);

	//Row major (unless CNFG3D_USE_OGL_MAJOR is selected)
	//manually transposed
	ftmp[m00] = cy*cz;
	ftmp[m10] = (sx*sy*cz)-(cx*sz);
	ftmp[m20] = (cx*sy*cz)+(sx*sz);
	ftmp[m30] = 0;

	ftmp[m01] = cy*sz;
	ftmp[m11] = (sx*sy*sz)+(cx*cz);
	ftmp[m21] = (cx*sy*sz)-(sx*cz);
	ftmp[m31] = 0;

	ftmp[m02] = -sy;
	ftmp[m12] = sx*cy;
	ftmp[m22] = cx*cy;
	ftmp[m32] = 0;

	ftmp[m03] = 0;
	ftmp[m13] = 0;
	ftmp[m23] = 0;
	ftmp[m33] = 1;

	matrix44multiply( f, f, ftmp );
}

void matrix44multiply( FLT * fout, FLT * fin1, FLT * fin2 )
{
	FLT fotmp[16];
	fotmp[m00] = fin1[m00] * fin2[m00] + fin1[m01] * fin2[m10] + fin1[m02] * fin2[m20] + fin1[m03] * fin2[m30];
	fotmp[m01] = fin1[m00] * fin2[m01] + fin1[m01] * fin2[m11] + fin1[m02] * fin2[m21] + fin1[m03] * fin2[m31];
	fotmp[m02] = fin1[m00] * fin2[m02] + fin1[m01] * fin2[m12] + fin1[m02] * fin2[m22] + fin1[m03] * fin2[m32];
	fotmp[m03] = fin1[m00] * fin2[m03] + fin1[m01] * fin2[m13] + fin1[m02] * fin2[m23] + fin1[m03] * fin2[m33];

	fotmp[m10] = fin1[m10] * fin2[m00] + fin1[m11] * fin2[m10] + fin1[m12] * fin2[m20] + fin1[m13] * fin2[m30];
	fotmp[m11] = fin1[m10] * fin2[m01] + fin1[m11] * fin2[m11] + fin1[m12] * fin2[m21] + fin1[m13] * fin2[m31];
	fotmp[m12] = fin1[m10] * fin2[m02] + fin1[m11] * fin2[m12] + fin1[m12] * fin2[m22] + fin1[m13] * fin2[m32];
	fotmp[m13] = fin1[m10] * fin2[m03] + fin1[m11] * fin2[m13] + fin1[m12] * fin2[m23] + fin1[m13] * fin2[m33];

	fotmp[m20] = fin1[m20] * fin2[m00] + fin1[m21] * fin2[m10] + fin1[m22] * fin2[m20] + fin1[m23] * fin2[m30];
	fotmp[m21] = fin1[m20] * fin2[m01] + fin1[m21] * fin2[m11] + fin1[m22] * fin2[m21] + fin1[m23] * fin2[m31];
	fotmp[m22] = fin1[m20] * fin2[m02] + fin1[m21] * fin2[m12] + fin1[m22] * fin2[m22] + fin1[m23] * fin2[m32];
	fotmp[m23] = fin1[m20] * fin2[m03] + fin1[m21] * fin2[m13] + fin1[m22] * fin2[m23] + fin1[m23] * fin2[m33];

	fotmp[m30] = fin1[m30] * fin2[m00] + fin1[m31] * fin2[m10] + fin1[m32] * fin2[m20] + fin1[m33] * fin2[m30];
	fotmp[m31] = fin1[m30] * fin2[m01] + fin1[m31] * fin2[m11] + fin1[m32] * fin2[m21] + fin1[m33] * fin2[m31];
	fotmp[m32] = fin1[m30] * fin2[m02] + fin1[m31] * fin2[m12] + fin1[m32] * fin2[m22] + fin1[m33] * fin2[m32];
	fotmp[m33] = fin1[m30] * fin2[m03] + fin1[m31] * fin2[m13] + fin1[m32] * fin2[m23] + fin1[m33] * fin2[m33];
	matrix44copy( fout, fotmp );
}

void matrix44print( const FLT * f )
{
	int i;
	cnovrmathprintf( "{\n" );
	for( i = 0; i < 16; i+=4 )
	{
		cnovrmathprintf( "  %f, %f, %f, %f\n", f[0+i], f[1+i], f[2+i], f[3+i] );
	}
	cnovrmathprintf( "}\n" );
}

void matrix44perspective( FLT * out, FLT fovy, FLT aspect, FLT zNear, FLT zFar )
{
	FLT f = 1./FLT_TAN(fovy * CNOVRPI / 360.0);
	out[m00] = f/aspect; out[m01] = 0; out[m02] = 0; out[m03] = 0;
	out[m10] = 0; out[m11] = f; out[m12] = 0; out[m13] = 0;
	out[m20] = 0; out[m21] = 0;
	out[m22] = (zFar + zNear)/(zNear - zFar);
	out[m23] = 2*zFar*zNear  /(zNear - zFar);
	out[m30] = 0; out[m31] = 0; out[m32] = -1; out[m33] = 0;
}

void matrix44lookat( FLT * m, FLT * eye, FLT * at, FLT * up )
{
	FLT out[16];
	FLT otmp[16];
	FLT F[3] = { at[0] - eye[0], at[1] - eye[1], at[2] - eye[2] };
	FLT fdiv = 1./FLT_SQRT( F[0]*F[0] + F[1]*F[1] + F[2]*F[2] );
	FLT f[3] = { F[0]*fdiv, F[1]*fdiv, F[2]*fdiv };
	FLT udiv = 1./FLT_SQRT( up[0]*up[0] + up[1]*up[1] + up[2]*up[2] );
	FLT UP[3] = { up[0]*udiv, up[1]*udiv, up[2]*udiv };
	FLT s[3];
	FLT u[3];
	cross3d( s, f, UP );
	cross3d( u, s, f );

	out[m00] = s[0]; out[m01] = s[1]; out[m02] = s[2]; out[m03] = 0;
	out[m10] = u[0]; out[m11] = u[1]; out[m12] = u[2]; out[m13] = 0;
	out[m20] = -f[0];out[m21] =-f[1]; out[m22] =-f[2]; out[m23] = 0;
	out[m30] = 0;    out[m31] = 0;    out[m32] = 0;    out[m33] = 1;

	matrix44multiply( m, m, out );
	matrix44translate( m, -eye[0], -eye[1], -eye[2] );
}


void matrix44ptransform( FLT * pout, const FLT * pin, const FLT * f )
{
	FLT ptmp[2];
	ptmp[0] = pin[0] * f[m00] + pin[1] * f[m01] + pin[2] * f[m02] + f[m03];
	ptmp[1] = pin[0] * f[m10] + pin[1] * f[m11] + pin[2] * f[m12] + f[m13];
	pout[2] = pin[0] * f[m20] + pin[1] * f[m21] + pin[2] * f[m22] + f[m23];
	pout[0] = ptmp[0];
	pout[1] = ptmp[1];
}

void matrix44vtransform( FLT * vout, const FLT * vin, const FLT * f )
{
	FLT ptmp[2];
	ptmp[0] = vin[0] * f[m00] + vin[1] * f[m01] + vin[2] * f[m02];
	ptmp[1] = vin[0] * f[m10] + vin[1] * f[m11] + vin[2] * f[m12];
	vout[2] = vin[0] * f[m20] + vin[1] * f[m21] + vin[2] * f[m22];
	vout[0] = ptmp[0];
	vout[1] = ptmp[1];
}

void matrix444transform( FLT * kout, const FLT * pin, const FLT * f )
{
	FLT ptmp[3];
	ptmp[0] = pin[0] * f[m00] + pin[1] * f[m01] + pin[2] * f[m02] + pin[3] * f[m03];
	ptmp[1] = pin[0] * f[m10] + pin[1] * f[m11] + pin[2] * f[m12] + pin[3] * f[m13];
	ptmp[2] = pin[0] * f[m20] + pin[1] * f[m21] + pin[2] * f[m22] + pin[3] * f[m23];
	kout[3] = pin[0] * f[m30] + pin[1] * f[m31] + pin[2] * f[m32] + pin[3] * f[m33];
	kout[0] = ptmp[0];
	kout[1] = ptmp[1];
	kout[2] = ptmp[2];
}




static inline float tdNoiseAt( int x, int y )
{
	return ((x*13241 + y * 33455927)%9293) / 9292.;
//	srand( x + y * 1314);
//	return ((rand()%1000)/500.) - 1.0;
}

static inline float tdFade( float f )
{
	float ft3 = f*f*f;
	return ft3 * 10 - ft3 * f * 15 + 6 * ft3 * f * f;
}

float tdFLerp( float a, float b, float t )
{
	return cnovr_lerp( a, b, tdFade( t ) );
}

static inline float tdFNoiseAt( float x, float y )
{
	int ix = x;
	int iy = y;
	float fx = x - ix;
	float fy = y - iy;

	float a = tdNoiseAt( ix, iy );
	float b = tdNoiseAt( ix+1, iy );
	float c = tdNoiseAt( ix, iy+1 );
	float d = tdNoiseAt( ix+1, iy+1 );

	float top = tdFLerp( a, b, fx );
	float bottom = tdFLerp( c, d, fx );

	return tdFLerp( top, bottom, fy );
}

FLT cnovr_perlin( FLT x, FLT y )
{
	int ndepth = 5;

	int depth;
	float ret = 0;
	for( depth = 0; depth < ndepth; depth++ )
	{
		float nx = x / (1<<(ndepth-depth-1));
		float ny = y / (1<<(ndepth-depth-1));
		ret += tdFNoiseAt( nx, ny ) / (1<<(depth+1));
	}
	return ret;
}

cnovr_quat cnovr_quat_identity = {1.0,0.0,0.0,0.0};
cnovr_pose cnovr_pose_identity = {.Rot = {1.0, 0.0, 0.0, 0.0}, .Scale = 1, .Pos = { 0.0, 0.0, 0.0 } };
