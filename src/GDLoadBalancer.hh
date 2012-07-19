#ifndef GDLOADBALANCER_H
#define GDLOADBALANCER_H

#include <vector>
#include <mpi.h> 
using std::vector;

class AnatomyCell;

class GDLoadBalancer {

  private:

  int ntissue_;                  // number of non-zero grid points
  int npex_, npey_, npez_;       // dimensions of process grid
  int tnx_, tny_, tnz_;          // reduced process grid for test runs
  int nx_, ny_, nz_;             // dimensions of data grid
  int npegrid_;                  // size of process grid
  int nnbr_;                     // number of neighboring processors to share data with
  int nloctot_;
  double nlocavg_;
  int gapthresh_;                // number of consecutive zero cells that defines a gap
  vector<vector<int> > penbr_;   // process numbers of all neighbors
  vector<int> thatn_;            // convenience function for neighbor pair indices
  vector<int> haveData_;         // (testing) current process owns data for processor ip
  vector<int> myRankList_;       // (testing) list of processes owned by myRank_
  vector<vector<int> > togive_;  // list of all data exchanges needed for balance
  vector<int> pevol_;            // bounding box volume of each processor's cells
  vector<vector<double> > pecom_;
  vector<vector<int> > pemin_;
  vector<vector<int> > pemax_;
  vector<int> pexind_;
  vector<int> peyind_;
  MPI_Comm comm_;
  int nTasks_, myRank_;

  int distributePlane(vector<AnatomyCell>& cells, int zmin, int zmax, int zvol, int kp);
    
  public:

  GDLoadBalancer(MPI_Comm comm, int npex, int npey, int npez);
  ~GDLoadBalancer();
  void initialDistByVol(vector<AnatomyCell>& cells, int nx, int ny, int nz);
  void xstripDist(vector<int>& xcnt, vector<int>& pexmin, vector<int>& pexmax, bool verbose);
  void redistributeCells(vector<AnatomyCell>& cells);
  void computeReducedProcGrid(int nTasks);
  void setReducedProcGrid(int rnx, int rny, int rnz);
  int ipow(int a, int b);
};

class GapSort
{
 public:
    GapSort(vector<int>& v): v_(v) {};
    vector<int> v_;
    bool operator()(const int i, const int j)
   {
      return v_[i] < v_[j];
   }
};
    
#endif
