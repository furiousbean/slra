/** Layered Hankel structure with blockwise weights.
 * Implements 
 * \f$ \mathcal{H}_{{\bf m}, n} := 
 * \begin{bmatrix} \mathcal{H}_{m_1,n} \\ \vdots \\ \mathcal{H}_{m_q,n} 
 * \end{bmatrix} \f$ 
 * with blockwise weights 
 * \f$\begin{bmatrix} w_1 & \dots \\ w_q \end{bmatrix}\f$
 */

class LayeredHStructure : public StationaryStructure {
  typedef struct {
    size_t blocks_in_row;       /* Number of blocks in a row of Ci */
    double inv_w;            /* Square root of inverse of the weight */
  } Layer;

  int myQ;	                /* number of layers */
  size_t myM;
  size_t myNplusD;
  size_t myMaxLag;
  gsl_matrix **myA;
  Layer *mySA;	/* q-element array describing C1,...,Cq; */  

  void computeStats();
  void computeWkParams(); 
protected:
  int nvGetNp() const { return (myM - 1) * myQ + myNplusD; }  
public:
  /** Constructs Layered Hankel structure.
   * 
   */
  LayeredHStructure( const double *oldNk, /**< Array of \f$m_k\f$*/
                     size_t q,            /**< \f$q\f$ */   
                     int M,               /**< \f$n\f$ */  
                     const double *layer_w = NULL /**< Array of \f$w_k\f$*/ );
  virtual ~LayeredHStructure();

  /** @name Implementing Structure interface */
  /**@{*/
  virtual int getNplusD() const { return myNplusD; }
  virtual int getM() const { return myM; }
  virtual int getNp() const { return nvGetNp(); }
  virtual Cholesky *createCholesky( int D, double reg_gamma ) const;
  virtual DGamma *createDGamma( int D ) const;
  virtual void fillMatrixFromP( gsl_matrix* c, const gsl_vector* p ); 
  virtual void correctP( gsl_vector* p, gsl_matrix *R, gsl_vector *yr,
                         bool scaled = true );
  /**@}*/

  /** @name Implementing StationaryStructure interface */
  /**@{*/
  virtual int getS() const { return myMaxLag; }
  virtual void WkB( gsl_matrix *res, int k, const gsl_matrix *B ) const;
  virtual void AtWkB( gsl_matrix *res, int k, 
                      const gsl_matrix *A, const gsl_matrix *B, 
                      gsl_matrix *tmpWkB, double beta = 0 ) const;
  virtual void AtWkV( gsl_vector *res, int k,
                      const gsl_matrix *A, const gsl_vector *V, 
                      gsl_vector *tmpWkV, double beta = 0 ) const;
  /**@}*/


  /** @name LayeredHankel- specific methods */
  /**@{*/
  /** Returns a pointer to Wk matrix. @todo remove */
  const gsl_matrix *getWk( int l ) const { 
    return myA[l]; 
  } 
  /** Returns a pointer to Wk matrix. @todo remove */
  /* Returns \f$q\f$ */
  int getQ() const { return myQ; }
  /** Returns \f$\max \{m_l\}_{l=1}^{q}\f$ */
  int getMaxLag() const { return myMaxLag; }
  /** Returns \f$\max \{m_l\}\f$ */
  int getLayerLag( int l ) const { return mySA[l].blocks_in_row; }
  /** Checks whether \f$w_l=\infty\f$ (block is exact) */
  bool isLayerExact( int l ) const { return (mySA[l].inv_w == 0.0); }
  /** Returns \f$w_l^{-1}\f$ */
  double getLayerInvWeight( int l ) const { return mySA[l].inv_w; }
  /** Returns numper of parameters in each block: \f$m_l +n- 1\f$ */
  int getLayerNp( int l ) const { return getLayerLag(l) + getM() - 1; }
  /**@}*/
};


class MosaicHStructure : public StripedStructure {
  bool myWkIsCol;
protected:
  static Structure **allocStripe( gsl_vector *oldNk, gsl_vector *oldMl,  
                                  gsl_vector *Wk, bool wkIsCol = false );
public:
  MosaicHStructure( gsl_vector *oldNk, gsl_vector *oldMl,  
                    gsl_vector *Wk, bool wkIsCol = false );
  virtual ~MosaicHStructure() {}
  virtual Cholesky *createCholesky( int r, double reg_gamma ) const;
};


