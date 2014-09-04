#include <iostream>
#include <iomanip>

#include <thrust/host_vector.h>
#include <mpi.h>
#include "backend/timer.cuh"
#include "backend/mpi_evaluation.h"
#include "backend/mpi_derivatives.h"

#include "elliptic.h"
#include "cg.h"
#include "backend/mpi_init.h"

//leo3 can do 350 x 350 but not 375 x 375
const double ly = 2.*M_PI;

const double eps = 1e-6; //# of pcg iterations increases very much if 
 // eps << relativer Abstand der exakten Lösung zur Diskretisierung vom Sinus

const double lx = 2.*M_PI;
const double lz = 1.;
double fct(double x, double y, double z){ return sin(y)*sin(x);}
double laplace_fct( double x, double y, double z) { return 2*sin(y)*sin(x);}
dg::bc bcx = dg::DIR;
double initial( double x, double y, double z) {return sin(0);}


int main( int argc, char* argv[])
{
    MPI_Init(&argc, &argv);
    unsigned n, Nx, Ny, Nz; 
    MPI_Comm comm;
    mpi_init3d( bcx, dg::PER, dg::PER, n, Nx, Ny, Nz, comm);

    dg::MPI_Grid3d grid( 0., lx, 0, ly, 0, lz, n, Nx, Ny,Nz, bcx, dg::PER,dg::PER, dg::cartesian, comm);
    const dg::MPrecon w3d = dg::create::weights( grid);
    const dg::MPrecon v3d = dg::create::inv_weights( grid);
    int rank;
    MPI_Comm_rank( MPI_COMM_WORLD, &rank);
    if( rank == 0) std::cout<<"Expand initial condition\n";
    dg::MVec x = dg::evaluate( initial, grid);

    if( rank == 0) std::cout << "Create symmetric Laplacian\n";
    dg::Timer t;
    t.tic();
    dg::Elliptic<dg::MMatrix, dg::MVec, dg::MPrecon> A ( grid, dg::not_normed); 
    t.toc();
    if( rank == 0) std::cout<< "Creation took "<<t.diff()<<"s\n";

    dg::CG< dg::MVec > pcg( x, n*n*Nx*Ny*Nz);
    if( rank == 0) std::cout<<"Expand right hand side\n";
    const dg::MVec solution = dg::evaluate ( fct, grid);
    dg::MVec b = dg::evaluate ( laplace_fct, grid);
    //compute W b
    dg::blas2::symv( w3d, b, b);
    
    t.tic();
    int number = pcg( A, x, b, v3d, eps);
    t.toc();
    if( rank == 0)
    {
        std::cout << "# of pcg itersations   "<<number<<std::endl;
        std::cout << "... for a precision of "<< eps<<std::endl;
        std::cout << "...               took "<< t.diff()<<"s\n";
    }

    dg::MVec  error(  solution);
    dg::blas1::axpby( 1., x,-1., error);

    double normerr = dg::blas2::dot( w3d, error);
    double norm = dg::blas2::dot( w3d, solution);
    if( rank == 0) std::cout << "L2 Norm of relative error is:  " <<sqrt( normerr/norm)<<std::endl;

    MPI_Finalize();
    return 0;
}