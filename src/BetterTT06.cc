/**

   How to convert this code to work for any other model:

   - Search/Replace the model name with your own specific string in the header and source files
   - Add your own code to EDIT_FLAGS and EDIT_PARAMETERS
   - Add your own code to EDIT_PERCELL_FLAGS and EDIT_PERCELL_PARAMETERS
   - Add your own states to EDIT_STATE
   - Add your computation code to the main calc routine, copy pasting frmo matlab.
   
 */


#include "BetterTT06.hh"
#include "object_cc.hh"
#include "mpiUtils.h"
#include <cmath>
#include <cassert>
#include <fstream>
#include <iostream>

using namespace std;

namespace scanReaction 
{


   
   Reaction* scanBetterTT06(OBJECT* obj, const int numPoints, const double _dt)
   {
      BetterTT06::ThisReaction* reaction = new BetterTT06::ThisReaction(numPoints, _dt);

      //override the defaults
#define setDefault(name, value) objectGet(obj, #name, reaction->name, #value)
      //EDIT_PARAMETERS
#undef setDefault

      bool reusingInterpolants = false;
      string fitName;
      objectGet(obj, "fit", fitName, "");
      int funcCount = sizeof(reaction->_interpolant)/sizeof(reaction->_interpolant[0]);
      if (fitName != "")
      {
         OBJECT* fitObj = objectFind(fitName, "FIT");
         double _fit_dt; objectGet(fitObj, "dt", _fit_dt, "nan");
         if (1
            && _fit_dt == _dt
         )
         {
            vector<string> functions;
            objectGet(fitObj, "functions", functions);
            OBJECT* funcObj;
            if (functions.size() == funcCount)
            {
               for (int _ii=0; _ii<functions.size(); _ii++)
               {
                  OBJECT* funcObj = objectFind(functions[_ii], "FUNCTION");
                  objectGet(funcObj, "numer", reaction->_interpolant[_ii].numNumer_, "-1");
                  objectGet(funcObj, "denom", reaction->_interpolant[_ii].numDenom_, "-1");
                  objectGet(funcObj, "coeff", reaction->_interpolant[_ii].coeff_);
               }
               reusingInterpolants = true;
            }
         }
      }

      if (!reusingInterpolants)
      {
         reaction->createInterpolants(_dt);

         //save the interpolants
         if (funcCount > 0 && getRank(0) == 0)
         {
            ofstream outfile((string(obj->name) +".fit.data").c_str());
            outfile.precision(16);
            fitName = string(obj->name) + "_fit";
            outfile << obj->name << " REACTION { fit=" << fitName << "; }\n";
            outfile << fitName << " FIT {\n";
            outfile << "   dt = " << _dt << ";\n";
            outfile << "   functions = ";
            for (int _ii=0; _ii<funcCount; _ii++) {
               outfile << obj->name << "_interpFunc" << _ii << " ";
            }
            outfile << ";\n";
            outfile << "}\n";

            for (int _ii=0; _ii<funcCount; _ii++)
            {
               outfile << obj->name << "_interpFunc" << _ii << " FUNCTION { "
                       << "numer=" << reaction->_interpolant[_ii].numNumer_ << "; "
                       << "denom=" << reaction->_interpolant[_ii].numDenom_ << "; "
                       << "coeff=";
               for (int _jj=0; _jj<reaction->_interpolant[_ii].coeff_.size(); _jj++)
               {
                  outfile << reaction->_interpolant[_ii].coeff_[_jj] << " ";
               }
               outfile << "; }\n";
            }
            outfile.close();
         }
      }

      reaction->constructKernel();
      return reaction;
   }

}

namespace BetterTT06 
{
void generateInterpString(stringstream& ss, const Interpolation& interp, const char* interpVar)
{
   ss <<
   "{\n"
   "   const double _numerCoeff[]={";
    for (int _ii=interp.numNumer_-1; _ii>=0; _ii--)
   {
      if (_ii != interp.numNumer_-1) { ss << ", "; }
      ss << interp.coeff_[_ii];
   }
   ss<< "};\n"
   "   const double _denomCoeff[]={";
    for (int _ii=interp.numDenom_+interp.numNumer_-2; _ii>=interp.numNumer_; _ii--)
   {
      ss << interp.coeff_[_ii] << ", ";
   }   
   ss<< "1};\n"
   "   double _inVal = " << interpVar << ";\n"
   "   double _numerator=_numerCoeff[0];\n"
   "   for (int _jj=1; _jj<sizeof(_numerCoeff)/sizeof(_numerCoeff[0]); _jj++)\n"
   "   {\n"
   "      _numerator = _numerCoeff[_jj] + _inVal*_numerator;\n"
   "   }\n"
   "   if (sizeof(_denomCoeff)/sizeof(_denomCoeff[0]) == 1)\n"
   "   {\n"
   "      _ratPoly = _numerator;\n"
   "   }\n"
   "   else\n"
   "   {\n"
   "      double _denominator=_denomCoeff[0];\n"
   "      for (int _jj=1; _jj<sizeof(_denomCoeff)/sizeof(_denomCoeff[0]); _jj++)\n"
   "      {\n"
   "         _denominator = _denomCoeff[_jj] + _inVal*_denominator;\n"
   "      }\n"
   "      _ratPoly = _numerator/_denominator;\n"
   "   }\n"
   "}"
      ;
}

void ThisReaction::constructKernel()
{

   stringstream ss;
   ss.precision(16);
   ss << 
   "enum StateOffset {\n"

   "   Ca_SR_off,\n"
   "   Ca_i_off,\n"
   "   Ca_ss_off,\n"
   "   K_i_off,\n"
   "   Na_i_off,\n"
   "   R_prime_off,\n"
   "   Xr1_off,\n"
   "   Xr2_off,\n"
   "   Xs_off,\n"
   "   d_off,\n"
   "   f_off,\n"
   "   f2_off,\n"
   "   fCass_off,\n"
   "   h_off,\n"
   "   j_off,\n"
   "   m_off,\n"
   "   r_off,\n"
   "   s_off,\n"
   "   NUMSTATES\n"
   "};\n"
   "extern \"C\"\n"
   "__global__ void BetterTT06_kernel(const double* _Vm, const double* _iStim, double* _dVm, double* _state) {\n"
   "const double _dt = " << __cachedDt << ";\n"
   "const int _nCells = " << nCells_ << ";\n"

   "const int _ii = threadIdx.x + blockIdx.x*blockDim.x;\n"
   "if (_ii >= _nCells) { return; }\n"
   "const double V = _Vm[_ii];\n"
   "double _ratPoly;\n"

   "const double Ca_SR = _state[_ii+Ca_SR_off*_nCells];\n"
   "const double Ca_i = _state[_ii+Ca_i_off*_nCells];\n"
   "const double Ca_ss = _state[_ii+Ca_ss_off*_nCells];\n"
   "const double K_i = _state[_ii+K_i_off*_nCells];\n"
   "const double Na_i = _state[_ii+Na_i_off*_nCells];\n"
   "const double R_prime = _state[_ii+R_prime_off*_nCells];\n"
   "const double Xr1 = _state[_ii+Xr1_off*_nCells];\n"
   "const double Xr2 = _state[_ii+Xr2_off*_nCells];\n"
   "const double Xs = _state[_ii+Xs_off*_nCells];\n"
   "const double d = _state[_ii+d_off*_nCells];\n"
   "const double f = _state[_ii+f_off*_nCells];\n"
   "const double f2 = _state[_ii+f2_off*_nCells];\n"
   "const double fCass = _state[_ii+fCass_off*_nCells];\n"
   "const double h = _state[_ii+h_off*_nCells];\n"
   "const double j = _state[_ii+j_off*_nCells];\n"
   "const double m = _state[_ii+m_off*_nCells];\n"
   "const double r = _state[_ii+r_off*_nCells];\n"
   "const double s = _state[_ii+s_off*_nCells];\n"
   "double celltype = 0;\n"
   "double Cm = 0.185000000000000;\n"
   "double F = 96485.3415000000;\n"
   "double R = 8314.47200000000;\n"
   "double T = 310;\n"
   "double V_c = 0.0164040000000000;\n"
   "double factor_fix = 1;\n"
   "double Ca_o = 2;\n"
   "double Buf_c = 0.200000000000000;\n"
   "double Buf_sr = 10;\n"
   "double Buf_ss = 0.400000000000000;\n"
   "double EC = 1.50000000000000;\n"
   "double K_buf_c = 0.00100000000000000;\n"
   "double K_buf_sr = 0.300000000000000;\n"
   "double K_buf_ss = 0.000250000000000000;\n"
   "double K_up = 0.000250000000000000;\n"
   "double V_leak = 0.000360000000000000;\n"
   "double V_rel = 0.102000000000000;\n"
   "double V_sr = 0.00109400000000000;\n"
   "double V_ss = 5.46800000000000e-5;\n"
   "double V_xfer = 0.00380000000000000;\n"
   "double Vmax_up = 0.00637500000000000;\n"
   "double k1_prime = 0.150000000000000;\n"
   "double k2_prime = 0.0450000000000000;\n"
   "double k3 = 0.0600000000000000;\n"
   "double k4 = 0.00500000000000000;\n"
   "double max_sr = 2.50000000000000;\n"
   "double min_sr = 1;\n"
   "double g_CaL = 3.98000000000000e-5;\n"
   ""; generateInterpString(ss,_interpolant[26], "V"); ss << "\n"
   "double i_CalTerm3 = _ratPoly;\n"
   ""; generateInterpString(ss,_interpolant[27], "V"); ss << "\n"
   "double i_CalTerm4 = _ratPoly;\n"
   "double g_bca = 0.000592000000000000;\n"
   "double _expensive_functions_012 = log(Ca_o/Ca_i);\n"
   "double E_Ca = 0.5*R*T*_expensive_functions_012/F;\n"
   "double i_b_Ca = g_bca*(V - E_Ca);\n"
   "double i_Caitot = i_b_Ca;\n"
   "double K_pCa = 0.000500000000000000;\n"
   "double g_pCa = 0.123800000000000;\n"
   "double i_p_Ca = Ca_i*g_pCa/(Ca_i + K_pCa);\n"
   "double calcium_pump_current_i_Caitot = i_p_Ca;\n"
   "double K_NaCa = 1000;\n"
   "double K_sat = 0.100000000000000;\n"
   "double Km_Ca = 1.38000000000000;\n"
   "double Km_Nai = 87.5000000000000;\n"
   "double alpha = 2.50000000000000;\n"
   ""; generateInterpString(ss,_interpolant[24], "V"); ss << "\n"
   "double exp_gamma_VFRT = _ratPoly;\n"
   ""; generateInterpString(ss,_interpolant[25], "V"); ss << "\n"
   "double exp_gamma_m1_VFRT = _ratPoly;\n"
   "double g_Na = 14.8380000000000;\n"
   "double K_o = 5.40000000000000;\n"
   "double g_pK = 0.0146000000000000;\n"
   ""; generateInterpString(ss,_interpolant[29], "V"); ss << "\n"
   "double i_p_K_term = _ratPoly;\n"
   "double g_Kr = 0.153000000000000;\n"
   "double _expensive_functions_026 = log(K_o/K_i);\n"
   "double E_K = R*T*_expensive_functions_026/F;\n"
   "double P_kna = 0.0300000000000000;\n"
   "double __melodee_temp_002 = celltype == 1;\n"
   "double g_Ks;\n"
   "if (__melodee_temp_002)\n"
   "{\n"
   "   g_Ks = 0.0980000000000000;\n"
   "}\n"
   "else\n"
   "{\n"
   "   g_Ks = 0.392000000000000;\n"
   "}\n"
   "double g_bna = 0.000290000000000000;\n"
   "double Na_o = 140;\n"
   "double K_mNa = 40;\n"
   ""; generateInterpString(ss,_interpolant[28], "V"); ss << "\n"
   "double i_NaK_term = _ratPoly;\n"
   "double i_NaK = Na_i*i_NaK_term/(Na_i + K_mNa);\n"
   "double i_Naitot = 3*i_NaK;\n"
   "double i_Kitot = -2*i_NaK;\n"
   "double __melodee_temp_004 = celltype == 0;\n"
   "double g_to;\n"
   "if (__melodee_temp_004)\n"
   "{\n"
   "   g_to = 0.0730000000000000;\n"
   "}\n"
   "else\n"
   "{\n"
   "   g_to = 0.294000000000000;\n"
   "}\n"
   "double VEK = -E_K + V;\n"
   "double Ca_i_bufc = (1.0/(Buf_c*K_buf_c/((Ca_i + K_buf_c)*(Ca_i + K_buf_c)) + 1));\n"
   "double Ca_sr_bufsr = (1.0/(Buf_sr*K_buf_sr/((Ca_SR + K_buf_sr)*(Ca_SR + K_buf_sr)) + 1));\n"
   "double Ca_ss_bufss = (1.0/(Buf_ss*K_buf_ss/((Ca_ss + K_buf_ss)*(Ca_ss + K_buf_ss)) + 1));\n"
   "double i_leak = V_leak*(Ca_SR - Ca_i);\n"
   "double i_up = Vmax_up/(1 + (K_up*K_up)/(Ca_i*Ca_i));\n"
   "double i_xfer = V_xfer*(-Ca_i + Ca_ss);\n"
   "double kcasr = max_sr - (max_sr - min_sr)/(1 + (EC*EC)/(Ca_SR*Ca_SR));\n"
   "double k1 = k1_prime/kcasr;\n"
   "double k2 = k2_prime*kcasr;\n"
   "double O = (Ca_ss*Ca_ss)*R_prime*k1/((Ca_ss*Ca_ss)*k1 + k3);\n"
   "double R_prime_diff = -Ca_ss*R_prime*k2 + k4*(-R_prime + 1);\n"
   "double i_rel = O*V_rel*(Ca_SR - Ca_ss);\n"
   "double Ca_SR_diff = Ca_sr_bufsr*(-i_leak - i_rel + i_up);\n"
   "double i_CaL = d*f*f2*fCass*g_CaL*(-Ca_o*i_CalTerm3 + 0.25*Ca_ss*i_CalTerm4);\n"
   "double L_type_Ca_current_i_Caitot = i_CaL;\n"
   "double i_NaCa = K_NaCa*(Ca_o + Km_Ca)*(K_sat*exp_gamma_m1_VFRT + 1)*((Na_i*Na_i*Na_i)*Ca_o*exp_gamma_VFRT - (Na_o*Na_o*Na_o)*Ca_i*alpha*exp_gamma_m1_VFRT)/((Na_o*Na_o*Na_o) + (Km_Nai*Km_Nai*Km_Nai));\n"
   "double sodium_calcium_exchanger_current_i_Naitot = 3*i_NaCa;\n"
   "double sodium_calcium_exchanger_current_i_Caitot = -2*i_NaCa;\n"
   ""; generateInterpString(ss,_interpolant[30], "VEK"); ss << "\n"
   "double inward_rectifier_potassium_current_i_Kitot = _ratPoly;\n"
   "double i_p_K = VEK*g_pK*i_p_K_term;\n"
   "double potassium_pump_current_i_Kitot = i_p_K;\n"
   "double _expensive_functions_040 = sqrt(K_o);\n"
   "double i_Kr = 0.430331482911935*VEK*_expensive_functions_040*Xr1*Xr2*g_Kr;\n"
   "double rapid_time_dependent_potassium_current_i_Kitot = i_Kr;\n"
   "double _expensive_functions_041 = log(Na_o/Na_i);\n"
   "double E_Na = R*T*_expensive_functions_041/F;\n"
   "double _expensive_functions_042 = log((K_o + Na_o*P_kna)/(K_i + Na_i*P_kna));\n"
   "double E_Ks = R*T*_expensive_functions_042/F;\n"
   "double i_Ks = (Xs*Xs)*g_Ks*(V - E_Ks);\n"
   "double slow_time_dependent_potassium_current_i_Kitot = i_Ks;\n"
   "double i_b_Na = g_bna*(-E_Na + V);\n"
   "double sodium_background_current_i_Naitot = i_b_Na;\n"
   "double i_to = VEK*g_to*r*s;\n"
   "double transient_outward_current_i_Kitot = i_to;\n"
   "double i_Kitot_001 = i_Kitot + inward_rectifier_potassium_current_i_Kitot + potassium_pump_current_i_Kitot + rapid_time_dependent_potassium_current_i_Kitot + slow_time_dependent_potassium_current_i_Kitot + transient_outward_current_i_Kitot;\n"
   "double i_Caitot_001 = L_type_Ca_current_i_Caitot + calcium_pump_current_i_Caitot + i_Caitot + sodium_calcium_exchanger_current_i_Caitot;\n"
   "double Ca_i_diff = Ca_i_bufc*(-1.0L/2.0L*Cm*(-2*i_NaCa + i_b_Ca + i_p_Ca)/(F*V_c) + i_xfer + V_sr*(i_leak - i_up)/V_c);\n"
   "double Ca_ss_diff = Ca_ss_bufss*(-1.0L/2.0L*Cm*i_CaL/(F*V_ss) - V_c*i_xfer/V_ss + V_sr*i_rel/V_ss);\n"
   "double i_Na = (m*m*m)*g_Na*h*j*(-E_Na + V);\n"
   "double fast_sodium_current_i_Naitot = i_Na;\n"
   "double K_i_diff = -Cm*factor_fix*i_Kitot_001/(F*V_c);\n"
   "double i_Naitot_001 = fast_sodium_current_i_Naitot + i_Naitot + sodium_background_current_i_Naitot + sodium_calcium_exchanger_current_i_Naitot;\n"
   "double Iion = i_Caitot_001 + i_Kitot_001 + i_Naitot_001;\n"
   "double Na_i_diff = -Cm*factor_fix*i_Naitot_001/(F*V_c);\n"
   "double Iion_001 = Iion;\n"
   ""; generateInterpString(ss,_interpolant[2], "V"); ss << "\n"
   "double _Xr1_RLA = _ratPoly;\n"
   ""; generateInterpString(ss,_interpolant[3], "V"); ss << "\n"
   "double _Xr1_RLB = _ratPoly;\n"
   ""; generateInterpString(ss,_interpolant[4], "V"); ss << "\n"
   "double _Xr2_RLA = _ratPoly;\n"
   ""; generateInterpString(ss,_interpolant[5], "V"); ss << "\n"
   "double _Xr2_RLB = _ratPoly;\n"
   ""; generateInterpString(ss,_interpolant[6], "V"); ss << "\n"
   "double _Xs_RLA = _ratPoly;\n"
   ""; generateInterpString(ss,_interpolant[7], "V"); ss << "\n"
   "double _Xs_RLB = _ratPoly;\n"
   ""; generateInterpString(ss,_interpolant[8], "V"); ss << "\n"
   "double _d_RLA = _ratPoly;\n"
   ""; generateInterpString(ss,_interpolant[9], "V"); ss << "\n"
   "double _d_RLB = _ratPoly;\n"
   ""; generateInterpString(ss,_interpolant[12], "V"); ss << "\n"
   "double _f_RLA = _ratPoly;\n"
   ""; generateInterpString(ss,_interpolant[13], "V"); ss << "\n"
   "double _f_RLB = _ratPoly;\n"
   ""; generateInterpString(ss,_interpolant[10], "V"); ss << "\n"
   "double _f2_RLA = _ratPoly;\n"
   ""; generateInterpString(ss,_interpolant[11], "V"); ss << "\n"
   "double _f2_RLB = _ratPoly;\n"
   ""; generateInterpString(ss,_interpolant[0], "Ca_ss"); ss << "\n"
   "double _fCass_RLA = _ratPoly;\n"
   ""; generateInterpString(ss,_interpolant[1], "Ca_ss"); ss << "\n"
   "double _fCass_RLB = _ratPoly;\n"
   ""; generateInterpString(ss,_interpolant[14], "V"); ss << "\n"
   "double _h_RLA = _ratPoly;\n"
   ""; generateInterpString(ss,_interpolant[15], "V"); ss << "\n"
   "double _h_RLB = _ratPoly;\n"
   ""; generateInterpString(ss,_interpolant[16], "V"); ss << "\n"
   "double _j_RLA = _ratPoly;\n"
   ""; generateInterpString(ss,_interpolant[17], "V"); ss << "\n"
   "double _j_RLB = _ratPoly;\n"
   ""; generateInterpString(ss,_interpolant[18], "V"); ss << "\n"
   "double _m_RLA = _ratPoly;\n"
   ""; generateInterpString(ss,_interpolant[19], "V"); ss << "\n"
   "double _m_RLB = _ratPoly;\n"
   ""; generateInterpString(ss,_interpolant[20], "V"); ss << "\n"
   "double _r_RLA = _ratPoly;\n"
   ""; generateInterpString(ss,_interpolant[21], "V"); ss << "\n"
   "double _r_RLB = _ratPoly;\n"
   ""; generateInterpString(ss,_interpolant[22], "V"); ss << "\n"
   "double _s_RLA = _ratPoly;\n"
   ""; generateInterpString(ss,_interpolant[23], "V"); ss << "\n"
   "double _s_RLB = _ratPoly;\n"
   "\n\n//EDIT STATE\n"
   "_state[_ii+Ca_SR_off*_nCells] += _dt*Ca_SR_diff;\n"
   "_state[_ii+Ca_i_off*_nCells] += _dt*Ca_i_diff;\n"
   "_state[_ii+Ca_ss_off*_nCells] += _dt*Ca_ss_diff;\n"
   "_state[_ii+K_i_off*_nCells] += _dt*K_i_diff;\n"
   "_state[_ii+Na_i_off*_nCells] += _dt*Na_i_diff;\n"
   "_state[_ii+R_prime_off*_nCells] += _dt*R_prime_diff;\n"
   "_state[_ii+Xr1_off*_nCells] = _Xr1_RLA*Xr1 + _Xr1_RLB;\n"
   "_state[_ii+Xr2_off*_nCells] = _Xr2_RLA*Xr2 + _Xr2_RLB;\n"
   "_state[_ii+Xs_off*_nCells] = _Xs_RLA*Xs + _Xs_RLB;\n"
   "_state[_ii+d_off*_nCells] = _d_RLA*d + _d_RLB;\n"
   "_state[_ii+f_off*_nCells] = _f_RLA*f + _f_RLB;\n"
   "_state[_ii+f2_off*_nCells] = _f2_RLA*f2 + _f2_RLB;\n"
   "_state[_ii+fCass_off*_nCells] = _fCass_RLA*fCass + _fCass_RLB;\n"
   "_state[_ii+h_off*_nCells] = _h_RLA*h + _h_RLB;\n"
   "_state[_ii+j_off*_nCells] = _j_RLA*j + _j_RLB;\n"
   "_state[_ii+m_off*_nCells] = _m_RLA*m + _m_RLB;\n"
   "_state[_ii+r_off*_nCells] = _r_RLA*r + _r_RLB;\n"
   "_state[_ii+s_off*_nCells] = _s_RLA*s + _s_RLB;\n"
   "_dVm[_ii] = -Iion_001;\n"
   "}\n";

   _program_code = ss.str();
   //cout << ss.str();
   nvrtcCreateProgram(&_program,
                      _program_code.c_str(),
                      "BetterTT06_program",
                      0,
                      NULL,
                      NULL);
   nvrtcCompileProgram(_program,
                       0,
                       NULL);
   std::size_t size;
   nvrtcGetPTXSize(_program, &size);
   _ptx.resize(size);
   nvrtcGetPTX(_program, &_ptx[0]);

   cuModuleLoadDataEx(&_module, &_ptx[0], 0, 0, 0);
   cuModuleGetFunction(&_kernel, _module, "BetterTT06_kernel");
}

string ThisReaction::methodName() const
{
   return "BetterTT06";
}

enum StateOffset {
   _Ca_SR_off,
   _Ca_i_off,
   _Ca_ss_off,
   _K_i_off,
   _Na_i_off,
   _R_prime_off,
   _Xr1_off,
   _Xr2_off,
   _Xs_off,
   _d_off,
   _f_off,
   _f2_off,
   _fCass_off,
   _h_off,
   _j_off,
   _m_off,
   _r_off,
   _s_off,
   NUMSTATES
};
   
ThisReaction::ThisReaction(const int numPoints, const double __dt)
: nCells_(numPoints)
{
   stateTransport_.setup(std::vector<double>(nCells_*NUMSTATES));
   __cachedDt = __dt;
   blockSize_ = -1;
   _program = NULL;
   _module = NULL;
}

ThisReaction::~ThisReaction() {
   if (_program)
   {
      nvrtcDestroyProgram(&_program);
      _program = NULL;
   }
   if (_module)
   {
      cuModuleUnload(_module);
      _module = NULL;
   }
}
   
void ThisReaction::createInterpolants(const double _dt) {

   {
      int _numPoints = (1e-1 - 1e-7)/1e-6;
      vector<double> _inputs(_numPoints);
      vector<double> _outputs(_numPoints);
      for (int _ii=0; _ii<_numPoints; _ii++)
      {
         double Ca_ss = 1e-7 + (1e-1 - 1e-7)*(_ii+0.5)/_numPoints;
         _inputs[_ii] = Ca_ss;
         double tau_fCass = 2 + 80/(400.0*(Ca_ss*Ca_ss) + 1);
         double _fCass_RLA = exp(-_dt/tau_fCass);
         _outputs[_ii] = _fCass_RLA;
      }
      double relError = 1e-4;
      double actualTolerance = _interpolant[0].create(_inputs,_outputs, relError);
      if (actualTolerance > relError  && getRank(0) == 0)
      {
         cerr << "Warning: Could not meet tolerance for _fCass_RLA: " 
              << actualTolerance << " > " << relError
              << " target" << endl;
      }
   }
   {
      int _numPoints = (1e-1 - 1e-7)/1e-6;
      vector<double> _inputs(_numPoints);
      vector<double> _outputs(_numPoints);
      for (int _ii=0; _ii<_numPoints; _ii++)
      {
         double Ca_ss = 1e-7 + (1e-1 - 1e-7)*(_ii+0.5)/_numPoints;
         _inputs[_ii] = Ca_ss;
         double fCass_inf = 0.4 + 0.6/(400.0*(Ca_ss*Ca_ss) + 1);
         double tau_fCass = 2 + 80/(400.0*(Ca_ss*Ca_ss) + 1);
         double _fCass_RLA = exp(-_dt/tau_fCass);
         double _fCass_RLB = -fCass_inf*(_fCass_RLA - 1);
         _outputs[_ii] = _fCass_RLB;
      }
      double relError = 1e-4;
      double actualTolerance = _interpolant[1].create(_inputs,_outputs, relError);
      if (actualTolerance > relError  && getRank(0) == 0)
      {
         cerr << "Warning: Could not meet tolerance for _fCass_RLB: " 
              << actualTolerance << " > " << relError
              << " target" << endl;
      }
   }
   {
      int _numPoints = (100 - -100)/1e-2;
      vector<double> _inputs(_numPoints);
      vector<double> _outputs(_numPoints);
      for (int _ii=0; _ii<_numPoints; _ii++)
      {
         double V = -100 + (100 - -100)*(_ii+0.5)/_numPoints;
         _inputs[_ii] = V;
         double _expensive_functions_020 = exp(-1.0L/10.0L*V - 9.0L/2.0L);
         double alpha_xr1 = 450/(_expensive_functions_020 + 1);
         double _expensive_functions_021 = exp(0.0869565217391304*V);
         double beta_xr1 = 6/(13.5813245225782*_expensive_functions_021 + 1);
         double tau_xr1 = alpha_xr1*beta_xr1;
         double _Xr1_RLA = exp(-_dt/tau_xr1);
         _outputs[_ii] = _Xr1_RLA;
      }
      double relError = 1e-4;
      double actualTolerance = _interpolant[2].create(_inputs,_outputs, relError);
      if (actualTolerance > relError  && getRank(0) == 0)
      {
         cerr << "Warning: Could not meet tolerance for _Xr1_RLA: " 
              << actualTolerance << " > " << relError
              << " target" << endl;
      }
   }
   {
      int _numPoints = (100 - -100)/1e-2;
      vector<double> _inputs(_numPoints);
      vector<double> _outputs(_numPoints);
      for (int _ii=0; _ii<_numPoints; _ii++)
      {
         double V = -100 + (100 - -100)*(_ii+0.5)/_numPoints;
         _inputs[_ii] = V;
         double _expensive_functions_020 = exp(-1.0L/10.0L*V - 9.0L/2.0L);
         double alpha_xr1 = 450/(_expensive_functions_020 + 1);
         double _expensive_functions_021 = exp(0.0869565217391304*V);
         double beta_xr1 = 6/(13.5813245225782*_expensive_functions_021 + 1);
         double _expensive_functions_022 = exp(-1.0L/7.0L*V - 26.0L/7.0L);
         double xr1_inf = (1.0/(_expensive_functions_022 + 1));
         double tau_xr1 = alpha_xr1*beta_xr1;
         double _Xr1_RLA = exp(-_dt/tau_xr1);
         double _Xr1_RLB = -xr1_inf*(_Xr1_RLA - 1);
         _outputs[_ii] = _Xr1_RLB;
      }
      double relError = 1e-4;
      double actualTolerance = _interpolant[3].create(_inputs,_outputs, relError);
      if (actualTolerance > relError  && getRank(0) == 0)
      {
         cerr << "Warning: Could not meet tolerance for _Xr1_RLB: " 
              << actualTolerance << " > " << relError
              << " target" << endl;
      }
   }
   {
      int _numPoints = (100 - -100)/1e-2;
      vector<double> _inputs(_numPoints);
      vector<double> _outputs(_numPoints);
      for (int _ii=0; _ii<_numPoints; _ii++)
      {
         double V = -100 + (100 - -100)*(_ii+0.5)/_numPoints;
         _inputs[_ii] = V;
         double _expensive_functions_023 = exp(-1.0L/20.0L*V - 3);
         double alpha_xr2 = 3/(_expensive_functions_023 + 1);
         double _expensive_functions_024 = exp((1.0L/20.0L)*V - 3);
         double beta_xr2 = 1.12/(_expensive_functions_024 + 1);
         double tau_xr2 = alpha_xr2*beta_xr2;
         double _Xr2_RLA = exp(-_dt/tau_xr2);
         _outputs[_ii] = _Xr2_RLA;
      }
      double relError = 1e-4;
      double actualTolerance = _interpolant[4].create(_inputs,_outputs, relError);
      if (actualTolerance > relError  && getRank(0) == 0)
      {
         cerr << "Warning: Could not meet tolerance for _Xr2_RLA: " 
              << actualTolerance << " > " << relError
              << " target" << endl;
      }
   }
   {
      int _numPoints = (100 - -100)/1e-2;
      vector<double> _inputs(_numPoints);
      vector<double> _outputs(_numPoints);
      for (int _ii=0; _ii<_numPoints; _ii++)
      {
         double V = -100 + (100 - -100)*(_ii+0.5)/_numPoints;
         _inputs[_ii] = V;
         double _expensive_functions_023 = exp(-1.0L/20.0L*V - 3);
         double alpha_xr2 = 3/(_expensive_functions_023 + 1);
         double _expensive_functions_024 = exp((1.0L/20.0L)*V - 3);
         double beta_xr2 = 1.12/(_expensive_functions_024 + 1);
         double _expensive_functions_025 = exp((1.0L/24.0L)*V + 11.0L/3.0L);
         double xr2_inf = (1.0/(_expensive_functions_025 + 1));
         double tau_xr2 = alpha_xr2*beta_xr2;
         double _Xr2_RLA = exp(-_dt/tau_xr2);
         double _Xr2_RLB = -xr2_inf*(_Xr2_RLA - 1);
         _outputs[_ii] = _Xr2_RLB;
      }
      double relError = 1e-4;
      double actualTolerance = _interpolant[5].create(_inputs,_outputs, relError);
      if (actualTolerance > relError  && getRank(0) == 0)
      {
         cerr << "Warning: Could not meet tolerance for _Xr2_RLB: " 
              << actualTolerance << " > " << relError
              << " target" << endl;
      }
   }
   {
      int _numPoints = (100 - -100)/1e-2;
      vector<double> _inputs(_numPoints);
      vector<double> _outputs(_numPoints);
      for (int _ii=0; _ii<_numPoints; _ii++)
      {
         double V = -100 + (100 - -100)*(_ii+0.5)/_numPoints;
         _inputs[_ii] = V;
         double _expensive_functions_027 = exp(-1.0L/6.0L*V + 5.0L/6.0L);
         double _expensive_functions_028 = pow(_expensive_functions_027 + 1, -1.0L/2.0L);
         double alpha_xs = 1400*_expensive_functions_028;
         double _expensive_functions_029 = exp((1.0L/15.0L)*V - 7.0L/3.0L);
         double beta_xs = (1.0/(_expensive_functions_029 + 1));
         double tau_xs = alpha_xs*beta_xs + 80;
         double _Xs_RLA = exp(-_dt/tau_xs);
         _outputs[_ii] = _Xs_RLA;
      }
      double relError = 1e-4;
      double actualTolerance = _interpolant[6].create(_inputs,_outputs, relError);
      if (actualTolerance > relError  && getRank(0) == 0)
      {
         cerr << "Warning: Could not meet tolerance for _Xs_RLA: " 
              << actualTolerance << " > " << relError
              << " target" << endl;
      }
   }
   {
      int _numPoints = (100 - -100)/1e-2;
      vector<double> _inputs(_numPoints);
      vector<double> _outputs(_numPoints);
      for (int _ii=0; _ii<_numPoints; _ii++)
      {
         double V = -100 + (100 - -100)*(_ii+0.5)/_numPoints;
         _inputs[_ii] = V;
         double _expensive_functions_027 = exp(-1.0L/6.0L*V + 5.0L/6.0L);
         double _expensive_functions_028 = pow(_expensive_functions_027 + 1, -1.0L/2.0L);
         double alpha_xs = 1400*_expensive_functions_028;
         double _expensive_functions_029 = exp((1.0L/15.0L)*V - 7.0L/3.0L);
         double beta_xs = (1.0/(_expensive_functions_029 + 1));
         double _expensive_functions_030 = exp(-1.0L/14.0L*V - 5.0L/14.0L);
         double xs_inf = (1.0/(_expensive_functions_030 + 1));
         double tau_xs = alpha_xs*beta_xs + 80;
         double _Xs_RLA = exp(-_dt/tau_xs);
         double _Xs_RLB = -xs_inf*(_Xs_RLA - 1);
         _outputs[_ii] = _Xs_RLB;
      }
      double relError = 1e-4;
      double actualTolerance = _interpolant[7].create(_inputs,_outputs, relError);
      if (actualTolerance > relError  && getRank(0) == 0)
      {
         cerr << "Warning: Could not meet tolerance for _Xs_RLB: " 
              << actualTolerance << " > " << relError
              << " target" << endl;
      }
   }
   {
      int _numPoints = (100 - -100)/1e-2;
      vector<double> _inputs(_numPoints);
      vector<double> _outputs(_numPoints);
      for (int _ii=0; _ii<_numPoints; _ii++)
      {
         double V = -100 + (100 - -100)*(_ii+0.5)/_numPoints;
         _inputs[_ii] = V;
         double _expensive_functions = exp(-1.0L/13.0L*V - 35.0L/13.0L);
         double alpha_d = 0.25 + 1.4/(_expensive_functions + 1);
         double _expensive_functions_001 = exp((1.0L/5.0L)*V + 1);
         double beta_d = 1.4/(_expensive_functions_001 + 1);
         double _expensive_functions_003 = exp(-1.0L/20.0L*V + 5.0L/2.0L);
         double gamma_d = (1.0/(_expensive_functions_003 + 1));
         double tau_d = alpha_d*beta_d + gamma_d;
         double _d_RLA = exp(-_dt/tau_d);
         _outputs[_ii] = _d_RLA;
      }
      double relError = 1e-4;
      double actualTolerance = _interpolant[8].create(_inputs,_outputs, relError);
      if (actualTolerance > relError  && getRank(0) == 0)
      {
         cerr << "Warning: Could not meet tolerance for _d_RLA: " 
              << actualTolerance << " > " << relError
              << " target" << endl;
      }
   }
   {
      int _numPoints = (100 - -100)/1e-2;
      vector<double> _inputs(_numPoints);
      vector<double> _outputs(_numPoints);
      for (int _ii=0; _ii<_numPoints; _ii++)
      {
         double V = -100 + (100 - -100)*(_ii+0.5)/_numPoints;
         _inputs[_ii] = V;
         double _expensive_functions = exp(-1.0L/13.0L*V - 35.0L/13.0L);
         double alpha_d = 0.25 + 1.4/(_expensive_functions + 1);
         double _expensive_functions_001 = exp((1.0L/5.0L)*V + 1);
         double beta_d = 1.4/(_expensive_functions_001 + 1);
         double _expensive_functions_002 = exp(-0.133333333333333*V);
         double d_inf = (1.0/(0.344153786865412*_expensive_functions_002 + 1));
         double _expensive_functions_003 = exp(-1.0L/20.0L*V + 5.0L/2.0L);
         double gamma_d = (1.0/(_expensive_functions_003 + 1));
         double tau_d = alpha_d*beta_d + gamma_d;
         double _d_RLA = exp(-_dt/tau_d);
         double _d_RLB = -d_inf*(_d_RLA - 1);
         _outputs[_ii] = _d_RLB;
      }
      double relError = 1e-4;
      double actualTolerance = _interpolant[9].create(_inputs,_outputs, relError);
      if (actualTolerance > relError  && getRank(0) == 0)
      {
         cerr << "Warning: Could not meet tolerance for _d_RLB: " 
              << actualTolerance << " > " << relError
              << " target" << endl;
      }
   }
   {
      int _numPoints = (100 - -100)/1e-2;
      vector<double> _inputs(_numPoints);
      vector<double> _outputs(_numPoints);
      for (int _ii=0; _ii<_numPoints; _ii++)
      {
         double V = -100 + (100 - -100)*(_ii+0.5)/_numPoints;
         _inputs[_ii] = V;
         double _expensive_functions_005 = exp(-1.0L/10.0L*V + 5.0L/2.0L);
         double _expensive_functions_006 = exp((1.0L/10.0L)*V + 3);
         double _expensive_functions_007 = exp(-1.0L/240.0L*((V + 27)*(V + 27)));
         double tau_f2 = 562*_expensive_functions_007 + 80/(_expensive_functions_006 + 1) + 31/(_expensive_functions_005 + 1);
         double _f2_RLA = exp(-_dt/tau_f2);
         _outputs[_ii] = _f2_RLA;
      }
      double relError = 1e-4;
      double actualTolerance = _interpolant[10].create(_inputs,_outputs, relError);
      if (actualTolerance > relError  && getRank(0) == 0)
      {
         cerr << "Warning: Could not meet tolerance for _f2_RLA: " 
              << actualTolerance << " > " << relError
              << " target" << endl;
      }
   }
   {
      int _numPoints = (100 - -100)/1e-2;
      vector<double> _inputs(_numPoints);
      vector<double> _outputs(_numPoints);
      for (int _ii=0; _ii<_numPoints; _ii++)
      {
         double V = -100 + (100 - -100)*(_ii+0.5)/_numPoints;
         _inputs[_ii] = V;
         double _expensive_functions_004 = exp((1.0L/7.0L)*V + 5);
         double f2_inf = 0.33 + 0.67/(_expensive_functions_004 + 1);
         double _expensive_functions_005 = exp(-1.0L/10.0L*V + 5.0L/2.0L);
         double _expensive_functions_006 = exp((1.0L/10.0L)*V + 3);
         double _expensive_functions_007 = exp(-1.0L/240.0L*((V + 27)*(V + 27)));
         double tau_f2 = 562*_expensive_functions_007 + 80/(_expensive_functions_006 + 1) + 31/(_expensive_functions_005 + 1);
         double _f2_RLA = exp(-_dt/tau_f2);
         double _f2_RLB = -f2_inf*(_f2_RLA - 1);
         _outputs[_ii] = _f2_RLB;
      }
      double relError = 1e-4;
      double actualTolerance = _interpolant[11].create(_inputs,_outputs, relError);
      if (actualTolerance > relError  && getRank(0) == 0)
      {
         cerr << "Warning: Could not meet tolerance for _f2_RLB: " 
              << actualTolerance << " > " << relError
              << " target" << endl;
      }
   }
   {
      int _numPoints = (100 - -100)/1e-2;
      vector<double> _inputs(_numPoints);
      vector<double> _outputs(_numPoints);
      for (int _ii=0; _ii<_numPoints; _ii++)
      {
         double V = -100 + (100 - -100)*(_ii+0.5)/_numPoints;
         _inputs[_ii] = V;
         double _expensive_functions_009 = exp((1.0L/10.0L)*V + 3);
         double _expensive_functions_010 = exp(-1.0L/10.0L*V + 13.0L/10.0L);
         double _expensive_functions_011 = exp(-1.0L/225.0L*((V + 27)*(V + 27)));
         double tau_f = 1102.5*_expensive_functions_011 + 20 + 200/(_expensive_functions_010 + 1) + 180/(_expensive_functions_009 + 1);
         double _f_RLA = exp(-_dt/tau_f);
         _outputs[_ii] = _f_RLA;
      }
      double relError = 1e-4;
      double actualTolerance = _interpolant[12].create(_inputs,_outputs, relError);
      if (actualTolerance > relError  && getRank(0) == 0)
      {
         cerr << "Warning: Could not meet tolerance for _f_RLA: " 
              << actualTolerance << " > " << relError
              << " target" << endl;
      }
   }
   {
      int _numPoints = (100 - -100)/1e-2;
      vector<double> _inputs(_numPoints);
      vector<double> _outputs(_numPoints);
      for (int _ii=0; _ii<_numPoints; _ii++)
      {
         double V = -100 + (100 - -100)*(_ii+0.5)/_numPoints;
         _inputs[_ii] = V;
         double _expensive_functions_008 = exp((1.0L/7.0L)*V + 20.0L/7.0L);
         double f_inf = (1.0/(_expensive_functions_008 + 1));
         double _expensive_functions_009 = exp((1.0L/10.0L)*V + 3);
         double _expensive_functions_010 = exp(-1.0L/10.0L*V + 13.0L/10.0L);
         double _expensive_functions_011 = exp(-1.0L/225.0L*((V + 27)*(V + 27)));
         double tau_f = 1102.5*_expensive_functions_011 + 20 + 200/(_expensive_functions_010 + 1) + 180/(_expensive_functions_009 + 1);
         double _f_RLA = exp(-_dt/tau_f);
         double _f_RLB = -f_inf*(_f_RLA - 1);
         _outputs[_ii] = _f_RLB;
      }
      double relError = 1e-4;
      double actualTolerance = _interpolant[13].create(_inputs,_outputs, relError);
      if (actualTolerance > relError  && getRank(0) == 0)
      {
         cerr << "Warning: Could not meet tolerance for _f_RLB: " 
              << actualTolerance << " > " << relError
              << " target" << endl;
      }
   }
   {
      int _numPoints = (100 - -100)/1e-2;
      vector<double> _inputs(_numPoints);
      vector<double> _outputs(_numPoints);
      for (int _ii=0; _ii<_numPoints; _ii++)
      {
         double V = -100 + (100 - -100)*(_ii+0.5)/_numPoints;
         _inputs[_ii] = V;
         double __melodee_temp_000 = V < -40;
         double beta_h;
         double alpha_h;
         if (__melodee_temp_000)
         {
            double _expensive_functions_013 = exp(-0.147058823529412*V);
            alpha_h = 4.43126792958051e-7*_expensive_functions_013;
            double _expensive_functions_014 = exp(0.3485*V);
            double _expensive_functions_015 = exp(0.079*V);
            beta_h = 310000*_expensive_functions_014 + 2.7*_expensive_functions_015;
         }
         else
         {
            alpha_h = 0;
            double _expensive_functions_013 = exp(-0.0900900900900901*V);
            beta_h = 0.77/(0.0497581410839387*_expensive_functions_013 + 0.13);
         }
         double tau_h = (1.0/(alpha_h + beta_h));
         double _h_RLA = exp(-_dt/tau_h);
         _outputs[_ii] = _h_RLA;
      }
      double relError = 1e-4;
      double actualTolerance = _interpolant[14].create(_inputs,_outputs, relError);
      if (actualTolerance > relError  && getRank(0) == 0)
      {
         cerr << "Warning: Could not meet tolerance for _h_RLA: " 
              << actualTolerance << " > " << relError
              << " target" << endl;
      }
   }
   {
      int _numPoints = (100 - -100)/1e-2;
      vector<double> _inputs(_numPoints);
      vector<double> _outputs(_numPoints);
      for (int _ii=0; _ii<_numPoints; _ii++)
      {
         double V = -100 + (100 - -100)*(_ii+0.5)/_numPoints;
         _inputs[_ii] = V;
         double __melodee_temp_000 = V < -40;
         double beta_h;
         double alpha_h;
         if (__melodee_temp_000)
         {
            double _expensive_functions_013 = exp(-0.147058823529412*V);
            alpha_h = 4.43126792958051e-7*_expensive_functions_013;
            double _expensive_functions_014 = exp(0.3485*V);
            double _expensive_functions_015 = exp(0.079*V);
            beta_h = 310000*_expensive_functions_014 + 2.7*_expensive_functions_015;
         }
         else
         {
            alpha_h = 0;
            double _expensive_functions_013 = exp(-0.0900900900900901*V);
            beta_h = 0.77/(0.0497581410839387*_expensive_functions_013 + 0.13);
         }
         double _expensive_functions_013 = exp(0.134589502018843*V);
         double h_inf = (1.0/(15212.5932856544*_expensive_functions_013 + 1)/(15212.5932856544*_expensive_functions_013 + 1));
         double tau_h = (1.0/(alpha_h + beta_h));
         double _h_RLA = exp(-_dt/tau_h);
         double _h_RLB = -h_inf*(_h_RLA - 1);
         _outputs[_ii] = _h_RLB;
      }
      double relError = 1e-4;
      double actualTolerance = _interpolant[15].create(_inputs,_outputs, relError);
      if (actualTolerance > relError  && getRank(0) == 0)
      {
         cerr << "Warning: Could not meet tolerance for _h_RLB: " 
              << actualTolerance << " > " << relError
              << " target" << endl;
      }
   }
   {
      int _numPoints = (100 - -100)/1e-2;
      vector<double> _inputs(_numPoints);
      vector<double> _outputs(_numPoints);
      for (int _ii=0; _ii<_numPoints; _ii++)
      {
         double V = -100 + (100 - -100)*(_ii+0.5)/_numPoints;
         _inputs[_ii] = V;
         double __melodee_temp_001 = V < -40;
         double beta_j;
         double alpha_j;
         if (__melodee_temp_001)
         {
            double _expensive_functions_014 = exp(0.311*V);
            double _expensive_functions_015 = exp(0.2444*V);
            double _expensive_functions_016 = exp(-0.04391*V);
            alpha_j = (-25428*_expensive_functions_015 - 6.948e-6*_expensive_functions_016)*(V + 37.78)/(50262745825.954*_expensive_functions_014 + 1);
            double _expensive_functions_017 = exp(-0.1378*V);
            double _expensive_functions_018 = exp(-0.01052*V);
            beta_j = 0.02424*_expensive_functions_018/(0.00396086833990426*_expensive_functions_017 + 1);
         }
         else
         {
            alpha_j = 0;
            double _expensive_functions_014 = exp(-0.1*V);
            double _expensive_functions_015 = exp(0.057*V);
            beta_j = 0.6*_expensive_functions_015/(0.0407622039783662*_expensive_functions_014 + 1);
         }
         double tau_j = (1.0/(alpha_j + beta_j));
         double _j_RLA = exp(-_dt/tau_j);
         _outputs[_ii] = _j_RLA;
      }
      double relError = 1e-4;
      double actualTolerance = _interpolant[16].create(_inputs,_outputs, relError);
      if (actualTolerance > relError  && getRank(0) == 0)
      {
         cerr << "Warning: Could not meet tolerance for _j_RLA: " 
              << actualTolerance << " > " << relError
              << " target" << endl;
      }
   }
   {
      int _numPoints = (100 - -100)/1e-2;
      vector<double> _inputs(_numPoints);
      vector<double> _outputs(_numPoints);
      for (int _ii=0; _ii<_numPoints; _ii++)
      {
         double V = -100 + (100 - -100)*(_ii+0.5)/_numPoints;
         _inputs[_ii] = V;
         double __melodee_temp_001 = V < -40;
         double beta_j;
         double alpha_j;
         if (__melodee_temp_001)
         {
            double _expensive_functions_014 = exp(0.311*V);
            double _expensive_functions_015 = exp(0.2444*V);
            double _expensive_functions_016 = exp(-0.04391*V);
            alpha_j = (-25428*_expensive_functions_015 - 6.948e-6*_expensive_functions_016)*(V + 37.78)/(50262745825.954*_expensive_functions_014 + 1);
            double _expensive_functions_017 = exp(-0.1378*V);
            double _expensive_functions_018 = exp(-0.01052*V);
            beta_j = 0.02424*_expensive_functions_018/(0.00396086833990426*_expensive_functions_017 + 1);
         }
         else
         {
            alpha_j = 0;
            double _expensive_functions_014 = exp(-0.1*V);
            double _expensive_functions_015 = exp(0.057*V);
            beta_j = 0.6*_expensive_functions_015/(0.0407622039783662*_expensive_functions_014 + 1);
         }
         double _expensive_functions_014 = exp(0.134589502018843*V);
         double j_inf = (1.0/(15212.5932856544*_expensive_functions_014 + 1)/(15212.5932856544*_expensive_functions_014 + 1));
         double tau_j = (1.0/(alpha_j + beta_j));
         double _j_RLA = exp(-_dt/tau_j);
         double _j_RLB = -j_inf*(_j_RLA - 1);
         _outputs[_ii] = _j_RLB;
      }
      double relError = 1e-4;
      double actualTolerance = _interpolant[17].create(_inputs,_outputs, relError);
      if (actualTolerance > relError  && getRank(0) == 0)
      {
         cerr << "Warning: Could not meet tolerance for _j_RLB: " 
              << actualTolerance << " > " << relError
              << " target" << endl;
      }
   }
   {
      int _numPoints = (100 - -100)/1e-2;
      vector<double> _inputs(_numPoints);
      vector<double> _outputs(_numPoints);
      for (int _ii=0; _ii<_numPoints; _ii++)
      {
         double V = -100 + (100 - -100)*(_ii+0.5)/_numPoints;
         _inputs[_ii] = V;
         double _expensive_functions_015 = exp(-1.0L/5.0L*V - 12);
         double alpha_m = (1.0/(_expensive_functions_015 + 1));
         double _expensive_functions_016 = exp((1.0L/5.0L)*V + 7);
         double _expensive_functions_017 = exp((1.0L/200.0L)*V - 1.0L/4.0L);
         double beta_m = 0.1/(_expensive_functions_017 + 1) + 0.1/(_expensive_functions_016 + 1);
         double tau_m = alpha_m*beta_m;
         double _m_RLA = exp(-_dt/tau_m);
         _outputs[_ii] = _m_RLA;
      }
      double relError = 1e-4;
      double actualTolerance = _interpolant[18].create(_inputs,_outputs, relError);
      if (actualTolerance > relError  && getRank(0) == 0)
      {
         cerr << "Warning: Could not meet tolerance for _m_RLA: " 
              << actualTolerance << " > " << relError
              << " target" << endl;
      }
   }
   {
      int _numPoints = (100 - -100)/1e-2;
      vector<double> _inputs(_numPoints);
      vector<double> _outputs(_numPoints);
      for (int _ii=0; _ii<_numPoints; _ii++)
      {
         double V = -100 + (100 - -100)*(_ii+0.5)/_numPoints;
         _inputs[_ii] = V;
         double _expensive_functions_015 = exp(-1.0L/5.0L*V - 12);
         double alpha_m = (1.0/(_expensive_functions_015 + 1));
         double _expensive_functions_016 = exp((1.0L/5.0L)*V + 7);
         double _expensive_functions_017 = exp((1.0L/200.0L)*V - 1.0L/4.0L);
         double beta_m = 0.1/(_expensive_functions_017 + 1) + 0.1/(_expensive_functions_016 + 1);
         double _expensive_functions_018 = exp(-0.110741971207087*V);
         double m_inf = (1.0/(0.00184221158116513*_expensive_functions_018 + 1)/(0.00184221158116513*_expensive_functions_018 + 1));
         double tau_m = alpha_m*beta_m;
         double _m_RLA = exp(-_dt/tau_m);
         double _m_RLB = -m_inf*(_m_RLA - 1);
         _outputs[_ii] = _m_RLB;
      }
      double relError = 1e-4;
      double actualTolerance = _interpolant[19].create(_inputs,_outputs, relError);
      if (actualTolerance > relError  && getRank(0) == 0)
      {
         cerr << "Warning: Could not meet tolerance for _m_RLB: " 
              << actualTolerance << " > " << relError
              << " target" << endl;
      }
   }
   {
      int _numPoints = (100 - -100)/1e-2;
      vector<double> _inputs(_numPoints);
      vector<double> _outputs(_numPoints);
      for (int _ii=0; _ii<_numPoints; _ii++)
      {
         double V = -100 + (100 - -100)*(_ii+0.5)/_numPoints;
         _inputs[_ii] = V;
         double _expensive_functions_034 = exp(-1.0L/1800.0L*((V + 40)*(V + 40)));
         double tau_r = 9.5*_expensive_functions_034 + 0.8;
         double _r_RLA = exp(-_dt/tau_r);
         _outputs[_ii] = _r_RLA;
      }
      double relError = 1e-4;
      double actualTolerance = _interpolant[20].create(_inputs,_outputs, relError);
      if (actualTolerance > relError  && getRank(0) == 0)
      {
         cerr << "Warning: Could not meet tolerance for _r_RLA: " 
              << actualTolerance << " > " << relError
              << " target" << endl;
      }
   }
   {
      int _numPoints = (100 - -100)/1e-2;
      vector<double> _inputs(_numPoints);
      vector<double> _outputs(_numPoints);
      for (int _ii=0; _ii<_numPoints; _ii++)
      {
         double V = -100 + (100 - -100)*(_ii+0.5)/_numPoints;
         _inputs[_ii] = V;
         double _expensive_functions_033 = exp(-1.0L/6.0L*V + 10.0L/3.0L);
         double r_inf = (1.0/(_expensive_functions_033 + 1));
         double _expensive_functions_034 = exp(-1.0L/1800.0L*((V + 40)*(V + 40)));
         double tau_r = 9.5*_expensive_functions_034 + 0.8;
         double _r_RLA = exp(-_dt/tau_r);
         double _r_RLB = -r_inf*(_r_RLA - 1);
         _outputs[_ii] = _r_RLB;
      }
      double relError = 1e-4;
      double actualTolerance = _interpolant[21].create(_inputs,_outputs, relError);
      if (actualTolerance > relError  && getRank(0) == 0)
      {
         cerr << "Warning: Could not meet tolerance for _r_RLB: " 
              << actualTolerance << " > " << relError
              << " target" << endl;
      }
   }
   {
      int _numPoints = (100 - -100)/1e-2;
      vector<double> _inputs(_numPoints);
      vector<double> _outputs(_numPoints);
      for (int _ii=0; _ii<_numPoints; _ii++)
      {
         double V = -100 + (100 - -100)*(_ii+0.5)/_numPoints;
         _inputs[_ii] = V;
         double celltype = 0;
         double __melodee_temp_003 = celltype == 0;
         double tau_s;
         double s_inf;
         if (__melodee_temp_003)
         {
            double _expensive_functions_036 = exp(-1.0L/1000.0L*((V + 67)*(V + 67)));
            tau_s = 1000*_expensive_functions_036 + 8;
         }
         else
         {
            double _expensive_functions_036 = exp((1.0L/5.0L)*V - 4);
            double _expensive_functions_037 = exp(-1.0L/320.0L*((V + 45)*(V + 45)));
            tau_s = 85*_expensive_functions_037 + 3 + 5/(_expensive_functions_036 + 1);
         }
         double _s_RLA = exp(-_dt/tau_s);
         _outputs[_ii] = _s_RLA;
      }
      double relError = 1e-4;
      double actualTolerance = _interpolant[22].create(_inputs,_outputs, relError);
      if (actualTolerance > relError  && getRank(0) == 0)
      {
         cerr << "Warning: Could not meet tolerance for _s_RLA: " 
              << actualTolerance << " > " << relError
              << " target" << endl;
      }
   }
   {
      int _numPoints = (100 - -100)/1e-2;
      vector<double> _inputs(_numPoints);
      vector<double> _outputs(_numPoints);
      for (int _ii=0; _ii<_numPoints; _ii++)
      {
         double V = -100 + (100 - -100)*(_ii+0.5)/_numPoints;
         _inputs[_ii] = V;
         double celltype = 0;
         double __melodee_temp_003 = celltype == 0;
         double tau_s;
         double s_inf;
         if (__melodee_temp_003)
         {
            double _expensive_functions_035 = exp((1.0L/5.0L)*V + 28.0L/5.0L);
            s_inf = (1.0/(_expensive_functions_035 + 1));
            double _expensive_functions_036 = exp(-1.0L/1000.0L*((V + 67)*(V + 67)));
            tau_s = 1000*_expensive_functions_036 + 8;
         }
         else
         {
            double _expensive_functions_035 = exp((1.0L/5.0L)*V + 4);
            s_inf = (1.0/(_expensive_functions_035 + 1));
            double _expensive_functions_036 = exp((1.0L/5.0L)*V - 4);
            double _expensive_functions_037 = exp(-1.0L/320.0L*((V + 45)*(V + 45)));
            tau_s = 85*_expensive_functions_037 + 3 + 5/(_expensive_functions_036 + 1);
         }
         double _s_RLA = exp(-_dt/tau_s);
         double _s_RLB = -s_inf*(_s_RLA - 1);
         _outputs[_ii] = _s_RLB;
      }
      double relError = 1e-4;
      double actualTolerance = _interpolant[23].create(_inputs,_outputs, relError);
      if (actualTolerance > relError  && getRank(0) == 0)
      {
         cerr << "Warning: Could not meet tolerance for _s_RLB: " 
              << actualTolerance << " > " << relError
              << " target" << endl;
      }
   }
   {
      int _numPoints = (100 - -100)/1e-2;
      vector<double> _inputs(_numPoints);
      vector<double> _outputs(_numPoints);
      for (int _ii=0; _ii<_numPoints; _ii++)
      {
         double V = -100 + (100 - -100)*(_ii+0.5)/_numPoints;
         _inputs[_ii] = V;
         double F = 96485.3415000000;
         double R = 8314.47200000000;
         double T = 310;
         double gamma = 0.350000000000000;
         double exp_gamma_VFRT = exp(F*V*gamma/(R*T));
         _outputs[_ii] = exp_gamma_VFRT;
      }
      double relError = 1e-4;
      double actualTolerance = _interpolant[24].create(_inputs,_outputs, relError);
      if (actualTolerance > relError  && getRank(0) == 0)
      {
         cerr << "Warning: Could not meet tolerance for exp_gamma_VFRT: " 
              << actualTolerance << " > " << relError
              << " target" << endl;
      }
   }
   {
      int _numPoints = (100 - -100)/1e-2;
      vector<double> _inputs(_numPoints);
      vector<double> _outputs(_numPoints);
      for (int _ii=0; _ii<_numPoints; _ii++)
      {
         double V = -100 + (100 - -100)*(_ii+0.5)/_numPoints;
         _inputs[_ii] = V;
         double F = 96485.3415000000;
         double R = 8314.47200000000;
         double T = 310;
         double gamma = 0.350000000000000;
         double exp_gamma_m1_VFRT = exp(F*V*(gamma - 1)/(R*T));
         _outputs[_ii] = exp_gamma_m1_VFRT;
      }
      double relError = 1e-4;
      double actualTolerance = _interpolant[25].create(_inputs,_outputs, relError);
      if (actualTolerance > relError  && getRank(0) == 0)
      {
         cerr << "Warning: Could not meet tolerance for exp_gamma_m1_VFRT: " 
              << actualTolerance << " > " << relError
              << " target" << endl;
      }
   }
   {
      int _numPoints = (100 - -100)/1e-2;
      vector<double> _inputs(_numPoints);
      vector<double> _outputs(_numPoints);
      for (int _ii=0; _ii<_numPoints; _ii++)
      {
         double V = -100 + (100 - -100)*(_ii+0.5)/_numPoints;
         _inputs[_ii] = V;
         double F = 96485.3415000000;
         double R = 8314.47200000000;
         double T = 310;
         double i_CalTerm1 = (F*F)*(4*V - 60)/(R*T);
         double i_CalTerm2 = exp(F*(2*V - 30)/(R*T));
         double i_CalTerm3 = i_CalTerm1/(i_CalTerm2 - 1);
         _outputs[_ii] = i_CalTerm3;
      }
      double relError = 1e-4;
      double actualTolerance = _interpolant[26].create(_inputs,_outputs, relError);
      if (actualTolerance > relError  && getRank(0) == 0)
      {
         cerr << "Warning: Could not meet tolerance for i_CalTerm3: " 
              << actualTolerance << " > " << relError
              << " target" << endl;
      }
   }
   {
      int _numPoints = (100 - -100)/1e-2;
      vector<double> _inputs(_numPoints);
      vector<double> _outputs(_numPoints);
      for (int _ii=0; _ii<_numPoints; _ii++)
      {
         double V = -100 + (100 - -100)*(_ii+0.5)/_numPoints;
         _inputs[_ii] = V;
         double F = 96485.3415000000;
         double R = 8314.47200000000;
         double T = 310;
         double i_CalTerm1 = (F*F)*(4*V - 60)/(R*T);
         double i_CalTerm2 = exp(F*(2*V - 30)/(R*T));
         double i_CalTerm3 = i_CalTerm1/(i_CalTerm2 - 1);
         double i_CalTerm4 = i_CalTerm2*i_CalTerm3;
         _outputs[_ii] = i_CalTerm4;
      }
      double relError = 1e-4;
      double actualTolerance = _interpolant[27].create(_inputs,_outputs, relError);
      if (actualTolerance > relError  && getRank(0) == 0)
      {
         cerr << "Warning: Could not meet tolerance for i_CalTerm4: " 
              << actualTolerance << " > " << relError
              << " target" << endl;
      }
   }
   {
      int _numPoints = (100 - -100)/1e-2;
      vector<double> _inputs(_numPoints);
      vector<double> _outputs(_numPoints);
      for (int _ii=0; _ii<_numPoints; _ii++)
      {
         double V = -100 + (100 - -100)*(_ii+0.5)/_numPoints;
         _inputs[_ii] = V;
         double F = 96485.3415000000;
         double R = 8314.47200000000;
         double T = 310;
         double K_o = 5.40000000000000;
         double K_mk = 1;
         double P_NaK = 2.72400000000000;
         double _expensive_functions_031 = exp(-F*V/(R*T));
         double _expensive_functions_032 = exp(-0.1*F*V/(R*T));
         double i_NaK_term = K_o*P_NaK/((K_o + K_mk)*(0.0353*_expensive_functions_031 + 0.1245*_expensive_functions_032 + 1));
         _outputs[_ii] = i_NaK_term;
      }
      double relError = 1e-4;
      double actualTolerance = _interpolant[28].create(_inputs,_outputs, relError);
      if (actualTolerance > relError  && getRank(0) == 0)
      {
         cerr << "Warning: Could not meet tolerance for i_NaK_term: " 
              << actualTolerance << " > " << relError
              << " target" << endl;
      }
   }
   {
      int _numPoints = (100 - -100)/1e-2;
      vector<double> _inputs(_numPoints);
      vector<double> _outputs(_numPoints);
      for (int _ii=0; _ii<_numPoints; _ii++)
      {
         double V = -100 + (100 - -100)*(_ii+0.5)/_numPoints;
         _inputs[_ii] = V;
         double _expensive_functions_019 = exp(-0.167224080267559*V);
         double i_p_K_term = (1.0/(65.4052157419383*_expensive_functions_019 + 1));
         _outputs[_ii] = i_p_K_term;
      }
      double relError = 1e-4;
      double actualTolerance = _interpolant[29].create(_inputs,_outputs, relError);
      if (actualTolerance > relError  && getRank(0) == 0)
      {
         cerr << "Warning: Could not meet tolerance for i_p_K_term: " 
              << actualTolerance << " > " << relError
              << " target" << endl;
      }
   }
   {
      int _numPoints = (100 - -100)/1e-2;
      vector<double> _inputs(_numPoints);
      vector<double> _outputs(_numPoints);
      for (int _ii=0; _ii<_numPoints; _ii++)
      {
         double VEK = -100 + (100 - -100)*(_ii+0.5)/_numPoints;
         _inputs[_ii] = VEK;
         double g_K1 = 5.40500000000000;
         double K_o = 5.40000000000000;
         double _expensive_functions_035 = exp(0.06*VEK);
         double alpha_K1 = 0.1/(6.14421235332821e-6*_expensive_functions_035 + 1);
         double _expensive_functions_036 = exp(-0.5*VEK);
         double _expensive_functions_037 = exp(0.1*VEK);
         double _expensive_functions_038 = exp(0.0002*VEK);
         double beta_K1 = (0.367879441171442*_expensive_functions_037 + 3.06060402008027*_expensive_functions_038)/(_expensive_functions_036 + 1);
         double xK1_inf = alpha_K1/(alpha_K1 + beta_K1);
         double _expensive_functions_039 = sqrt(K_o);
         double i_K1 = 0.430331482911935*VEK*_expensive_functions_039*g_K1*xK1_inf;
         double inward_rectifier_potassium_current_i_Kitot = i_K1;
         _outputs[_ii] = inward_rectifier_potassium_current_i_Kitot;
      }
      double relError = 1e-4;
      double actualTolerance = _interpolant[30].create(_inputs,_outputs, relError);
      if (actualTolerance > relError  && getRank(0) == 0)
      {
         cerr << "Warning: Could not meet tolerance for inward_rectifier_potassium_current_i_Kitot: " 
              << actualTolerance << " > " << relError
              << " target" << endl;
      }
   }
}

void ThisReaction::calc(double _dt, const VectorDouble32& __Vm,
                       const vector<double>& __iStim , VectorDouble32& __dVm)
{
   vector<double>& state(stateTransport_.modifyOnDevice());

   {
      int errorCode=-1;
      if (blockSize_ == -1) { blockSize_ = 1024; }
      while(1)
      {
         void* args[] = { ledger_lookup(&__Vm[0]),
                          ledger_lookup(&__iStim[0]),
                          ledger_lookup(&__dVm[0]),
                          ledger_lookup(&state[0])};
         int errorCode = cuLaunchKernel(_kernel,
                                        (nCells_+blockSize_-1)/blockSize_, 1, 1,
                                        blockSize_,1,1,
                                        0, NULL,
                                        args, 0);
         if (errorCode == CUDA_ERROR_LAUNCH_OUT_OF_RESOURCES && blockSize_ > 0)
         {
            blockSize_ /= 2;
            continue;
         }
         else if (errorCode != CUDA_SUCCESS)
         {
            printf("Cuda return %d;\n", errorCode);
            assert(0 && "Launch failed");
         }
         else
         {
            break;
         }
         //cuCtxSynchronize();
      }
   }
}

void ThisReaction::initializeMembraneVoltage(VectorDouble32& __Vm)
{
   assert(__Vm.size() >= nCells_);


   double V_init = -83;
   double V = V_init;
   double Ca_i_init = 2.00000000000000e-5;
   double Ca_i = Ca_i_init;
   double R_prime_init = 0.986800000000000;
   double R_prime = R_prime_init;
   double Ca_SR_init = 3.15500000000000;
   double Ca_SR = Ca_SR_init;
   double Ca_ss_init = 0.000170000000000000;
   double Ca_ss = Ca_ss_init;
   double d_init = 3.16400000000000e-5;
   double d = d_init;
   double f2_init = 0.977800000000000;
   double f2 = f2_init;
   double fCass_init = 0.995300000000000;
   double fCass = fCass_init;
   double f_init = 0.960900000000000;
   double f = f_init;
   double h_init = 0.550000000000000;
   double h = h_init;
   double j_init = 0.660000000000000;
   double j = j_init;
   double m_init = 0.00155000000000000;
   double m = m_init;
   double K_i_init = 138.400000000000;
   double K_i = K_i_init;
   double Xr1_init = 0.00448000000000000;
   double Xr1 = Xr1_init;
   double Xr2_init = 0.476000000000000;
   double Xr2 = Xr2_init;
   double Xs_init = 0.00870000000000000;
   double Xs = Xs_init;
   double Na_i_init = 10.3550000000000;
   double Na_i = Na_i_init;
   double r_init = 2.23500000000000e-8;
   double r = r_init;
   double s_init = 0.601200000000000;
   double s = s_init;
      
   
   vector<double>& stateData(stateTransport_.modifyOnHost());
   
   for (int iCell=0; iCell<nCells_; iCell++)
   {
      stateData[_Ca_SR_off*nCells_+iCell] = Ca_SR;
      stateData[_Ca_i_off*nCells_+iCell] = Ca_i;
      stateData[_Ca_ss_off*nCells_+iCell] = Ca_ss;
      stateData[_K_i_off*nCells_+iCell] = K_i;
      stateData[_Na_i_off*nCells_+iCell] = Na_i;
      stateData[_R_prime_off*nCells_+iCell] = R_prime;
      stateData[_Xr1_off*nCells_+iCell] = Xr1;
      stateData[_Xr2_off*nCells_+iCell] = Xr2;
      stateData[_Xs_off*nCells_+iCell] = Xs;
      stateData[_d_off*nCells_+iCell] = d;
      stateData[_f_off*nCells_+iCell] = f;
      stateData[_f2_off*nCells_+iCell] = f2;
      stateData[_fCass_off*nCells_+iCell] = fCass;
      stateData[_h_off*nCells_+iCell] = h;
      stateData[_j_off*nCells_+iCell] = j;
      stateData[_m_off*nCells_+iCell] = m;
      stateData[_r_off*nCells_+iCell] = r;
      stateData[_s_off*nCells_+iCell] = s;
   }

   __Vm.assign(__Vm.size(), V_init);
}

enum varHandles
{
   Ca_SR_handle,
   Ca_i_handle,
   Ca_ss_handle,
   K_i_handle,
   Na_i_handle,
   R_prime_handle,
   Xr1_handle,
   Xr2_handle,
   Xs_handle,
   d_handle,
   f_handle,
   f2_handle,
   fCass_handle,
   h_handle,
   j_handle,
   m_handle,
   r_handle,
   s_handle,
   Iion_handle,
   i_CaL_handle,
   i_K1_handle,
   i_Kr_handle,
   i_Ks_handle,
   i_Na_handle,
   i_NaCa_handle,
   i_NaK_handle,
   i_b_Ca_handle,
   i_b_Na_handle,
   i_leak_handle,
   i_p_Ca_handle,
   i_p_K_handle,
   i_rel_handle,
   i_to_handle,
   i_up_handle,
   i_xfer_handle,
   NUMHANDLES
};

const string ThisReaction::getUnit(const std::string& varName) const
{
   if(0) {}
   else if (varName == "Ca_SR") { return "uM"; }
   else if (varName == "Ca_i") { return "uM"; }
   else if (varName == "Ca_ss") { return "uM"; }
   else if (varName == "Iion") { return "V/s"; }
   else if (varName == "K_i") { return "mM"; }
   else if (varName == "Na_i") { return "mM"; }
   else if (varName == "R_prime") { return "1"; }
   else if (varName == "Xr1") { return "1"; }
   else if (varName == "Xr2") { return "1"; }
   else if (varName == "Xs") { return "1"; }
   else if (varName == "d") { return "1"; }
   else if (varName == "f") { return "1"; }
   else if (varName == "f2") { return "1"; }
   else if (varName == "fCass") { return "1"; }
   else if (varName == "h") { return "1"; }
   else if (varName == "i_CaL") { return "V/s"; }
   else if (varName == "i_K1") { return "INVALID"; }
   else if (varName == "i_Kr") { return "INVALID"; }
   else if (varName == "i_Ks") { return "INVALID"; }
   else if (varName == "i_Na") { return "INVALID"; }
   else if (varName == "i_NaCa") { return "V/s"; }
   else if (varName == "i_NaK") { return "INVALID"; }
   else if (varName == "i_b_Ca") { return "V/s"; }
   else if (varName == "i_b_Na") { return "INVALID"; }
   else if (varName == "i_leak") { return "INVALID"; }
   else if (varName == "i_p_Ca") { return "V/s"; }
   else if (varName == "i_p_K") { return "INVALID"; }
   else if (varName == "i_rel") { return "INVALID"; }
   else if (varName == "i_to") { return "INVALID"; }
   else if (varName == "i_up") { return "INVALID"; }
   else if (varName == "i_xfer") { return "INVALID"; }
   else if (varName == "j") { return "1"; }
   else if (varName == "m") { return "1"; }
   else if (varName == "r") { return "1"; }
   else if (varName == "s") { return "1"; }
   return "INVALID";
}

int ThisReaction::getVarHandle(const std::string& varName) const
{
   if (0) {}
   else if (varName == "Ca_SR") { return Ca_SR_handle; }
   else if (varName == "Ca_i") { return Ca_i_handle; }
   else if (varName == "Ca_ss") { return Ca_ss_handle; }
   else if (varName == "Iion") { return Iion_handle; }
   else if (varName == "K_i") { return K_i_handle; }
   else if (varName == "Na_i") { return Na_i_handle; }
   else if (varName == "R_prime") { return R_prime_handle; }
   else if (varName == "Xr1") { return Xr1_handle; }
   else if (varName == "Xr2") { return Xr2_handle; }
   else if (varName == "Xs") { return Xs_handle; }
   else if (varName == "d") { return d_handle; }
   else if (varName == "f") { return f_handle; }
   else if (varName == "f2") { return f2_handle; }
   else if (varName == "fCass") { return fCass_handle; }
   else if (varName == "h") { return h_handle; }
   else if (varName == "i_CaL") { return i_CaL_handle; }
   else if (varName == "i_K1") { return i_K1_handle; }
   else if (varName == "i_Kr") { return i_Kr_handle; }
   else if (varName == "i_Ks") { return i_Ks_handle; }
   else if (varName == "i_Na") { return i_Na_handle; }
   else if (varName == "i_NaCa") { return i_NaCa_handle; }
   else if (varName == "i_NaK") { return i_NaK_handle; }
   else if (varName == "i_b_Ca") { return i_b_Ca_handle; }
   else if (varName == "i_b_Na") { return i_b_Na_handle; }
   else if (varName == "i_leak") { return i_leak_handle; }
   else if (varName == "i_p_Ca") { return i_p_Ca_handle; }
   else if (varName == "i_p_K") { return i_p_K_handle; }
   else if (varName == "i_rel") { return i_rel_handle; }
   else if (varName == "i_to") { return i_to_handle; }
   else if (varName == "i_up") { return i_up_handle; }
   else if (varName == "i_xfer") { return i_xfer_handle; }
   else if (varName == "j") { return j_handle; }
   else if (varName == "m") { return m_handle; }
   else if (varName == "r") { return r_handle; }
   else if (varName == "s") { return s_handle; }
   return -1;
}

void ThisReaction::setValue(int iCell, int varHandle, double value) 
{
   vector<double>& stateData(stateTransport_.modifyOnHost());
   
   if (0) {}
   else if (varHandle == Ca_SR_handle) { stateData[_Ca_SR_off*nCells_+iCell] = value; }
   else if (varHandle == Ca_i_handle) { stateData[_Ca_i_off*nCells_+iCell] = value; }
   else if (varHandle == Ca_ss_handle) { stateData[_Ca_ss_off*nCells_+iCell] = value; }
   else if (varHandle == K_i_handle) { stateData[_K_i_off*nCells_+iCell] = value; }
   else if (varHandle == Na_i_handle) { stateData[_Na_i_off*nCells_+iCell] = value; }
   else if (varHandle == R_prime_handle) { stateData[_R_prime_off*nCells_+iCell] = value; }
   else if (varHandle == Xr1_handle) { stateData[_Xr1_off*nCells_+iCell] = value; }
   else if (varHandle == Xr2_handle) { stateData[_Xr2_off*nCells_+iCell] = value; }
   else if (varHandle == Xs_handle) { stateData[_Xs_off*nCells_+iCell] = value; }
   else if (varHandle == d_handle) { stateData[_d_off*nCells_+iCell] = value; }
   else if (varHandle == f_handle) { stateData[_f_off*nCells_+iCell] = value; }
   else if (varHandle == f2_handle) { stateData[_f2_off*nCells_+iCell] = value; }
   else if (varHandle == fCass_handle) { stateData[_fCass_off*nCells_+iCell] = value; }
   else if (varHandle == h_handle) { stateData[_h_off*nCells_+iCell] = value; }
   else if (varHandle == j_handle) { stateData[_j_off*nCells_+iCell] = value; }
   else if (varHandle == m_handle) { stateData[_m_off*nCells_+iCell] = value; }
   else if (varHandle == r_handle) { stateData[_r_off*nCells_+iCell] = value; }
   else if (varHandle == s_handle) { stateData[_s_off*nCells_+iCell] = value; }
}


double ThisReaction::getValue(int iCell, int varHandle) const
{
   const vector<double>& stateData(stateTransport_.readOnHost());

   if (0) {}
   else if (varHandle == Ca_SR_handle) { return stateData[_Ca_SR_off*nCells_+iCell]; }
   else if (varHandle == Ca_i_handle) { return stateData[_Ca_i_off*nCells_+iCell]; }
   else if (varHandle == Ca_ss_handle) { return stateData[_Ca_ss_off*nCells_+iCell]; }
   else if (varHandle == K_i_handle) { return stateData[_K_i_off*nCells_+iCell]; }
   else if (varHandle == Na_i_handle) { return stateData[_Na_i_off*nCells_+iCell]; }
   else if (varHandle == R_prime_handle) { return stateData[_R_prime_off*nCells_+iCell]; }
   else if (varHandle == Xr1_handle) { return stateData[_Xr1_off*nCells_+iCell]; }
   else if (varHandle == Xr2_handle) { return stateData[_Xr2_off*nCells_+iCell]; }
   else if (varHandle == Xs_handle) { return stateData[_Xs_off*nCells_+iCell]; }
   else if (varHandle == d_handle) { return stateData[_d_off*nCells_+iCell]; }
   else if (varHandle == f_handle) { return stateData[_f_off*nCells_+iCell]; }
   else if (varHandle == f2_handle) { return stateData[_f2_off*nCells_+iCell]; }
   else if (varHandle == fCass_handle) { return stateData[_fCass_off*nCells_+iCell]; }
   else if (varHandle == h_handle) { return stateData[_h_off*nCells_+iCell]; }
   else if (varHandle == j_handle) { return stateData[_j_off*nCells_+iCell]; }
   else if (varHandle == m_handle) { return stateData[_m_off*nCells_+iCell]; }
   else if (varHandle == r_handle) { return stateData[_r_off*nCells_+iCell]; }
   else if (varHandle == s_handle) { return stateData[_s_off*nCells_+iCell]; }
   return NAN;
}

double ThisReaction::getValue(int iCell, int varHandle, double V) const
{
   const vector<double>& stateData(stateTransport_.readOnHost());

   const double Ca_SR=stateData[_Ca_SR_off*nCells_+iCell];
   const double Ca_i=stateData[_Ca_i_off*nCells_+iCell];
   const double Ca_ss=stateData[_Ca_ss_off*nCells_+iCell];
   const double K_i=stateData[_K_i_off*nCells_+iCell];
   const double Na_i=stateData[_Na_i_off*nCells_+iCell];
   const double R_prime=stateData[_R_prime_off*nCells_+iCell];
   const double Xr1=stateData[_Xr1_off*nCells_+iCell];
   const double Xr2=stateData[_Xr2_off*nCells_+iCell];
   const double Xs=stateData[_Xs_off*nCells_+iCell];
   const double d=stateData[_d_off*nCells_+iCell];
   const double f=stateData[_f_off*nCells_+iCell];
   const double f2=stateData[_f2_off*nCells_+iCell];
   const double fCass=stateData[_fCass_off*nCells_+iCell];
   const double h=stateData[_h_off*nCells_+iCell];
   const double j=stateData[_j_off*nCells_+iCell];
   const double m=stateData[_m_off*nCells_+iCell];
   const double r=stateData[_r_off*nCells_+iCell];
   const double s=stateData[_s_off*nCells_+iCell];
   if (0) {}
   else if (varHandle == Ca_SR_handle)
   {
      return Ca_SR;
   }
   else if (varHandle == Ca_i_handle)
   {
      return Ca_i;
   }
   else if (varHandle == Ca_ss_handle)
   {
      return Ca_ss;
   }
   else if (varHandle == Iion_handle)
   {
      double celltype = 0;
      double F = 96485.3415000000;
      double R = 8314.47200000000;
      double T = 310;
      double Ca_o = 2;
      double g_CaL = 3.98000000000000e-5;
      double i_CalTerm1 = (F*F)*(4*V - 60)/(R*T);
      double i_CalTerm2 = exp(F*(2*V - 30)/(R*T));
      double i_CalTerm3 = i_CalTerm1/(i_CalTerm2 - 1);
      double i_CalTerm4 = i_CalTerm2*i_CalTerm3;
      double g_bca = 0.000592000000000000;
      double _expensive_functions_012 = log(Ca_o/Ca_i);
      double E_Ca = 0.5*R*T*_expensive_functions_012/F;
      double i_b_Ca = g_bca*(V - E_Ca);
      double i_Caitot = i_b_Ca;
      double K_pCa = 0.000500000000000000;
      double g_pCa = 0.123800000000000;
      double i_p_Ca = Ca_i*g_pCa/(Ca_i + K_pCa);
      double calcium_pump_current_i_Caitot = i_p_Ca;
      double K_NaCa = 1000;
      double K_sat = 0.100000000000000;
      double Km_Ca = 1.38000000000000;
      double Km_Nai = 87.5000000000000;
      double alpha = 2.50000000000000;
      double gamma = 0.350000000000000;
      double exp_gamma_VFRT = exp(F*V*gamma/(R*T));
      double exp_gamma_m1_VFRT = exp(F*V*(gamma - 1)/(R*T));
      double g_Na = 14.8380000000000;
      double g_K1 = 5.40500000000000;
      double K_o = 5.40000000000000;
      double g_pK = 0.0146000000000000;
      double _expensive_functions_019 = exp(-0.167224080267559*V);
      double i_p_K_term = (1.0/(65.4052157419383*_expensive_functions_019 + 1));
      double g_Kr = 0.153000000000000;
      double _expensive_functions_026 = log(K_o/K_i);
      double E_K = R*T*_expensive_functions_026/F;
      double P_kna = 0.0300000000000000;
      double __melodee_temp_002 = celltype == 1;
      double g_Ks;
      if (__melodee_temp_002)
      {
         g_Ks = 0.0980000000000000;
      }
      else
      {
         g_Ks = 0.392000000000000;
      }
      double g_bna = 0.000290000000000000;
      double Na_o = 140;
      double K_mNa = 40;
      double K_mk = 1;
      double P_NaK = 2.72400000000000;
      double _expensive_functions_031 = exp(-F*V/(R*T));
      double _expensive_functions_032 = exp(-0.1*F*V/(R*T));
      double i_NaK_term = K_o*P_NaK/((K_o + K_mk)*(0.0353*_expensive_functions_031 + 0.1245*_expensive_functions_032 + 1));
      double i_NaK = Na_i*i_NaK_term/(Na_i + K_mNa);
      double i_Naitot = 3*i_NaK;
      double i_Kitot = -2*i_NaK;
      double __melodee_temp_004 = celltype == 0;
      double g_to;
      if (__melodee_temp_004)
      {
         g_to = 0.0730000000000000;
      }
      else
      {
         g_to = 0.294000000000000;
      }
      double VEK = -E_K + V;
      double i_CaL = d*f*f2*fCass*g_CaL*(-Ca_o*i_CalTerm3 + 0.25*Ca_ss*i_CalTerm4);
      double L_type_Ca_current_i_Caitot = i_CaL;
      double i_NaCa = K_NaCa*(Ca_o + Km_Ca)*(K_sat*exp_gamma_m1_VFRT + 1)*((Na_i*Na_i*Na_i)*Ca_o*exp_gamma_VFRT - (Na_o*Na_o*Na_o)*Ca_i*alpha*exp_gamma_m1_VFRT)/((Na_o*Na_o*Na_o) + (Km_Nai*Km_Nai*Km_Nai));
      double sodium_calcium_exchanger_current_i_Naitot = 3*i_NaCa;
      double sodium_calcium_exchanger_current_i_Caitot = -2*i_NaCa;
      double _expensive_functions_035 = exp(0.06*VEK);
      double alpha_K1 = 0.1/(6.14421235332821e-6*_expensive_functions_035 + 1);
      double _expensive_functions_036 = exp(-0.5*VEK);
      double _expensive_functions_037 = exp(0.1*VEK);
      double _expensive_functions_038 = exp(0.0002*VEK);
      double beta_K1 = (0.367879441171442*_expensive_functions_037 + 3.06060402008027*_expensive_functions_038)/(_expensive_functions_036 + 1);
      double xK1_inf = alpha_K1/(alpha_K1 + beta_K1);
      double _expensive_functions_039 = sqrt(K_o);
      double i_K1 = 0.430331482911935*VEK*_expensive_functions_039*g_K1*xK1_inf;
      double inward_rectifier_potassium_current_i_Kitot = i_K1;
      double i_p_K = VEK*g_pK*i_p_K_term;
      double potassium_pump_current_i_Kitot = i_p_K;
      double _expensive_functions_040 = sqrt(K_o);
      double i_Kr = 0.430331482911935*VEK*_expensive_functions_040*Xr1*Xr2*g_Kr;
      double rapid_time_dependent_potassium_current_i_Kitot = i_Kr;
      double _expensive_functions_041 = log(Na_o/Na_i);
      double E_Na = R*T*_expensive_functions_041/F;
      double _expensive_functions_042 = log((K_o + Na_o*P_kna)/(K_i + Na_i*P_kna));
      double E_Ks = R*T*_expensive_functions_042/F;
      double i_Ks = (Xs*Xs)*g_Ks*(V - E_Ks);
      double slow_time_dependent_potassium_current_i_Kitot = i_Ks;
      double i_b_Na = g_bna*(-E_Na + V);
      double sodium_background_current_i_Naitot = i_b_Na;
      double i_to = VEK*g_to*r*s;
      double transient_outward_current_i_Kitot = i_to;
      double i_Kitot_001 = i_Kitot + inward_rectifier_potassium_current_i_Kitot + potassium_pump_current_i_Kitot + rapid_time_dependent_potassium_current_i_Kitot + slow_time_dependent_potassium_current_i_Kitot + transient_outward_current_i_Kitot;
      double i_Caitot_001 = L_type_Ca_current_i_Caitot + calcium_pump_current_i_Caitot + i_Caitot + sodium_calcium_exchanger_current_i_Caitot;
      double i_Na = (m*m*m)*g_Na*h*j*(-E_Na + V);
      double fast_sodium_current_i_Naitot = i_Na;
      double i_Naitot_001 = fast_sodium_current_i_Naitot + i_Naitot + sodium_background_current_i_Naitot + sodium_calcium_exchanger_current_i_Naitot;
      double Iion = i_Caitot_001 + i_Kitot_001 + i_Naitot_001;
      return Iion;
   }
   else if (varHandle == K_i_handle)
   {
      return K_i;
   }
   else if (varHandle == Na_i_handle)
   {
      return Na_i;
   }
   else if (varHandle == R_prime_handle)
   {
      return R_prime;
   }
   else if (varHandle == Xr1_handle)
   {
      return Xr1;
   }
   else if (varHandle == Xr2_handle)
   {
      return Xr2;
   }
   else if (varHandle == Xs_handle)
   {
      return Xs;
   }
   else if (varHandle == d_handle)
   {
      return d;
   }
   else if (varHandle == f_handle)
   {
      return f;
   }
   else if (varHandle == f2_handle)
   {
      return f2;
   }
   else if (varHandle == fCass_handle)
   {
      return fCass;
   }
   else if (varHandle == h_handle)
   {
      return h;
   }
   else if (varHandle == i_CaL_handle)
   {
      double F = 96485.3415000000;
      double R = 8314.47200000000;
      double T = 310;
      double Ca_o = 2;
      double g_CaL = 3.98000000000000e-5;
      double i_CalTerm1 = (F*F)*(4*V - 60)/(R*T);
      double i_CalTerm2 = exp(F*(2*V - 30)/(R*T));
      double i_CalTerm3 = i_CalTerm1/(i_CalTerm2 - 1);
      double i_CalTerm4 = i_CalTerm2*i_CalTerm3;
      double i_CaL = d*f*f2*fCass*g_CaL*(-Ca_o*i_CalTerm3 + 0.25*Ca_ss*i_CalTerm4);
      return i_CaL;
   }
   else if (varHandle == i_K1_handle)
   {
      double F = 96485.3415000000;
      double R = 8314.47200000000;
      double T = 310;
      double g_K1 = 5.40500000000000;
      double K_o = 5.40000000000000;
      double _expensive_functions_026 = log(K_o/K_i);
      double E_K = R*T*_expensive_functions_026/F;
      double VEK = -E_K + V;
      double _expensive_functions_035 = exp(0.06*VEK);
      double alpha_K1 = 0.1/(6.14421235332821e-6*_expensive_functions_035 + 1);
      double _expensive_functions_036 = exp(-0.5*VEK);
      double _expensive_functions_037 = exp(0.1*VEK);
      double _expensive_functions_038 = exp(0.0002*VEK);
      double beta_K1 = (0.367879441171442*_expensive_functions_037 + 3.06060402008027*_expensive_functions_038)/(_expensive_functions_036 + 1);
      double xK1_inf = alpha_K1/(alpha_K1 + beta_K1);
      double _expensive_functions_039 = sqrt(K_o);
      double i_K1 = 0.430331482911935*VEK*_expensive_functions_039*g_K1*xK1_inf;
      return i_K1;
   }
   else if (varHandle == i_Kr_handle)
   {
      double F = 96485.3415000000;
      double R = 8314.47200000000;
      double T = 310;
      double K_o = 5.40000000000000;
      double g_Kr = 0.153000000000000;
      double _expensive_functions_026 = log(K_o/K_i);
      double E_K = R*T*_expensive_functions_026/F;
      double VEK = -E_K + V;
      double _expensive_functions_040 = sqrt(K_o);
      double i_Kr = 0.430331482911935*VEK*_expensive_functions_040*Xr1*Xr2*g_Kr;
      return i_Kr;
   }
   else if (varHandle == i_Ks_handle)
   {
      double celltype = 0;
      double F = 96485.3415000000;
      double R = 8314.47200000000;
      double T = 310;
      double K_o = 5.40000000000000;
      double P_kna = 0.0300000000000000;
      double __melodee_temp_002 = celltype == 1;
      double g_Ks;
      if (__melodee_temp_002)
      {
         g_Ks = 0.0980000000000000;
      }
      else
      {
         g_Ks = 0.392000000000000;
      }
      double Na_o = 140;
      double _expensive_functions_042 = log((K_o + Na_o*P_kna)/(K_i + Na_i*P_kna));
      double E_Ks = R*T*_expensive_functions_042/F;
      double i_Ks = (Xs*Xs)*g_Ks*(V - E_Ks);
      return i_Ks;
   }
   else if (varHandle == i_Na_handle)
   {
      double F = 96485.3415000000;
      double R = 8314.47200000000;
      double T = 310;
      double g_Na = 14.8380000000000;
      double Na_o = 140;
      double _expensive_functions_041 = log(Na_o/Na_i);
      double E_Na = R*T*_expensive_functions_041/F;
      double i_Na = (m*m*m)*g_Na*h*j*(-E_Na + V);
      return i_Na;
   }
   else if (varHandle == i_NaCa_handle)
   {
      double F = 96485.3415000000;
      double R = 8314.47200000000;
      double T = 310;
      double Ca_o = 2;
      double K_NaCa = 1000;
      double K_sat = 0.100000000000000;
      double Km_Ca = 1.38000000000000;
      double Km_Nai = 87.5000000000000;
      double alpha = 2.50000000000000;
      double gamma = 0.350000000000000;
      double exp_gamma_VFRT = exp(F*V*gamma/(R*T));
      double exp_gamma_m1_VFRT = exp(F*V*(gamma - 1)/(R*T));
      double Na_o = 140;
      double i_NaCa = K_NaCa*(Ca_o + Km_Ca)*(K_sat*exp_gamma_m1_VFRT + 1)*((Na_i*Na_i*Na_i)*Ca_o*exp_gamma_VFRT - (Na_o*Na_o*Na_o)*Ca_i*alpha*exp_gamma_m1_VFRT)/((Na_o*Na_o*Na_o) + (Km_Nai*Km_Nai*Km_Nai));
      return i_NaCa;
   }
   else if (varHandle == i_NaK_handle)
   {
      double F = 96485.3415000000;
      double R = 8314.47200000000;
      double T = 310;
      double K_o = 5.40000000000000;
      double K_mNa = 40;
      double K_mk = 1;
      double P_NaK = 2.72400000000000;
      double _expensive_functions_031 = exp(-F*V/(R*T));
      double _expensive_functions_032 = exp(-0.1*F*V/(R*T));
      double i_NaK_term = K_o*P_NaK/((K_o + K_mk)*(0.0353*_expensive_functions_031 + 0.1245*_expensive_functions_032 + 1));
      double i_NaK = Na_i*i_NaK_term/(Na_i + K_mNa);
      return i_NaK;
   }
   else if (varHandle == i_b_Ca_handle)
   {
      double F = 96485.3415000000;
      double R = 8314.47200000000;
      double T = 310;
      double Ca_o = 2;
      double g_bca = 0.000592000000000000;
      double _expensive_functions_012 = log(Ca_o/Ca_i);
      double E_Ca = 0.5*R*T*_expensive_functions_012/F;
      double i_b_Ca = g_bca*(V - E_Ca);
      return i_b_Ca;
   }
   else if (varHandle == i_b_Na_handle)
   {
      double F = 96485.3415000000;
      double R = 8314.47200000000;
      double T = 310;
      double g_bna = 0.000290000000000000;
      double Na_o = 140;
      double _expensive_functions_041 = log(Na_o/Na_i);
      double E_Na = R*T*_expensive_functions_041/F;
      double i_b_Na = g_bna*(-E_Na + V);
      return i_b_Na;
   }
   else if (varHandle == i_leak_handle)
   {
      double V_leak = 0.000360000000000000;
      double i_leak = V_leak*(Ca_SR - Ca_i);
      return i_leak;
   }
   else if (varHandle == i_p_Ca_handle)
   {
      double K_pCa = 0.000500000000000000;
      double g_pCa = 0.123800000000000;
      double i_p_Ca = Ca_i*g_pCa/(Ca_i + K_pCa);
      return i_p_Ca;
   }
   else if (varHandle == i_p_K_handle)
   {
      double F = 96485.3415000000;
      double R = 8314.47200000000;
      double T = 310;
      double K_o = 5.40000000000000;
      double g_pK = 0.0146000000000000;
      double _expensive_functions_019 = exp(-0.167224080267559*V);
      double i_p_K_term = (1.0/(65.4052157419383*_expensive_functions_019 + 1));
      double _expensive_functions_026 = log(K_o/K_i);
      double E_K = R*T*_expensive_functions_026/F;
      double VEK = -E_K + V;
      double i_p_K = VEK*g_pK*i_p_K_term;
      return i_p_K;
   }
   else if (varHandle == i_rel_handle)
   {
      double EC = 1.50000000000000;
      double V_rel = 0.102000000000000;
      double k1_prime = 0.150000000000000;
      double k3 = 0.0600000000000000;
      double max_sr = 2.50000000000000;
      double min_sr = 1;
      double kcasr = max_sr - (max_sr - min_sr)/(1 + (EC*EC)/(Ca_SR*Ca_SR));
      double k1 = k1_prime/kcasr;
      double O = (Ca_ss*Ca_ss)*R_prime*k1/((Ca_ss*Ca_ss)*k1 + k3);
      double i_rel = O*V_rel*(Ca_SR - Ca_ss);
      return i_rel;
   }
   else if (varHandle == i_to_handle)
   {
      double celltype = 0;
      double F = 96485.3415000000;
      double R = 8314.47200000000;
      double T = 310;
      double K_o = 5.40000000000000;
      double _expensive_functions_026 = log(K_o/K_i);
      double E_K = R*T*_expensive_functions_026/F;
      double __melodee_temp_004 = celltype == 0;
      double g_to;
      if (__melodee_temp_004)
      {
         g_to = 0.0730000000000000;
      }
      else
      {
         g_to = 0.294000000000000;
      }
      double VEK = -E_K + V;
      double i_to = VEK*g_to*r*s;
      return i_to;
   }
   else if (varHandle == i_up_handle)
   {
      double K_up = 0.000250000000000000;
      double Vmax_up = 0.00637500000000000;
      double i_up = Vmax_up/(1 + (K_up*K_up)/(Ca_i*Ca_i));
      return i_up;
   }
   else if (varHandle == i_xfer_handle)
   {
      double V_xfer = 0.00380000000000000;
      double i_xfer = V_xfer*(-Ca_i + Ca_ss);
      return i_xfer;
   }
   else if (varHandle == j_handle)
   {
      return j;
   }
   else if (varHandle == m_handle)
   {
      return m;
   }
   else if (varHandle == r_handle)
   {
      return r;
   }
   else if (varHandle == s_handle)
   {
      return s;
   }
   return NAN;
}

void ThisReaction::getCheckpointInfo(vector<string>& fieldNames,
                                     vector<string>& fieldUnits) const
{
   fieldNames.clear();
   fieldUnits.clear();
   fieldNames.push_back("Ca_SR");
   fieldUnits.push_back(getUnit("Ca_SR"));
   fieldNames.push_back("Ca_i");
   fieldUnits.push_back(getUnit("Ca_i"));
   fieldNames.push_back("Ca_ss");
   fieldUnits.push_back(getUnit("Ca_ss"));
   fieldNames.push_back("K_i");
   fieldUnits.push_back(getUnit("K_i"));
   fieldNames.push_back("Na_i");
   fieldUnits.push_back(getUnit("Na_i"));
   fieldNames.push_back("R_prime");
   fieldUnits.push_back(getUnit("R_prime"));
   fieldNames.push_back("Xr1");
   fieldUnits.push_back(getUnit("Xr1"));
   fieldNames.push_back("Xr2");
   fieldUnits.push_back(getUnit("Xr2"));
   fieldNames.push_back("Xs");
   fieldUnits.push_back(getUnit("Xs"));
   fieldNames.push_back("d");
   fieldUnits.push_back(getUnit("d"));
   fieldNames.push_back("f");
   fieldUnits.push_back(getUnit("f"));
   fieldNames.push_back("f2");
   fieldUnits.push_back(getUnit("f2"));
   fieldNames.push_back("fCass");
   fieldUnits.push_back(getUnit("fCass"));
   fieldNames.push_back("h");
   fieldUnits.push_back(getUnit("h"));
   fieldNames.push_back("j");
   fieldUnits.push_back(getUnit("j"));
   fieldNames.push_back("m");
   fieldUnits.push_back(getUnit("m"));
   fieldNames.push_back("r");
   fieldUnits.push_back(getUnit("r"));
   fieldNames.push_back("s");
   fieldUnits.push_back(getUnit("s"));
}

}
