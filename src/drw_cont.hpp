#ifndef _DRW_CONT_HPP

#define _DRW_CONT_HPP

#include "utilities.hpp"

/* class PixonCont */
class PixonDRW:public Pixon
{
  public:
    PixonDRW();
    PixonDRW(Data& cont_data_in, Data& cont_in, Data& line_data_in, int npixel_in,  
              int npixon_in, int npixon_in_cont, int ipositive_in, 
              double sigmad_in, double taud_in, double syserr_in);
    ~PixonDRW();
    void compute_cont(const double *x);
    void compute_rm_pixon(const double *x);
    double compute_chisquare(const double *x);
    double compute_chisquare_cont(const double *x);
    void compute_chisquare_grad(const double *x);
    double compute_mem(const double *x);
    void compute_mem_grad(const double *x);
    
    void compute_matrix();
    void set_covar_Umat(double sigma, double tau, double alpha);
    void set_covar_Pmat(double sigma, double tau, double alpha, double *PSmat);

    Data cont_data;  /* continuum data */
    PixonUniFFT pfft_cont; /* uniform pixon, for continuum */
    RMFFT rmfft_pixon;
    
    double sigmad, taud, syserr;

    double *grad_chisq_cont;
    
    int nq;
    int size_max;
    double *workspace;
    double *workspace_uv;
    double *Larr_data;
    double *USmat;
    double *PQmat;
    double *Cq;
    double *QLmat;
    double *qhat;
    double *D_data, *W_data, *phi_data;
    double *D_recon, *W_recon, *phi_recon;
};

double func_nlopt_cont_drw(const vector<double> &x, vector<double> &grad, void *f_data);
int func_tnc_cont_drw(double x[], double *f, double g[], void *state);
#endif