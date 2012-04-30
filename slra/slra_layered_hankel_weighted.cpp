#include <limits>
#include <memory.h>
#include <math.h>
extern "C" {
#include <gsl/gsl_vector.h>
#include <gsl/gsl_matrix.h>
#include <gsl/gsl_errno.h>
#include <gsl/gsl_blas.h>
#include <gsl/gsl_math.h>
}
#include "slra.h"

WLayeredHStructure::WLayeredHStructure( const double *oldNk, size_t q, int M, 
    const gsl_vector *weights ) : myBase(oldNk, q, M, NULL) {
  myInvWeights = gsl_vector_alloc(myBase.getNp());

  if (weights != NULL) {
    for (size_t l = 0; l < myInvWeights->size; l++) {
      if (!(gsl_vector_get(weights, l) > 0)) {
        throw new Exception("Value of weight not supported: %lf\n", 
                            gsl_vector_get(weights, l));
      }
      gsl_vector_set(myInvWeights, l, (1 / gsl_vector_get(weights, l)));
    }
  } else {
    gsl_vector_set_all(myInvWeights, 1);
  }

  print_vec(myInvWeights);
}

WLayeredHStructure::~WLayeredHStructure() {
  gsl_vector_free(myInvWeights);
}

void WLayeredHStructure::correctP( gsl_vector* p, gsl_matrix *R, 
                                   gsl_vector *yr ) {
  size_t l, k, sum_np = 0, sum_nl = 0, p_len;
  gsl_matrix yr_matr = gsl_matrix_view_vector(yr, getM(), R->size2).matrix;
  gsl_vector yr_matr_row;
  gsl_vector_view res_sub, p_chunk_sub, inv_w_chunk;
  gsl_vector *res = gsl_vector_alloc(R->size1);

  print_vec(myInvWeights);

  for (k = 0; k < getM(); k++) {
    yr_matr_row = gsl_matrix_row(&yr_matr, k).vector; 
    gsl_blas_dgemv(CblasNoTrans, 1.0, R,  &yr_matr_row, 0.0, res); 
    
    for (l = 0, sum_np = 0, sum_nl = 0; l < getQ(); 
         sum_np += getLayerNp(l), sum_nl += getLayerLag(l), ++l) {
      res_sub = gsl_vector_subvector(res, sum_nl, getLayerLag(l));
      p_chunk_sub =  gsl_vector_subvector(p, k + sum_np, getLayerLag(l));
      inv_w_chunk = gsl_vector_subvector(myInvWeights, k + sum_np, 
                                         getLayerLag(l));
      gsl_vector_mul(&res_sub.vector, &inv_w_chunk.vector);              
      gsl_vector_sub(&p_chunk_sub.vector, &res_sub.vector); 
    }
  }

  gsl_vector_free(res);
}

void WLayeredHStructure::mulInvWij( gsl_matrix *matr, int i  ) const {
  size_t sum_np = i, sum_nl = 0, l, k;
  gsl_vector matr_row;

  for (l = 0; l < getQ(); sum_np += getLayerNp(l), ++l) {
    for (k = 0; k < getLayerLag(l); ++k, ++sum_nl) {
      matr_row = gsl_matrix_row(matr, sum_nl).vector;
      gsl_vector_scale(&matr_row, getInvWeights(sum_np + k));
    }
  }
}

void WLayeredHStructure::WijB( gsl_matrix *res, int i, int j, 
         const gsl_matrix *B ) const {
  gsl_matrix_memcpy(res, B);
  if (j >= i) {
    gsl_blas_dtrmm(CblasLeft, CblasLower, CblasNoTrans, CblasNonUnit, 1.0, 
        myBase.getWk(j-i), res);
    mulInvWij(res, i);
  } else {
    mulInvWij(res, i);
    gsl_blas_dtrmm(CblasLeft, CblasLower, CblasTrans, CblasNonUnit, 1.0, 
        myBase.getWk(i-j), res);
  }
}

void WLayeredHStructure::AtWijB( gsl_matrix *res, int i, int j, 
         const gsl_matrix *A, const gsl_matrix *B, gsl_matrix *tmpWijB, 
         double beta ) const {
  gsl_matrix_scale(res, beta);
  size_t sum_np, sum_nl = 0;
  int l, k;

  if (i > j) {
    int tmp;
    const gsl_matrix* C;
    tmp = i; i = j; j = tmp;
    C = A; A = B; B = C;  
  }

  int diff = j - i;
  sum_nl = 0;
  for (l = 0, sum_np = j; l < getQ(); 
       sum_np += getLayerNp(l), sum_nl += getLayerLag(l), ++l) {
    for (k = 0; k < ((int)getLayerLag(l)) - diff; ++k) {
      const gsl_vector A_row = gsl_matrix_const_row(A, sum_nl+k+diff).vector;
      const gsl_vector B_row = gsl_matrix_const_row(B, sum_nl+k).vector;
      gsl_blas_dger(getInvWeights(sum_np + k), &A_row, &B_row, res);
    }
  }
}   

void WLayeredHStructure::AtWijV( gsl_vector *res, int i, int j, 
         const gsl_matrix *A, const gsl_vector *V, 
                     gsl_vector *tmpWijV, double beta ) const {
  gsl_vector_scale(res, beta);
  
  size_t sum_np, sum_nl = 0;
  int l, k;

  if (j >= i) {
    int diff = j - i;
    sum_nl = 0;
    for (l = 0, sum_np = j; l < getQ(); 
         sum_np += getLayerNp(l), sum_nl += getLayerLag(l), ++l) {
      for (k = 0; k < ((int)getLayerLag(l)) - diff; ++k) {
        const gsl_vector A_row = gsl_matrix_const_row(A, 
                                     sum_nl + k + diff).vector;
        gsl_blas_daxpy(getInvWeights(sum_np + k) * gsl_vector_get(V, sum_nl + k), 
            &A_row, res);
      }
    }
  } else {
    sum_nl = 0;
    int diff = i - j; 
    for (l = 0, sum_np = i; l < getQ(); 
         sum_np += getLayerNp(l), sum_nl += getLayerLag(l), ++l) {
      for (k = 0; k < ((int)getLayerLag(l)) - diff; ++k) {
        const gsl_vector A_row = gsl_matrix_const_row(A, sum_nl + k).vector;
        gsl_blas_daxpy(getInvWeights(sum_np + k) * 
                       gsl_vector_get(V, sum_nl + k + diff), &A_row, res);
      }
    }
  }
}   

Cholesky *WLayeredHStructure::createCholesky( int D, double reg_gamma ) const {
  return new SDependentCholesky(this, D, reg_gamma);
}

DGamma *WLayeredHStructure::createDGamma( int D ) const {
  return new SDependentDGamma(this, D);
}


typedef Structure* pStructure;

pStructure *WMosaicHStructure::allocStripe( gsl_vector *oldNk, gsl_vector *oldMl,  
                gsl_vector *Wk )  {
  pStructure *res = new pStructure[oldMl->size];

  for (size_t k = 0; k < oldMl->size; k++) {
    res[k] = new WLayeredHStructure(oldNk->data, oldNk->size, oldMl->data[k], Wk);
    if (Wk != NULL) {
      Wk->data += res[k]->getNp();
    }
  }
  return res;
}

WMosaicHStructure::WMosaicHStructure( gsl_vector *oldNk, gsl_vector *oldMl,  
                gsl_vector *Wk ) :
        StripedStructure(oldMl->size, allocStripe(oldNk, oldMl, Wk)) {
}


