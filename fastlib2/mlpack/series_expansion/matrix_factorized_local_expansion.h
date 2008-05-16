/**
 * @file matrix_factorized_local_expansion.h
 *
 * @author Dongryeol Lee (dongryel)
 * @bug No known bugs.
 */

#ifndef MATRIX_FACTORIZED_LOCAL_EXPANSION
#define MATRIX_FACTORIZED_LOCAL_EXPANSION

#include "fastlib/fastlib.h"

/**
 * Local expansion class
 */
template<typename TKernelAux>
class MatrixFactorizedLocalExpansion {

 private:

  /** The center of the expansion */
  Vector center_;
  
  /** auxiliary methods for the kernel (derivative, truncation error bound) */
  const TKernelAux *ka_;

  /** pointer to the kernel object inside kernel auxiliary object */
  const typename TKernelAux::TKernel *kernel_;

  /** pointer to the precomputed constants inside kernel auxiliary object */
  const typename TKernelAux::TSeriesExpansionAux *sea_;

  OT_DEF(MatrixFactorizedLocalExpansion) {
    OT_MY_OBJECT(center_);
    OT_MY_OBJECT(order_);
  }

 public:
  
  // getters and setters
  
  /** Get the coefficients */
  double bandwidth_sq() const { return kernel_->bandwidth_sq(); }
  
  /** Get the center of expansion */
  Vector* get_center() { return &center_; }

  const Vector* get_center() const { return &center_; }

  /** Get the coefficients */
  const Vector& get_coeffs() const { return coeffs_; }
  
  /** Get the approximation order */
  int get_order() const { return order_; }

  /** Get the maximum possible approximation order */
  int get_max_order() const { return sea_->get_max_order(); }

  /** Set the approximation order */
  void set_order(int new_order) { order_ = new_order; }

  // interesting functions...
  
  /**
   * Accumulates the local moment represented by the given reference
   * data into the coefficients
   */
  void AccumulateCoeffs(const Matrix& data, const Vector& weights,
			int begin, int end, int order);

  /**
   * This does not apply for local coefficients.
   */
  void RefineCoeffs(const Matrix& data, const Vector& weights,
		    int begin, int end, int order) { }
  
  /**
   * Evaluates the local coefficients at the given point
   */
  double EvaluateField(const Matrix& data, int row_num) const;
  double EvaluateField(const Vector& x_q) const;
  
  /**
   * Initializes the current local expansion object with the given
   * center.
   */
  void Init(const Vector& center, const TKernelAux &ka);
  void Init(const TKernelAux &ka);

  /**
   * Computes the required order for evaluating the local expansion
   * for any query point within the specified region for a given bound.
   */
  template<typename TBound>
  int OrderForEvaluating(const TBound &far_field_region,
			 const TBound &local_field_region,
			 double min_dist_sqd_regions,
			 double max_dist_sqd_regions,
                         double max_error, double *actual_error) const;

  /**
   * Prints out the series expansion represented by this object.
   */
  void PrintDebug(const char *name="", FILE *stream=stderr) const;

  /**
   * Translate from a far field expansion to the expansion here.
   * The translated coefficients are added up to the ones here.
   */
  void TranslateFromFarField
  (const MatrixFactorizedFarFieldExpansion<TKernelAux> &se);
  
  /**
   * Translate to the given local expansion. The translated coefficients
   * are added up to the passed-in local expansion coefficients.
   */
  void TranslateToLocal(MatrixFactorizedLocalExpansion &se);

};

#define INSIDE_MATRIX_FACTORIZED_LOCAL_EXPANSION_H
#include "matrix_factorized_local_expansion_impl.h"
#undef INSIDE_MATRIX_FACTORIZED_LOCAL_EXPANSION_H

#endif
