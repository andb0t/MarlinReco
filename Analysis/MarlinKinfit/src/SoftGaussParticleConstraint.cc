/*! \file 
 *  \brief Implements class SoftGaussParticleConstraint
 *
 * \b Changelog:
 * - 
 *
 * \b CVS Log messages:
 * - $Log: not supported by cvs2svn $
 * - Revision 1.4  2008/10/17 13:17:17  blist
 * - Avoid variable-size arrays
 * -
 * - Revision 1.3  2008/02/18 09:59:35  blist
 * - MomentumConstraint and SoftGaussMomentumCOnstraint added; PConstraint is obsolete
 * -
 * - Revision 1.2  2008/02/13 12:37:38  blist
 * - new file BaseFitter.cc
 * -
 * - Revision 1.1  2008/02/12 16:43:27  blist
 * - First Version of Soft Constraints
 * -
 * -
 */ 

#include "SoftGaussParticleConstraint.h"
#include "ParticleFitObject.h"
#include <iostream>
#include <cmath>
using namespace std;
SoftGaussParticleConstraint::SoftGaussParticleConstraint(double sigma_)
: sigma (sigma_)
{
  invalidateCache();
}

double SoftGaussParticleConstraint::getSigma() const
{
  return sigma;
}

double SoftGaussParticleConstraint::setSigma(double sigma_) 
{
  if (sigma_ != 0) sigma = std::abs(sigma_);
  return sigma;
}
    
double SoftGaussParticleConstraint::getChi2() const {
  double r = getValue()/getSigma();
  return r*r;
}
  
double SoftGaussParticleConstraint::getError() const {
  double dgdpi[4];
  double error2 = 0;
  for (unsigned int i = 0; i < fitobjects.size(); ++i) {
    const ParticleFitObject *foi = fitobjects[i];
    assert (foi);
    if (firstDerivatives (i, dgdpi)) {
      error2 += foi->getError2 (dgdpi);
    }
  }
  return std::sqrt(std::abs(error2));
}

/**
 * Calculates the second derivative of the constraint g w.r.t. the various parameters
 * and adds it to the global covariance matrix 
 *
 * We denote with P_i the 4-vector of the i-th ParticleFitObject,
 * then 
 * $$ \frac{\partial ^2 g}{\partial a_k \partial a_l}
 *   = \sum_i \sum_j \frac{\partial ^2 g}{\partial P_i \partial P_j} \cdot 
 *     \frac{\partial P_i}{\partial a_k} \cdot \frac{\partial P_j}{\partial a_l}
 *     + \sum_i \frac{\partial g}{\partial P_i} \cdot 
 *        \frac{\partial^2 P_i}{\partial a_k \partial a_l}
 * $$
 * Here, $\frac{\partial P_i}{\partial a_k}$ is a $4 \times n_i$ Matrix, where
 * $n_i$ is the number of parameters of FitObject i;
 * Correspondingly, $\frac{\partial^2 P_i}{\partial a_k \partial a_l}$ is a
 * $4 \times n_i \times n_i$ matrix.
 * Also, $\frac{\partial ^2 g}{\partial P_i \partial P_j}$ is a $4\times 4$ matrix
 * for a given i and j, and $\frac{\partial g}{\partial P_i}$ is a 4-vector
 * (though not a Lorentz-vector!).
 * 
 */
 
 
void SoftGaussParticleConstraint::add2ndDerivativesToMatrix (double *M, int idim) const
{

  /** First, treat the part 
   * $$
   *    \frac{\partial ^2 g}{\partial P_i \partial P_j}  \cdot 
   *     \frac{\partial P_i}{\partial a_k} \cdot \frac{\partial P_j}{\partial a_l}
   * $$
   */
  double s = getSigma();
  double fact = 2*getValue()/(s*s); 
   
  // Derivatives $\frac{\partial ^2 g}{\partial P_i \partial P_j}$ at fixed i, j
  // d2GdPidPj[4*ii+jj] is derivative w.r.t. P_i,ii and P_j,jj, where ii=0,1,2,3 for E,px,py,pz
  double d2GdPidPj[16];
  // Derivatives $\frac {\partial P_i}{\partial a_k}$ for all i; 
  // k is local parameter number
  // dPidAk[KMAX*4*i + 4*k + ii] is $\frac {\partial P_{i,ii}}{\partial a_k}$,
  // with ii=0, 1, 2, 3 for E, px, py, pz
  const int KMAX=4;
  const int n = fitobjects.size();
  double *dPidAk = new double[n*KMAX*4];
  bool *dPidAkval = new bool[n];
  
  for (int i = 0; i < n; ++i) dPidAkval[i] = false;
  
  // Derivatives $\frac{\partial ^2 g}{\partial P_i \partial a_l}$ at fixed i
  // d2GdPdAl[4*l + ii] is $\frac{\partial ^2 g}{\partial P_{i,ii} \partial a_l}$
  double d2GdPdAl[4*KMAX];
  // Derivatives $\frac{\partial ^2 g}{\partial a_k \partial a_l}$ 
  double d2GdAkdAl[KMAX*KMAX];
  
  // Global parameter numbers: parglobal[KMAX*i+klocal] 
  // is global parameter number of local parameter klocal of i-th Fit object
  int *parglobal = new int[KMAX*n];
  
  for (int i = 0; i < n; ++i) {
    const ParticleFitObject *foi = fitobjects[i];
    assert (foi);
    for (int klocal = 0; klocal < foi->getNPar(); ++klocal) {
      parglobal [KMAX*i+klocal] = foi->getGlobalParNum(klocal);
    }
  }
  
  
  for (int i = 0; i < n; ++i) {
    const ParticleFitObject *foi = fitobjects[i];
    assert (foi);
    for (int j = 0; j < n; ++j) {
      const ParticleFitObject *foj = fitobjects[j];
      assert (foj);
      if (secondDerivatives (i, j, d2GdPidPj)) {
        if (!dPidAkval[i]) {
          foi->getDerivatives (dPidAk+i*(KMAX*4), KMAX*4);
          dPidAkval[i] = true;
        }
        if (!dPidAkval[j]) {
          foj->getDerivatives (dPidAk+j*(KMAX*4), KMAX*4);
          dPidAkval[j] = true;
        }
        // Now sum over E/px/Py/Pz for object j:
        // $$\frac{\partial ^2 g}{\partial P_{i,ii} \partial a_l}
        //   = (sum_{j}) sum_{jj} frac{\partial ^2 g}{\partial P_{i,ii} \partial P_{j,jj}}  
        //     \cdot \frac{\partial P_{j,jj}}{\partial a_l}
        // We're summing over jj here
        for (int llocal = 0; llocal < foj->getNPar(); ++llocal) {
          for (int ii = 0; ii < 4; ++ii) {
            int ind1 = 4*ii;
            int ind2 = (KMAX*4)*j + 4*llocal;
            double& r = d2GdPdAl[4*llocal + ii];
            r  = d2GdPidPj[  ind1] * dPidAk[  ind2];   // E
            r += d2GdPidPj[++ind1] * dPidAk[++ind2];   // px
            r += d2GdPidPj[++ind1] * dPidAk[++ind2];   // py
            r += d2GdPidPj[++ind1] * dPidAk[++ind2];   // pz
          }
        }
        // Now sum over E/px/Py/Pz for object i, i.e. sum over ii:
        // $$
        // \frac{\partial ^2 g}{\partial a_k \partial a_l}
        //      = \sum_{ii} \frac{\partial ^2 g}{\partial P_{i,ii} \partial a_l} \cdot 
        //        \frac{\partial P_{i,ii}}{\partial a_k}
        // $$
        for (int klocal = 0; klocal < foi->getNPar(); ++klocal) {
          for (int llocal = 0; llocal < foj->getNPar(); ++llocal) {
            int ind1 = 4*llocal;
            int ind2 = (KMAX*4)*i + 4*klocal;
            double& r = d2GdAkdAl[KMAX*klocal+llocal];
            r  = d2GdPdAl[  ind1] * dPidAk[  ind2];    //E
            r += d2GdPdAl[++ind1] * dPidAk[++ind2];   // px
            r += d2GdPdAl[++ind1] * dPidAk[++ind2];   // py
            r += d2GdPdAl[++ind1] * dPidAk[++ind2];   // pz
          }
        }
        // Now expand the local parameter numbers to global ones
        for (int klocal = 0; klocal < foi->getNPar(); ++klocal) {
          int kglobal = parglobal [KMAX*i + klocal];
          for (int llocal = 0; llocal < foj->getNPar(); ++llocal) {
            int lglobal = parglobal [KMAX*j + llocal];
            M [idim*kglobal+lglobal] += fact*d2GdAkdAl[KMAX*klocal+llocal];
          }
        }
      }
    }
  }
  /** Second, treat the part 
   * $$
   * \sum_i \frac{\partial g}{\partial P_i} \cdot 
   *        \frac{\partial^2 P_i}{\partial a_k \partial a_l}
   * $$
   * Here, $\frac{\partial g}{\partial P_i}$ is a 4-vector, which we pass on to 
   * the FitObject
   */
  
  double *v = new double[idim];
  for (int i = 0; i < idim; ++i) v[i] = 0;
  double fact2 = sqrt(2.0)/s;
  
  double dgdpi[4];
  for (int i = 0; i < n; ++i) {
    const ParticleFitObject *foi = fitobjects[i];
    assert (foi);
    if (firstDerivatives (i, dgdpi)) {
      foi->addTo2ndDerivatives (M, idim, fact, dgdpi);
      foi->addToGlobalChi2DerVector (v, idim, fact2, dgdpi);
    }
  }
  
  for (int i = 0; i<idim; ++i) {
    if (double vi = v[i]) {
      int ioffs = i*idim;
      for (double *pvj = v; pvj < v+idim; ++pvj) {
        M[ioffs++] += vi*(*pvj);
      }
    }
  }
  
  
  delete[] dPidAk;
  delete[] dPidAkval;
  delete[] parglobal;
  delete[] v;
}

void SoftGaussParticleConstraint::addToGlobalChi2DerVector (double *y, int idim) const {
  double dgdpi[4];
  double s = getSigma();
  double r = 2*getValue()/(s*s);
  for (unsigned int i = 0; i < fitobjects.size(); ++i) {
    const ParticleFitObject *foi = fitobjects[i];
    assert (foi);
    if (firstDerivatives (i, dgdpi)) {
      foi->addToGlobalChi2DerVector (y, idim, r, dgdpi);
    }
  }
}

void SoftGaussParticleConstraint::test1stDerivatives () {
  cout << "SoftGaussParticleConstraint::test1stDerivatives for " << getName() << "\n";
  double y[100];
  for (int i = 0; i < 100; ++i) y[i]=0;
  addToGlobalChi2DerVector (y, 100);
  double eps = 0.00001;
  for (unsigned int ifo = 0; ifo < fitobjects.size(); ++ifo) {
    ParticleFitObject *fo = fitobjects[ifo];
    assert (fo);
    for (int ilocal = 0; ilocal < fo->getNPar(); ++ilocal) {
      int iglobal = fo->getGlobalParNum(ilocal);
      double calc = y[iglobal];
      double num = num1stDerivative (ifo, ilocal, eps);
      cout << "fo: " << fo->getName() << " par " << ilocal << "/" 
           << iglobal << " ("<< fo->getParamName(ilocal)
           << ") calc: " << calc << " - num: " << num << " = " << calc-num
           << endl;
    }
  }
}
void SoftGaussParticleConstraint::test2ndDerivatives () {
  cout << "SoftGaussParticleConstraint::test2ndDerivatives for " << getName() << "\n";
  const int idim=100;
  double *M = new double[idim*idim];
  for (int i = 0; i < idim*idim; ++i) M[i]=0;
  add2ndDerivativesToMatrix (M, idim);
  double eps = 0.0001;
  cout << "eps=" << eps << endl;
  
  for (unsigned int ifo1 = 0; ifo1 < fitobjects.size(); ++ifo1) {
    ParticleFitObject *fo1 = fitobjects[ifo1];
    assert (fo1);
    for (unsigned int ifo2 = ifo1; ifo2 < fitobjects.size(); ++ifo2) {
      ParticleFitObject *fo2 = fitobjects[ifo2];
      assert (fo2);
      for (int ilocal1 = 0; ilocal1 < fo1->getNPar(); ++ilocal1) {
        int iglobal1 = fo1->getGlobalParNum (ilocal1);
        for (int ilocal2 = (ifo1==ifo2 ? ilocal1 : 0); ilocal2 < fo2->getNPar(); ++ilocal2) {
          int iglobal2 = fo2->getGlobalParNum (ilocal2);
          double calc = M[idim*iglobal1 + iglobal2];
          double num = num2ndDerivative (ifo1, ilocal1, eps, ifo2, ilocal2, eps);
          cout << "fo1: " << fo1->getName() << " par " << ilocal1 << "/" 
               << iglobal1 << " ("<< fo1->getParamName(ilocal1)
               << "), fo2: " << fo2->getName() << " par " << ilocal2 << "/" 
               << iglobal2 << " ("<< fo2->getParamName(ilocal2)
               << ") calc: " << calc << " - num: " << num << " = " << calc-num
               << endl;
        }
      }
    }
  }
  delete[] M;
}


double SoftGaussParticleConstraint::num1stDerivative (int ifo, int ilocal, double eps) {
    ParticleFitObject *fo = fitobjects[ifo];
    assert (fo);
    double save = fo->getParam (ilocal);
    fo->setParam (ilocal, save+eps);
    double v1 = getChi2();
    fo->setParam (ilocal, save-eps);
    double v2 = getChi2();
    double result = (v1-v2)/(2*eps);
    fo->setParam (ilocal, save);
    return result;
}

double SoftGaussParticleConstraint::num2ndDerivative (int ifo1, int ilocal1, double eps1,
                                             int ifo2, int ilocal2, double eps2) {
  double result;

  if (ifo1 == ifo2 && ilocal1 == ilocal2) {
    ParticleFitObject *fo = fitobjects[ifo1];
    assert (fo);
    double save = fo->getParam (ilocal1);
    double v0 = getChi2();
    fo->setParam (ilocal1, save+eps1);
    double v1 = getChi2();
    fo->setParam (ilocal1, save-eps1);
    double v2 = getChi2();
    result = (v1+v2-2*v0)/(eps1*eps1);
    fo->setParam (ilocal1, save);
  }
  else {
    ParticleFitObject *fo1 = fitobjects[ifo1];
    assert (fo1);
    ParticleFitObject *fo2 = fitobjects[ifo2];
    assert (fo2);
    double save1 = fo1->getParam (ilocal1);
    double save2 = fo2->getParam (ilocal2);
    fo1->setParam (ilocal1, save1+eps1);
    fo2->setParam (ilocal2, save2+eps2);
    double v11 = getChi2();
    fo2->setParam (ilocal2, save2-eps2);
    double v12 = getChi2();
    fo1->setParam (ilocal1, save1-eps1);
    double v22 = getChi2();
    fo2->setParam (ilocal2, save2+eps2);
    double v21 = getChi2();
    result = (v11+v22-v12-v21)/(4*eps1*eps2);
    fo1->setParam (ilocal1, save1);
    fo2->setParam (ilocal2, save2);
  }
  return result;
}
