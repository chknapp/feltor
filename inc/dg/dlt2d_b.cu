
#include <iostream>

#include <thrust/host_vector.h>
#include <thrust/device_vector.h>
#include <cusp/ell_matrix.h>
#include <cusp/dia_matrix.h>

#include "blas.h"
#include "grid.cuh"
#include "laplace.cuh"
#include "tensor.cuh"
#include "timer.cuh"
#include "array.cuh"
#include "dlt.h"
#include "arrvec2d.cuh"
#include "evaluation.cuh"
#include "preconditioner.cuh"
#include "operator_dynamic.h"
#include "operator_matrix.cuh"
#include "tensor.cuh"


using namespace std;
using namespace dg;

const unsigned n = 3; //thrust is faster for 2, equal for 3 and slower for 4 
const unsigned Nx = 1e2;
const unsigned Ny = 1e2;

typedef thrust::device_vector<double>   DVec;
typedef thrust::host_vector<double>     HVec;

typedef ArrVec2d< HVec> HArrVec;
typedef ArrVec2d< DVec> DArrVec;
typedef cusp::ell_matrix<int, double, cusp::host_memory>   HMatrix;
typedef cusp::ell_matrix<int, double, cusp::device_memory> DMatrix;

double function( double x, double y ) { return sin(x)*sin(y);}

int main()
{
    cout << "# of Legendre coefficients: " << n<<endl;
    cout << "# of grid cells:            " << Nx*Ny<<endl;
    Timer t;
    Grid<double> g( 0, 2.*M_PI, 0., 2.*M_PI, n, Nx, Ny);
    HArrVec hv (evaluate( function, g ), n, Nx);
    HArrVec hv2( hv);
    DArrVec  dv( hv);
    DArrVec  dv2( hv2);
    Operator<double> forward = create::forward( n);
    Operator<double> forward2d = dg::tensor( forward, forward);
    //t.tic();
    //dg::blas2::symv(1., forward2d, dv.data(),0., dv.data());
    //t.toc();
    //cout << "Forward thrust transform took      "<<t.diff()<<"s\n";
    //t.tic();
    //dg::blas2::symv( forward2d, dv2.data(), dv2.data());
    //t.toc();
    //cout << "Forward thrust transform 2nd took  "<<t.diff()<<"s\n";

    HMatrix hforwardy = dgtensor<double>(n, tensor(Ny, forward), tensor(Nx, create::delta(n)));
    DMatrix dforwardy( hforwardy);
    HMatrix hforwardx = dgtensor<double>(n, tensor(Ny, create::delta(n)), tensor(Nx, forward));
    DMatrix dforwardx( hforwardx);
    t.tic();
    blas2::symv( dforwardy, dv2.data(), dv2.data());
    t.toc();
    cout << "Foward - y cusp transform took     "<<t.diff()<<"s\n";
    t.tic();
    blas2::symv( dforwardx, dv2.data(), dv2.data());
    t.toc();
    cout << "Foward - x cusp transform took     "<<t.diff()<<"s\n";
    //t.tic();
    //blas2::symv( dforwardy, dv2.data(), dv2.data());
    //blas2::symv( forward, dv2.data(), dv2.data());
    //t.toc();
    //cout << "Foward cusp-thrust transform took  "<<t.diff()<<"s\n";
    t.tic();
    blas2::symv( dforwardy, dv2.data(), dv2.data());
    blas2::symv( dforwardx, dv2.data(), dv2.data());
    t.toc();
    cout << "Foward cusp-cusp transform took    "<<t.diff()<<"s\n";

    HMatrix hforwardxy = dgtensor<double>(n, tensor(Ny, forward), tensor(Nx, forward));
    DMatrix dforwardxy( hforwardxy);
    t.tic();
    blas2::symv( dforwardxy, dv2.data(), dv2.data());
    t.toc();
    cout << "Foward cusp transform took         "<<t.diff()<<"s\n";
    
    return 0;
}
