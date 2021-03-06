#pragma once

/*! @file
 * Includes all container independent headers of the dg library.
 *
 * @note include <mpi.h> before this header to activate mpi support
 */
#include "blas.h"
#include "geometry.h"
#include "arakawa.h"
#include "helmholtz.h"
#include "cg.h"
#include "exceptions.h"
#include "functors.h"
#include "multistep.h"
#include "elliptic.h"
#include "runge_kutta.h"
#include "ds.h"
