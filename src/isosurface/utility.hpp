#pragma once

#include <Eigen/Core>
#include <array>
#include <boost/container/static_vector.hpp>
#include <polatory/geometry/bbox3d.hpp>
#include <polatory/geometry/point3d.hpp>
#include <polatory/isosurface/predicates.hpp>
#include <polatory/isosurface/triangle.hpp>
#include <polatory/isosurface/types.hpp>
#include <polatory/types.hpp>

namespace polatory::isosurface {

// Snaps any coordinate of p within 1e-10 * resolution of a bbox face exactly onto it.
inline geometry::Point3 snap_to_bbox(const geometry::Point3& p, const geometry::Bbox3& bbox,
                                     double resolution) {
  const auto& min = bbox.min();
  const auto& max = bbox.max();
  auto tiny = 1e-10 * resolution;

  geometry::Point3 q = p;
  q = ((q.array() - min.array()).abs() < tiny).select(min, q);
  q = ((q.array() - max.array()).abs() < tiny).select(max, q);
  return q;
}

// Tests if triangles a and b possibly intersect besides the simplices they share.
inline bool triangles_intersect(const geometry::Point3& a0, const geometry::Point3& a1,
                                const geometry::Point3& a2, const geometry::Point3& b0,
                                const geometry::Point3& b1, const geometry::Point3& b2) {
  std::array<geometry::Point3, 3> a{a0, a1, a2};
  std::array<geometry::Point3, 3> b{b0, b1, b2};

  // Vertices shared by position, as indices into a and into b.
  boost::container::static_vector<Index, 3> as;
  boost::container::static_vector<Index, 3> bs;
  for (Index i = 0; i < 3; i++) {
    for (Index j = 0; j < 3; j++) {
      if (a.at(i) == b.at(j)) {
        as.push_back(i);
        bs.push_back(j);
      }
    }
  }

  switch (as.size()) {
    case 0:
      return triangle3_triangle3_intersect(a0, a1, a2, b0, b1, b2);
    case 1: {
      Index i = (as.at(0) + 1) % 3;
      Index j = (as.at(0) + 2) % 3;
      Index k = (bs.at(0) + 1) % 3;
      Index l = (bs.at(0) + 2) % 3;
      return segment3_triangle3_intersect(a.at(i), a.at(j), b0, b1, b2) ||
             segment3_triangle3_intersect(b.at(k), b.at(l), a0, a1, a2);
    }
    case 2: {
      Index a_apex = 3 - as.at(0) - as.at(1);
      Index b_apex = 3 - bs.at(0) - bs.at(1);
      return folded(a.at(as.at(0)), a.at(as.at(1)), a.at(a_apex), b.at(b_apex));
    }
    default:
      return false;
  }
}

}  // namespace polatory::isosurface
