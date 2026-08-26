#pragma once

#include <Eigen/Core>
#include <charconv>
#include <format>
#include <fstream>
#include <polatory/geometry/point3d.hpp>
#include <polatory/isosurface/types.hpp>
#include <polatory/numeric/conv.hpp>
#include <polatory/types.hpp>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace polatory::isosurface {

struct EntireTag {};

class Mesh {
  using Points = geometry::Points3;

 public:
  Mesh() = default;

  Mesh(Points vertices, Faces faces) : vertices_(std::move(vertices)), faces_(std::move(faces)) {}

  explicit Mesh(EntireTag /*tag*/) : entire_(true) {}

  void export_obj(const std::string& filename) const {
    std::ofstream ofs(filename);
    if (!ofs) {
      throw std::runtime_error(std::format("cannot open file '{}'", filename));
    }

    if (faces_.rows() == 0) {
      if (entire_) {
        ofs << "# entire\n";
      } else {
        ofs << "# empty\n";
      }
    }

    for (auto v : vertices_.rowwise()) {
      ofs << "v " << numeric::to_string(v(0)) << ' ' << numeric::to_string(v(1)) << ' '
          << numeric::to_string(v(2)) << '\n';
    }

    for (auto f : faces_.rowwise()) {
      ofs << "f " << f(0) + 1 << ' ' << f(1) + 1 << ' ' << f(2) + 1 << '\n';
    }
  }

  const Faces& faces() const { return faces_; }

  bool is_empty() const { return faces_.rows() == 0 && !entire_; }

  bool is_entire() const { return entire_; }

  const Points& vertices() const { return vertices_; }

 private:
  Points vertices_;
  Faces faces_;
  bool entire_{};
};

// Reads a mesh from a Wavefront OBJ file. Only "v" and "f" statements are read; a face's texture
// and normal indices are ignored, and a polygonal face is fan-triangulated.
inline Mesh read_obj(const std::string& filename) {
  std::ifstream ifs(filename);
  if (!ifs) {
    throw std::runtime_error(std::format("cannot open file '{}'", filename));
  }

  std::vector<geometry::Point3> vertices;
  std::vector<Face> faces;
  std::vector<Index> polygon;
  std::string line;
  std::string token;
  while (std::getline(ifs, line)) {
    std::istringstream iss(line);
    if (!(iss >> token)) {
      continue;
    }

    if (token == "v") {
      std::string x;
      std::string y;
      std::string z;
      if (!(iss >> x >> y >> z)) {
        throw std::runtime_error(std::format("malformed vertex in file '{}'", filename));
      }
      vertices.emplace_back(numeric::to_double(x), numeric::to_double(y), numeric::to_double(z));
    } else if (token == "f") {
      polygon.clear();
      while (iss >> token) {
        // A vertex reference is "v", "v/vt", "v//vn", or "v/vt/vn"; only the first field is used.
        std::string_view field(token);
        field = field.substr(0, field.find('/'));
        Index index{};
        auto [ptr, ec] = std::from_chars(field.data(), field.data() + field.size(), index);
        if (ec != std::errc{} || ptr != field.data() + field.size()) {
          throw std::runtime_error(std::format("malformed face in file '{}'", filename));
        }
        // A negative index counts back from the last vertex read so far.
        auto v = index < 0 ? static_cast<Index>(vertices.size()) + index : index - 1;
        if (index == 0 || v < 0 || v >= static_cast<Index>(vertices.size())) {
          throw std::runtime_error(std::format("vertex index out of range in file '{}'", filename));
        }
        polygon.push_back(v);
      }
      for (std::size_t k = 2; k < polygon.size(); k++) {
        faces.emplace_back(polygon.at(0), polygon.at(k - 1), polygon.at(k));
      }
    }
  }

  geometry::Points3 v(static_cast<Index>(vertices.size()), 3);
  for (Index i = 0; i < v.rows(); i++) {
    v.row(i) = vertices.at(i);
  }
  Faces f(static_cast<Index>(faces.size()), 3);
  for (Index i = 0; i < f.rows(); i++) {
    f.row(i) = faces.at(i);
  }
  return {std::move(v), std::move(f)};
}

}  // namespace polatory::isosurface
