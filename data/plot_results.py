#
# a Python script for ploting results generated by PIXON
# by Yan-Rong Li, liyanrong@mail.ihep.ac.cn
# Jul 16, 2021
#

import numpy as np 
import matplotlib.pyplot as plt 
from matplotlib.backends.backend_pdf import PdfPages
import configparser as cfgpars
from os.path import basename

pdf = PdfPages("results.pdf")

#=============================================
# load params
# 
with open("param_input") as f:
  file_content = '[dump]\n' + f.read()
  
config = cfgpars.ConfigParser(delimiters='=', allow_no_value=True)
config.read_string(file_content)

for key in config["dump"].keys():
  print(key, config["dump"][key])
#
#=============================================

plt.rc("text", usetex=True)
plt.rc('font', family='serif', size=15)

cont = np.loadtxt(basename(config["dump"]["fcont"]))
line = np.loadtxt(basename(config["dump"]["fline"]))
offset = np.mean(line[:, 1]) - np.std(line[:, 1]) - (np.mean(cont[:, 1]) - np.std(cont[:, 1]))
tlim1 = np.min((cont[0, 0], line[0, 0]))
tlim2 = np.max((cont[-1,0], line[-1, 0]))
tspan= tlim2-tlim1
#resp_input = np.loadtxt("resp_input.txt")

# three types of runs
fnames = ["_contfix", "_pixon", "_drw"]
postfix = config["dump"]["pixon_basis_type"]
itype = int(config["dump"]["drv_lc_model"])

if itype == 3: # all types run
  itypes = [0, 1, 2]
else:
  itypes = [itype]

ntype = len(itypes)
wd = 0.75/ntype

fig = plt.figure(figsize=(15, 5*ntype))
fig_map = plt.figure(figsize=(10, 5*ntype))
for iw, i in enumerate(itypes):
  fn = fnames[i]
  #===========================================================
  # plot transfer function first
  #===========================================================
  ax1 = fig.add_axes((0.1, 0.95-(iw+1)*wd, 0.5, wd))
  ax1.errorbar(cont[:, 0], cont[:, 1], yerr = cont[:, 2], ls='none', marker='None', markersize=3, zorder=0)
  ax1.errorbar(line[:, 0], line[:, 1]-0.7*offset, yerr = line[:, 2], ls='none', marker='None', markersize=3, zorder=0)
  if i==0:
    cont_rec = np.loadtxt("cont_recon_drw.txt")
    cont_rec_uniform = cont_rec
    line_rec = np.loadtxt("line_contfix_full.txt_"+postfix)
    line_rec_uniform = np.loadtxt("line_contfix_uniform_full.txt_"+postfix)
    pixon_map = np.loadtxt("pixon_map_contfix.txt_"+postfix)
    pixon_map_uniform = np.loadtxt("pixon_map_contfix_uniform.txt_"+postfix)
   
  elif i==1:
    cont_rec = np.loadtxt("cont_pixon.txt_"+postfix)
    cont_rec_uniform = np.loadtxt("cont_pixon_uniform.txt_"+postfix)
    line_rec = np.loadtxt("line_pixon_full.txt_"+postfix)
    line_rec_uniform = np.loadtxt("line_pixon_uniform_full.txt_"+postfix)
    pixon_map = np.loadtxt("pixon_map_pixon.txt_"+postfix)
    pixon_map_uniform = np.loadtxt("pixon_map_pixon_uniform.txt_"+postfix)
   
  else:
    cont_rec = np.loadtxt("cont_drw.txt_"+postfix)
    cont_rec_uniform = np.loadtxt("cont_drw_uniform.txt_"+postfix)
    line_rec = np.loadtxt("line_drw_full.txt_"+postfix)
    line_rec_uniform = np.loadtxt("line_drw_uniform_full.txt_"+postfix)
    pixon_map = np.loadtxt("pixon_map_drw.txt_"+postfix)
    pixon_map_uniform = np.loadtxt("pixon_map_drw_uniform.txt_"+postfix)
  
  ax1.plot(cont_rec[:, 0], cont_rec[:, 1], lw=1, color='r',)
  ax1.plot(cont_rec_uniform[:, 0], cont_rec_uniform[:, 1],  lw=1, color='b')
  
  idx = np.where( (line_rec[:, 0] > line[0, 0] - 10) & (line_rec[:, 0] < line[-1, 0] + 10))
  ax1.plot(line_rec[idx[0], 0], line_rec[idx[0], 1]-0.7*offset, lw=1, color='r')
  ax1.plot(line_rec_uniform[idx[0], 0], line_rec_uniform[idx[0], 1]-0.7*offset,  lw=1, color='b')
  
  ax1.set_ylabel("Flux")
  ax1.set_xlim(tlim1-0.01*tspan, tlim2+0.01*tspan)
  ax1.minorticks_on()
   
  ax2 = fig.add_axes((0.64, 0.95-(iw+1)*wd, 0.3, wd))
  resp = np.loadtxt("resp"+fn+".txt_"+postfix)
  resp_uniform = np.loadtxt("resp"+fn+"_uniform.txt_"+postfix)
  #ax.plot(resp_input[:, 0], resp_input[:, 1], lw=2, color='k', label='Truth')
  ax2.plot(resp[:, 0], resp[:, 1], lw=1, label=fn[1:]+' pixel', color='r')
  ax2.plot(resp_uniform[:, 0], resp_uniform[:, 1], lw=1, label=fn[1:]+' uniform', color='b')
  ax2.legend(frameon=False, ncol=2)
  ax2.minorticks_on()

  #===========================================================
  # plot pixon map then
  #===========================================================
  ax_map = fig_map.add_axes((0.1, 0.95-(iw+1)*wd, 0.8, wd))
  ax_map.plot(pixon_map[:, 0], pixon_map[:, 1], lw=1, label=fn[1:]+' pixel', color='r')
  ax_map.plot(pixon_map_uniform[:, 0], pixon_map_uniform[:, 1], lw=1, label=fn[1:]+' uniform', color='b')
  ax_map.set_ylabel("Pixon Size")
  ax_map.legend(frameon=False, ncol=2)

ax1.set_xlabel("Time")
ax2.set_xlabel("Time Lag")
ax_map.set_xlabel("Time Lag")

fname = basename(config["dump"]["fline"])
fname = fname.replace(".txt", ".pdf")
pdf.savefig(fig)
pdf.savefig(fig_map)

pdf.close()