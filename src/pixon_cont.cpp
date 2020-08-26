
#include "utilities.hpp"
#include "pixon_cont.hpp"

PixonCont::PixonCont()
{
  image_cont = pseudo_image_cont = NULL;
  residual_cont = NULL;
  grad_chisq_cont = NULL;
}

PixonCont::PixonCont(
  Data& cont_data_in, Data& cont_in, Data& line_data_in, 
  int npixel_in,  int npixon_in, int npixon_cont_in
  )
  :Pixon(cont_in, line_data_in, npixel_in, npixon_in),
   cont_data(cont_data_in),
   pfft_cont(cont_in.size, npixon_cont_in),
   rmfft_pixon(cont_in.size, dt),
   ipixon_cont(npixon_cont_in-1)
{
  residual_cont = new double[cont_data_in.size];
  image_cont = new double[cont_in.size];
  pseudo_image_cont = new double[cont_in.size];
  grad_chisq_cont = new double[cont_in.size];
  grad_mem_cont = new double[cont_in.size];
}

PixonCont::~PixonCont()
{
  if(npixel > 0)
  {
    delete[] residual_cont;
    delete[] image_cont;
    delete[] pseudo_image_cont;
    delete[] grad_chisq_cont;
    delete[] grad_mem_cont;
  }
}

void PixonCont::compute_cont(const double *x)
{
  int i;
  double t;
  /* convolve with pixons for continuum */
  for(i=0; i<cont.size; i++)
  {
    pseudo_image_cont[i] = x[i];
  }
  pfft_cont.convolve(pseudo_image_cont, ipixon_cont, image_cont);
  
  /* enforce positive image */
  for(i=0; i<cont.size; i++)
  {
    if(image_cont[i] <= 0.0)
      image_cont[i] = EPS;
  }
  /* reset Data cont */
  cont.set_data(image_cont);

  for(i=0; i<cont_data.size; i++)
  {
    t = cont_data.time[i];
    residual_cont[i] = Pixon::interp_cont(t) - cont_data.flux[i];
  }

  return;
}

/* compute rm amd pixon convolutions */
void PixonCont::compute_rm_pixon(const double *x)
{
  int i;
  double t;

  compute_cont(x + npixel);
  rmfft.set_data(image_cont, cont.size);
  Pixon::compute_rm_pixon(x);
}

double PixonCont::compute_chisquare_cont(const double *x)
{
  int i;
  
  chisq_cont = 0;
  for(i=0; i<cont_data.size; i++)
  {
    chisq_cont += (residual_cont[i] * residual_cont[i])/(cont_data.error[i] * cont_data.error[i]);
  }
  
  return chisq_cont;
}

double PixonCont::compute_chisquare(const double *x)
{
  /* calculate chi square */
  chisq = Pixon::compute_chisquare(x) + compute_chisquare_cont(x + npixel);
  
  return chisq;
}

double PixonCont::compute_mem(const double *x)
{
  mem =  Pixon::compute_mem(x) + compute_mem_cont(x + npixel);
  return mem;
}

double PixonCont::compute_mem_cont(const double *x)
{
  double Itot, num, alpha;
  int i;

  Itot = 0.0;
  for(i=0; i<cont.size; i++)
  {
    Itot += image_cont[i];
  }

  num = compute_pixon_number_cont();
  alpha = log(num)/log(cont.size);

  mem_cont = 0.0;
  for(i=0; i<cont.size; i++)
  {
    mem_cont += (image_cont[i]/Itot) * log(image_cont[i]/Itot);
  }
  
  mem_cont *= 2.0*alpha;
  return mem_cont;
}

double PixonCont::compute_pixon_number_cont()
{
  int i;
  double num, psize;
    
  num = 0.0;
  for(i=0; i<cont.size; i++)
  {
    psize = pfft_cont.pixon_sizes[ipixon_cont];
    num += pixon_norm(psize);
  }
  return num;
}

void PixonCont::compute_chisquare_grad(const double *x)
{
  Pixon::compute_chisquare_grad(x);       /* derivative of chisq_line with respect to transfer function */
  compute_chisquare_grad_cont(x+npixel);  /* derivative of chisq_cont with respect to continuum */

  /* derivative of chisq_line with respect to continuum */
  int i, j;
  double psize, grad_in, grad_out, K, t;
  
  rmfft_pixon.set_data(image, npixel);
  psize = pfft_cont.pixon_sizes[ipixon_cont];
  for(i=0; i<cont.size; i++)
  {
    for(j=0; j<cont.size; j++)
    {
      resp_pixon[j] = pixon_function(j, i, psize);
    }
    rmfft_pixon.convolve(resp_pixon, cont.size, conv_pixon);

    grad_out = 0.0;
    for(j=0; j<line.size; j++)
    {
      t = line.time[j];
      grad_in = interp_pixon(t);
      grad_out += grad_in * residual[j]/line.error[j]/line.error[j];
    }
    grad_chisq_cont[i] += grad_out * 2.0; /* chisq = chisq_cont + chisq_line */
  }
}

void PixonCont::compute_chisquare_grad_cont(const double *x)
{
  int i, j, jt, jrange1, jrange2;
  double tj;
  double psize, grad_in, K, jt_real;
  
  /* uniform pixon size */
  psize = pfft_cont.pixon_sizes[ipixon_cont];
  for(i=0; i<cont.size; i++)
  {
    jrange1 = fmin(fmax(0, i - pixon_size_factor * psize), cont_data.size-1);
    jrange2 = fmin(cont_data.size-2, i + pixon_size_factor * psize);

    grad_in = 0.0;
    for(j=jrange1; j<=jrange2; j++)
    {
      tj = cont_data.time[j];
      jt_real = (tj - cont.time[0])/dt;
      jt = (int)jt_real;
      
      if( fabs( jt - i )  <= pixon_size_factor * psize )
      {
        K = pixon_function(i, jt, psize) * (1.0 - (jt_real - jt));
        grad_in += K * residual_cont[j]/cont_data.error[j]/cont_data.error[j];
      }
      
      if( fabs( jt+1 - i )  <= pixon_size_factor * psize )
      {
        K = pixon_function(i, jt+1, psize) * (jt_real - jt);
        grad_in += K * residual_cont[j]/cont_data.error[j]/cont_data.error[j];
      }
    }
    grad_chisq_cont[i] = 2.0 * grad_in;
  }
}

void PixonCont::compute_mem_grad(const double *x)
{
  Pixon::compute_mem_grad(x);
  compute_mem_grad_cont(x+npixel);
}

void PixonCont::compute_mem_grad_cont(const double *x)
{
  double Itot, num, alpha, grad_in, psize, K;
  int i, j, jrange1, jrange2;
  Itot = 0.0;
  for(i=0; i<cont.size; i++)
  {
    Itot += image_cont[i];
  }
  num = compute_pixon_number_cont();
  alpha = log(num)/log(cont.size);
  
  /* uniform pixon size */
  psize = pfft_cont.pixon_sizes[ipixon_cont];
  for(i=0; i<cont.size; i++)
  {       
    jrange1 = fmax(0, i - pixon_size_factor * psize);
    jrange2 = fmin(cont.size-1, i + pixon_size_factor*psize);
    grad_in = 0.0;
    for(j=jrange1; j<=jrange2; j++)
    {
      K = pixon_function(j, i, psize);
      grad_in += (1.0 + log(image_cont[j]/Itot)) * K;
    }
    grad_mem_cont[i] = 2.0 * alpha * pseudo_image_cont[i] * grad_in / Itot;
  }
}


void PixonCont::reduce_ipixon_cont()
{
  int i;
  pfft_cont.reduce_pixon_min();
  ipixon_cont--;
}

/* function for nlopt */
double func_nlopt_cont(const vector<double> &x, vector<double> &grad, void *f_data)
{
  PixonCont *pixon = (PixonCont *)f_data;
  double chisq, mem;

  pixon->compute_cont(x.data());
  if (!grad.empty()) 
  {
    int i;
    
    pixon->compute_chisquare_grad_cont(x.data());
    pixon->compute_mem_grad_cont(x.data());
    
    for(i=0; i<(int)grad.size(); i++)
      grad[i] = pixon->grad_chisq_cont[i] + pixon->grad_mem_cont[i];
  }
  chisq = pixon->compute_chisquare_cont(x.data());
  mem = pixon->compute_mem_cont(x.data());
  return chisq + mem;
}


/* function for tnc */
int func_tnc_cont(double x[], double *f, double g[], void *state)
{
  PixonCont *pixon = (PixonCont *)state;
  int i;
  double chisq, mem;

  pixon->compute_cont(x);
  pixon->compute_chisquare_grad_cont(x);
  pixon->compute_mem_grad_cont(x);
  
  chisq = pixon->compute_chisquare_cont(x);
  mem = pixon->compute_mem_cont(x);

  *f = chisq + mem;

  for(i=0; i<pixon->cont.size; i++)
    g[i] = pixon->grad_chisq_cont[i] + pixon->grad_mem_cont[i];

  return 0;
}

/* function for nlopt */
double func_nlopt_cont_rm(const vector<double> &x, vector<double> &grad, void *f_data)
{
  PixonCont *pixon = (PixonCont *)f_data;
  double chisq, mem;

  pixon->compute_rm_pixon(x.data());
  if (!grad.empty()) 
  {
    int i;
    
    pixon->compute_chisquare_grad(x.data());
    pixon->compute_mem_grad(x.data());
    
    for(i=0; i<pixon->npixel; i++)
      grad[i] = pixon->grad_chisq[i] + pixon->grad_mem[i];

    for(i=pixon->npixel; i<(int)grad.size(); i++)
      grad[i] = pixon->grad_chisq_cont[i-pixon->npixel] + pixon->grad_mem_cont[i-pixon->npixel];
  }
  chisq = pixon->compute_chisquare(x.data());
  mem = pixon->compute_mem(x.data());
  return chisq + mem;
}


/* function for tnc */
int func_tnc_cont_rm(double x[], double *f, double g[], void *state)
{
  PixonCont *pixon = (PixonCont *)state;
  int i;
  double chisq, mem;

  pixon->compute_rm_pixon(x);
  pixon->compute_chisquare_grad(x);
  pixon->compute_mem_grad(x);
  
  chisq = pixon->compute_chisquare(x);
  mem = pixon->compute_mem(x);

  *f = chisq + mem;

  for(i=0; i<pixon->npixel; i++)
    g[i] = pixon->grad_chisq[i] + pixon->grad_mem[i];

  for(i=pixon->npixel; i<pixon->cont.size + pixon->npixel; i++)
    g[i] = pixon->grad_chisq_cont[i - pixon->npixel] + pixon->grad_mem_cont[i - pixon->npixel];

  return 0;
}