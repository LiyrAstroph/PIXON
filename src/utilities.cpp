/*
 *  PIXON
 *  A Pixon-based method for reconstructing velocity-delay map in reverberation mapping.
 * 
 *  Yan-Rong Li, liyanrong@mail.ihep.ac.cn
 * 
 */
#include <iostream>
#include <fstream> 
#include <vector>
#include <iomanip>
#include <cstring>

#include "utilities.hpp"

#define EPS (1.0e-100)

using namespace std;

/*==================================================================*/
/* class PixonFunc */

PixonFunc pixon_function;
PixonNorm pixon_norm;

/* gaussian function, truncated at 3*psize */
double gaussian(double x, double y, double psize)
{
  if(fabs(y-x) <= 3 * psize)
    return gaussian_norm(psize) * exp( -0.5*(y-x)*(y-x)/psize/psize );
  else 
    return 0.0;
}
double gaussian_norm(double psize)
{
  return 1.0/sqrt(2.0*M_PI)/psize/norm_3sigma;
}

/* prarabloid function, truncated at 3*psize */
double parabloid(double x, double y, double psize)
{
  if(fabs(y-x) <= 3 * psize)
    return parabloid_norm(psize) * (1.0 - (y-x)*(y-x)/(3.0*psize * 3.0*psize));
  else 
    return 0.0;
}
double parabloid_norm(double psize)
{
  return 1.0/(psize * 4.0);
}

/*==================================================================*/
/* class Data */
Data::Data()
{
  size = 0;
  time = flux = error = NULL;
}

/* constructor with a size of n */
Data::Data(unsigned int n)
{
  if(n > 0)
  {
    size = n;
    time = new double[size];
    flux = new double[size];
    error = new double[size];
  }
  else
  {
    cout<<"Data size must be positive."<<endl;
    exit(0);
  }
}

Data::~Data()
{
  if(size > 0)
  {
    delete[] time;
    delete[] flux;
    delete[] error;
  }
}

Data::Data(Data& data)
{
  if(size != data.size)
  {
    size = data.size;
    time = new double[size];
    flux = new double[size];
    error = new double[size];
    memcpy(time, data.time, size*sizeof(double));
    memcpy(flux, data.flux, size*sizeof(double));
    memcpy(error, data.error, size*sizeof(double));
  }
  else 
  {
    memcpy(time, data.time, size*sizeof(double));
    memcpy(flux, data.flux, size*sizeof(double));
    memcpy(error, data.error, size*sizeof(double));
  }
}

Data& Data::operator = (Data& data)
{
  if(size != data.size)
  {
    if(size > 0)
    {
      delete[] time;
      delete[] flux;
      delete[] error;
    }
    size = data.size;
    time = new double[size];
    flux = new double[size];
    error = new double[size];
    memcpy(time, data.time, size*sizeof(double));
    memcpy(flux, data.flux, size*sizeof(double));
    memcpy(error, data.error, size*sizeof(double));
  }
  else 
  {
    memcpy(time, data.time, size*sizeof(double));
    memcpy(flux, data.flux, size*sizeof(double));
    memcpy(error, data.error, size*sizeof(double));
  }
  return *this;
}

void Data::load(const string& fname)
{
  ifstream fin;
  string line;
  unsigned int i;
  
  /* first determine number of lines */
  fin.open(fname);
  i = 0;
  while(1)
  {
    getline(fin, line);
    if(fin.good())
    {
      i++;
    }
    else
    {
      break;
    }
  }

  /* delete old data if sizes do not match */
  if(i != size)
  {
    if(size > 0)
    {
      delete[] time;
      delete[] flux;
      delete[] error;
    }
    size = i;
  }
  cout<<"file \""+fname+"\" has "<<size<<" lines."<<endl;

  /* allocate memory */
  time = new double[size];
  flux = new double[size];
  error = new double[size];

  /* now read data */
  fin.clear();  // clear flags
  fin.seekg(0); // go to the beginning
  for(i=0; i<size; i++)
  {
    fin>>time[i]>>flux[i]>>error[i];
    if(fin.fail())
    {
      cout<<"# Error in reading the file \""+fname+"\", no enough points in line "<<i<<"."<<endl;
      exit(0);
    }
  }
  fin.close();
}
/*==================================================================*/
/* class DataFFT */
DataFFT::DataFFT()
{
  nd = npad = nd_fft = nd_fft_cal = 0;
  data_fft = resp_fft = conv_fft = NULL;
  data_real = resp_real = conv_real = NULL;
}

DataFFT::DataFFT(unsigned int nd, double fft_dx, unsigned int npad)
      :nd(nd), npad(npad) 
{
  int i;

  nd_fft = nd + npad;
  nd_fft_cal = nd_fft/2 + 1;

  data_fft = (fftw_complex *) fftw_malloc((nd_fft_cal) * sizeof(fftw_complex));
  resp_fft = (fftw_complex *) fftw_malloc((nd_fft_cal) * sizeof(fftw_complex));
  conv_fft = (fftw_complex *) fftw_malloc((nd_fft_cal) * sizeof(fftw_complex));

  data_real = new double[nd_fft];
  resp_real = new double[nd_fft];
  conv_real = new double[nd_fft];
      
  pdata = fftw_plan_dft_r2c_1d(nd_fft, data_real, data_fft, FFTW_PATIENT);
  presp = fftw_plan_dft_r2c_1d(nd_fft, resp_real, resp_fft, FFTW_PATIENT);
  pback = fftw_plan_dft_c2r_1d(nd_fft, conv_fft, conv_real, FFTW_PATIENT);
      
  /* normalization */
  fft_norm = fft_dx/nd_fft;

  for(i=0; i < nd_fft; i++)
  {
    data_real[i] = resp_real[i] = conv_real[i] = 0.0;
  }
}

DataFFT::DataFFT(Data& cont, unsigned int npad)
      :nd(cont.size), npad(npad)
{
  int i;

  nd_fft = nd + npad;
  nd_fft_cal = nd_fft/2 + 1;

  data_fft = (fftw_complex *) fftw_malloc((nd_fft_cal) * sizeof(fftw_complex));
  resp_fft = (fftw_complex *) fftw_malloc((nd_fft_cal) * sizeof(fftw_complex));
  conv_fft = (fftw_complex *) fftw_malloc((nd_fft_cal) * sizeof(fftw_complex));

  data_real = new double[nd_fft];
  resp_real = new double[nd_fft];
  conv_real = new double[nd_fft];
      
  pdata = fftw_plan_dft_r2c_1d(nd_fft, data_real, data_fft, FFTW_PATIENT);
  presp = fftw_plan_dft_r2c_1d(nd_fft, resp_real, resp_fft, FFTW_PATIENT);
  pback = fftw_plan_dft_c2r_1d(nd_fft, conv_fft, conv_real, FFTW_PATIENT);
      
  fft_norm = (cont.time[1] - cont.time[0]) / nd_fft;

  for(i=0; i < nd_fft; i++)
  {
    data_real[i] = resp_real[i] = conv_real[i] = 0.0;
  }
}

DataFFT& DataFFT::operator = (DataFFT& df)
{
  if(nd != df.nd)
  {
    if(nd > 0)
    {
      fftw_free(data_fft);
      fftw_free(resp_fft);
      fftw_free(conv_fft);

      delete[] data_real;
      delete[] resp_real;
      delete[] conv_real;

      fftw_destroy_plan(pdata);
      fftw_destroy_plan(presp);
      fftw_destroy_plan(pback);
    }
        
    int i;
    nd = df.nd;
    npad = df.npad;
    nd_fft = nd + npad;
    nd_fft_cal = nd_fft/2 + 1;
  
    data_fft = (fftw_complex *) fftw_malloc((nd_fft_cal) * sizeof(fftw_complex));
    resp_fft = (fftw_complex *) fftw_malloc((nd_fft_cal) * sizeof(fftw_complex));
    conv_fft = (fftw_complex *) fftw_malloc((nd_fft_cal) * sizeof(fftw_complex));
  
    data_real = new double[nd_fft];
    resp_real = new double[nd_fft];
    conv_real = new double[nd_fft];
        
    pdata = fftw_plan_dft_r2c_1d(nd_fft, data_real, data_fft, FFTW_PATIENT);
    presp = fftw_plan_dft_r2c_1d(nd_fft, resp_real, resp_fft, FFTW_PATIENT);
    pback = fftw_plan_dft_c2r_1d(nd_fft, conv_fft, conv_real, FFTW_PATIENT);
        
    fft_norm = df.fft_norm;
  
    for(i=0; i < nd_fft; i++)
    {
      data_real[i] = resp_real[i] = conv_real[i] = 0.0;
    }
  }
  return *this;
}

DataFFT::~DataFFT()
{
  if(nd > 0)
  {
    fftw_free(data_fft);
    fftw_free(resp_fft);
    fftw_free(conv_fft);

    delete[] data_real;
    delete[] resp_real;
    delete[] conv_real;

    fftw_destroy_plan(pdata);
    fftw_destroy_plan(presp);
    fftw_destroy_plan(pback);
  }
}

/* convolution with resp, output to conv */
void DataFFT::convolve_simple(double *conv)
{
  int i;
  for(i=0; i<nd_fft_cal; i++)
  {
    conv_fft[i][0] = data_fft[i][0]*resp_fft[i][0] - data_fft[i][1]*resp_fft[i][1];
    conv_fft[i][1] = data_fft[i][0]*resp_fft[i][1] + data_fft[i][1]*resp_fft[i][0];
  }
  fftw_execute(pback);

  /* normalize */
  for(i=0; i<nd_fft; i++)
  {
    conv_real[i] *= fft_norm;
  }

  /* copy back, the first npad points are discarded */
   memcpy(conv, conv_real, nd*sizeof(double));      
   return;
}
/*==================================================================*/
/* class RMFFT */
RMFFT::RMFFT(unsigned int n, double *cont, double norm)
      :DataFFT(n, norm)
{
  /* fft of cont setup only once */
  memcpy(data_real+npad, cont, nd*sizeof(double));
  fftw_execute(pdata);
}
    
RMFFT::RMFFT(Data& cont):DataFFT(cont)
{
  memcpy(data_real, cont.flux, nd*sizeof(double));
  fftw_execute(pdata);
}

/* convolution with resp, output to conv */
void RMFFT::convolve(const double *resp, unsigned int n, double *conv)
{
  /* fft of resp */
  memcpy(resp_real, resp, n * sizeof(double));
  fftw_execute(presp);
  
  DataFFT::convolve_simple(conv);
  return;
}
/*==================================================================*/
/* class PixonFFT */
PixonFFT::PixonFFT()
{
  npixon = ipixon_min = 0;
  pixon_sizes = NULL;
  pixon_sizes_num = NULL;
}
PixonFFT::PixonFFT(unsigned int npixel, unsigned int npixon)
      :npixon(npixon), DataFFT(npixel, 1.0, npixon)
{
  unsigned int i;

  ipixon_min = npixon-1;
  pixon_sizes = new double[npixon];
  pixon_sizes_num = new double[npixon];
  for(i=0; i<npixon; i++)
  {
    pixon_sizes[i] = (i+1)/3.0;
    pixon_sizes_num[i] = 0;
  }
  /* assume that all pixels have the largest pixon size */
  pixon_sizes_num[ipixon_min] = npixel;
}

PixonFFT::~PixonFFT()
{
  if(npixon > 0)
  {
    delete[] pixon_sizes;
    delete[] pixon_sizes_num;
  }
}

void PixonFFT::convolve(const double *pseudo_img, unsigned int *pixon_map, double *conv)
{
  int ip, j;
  double psize;
  double *conv_tmp = new double[nd];

  /* fft of pseudo image */
  memcpy(data_real, pseudo_img, nd*sizeof(double));
  fftw_execute(pdata);

  /* loop over all pixon sizes */
  for(ip=ipixon_min; ip<npixon; ip++)
  {
    if(pixon_sizes_num[ip] > 0)
    {
      psize = pixon_sizes[ip];
      /* setup resp */
      for(j=0; j<nd_fft/2; j++)
      {
        resp_real[j] = pixon_function(j, 0, psize);
      }
      for(j=nd_fft-1; j>=nd_fft/2; j--)
      {
        resp_real[j] = pixon_function(j, nd_fft, psize);
      }
      fftw_execute(presp);
      
      DataFFT::convolve_simple(conv_tmp);
      for(j=0; j<nd; j++)
      {
        if(pixon_map[j] == ip)
          conv[j] = conv_tmp[j];
      }
    }
  }

  delete[] conv_tmp;
}

/* reduce the minimum pixon size */
void PixonFFT::reduce_pixon_min()
{
  if(ipixon_min > 0)
  {
    ipixon_min--;
  }
  else 
  {
    cout<<"reach minimumly allowed pixon sizes!"<<endl;
    exit(0);
  }
}

/* reduce the minimum pixon size */
void PixonFFT::increase_pixon_min()
{
  if(ipixon_min < npixon-1)
  {
    ipixon_min++;
  }
  else 
  {
    cout<<"reach maximumly allowed pixon sizes!"<<endl;
    exit(0);
  }
}

unsigned int PixonFFT::get_ipxion_min()
{
  return ipixon_min;
}
/*==================================================================*/
/* class Pixon */

Pixon::Pixon()
{
  npixel = 0;
  pixon_map = NULL; 
  image = pseudo_image = NULL;
  rmline = NULL;
  itline = NULL;
  residual = NULL;
  grad_chisq = NULL;
  grad_pixon_low = NULL;
  grad_pixon_up = NULL;
  grad_mem = NULL;
  grad_mem_pixon_low = NULL;
}

Pixon::Pixon(Data& cont, Data& line, unsigned int npixel,  unsigned int npixon)
  :cont(cont), line(line), npixel(npixel), rmfft(cont), pfft(npixel, npixon)
{
  pixon_map = new unsigned int[npixel];
  image = new double[npixel];
  pseudo_image = new double[npixel];
  rmline = new double[cont.size];
  itline = new double[line.size];
  residual = new double[line.size];
  grad_pixon_low = new double[npixel];
  grad_pixon_up = new double[npixel];
  grad_mem = new double[npixel];
  grad_chisq = new double[npixel];
  grad_mem_pixon_low = new double[npixel];
  grad_mem_pixon_up = new double[npixel];

  dt = cont.time[1]-cont.time[0];  /* time interval width of continuum light curve */
  unsigned int i;
  for(i=0; i<npixel; i++)
  {
    pixon_map[i] = npixon-1;  /* set the largest pixon size */
  }
}

Pixon::~Pixon()
{
  if(npixel > 0)
  {
    delete[] pixon_map;
    delete[] image;
    delete[] pseudo_image;
    delete[] rmline;
    delete[] itline;
    delete[] residual;
    delete[] grad_chisq;
    delete[] grad_pixon_low;
    delete[] grad_pixon_up;
    delete[] grad_mem;
    delete[] grad_mem_pixon_low;
    delete[] grad_mem_pixon_up;
  }
}

/* linear interplolation  */
double Pixon::interp(double t)
{
  int it;

  it = (t - cont.time[0])/dt;

  if(it < 0)
    return rmline[0];
  else if(it >= cont.size -1)
    return rmline[cont.size -1];

  return rmline[it] + (rmline[it+1] - rmline[it])/dt * (t - cont.time[it]);
}

/* compute rm amd pixon convolutions */
void Pixon::compute_rm_pixon(const double *x)
{
  int i;
  double t, it;
  /* convolve with pixons */
  for(i=0; i<npixel; i++)
  {
    pseudo_image[i] = exp(x[i]);
  }
  pfft.convolve(pseudo_image, pixon_map, image);

  /* reverberation mapping */
  rmfft.convolve(image, npixel, rmline);

  /* interpolation */
  for(i=0; i<line.size; i++)
  {
    t = line.time[i];
    itline[i] = interp(t);
    residual[i] = itline[i] - line.flux[i];
  }
}

/* compute chi square */
double Pixon::compute_chisquare(const double *x)
{
  int i;

  /* calculate chi square */
  chisq = 0.0;
  for(i=0; i<line.size; i++)
  {
    chisq += (residual[i] * residual[i])/(line.error[i] * line.error[i]);
  }
  //printf("%f\n", chisq);
  return chisq;
}

/* compute entropy */
double Pixon::compute_mem(const double *x)
{
  double Itot, num, alpha;
  int i, j;

  Itot = 0.0;
  for(i=0; i<npixel; i++)
  {
    Itot += image[i];
  }
  
  num = compute_pixon_number();
  alpha = log(num)/log(npixel);

  mem = 0.0;
  for(i=0; i<npixel; i++)
  {
    mem += image[i]/Itot * log(image[i]/Itot + EPS);
  }
  
  mem *= 2.0*alpha;

  return mem;
}

/* compute gradient of chi square */
void Pixon::compute_chisquare_grad(const double *x)
{
  int i, k, j, joffset, jrange1, jrange2;
  double psize, t, tau, cont_intp, grad_in, grad_out, K;
  for(i=0; i<npixel; i++)
  {
    grad_out = 0.0;
    psize = pfft.pixon_sizes[pixon_map[i]];
    joffset = 3 * psize;
    jrange1 = fmax(i - joffset, 0);
    jrange2 = fmin(i + joffset, npixel-1);
    
    for(k=0; k<line.size; k++)
    {          
      grad_in = 0.0;
      t = line.time[k];
      for(j=jrange1; j<=jrange2; j++)
      {
        tau = j * dt;
        cont_intp = interp(t-tau);
        K = pixon_function(j, i, psize);
        grad_in += K * cont_intp;
      }
      grad_out += grad_in * residual[k]/line.error[k]/line.error[k];
    }
    grad_chisq[i] = grad_out * 2.0*dt * pseudo_image[i];
  }
}

/* calculate chisqure gradient with respect to pixon size 
 * when pixon size decreases, chisq decreases, 
 * so chisq gradient is positive 
 */
void Pixon::compute_chisquare_grad_pixon_low()
{
  int i, k, j, joffset, jrange1, jrange2, joffset_low;
  double psize, psize_low, t, tau, cont_intp, grad_in, grad_out, K;
  for(i=0; i<npixel; i++)
  {
    grad_out = 0.0;
    psize = pfft.pixon_sizes[pixon_map[i]];
    psize_low = pfft.pixon_sizes[pixon_map[i]-1];
    joffset = 3 * psize;
    joffset_low = 3 * psize_low;
    jrange1 = fmax(fmin(i - joffset, i - joffset_low), 0.0);
    jrange2 = fmin(fmax(i + joffset, i + joffset_low), npixel-1);
    
    for(k=0; k<line.size; k++)
    {          
      grad_in = 0.0;
      t = line.time[k];
      for(j=jrange1; j<=jrange2; j++)
      {
        tau = j * dt;
        cont_intp = interp(t-tau);
        K =  pixon_function(j, i, psize) - pixon_function(j, i, psize_low);
        grad_in += K * cont_intp;
      }
      grad_out += grad_in * residual[k]/line.error[k]/line.error[k];
    }
    grad_pixon_low[i] = grad_out * 2.0*dt * pseudo_image[i];
  }
}

/* calculate chisqure gradient with respect to pixon size 
 * when pixon size increases, chisq increases, 
 * so chisq gradient is positive too
 */
void Pixon::compute_chisquare_grad_pixon_up()
{
  int i, k, j, joffset, jrange1, jrange2, joffset_up;
  double psize, psize_up, t, tau, cont_intp, grad_in, grad_out, K;
  for(i=0; i<npixel; i++)
  {
    grad_out = 0.0;
    psize = pfft.pixon_sizes[pixon_map[i]];
    psize_up = pfft.pixon_sizes[pixon_map[i]+1];
    joffset = 3 * psize;
    joffset_up = 3 * psize_up;
    jrange1 = fmax(fmin(i - joffset, i - joffset_up), 0.0);
    jrange2 = fmin(fmax(i + joffset, i + joffset_up), npixel-1);
    
    for(k=0; k<line.size; k++)
    {          
      grad_in = 0.0;
      t = line.time[k];
      for(j=jrange1; j<jrange2; j++)
      {
        tau = j * dt;
        cont_intp = interp(t-tau);
        K =  pixon_function(j, i, psize_up) - pixon_function(j, i, psize);
        grad_in += K * cont_intp;
      }
      grad_out += grad_in * residual[k]/line.error[k]/line.error[k];
    }
    grad_pixon_up[i] = grad_out * 2.0*dt * pseudo_image[i];
  }
}

void Pixon::compute_mem_grad(const double *x)
{
  double Itot, num, alpha, grad_in, psize, K;
  int i, j, jrange1, jrange2, joffset;
  Itot = 0.0;
  for(i=0; i<npixel; i++)
  {
    Itot += image[i];
  }
  num = compute_pixon_number();
  alpha = log(num)/log(npixel);
  
  for(i=0; i<npixel; i++)
  {       
    grad_in = 0.0;
    psize = pfft.pixon_sizes[pixon_map[i]];
    joffset = 3 * psize;
    jrange1 = fmax(i - joffset, 0.0);
    jrange2 = fmin(i + joffset, npixel-1);
    for(j=jrange1; j<=jrange2; j++)
    {
      K = pixon_function(i, j, psize);
      grad_in += (1.0 + log(image[j]/Itot + EPS)) * K;
    } 
    grad_mem[i] = 2.0* alpha * pseudo_image[i] * grad_in / Itot;
  }
}

void Pixon::compute_mem_grad_pixon_low()
{
  double Itot, num, alpha, grad_in, psize, psize_low, K;
  int i, j, jrange1, jrange2, joffset, joffset_low;
  Itot = 0.0;
  for(i=0; i<npixel; i++)
  {
    Itot += image[i];
  }
  num = compute_pixon_number();
  alpha = log(num)/log(npixel);

  for(i=0; i<npixel; i++)
  {       
    grad_in = 0.0;
    psize = pfft.pixon_sizes[pixon_map[i]];
    psize_low = pfft.pixon_sizes[pixon_map[i]-1];
    joffset = 3 * psize;
    joffset = 3 * psize_low;
    jrange1 = fmax(fmin(i - joffset, i - joffset_low), 0.0);
    jrange2 = fmin(fmax(i + joffset, i + joffset_low), npixel-1);
    for(j=jrange1; j<=jrange2; j++)
    {
      K =  pixon_function(j, i, psize) - pixon_function(j, i, psize_low);
      grad_in += (1.0 + log(image[j]/Itot + EPS)) * K;
    } 
    grad_mem_pixon_low[i] = 2.0* alpha * pseudo_image[i] * grad_in / Itot;
  }
}

void Pixon::compute_mem_grad_pixon_up()
{
  double Itot, num, alpha, grad_in, psize, psize_up, K;
  int i, j, jrange1, jrange2, joffset, joffset_up;
  Itot = 0.0;
  for(i=0; i<npixel; i++)
  {
    Itot += image[i];
  }
  num = compute_pixon_number();
  alpha = log(num)/log(npixel);

  for(i=0; i<npixel; i++)
  {       
    grad_in = 0.0;
    psize = pfft.pixon_sizes[pixon_map[i]];
    psize_up = pfft.pixon_sizes[pixon_map[i]+1];
    joffset = 3 * psize;
    joffset = 3 * psize_up;
    jrange1 = fmax(fmin(i - joffset, i - joffset_up), 0.0);
    jrange2 = fmin(fmax(i + joffset, i + joffset_up), npixel-1);
    for(j=jrange1; j<=jrange2; j++)
    {
      K =  pixon_function(j, i, psize) - pixon_function(j, i, psize_up);
      grad_in += (1.0 + log(image[j]/Itot + EPS)) * K;
    } 
    grad_mem_pixon_up[i] = 2.0* alpha * pseudo_image[i] * grad_in / Itot;
  }
}

double Pixon::compute_pixon_number()
{
  int i;
  double num, psize;
    
  num = 0.0;
  for(i=0; i<pfft.nd; i++)
  {
    psize = pfft.pixon_sizes[pixon_map[i]];
    num += pixon_norm(psize);
  }
  return num;
}

void Pixon::reduce_pixon_map_all()
{
  int i;
  pfft.pixon_sizes_num[pfft.ipixon_min] = 0;
  pfft.reduce_pixon_min();
  pfft.pixon_sizes_num[pfft.ipixon_min] = npixel;
  for(i=0; i<npixel; i++)
  {
    pixon_map[i]--;
  }
}

void Pixon::increase_pixon_map_all()
{
  int i;
  pfft.pixon_sizes_num[pfft.ipixon_min] = 0;
  pfft.increase_pixon_min();
  pfft.pixon_sizes_num[pfft.ipixon_min] = npixel;
  for(i=0; i<npixel; i++)
  {
    pixon_map[i]++;
  }
}

void Pixon::reduce_pixon_map(unsigned int ip)
{
  pfft.pixon_sizes_num[pixon_map[ip]]--;
  pixon_map[ip]--;
  pfft.pixon_sizes_num[pixon_map[ip]]++;
  if(pfft.ipixon_min > pixon_map[ip])
  {
    pfft.ipixon_min = pixon_map[ip];
  }
}

void Pixon::increase_pixon_map(unsigned int ip)
{
  pfft.pixon_sizes_num[pixon_map[ip]]--;
  pixon_map[ip]++;
  pfft.pixon_sizes_num[pixon_map[ip]]++;
}

bool Pixon::update_pixon_map()
{
  int i;
  double psize, psize_low, dnum_low, num;
  bool flag=false;

  cout<<"update pixon map."<<endl;
  compute_chisquare_grad_pixon_low();
  compute_mem_grad_pixon_low();
  for(i=0; i<npixel; i++)
  {
    if(pixon_map[i] > 1)
    {
      psize = pfft.pixon_sizes[pixon_map[i]];
      psize_low = pfft.pixon_sizes[pixon_map[i]-1];
      num = pixon_norm(psize);
      dnum_low = pixon_norm(psize_low) - num;
      if( grad_pixon_low[i] + grad_mem_pixon_low[i] > dnum_low  * (1.0 + 1.0/sqrt(2.0*num)))
      {
        reduce_pixon_map(i);
        cout<<"decrease "<< i <<"-th pixel to "<<pfft.pixon_sizes[pixon_map[i]]<<endl;
        flag=true;
      }
    }
  }
  return flag;
}

bool Pixon::increase_pixon_map()
{
  int i;
  double psize, psize_up, dnum_low, dnum_up, num;
  bool flag=false;

  cout<<"update pixon map."<<endl;
  compute_chisquare_grad_pixon_up();
  compute_mem_grad_pixon_up();
  for(i=0; i<npixel; i++)
  {
    if(pixon_map[i] < pfft.npixon - 1)
    {
      psize = pfft.pixon_sizes[pixon_map[i]];
      psize_up = pfft.pixon_sizes[pixon_map[i]+1];
      num = pixon_norm(psize);
      dnum_up = num - pixon_norm(psize_up);
      if(grad_pixon_up[i] + grad_mem_pixon_up[i] <= dnum_up )
      {
        increase_pixon_map(i);
        cout<<"increase "<< i <<"-th pixel to "<<pfft.pixon_sizes[pixon_map[i]]<<endl;
        flag=true;
      }
    }
  }
  return flag;
}
/*==================================================================*/


double func_nlopt(const vector<double> &x, vector<double> &grad, void *f_data)
{
  Pixon *pixon = (Pixon *)f_data;
  double chisq, mem;

  pixon->compute_rm_pixon(x.data());
  if (!grad.empty()) 
  {
    int i;
    
    pixon->compute_chisquare_grad(x.data());
    pixon->compute_mem_grad(x.data());
    
    for(i=0; i<grad.size(); i++)
      grad[i] = pixon->grad_chisq[i] + pixon->grad_mem[i];
  }
  chisq = pixon->compute_chisquare(x.data());
  mem = pixon->compute_mem(x.data());
  return chisq + mem;
}

int func_tnc(double x[], double *f, double g[], void *state)
{
  Pixon *pixon = (Pixon *)state;
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

  return 0;
}