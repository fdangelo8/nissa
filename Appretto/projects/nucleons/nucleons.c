#include <mpi.h>
#include <lemon.h>

#include "appretto.h"

//configuration
int nconf;
char **conf_path,**out_path;
quad_su3 *conf;
double mass;
double kappa;

//source
int source_pos[4];
spincolor *source;
su3spinspin *original_source;
su3spinspin *seq_source;

//the propagators
su3spinspin *S0[2];
su3spinspin *S1;

//inverter
int nitermax;
double residue;
spincolor *solDD,*sol_reco[2];

//insertion
int tseparation;
int tsink;

spinspin Proj[2]; //projectors over N and N*
spinspin C5; //C*gamma5

//two points contractions
int nproton_2pt_contr=1;
int list_2pt_op1[1]={5};
int list_2pt_op2[1]={0};

//three point contractions
int nproton_3pt_contr=1;
int list_3pt_op[1]={4};

//                      e_00x   e_01x    e_02x     e_10x    e_11x   e_12x     e_20x   e_21x    e_22x
int epsilon[3][3][3]={{{0,0,0},{0,0,1},{0,-1,0}},{{0,0,-1},{0,0,0},{1,0,0}},{{0,1,0},{-1,0,0},{0,0,0}}};

//timings
double tinv=0,tcontr=0,tot_time=0;

dirac_matr gC;

void put_dirac_matr_into_spinspin(spinspin out,dirac_matr *in)
{
  memset(out,0,sizeof(spinspin));

  for(int id1=0;id1<4;id1++)
    {
      int id2=in->pos[id1];
      for(int ri=0;ri<2;ri++)
	out[id1][id2][ri]=in->entr[id1][ri];
    }
}

void initialize_nucleons(char *input_path)
{
  //C5
  complex ima={0,1};
  dirac_matr migC,gC5;
  dirac_prod(&migC,&(base_gamma[2]),&(base_gamma[4]));
  unsafe_dirac_compl_prod(&gC,&migC,ima);
  dirac_prod(&gC5,&gC,&(base_gamma[5]));
  
  put_dirac_matr_into_spinspin(C5,&gC5);
  
  //Proj[0] and Proj[1]
  for(int nns=0;nns<2;nns++) memset(Proj[nns],0,sizeof(spinspin));
  for(int id1=0;id1<4;id1++)
#ifdef APE
    if(id1==0||id1==2)
#endif
    {
      int id2=base_gamma[4].pos[id1];
      
      Proj[0][id1][id1][0]=Proj[1][id1][id1][0]=0.5;
#ifndef APE //this is not needed by APE
      complex_prod_with_real(Proj[0][id1][id2],base_gamma[4].entr[id1],+0.5);
      complex_prod_with_real(Proj[1][id1][id2],base_gamma[4].entr[id1],-0.5);
#endif
    }
#ifdef APE //because only this is needed
  Proj[0][0][2][0]=Proj[0][2][0][0]=-0.5; 
  Proj[1][0][2][0]=Proj[1][2][0][0]=+0.5;
#endif
  open_input(input_path);

  // 1) Information about the gauge conf
  
  read_str_int("L",&(glb_size[1]));
  read_str_int("T",&(glb_size[0]));
  //Init the MPI grid 
  init_grid();
  //Allocate the gauge Conf
  conf=allocate_quad_su3(loc_vol+loc_bord,"conf");
  //Read the gauge conf
  read_str_int("NGaugeConf",&nconf);
  conf_path=(char**)malloc(sizeof(char*)*nconf);
  out_path=(char**)malloc(sizeof(char*)*nconf);
  for(int iconf=0;iconf<nconf;iconf++)
    {
      conf_path[iconf]=(char*)malloc(1024);
      out_path[iconf]=(char*)malloc(1024);
      read_str_str("GaugeConfPath",conf_path[iconf],1024);
      read_str(out_path[iconf],1024);
    }
  //Put border condition and communicate
  //double theta[4]={1,0,0,0};

  //put_boundaries_conditions(conf,theta,1,0);
  //Kappa
  read_str_double("Kappa",&(kappa));
  
  // 2) Source position and mass
  read_str_int("SourcePosition",&(source_pos[0]));
  for(int i=1;i<4;i++) read_int(&(source_pos[i]));
  read_str_double("Mass",&mass);

  // 3) inverter

  //Residue
  read_str_double("Residue",&residue);
  //Number of iterations
  read_str_int("NiterMax",&nitermax);
  
  // 4) insertion info
  //tsink-tsource
  read_str_int("TSeparation",&tseparation);
  tsink=(source_pos[0]+tseparation)%glb_size[0];
  
  close_input();

  ///////////////////// Allocate the various spinors ///////////////////////
  
  original_source=allocate_su3spinspin(loc_vol,"original_source");
  
  source=allocate_spincolor(loc_vol+loc_bord,"source");
  solDD=allocate_spincolor(loc_vol+loc_bord,"solDD");

  sol_reco[0]=allocate_spincolor(loc_vol,"solution_reco[0]");
  sol_reco[1]=allocate_spincolor(loc_vol,"solution_reco[1]");

  S0[0]=allocate_su3spinspin(loc_vol,"S0[0]");
  S0[1]=allocate_su3spinspin(loc_vol,"S0[1]");

  seq_source=allocate_su3spinspin(loc_vol,"seqential_source");

  S1=allocate_su3spinspin(loc_vol,"S1");
}

//read a configuration and put anti-periodic condition at the slice tsource-1
void read_conf_and_put_antiperiodic(quad_su3 *conf,char *conf_path,int tsource)
{
  read_local_gauge_conf(conf,conf_path);
  
  for(int ivol=0;ivol<loc_vol;ivol++)
    if(glb_coord_of_loclx[ivol][0]==(tsource+glb_size[0]-1)%glb_size[0])
      for(int ic1=0;ic1<3;ic1++)
	for(int ic2=0;ic2<3;ic2++)
	  for(int ri=0;ri<2;ri++)
	    conf[ivol][0][ic1][ic2][ri]*=-1;
  communicate_gauge_borders(conf);  
}

//create a 12 index point source
void prepare_source()
{
  int isloc=1;

  int lx[4];

  memset(original_source,0,sizeof(spincolor)*loc_vol);

  for(int idir=0;idir<4;idir++)
    {
      lx[idir]=source_pos[idir]-proc_coord[idir]*loc_size[idir];
      isloc=isloc && (lx[idir]>=0 && lx[idir]<loc_size[idir]);
    }

  int ivol=loclx_of_coord(lx);

  if(isloc)
    for(int ic=0;ic<3;ic++)
      for(int id=0;id<4;id++)
	original_source[ivol][ic][ic][id][id][0]=1;
}      

//perform the first inversion to produce the S0 for u and d
void calculate_S0()
{
  for(int ic_sour=0;ic_sour<3;ic_sour++)
    for(int id_sour=0;id_sour<4;id_sour++)
      { //take the source and put g5
	for(int ivol=0;ivol<loc_vol;ivol++)
	  {
	    get_spincolor_from_su3spinspin(source[ivol],original_source[ivol],id_sour,ic_sour);
	    for(int id_sink=2;id_sink<4;id_sink++)
	      for(int ic_sink=0;ic_sink<3;ic_sink++)
		for(int ri=0;ri<2;ri++)
		  source[ivol][id_sink][ic_sink][ri]=-source[ivol][id_sink][ic_sink][ri];
	  }
	
	if(rank==0) printf("\n(S0) source index: id=%d, ic=%d\n",id_sour,ic_sour);

	tinv-=take_time();
	inv_Q2_cg(solDD,source,NULL,conf,kappa,mass,nitermax,1,residue);	
	tinv+=take_time();
	reconstruct_doublet(sol_reco[0],sol_reco[1],solDD,conf,kappa,mass);
	
	for(int r=0;r<2;r++) //convert the id-th spincolor into the colorspinspin
	  for(int ivol=0;ivol<loc_vol;ivol++)
	    put_spincolor_into_su3spinspin(S0[r][ivol],sol_reco[r][ivol],id_sour,ic_sour);
      }
  
  if(rank==0) printf("inversions finished\n");
  
  //put the (1+ig5)/sqrt(2) factor
  for(int r=0;r<2;r++) //remember that D^-1 rotate opposite than D!
    for(int ivol=0;ivol<loc_vol;ivol++)
      for(int ic1=0;ic1<3;ic1++)
	for(int ic2=0;ic2<3;ic2++)
	  rotate_spinspin_to_physical_basis(S0[r][ivol][ic1][ic2],!r,!r);
  
  if(rank==0) printf("rotations performed\n");
}

//Calculate the proton contraction for a single point
void point_proton_contraction(spinspin contr,su3spinspin SU,su3spinspin SD,dirac_matr gamma1,dirac_matr gamma2,dirac_matr gamma3,dirac_matr gamma4)
{
  memset(contr,0,sizeof(spinspin));
  
  //prepare the gammas
  dirac_matr Cgamma1,Cgamma3;
  dirac_prod(&Cgamma1,&gC,&gamma1);
  dirac_prod(&Cgamma3,&gC,&gamma3);

  for(int a1=0;a1<3;a1++)
    for(int b1=0;b1<3;b1++)
      for(int c1=0;c1<3;c1++)
	if(epsilon[a1][b1][c1])
	  for(int a2=0;a2<3;a2++)
	    for(int b2=0;b2<3;b2++)
	      for(int c2=0;c2<3;c2++)
		if(epsilon[a2][b2][c2])
		  for(int al1=0;al1<4;al1++)
		    for(int al2=0;al2<4;al2++)
		      for(int ga1=0;ga1<4;ga1++)
			for(int ga2=0;ga2<4;ga2++)
			  {
			    int be1=Cgamma1.pos[al1];
			    int be2=Cgamma3.pos[al2];
			    
			    int delta1=gamma2.pos[ga1];
			    int delta2=gamma4.pos[ga2];
			    
			    int se=epsilon[a1][b1][c1]*epsilon[a2][b2][c2];
			    
			    complex ter;
			    unsafe_complex_prod(ter,SU[a1][a2][al1][al2],SU[c1][c2][delta1][delta2]);
			    complex_subt_the_prod(ter,SU[a1][c2][al1][delta2],SU[c1][a2][delta1][al2]);
				
			    safe_complex_prod(ter,SD[b1][b2][be1][be2],ter);
			    safe_complex_prod(ter,Cgamma1.entr[al1],ter);
			    safe_complex_prod(ter,Cgamma3.entr[al2],ter);
			    
			    safe_complex_prod(ter,ter,gamma2.entr[ga1]);
			    safe_complex_prod(ter,ter,gamma4.entr[ga2]);
			    
			    for(int ri=0;ri<2;ri++)
			      if(se==1) contr[ga1][ga2][ri]+=ter[ri];
			      else      contr[ga1][ga2][ri]-=ter[ri];
			  }
}

//calculate all the 2pts contractions
void calculate_all_2pts(char *path)
{
  //output file
  FILE *output=open_text_file_for_output(path);

  tcontr-=take_time();
  
  char pm_tag[2][2]={"+","-"};
  
  complex *loc_contr[2],*glb_contr[2];
  for(int nns=0;nns<2;nns++)
    {
      loc_contr[nns]=(complex*)malloc(sizeof(complex)*glb_size[0]);
      glb_contr[nns]=(complex*)malloc(sizeof(complex)*glb_size[0]);
    }
  
  spinspin ter;
  complex point_contr[2];
  
  for(int icontr=0;icontr<nproton_2pt_contr;icontr++)
    {
      for(int rlike=0;rlike<2;rlike++)
	for(int rdislike=0;rdislike<2;rdislike++)
	  {
	    dirac_matr o1=base_gamma[list_2pt_op1[icontr]],o2=base_gamma[list_2pt_op2[icontr]];
	    
	    if(rank==0) fprintf(output," # Two point for rlike=%d, rdislike=%d\n",rlike,rdislike);
	    
	    //perform the proton contraction putting operators on the sink or on the source
	    for(int SS=0;SS<2;SS++)
	      {
		//reset output
		for(int nns=0;nns<2;nns++) memset(loc_contr[nns],0,sizeof(complex)*glb_size[0]);
		
		//local loop
		for(int loc_site=0;loc_site<loc_vol;loc_site++)
		  {
		    int glb_t=glb_coord_of_loclx[loc_site][0];
		    
		    if(SS==0) point_proton_contraction(ter,S0[rlike][loc_site],S0[rdislike][loc_site],o1,o2,base_gamma[5],base_gamma[0]);
		    else point_proton_contraction(ter,S0[rlike][loc_site],S0[rdislike][loc_site],base_gamma[5],base_gamma[0],o1,o2);
		    
		    for(int nns=0;nns<2;nns++)
		      {
			trace_prod_spinspins(point_contr[nns],ter,Proj[nns]);
			complex_summ(loc_contr[nns][glb_t],loc_contr[nns][glb_t],point_contr[nns]);
		      }
		  }
		
		//final reduction
		for(int nns=0;nns<2;nns++) MPI_Reduce(loc_contr[nns],glb_contr[nns],2*glb_size[0],MPI_DOUBLE,MPI_SUM,0,MPI_COMM_WORLD);
		
		if(rank==0)
		  {
		    if(SS==0) fprintf(output," # %s%s-P5S0\n",gtag[list_2pt_op2[icontr]],gtag[list_2pt_op1[icontr]]);
		    else      fprintf(output," # P5S0-%s%s\n",gtag[list_2pt_op2[icontr]],gtag[list_2pt_op1[icontr]]);
		    for(int nns=0;nns<2;nns++)
		      {
			fprintf(output,"# Contraction with (1%sg4)/2\n",pm_tag[nns]);
			for(int tt=0;tt<glb_size[0];tt++)
			  {
			    int t=(tt+source_pos[0])%glb_size[0];
			    fprintf(output," %+016.16g\t%+016.16g\n",glb_contr[nns][t][0],glb_contr[nns][t][1]);
			  }
			fprintf(output,"\n");
		      }
		  }
	      }
	  }
    }
  
  tcontr+=take_time();

  if(rank==0)
    {
      printf("contractions ultimated\n");
      fclose(output);
    }

  for(int nns=0;nns<2;nns++)
    {    
      free(loc_contr[nns]);
      free(glb_contr[nns]);
    }
}

void prepare_like_sequential_source(int rlike,int rdislike,int slice_to_take)
{
  memset(seq_source,0,sizeof(su3spinspin)*loc_vol);
  
  for(int ivol=0;ivol<loc_vol;ivol++)
    if(glb_coord_of_loclx[ivol][0]==slice_to_take)
      {
	for(int a1=0;a1<3;a1++)
	  for(int b1=0;b1<3;b1++)
	    for(int c1=0;c1<3;c1++)
	      if(epsilon[a1][b1][c1])
		for(int a2=0;a2<3;a2++)
		  for(int b2=0;b2<3;b2++)
		    for(int c2=0;c2<3;c2++)
		      if(epsilon[a2][b2][c2])
			for(int mu1=0;mu1<4;mu1++)
			  for(int mu2=0;mu2<4;mu2++)
			    for(int be1=0;be1<4;be1++)
			      for(int be2=0;be2<4;be2++)
				for(int al1=0;al1<4;al1++)
				  for(int al2=0;al2<4;al2++)
				    if((C5[be1][mu1][0]||C5[be1][mu1][1])&&(C5[be2][mu2][0]||C5[be2][mu2][1]))
				      {
					int se=epsilon[a1][b1][c1]*epsilon[a2][b2][c2];
					
					complex ter;
					unsafe_complex_prod(ter,S0[rlike][ivol][b1][b2][al1][al2],S0[rlike][ivol][c1][c2][be1][be2]);
					complex_summ_the_prod(ter,S0[rlike][ivol][b1][b2][al1][be2],S0[rlike][ivol][c1][c2][be1][al2]);
					
					safe_complex_prod(ter,C5[be1][mu1],ter);
					safe_complex_prod(ter,C5[be2][mu2],ter);
					safe_complex_prod(ter,Proj[0][al2][al1],ter);
					
					for(int ri=0;ri<2;ri++)
					  if(se==1) seq_source[ivol][a2][a1][mu2][mu1][ri]+=ter[ri];
					  else      seq_source[ivol][a2][a1][mu2][mu1][ri]-=ter[ri];
				      }
      }
}

void prepare_dislike_sequential_source(int rlike,int rdislike,int slice_to_take)
{
  memset(seq_source,0,sizeof(su3spinspin)*loc_vol);
  
  for(int ivol=0;ivol<loc_vol;ivol++)
    if(glb_coord_of_loclx[ivol][0]==slice_to_take)
      {
	for(int x=0;x<3;x++)
	  for(int z=0;z<3;z++)
	    for(int rho=0;rho<4;rho++)
	      for(int tau=0;tau<4;tau++)
		{
		  for(int ga1=0;ga1<4;ga1++)
		    for(int ga2=0;ga2<4;ga2++)
		      {
			if(Proj[0][ga2][ga1][0]||Proj[0][ga2][ga1][1])
			  for(int a1=0;a1<3;a1++)
			    for(int b1=0;b1<3;b1++)
			      for(int c1=0;c1<3;c1++)
				if(epsilon[a1][b1][c1])
				  for(int al1=0;al1<4;al1++)
				    for(int be1=0;be1<4;be1++)
				      if(C5[al1][be1][0]||C5[al1][be1][1])
					  for(int be2=0;be2<4;be2++)
					    {
					      //first part
					      if(C5[rho][be2][0]||C5[rho][be2][1])
						for(int b2=0;b2<3;b2++)
						  for(int c2=0;c2<3;c2++)
						    if(epsilon[x][b2][c2])
						      {
							complex part={0,0};

							if(a1==z && al1==tau) complex_summ(part,part,S0[rlike][ivol][c1][c2][ga1][ga2]);
							if(c1==z && ga1==tau) complex_subt(part,part,S0[rlike][ivol][a1][c2][al1][ga2]);
							
							safe_complex_prod(part,C5[rho][be2],part);
							complex_prod_with_real(part,part,epsilon[x][b2][c2]);
							safe_complex_prod(part,S0[rdislike][ivol][b1][b2][be1][be2],part);
							safe_complex_prod(part,Proj[0][ga2][ga1],part);
							
							if(epsilon[a1][b1][c1]==1) complex_summ_the_prod(seq_source[ivol][x][z][rho][tau],part,C5[al1][be1]);
							else complex_subt_the_prod(seq_source[ivol][x][z][rho][tau],part,C5[al1][be1]);
						      }
					      
					      //second part
					      for(int al2=0;al2<4;al2++)					       
						if((C5[al2][be2][0]||C5[al2][be2][1])&&(ga2==rho))
						  for(int a2=0;a2<3;a2++)
						    for(int b2=0;b2<3;b2++)
						      if(epsilon[a2][b2][x])
							{
							  complex part={0,0};
							  
							  if(c1==z && ga1==tau) complex_summ(part,part,S0[rlike][ivol][a1][a2][al1][al2]);
							  if(a1==z && al1==tau) complex_subt(part,part,S0[rlike][ivol][c1][a2][ga1][al2]);
							  
							  safe_complex_prod(part,C5[al2][be2],part);
							  complex_prod_with_real(part,part,epsilon[a2][b2][x]);
							  safe_complex_prod(part,S0[rdislike][ivol][b1][b2][be1][be2],part);
							  safe_complex_prod(part,Proj[0][ga2][ga1],part);
							  
							  if(epsilon[a1][b1][c1]==1) complex_summ_the_prod(seq_source[ivol][x][z][rho][tau],part,C5[al1][be1]);
							  else complex_subt_the_prod(seq_source[ivol][x][z][rho][tau],part,C5[al1][be1]);
							}
					    }
		      }
		}
      }
}

void calculate_S1_like(int rlike,int rdislike)
{
  for(int id_sink=0;id_sink<4;id_sink++)
    for(int ic_sink=0;ic_sink<3;ic_sink++)
      { //take the source and put g5 on the source, and pre-rotate
	for(int ivol=0;ivol<loc_vol;ivol++)
	  {
	    for(int id_sour=0;id_sour<4;id_sour++)
	      for(int ic_sour=0;ic_sour<3;ic_sour++)
		for(int ri=0;ri<2;ri++)
		  if(id_sour<2)source[ivol][id_sour][ic_sour][ri]= seq_source[ivol][ic_sink][ic_sour][id_sink][id_sour][ri];
		  else         source[ivol][id_sour][ic_sour][ri]=-seq_source[ivol][ic_sink][ic_sour][id_sink][id_sour][ri];
	    if(rdislike==0) safe_spincolor_prod_dirac(source[ivol],source[ivol],&Pminus); //up
	    else            safe_spincolor_prod_dirac(source[ivol],source[ivol],&Pplus);  //dw
	  }

	if(rank==0) printf("\n(S1) like, rlike=%d rdislike=%d, sink index: id=%d, ic=%d\n",rlike,rdislike,id_sink,ic_sink);

	tinv-=take_time();
	inv_Q2_cg_left(solDD,source,NULL,conf,kappa,mass,nitermax,1,residue);
	tinv+=take_time();
	
	//use sol_reco[0] as temporary storage. We solve for rdislike
	if(rdislike==0)
	  {
	    apply_Q_left(sol_reco[0],solDD,conf,kappa, mass); //cancel d
	    for(int ivol=0;ivol<loc_vol;ivol++) safe_spincolor_prod_dirac(sol_reco[0][ivol],sol_reco[0][ivol],&Pminus);
	  }
	else
	  {
            apply_Q_left(sol_reco[0],solDD,conf,kappa,-mass); //cancel u
	    for(int ivol=0;ivol<loc_vol;ivol++) safe_spincolor_prod_dirac(sol_reco[0][ivol],sol_reco[0][ivol],&Pplus);
	  }
	
	for(int ivol=0;ivol<loc_vol;ivol++)
	  for(int id_sour=0;id_sour<4;id_sour++)
	    for(int ic_sour=0;ic_sour<3;ic_sour++)
	      for(int ri=0;ri<2;ri++)
		S1[ivol][ic_sink][ic_sour][id_sink][id_sour][ri]=sol_reco[0][ivol][id_sour][ic_sour][ri];
      }
  
  if(rank==0) printf("like sequential inversions finished\n");
}

void calculate_S1_dislike(int rlike,int rdislike)
{
  for(int id_sink=0;id_sink<4;id_sink++)
    for(int ic_sink=0;ic_sink<3;ic_sink++)
      { //take the source and put g5 on the source
	for(int ivol=0;ivol<loc_vol;ivol++)
	  {
	    for(int id_sour=0;id_sour<4;id_sour++)
	      for(int ic_sour=0;ic_sour<3;ic_sour++)
		for(int ri=0;ri<2;ri++)
		  if(id_sour<2)source[ivol][id_sour][ic_sour][ri]= seq_source[ivol][ic_sink][ic_sour][id_sink][id_sour][ri];
		  else         source[ivol][id_sour][ic_sour][ri]=-seq_source[ivol][ic_sink][ic_sour][id_sink][id_sour][ri];
	    
	    if(rlike==0) safe_spincolor_prod_dirac(source[ivol],source[ivol],&Pminus); //up
	    else         safe_spincolor_prod_dirac(source[ivol],source[ivol],&Pplus);  //dw
	  }

	if(rank==0) printf("\n(S1) dislike, rlike=%d rdislike=%d, sink index: id=%d, ic=%d\n",rlike,rdislike,id_sink,ic_sink);
	
	tinv-=take_time();
	inv_Q2_cg_left(solDD,source,NULL,conf,kappa,mass,nitermax,1,residue);
	tinv+=take_time();
	
	//use sol_reco[0] as temporary storage. We solve for rlike
	if(rlike==0)
	  {
	    apply_Q_left(sol_reco[0],solDD,conf,kappa, mass); //cancel d
	    for(int ivol=0;ivol<loc_vol;ivol++) safe_spincolor_prod_dirac(sol_reco[0][ivol],sol_reco[0][ivol],&Pminus); //up
	  }
	else
	  {
	    apply_Q_left(sol_reco[0],solDD,conf,kappa,-mass); //cancel u
	    for(int ivol=0;ivol<loc_vol;ivol++) safe_spincolor_prod_dirac(sol_reco[0][ivol],sol_reco[0][ivol],&Pplus);  //dw
	  }
	
	for(int ivol=0;ivol<loc_vol;ivol++)
	  for(int id_sour=0;id_sour<4;id_sour++)
	    for(int ic_sour=0;ic_sour<3;ic_sour++)
	      for(int ri=0;ri<2;ri++)
		S1[ivol][ic_sink][ic_sour][id_sink][id_sour][ri]=sol_reco[0][ivol][id_sour][ic_sour][ri];
      }
  
  if(rank==0) printf("dislike sequential inversions finished\n");
}

//this is needed to check 2pts
void contract_with_source(complex *glb_contr,su3spinspin *eta,su3spinspin *S)
{
  complex *loc_contr=malloc(sizeof(complex)*glb_size[0]);
  memset(loc_contr,0,sizeof(complex)*glb_size[0]);

  for(int ivol=0;ivol<loc_vol;ivol++)
    for(int id1=0;id1<4;id1++)
      for(int id2=0;id2<4;id2++)
	for(int ic1=0;ic1<3;ic1++)
	  for(int ic2=0;ic2<3;ic2++) //eta+*S
	    complex_summ_the_conj1_prod(loc_contr[glb_coord_of_loclx[ivol][0]],eta[ivol][ic2][ic1][id2][id1],S[ivol][ic2][ic1][id2][id1]);

  MPI_Reduce(loc_contr,glb_contr,2*glb_size[0],MPI_DOUBLE,MPI_SUM,0,MPI_COMM_WORLD);
  
  free(loc_contr);
}

//check two points with the current sequential source
void check_2pts_with_current_sequential_source(char *path)
{
  FILE *fout=open_text_file_for_output(path);
  
  complex *contr_2pts=malloc(sizeof(complex)*glb_size[0]);
  
  contract_with_source(contr_2pts,original_source,S1);
  
  if(rank==0)
    {
      fprintf(fout," %+016.16g\t%+016.16g\n",contr_2pts[source_pos[0]][0],contr_2pts[source_pos[0]][1]);
      fclose(fout);
    }
  
  free(contr_2pts);
}

//Calculate the proton contraction with the inserction of a gamma
void point_proton_sequential_contraction(complex contr,su3spinspin S0,dirac_matr g,su3spinspin S1)
{
  contr[0]=contr[1]=0;
  
  complex temp;

  for(int ic1=0;ic1<3;ic1++)
    for(int ic2=0;ic2<3;ic2++)
      for(int id1=0;id1<4;id1++)
	for(int id2=0;id2<4;id2++)
	  {
	    int idg=g.pos[id2];
	    unsafe_complex_prod(temp,        S0[ic2][ic1][id2][id1],g.entr[id2]);
	    complex_summ_the_prod(contr,temp,S1[ic1][ic2][id1][idg]);
	  }
}

//Calculate the proton contraction with the inserction of the dipole operator
void point_proton_sequential_J0_contraction(complex contr,su3spinspin S0,double t,su3spinspin S1)
{
  contr[0]=contr[1]=0;
  
  complex temp;
  if(t>glb_size[0]/2) t-=glb_size[0];

  for(int icso=0;icso<3;icso++)
    for(int icsi=0;icsi<3;icsi++)
      for(int idso=0;idso<4;idso++)
	for(int idsi=0;idsi<4;idsi++)
	  {
	    complex_prod_real(temp,S0[icsi][icso][idsi][idso],t);
	    complex_summ_the_prod(contr,temp,S1[icsi][icso][idsi][idso]);
	  }
}

//calculate all the 3pts contractions
void calculate_all_3pts_with_current_sequential(int rlike,int rdislike,int rS0,char *path,const char *tag)
{
  FILE *fout=open_text_file_for_output(path);

  tcontr-=take_time();
  
  complex *loc_contr=(complex*)malloc(sizeof(complex)*glb_size[0]);
  complex *glb_contr=(complex*)malloc(sizeof(complex)*glb_size[0]);

  for(int icontr=0;icontr<nproton_3pt_contr;icontr++)
    {
      //reset output
      memset(loc_contr,0,sizeof(complex)*glb_size[0]);
      
      for(int loc_site=0;loc_site<loc_vol;loc_site++)
	{
	  complex point_contr;
	  int glb_t=glb_coord_of_loclx[loc_site][0];
	  point_proton_sequential_contraction(point_contr,S0[rS0][loc_site],base_gamma[list_3pt_op[icontr]],S1[loc_site]);
	  complex_summ(loc_contr[glb_t],loc_contr[glb_t],point_contr);
	}
      
      MPI_Reduce(loc_contr,glb_contr,2*glb_size[0],MPI_DOUBLE,MPI_SUM,0,MPI_COMM_WORLD);
      
      if(rank==0)
	{
	  fprintf(fout," # Three point for rlike=%d, rdislike=%d, %s source\n",rlike,rdislike,tag);
	  fprintf(fout," # Proton-%s-Proton\n",gtag[list_3pt_op[icontr]]);
	  for(int tt=0;tt<glb_size[0];tt++)
	    {
	      int t=(tt+source_pos[0])%glb_size[0];
	      fprintf(fout," %+016.16g\t%+016.16g\n",glb_contr[t][0],glb_contr[t][1]);
	    }
	  fprintf(fout,"\n");
	}
    }

  tcontr+=take_time();
  
  if(rank==0)
    {
      fclose(fout);
      printf("contractions ultimated\n");
    }

  free(loc_contr);
  free(glb_contr);
}

int main(int narg,char **arg)
{
  //basic mpi initialization
  init_appretto();

  tot_time-=take_time();

  if(narg<2 && rank==0)
    {
      fprintf(stderr,"Use: %s input_file\n",arg[0]);
      fflush(stderr);
      MPI_Abort(MPI_COMM_WORLD,1);
    }
  
  initialize_nucleons(arg[1]);
  
  ///////////////////////////////////////////
  
  prepare_source();
  
  for(int iconf=0;iconf<nconf;iconf++)
    {
      read_conf_and_put_antiperiodic(conf,conf_path[iconf],source_pos[0]);
      
      calculate_S0();
      calculate_all_2pts(out_path[iconf]);

      
      for(int rlike=0;rlike<2;rlike++)
	for(int rdislike=1;rdislike>=0;rdislike--)
	  {
	    char out2pts_check_like[1024],out3pts_like[1024];
	    char out2pts_check_dislike[1024],out3pts_dislike[1024];
	    
	    sprintf(out2pts_check_like,"2pts_check_like%d%d%d",rlike,rlike,rdislike);
	    sprintf(out2pts_check_dislike,"2pts_check_dislike%d%d%d",rlike,rlike,rdislike);
	    sprintf(out3pts_like,"3pts_like%d%d%d",rlike,rlike,rdislike);
	    sprintf(out3pts_dislike,"3pts_dislike%d%d%d",rlike,rlike,rdislike);
	    
	    //like three points
	    prepare_like_sequential_source(rlike,rdislike,tsink);
	    calculate_S1_like(rlike,rdislike);
	    check_2pts_with_current_sequential_source(out2pts_check_like);
	    calculate_all_3pts_with_current_sequential(rlike,rdislike,rdislike,out3pts_like,"like");
	    
	    //dislike three points
	    prepare_dislike_sequential_source(rlike,rdislike,tsink);
	    calculate_S1_dislike(rlike,rdislike);
	    check_2pts_with_current_sequential_source(out2pts_check_dislike);
	    calculate_all_3pts_with_current_sequential(rlike,rdislike,rlike,out3pts_dislike,"dislike");
	  }
      
    }

  tot_time+=take_time();

  ///////////////////////////////////////////

  if(rank==0)
    {
      printf("Total time: %g s\n",tot_time);
      printf("-inversion time: %g%s avg: %d s\n",tinv/tot_time*100,"%",(int)(tinv/nconf/12));
      printf("-contraction time: %g%s\n",tcontr/tot_time*100,"%");
    }

  close_appretto();

  return 0;
}
