/*
 * Copyright (C) 2024 Dillon Cislo
 *
 * This file is part of MINGROC++.
 *
 * MINGROC++ is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will by useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTIBILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program.
 * If not, see <http://www.gnu.org/licenses/>
 *
 */

#include "LineSearchBacktracking.h"
#include "../MINGROC/clipToUnitCircle.h"

#include <stdexcept>
#include <iostream>

#include <igl/predicates/find_self_intersections.h>

///
/// Line search by backtracking
///
template <typename Scalar, typename Index>
void MINGROCpp::LineSearchBacktracking<Scalar, Index>::LineSearch(
    const MINGROC<Scalar, Index> &mingroc, const MINGROCParam<Scalar> &param,
    const NNIpp::NaturalNeighborInterpolant<Scalar> &NNI,
    const Eigen::Matrix<Index, Eigen::Dynamic, 1> &fixIDx,
    const Vector &drt, const CplxVector &dw, const Vector &grad,
    bool calcGrowthEnergy, bool calcMuEnergy,
    Scalar &fx, Vector &x, CplxVector &w, Scalar &step ) {


  int numF = mingroc.m_F.rows(); // The number of faces
  int numV = mingroc.m_V.rows(); // The number of vertices

  // Decreasing and increasing factors
  const Scalar dec = Scalar(0.5);
  const Scalar inc = Scalar(2.1);

  // Check the initial step length
  if ( step < Scalar(0.0) )
    std::invalid_argument("'step' must be positive");

  // Make a copy of the system's current state
  const Vector xp = x; // The current Beltrami coefficient values
  const CplxVector wp = w; // The current quasiconformal mapping
  const Scalar fx_init = fx; // Save the function value at the current x
  const Scalar dg_init = grad.dot(drt); // Projection of gradient onto search direction

  // Make sure the search direction is a descent direction
  if ( dg_init > Scalar(0.0) )
    throw std::logic_error("The update direction increases the objective function value");

  const Scalar test_decr = param.ftol * dg_init;
  Scalar width;
  bool validStep;

  // The complex Beltrami coefficient
  CplxVector mu( numV, 1 );

  // Some helper variables to check for self-intersections
  // Why the hell do I need to know the column sizes at 
  // compile time here?
  // Eigen::Matrix<Scalar, Eigen::Dynamic, 3> nullEV = 
  //  Eigen::Matrix<Scalar, Eigen::Dynamic, 3>::Zero(1,3);
  Eigen::Matrix<Index, Eigen::Dynamic, 2> nullIF =
    Eigen::Matrix<Index, Eigen::Dynamic, 2>::Zero(1,2);
  Eigen::Array<bool, Eigen::Dynamic, 1> nullCP =
    Eigen::Array<bool, Eigen::Dynamic, 1>::Constant(1, 1, false);
  // Eigen::Matrix<Index, Eigen::Dynamic, 2> nullEE =
  //  Eigen::Matrix<Index, Eigen::Dynamic, 2>::Zero(1,2);
  // Eigen::Matrix<Index, Eigen::Dynamic, 1> nullEI =
  //  Eigen::Matrix<Index, Eigen::Dynamic, 1>::Zero(1,1);

  // Some helper variables to calculate the energy
  Matrix map3D(numV, 3);
  Vector gamma(numV, 1);

  for( int iter = 0; iter <= param.maxLineSearch; iter++ ) {

    // Evaluate the current candidate ---------------------------------------------------

    // Update the unknown vector
    // x_{k+1} = x_k + step * d_k
    x.noalias() = xp + step * drt;

    // Update the Beltrami coefficient
    mingroc.convertRealToComplex(x, mu);

    // Update the quasiconformal mapping
    w.noalias() = wp + step * dw;

    // Clip the boundary points of the updated mapping to the unit circle
    MINGROCpp::clipToUnitCircle( mingroc.m_bdyIDx, w );

    // Pin fixed points, if necessary
    for(int i = 0; i < fixIDx.size(); i++)
      w(fixIDx(i)) = wp(fixIDx(i));

    // Reject current step size if it produces an invalid candidate
    validStep = ( (mu.array().abs() < 1.0).all() )
      && ( (w.array().abs() <= 1.0).all() );
    
    // Reject current step size if it produces a self-intersection in the virtual
    // isothermal parameterization
    if (param.checkSelfIntersections)
    {
      Matrix w3D(numV, 3);
      w3D << w.real(), w.imag(), Vector::Zero(numV);

      // std::cout << "Checking for self-intersections... ";
      // bool intersects = igl::predicates::find_self_intersections(
      //    w3D, mingroc.m_F, nullIF, nullCP, nullEV, nullEE, nullEI);
      bool intersects = igl::predicates::find_self_intersections(
          w3D, mingroc.m_F, true, nullIF, nullCP);
      // std::cout << (intersects ? "Found" : "Not found") << std::endl;

      validStep = validStep && (!intersects);
    }

    if ( !validStep ) {

      width = dec;

      if ( iter >= param.maxLineSearch ) {
        throw std::runtime_error(
            "The line search routine reached the maximum number of iterations" );
      }

      if ( step < param.minStep ) {
        throw std::runtime_error(
            "The line search step became smaller than the minimum allowed value" );
      }
      

      if ( step > param.maxStep ) {
        throw std::runtime_error(
            "The line search step became larger than the maximum allowed value" );
      }

      step *= width;

      continue;

    }

    // Evaluate the energy at the new location
    fx = mingroc.calculateEnergy(mu, w, NNI, calcGrowthEnergy, calcMuEnergy,
        map3D, gamma);

    // Evaluate line search termination conditions --------------------------------------
    
    // Reject a step if it results in Inf or NaN
    if ( (fx != fx) || std::isinf( (double) fx ) ) {

      width = dec;

    // Accept any valid step, even if it increases the energy
    } else if ( param.lineSearchTermination == LINE_SEARCH_TERMINATION_NONE ) {

      break;

    } else if ( fx > fx_init ) {

      width = dec;

    // Accept any step that at least decreases the energy
    } else if ( param.lineSearchTermination == LINE_SEARCH_TERMINATION_DECREASE ) {

      break;
    
    } else if (fx > fx_init + step * test_decr ) {

      width = dec;

    } else {

      // Armijo condition is met
      if ( param.lineSearchTermination == LINE_SEARCH_TERMINATION_ARMIJO ) {

        break;

      } else {

        throw std::invalid_argument("Invalid line search termination procedure");

      }

    }

    if ( iter >= param.maxLineSearch ) {
      throw std::runtime_error(
          "The line search routine reached the maximum number of iterations" );
    }

    if ( step < param.minStep ) {
      throw std::runtime_error(
          "The line search step became smaller than the minimum allowed value" );
    }

    if ( step > param.maxStep ) {
      throw std::runtime_error(
          "The line search step became larger than the maximum allowed value" );
    }

    step *= width;

  }

};

// TODO: Add explicit template instantiation
#ifdef MINGROC_STATIC_LIBRARY
#endif
