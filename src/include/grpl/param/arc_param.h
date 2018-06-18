#pragma once

#include "grpl/curve/arc.h"
#include "grpl/spline/spline.h"

namespace grpl {
namespace param {

  // Augmented Arc is an arc with a non-constant curvature.
  // This slightly disobeys the geometry of the arc, since an arc is, by
  // definition, a circle segment, and a circles radius (therefore curvature)
  // is constant, however this functions as an approximation so that curvature
  // at knot points in the parameterization (spline sections) is continuous, via
  // linear interpolation with dk/ds. As such, if you run a test on curvature
  // vs radius of the circle, they will not match, instead being (slightly) out.
  // This is another case of pure math not _quite_ working out in real-world
  // applications, who would-a thunk it.
  class augmented_arc2d : public grpl::curve::arc2d {
   public:
    augmented_arc2d() : grpl::curve::arc2d() {}
    augmented_arc2d(vector_t start, vector_t mid, vector_t end)
        : grpl::curve::arc2d(start, mid, end) {}

    void set_curvature(double start_k, double end_k) {
      _curvature       = start_k;
      _curvature_dk_ds = (end_k - start_k) / length();
      _curvature_set   = true;
    }

    double curvature(const double s) const override {
      if (_curvature_set)
        return _curvature + s * _curvature_dk_ds;
      else
        return grpl::curve::arc2d::curvature(s);
    }

   private:
    double _curvature, _curvature_dk_ds;
    bool   _curvature_set = false;
  };

  class arc_parameterizer {
   public:
    using vector_t = typename Eigen::Matrix<double, 2, 1>;
    using curve_t  = augmented_arc2d;

    arc_parameterizer() {}

    void configure(double max_arc_length, double max_delta_curvature) {
      _max_arc_length      = max_arc_length;
      _max_delta_curvature = max_delta_curvature;
    }

    bool has_overrun() { return _has_overrun; }

    size_t curve_count(grpl::spline::spline<2> *spline, double tLo = 0, double tHi = 1,
                       size_t count = 0) const {
      double tMid = (tHi + tLo) / 2.0;

      augmented_arc2d current_arc(spline->calculate(tLo), spline->calculate(tMid),
                                  spline->calculate(tHi));

      double kLo = spline->curvature(tLo);
      double kHi = spline->curvature(tHi);

      bool subdivide = (fabs(kHi - kLo) > _max_delta_curvature) ||
                       (current_arc.length() > _max_arc_length);

      if (subdivide) {
        count = curve_count(spline, tLo, tMid, count);
        count = curve_count(spline, tMid, tHi, count);
        return count;
      } else {
        return count + 1;
      }
    }

    template <typename iterator_spline_t>
    size_t curve_count(iterator_spline_t spline_begin, iterator_spline_t spline_end,
                       size_t count = 0) const {
      size_t total_count = 0;
      for (iterator_spline_t it = spline_begin; it != spline_end; it++) {
        total_count += curve_count((grpl::spline::spline<2> *)it);
      }
      return total_count;
    }

    // Must call .resize() on dynamic containers before calling this function.
    template <typename iterator_curve_t>
    iterator_curve_t parameterize(grpl::spline::spline<2> *spline, iterator_curve_t curve_begin,
                                  iterator_curve_t curve_end, double tLo = 0,
                                  double tHi = 1) {
      _has_overrun = false;
      if (curve_begin == curve_end) {
        _has_overrun = true;
        return curve_end;
      }

      double tMid = (tHi + tLo) / 2.0;

      augmented_arc2d current_arc(spline->calculate(tLo), spline->calculate(tMid),
                                  spline->calculate(tHi));

      double kLo = spline->curvature(tLo);
      double kHi = spline->curvature(tHi);

      bool subdivide = (fabs(kHi - kLo) > _max_delta_curvature) ||
                       (current_arc.length() > _max_arc_length);

      if (subdivide) {
        curve_begin = parameterize(spline, curve_begin, curve_end, tLo, tMid);
        curve_begin = parameterize(spline, curve_begin, curve_end, tMid, tHi);
        return curve_begin;
      } else {
        current_arc.set_curvature(kLo, kHi);
        *curve_begin = current_arc;
        return ++curve_begin;
      }
    }

    template <typename iterator_curve_t, typename iterator_spline_t>
    iterator_curve_t parameterize(iterator_spline_t spline_begin, iterator_spline_t spline_end,
                                  iterator_curve_t curve_begin,
                                  iterator_curve_t curve_end) {
      iterator_curve_t head = curve_begin;
      for (iterator_spline_t it = spline_begin; it != spline_end; it++) {
        head = parameterize((grpl::spline::spline<2> *)it, head, curve_end);
      }
      return head;
    }

   private:
    double _max_arc_length;
    double _max_delta_curvature;
    bool   _has_overrun;
  };
}  // namespace param
}  // namespace grpl