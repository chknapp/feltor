#include <iostream>
#include <iomanip>
#include <vector>
#include <sstream>

#include "draw/host_window.h"
#include "dg/backend/xspacelib.cuh"
#include "dg/backend/timer.cuh"
#include "dg/algorithm.h"
#include "file/read_input.h"
#include "file/file.h"

#include "toefl/parameters.h"
//#include "lamb_dipole/parameters.h"


//can read TOEFL and INNTO h5-files and plot them on screen

int main( int argc, char* argv[])
{
    dg::Timer t;
    std::stringstream title;
    std::vector<double> v = file::read_input( "window_params.txt");
    GLFWwindow* w = draw::glfwInitAndCreateWindow( v[3]*v[2], v[4]*v[1], "");
    draw::RenderHostData render( v[1], v[2]);

    if( argc != 2)
    {
        std::cerr << "Usage: "<<argv[0]<<" [inputfile]\n";
        return -1;
    }

    std::string in;
    file::T5rdonly t5file( argv[1], in);
    unsigned nlinks = t5file.get_size();
    //std::cout <<"NLINKS "<<nlinks<<"\n";

    int layout = 0;
    if( in.find( "TOEFLI") != std::string::npos)
    {
        layout = 2;
        std::cout << "Found Impurity file!\n";
    }
    else if( in.find( "INNTO_HW") != std::string::npos)
    {
        layout = 3;
        std::cout << "Found INNTO_HW file!\n";
    }
    else if( in.find( "INNTO") != std::string::npos)
    {
        layout = 1;
        std::cout << "Found INNTO file!\n";
    }
    else if( in.find( "TOEFL") != std::string::npos)
    {
        layout = 0;
        std::cout << "Found TOEFL file!\n";
    }
    else 
        std::cerr << "Unknown input file format: default to 0"<<std::endl;
    const Parameters p( file::read_input( in), layout);

    p.display();
    dg::Grid2d<double> grid( 0, p.lx, 0, p.ly, p.n, p.Nx, p.Ny, p.bc_x, p.bc_y);
    dg::HVec visual(  grid.size(), 0.), input( visual);
    dg::IHMatrix equi = dg::create::backscatter( grid);
    dg::Elliptic<dg::HMatrix, dg::HVec, dg::HVec> laplacianM( grid, dg::normed);
    draw::ColorMapRedBlueExt colors( 1.);
    //create timer
    unsigned index = 1;
    std::cout << "PRESS N FOR NEXT FRAME!\n";
    std::cout << "PRESS P FOR PREVIOUS FRAME!\n";
    std::vector<double> mass, energy, diffusion, dissipation, massAcc, energyAcc;
    if( p.global )
    {
        t5file.get_xfile( mass, "mass");
        t5file.get_xfile( energy, "energy");
        t5file.get_xfile( diffusion, "diffusion");
        t5file.get_xfile( dissipation, "dissipation");
        massAcc.resize(mass.size()), energyAcc.resize(mass.size());
        mass.insert(mass.begin(), 0), mass.push_back(0);
        energy.insert( energy.begin(), 0), energy.push_back(0);
        for(unsigned i=0; i<massAcc.size(); i++ )
        {
            massAcc[i] = (mass[i+2]-mass[i])/2./p.dt; //first column
            massAcc[i] = fabs(2.*(massAcc[i]-diffusion[i])/(massAcc[i]+diffusion[i]));
            energyAcc[i] = (energy[i+2]-energy[i])/2./p.dt;
            energyAcc[i] = fabs(2.*(energyAcc[i]-dissipation[i])/(energyAcc[i]+dissipation[i]));
        }
    }

    std::cout << std::scientific << std::setprecision( 2);
    /*
        bool waiting = true;
        do
        {
            glfwPollEvents();
            if( glfwGetKey( 'S')){
                waiting = false;
            }
        }while( waiting && !glfwGetKey( GLFW_KEY_ESC) && glfwGetWindowParam( GLFW_OPENED));
        */
    while (!glfwWindowShouldClose(w) && index < nlinks + 1 )
    {
        t.tic();
        t5file.get_field( input, "electrons", index);
        t.toc();
        //std::cout << "Reading of electrons took "<<t.diff()<<"s\n";
        t.tic();
        if( p.global)
        {
            if( in.find( "SOL") != std::string::npos)
                std::cout << "Hello SOL\n";
            else if( in.find( "SHU") != std::string::npos)
                std::cout << "Hello SHU\n";
            else
                thrust::transform( input.begin(), input.end(), input.begin(), dg::PLUS<double>(-1));
        }

        dg::blas2::gemv( equi, input, visual);

        //compute the color scale
        colors.scale() =  (float)thrust::reduce( visual.begin(), visual.end(), 0., dg::AbsMax<double>() );
        //colors.scale() = p.n0;
        if( v[6] > 0) colors.scale() = v[6];
        t.toc();
        //std::cout << "Computing colorscale took "<<t.diff()<<"s\n";
        //draw ions
        title << std::setprecision(2) << std::scientific;
        title <<"ne / "<<colors.scale()<<"\t";
        t.tic();
        render.renderQuad( visual, grid.n()*grid.Nx(), grid.n()*grid.Ny(), colors);
        t.toc();
        //std::cout << "Drawing took              "<<t.diff()<<"s\n";
        if( (layout == 2 || layout == 3) && v[1]*v[2]>2 )
        {
            //draw ions
            t5file.get_field( input, "ions", index);
            thrust::transform( input.begin(), input.end(), input.begin(), dg::PLUS<double>(-1));
            dg::blas2::gemv( equi, input, visual);
            colors.scale() =  (float)thrust::reduce( visual.begin(), visual.end(), 0., dg::AbsMax<double>() );
            title <<"ni / "<<colors.scale()<<"\t";
            render.renderQuad( visual, grid.n()*grid.Nx(), grid.n()*grid.Ny(), colors);
            //draw impurities
            t5file.get_field( input, "impurities", index);
            thrust::transform( input.begin(), input.end(), input.begin(), dg::PLUS<double>(-1));
            dg::blas2::gemv( equi, input, visual);
            colors.scale() =  (float)thrust::reduce( visual.begin(), visual.end(), 0., dg::AbsMax<double>() );
            title <<"nz / "<<colors.scale()<<"\t";
            render.renderQuad( visual, grid.n()*grid.Nx(), grid.n()*grid.Ny(), colors);
        }

        //transform phi
        t.tic();
        t5file.get_field( input, "potential", index);
        //Vorticity is \curl \bm{u}_E \approx \frac{\Delta\phi}{B}
        dg::blas2::gemv( laplacianM, input, visual);
        input.swap( visual);
        dg::blas2::gemv( equi, input, visual);
        dg::blas1::axpby( -1., visual, 0., visual);//minus laplacian
        if( p.global)
        {
            std::cout <<"(m_tot-m_0)/m_0: "<<(mass[(index-1)*p.itstp+1]-mass[1])/(mass[1]-grid.lx()*grid.ly()) //blob mass is mass[] - Area
                      <<"\t(E_tot-E_0)/E_0: "<<(energy[1+(index-1)*p.itstp]-energy[1])/energy[1]
                      <<"\tAccuracy: "<<energyAcc[(index-1)*p.itstp]<<std::endl;
        }

        //compute the color scale
        colors.scale() =  (float)thrust::reduce( visual.begin(), visual.end(), 0., dg::AbsMax<double>() );
        if(colors.scale() == 0) { colors.scale() = 1;}
        if( v[7] > 0)
            colors.scale() = v[7];
        //draw phi and swap buffers
        title <<"omega / "<<colors.scale()<<"\t";
        title << std::fixed; 
        title << " && time = "<<t5file.get_time( index);
        glfwSetWindowTitle( w, title.str().c_str());
        title.str("");
        render.renderQuad( visual, grid.n()*grid.Nx(), grid.n()*grid.Ny(), colors);
        t.toc();
        glfwPollEvents();
        glfwSwapBuffers( w);
        //std::cout <<"2nd half took          "<<t.diff()<<"s\n";
        bool waiting = true;
        do
        {
            glfwPollEvents();
            if( glfwGetKey(w, 'B')||glfwGetKey(w, 'P') ){
                index -= v[5];
                waiting = false;
            }
            else if( glfwGetKey(w, 'N') ){
                index +=v[5];
                waiting = false;
            }
            //glfwWaitEvents();
        }while( waiting && !glfwGetKey(w, GLFW_KEY_ESCAPE) );
    }
    glfwTerminate();
    return 0;
}
