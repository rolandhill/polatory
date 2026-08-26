#pragma once

#include <Eigen/Core>
#include <algorithm>
#include <limits>
#include <numeric>
#include <polatory/geometry/point3d.hpp>
#include <polatory/isosurface/triangle.hpp>
#include <polatory/isosurface/types.hpp>
#include <polatory/types.hpp>
#include <utility>
#include <vector>

// A bounding volume hierarchy over a mesh's faces, for closest-point queries. Built by recursively
// splitting the faces at the median centroid along the widest axis; queried by branch and bound on
// the node boxes, so the answer is exact.
class FaceTree {
  using Faces = polatory::isosurface::Faces;
  using Index = polatory::Index;
  using Point3 = polatory::geometry::Point3;
  using Points3 = polatory::geometry::Points3;

  static constexpr Index kLeafSize = 8;

  struct Node {
    Point3 lo;
    Point3 hi;
    Index left{-1};  // the child nodes; both are -1 in a leaf
    Index right{-1};
    Index begin{};  // a leaf's faces, as the range [begin, end) of order_
    Index end{};
  };

 public:
  FaceTree(const Points3& vertices, const Faces& faces) : vertices_(vertices), faces_(faces) {
    auto nf = faces_.rows();
    order_.resize(nf);
    std::iota(order_.begin(), order_.end(), Index{0});
    centroids_.resize(nf, 3);
    for (Index fi = 0; fi < nf; fi++) {
      centroids_.row(fi) = vertices_(faces_.row(fi), polatory::kAll).colwise().mean();
    }
    if (nf > 0) {
      nodes_.reserve(2 * (nf / kLeafSize + 1));
      build(0, nf);
    }
  }

  // The squared distance from p to the mesh; sets fi to the nearest face and closest to the
  // nearest point on it. Returns infinity (and fi = -1) if the mesh has no faces.
  double squared_distance(const Point3& p, Index& fi, Point3& closest) const {
    auto best = std::numeric_limits<double>::infinity();
    fi = -1;
    if (!nodes_.empty()) {
      search(0, p, best, fi, closest);
    }
    return best;
  }

 private:
  // Builds the node over order_[begin, end) and returns its index.
  Index build(Index begin, Index end) {
    auto ni = static_cast<Index>(nodes_.size());
    nodes_.emplace_back();

    auto inf = std::numeric_limits<double>::infinity();
    Point3 lo = Point3::Constant(inf);
    Point3 hi = Point3::Constant(-inf);
    for (auto k = begin; k < end; k++) {
      auto vs = vertices_(faces_.row(order_.at(k)), polatory::kAll);
      lo = lo.cwiseMin(vs.colwise().minCoeff());
      hi = hi.cwiseMax(vs.colwise().maxCoeff());
    }
    nodes_.at(ni).lo = lo;
    nodes_.at(ni).hi = hi;

    if (end - begin <= kLeafSize) {
      nodes_.at(ni).begin = begin;
      nodes_.at(ni).end = end;
      return ni;
    }

    Point3 clo = Point3::Constant(inf);
    Point3 chi = Point3::Constant(-inf);
    for (auto k = begin; k < end; k++) {
      clo = clo.cwiseMin(centroids_.row(order_.at(k)));
      chi = chi.cwiseMax(centroids_.row(order_.at(k)));
    }
    Index axis{};
    (chi - clo).maxCoeff(&axis);

    // A median split by count, so the recursion terminates even where the centroids coincide.
    auto mid = begin + (end - begin) / 2;
    std::nth_element(order_.begin() + begin, order_.begin() + mid, order_.begin() + end,
                     [&](Index a, Index b) { return centroids_(a, axis) < centroids_(b, axis); });

    auto left = build(begin, mid);
    auto right = build(mid, end);
    nodes_.at(ni).left = left;
    nodes_.at(ni).right = right;
    return ni;
  }

  static double box_dist2(const Node& node, const Point3& p) {
    Point3 q = p.cwiseMax(node.lo).cwiseMin(node.hi);
    return (p - q).squaredNorm();
  }

  void search(Index ni, const Point3& p, double& best, Index& fi, Point3& closest) const {
    const auto& node = nodes_.at(ni);

    if (node.left < 0) {
      for (auto k = node.begin; k < node.end; k++) {
        auto j = order_.at(k);
        auto f = faces_.row(j);
        Point3 c;
        auto d2 = polatory::isosurface::point_triangle_closest(
            p, vertices_.row(f(0)), vertices_.row(f(1)), vertices_.row(f(2)), c);
        if (d2 < best) {
          best = d2;
          fi = j;
          closest = c;
        }
      }
      return;
    }

    // The nearer child first, so the other is more often pruned.
    auto first = node.left;
    auto second = node.right;
    auto d_first = box_dist2(nodes_.at(first), p);
    auto d_second = box_dist2(nodes_.at(second), p);
    if (d_second < d_first) {
      std::swap(first, second);
      std::swap(d_first, d_second);
    }
    if (d_first < best) {
      search(first, p, best, fi, closest);
    }
    if (d_second < best) {
      search(second, p, best, fi, closest);
    }
  }

  const Points3& vertices_;
  const Faces& faces_;
  Points3 centroids_;
  std::vector<Index> order_;  // the face indices, permuted so each leaf holds a contiguous range
  std::vector<Node> nodes_;   // node 0 is the root
};
