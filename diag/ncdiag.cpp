#include <iostream>
#include <fstream>
#include <iomanip>
#include <vector>
#include <string>

#include "dg/algorithm.h"
#include "dg/poisson.h"

#include "dg/backend/interpolation.cuh"
#include "dg/backend/xspacelib.cuh"
#include "dg/functors.h"

#include "file/read_input.h"
#include "file/nc_utilities.h"

#include "solovev/geometry.h"
#include "solovev/init.h"

#define RADIALDIFFUSIONCOEFF
#define GRADIENTLENGTH
int main( int argc, char* argv[])
{
    if( argc != 4)
    {
        std::cerr << "Usage: "<<argv[0]<<" [input.nc] [outputfsa.nc] [output2d.nc]\n";
        return -1;
    }
//     std::ofstream os( argv[2]);
    std::cout << argv[1]<< " -> "<<argv[2]<<" & "<<argv[3]<<std::endl;

    //////////////////////////////open nc file//////////////////////////////////
    file::NC_Error_Handle err;
    int ncid;
    err = nc_open( argv[1], NC_NOWRITE, &ncid);
    ///////////////////read in and show inputfile und geomfile//////////////////
    size_t length;
    err = nc_inq_attlen( ncid, NC_GLOBAL, "inputfile", &length);
    std::string input( length, 'x');
    err = nc_get_att_text( ncid, NC_GLOBAL, "inputfile", &input[0]);
    err = nc_inq_attlen( ncid, NC_GLOBAL, "geomfile", &length);
    std::string geom( length, 'x');
    err = nc_get_att_text( ncid, NC_GLOBAL, "geomfile", &geom[0]);
    std::cout << "input "<<input<<std::endl;
    std::cout << "geome "<<geom <<std::endl;
    const eule::Parameters p(file::read_input( input));
    const solovev::GeomParameters gp(file::read_input( geom));
    p.display();
    gp.display();
    ///////////////////////////////////////////////////////////////////////////

    double Rmin=gp.R_0-p.boxscaleRm*gp.a;
    double Zmin=-p.boxscaleZm*gp.a*gp.elongation;
    double Rmax=gp.R_0+p.boxscaleRp*gp.a; 
    double Zmax=p.boxscaleZp*gp.a*gp.elongation;
    //old boxscale
//     double Rmin=gp.R_0-p.boxscaleRp*gp.a;
//     double Zmin=-p.boxscaleRp*gp.a*gp.elongation;
//     double Rmax=gp.R_0+p.boxscaleRp*gp.a; 
//     double Zmax=p.boxscaleRp*gp.a*gp.elongation;

    //Grids
    dg::Grid3d<double > g3d_out( Rmin,Rmax, Zmin,Zmax, 0, 2.*M_PI, p.n_out, p.Nx_out, p.Ny_out, p.Nz_out, dg::NEU, dg::NEU, dg::PER, dg::cylindrical);  
    dg::Grid2d<double>  g2d_out( Rmin,Rmax, Zmin,Zmax,p.n_out, p.Nx_out, p.Ny_out, dg::NEU, dg::NEU);
    //1d grid
    solovev::Psip psip(gp);
    dg::HVec w3d = dg::create::weights( g3d_out);   
    
    dg::HVec psipog2d   = dg::evaluate( psip, g2d_out);    
    double psipmin = (float)thrust::reduce( psipog2d.begin(), psipog2d.end(), 0.0,thrust::minimum<double>()  );
    double psipmax = (float)thrust::reduce( psipog2d.begin(), psipog2d.end(),psipmin,thrust::maximum<double>()  );
//     double psipmax = 0.0;
    solovev::PsiPupil psipupil(gp,psipmax);
    dg::HVec psipupilog2d   = dg::evaluate( psipupil, g2d_out);    
    dg::HVec psipupilog3d   = dg::evaluate( psipupil, g3d_out);    

    unsigned Npsi = 25;//set number of psivalues
    std::cout << "psipmin =" << psipmin << " psipmax =" << psipmax << " Npsi =" << Npsi  <<std::endl;
    dg::Grid1d<double>  g1d_out(psipmin ,psipmax, 3, Npsi,dg::DIR); //one dimensional sipgrid
    dg::HVec w1d = dg::create::weights( g1d_out);   
    //read in midplane of electrons, ions Ue, Ui, and potential, and energy
    std::string names[5] = {"electrons", "ions", "Ue", "Ui", "potential"}; 
    int dataIDs[5];

    std::string names2d[14] = {"Ne_avg", "Ni_avg", "Ue_avg", "Ui_avg", "phi_avg","dNe_mp", "dNi_mp", "dUe_mp", "dUi_mp", "dphi_mp","vor_avg","Deperp_avg","Depsi_avg","Lperpinv_avg"}; 
    int dataIDs2d[14];
     //generate 2d nc file for one time step
    file::NC_Error_Handle err2d; 
    int ncid2d; 
    err2d = nc_create(argv[3],NC_NETCDF4|NC_CLOBBER, &ncid2d);
    err2d = nc_put_att_text( ncid2d, NC_GLOBAL, "inputfile", input.size(), input.data());
    err2d = nc_put_att_text( ncid2d, NC_GLOBAL, "geomfile", geom.size(), geom.data());
    int dim_ids[3], tvarID;
    err2d = file::define_dimensions( ncid2d, dim_ids, &tvarID, g2d_out);
    for( unsigned i=0; i<14; i++){
//         for( unsigned i=0; i<10; i++){
        err2d = nc_def_var( ncid2d, names2d[i].data(), NC_DOUBLE, 3, dim_ids, &dataIDs2d[i]);
    }   
    //midplane 2d fields
    size_t count2d[3]  = {1, g3d_out.n()*g3d_out.Ny(), g3d_out.n()*g3d_out.Nx()};
    size_t start2d[3]  = {0, 0, 0};
    size_t count3d[4]  = {1, g3d_out.Nz(), g3d_out.n()*g3d_out.Ny(), g3d_out.n()*g3d_out.Nx()};
    size_t start3d[4]  = {0, 0, 0, 0};
    size_t count3dp[4] = {1, 1, g3d_out.n()*g3d_out.Ny(), g3d_out.n()*g3d_out.Nx()};
    size_t start3dp[4] = {0, 0, 0, 0};

    err2d = nc_close(ncid2d);
    err = nc_close(ncid); 
    
     //generate 1d nc file for one time step for the f(psi) quantities

    std::string names1d[10] = {"Ne_fsa", "Ni_fsa", "Ue_Fsa", "Ui_fsa", "phi_fsa","q","vor_fsa","Deperp_fsa","Depsi_fsa","Lperpinv_fsa"}; 
    int dataIDs1d[10];
    file::NC_Error_Handle err1d; 
    int ncid1d; 
    err1d = nc_create(argv[2],NC_NETCDF4|NC_CLOBBER, &ncid1d);
    err1d = nc_put_att_text( ncid1d, NC_GLOBAL, "inputfile", input.size(), input.data());
    err1d = nc_put_att_text( ncid1d, NC_GLOBAL, "geomfile", geom.size(), geom.data());
    int dim_ids1d[2], tvarID1d;
    err1d = file::define_dimensions( ncid1d, dim_ids1d, &tvarID1d, g1d_out);
    for( unsigned i=0; i<10; i++){
        err1d = nc_def_var( ncid1d, names1d[i].data(), NC_DOUBLE, 2, dim_ids1d, &dataIDs1d[i]);
    }   
//     midplane 2d fields
    size_t count1d[2] = {1, g1d_out.n()*g1d_out.N()};
    size_t start1d[2] = {0, 0};

    err1d = nc_close(ncid1d);
    double time=0.;

//     double energy_0 =0.,U_i_0=0.,U_e_0=0.,U_phi_0=0.,U_pare_0=0.,U_pari_0=0.,mass_0=0.;
//     os << "#Time(1) mass(2) Ue(3) Ui(4) Uphi(5) Upare(6) Upari(7) Utot(8) EDiff(9)\n";
//         std::cout << "Compute safety factor   "<< "\n";
//         solovev::Alpha alpha(gp); 
//         dg::HVec alphaog2d   = dg::evaluate( alpha, g2d_out);      
//         dg::HVec abs = dg::evaluate( dg::coo1, g1d_out);
//         solovev::SafetyFactor<dg::HVec> qprofile(g2d_out, gp, alphaog2d );
//         dg::HVec sf = dg::evaluate(qprofile, g1d_out);

    //perp laplacian for computation of vorticity

    dg::HVec vor3d    = dg::evaluate( dg::zero, g3d_out);
    dg::Elliptic<dg::HMatrix, dg::HVec, dg::HVec> laplacian(g3d_out,dg::DIR, dg::DIR, dg::normed, dg::centered); 
    dg::HMatrix fsaonrzmatrix,fsaonrzphimatrix;     
    fsaonrzmatrix    =  dg::create::interpolation(psipupilog2d ,g1d_out);    
    fsaonrzphimatrix =  dg::create::interpolation(psipupilog3d ,g1d_out);    
    std::cout << "Compute safety factor   "<< "\n";
    
    //Vectors and Matrices for Diffusion coefficient
    const dg::HVec curvR = dg::evaluate( solovev::CurvatureR(gp), g3d_out);
    const dg::HVec curvZ = dg::evaluate( solovev::CurvatureZ(gp), g3d_out);
    dg::Poisson<dg::HMatrix, dg::HVec> poisson(g3d_out,  dg::DIR, dg::DIR,  g3d_out.bcx(), g3d_out.bcy());
    const dg::HVec binv = dg::evaluate(solovev::Field(gp) , g3d_out) ;
    dg::HVec Deperp3d =  dg::evaluate(dg::zero , g3d_out) ; 
    dg::HVec temp1 = dg::evaluate(dg::zero , g3d_out) ;
    dg::HVec temp2 = dg::evaluate(dg::zero , g3d_out) ;
    dg::HVec temp3 = dg::evaluate(dg::zero , g3d_out) ;
    #ifdef RADIALDIFFUSIONCOEFF
    const dg::HVec psipR =  dg::evaluate( solovev::PsipR(gp), g3d_out);
    const dg::HVec psipRR = dg::evaluate( solovev::PsipRR(gp), g3d_out);
    const dg::HVec psipZ =  dg::evaluate( solovev::PsipZ(gp), g3d_out);
    const dg::HVec psipZZ = dg::evaluate( solovev::PsipZZ(gp), g3d_out);
    const dg::HVec psipRZ = dg::evaluate( solovev::PsipRZ(gp), g3d_out);
    dg::HVec Depsip3d =  dg::evaluate(dg::zero , g3d_out) ;   
    dg::HVec one3d    =  dg::evaluate(dg::one,g3d_out);
    dg::HVec one1d    =  dg::evaluate(dg::one,g1d_out);
    #endif
#ifdef GRADIENTLENGTH
    dg::HVec Lperpinv3d =  dg::evaluate(dg::zero , g3d_out) ;   
#endif
    std::vector<dg::HVec> fields3d(5,dg::evaluate(dg::zero,g3d_out));
    std::vector<dg::HVec> fields2d(5,dg::evaluate(dg::zero,g3d_out));
    for( unsigned i=0; i<p.maxout; i++)//timestepping
    {
        start3dp[0] = i; //set specific time  
        start3d[0] = i; //set specific time  
        start2d[0] = i;
        start1d[0] = i;
        time += p.itstp*p.dt;
        err2d = nc_open(argv[3], NC_WRITE, &ncid2d);
        err1d = nc_open(argv[2], NC_WRITE, &ncid1d);

        std::cout << "Timestep = " << i << "  time = " << time << "\n";
//         std::cout << "Extract 2d planes for avg 2d field and phi_fluc at midplane and computing fsa of Phi_Avg quantities"<< "\n";     

        //Compute toroidal average and fluctuation at midplane for every timestep
        dg::HVec data2davg = dg::evaluate( dg::zero, g2d_out);   
        dg::HVec data2dfsa = dg::evaluate( dg::zero, g2d_out);    
        dg::HVec Deperp2davg =  dg::evaluate(dg::zero , g2d_out); 
        dg::HVec Deperp3dfluc =  dg::evaluate(dg::zero , g3d_out);
        dg::HVec Deperp2dflucavg =  dg::evaluate(dg::zero , g2d_out); 
        dg::HVec vor2davg = dg::evaluate( dg::zero, g2d_out);
        dg::HVec Depsip2davg =  dg::evaluate(dg::zero , g2d_out); 
        dg::HVec Depsip3dfluc =  dg::evaluate(dg::zero , g3d_out);
        dg::HVec Depsip2dflucavg =  dg::evaluate(dg::zero , g2d_out);  
        dg::HVec Lperpinv2davg =  dg::evaluate(dg::zero , g2d_out);  

        
        
        for( unsigned i=0; i<5; i++)
        {
            //set quantities to zero
            data2davg = dg::evaluate( dg::zero, g2d_out);   
            data2dfsa = dg::evaluate( dg::zero, g2d_out);    

            //get 3d data
            err = nc_open( argv[1], NC_NOWRITE, &ncid); //open 3d file
            err = nc_inq_varid(ncid, names[i].data(), &dataIDs[i]);
            err = nc_get_vara_double( ncid, dataIDs[i], start3d, count3d, fields3d[i].data());
            err = nc_close(ncid);  //close 3d file
    
            //get 2d data and sum up for avg
            for( unsigned k=0; k<g3d_out.Nz(); k++)
            {
                dg::HVec data2d(fields3d[i].begin() + k*g2d_out.size(),fields3d[i].begin() + (k+1)*g2d_out.size());
                dg::blas1::axpby(1.0,data2d,1.0,data2davg); 
            }
            //get 2d data of MidPlane
            unsigned kmp = (g3d_out.Nz()/2);
            dg::HVec data2dflucmid(fields3d[i].begin() + kmp*g2d_out.size(),fields3d[i].begin() + (kmp+1)*g2d_out.size());
            dg::blas1::scal(data2davg,1./g3d_out.Nz()); //scale avg
            
            //for fluctuations to be  f_varphi
//             dg::blas1::axpby(1.0,data2dflucmid,-1.0,data2davg,data2dflucmid); //Compute z fluctuation

  
            err2d = nc_put_vara_double( ncid2d, dataIDs2d[i],   start2d, count2d, data2davg.data()); //write avg


            //computa fsa of quantities
            solovev::FluxSurfaceAverage<dg::HVec> fsadata(g2d_out,gp, data2davg );
            dg::HVec data1dfsa = dg::evaluate(fsadata,g1d_out);
            err1d = nc_put_vara_double( ncid1d, dataIDs1d[i], start1d, count1d,  data1dfsa.data());
            
            //compute delta f on midplane : df = f_mp - <f>
            dg::blas2::gemv(fsaonrzmatrix, data1dfsa, data2dfsa); //fsa on RZ grid
            dg::blas1::axpby(1.0,data2dflucmid,-1.0,data2dfsa,data2dflucmid); 

            err2d = nc_put_vara_double( ncid2d, dataIDs2d[i+5], start2d, count2d, data2dflucmid.data());

        }
        //----------------Start vorticity computation
        dg::blas2::gemv( laplacian,fields3d[4],vor3d);
        for( unsigned k=0; k<g3d_out.Nz(); k++)
        {
            dg::HVec data2d(vor3d.begin() + k*g2d_out.size(),vor3d.begin() + (k+1)*g2d_out.size());
            dg::blas1::axpby(1.0,data2d,1.0,vor2davg); 
        }
        dg::blas1::scal(vor2davg,1./g3d_out.Nz()); //scale avg
        err2d = nc_put_vara_double( ncid2d, dataIDs2d[10],   start2d, count2d, vor2davg.data());
        solovev::FluxSurfaceAverage<dg::HVec> fsavor(g2d_out,gp,vor2davg );
        dg::HVec vor1dfsa = dg::evaluate(fsavor,g1d_out);
        err1d = nc_put_vara_double( ncid1d, dataIDs1d[6], start1d, count1d,  vor1dfsa.data()); 
        //----------------Stop vorticity computation
        //----------------Start Perpendicular DiffusionCoefficient computation
        //poissonpart
        dg::blas1::transform(fields3d[0], temp3, dg::PLUS<>(+1)); //Ne +1
        poisson( fields3d[4], fields3d[0], Deperp3d); //D_perp,e = [phi,N_e]_RZ
        dg::blas1::pointwiseDot( Deperp3d, binv, Deperp3d); //D_perp,e = 1/B*[phi,N_e]_RZ              
        //curvpart
        dg::blas2::gemv( poisson.dxrhs(), fields3d[0], temp1); //d_R Ne
        dg::blas2::gemv( poisson.dyrhs(),fields3d[0], temp2);  //d_Z Ne
        dg::blas1::pointwiseDot( curvR, temp1, temp1); // C^R d_R Ne
        dg::blas1::pointwiseDot( curvZ, temp2, temp2);   // C^Z d_Z Ne
        dg::blas1::axpby( 1.0, temp1, 1.0, temp2);  //temp2 = K(N_e)
        dg::blas1::pointwiseDot(fields3d[2], fields3d[2], temp1); // temp1=U_e^2
        dg::blas1::pointwiseDot(temp1,temp2, temp1); // temp1=U_e^2 K(N_e)
        dg::blas1::axpby( -1.0, temp2,1.0,  Deperp3d );  //D_perp,e = 1/B*[phi,N_e]_RZ - 1*K(N_e) 
        dg::blas1::axpby(  0.5*p.mu[0], temp1,1.0,  Deperp3d);  //D_perp,e = 1/B*[phi,N_e]_RZ - 1*K(N_e) + 0.5*nu_e*U_e^2*K(N_e)
        dg::blas1::pointwiseDot( Deperp3d, temp3, Deperp3d); //D_perp,e = N_e*(1/B*[phi,N_e]_RZ - 0.5*K(N_e))
        //(nabla_perp Ne)^2 part
//         poisson.variationRHS(fields3d[0],temp2);
//         dg::blas1::pointwiseDivide( Deperp3d, temp2, Deperp3d); 
        for( unsigned k=0; k<g3d_out.Nz(); k++)
        {
            dg::HVec data2d(Deperp3d.begin() + k*g2d_out.size(),Deperp3d.begin() + (k+1)*g2d_out.size());
            dg::blas1::axpby(1.0,data2d,1.0,Deperp2davg); 
        }
        dg::blas1::scal(Deperp2davg,1./g3d_out.Nz()); //scale avg
        err2d = nc_put_vara_double( ncid2d, dataIDs2d[11],   start2d, count2d, Deperp2davg.data());
        solovev::FluxSurfaceAverage<dg::HVec> fsaDeperp(g2d_out,gp, Deperp2davg );
        dg::HVec  Deperp1Dfsa = dg::evaluate(fsaDeperp,g1d_out);
        err1d = nc_put_vara_double( ncid1d, dataIDs1d[7], start1d, count1d,   Deperp1Dfsa.data()); 
        //--------------- Stop Perpendicular DiffusionCoefficient computation
        //--------------- Start Radial DiffusionCoefficient computation
        #ifdef RADIALDIFFUSIONCOEFF
        //ExB term  =  1/B[phi,psi_p] term
        dg::blas2::gemv( poisson.dxlhs(), fields3d[4], temp1); //temp1 = d_R phi
        dg::blas2::gemv( poisson.dylhs(), fields3d[4], temp2);  //temp2 = d_Z phi
        dg::blas1::pointwiseDot( psipZ, temp1, temp1);//temp1 = d_R phi d_Z psi_p
        dg::blas1::pointwiseDot( psipR, temp2, temp2); //temp2 = d_Z phi d_R psi_p 
        dg::blas1::axpby( 1.0, temp1, -1.0,temp2, Depsip3d);  //Depsip3d=[phi,psip]_RZ
        dg::blas1::pointwiseDot( Depsip3d, binv, Depsip3d); //Depsip3d = 1/B*[phi,psip]_RZ             
        //Curvature Term = -(1-0.5*mu_e U_e^2) K(psi_p) term
        dg::blas1::pointwiseDot( curvR,  psipR, temp1);  //temp1 = K^R d_R psi
        dg::blas1::pointwiseDot( curvZ,  psipZ, temp2);  //temp2 = K^Z d_Z psi
        dg::blas1::axpby( 1.0, temp1, 1.0,temp2,  temp2);  //temp2 =K(psi_p)
        dg::blas1::pointwiseDot(fields3d[2], fields3d[2], temp1); // temp1=U_e^2
        dg::blas1::pointwiseDot(temp1,temp2, temp1); // temp1=U_e^2 K(psi_p)
        dg::blas1::axpby( -1.0, temp2,1.0,  Depsip3d );  //Depsip3d = 1/B*[phi,psi_p]_RZ - K(psi_p) 
        dg::blas1::axpby(  0.5*p.mu[0], temp1, 1.0,  Depsip3d);  //Depsip3d = 1/B*[phi,psi_p]_RZ - K(psi_p) + 0.5*nu_e*U_e^2*K(psi_p)
        dg::blas1::pointwiseDot( Depsip3d, temp3, Depsip3d); //Depsip3d = N_e*(1/B*[phi,psi_p]_RZ - K(psi_p) + 0.5*nu_e*U_e^2*K(psi_p))
        //Background gradient Term = d_R psi_p d_R N_e + d_Z psi_p d_Z N_e term
//         dg::blas2::gemv( poisson.dxrhs(), fields3d[0], temp1); //temp1 =  d_R (N_e -1)
//         dg::blas2::gemv( poisson.dyrhs(), fields3d[0], temp2);  //temp2 = d_Z (N_e -1)
//         dg::blas1::pointwiseDot( psipR, temp1, temp1); //temp1 =  d_R psi_p d_R (N_e -1)
//         dg::blas1::pointwiseDot( psipZ, temp2, temp2); //temp2 =  d_Z psi_p d_Z (N_e -1)
//         dg::blas1::axpby( 1.0, temp1, 1.0,temp2,temp2);  //temp2 =  d_R psi_p d_R (N_e -1) + d_Z psi_p d_Z (N_e -1)
//         dg::blas1::pointwiseDivide( Depsip3d, temp2,Depsip3d); 
        for( unsigned k=0; k<g3d_out.Nz(); k++)
        {
            dg::HVec data2d(Depsip3d.begin() + k*g2d_out.size(),Depsip3d.begin() + (k+1)*g2d_out.size());
            dg::blas1::axpby(1.0,data2d,1.0,Depsip2davg); 
        }
        dg::blas1::scal(Depsip2davg,1./g3d_out.Nz()); //scale avg
        solovev::FluxSurfaceAverage<dg::HVec> fsaDepsip(g2d_out,gp, Depsip2davg );
        dg::HVec  Depsip1Dfsa = dg::evaluate(fsaDepsip,g1d_out);
        //compute delta f on midplane : d Depsip2d = Depsip - <Depsip>       
        dg::blas2::gemv(fsaonrzphimatrix, Depsip1Dfsa , Depsip3dfluc ); //fsa on RZ grid
        dg::blas1::axpby(1.0,Depsip3d,-1.0, Depsip3dfluc, Depsip3dfluc); 
        //Same procedure for fluc
        for( unsigned k=0; k<g3d_out.Nz(); k++)
        {
            dg::HVec data2d( Depsip3dfluc.begin() + k*g2d_out.size(), Depsip3dfluc.begin() + (k+1)*g2d_out.size());
            dg::blas1::axpby(1.0,data2d,1.0, Depsip2dflucavg); 
        }
        dg::blas1::scal(Depsip2dflucavg,1./g3d_out.Nz()); //scale avg
        err2d = nc_put_vara_double( ncid2d, dataIDs2d[12],   start2d, count2d, Depsip2dflucavg.data());
        solovev::FluxSurfaceAverage<dg::HVec> fsaDepsipfluc(g2d_out,gp,  Depsip2dflucavg );
        dg::HVec  Depsip1Dflucfsa = dg::evaluate(fsaDepsipfluc,g1d_out);
        err1d = nc_put_vara_double( ncid1d, dataIDs1d[8], start1d, count1d,   Depsip1Dflucfsa.data()); 
        std::cout << "Depsip =" << dg::blas2::dot(psipupilog3d,w3d, Depsip3dfluc) << std::endl;
        #endif
        #ifdef GRADIENTLENGTH
        dg::blas1::transform(temp3, temp1, dg::LN<double>());
        poisson.variationRHS(temp1,temp2);
        dg::blas1::transform(temp2, Lperpinv3d, dg::SQRT<double>());

//         dg::blas1::pointwiseDivide( one3d, Lperpinv3d,Lperpinv3d); //Ln = ( d_R psi_p d_R (N_e -1) + d_Z psi_p d_Z (N_e -1) )/N_e
        for( unsigned k=0; k<g3d_out.Nz(); k++)
        {
            dg::HVec data2d(Lperpinv3d.begin() + k*g2d_out.size(),Lperpinv3d.begin() + (k+1)*g2d_out.size());
            dg::blas1::axpby(1.0,data2d,1.0,Lperpinv2davg); 
        } 
        
        dg::blas1::scal(Lperpinv2davg,1./g3d_out.Nz()); //scale avg
        err2d = nc_put_vara_double( ncid2d, dataIDs2d[13],   start2d, count2d, Lperpinv2davg.data());
        solovev::FluxSurfaceAverage<dg::HVec> fsaLperpinv(g2d_out,gp,Lperpinv2davg );
        dg::HVec  Lperpinv1Dfsa = dg::evaluate(fsaLperpinv,g1d_out);
        err1d = nc_put_vara_double( ncid1d, dataIDs1d[9], start1d, count1d,   Lperpinv1Dfsa .data()); 
        std::cout << "Lperpinv=" <<dg::blas2::dot(psipupilog3d,w3d, Lperpinv3d) << std::endl;
        #endif
        //--------------- Stop Radial DiffusionCoefficient computation
        
        //put safety factor into file
//         err1d = nc_put_vara_double( ncid1d, dataIDs1d[5], start1d, count1d,  sf.data());
        //write time data
        err1d = nc_put_vara_double( ncid1d, tvarID1d, start1d, count1d, &time);
        err1d = nc_close(ncid1d);  //close 1d netcdf files
        err2d = nc_put_vara_double( ncid2d, tvarID, start2d, count2d, &time);
        err2d = nc_close(ncid2d); //close 2d netcdf files
      
        //Probe 
        const double Rprobe = gp.R_0+p.boxscaleRm*gp.a*0.8;
        const double Zprobe = 0.0;
        const double Phiprobe = M_PI;
//         dg::HMatrix probeinterp  = dg::create::interpolation( Rprobe,  Zprobe, Phiprobe, g3d_out, dg::NEU);

//         double probevalue = fields3d[0](Rprobe,Zprobe,Phiprobe);
        
////////////////////////////////transport coefficient
// D_perp,e = N_e u_perp,e . nabla_perp N_e/(nabla_perp N_e)^2
        //[phi-beta_e0Apar,Ne]
        //Curv(N_e)

        
        //compute D_e

//         

        
        
        
        
        
        
        
        // ---- Compute energies ----
//         std::cout << "Compute macroscopic timedependent quantities"<< "\n";

    

        //write macroscopic timedependent quantities into output.dat file
//         os << time << " " << mass_norm << " " <<  U_e_norm <<" " <<  U_i_norm <<" " << U_phi_norm <<" " << U_pare_norm <<" " << U_pari_norm <<" "  << energy_norm <<" " << energy_diff<<std::endl;
        
        
    } //end timestepping
    //cross correleation between phi and ne
    
    //relative fluctuation amplitude(R,Z,phi) = delta n(R,Z,phi)/n0(psi)
   

    
    //Compute energys
    
    
    //const unsigned num_out = t5file.get_size();

    //dg::Grid2d<double> grid( 0, p.lx, 0, p.ly, p.n, p.Nx, p.Ny, p.bc_x, p.bc_y);

    //dg::HVec input_h( grid.size());
    //dg::HVec input0( input_h), input1(input0), ln0( input0), ln1(input0);
    //dg::HVec visual( input0);
    //std::vector<double> mass(p.maxout*p.itstp+1,0), energy(mass), diffusion(mass), dissipation(mass);
    //if( p.global)
    //{
    //    t5file.get_xfile( mass, "mass");
    //    t5file.get_xfile( energy, "energy");
    //    t5file.get_xfile( diffusion, "diffusion");
    //    t5file.get_xfile( dissipation, "dissipation");
    //}

    //dg::HVec xvec = dg::evaluate( X, grid);
    //dg::HVec yvec = dg::evaluate( Y, grid);
    //dg::HVec one = dg::evaluate( dg::one, grid);
    //dg::HVec w2d = dg::create::weights( grid);
    //dg::HMatrix equi = dg::create::backscatter( grid);

    //t5file.get_field( input0, "electrons", 1);
    //if( p.global)
    //    thrust::transform( input0.begin(), input0.end(), input0.begin(), dg::PLUS<double>(-1));
    //double mass_ = dg::blas2::dot( one, w2d, input0 ); 
    //const double posX_init = dg::blas2::dot( xvec, w2d, input0)/mass_;
    //const double posY_init = dg::blas2::dot( yvec, w2d, input0)/mass_;
    //double posX, posY, velX, velY, accX, accY;
    //double posX_max, posY_max;
    //double posX_old = 0, posY_old = 0;
    //double deltaT = p.dt*p.itstp;
    //t5file.get_field( input0, "electrons", 2);
    //    if( p.global)
    //        thrust::transform( input0.begin(), input0.end(), input0.begin(), dg::PLUS<double>(-1));
    //    mass_ = dg::blas2::dot( one, w2d, input0 ); 
    //    posX = dg::blas2::dot( xvec, w2d, input0)/mass_ - posX_init;
    //    posY = dg::blas2::dot( yvec, w2d, input0)/mass_ - posY_init;
    //    double velX_old = -posX/deltaT, velY_old = -posY/deltaT; 
    //    //velX_old = NAN, velY_old = NAN;

    //Vesqr<dg::HMatrix, dg::HVec> vesqr( grid, p.kappa);
    //os << "#Time(1) posX(2) posY(3) velX(4) velY(5) mass(6) diff(7) (m_tot-m_0)/m_0(8) "
    //   << "Ue(9) Ui(10) Uphi(11) Utot(12) (U_tot-U_0)/U_0(13) diss(14) posX_max(15) posY_max(16) max_amp(19)\n";
    ////dg::Timer t;
    //for( unsigned idx=1; idx<=num_out; idx++)
    //{
    //    //t.tic();
    //    //std::cout << idx<<std::endl;
    //    t5file.get_field( input0, "electrons", idx);
    //    //input0 = input_h;
    //    t5file.get_field( input1, "ions", idx);
    //    //input1 = input_h;
    //    //t.toc();
    //    //std::cout << "Reading took "<<t.diff()<<"s\n";
    //    //t.tic();
    //    double Ue = 0, Ui = 0, Uphi = 0;
    //    if( p.global)
    //    {
    //        //std::cout << "in global branch!\n";
    //        log( input0, ln0), log( input1, ln1);
    //        Ue = dg::blas2::dot( input0, w2d, ln0);
    //        Ui = p.tau*dg::blas2::dot( input1, w2d, ln1);
    //        Uphi = energy[(idx-1)*p.itstp] - Ue - Ui;
    //        thrust::transform( input0.begin(), input0.end(), input0.begin(), dg::PLUS<double>(-1));
    //    }
    //    mass_ = dg::blas2::dot( one, w2d, input0 ); 
    //    posX = dg::blas2::dot( xvec, w2d, input0)/mass_ - posX_init;
    //    posY = dg::blas2::dot( yvec, w2d, input0)/mass_ - posY_init;

    //    velX = (posX - posX_old)/deltaT;
    //    velY = (posY - posY_old)/deltaT;
    //    accX = (velX - velX_old)/deltaT;
    //    accY = (velY - velY_old)/deltaT;
    //    posX_old = posX; posY_old = posY;
    //    velX_old = velX; velY_old = velY;
    //    //output
    //    os << t5file.get_time( idx);//(1)
    //    os << " "<<posX << " " << posY << " "<<velX<<" "<<velY;//(2-5)
    //    os << " "<<mass[(idx-1)*p.itstp] << " "<<diffusion[(idx-1)*p.itstp];//(6,7)
    //    os << " "<< (mass[(idx-1)*p.itstp]-mass[0])/(mass[0]-grid.lx()*grid.ly());//blob mass is mass[] - Area (8)
    //    os << " "<<Ue<<" "<<Ui<<" "<<Uphi<<" "<<energy[(idx-1)*p.itstp]; //(9-12)
    //    os << " "<<(energy[(idx-1)*p.itstp]-energy[0])/energy[0];//(13)
    //    os << " "<<dissipation[(idx-1)*p.itstp]; //(14)
    //    //get the maximum amplitude position
    //    dg::blas2::gemv( equi, input0, visual);
    //    unsigned position = thrust::distance( visual.begin(), thrust::max_element( visual.begin(), visual.end()) );
    //    unsigned Nx = p.Nx*p.n; 
    //    const double hx = grid.hx()/(double)grid.n();
    //    const double hy = grid.hy()/(double)grid.n();
    //    posX_max = hx*(1./2. + (double)(position%Nx))-posX_init;
    //    posY_max = hy*(1./2. + (double)(position/Nx))-posY_init;
    //    os << " "<<posX_max<<" "<<posY_max;
    //    os << " "<<accX<<" "<<accY;
    //    os << " "<<*thrust::max_element( visual.begin(), visual.end());
    //    os <<"\n";
    //    //t.toc();
    //    //std::cout << "The rest took "<<t.diff()<<"s\n";
    //}
    //os.close();
    return 0;
}

