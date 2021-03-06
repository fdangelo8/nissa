#ifdef HAVE_CONFIG_H
 #include "config.hpp"
#endif

#include "dirac_operators/stD/dirac_operator_stD.hpp"
#include "eigenvalues/eigenvalues.hpp"
#include "eigenvalues/eigenvalues_staggered.hpp"
#include "geometry/geometry_eo.hpp"
#include "geometry/geometry_mix.hpp"
#include "measures/fermions/stag.hpp"

#ifdef USE_THREADS
 #include "routines/thread.hpp"
#endif

#include "spectral_projectors.hpp"

namespace nissa
{
//  //This measure will compute the first 'n' eigenvalues (parameter)
//  //and eigenvectors of DD^+ in staggered formulation, in order to
//  //build an estimate of the topological charge as Q=\sum_i
//  //\bar(u)_i gamma5 u_i, where {u_i} are the first n eigenvectors.
//  THREADABLE_FUNCTION_7ARG(measure_spectral_proj, color**,eigvec, quad_su3**,conf,complex*, charge_cut, complex*, eigval, int,neigs, double,eig_precision, int,wspace_size)
//  {
//    //parameters of the eigensolver
//    const bool min_max=0;
//    
//    //results of the g5 application
//    color *eigvec_g5_eo[2]={nissa_malloc("eigvec_g5_EVN",loc_volh+bord_volh,color),nissa_malloc("eigvec_g5_ODD",loc_volh+bord_volh,color)};
//    
//    //identity backfield
//    quad_u1 *u1b[2]={nissa_malloc("u1b",loc_volh+bord_volh,quad_u1),nissa_malloc("u1b",loc_volh+bord_volh,quad_u1)};
//    init_backfield_to_id(u1b);
//    
//    //launch the eigenfinder
//    double eig_time=-take_time();
//    find_eigenvalues_staggered_D2ee(eigvec,eigval,neigs,min_max,conf,u1b,0.0,eig_precision,wspace_size);
//    
//    //Buffer to apply the g5 operator
//    //The ODD part is set to zero
//    //The EVN part is linked with the eigenvector
//    color *eigvec_eo[2];
//    eigvec_eo[ODD]=nissa_malloc("eigvec_evn",loc_volh+bord_volh,color);
//    vector_reset(eigvec_eo[ODD]);
//    
//    verbosity_lv1_master_printf("\n\nEigenvalues of DD^+ and debug info on spectral projectors:\n");
//    for(int ieig=0;ieig<neigs;ieig++)
//    {
//      master_printf("lam_%d = (%.16lg,%.16lg)\n",ieig,eigval[ieig][RE],eigval[ieig][IM]);
//      
//      // compute terms u_j^+ g5 u_i
//      // convert 'eigvec[ieig]' in staggered format ('in_tmp_eo'),
//      // then multiply it with gamma5 and save the result in
//      // 'out_tmp_eo'. The term corresponding to u_j^+ g5 u_i
//      // will be stored in 'charge_cut[ieig*neigs+jeigs]' as the hermitian product
//      // between 'eigvec[jeig]' and 'out_tmp_eo[EVN]'.
//      
//      //multiply by gamma5
//      eigvec_eo[EVN]=eigvec[ieig];
//      apply_stag_op(eigvec_g5_eo,conf,u1b,stag::GAMMA_5,stag::IDENTITY,eigvec_eo);
//      
//      //take hermitian products of the even parts (unit norm checked for both vectors)
//      for(int jeig=ieig;jeig<neigs;jeig++)
//	{
//	  complex_vector_glb_scalar_prod(charge_cut[ieig*neigs+jeig],(complex*)eigvec[jeig],(complex*)eigvec_g5_eo[EVN],loc_volh*sizeof(color)/sizeof(complex));
//	  verbosity_lv1_master_printf("u_%d^+ g5 u_%d = (%.16lg,%.16lg)\n",jeig,ieig,charge_cut[ieig*neigs+jeig][RE],charge_cut[ieig*neigs+jeig][IM]);
//	}
//    }
//    verbosity_lv2_master_printf("\n\n\n");
//    
//    eig_time+=take_time();
//    verbosity_lv1_master_printf("Eigenvalues time: %lg\n",eig_time);
//    
//    nissa_free(eigvec_g5_eo[EVN]);
//    nissa_free(eigvec_g5_eo[ODD]);
//    nissa_free(eigvec_eo[ODD]);
//    nissa_free(u1b[0]);
//    nissa_free(u1b[1]);
//  }
//  THREADABLE_FUNCTION_END


  //This measure will compute the first 'n' eigenvalues (parameter)
  //and eigenvectors of the Adams operator (staggered).
  THREADABLE_FUNCTION_8ARG(measure_Adams_spectrum, color**,eigvec, quad_su3**,conf,double,m_Adams,complex*, charge_cut, complex*, eigval, int,neigs, double,eig_precision, int,wspace_size)
  {
    //parameters of the eigensolver
    const bool min_max=0;
    
    //identity backfield
    quad_u1 *u1b[2]={nissa_malloc("u1b",loc_volh+bord_volh,quad_u1),nissa_malloc("u1b",loc_volh+bord_volh,quad_u1)};
    init_backfield_to_id(u1b);
    
    //launch the eigenfinder
    double eig_time=-take_time();
    find_eigenvalues_staggered_Adams(eigvec,eigval,neigs,min_max,conf,u1b,0.0,m_Adams,eig_precision,wspace_size);
    
    verbosity_lv1_master_printf("\n\nEigenvalues of staggered Adams operator with m_Adams=%.16lg:\n",m_Adams);
    for(int ieig=0;ieig<neigs;ieig++)
    {
      master_printf("lam_%d = (%.16lg,%.16lg)\n",ieig,eigval[ieig][RE],eigval[ieig][IM]);
    }
    verbosity_lv2_master_printf("\n\n\n");
    
    eig_time+=take_time();
    verbosity_lv1_master_printf("Eigenvalues time: %lg\n",eig_time);
    
    nissa_free(u1b[0]);
    nissa_free(u1b[1]);
  }
  THREADABLE_FUNCTION_END
  
  //measure of the spectrum of Adams operator
  void measure_spectral_proj(quad_su3 **conf,theory_pars_t &theory_pars,spectr_proj_meas_pars_t &meas_pars,int iconf,bool conf_created)
  {
    /* The format of the file for a single measure is the following:
     *
     * n\t<lam_1>\t<lam_2>\t...\t<lam_n>
     *
     * where:
     *  - 'n' is the number of required eigenvalues (integer)
     *  - 'lam_i' is the i-th eigenvalue of DD^+ (real)
     */
    FILE *file=open_file(meas_pars.path,conf_created?"w":"a");
    int neigs=meas_pars.neigs;
    
    complex *charge_cut=nissa_malloc("charge_cut",neigs*neigs,complex);
    complex *eigval=nissa_malloc("DD_eig_Val",neigs,complex);
    
    color **eigvec=nissa_malloc("eigvec",neigs,color*);
    for(int ieig=0;ieig<neigs;ieig++)
      {
	eigvec[ieig]=nissa_malloc("eigvec_ieig",loc_vol+bord_vol,color);
	vector_reset(eigvec[ieig]);
      }
    
    //loop over hits
    int nhits=meas_pars.nhits;
    for(int hit=0;hit<nhits;hit++)
      {
	verbosity_lv2_master_printf("Evaluating Adams spectrum for m_Adams = %.16lg, nhits %d/%d\n",meas_pars.m_Adams,hit+1,nhits);
	
	measure_Adams_spectrum(eigvec,conf,meas_pars.m_Adams,charge_cut,eigval,meas_pars.neigs,meas_pars.eig_precision,meas_pars.wspace_size);
      }
    
    master_fprintf(file,"%d\t%.16lg\t",neigs,meas_pars.m_Adams);
    for(int ieig=0;ieig<neigs;++ieig)
      master_fprintf(file,"%.16lg\t",eigval[ieig][RE]);
    master_fprintf(file,"\n");
    
    close_file(file);
    
    for(int ieig=0; ieig<neigs; ieig++)
      verbosity_lv1_master_printf("%d\t%.16lg\n",ieig,eigval[ieig][RE]);
    
    verbosity_lv1_master_printf("\n\n");
    
    // deallocating vectors
    for(int ieig=0;ieig<neigs;ieig++)
      nissa_free(eigvec[ieig]);
    
    nissa_free(eigvec);
    nissa_free(charge_cut);
    nissa_free(eigval);
  }

//  //measure of spectrally projected components of gamma5 in the staggered formulation
//  void measure_spectral_proj(quad_su3 **conf,theory_pars_t &theory_pars,spectr_proj_meas_pars_t &meas_pars,int iconf,bool conf_created)
//  {
//    /* The format of the file for a single measure is the following:
//     *
//     * n\t<lam_1>\t<lam_2>\t...\t<lam_n>\t<A_1>\t<A_2>\t...\t<A_n>\t<B_1>\t<B_2>\t...\t<B_n>\n,
//     *
//     * where:
//     *  - 'n' is the number of required eigenvalues (integer)
//     *  - 'lam_i' is the i-th eigenvalue of DD^+ (real),
//     *  - A_k = \sumt_{i<=k} u_i^+ g5 u_i (i.e., the k-th partial sum of spectral projections of gamma5; real),
//     *  - B_k = \sumt_{i,j<=k} |u_i^+ g5 u_j|^2  (used to estimate the renormalization constant due to
//     *                                            the pseudoscalar current;real and positive)
//     */
//    FILE *file=open_file(meas_pars.path,conf_created?"w":"a");
//    int neigs=meas_pars.neigs;
//    
//    complex *charge_cut=nissa_malloc("charge_cut",neigs*neigs,complex);
//    complex *eigval=nissa_malloc("DD_eig_Val",neigs,complex);
//    vector_reset(charge_cut);
//    vector_reset(eigval);
//    
//    color **eigvec=nissa_malloc("eigvec",neigs,color*);
//    for(int ieig=0;ieig<neigs;ieig++)
//      {
//	eigvec[ieig]=nissa_malloc("eigvec_ieig",loc_volh+bord_volh,color);
//	vector_reset(eigvec[ieig]);
//      }
//    
//    //loop over hits
//    int nhits=meas_pars.nhits;
//    for(int hit=0;hit<nhits;hit++)
//      {
//    verbosity_lv2_master_printf("Evaluating spectral projections of gamma5, nhits %d/%d\n",hit+1,nhits);
//    
//    measure_spectral_proj(eigvec,conf,charge_cut,eigval,meas_pars.neigs,meas_pars.eig_precision,meas_pars.wspace_size);
//      }
//    
//    //print the result on file
//    verbosity_lv1_master_printf("\n\nPartial sums for spectral projectors:\n\nk\t\t\teig\t\t\tA_k\t\t\tB_k\n");
//    // vectors storing A_k and B_k partial sums, offset by 1 for convenience 
//    double *cum_sumA=nissa_malloc("cum_sumA",meas_pars.neigs+1,double);
//    double *cum_sumB=nissa_malloc("cum_sumB",meas_pars.neigs+1,double);
//    vector_reset(cum_sumA);
//    vector_reset(cum_sumB);
//
//    master_fprintf(file,"%d\t",neigs);
//    for(int ieig=0;ieig<neigs;++ieig)
//      master_fprintf(file,"%.16lg\t",eigval[ieig][RE]);
//    
//    for(int ieig=0;ieig<neigs;ieig++)
//      {
//	cum_sumA[1+ieig]=cum_sumA[ieig]+charge_cut[ieig*neigs+ieig][RE];
//	master_fprintf(file,"%.16lg\t",cum_sumA[1+ieig]);
//      }
//
//    for(int kcutoff=0;kcutoff<neigs;kcutoff++)
//      {
//      cum_sumB[1+kcutoff]=cum_sumB[kcutoff]+complex_norm2(charge_cut[kcutoff*neigs+kcutoff]); //diagonal part
//      for(int ieig=0;ieig<kcutoff;ieig++)
//        cum_sumB[1+kcutoff]+=2.0*complex_norm2(charge_cut[ieig*neigs+kcutoff]); //offdiagonal part
//
//      master_fprintf(file,"%.16lg\t",cum_sumB[1+kcutoff]);
//      }
//    master_fprintf(file,"\n");
//    
//    close_file(file);
//
//    for(int ieig=0; ieig<neigs; ieig++){
//      verbosity_lv1_master_printf("%d\t%.16lg\t%.16lg\t%.16lg\n",ieig,eigval[ieig][RE],cum_sumA[1+ieig],cum_sumB[1+ieig]);
//    }
//    verbosity_lv1_master_printf("\n\n");
//   
//    // deallocating vectors 
//    for(int ieig=0;ieig<neigs;ieig++)
//      nissa_free(eigvec[ieig]);
//
//    nissa_free(eigvec);
//    nissa_free(charge_cut);
//    nissa_free(eigval);
//    nissa_free(cum_sumA);
//    nissa_free(cum_sumB);
//  }
  
  //print pars
  std::string spectr_proj_meas_pars_t::get_str(bool full)
  {
    std::ostringstream os;
    
    os<<"MeasSpectrProj\n";
    os<<base_fermionic_meas_t::get_str(full);
    if(m_Adams!=def_m_Adams() or full) os<<" MAdams\t\t=\t"<<m_Adams<<"\n";
    if(neigs!=def_neigs() or full) os<<" Neigs\t\t=\t"<<neigs<<"\n";
    if(eig_precision!=def_eig_precision() or full) os<<" EigPrecision\t\t=\t"<<eig_precision<<"\n";
    if(wspace_size!=def_wspace_size() or full) os<<" WSpaceSize\t\t=\t"<<wspace_size<<"\n";
    
    return os.str();
  }
}
