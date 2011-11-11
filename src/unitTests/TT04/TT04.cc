#include <cstdio>
#include <iostream>
#include <cassert>
#include <cmath>
#include <string>
#include <vector>

#include "Anatomy.hh"
#include "Reaction.hh"
#include "TT04_bbReaction.hh"
#include "TT04Reaction.hh"
#include "TT04_CellML_Reaction.hh"
#include "TT04Dev_Reaction.hh"
#include "TT06_CellML_Reaction.hh"

using namespace std;


class PeriodicPulse
{
 public:
   PeriodicPulse(double magnitude, double tStart, double tEnd, double tRepeat)
   :magnitude_(magnitude), tStart_(tStart), tEnd_(tEnd), tRepeat_(tRepeat){}
   double operator()(double time)
   {
      time -= (floor(time/tRepeat_) * tRepeat_);
      if (time >= tStart_ && time < tEnd_)
        return magnitude_;
      return 0;
   }
   
 private:
   double magnitude_;
   double tStart_;
   double tEnd_;
   double tRepeat_;
};

Anatomy buildAnatomy(int cellType)
{
   AnatomyCell c;
   c.gid_ = 0;
   c.cellType_ = 100+cellType;
   c.theta_ = 0;
   c.phi_ = 0;
   Anatomy a;
   a.setGridSize(1, 1, 1);
   a.cellArray().push_back(c);
   a.nLocal() = 1;
   a.nRemote() = 0;
   a.dx() = 0.2;
   a.dy() = 0.2;
   a.dz() = 0.2;
   return a;
}

Reaction* factory(const string& name, const Anatomy& anatomy)
{
   if (name == "bb")          return new TT04_bbReaction(anatomy);
   if (name == "mr")          return new TT04Reaction(anatomy);
   if (name == "cellml")      return new TT04_CellML_Reaction(anatomy);
   if (name == "tt04dev")    return new TT04Dev_Reaction(anatomy);
   if (name == "cellml_tt06") return new TT06_CellML_Reaction(anatomy);
   assert(false);
   return 0;
}



int main(int argc, char *argv[])
{
   if (argc < 10)
   {
      cout << "program arguments:" << endl;
      cout << "argv[1] - method name (bb, mr, tt04dev, cellml, cellml_tt06)" << endl;
      cout << "argv[2] - amplitude of stimulus -52.0" << endl;
      cout << "argv[3] - start time of stimulus [ms]  2 ms" << endl; 
      cout << "argv[4] - length of stimulus [ms] 1 ms" << endl;
      cout << "argv[5] - frequency of stimulus [ms] 1000 ms" << endl;
      cout << "argv[6] - simulation time [ms] 1000 ms" << endl;
      cout << "argv[7] - time step [ms] 2e-4 ms"<< endl;
      cout << "argv[8] - print Vm every N time steps   50" << endl;
      cout << "argv[9] - equilibration time t [ms]    0" << endl;
      cout << "argv[10] - cell position [endo=0; mid=1; epi=2]" << endl;
      return 0;
   }

   string method = (argv[1]);
   double stimMagnitude = atof(argv[2]);
   double stimStart =        atof(argv[3]);
   double stimLength =       atof(argv[4]);
   double stimCycleLength =  atof(argv[5]);
   double tEnd =          atof(argv[6]);
   double dt =            atof(argv[7]);
   int printRate =        atoi(argv[8]);
   double equilTime =        atof(argv[9]);
   int cellPosition =     atoi(argv[10]);
   int equilTimeStep = equilTime/dt;

   PeriodicPulse stimFunction(
      stimMagnitude, stimStart, stimStart+stimLength, stimCycleLength);
   
   Anatomy anatomy = buildAnatomy(cellPosition);
   Reaction* cellModel = factory(method, anatomy);
     
   cout << "# method: " << method
	<< "\tstimMagnitude " << stimMagnitude
	<< "\tstimStart " << stimLength
	<< "\tstimLength " << stimLength
	<< "\tstimCycleLength " << stimCycleLength
	<< "\ttend " << tEnd
	<< "\tdt " << dt
	<< "\tcell type " << cellPosition
	<< endl;
                                            
   double time = 0.0;
   unsigned loop = 0;
   unsigned nLocal = anatomy.nLocal();
   double tmp = stimFunction(time);
   vector<double> Vm(nLocal, -86.2);
   vector<double> iStim(nLocal, tmp);
   vector<double> dVmReaction(nLocal, 0);
   
   printf("# Starting the computation time loop\n");
   printf("# time Vm iStim dVmReaction\n");
   if (equilTimeStep == 0)
     printf("%f %le %le %le\n",time,Vm[0],iStim[0],dVmReaction[0]); 
   fflush (stdout);

   while (time < tEnd)
   {
      cellModel->calc(dt, Vm, iStim, dVmReaction);
      for (unsigned ii=0; ii<nLocal; ++ii)
        Vm[ii] += dt*(dVmReaction[ii] - iStim[ii]);
      ++loop;
      time = loop*dt;
      tmp = stimFunction(time);
      iStim.assign(nLocal, tmp);
      
      if (loop%printRate == 0 && loop >= equilTimeStep) 
      {
        printf("%f %le %le %le\n",time-equilTime,Vm[0],iStim[0],dVmReaction[0]); 
        fflush (stdout);
      }
   } // end time loop

   return 0;

}
