#pragma once
#include <vector>
/*!@file
 *
 * Geometry parameters
 */
namespace solovev
{
///@addtogroup geom
///@{
/**
 * @brief Constructs and display geometric parameters
 */    
struct GeomParameters
{
    double I_0, //!< the current
           R_0, //!< central tokamak radius
           a,  //!<  little tokamak radius
           elongation, //!< elongation of the magnetic surfaces
           triangularity, //!< triangularity of the magnetic surfaces
           alpha, //!< damping width
           rk4eps,  //!< accuracy for the field line mapping
           psipmin, //!< for source 
           psipmax, //!< for profile
           psipmaxcut, //!< for cutting
           psipmaxlim; //!< for limiter
    std::vector<double> c;  //!< coefficients for the solovev equilibrium
     /**
     * @brief constructor to make an object
     *
     * maps parameters from input file to parameters 
     * @param v Vector from read_input function
     */   
    GeomParameters( const std::vector< double>& v) {
        I_0=v[1];
        R_0 = v[2];
        a=R_0*v[3];
        elongation=v[4];
        triangularity=v[5];
        alpha=v[6];
        rk4eps=v[7];
        psipmin= v[8];
        psipmax= v[9];
        psipmaxcut = v[10];
        psipmaxlim = v[11];
    }
    /**
     * @brief Display parameters
     *
     * @param os Output stream
     */
    void display( std::ostream& os = std::cout ) const
    {
        os << "Geometrical parameters are: \n"
            <<" I0             = "<<I_0<<"\n"
            <<" R0            = "<<R_0<<"\n"
            <<" epsilon_a     = "<<a/R_0<<"\n"
            <<" elongation    = "<<elongation<<"\n"
            <<" triangularity = "<<triangularity<<"\n"
            <<" alpha         = "<<alpha<<"\n"
            <<" rk4 epsilon   = "<<rk4eps<<"\n"
            <<" psipmin       = "<<psipmin<<"\n"
            <<" psipmax       = "<<psipmax<<"\n"
            <<" psipmaxcut    = "<<psipmaxcut<<"\n"
            <<" psipmaxlim    = "<<psipmaxlim<<"\n";
        os << std::flush;

    }
};
} //namespace solovev
