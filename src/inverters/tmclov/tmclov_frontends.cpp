#ifdef HAVE_CONFIG_H
 #include "config.h"
#endif

#include "cgm_invert_tmclovQ2.h"

#include "../../base/debug.h"
#include "../../base/global_variables.h"
#include "../../base/vectors.h"
#include "../../dirac_operators/dirac_operator_tmclovQ/reconstruct_tmclov_doublet.h"
#include "../../new_types/new_types_definitions.h"
#include "../../new_types/spin.h"
#include "../../routines/ios.h"

//invert a set of propagators using the passed source
//the output is stored in twisted basis, assuming that prop=su3spinspin[2][nmass][>=loc_vol]
void compute_su3spinspin_tmclov_propagators_multi_mass(su3spinspin ***prop,quad_su3 *conf,double kappa,double csw,as2t_su3 *Pmunu,double *mass,int nmass,int niter_max,double *req_res,su3spinspin *source)
{
  //allocate temporary source
  spincolor *temp_source=nissa_malloc("temp_source",loc_vol+bord_vol,spincolor);
  //allocate temp_vec
  spincolor *temp_vec[2];
  temp_vec[0]=nissa_malloc("temp_vec[0]",loc_vol+bord_vol,spincolor);
  temp_vec[1]=nissa_malloc("temp_vec[1]",loc_vol+bord_vol,spincolor);
  //allocate nmass spincolors, for the cgm solutions
  spincolor **cgm_solution;
  cgm_solution=nissa_malloc("cgm_solution",nmass,spincolor*);
  for(int imass=0;imass<nmass;imass++) cgm_solution[imass]=nissa_malloc("cgm_solution[imass]",loc_vol+bord_vol,spincolor);
  
  //loop over the source dirac and color index
  for(int id=0;id<4;id++)
    for(int ic=0;ic<3;ic++)
      {
        nissa_loc_vol_loop(ivol)
          get_spincolor_from_su3spinspin(temp_source[ivol],source[ivol],id,ic);
        set_borders_invalid(temp_source);
	
        double init_time=take_time();
	inv_tmclovDQ_cgm(cgm_solution,conf,kappa,csw,Pmunu,mass,nmass,niter_max,req_res,temp_source);
        master_printf("Finished the inversion of D*Q, dirac index %d, color %d in %g sec\n",id,ic,take_time()-init_time);
        
        //reconstruct the doublet
        for(int imass=0;imass<nmass;imass++)
          {
            reconstruct_tmclov_doublet(temp_vec[0],temp_vec[1],conf,kappa,csw,Pmunu,mass[imass],cgm_solution[imass]);
            
            //convert the id-th spincolor into the su3spinspin
            for(int r=0;r<2;r++)
	      {
		nissa_loc_vol_loop(ivol)       
		  put_spincolor_into_su3spinspin(prop[r][imass][ivol],temp_vec[r][ivol],id,ic);
		set_borders_invalid(prop[r]);
	      }
            master_printf("Mass %d (%g) reconstructed \n",imass,mass[imass]);
          }
      }
  
  //free temp vec
  nissa_free(temp_vec[1]);nissa_free(temp_vec[0]);
  nissa_free(temp_source);
  for(int imass=0;imass<nmass;imass++) nissa_free(cgm_solution[imass]);
  nissa_free(cgm_solution);
}