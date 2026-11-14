// Golf Dynamics — engine-independent golf simulation core.
// SPDX-License-Identifier: MIT
#include "golf/impact.hpp"

#include <cmath>

namespace golf {

namespace {

struct Mat3 {
    double m[3][3]{};
    static Mat3 identity(double s) {
        Mat3 a;
        a.m[0][0] = a.m[1][1] = a.m[2][2] = s;
        return a;
    }
    static Mat3 skew(const Vec3& v) {
        Mat3 a;
        a.m[0][1] = -v.z; a.m[0][2] = v.y;
        a.m[1][0] = v.z;  a.m[1][2] = -v.x;
        a.m[2][0] = -v.y; a.m[2][1] = v.x;
        return a;
    }
    static Mat3 outer(const Vec3& a, const Vec3& b, double s) {
        const double av[3] = {a.x, a.y, a.z}, bv[3] = {b.x, b.y, b.z};
        Mat3 o;
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j) o.m[i][j] = s * av[i] * bv[j];
        return o;
    }
    Mat3 operator*(const Mat3& b) const {
        Mat3 o;
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j) {
                double s = 0;
                for (int k = 0; k < 3; ++k) s += m[i][k] * b.m[k][j];
                o.m[i][j] = s;
            }
        return o;
    }
    Mat3 operator+(const Mat3& b) const {
        Mat3 o;
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j) o.m[i][j] = m[i][j] + b.m[i][j];
        return o;
    }
    Mat3 operator-(const Mat3& b) const {
        Mat3 o;
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j) o.m[i][j] = m[i][j] - b.m[i][j];
        return o;
    }
    Vec3 operator*(const Vec3& v) const {
        return {m[0][0] * v.x + m[0][1] * v.y + m[0][2] * v.z, m[1][0] * v.x + m[1][1] * v.y + m[1][2] * v.z,
                m[2][0] * v.x + m[2][1] * v.y + m[2][2] * v.z};
    }
    // Solve A x = b (Cramer's rule; K is symmetric positive definite).
    Vec3 solve(const Vec3& b) const {
        const double det = m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) -
                           m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0]) +
                           m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
        auto col = [&](int c) {
            Mat3 a = *this;
            a.m[0][c] = b.x;
            a.m[1][c] = b.y;
            a.m[2][c] = b.z;
            return a.m[0][0] * (a.m[1][1] * a.m[2][2] - a.m[1][2] * a.m[2][1]) -
                   a.m[0][1] * (a.m[1][0] * a.m[2][2] - a.m[1][2] * a.m[2][0]) +
                   a.m[0][2] * (a.m[1][0] * a.m[2][1] - a.m[1][1] * a.m[2][0]);
        };
        return {col(0) / det, col(1) / det, col(2) / det};
    }
};

const Vec3 kTarget{1, 0, 0};
const Vec3 kRight{0, 0, 1};

}  // namespace

ImpactResult strike(const Club& club, const BallModel& ball, const Delivery& d) {
    ImpactResult res;
    const double m = ball.massKg, M = club.headMassKg, r = ball.radius();
    const double a = d.attackDeg * kDeg, p = d.pathDeg * kDeg, dl = d.dynamicLoftDeg * kDeg, f = d.faceAngleDeg * kDeg;
    const Vec3 vClub = (kTarget * (std::cos(a) * std::cos(p)) + kRight * (std::cos(a) * std::sin(p)) + kUp * std::sin(a)) *
                       d.clubSpeed;
    const Vec3 nFace =
        (kTarget * (std::cos(dl) * std::cos(f)) + kRight * (std::cos(dl) * std::sin(f)) + kUp * std::sin(dl)).normalized();
    const Vec3 eV = tangential(kUp, nFace).normalized();         // up the face
    const Vec3 eT = d.leftHanded ? eV.cross(nFace) : nFace.cross(eV);  // toward the toe
    // Contact point from the head's CG; the CG sits cgDepth behind the face
    // (so the contact is cgDepth in front of it, along the face normal),
    // and the face normal at the strike point tilts with the bulge and roll.
    const Vec3 rC = nFace * club.cgDepthM + eT * d.strikeToeM + eV * (d.strikeUpM + club.cgBelowCenterM);
    Vec3 n = nFace;
    if (club.bulgeRadiusM > 0) n += eT * (d.strikeToeM / club.bulgeRadiusM);
    if (club.rollRadiusM > 0) n += eV * (d.strikeUpM / club.rollRadiusM);
    n = n.normalized();
    const Vec3 rB = n * -r;

    // Inverse inertias.
    const Mat3 Wb = Mat3::identity(1.0 / (ball.inertiaFactor * m * r * r));
    const Mat3 Wc = Mat3::outer(eT, eT, 1.0 / club.moiHeelToe) + Mat3::outer(eV, eV, 1.0 / club.moiVertical) +
                    Mat3::outer(nFace, nFace, 1.0 / (club.moiHeelToe + club.moiVertical));
    const Mat3 Sb = Mat3::skew(rB), Sc = Mat3::skew(rC);
    const Mat3 K = Mat3::identity(1.0 / m + 1.0 / M) - Sb * Wb * Sb - Sc * Wc * Sc;

    // Relative velocity of the ball's contact point to the club's (ball at rest).
    const Vec3 u = vClub * -1.0;
    const double un = u.dot(n);
    if (un >= 0) return res;  // not approaching
    const Vec3 ut = u - n * un;
    const double mu = club.friction * d.frictionScale;
    const double et = club.tangentialRestitution * d.frictionScale;
    const Vec3 want = n * (-(1 + club.cor) * un) + ut * -(1 + et);
    Vec3 J = K.solve(want);
    const double jn = J.dot(n);
    const Vec3 jt = J - n * jn;
    if (jt.length() > mu * jn && ut.length() > 1e-12) {
        res.sliding = true;
        const Vec3 s = ut.normalized();
        const Vec3 dir = n - s * mu;
        const double jn2 = -(1 + club.cor) * un / n.dot(K * dir);
        J = dir * jn2;
    }
    res.ballVelocity = J / m;
    res.ballSpin = Wb * rB.cross(J);

    const Vec3 v = res.ballVelocity;
    res.speed = v.length();
    res.vlaDeg = std::asin(clamp(v.y / std::max(res.speed, 1e-12), -1.0, 1.0)) / kDeg;
    res.hlaDeg = std::atan2(v.z, v.x) / kDeg;
    const Vec3 h = Vec3{v.x, 0, v.z}.normalized();
    const Vec3 right = h.cross(kUp);
    const Vec3 w = res.ballSpin - (v / std::max(res.speed, 1e-12)) * res.ballSpin.dot(v / std::max(res.speed, 1e-12));
    res.spinRpm = w.length() / kRpm;
    res.spinAxisDeg = std::atan2(-w.dot(kUp), w.dot(right)) / kDeg;
    // A topspinning putt has its axis pointing left: report it as negative
    // spin about the usual axis rather than a 180-degree axis.
    if (res.spinAxisDeg > 90) {
        res.spinAxisDeg -= 180;
        res.spinRpm = -res.spinRpm;
    } else if (res.spinAxisDeg < -90) {
        res.spinAxisDeg += 180;
        res.spinRpm = -res.spinRpm;
    }
    res.smash = d.clubSpeed > 0 ? res.speed / d.clubSpeed : 0;
    res.spinLoftDeg = std::acos(clamp(nFace.dot(vClub.normalized()), -1.0, 1.0)) / kDeg;
    // Contact time ~0.45 ms at a 50 m/s normal approach, scaling as v^-1/5
    // (Hertz; Cochran & Stobbs 1968 measured about 0.45 ms for a drive).
    res.contactTimeS = 0.45e-3 * std::pow(std::max(-un, 1.0) / 50.0, -0.2);
    return res;
}

Launch toLaunch(const ImpactResult& r, const Vec3& position, double targetHeading) {
    Launch l;
    l.position = position;
    l.targetHeading = targetHeading;
    l.speed = r.speed;
    l.vlaDeg = r.vlaDeg;
    l.hlaDeg = r.hlaDeg;
    l.spinRpm = r.spinRpm;
    l.spinAxisDeg = r.spinAxisDeg;
    return l;
}

double solveDynamicLoft(const Club& club, const BallModel& ball, double speed, double attackDeg, double launchDeg) {
    auto f = [&](double loft) {
        Delivery d;
        d.clubSpeed = speed;
        d.attackDeg = attackDeg;
        d.dynamicLoftDeg = loft;
        return strike(club, ball, d).vlaDeg - launchDeg;
    };
    double x0 = launchDeg + 2, x1 = launchDeg + 8;
    double f0 = f(x0), f1 = f(x1);
    for (int i = 0; i < 40 && std::abs(f1) > 1e-9; ++i) {
        if (f1 == f0) break;
        const double x2 = x1 - f1 * (x1 - x0) / (f1 - f0);
        x0 = x1;
        f0 = f1;
        x1 = clamp(x2, -10.0, 80.0);
        f1 = f(x1);
    }
    return x1;
}

}  // namespace golf
