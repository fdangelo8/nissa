#ifndef _CG_128_INVERT_TMDKERN_EOPREC_SQUARE_EOS_H
#define _CG_128_INVERT_TMDKERN_EOPREC_SQUARE_EOS_H

void inv_tmDkern_eoprec_square_eos_128(spincolor *sol,spincolor *guess,quad_su3 **conf,double kappa,double mu,int niter,int rniter,double external_solver_residue,spincolor *external_source);

#endif