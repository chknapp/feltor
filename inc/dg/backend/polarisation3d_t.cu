#include <iostream>
#include <iomanip>

#include <cusp/print.h>

#include "xspacelib.cuh"
#include "cg.h"

unsigned n = 3; //global relative error in L2 norm is O(h^P)
unsigned Nx = 20;  //more N means less iterations for same error
unsigned Ny = 20;  //more N means less iterations for same error
unsigned Nz = 4;

const double lx = M_PI;
const double ly = M_PI;
double eps = 1e-3; //# of pcg iterations increases very much if 
 // eps << relativer Abstand der exakten Lösung zur Diskretisierung vom Sinus

double initial( double x, double y, double z) {return 0.;}
//double pol( double x, double y) {return 1. + sin(x)*sin(y); } //must be strictly positive
//double pol( double x, double y) {return 1.; }
double pol( double x, double y, double z) {return 1. + sin(x)*sin(y) + x; } //must be strictly positive
double sol(double x, double y, double z)  { return sin( x)*sin(y);}

//double rhs( double x, double y, double z) { return 2.*sin(x)*sin(y)*(sin(x)*sin(y)+1)-sin(x)*sin(x)*cos(y)*cos(y)-cos(x)*cos(x)*sin(y)*sin(y);}
//double rhs( double x, double y) { return 4.*sol(x,y)*sol(x,y) + 2.*sol(x,y);}
//double rhs( double x, double y) { return 2.*sin( x)*sin(y);}
double rhs( double x, double y, double z) { return 2.*sin(x)*sin(y)*(sin(x)*sin(y)+1)-sin(x)*sin(x)*cos(y)*cos(y)-cos(x)*cos(x)*sin(y)*sin(y)+(x*sin(x)-cos(x))*sin(y) + x*sin(x)*sin(y);}

using namespace std;

int main()
{
    std::cout << "Write n Nx Ny and eps!\n";
    std::cin >> n >> Nx >> Ny >> eps;
    dg::Grid3d<double> grid( 0, lx, 0, ly,0., 1., n, Nx, Ny, Nz, dg::DIR, dg::DIR, dg::PER);
    dg::DVec v3d = dg::create::v3d( grid);
    dg::DVec w3d = dg::create::w3d( grid);
    //create functions A(chi) x = b
    dg::DVec x =    dg::evaluate( initial, grid);
    dg::DVec b =    dg::evaluate( rhs, grid);
    dg::DVec chi =  dg::evaluate( pol, grid);
    const dg::DVec solution = dg::evaluate( sol, grid);
    dg::DVec error( solution);

    cout << "Create Polarisation object!\n";
    dg::Polarisation2dX<dg::HVec> pol( grid);
    cout << "Create Polarisation matrix!\n";
    dg::DMatrix A = pol.create( chi ); 
    dg::Matrix Ap= dg::create::laplacianM_perp( grid, dg::not_normed); 
    //cout << "Polarisation matrix: "<< endl;
    //cusp::print( A);
    //cout << "Laplacian    matrix: "<< endl;
    //cusp::print( Ap);
    cout << "Create conjugate gradient!\n";
    dg::CG< dg::DVec> pcg( x, n*n*Nx*Ny);

    cout << "# of polynomial coefficients: "<< n <<endl;
    cout << "# of 2d cells                 "<< Nx*Ny <<endl;
    //compute W b
    dg::blas2::symv( w3d, b, b);
    std::cout << "Number of pcg iterations "<< pcg( A, x, b, v3d, eps)<<endl;
    cout << "For a precision of "<< eps<<endl;
    //compute error
    dg::blas1::axpby( 1.,x,-1., error);

    double eps = dg::blas2::dot( w3d, error);
    cout << "L2 Norm2 of Error is " << eps << endl;
    double norm = dg::blas2::dot( w3d, solution);
    std::cout << "L2 Norm of relative error is "<<sqrt( eps/norm)<<std::endl;

    return 0;
}
