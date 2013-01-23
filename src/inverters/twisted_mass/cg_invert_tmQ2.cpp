#include "cg_64_invert_tmQ2.h"
#include "cg_128_invert_tmQ2.h"

#include "../../new_types/new_types_definitions.h"
#include "../../base/global_variables.h"

//switch 64 and 128
void inv_tmQ2_RL_cg(spincolor *sol,spincolor *guess,quad_su3 *conf,double kappa,int RL,double m,int niter,int rniter,double residue,spincolor *source)
{
  if(nissa_use_128_bit_precision) inv_tmQ2_RL_cg_64(sol,guess,conf,kappa,RL,m,niter,rniter,residue,source);
  else inv_tmQ2_RL_cg_128(sol,guess,conf,kappa,RL,m,niter,rniter,residue,source);

}