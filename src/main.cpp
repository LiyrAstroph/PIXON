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

using namespace std;

int main(int argc, char ** argv)
{
  /* if input pixon type */
  if(argc >= 2)
  {
    config.pixon_type = atoi(argv[1]);
  }
  else
  {
    config.pixon_type = 0;
  }
  cout<<"Pixon type: "<<config.pixon_type<<","<<PixonBasis::pixonbasis_name[config.pixon_type]<<endl;
  
  config.fcon = "data/con.txt";
  config.fline = "data/line.txt";
  
  config.tau_range_low = 0.0;
  config.tau_range_up = 900.0;
  config.dt_rec = 10.0;

  config.fix_bg = false;
  config.bg = 0.0;

  config.tol = 1.0e-6;
  config.nfeval_max = 10000;

  config.pixon_sub_factor = 1;
  config.pixon_size_factor = 1;
  config.pixon_map_low_bound = config.pixon_sub_factor - 1;
  config.npixon_max = 30;

  run(config);

  return 0;
}