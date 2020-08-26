/*
 *  PIXON
 *  A Pixon-based method for reconstructing velocity-delay map in reverberation mapping.
 * 
 *  Yan-Rong Li, liyanrong@mail.ihep.ac.cn
 * 
 */
#include <iostream>
#include <vector>
#include <iomanip>
#include <cmath>
#include <cstring>
#include <random>
#include <nlopt.hpp>
#include <fftw3.h>

#include "proto.hpp"
#include "utilities.hpp"
#include "cont_model.hpp"
#include "pixon_cont.hpp"
#include "tnc.h"

using namespace std;

int main(int argc, char ** argv)
{
  if(argc < 2)
  {
    cout<<"Please input pixon type."<<endl;
    exit(0);
  }
  int pixon_type = atoi(argv[1]);

  cout<<"Pixon type: "<<pixon_type<<","<<PixonBasis::pixonbasis_name[pixon_type]<<endl;

  Data cont, line;
  string fcon, fline;
  fcon="data/con.txt";
  cont.load(fcon);
  fline = "data/line.txt";
  line.load(fline);

  /* continuum reconstruction */
  double tback = 900.0, tforward = 100.0;
  cont_model = new ContModel(cont, tback, tforward);
  cont_model->mcmc();
  cont_model->get_best_params();
  cont_model->recon();
  
  int npixel;
  int npixon;
  double tau_range;
  double *pimg;

  pixon_sub_factor = 1;
  pixon_size_factor = 1;
  pixon_map_low_bound = pixon_sub_factor - 1;
  npixon = 20*pixon_sub_factor/pixon_size_factor;

  tau_range = tback;
  npixel = tau_range / (cont_model->cont_recon.time[1]-cont_model->cont_recon.time[0]);
  pimg = new double[npixel];
  switch(pixon_type)
  {
    case 0:  /* Gaussian */
      
      PixonBasis::norm_gaussian = sqrt(2.0*M_PI) * erf(3.0*pixon_size_factor/sqrt(2.0));

      pixon_function = PixonBasis::gaussian;
      pixon_norm = PixonBasis::gaussian_norm;
      break;
    
    case 1:
      PixonBasis::coeff1_modified_gaussian = exp(-0.5 * pixon_size_factor*3.0*pixon_size_factor*3.0);
      PixonBasis::coeff2_modified_gaussian = 1.0 - PixonBasis::coeff1_modified_gaussian;
      PixonBasis::norm_gaussian = (sqrt(2.0*M_PI) * erf(3.0*pixon_size_factor/sqrt(2.0)) 
                    - 2.0*3.0*pixon_size_factor * PixonBasis::coeff1_modified_gaussian)/PixonBasis::coeff2_modified_gaussian;
      
      pixon_function = PixonBasis::modified_gaussian;
      pixon_norm = PixonBasis::modified_gaussian_norm;
      break;
    
    case 2:  /* Lorentz */ 
      pixon_function = PixonBasis::lorentz;
      pixon_norm = PixonBasis::lorentz_norm;
      break;

    case 3:  /* parabloid */      
      pixon_function = PixonBasis::parabloid;
      pixon_norm = PixonBasis::parabloid_norm;
      break;
    
    case 4:  /* triangle */ 
      pixon_function = PixonBasis::triangle;
      pixon_norm = PixonBasis::triangle_norm;
      break;
    
    case 5:  /* top-hat */ 
      pixon_sub_factor = 1; /* enforce to 1 */
      pixon_function = PixonBasis::tophat;
      pixon_norm = PixonBasis::tophat_norm;
      break;
    
    default:  /* default */
      PixonBasis::norm_gaussian = sqrt(2.0*M_PI) * erf(3.0*pixon_size_factor/sqrt(2.0));

      pixon_function = PixonBasis::gaussian;
      pixon_norm = PixonBasis::gaussian_norm;
      break;
  }
  
  run_cont_pixon(cont, cont_model->cont_recon, line, pimg, npixel, npixon, pixon_type);
  //return 0;
  
  npixon = fmax(10, fmin(npixon*2, 40*pixon_sub_factor));
  run_uniform(cont_model->cont_recon, line, pimg, npixel, npixon, pixon_type);
  npixon = fmax(10, fmin(npixon*2, 40*pixon_sub_factor));
  run(cont_model->cont_recon, line, pimg, npixel, npixon, pixon_type);
  delete[] pimg;

  return 0;
}

/* set continuum free and use pixons to model continuum */
void run_cont_pixon(Data& cont_data, Data& cont_recon, Data& line, double *pimg, int npixel, int& npixon, int pixon_type)
{
  int i, iter;
  int npixon_cont = 10;
  PixonCont pixon(cont_data, cont_recon, line, npixel, npixon, npixon_cont);
  void *args = (void *)&pixon;
  double f, f_old, num, num_old, chisq, chisq_old, df, dnum;
  double *image_cont=new double[cont_recon.size];

  /* TNC */
  int rc, maxCGit = cont_recon.size, maxnfeval = 10000, nfeval, niter;
  double eta = -1.0, stepmx = -1.0, accuracy =  1.0e-6, fmin = pixon.cont_data.size, 
    ftol = 1.0e-6, xtol = 1.0e-6, pgtol = 1.0e-6, rescale = -1.0;
  
  /* NLopt */
  nlopt::opt opt0(nlopt::LN_BOBYQA, cont_recon.size);
  vector<double> x_cont(cont_recon.size), g_cont(cont_recon.size), x_old_cont(cont_recon.size);
  vector<double> low_cont(cont_recon.size), up_cont(cont_recon.size);

  /* bounds and initial values */
  for(i=0; i<cont_recon.size; i++)
  {
    low_cont[i] = fmax(0.0, cont_recon.flux[i] - 3.0 * cont_recon.error[i]);
    up_cont[i] =  cont_recon.flux[i] + 3.0 * cont_recon.error[i];
    x_cont[i] = cont_recon.flux[i];
  }
  
  opt0.set_min_objective(func_nlopt_cont, args);
  opt0.set_lower_bounds(low_cont);
  opt0.set_upper_bounds(up_cont);
  opt0.set_maxeval(1000);
  opt0.set_ftol_abs(1.0e-6);
  opt0.set_xtol_abs(1.0e-6);
  
  opt0.optimize(x_cont, f);
  rc = tnc(cont_recon.size, x_cont.data(), &f, g_cont.data(), func_tnc_cont, args, 
      low_cont.data(), up_cont.data(), NULL, NULL, TNC_MSG_ALL,
      maxCGit, maxnfeval, eta, stepmx, accuracy, fmin, ftol, xtol, pgtol,
      rescale, &nfeval, &niter, NULL);
  
  f_old = f;
  num_old = pixon.compute_pixon_number_cont();
  pixon.compute_cont(x_cont.data());
  chisq_old = pixon.compute_chisquare_cont(x_cont.data());
  memcpy(image_cont, pixon.image_cont, cont_recon.size*sizeof(double));
  memcpy(x_old_cont.data(), x_cont.data(), cont_recon.size*sizeof(double));
  cout<<f_old<<"  "<<num_old<<"  "<<chisq_old<<endl;
 
  while(npixon_cont>2)
  {
    npixon_cont--;
    cout<<"npixon_cont:"<<npixon_cont<<",  size: "<<pixon.pfft_cont.pixon_sizes[npixon_cont-1]<<endl;
    
    pixon.reduce_ipixon_cont();
    num = pixon.compute_pixon_number_cont();
    
    for(i=0; i<cont_recon.size; i++)
    {
      if(x_cont[i] < low_cont[i])
        x_cont[i] = low_cont[i];
      if(x_cont[i] > up_cont[i])
        x_cont[i] = up_cont[i];
    }

    opt0.optimize(x_cont, f);
    rc = tnc(cont_recon.size, x_cont.data(), &f, g_cont.data(), func_tnc_cont, args, 
      low_cont.data(), up_cont.data(), NULL, NULL, TNC_MSG_ALL,
      maxCGit, maxnfeval, eta, stepmx, accuracy, fmin, ftol, xtol, pgtol,
      rescale, &nfeval, &niter, NULL);
    
    if(rc <0 || rc > 3)
    {
      opt0.optimize(x_cont, f);
    }
    
    pixon.compute_cont(x_cont.data());
    chisq = pixon.compute_chisquare_cont(x_cont.data());
    cout<<f<<"  "<<num<<"  "<<chisq<<endl;

    if(f <= fmin)
    {
      memcpy(image_cont, pixon.image_cont, cont_recon.size*sizeof(double));
      memcpy(x_old_cont.data(), x_cont.data(), cont_recon.size*sizeof(double));
      break;
    }
    
    df = f-f_old;
    dnum = num - num_old;

    if(-df < dnum * (1.0 + 1.0/sqrt(2.0*num)))
      break;

    num_old = num;
    f_old = f;
    chisq_old = chisq;
    memcpy(image_cont, pixon.image_cont, cont_recon.size*sizeof(double));
    memcpy(x_old_cont.data(), x_cont.data(), cont_recon.size*sizeof(double));
  }
  
  ofstream fp;
  fp.open("data/con_pixon.txt");
  for(i=0; i<cont_recon.size; i++)
  {
    fp<<pixon.cont.time[i]<<" "<<image_cont[i]<<endl;
  }
  fp.close();
  
  cout<<"Start to RM"<<endl;
  /* then continuum and line reverberation */
  pixon.cont.set_data(image_cont);
  /* TNC */
  int ndim = npixel + pixon.cont.size;
  maxCGit = ndim;
  fmin = pixon.line.size + pixon.cont_data.size;
  double *image=new double[ndim], *itline=new double[pixon.line.size];

  /* NLopt */
  nlopt::opt opt1(nlopt::LN_BOBYQA, ndim);
  vector<double> x(ndim), g(ndim), x_old(ndim);
  vector<double> low(ndim), up(ndim);

  /* bounds and initial values */
  for(i=0; i<npixel; i++)
  {
    low[i] = -100.0;
    up[i] =  10.0;
    x[i] = log(1.0/(npixel * pixon.dt));
  }
  for(i=0; i<pixon.cont.size; i++)
  {
    low[i+npixel] = fmax(0.0, cont_recon.flux[i] - 5.0 * cont_recon.error[i]);
    up[i+npixel] =            cont_recon.flux[i] + 5.0 * cont_recon.error[i];
    x[i+npixel] = pixon.cont.flux[i];
  }

  opt1.set_min_objective(func_nlopt_cont_rm, args);
  opt1.set_lower_bounds(low);
  opt1.set_upper_bounds(up);
  opt1.set_maxeval(1000);
  opt1.set_ftol_abs(1.0e-6);
  opt1.set_xtol_abs(1.0e-6);
  
  opt1.optimize(x, f);
  rc = tnc(ndim, x.data(), &f, g.data(), func_tnc_cont_rm, args, low.data(), up.data(), NULL, NULL, TNC_MSG_ALL,
      maxCGit, maxnfeval, eta, stepmx, accuracy, fmin, ftol, xtol, pgtol,
      rescale, &nfeval, &niter, NULL);
    
  f_old = f;
  num_old = pixon.compute_pixon_number();
  pixon.compute_rm_pixon(x.data());
  chisq_old = pixon.compute_chisquare(x.data());
  memcpy(image, pixon.image, npixel*sizeof(double));
  memcpy(image+npixel, pixon.image_cont, pixon.cont.size*sizeof(double));
  memcpy(itline, pixon.itline, pixon.line.size*sizeof(double));
  memcpy(x_old.data(), x.data(), ndim*sizeof(double));
  cout<<f_old<<"  "<<num_old<<"  "<<chisq_old<<endl;

  while(npixon>pixon_map_low_bound+1)
  {
    npixon--;
    cout<<"npixon:"<<npixon<<",  size: "<<pixon.pfft.pixon_sizes[npixon-1]<<endl;

    pixon.reduce_pixon_map_all();
    num = pixon.compute_pixon_number();

    for(i=0; i<ndim; i++)
    {
      if(x[i] < low[i])
        x[i] = low[i];
      if(x[i] > up[i])
        x[i] = up[i];
    }

    opt1.optimize(x, f);
    rc = tnc(ndim, x.data(), &f, g.data(), func_tnc_cont_rm, args, low.data(), up.data(), NULL, NULL, TNC_MSG_ALL,
      maxCGit, maxnfeval, eta, stepmx, accuracy, fmin, ftol, xtol, pgtol,
      rescale, &nfeval, &niter, NULL);
    
    if(rc <0 || rc > 3)
    {
      opt1.optimize(x, f);
    }
    
    pixon.compute_rm_pixon(x.data());
    chisq = pixon.compute_chisquare(x.data());
    cout<<f<<"  "<<num<<"  "<<chisq<<endl;

    if(f <= fmin)
    {
      memcpy(image, pixon.image, npixel*sizeof(double));
      memcpy(image+npixel, pixon.image_cont, pixon.cont.size*sizeof(double));
      memcpy(itline, pixon.itline, pixon.line.size*sizeof(double));
      memcpy(x_old.data(), x.data(), ndim*sizeof(double));
      break;
    }
    
    df = f-f_old;
    dnum = num - num_old;

    if(-df < dnum * (1.0 + 1.0/sqrt(2.0*num)))
      break;

    num_old = num;
    f_old = f;
    chisq_old = chisq;
    memcpy(image, pixon.image, npixel*sizeof(double));
    memcpy(image+npixel, pixon.image_cont, pixon.cont.size*sizeof(double));
    memcpy(itline, pixon.itline, pixon.line.size*sizeof(double));
    memcpy(x_old.data(), x.data(), ndim*sizeof(double));
  }
  
  ofstream fout;
  string fname;
  fname = "data/resp_cont.txt_" + to_string(pixon_type);
  fout.open(fname);
  for(i=0; i<npixel; i++)
  {
    fout<<pixon.dt*i<<"  "<<image[i]<<exp(x_old[i])<<endl;
  }
  fout.close();

  fname = "data/line_sim_cont.txt_" + to_string(pixon_type);
  fout.open(fname);
  for(i=0; i<pixon.line.size; i++)
  {
    fout<<pixon.line.time[i]<<"  "<<itline[i]<<"   "<<itline[i] - line.flux[i]<<endl;
  }
  fout.close();

  fp.open("data/con_pixon_rm.txt");
  for(i=0; i<pixon.cont.size; i++)
  {
    fp<<pixon.cont.time[i]<<" "<<image[i+npixel]<<endl;
  }
  fp.close();

  delete[] image_cont;
  delete[] image;
  delete[] itline;
}

/* set continuum fixed from a drw reconstruction and use pixel dependent pixon sizes */
void run(Data& cont, Data& line, double *pimg, int npixel, int& npixon, int pixon_type)
{
  int i, iter;
  Pixon pixon(cont, line, npixel, npixon);
  void *args = (void *)&pixon;
  bool flag;
  double f, f_old, num, num_old, chisq, chisq_old, df, dnum;
  double *image=new double[npixel], *itline=new double[pixon.line.size];
  
  /* TNC */
  int rc, maxCGit = npixel, maxnfeval = 10000, nfeval, niter;
  double eta = -1.0, stepmx = -1.0, accuracy =  1.0e-6, fmin = pixon.line.size, 
    ftol = 1.0e-6, xtol = 1.0e-6, pgtol = 1.0e-6, rescale = -1.0;
  
  /* NLopt */
  nlopt::opt opt0(nlopt::LN_BOBYQA, npixel);
  vector<double> x(npixel), g(npixel), x_old(npixel);
  vector<double>low(npixel), up(npixel);

  /* bounds and initial values */
  for(i=0; i<npixel; i++)
  {
    low[i] = -100.0;
    up[i] =  10.0;
    x[i] = pimg[i];
  }

  opt0.set_min_objective(func_nlopt, args);
  opt0.set_lower_bounds(low);
  opt0.set_upper_bounds(up);
  opt0.set_maxeval(10000);
  opt0.set_ftol_abs(1.0e-6);
  opt0.set_xtol_abs(1.0e-6);
  
  opt0.optimize(x, f);
  rc = tnc(npixel, x.data(), &f, g.data(), func_tnc, args, low.data(), up.data(), NULL, NULL, TNC_MSG_ALL,
      maxCGit, maxnfeval, eta, stepmx, accuracy, fmin, ftol, xtol, pgtol,
      rescale, &nfeval, &niter, NULL);
  
  f_old = f;
  num_old = pixon.compute_pixon_number();
  pixon.compute_rm_pixon(x.data());
  chisq_old = pixon.compute_chisquare(x.data());
  memcpy(image, pixon.image, npixel*sizeof(double));
  memcpy(itline, pixon.itline, pixon.line.size*sizeof(double));
  memcpy(x_old.data(), x.data(), npixel*sizeof(double));
  cout<<f_old<<"  "<<num_old<<"  "<<chisq_old<<endl;

  /* then pixel-dependent pixon size */
  iter = 0;
  do
  {
    iter++;
    cout<<"iter:"<<iter<<endl;

    num = pixon.compute_pixon_number();
    
    for(i=0; i<npixel; i++)
    {
      if(x[i] < low[i])
        x[i] = low[i];
      if(x[i] > up[i])
        x[i] = up[i];
    }

    opt0.optimize(x, f);
    rc = tnc(npixel, x.data(), &f, g.data(), func_tnc, args, low.data(), up.data(), NULL, NULL, TNC_MSG_ALL,
      maxCGit, maxnfeval, eta, stepmx, accuracy, fmin, ftol, xtol, pgtol,
      rescale, &nfeval, &niter, NULL);
    
    if(rc <0 || rc > 3)
    {
      opt0.optimize(x, f);
    }
    
    pixon.compute_rm_pixon(x.data());
    chisq = pixon.compute_chisquare(x.data());
    cout<<f<<"  "<<num<<"  "<<chisq<<endl;

    if(f <= pixon.line.size)
    {
      memcpy(image, pixon.image, npixel*sizeof(double));
      memcpy(itline, pixon.itline, pixon.line.size*sizeof(double));
      memcpy(x_old.data(), x.data(), npixel*sizeof(double));
      break;
    }

    df = f-f_old;
    dnum = num - num_old;

    //if(-df < dnum * (1.0 + 1.0/sqrt(2.0*num)))
    //  break;

    flag = pixon.update_pixon_map();

    if(!flag)
      break;

    num_old = num;
    f_old = f;
    chisq_old = chisq;
    memcpy(image, pixon.image, npixel*sizeof(double));
    memcpy(itline, pixon.itline, pixon.line.size*sizeof(double));
    memcpy(x_old.data(), x.data(), npixel*sizeof(double));
  }while(pixon.pfft.get_ipxion_min() >= pixon_map_low_bound); 

  ofstream fout;
  string fname;
  fname = "data/resp.txt_" + to_string(pixon_type);
  fout.open(fname);
  for(i=0; i<npixel; i++)
  {
    fout<<pixon.dt*i<<"  "<<image[i]<<exp(x_old[i])<<"   "<<"  "<<endl;
  }
  fout.close();
  
  fname = "data/line_sim.txt_" + to_string(pixon_type);
  fout.open(fname);
  for(i=0; i<pixon.line.size; i++)
  {
    fout<<pixon.line.time[i]<<"  "<<itline[i]<<"   "<<itline[i] - line.flux[i]<<endl;
  }
  fout.close();

  fname = "data/pixon_map.txt_" + to_string(pixon_type);
  fout.open(fname);
  for(i=0; i<npixel; i++)
  {
    fout<<i*pixon.dt<<"  "<<pixon.pfft.pixon_sizes[pixon.pixon_map[i]]<<endl;
  }
  fout.close();

  delete[] image;
  delete[] itline;

}

/* set continuum fixed from a drw reconstruction and use uniform pixon sizes */
void run_uniform(Data& cont, Data& line, double *pimg, int npixel, int& npixon, int pixon_type)
{
  int i;
  Pixon pixon(cont, line, npixel, npixon);
  void *args = (void *)&pixon;
  double f, f_old, num_old, num, df, dnum, chisq, chisq_old;
  double *image=new double[npixel], *itline=new double[pixon.line.size];
 
  /* TNC */
  int rc, maxCGit = npixel, maxnfeval = 10000, nfeval, niter;
  double eta = -1.0, stepmx = -1.0, accuracy = 1.0e-6, fmin = pixon.line.size, 
    ftol = 1.0e-6, xtol = 1.0e-6, pgtol = 1.0e-6, rescale = -1.0;

  /* NLopt */
  nlopt::opt opt0(nlopt::LN_BOBYQA, npixel);
  vector<double> x(npixel), g(npixel), x_old(npixel);
  vector<double> low(npixel), up(npixel);
  
  /* bounds and initial values */
  for(i=0; i<npixel; i++)
  {
    low[i] = -100.0;
    up[i] =  10.0;
    x[i] = log(1.0/(npixel * pixon.dt));
  }

  /* NLopt settings */
  opt0.set_min_objective(func_nlopt, args);
  opt0.set_lower_bounds(low);
  opt0.set_upper_bounds(up);
  opt0.set_maxeval(10000);
  opt0.set_ftol_abs(1.0e-6);
  opt0.set_xtol_abs(1.0e-6);
   
  opt0.optimize(x, f);
  rc = tnc(npixel, x.data(), &f, g.data(), func_tnc, args, low.data(), up.data(), NULL, NULL, TNC_MSG_ALL,
      maxCGit, maxnfeval, eta, stepmx, accuracy, fmin, ftol, xtol, pgtol,
      rescale, &nfeval, &niter, NULL);
  
  f_old = f;
  num_old = pixon.compute_pixon_number();
  pixon.compute_rm_pixon(x.data());
  chisq_old = pixon.compute_chisquare(x.data());
  memcpy(image, pixon.image, npixel*sizeof(double));
  memcpy(itline, pixon.itline, pixon.line.size*sizeof(double));
  memcpy(x_old.data(), x.data(), npixel*sizeof(double));
  cout<<f_old<<"  "<<num_old<<"  "<<chisq_old<<endl;

  while(npixon>pixon_map_low_bound+1)
  {
    npixon--;
    cout<<"npixon:"<<npixon<<",  size: "<<pixon.pfft.pixon_sizes[npixon-1]<<endl;

    pixon.reduce_pixon_map_all();
    num = pixon.compute_pixon_number();
    
    for(i=0; i<npixel; i++)
    {
      if(x[i] < low[i])
        x[i] = low[i];
      if(x[i] > up[i])
        x[i] = up[i];
    }

    opt0.optimize(x, f);
    rc = tnc(npixel, x.data(), &f, g.data(), func_tnc, args, low.data(), up.data(), NULL, NULL, TNC_MSG_ALL,
      maxCGit, maxnfeval, eta, stepmx, accuracy, fmin, ftol, xtol, pgtol,
      rescale, &nfeval, &niter, NULL);
    
    if(rc <0 || rc > 3)
    {
      opt0.optimize(x, f);
    }
    
    pixon.compute_rm_pixon(x.data());
    chisq = pixon.compute_chisquare(x.data());
    cout<<f<<"  "<<num<<"  "<<chisq<<endl;

    if(f <= pixon.line.size)
    {
      memcpy(image, pixon.image, npixel*sizeof(double));
      memcpy(itline, pixon.itline, pixon.line.size*sizeof(double));
      memcpy(x_old.data(), x.data(), npixel*sizeof(double));
      break;
    }
    
    df = f-f_old;
    dnum = num - num_old;

    if(-df < dnum * (1.0 + 1.0/sqrt(2.0*num)))
      break;

    num_old = num;
    f_old = f;
    chisq_old = chisq;
    memcpy(image, pixon.image, npixel*sizeof(double));
    memcpy(itline, pixon.itline, pixon.line.size*sizeof(double));
    memcpy(x_old.data(), x.data(), npixel*sizeof(double));
  }

  ofstream fout;
  string fname;
  fname = "data/resp_uniform.txt_" + to_string(pixon_type);
  fout.open(fname);
  for(i=0; i<npixel; i++)
  {
    fout<<pixon.dt*i<<"   "<<image[i]<<"  "<<exp(x_old[i])<<endl;
  }
  fout.close();
  
  fname = "data/line_sim_uniform.txt_" + to_string(pixon_type);
  fout.open(fname);
  for(i=0; i<pixon.line.size; i++)
  {
    fout<<pixon.line.time[i]<<"  "<<itline[i]<<"   "<<itline[i] - line.flux[i]<<endl;
  }
  fout.close();
  
  memcpy(pixon.image, image, npixel*sizeof(double));
  memcpy(pixon.itline, itline, pixon.line.size*sizeof(double));

  memcpy(pimg, x_old.data(), npixel*sizeof(double));

  delete[] image;
  delete[] itline;
}