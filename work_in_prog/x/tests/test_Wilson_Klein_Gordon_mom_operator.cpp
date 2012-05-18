#include <math.h>

#include "nissa.h"

#include "../src/types.h"
#include "../src/types_routines.h"
#include "../src/Wilson_gluon_propagator.h"
#include "../src/tlSym_gluon_propagator.h"

spin1prop *prop_wi_fft;
spin1prop *prop_tl_fft;

//initialize the program
void init_test()
{
  //Basic mpi initialization
  init_nissa();
  
  //init the grid
  init_grid(8,4);
  
  //allocatepropagators
  prop_wi_fft=nissa_malloc("prop_wi_fft",loc_vol,spin1prop);
  prop_tl_fft=nissa_malloc("prop_tl_fft",loc_vol,spin1prop);
}

//close the program
void close_test()
{
  nissa_free(prop_wi_fft);
  nissa_free(prop_tl_fft);
  
  close_nissa();
}

int main(int narg,char **arg)
{
  init_test();
  
  //anti-periodic boundary condition in one space direction
  double theta[4]={0,1,0,0};
  
  //covariant gauge fixing constant
  double alpha=0.3;
  
  //gluon information
  gluon_info gl=create_Wilson_gluon_info(alpha,theta);
  
  //compute the point to be printed
  coords ix={1,2,1,2};
  int lx,rx;
  get_loclx_and_rank_of_coord(&lx,&rx,ix);
  
  /////////////////////////////// propagator and pion computed analytically //////////////////////////
  
  compute_x_space_Wilson_gluon_propagator_by_fft(prop_wi_fft,gl);
  
  /////////////////////////////// propagator and pion computed numerically //////////////////////////
  
  compute_x_space_tlSym_gluon_propagator_by_fft(prop_tl_fft,gl);  
  
  /////////////////////////////////////////// output ////////////////////////////////////////////////
  
  if(rank==rx)
    {
      printf("\n\nComparing the propagator on site of coordinates: (%d,%d,%d,%d), rank: %d\n",ix[0],ix[1],ix[2],ix[3],rx);
      print_spinspin(prop_wi_fft[lx]);
      printf("\n");
      print_spinspin(prop_tl_fft[lx]);
    }

  //take the squared norm of the differnce between the two computed propagators
  
  double loc_d=0;
  nissa_loc_vol_loop(ivol)
    for(int id=0;id<32;id++)
      {
	double t=((double*)prop_wi_fft[ivol])[id]-((double*)prop_tl_fft[ivol])[id];
	loc_d+=t*t;
      }
  double glb_d=sqrt(glb_reduce_double(loc_d)/glb_vol);
  master_printf("\n\nAverage norm2 difference between Wilson fft and tlSym fft computed propagators: %lg\n\n",glb_d);
  
  /////////////////////////// check Wilson Klein Gordon operator in momentum space //////////////////////
  
  compute_mom_space_Wilson_gluon_propagator(prop_tl_fft,gl);
  
  spin1field *temp=nissa_malloc("temo",loc_vol,spin1field);
  
  for(int nu=0;nu<4;nu++)
    {
      //take index nu of the propagator
      for(int mu=0;mu<4;mu++)
	nissa_loc_vol_loop(imom)
	  memcpy(temp[imom][mu],prop_tl_fft[imom][mu][nu],sizeof(complex));
      
      //apply the KG operator in momentum space
      apply_Wilson_gluon_mom_Klein_Gordon_operator(temp,temp,gl);
      
      //put back index nu of the propagator
      for(int mu=0;mu<4;mu++)
	nissa_loc_vol_loop(imom)
	  memcpy(prop_wi_fft[imom][mu][nu],temp[imom][mu],sizeof(complex));
    }
  
  print_spinspin(prop_wi_fft[0]);
  
  nissa_free(temp);
    
  close_test();
  
  return 0;
}
