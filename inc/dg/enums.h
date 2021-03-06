#pragma once
#include "exceptions.h"

/*! @file 
  
  enums
  */

namespace dg
{
/**
 * @brief Switch between boundary conditions
 * 
 * @ingroup creation
 */
enum bc{ 
    PER = 0, //!< periodic boundaries
    DIR = 1, //!< homogeneous dirichlet boundaries
    DIR_NEU = 2, //!< Dirichlet on left, Neumann on right boundary
    NEU_DIR = 3, //!< Neumann on left, Dirichlet on right boundary
    NEU = 4 //!< Neumann on both boundaries
};

bc str2bc( std::string s)
{
    if( s=="PER"||s=="per"||s=="periodic"||s=="Periodic")
        return PER;
    if( s=="DIR"||s=="dir"||s=="dirichlet"||s=="Dirichlet")
        return DIR;
    if( s=="NEU"||s=="neu"||s=="neumann"||s=="Neumann")
        return NEU;
    if( s=="NEU_DIR"||s=="neu_dir")
        return NEU_DIR;
    if( s=="DIR_NEU"||s=="dir_neu")
        return DIR_NEU;
    throw Ooops( "No matching boundary condition!");
}

/**
 * @brief Switch between normalisations
 *
 * @ingroup creation
 */
enum norm{
    normed,   //!< indicates that output is properly normalized
    not_normed //!< indicates that normalisation weights (either T or V) are missing from output
};
/**
 * @brief Direction of a discrete derivative
 *
 * @ingroup creation
 */
enum direction{
    forward, //!< forward derivative
    backward, //!< backward derivative
    centered //!< centered derivative
};
/**
 * @brief Coordinate system
 *
 * @ingroup creation
 */
enum system{
    cartesian, //!< cartesian coordinate system
    cylindrical //!< cylindrical coordinate system
};
}//namespace dg
