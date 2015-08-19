#include <iostream>
#include <iomanip>
#include <mpi.h>

#include "mpi_evaluation.h"
#include "mpi_derivatives.h"
#include "blas.h"
#include "mpi_matrix.h"
#include "mpi_precon.h"
#include "mpi_init.h"
#include "../average.h"

const double lx = 2.*M_PI;
const double ly = M_PI;

double function( double x, double y) {return cos(x)*sin(y);}
double pol_average( double x, double y) {return cos(x)*2./M_PI;}

dg::bc bcx = dg::PER; 
dg::bc bcy = dg::PER;

int main(int argc, char* argv[])
{
    MPI_Init( &argc, &argv);
    int rank;
    unsigned n, Nx, Ny; 

    MPI_Comm comm;
    mpi_init2d( bcx, bcy, n, Nx, Ny, comm);
    MPI_Comm_rank( MPI_COMM_WORLD, &rank);

    dg::MPI_Grid2d g( 0, lx, 0, ly, n, Nx, Ny, bcx, bcy, comm);
    

    std::cout << "constructing polavg" << std::endl;
    dg::PoloidalAverage<dg::MVec,dg::HVec > pol(g);
    std::cout << "constructing polavg end" << std::endl;
    dg::MVec vector = dg::evaluate( function ,g), average_y( vector);
    const dg::MVec solution = dg::evaluate( pol_average, g);
    std::cout << "Averaging ... \n";
    pol( vector, average_y);
    dg::blas1::axpby( 1., solution, -1., average_y, vector);
    std::cout << "Distance to solution is: "<<        sqrt(dg::blas2::dot( vector, dg::create::weights( g), vector))<<std::endl;

    MPI_Finalize();
    return 0;
}