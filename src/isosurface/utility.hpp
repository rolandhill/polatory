#pragma once

#include <Eigen/Core>
#include <array>
#include <boost/container/static_vector.hpp>
#include <jizai/geometry/bbox3d.hpp>
#include <jizai/geometry/point3d.hpp>
#include <jizai/isosurface/predicates.hpp>
#include <jizai/isosurface/triangle.hpp>
#include <jizai/isosurface/types.hpp>
#include <jizai/types.hpp>

namespace jizai::isosurface {

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

  boost::container::static_vector<Index, 3> a_shared;
  boost::container::static_vector<Index, 3> b_shared;
  for (Index i = 0; i < 3; i++) {
    for (Index j = 0; j < 3; j++) {
      if (a.at(i) == b.at(j)) {
        a_shared.push_back(i);
        b_shared.push_back(j);
      }
    }
  }

  switch (a_shared.size()) {
    case 0:
      return triangle3_triangle3_intersect(a0, a1, a2, b0, b1, b2);
    case 1: {
      Index i = (a_shared.at(0) + 1) % 3;
      Index j = (a_shared.at(0) + 2) % 3;
      Index k = (b_shared.at(0) + 1) % 3;
      Index l = (b_shared.at(0) + 2) % 3;
      return segment3_triangle3_intersect(a.at(i), a.at(j), b0, b1, b2) ||
             segment3_triangle3_intersect(b.at(k), b.at(l), a0, a1, a2);
    }
    case 2: {
      Index a_apex = 3 - a_shared.at(0) - a_shared.at(1);
      Index b_apex = 3 - b_shared.at(0) - b_shared.at(1);
      return folded(a.at(a_shared.at(0)), a.at(a_shared.at(1)), a.at(a_apex), b.at(b_apex));
    }
    default:
      return false;
  }
}

}  // namespace jizai::isosurface
