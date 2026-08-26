#pragma once

#include <Eigen/Core>
#include <Eigen/Geometry>
#include <algorithm>
#include <cmath>
#include <polatory/geometry/point3d.hpp>

namespace polatory::isosurface {

// The barycentric coordinates of p with respect to triangle (a, b, c), by Cramer's rule on the
// 2x2 Gram system of the edge vectors. p need not lie in the plane; its normal component is
// ignored. The triangle must be non-degenerate.
inline geometry::Vector3 barycentric_coordinates(const geometry::Point3& p,
                                                 const geometry::Point3& a,
                                                 const geometry::Point3& b,
                                                 const geometry::Point3& c) {
  geometry::Vector3 ab = b - a;
  geometry::Vector3 ac = c - a;
  geometry::Vector3 ap = p - a;
  auto d00 = ab.dot(ab);
  auto d01 = ab.dot(ac);
  auto d11 = ac.dot(ac);
  auto d20 = ap.dot(ab);
  auto d21 = ap.dot(ac);
  auto denom = d00 * d11 - d01 * d01;
  auto v = (d11 * d20 - d01 * d21) / denom;
  auto w = (d00 * d21 - d01 * d20) / denom;
  return {1.0 - v - w, v, w};
}

// Ericson's closest-point region test; sets closest to the nearest point of triangle (a, b, c).
inline double point_triangle_closest(const geometry::Point3& p, const geometry::Point3& a,
                                     const geometry::Point3& b, const geometry::Point3& c,
                                     geometry::Point3& closest) {
  geometry::Vector3 ab = b - a;
  geometry::Vector3 ac = c - a;
  geometry::Vector3 ap = p - a;
  double d1 = ab.dot(ap);
  double d2 = ac.dot(ap);
  if (d1 <= 0.0 && d2 <= 0.0) {
    closest = a;
    return ap.squaredNorm();
  }
  geometry::Vector3 bp = p - b;
  double d3 = ab.dot(bp);
  double d4 = ac.dot(bp);
  if (d3 >= 0.0 && d4 <= d3) {
    closest = b;
    return bp.squaredNorm();
  }
  geometry::Vector3 cp = p - c;
  double d5 = ab.dot(cp);
  double d6 = ac.dot(cp);
  if (d6 >= 0.0 && d5 <= d6) {
    closest = c;
    return cp.squaredNorm();
  }
  double vc = d1 * d4 - d3 * d2;
  if (vc <= 0.0 && d1 >= 0.0 && d3 <= 0.0) {
    double v = d1 / (d1 - d3);
    closest = a + v * ab;
    return (ap - v * ab).squaredNorm();
  }
  double vb = d5 * d2 - d1 * d6;
  if (vb <= 0.0 && d2 >= 0.0 && d6 <= 0.0) {
    double w = d2 / (d2 - d6);
    closest = a + w * ac;
    return (ap - w * ac).squaredNorm();
  }
  double va = d3 * d6 - d5 * d4;
  if (va <= 0.0 && (d4 - d3) >= 0.0 && (d5 - d6) >= 0.0) {
    double w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
    closest = b + w * (c - b);
    return (p - closest).squaredNorm();
  }
  double denom = 1.0 / (va + vb + vc);
  double v = vb * denom;
  double w = vc * denom;
  closest = a + ab * v + ac * w;
  return (p - closest).squaredNorm();
}

inline double point_triangle_dist2(const geometry::Point3& p, const geometry::Point3& a,
                                   const geometry::Point3& b, const geometry::Point3& c) {
  geometry::Point3 closest;
  return point_triangle_closest(p, a, b, c, closest);
}

inline double triangle_min_angle(const geometry::Point3& a, const geometry::Point3& b,
                                 const geometry::Point3& c) {
  auto angle = [](const geometry::Vector3& u, const geometry::Vector3& w) {
    auto lu = u.norm();
    auto lw = w.norm();
    if (!(lu > 0.0) || !(lw > 0.0)) {
      return 0.0;
    }
    return std::acos(std::clamp(u.dot(w) / (lu * lw), -1.0, 1.0));
  };
  return std::min({angle(b - a, c - a), angle(a - b, c - b), angle(a - c, b - c)});
}

// The unnormalized normal of triangle (a, b, c); its length is twice the triangle's area.
inline geometry::Vector3 triangle_normal(const geometry::Point3& a, const geometry::Point3& b,
                                         const geometry::Point3& c) {
  return geometry::Vector3((b - a).cross(c - a));
}

}  // namespace polatory::isosurface
