#include <iostream>
#include <iomanip>
#include <vector>


#include "toeflR.cuh"
#include "parameters.h"
#include "dg/rk.cuh"
#include "file/file.h"
#include "file/read_input.h"

#include "dg/timer.cuh"


/*
   - reads parameters from input.txt or any other given file, 
   - integrates the ToeflR - functor and 
   - writes outputs to a given outputfile using hdf5.
*/
using namespace std;
using namespace dg;

const unsigned k = 3;

using namespace std;

int main( int argc, char* argv[])
{
    //Parameter initialisation
    std::vector<double> v;
    std::string input;
    if( argc != 3)
    {
        cerr << "ERROR: Wrong number of arguments!\nUsage: "<< argv[0]<<" [inputfile] [outputfile]\n";
        return -1;
    }
    else 
    {
        v = file::read_input( argv[1]);
        input = file::read_file( argv[1]);
    }
    const Parameters p( v);
    if( p.k != k)
    {
        cerr << "ERROR: k doesn't match: "<<k<<" (code) vs. "<<p.k<<" (input)\n";
        return -1;
    }

    //set up computations
    dg::Grid<double > grid( 0, p.lx, 0, p.ly, p.n, p.Nx, p.Ny, p.bc_x, p.bc_y);
    //create RHS 
    dg::ToeflR<dg::DVec > test( grid, p.kappa, p.nu, p.tau, p.eps_pol, p.eps_gamma, p.global); 
    //create initial vector
    dg::Gaussian g( p.posX*grid.lx(), p.posY*grid.ly(), p.sigma, p.sigma, p.n0); 
    std::vector<dg::DVec> y0(2, dg::evaluate( g, grid)), y1(y0); // n_e' = gaussian
    blas2::symv( test.gamma(), y0[0], y0[1]); // n_e = \Gamma_i n_i -> n_i = ( 1+alphaDelta) n_e' + 1
    blas2::symv( (dg::DVec)create::v2d( grid), y0[1], y0[1]);
    if( p.global)
    {
        thrust::transform( y0[0].begin(), y0[0].end(), y0[0].begin(), dg::PLUS<double>(+1));
        thrust::transform( y0[1].begin(), y0[1].end(), y0[1].begin(), dg::PLUS<double>(+1));
        test.log( y0, y0); //transform to logarithmic values
    }
    //create timestepper
    dg::AB< k, std::vector<dg::DVec> > ab( y0);
    /////////////////////////////////////////////////////////////////////////
    //set up hdf5
    dg::HVec output( y1[0]); //intermediate transport location
    hid_t   file, grp;
    herr_t  status;
    hsize_t dims[] = { grid.n()*grid.Ny(), grid.n()*grid.Nx() };
    //dims[0] = n*grid.Ny(); 
    //dims[1] = n*grid.Nx(); 
    file = H5Fcreate( argv[2], H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
    //std::stringstream title; 
    hsize_t size = input.size();
    status = H5LTmake_dataset_char( file, "inputfile", 1, &size, input.data()); //name should precede t so that reading is easier
    /////////////////////////////////////////////////////////////////////////
    double time = 0;
    ab.init( test, y0, p.dt);
    /////////////////////////////////////first output (with zero potential)
    if( p.global)
        test.exp( y0,y1); //transform to logarithmic values
    grp = H5Gcreate( file, "t=0", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT  );
    //output all three fields
    output = y1[0];
    status = H5LTmake_dataset_double( grp, "electrons", 2,  dims, output.data());
    output = y1[1];
    status = H5LTmake_dataset_double( grp, "ions", 2,  dims, output.data());
    blas1::axpby( 0., output, 0., output); //set output zero as it should be
    status = H5LTmake_dataset_double( grp, "potential", 2,  dims, output.data());
    H5Gclose( grp);

    //title << std::setfill('0');
    ///////////////////////////////////Timeloop////////////////////////////////
    for( unsigned i=0; i<p.maxout; i++)
    {
        for( unsigned i=0; i<p.itstp; i++)
        {
            try{ ab( test, y0, y1, p.dt);}
            catch( dg::Fail& fail) { 
                cerr << "CG failed to converge to "<<fail.epsilon()<<"\n";
                cerr << "Does Simulation respect CFL condition?\n";
                H5Fclose( file);
                return -1;
            }
            y0.swap( y1); //attention on -O3 ?
        }
        time += p.itstp*p.dt;
        if( p.global)
            test.exp( y0,y1); //transform to logarithmic values
        
        //title << "t=";
        //title <<std::setw(6)<<std::right<<(unsigned)(floor(time))<<"."<<std::setw(6)<<std::left<<(unsigned)((time-floor(time))*1e6);
        grp = H5Gcreate( file, file::setTime( time).data(), H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT  );

        //title.str("");
        //output all three fields
        output = y1[0]; //electrons
        status = H5LTmake_dataset_double( grp, "electrons", 2,  dims, output.data());
        output = y1[1]; //ions
        status = H5LTmake_dataset_double( grp, "ions", 2,  dims, output.data());
        output = test.potential()[0];
        status = H5LTmake_dataset_double( grp, "potential", 2,  dims, output.data());
        H5Gclose( grp);
    }

    //writing takes the same time as device-host transfers
    ////////////////////////////////////////////////////////////////////
    H5Fclose( file);

    return 0;

}
