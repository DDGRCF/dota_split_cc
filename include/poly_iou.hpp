#ifndef POLY_IOU_H_
#define POLY_IOU_H_

#include <stdint.h>

#include <cmath>
#include <type_traits>

namespace std {

const int MaxN = 10;
const float Eps = 1e-6;

enum OverLaps { kIoU = 0, kIoF = 1 };

template <typename T>
inline int sig(T d) {
  return (d > Eps) - (d < -Eps);
}

template <typename T>
struct Point {
  T x, y;
  ~Point() = default;

  Point(const T& px = 0, const T& py = 0) : x(px), y(py) {}

  Point operator+(const Point& p) const { return Point(x + p.x, y + p.y); }

  Point& operator+=(const Point& p) {
    x += p.x;
    y += p.y;
    return *this;
  }

  Point operator-(const Point& p) const { return Point(x - p.x, y - p.y); }

  Point operator*(const T coeff) const { return Point(x * coeff, y * coeff); }

  bool operator==(const Point& p) const {
    if (this == &p) return true;
    return sig<T>(this->x - p.x) == 0 && sig<T>(this->y - p.y) == 0;
  }

  Point& operator=(const Point& p) {
    if (this == &p) return *this;
    this->x = p.x;
    this->y = p.y;
    return *this;
  }

  Point& operator[](const int index) { return *(this + index); }
};

template <typename T>
inline T cross(const Point<T>& o, const Point<T>& a, const Point<T>& b) {
  return (a.x - o.x) * (b.y - o.y) - (b.x - o.x) * (a.y - o.y);
}

template <typename T>
inline T line_cross(const Point<T>& a, const Point<T>& b, const Point<T>& c,
                    const Point<T>& d, Point<T>& p) {
  T s1, s2;
  s1 = cross<T>(a, b, c);
  s2 = cross<T>(a, b, d);
  if (sig<T>(s1) == 0 && sig<T>(s2) == 0) return 2;
  if (sig<T>(s2 - s1) == 0) return 0;
  p.x = (c.x * s2 - d.x * s1) / (s2 - s1);
  p.y = (c.y * s2 - d.y * s1) / (s2 - s1);
  return 1;
}

template <typename T>
inline void polygon_cut(Point<T>* p, uint8_t& n, const Point<T>& a,
                        const Point<T>& b, Point<T>* pp) {
  int m = 0;
  p[n] = p[0];
  for (uint8_t i = 0; i < n; i++) {
    if (sig<T>(cross<T>(a, b, p[i])) > 0) pp[m++] = p[i];
    if (sig<T>(cross<T>(a, b, p[i])) != sig<T>(cross<T>(a, b, p[i + 1])))
      line_cross<T>(a, b, p[i], p[i + 1], pp[m++]);
  }
  n = 0;
  for (uint8_t i = 0; i < m; i++)
    if (!i || !(pp[i] == pp[i - 1])) p[n++] = pp[i];
  while (n > 1 && (p[n - 1] == p[0])) n--;
}

template <typename T>
inline T area(Point<T>* q, const uint8_t& m) {
  q[m] = q[0];
  T res{0.};
  for (uint8_t i = 0; i < m; ++i) {
    res += q[i].x * q[i + 1].y - q[i].y * q[i + 1].x;
  }
  return res / 2.0;
}

template <typename T>
inline void point_swap(Point<T>* a, Point<T>* b) {
  Point<T> tmp = *a;
  *a = *b;
  *b = tmp;
}

template <typename T>
inline void point_reverse(Point<T>* first, Point<T>* last) {
  while ((first != last) && (first != --last)) {
    point_swap<T>(first, last);
    ++first;
  }
}

template <typename T>
inline T rotated_boxes_intersection(Point<T> a, Point<T> b, Point<T> c,
                                    Point<T> d) {
  Point<T> o{0, 0};
  int s1 = sig<T>(cross<T>(o, a, b));
  int s2 = sig<T>(cross<T>(o, c, d));
  if (s1 == 0 || s2 == 0) return 0.;
  if (s1 == -1) point_swap<T>(&a, &b);
  if (s2 == -1) point_swap<T>(&c, &d);
  Point<T> p[MaxN] = {o, a, b};
  uint8_t n = 3;
  Point<T> pp[MaxN];
  polygon_cut<T>(p, n, o, c, pp);
  polygon_cut<T>(p, n, c, d, pp);
  polygon_cut<T>(p, n, d, o, pp);
  T res = fabs(area<T>(p, n));
  if (s1 * s2 == -1) res = -res;
  return res;
}

template <typename T>
inline T rotated_boxes_intersection(Point<T> (&pts1)[MaxN], const uint8_t& n1,
                                    Point<T> (&pts2)[MaxN], const uint8_t& n2) {
  if (area<T>(pts1, n1) < 0) point_reverse<T>(pts1, pts1 + n1);
  if (area<T>(pts2, n2) < 0) point_reverse<T>(pts2, pts2 + n2);
  pts1[n1] = pts1[0];
  pts2[n2] = pts2[0];
  T res{0.};
  for (uint8_t i = 0; i < n1; i++) {
    for (uint8_t j = 0; j < n2; j++) {
      res += rotated_boxes_intersection<T>(pts1[i], pts1[i + 1], pts2[j],
                                           pts2[j + 1]);
    }
  }

  return res;
}

template <typename T>
inline T single_poly_iou_rotated(T const* const box1_raw,
                                 T const* const box2_raw,
                                 const bool& mode = kIoU) {
  static_assert(
      (is_same_v<decay_t<T>, float> || is_same_v<decay_t<T>, double>)&&(
          is_same_v<decay_t<T>, float> || is_same_v<decay_t<T>, double>),
      "box1 and box2 must be float or double");
  Point<T> pts1[MaxN];
  const uint8_t n1 = 4;
  Point<T> pts2[MaxN];
  const uint8_t n2 = 4;

  for (uint8_t i = 0; i < 4; ++i) {
    pts1[i].x = box1_raw[i * 2];
    pts1[i].y = box1_raw[i * 2 + 1];

    pts2[i].x = box2_raw[i * 2];
    pts2[i].y = box2_raw[i * 2 + 1];
  }

  T inter_area = rotated_boxes_intersection(pts1, n1, pts2, n2);
  T union_area;
  if (mode == kIoU) {
    union_area = fabs(area(pts1, n1)) + fabs(area(pts2, n2)) - inter_area;
  } else if (mode == kIoF) {
    union_area = fabs(area(pts1, n1));
  }

  T iou = 0;

  if (union_area == 0) {
    iou = (inter_area + 1) / (union_area + 1);
  } else {
    iou = inter_area / union_area;
  }

  return iou;
}

}  // namespace std

#endif