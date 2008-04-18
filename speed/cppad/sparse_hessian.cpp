/* --------------------------------------------------------------------------
CppAD: C++ Algorithmic Differentiation: Copyright (C) 2003-08 Bradley M. Bell

CppAD is distributed under multiple licenses. This distribution is under
the terms of the 
                    Common Public License Version 1.0.

A copy of this license is included in the COPYING file of this distribution.
Please visit http://www.coin-or.org/CppAD/ for information on other licenses.
-------------------------------------------------------------------------- */
/*
$begin cppad_sparse_hessian.cpp$$
$spell
	CppAD
	cppad
	hpp
	bool
	typedef
	endif
	tmp
	std
$$

$section CppAD Speed: Sparse Hessian$$

$index cppad, speed sparse Hessian$$
$index speed, cppad sparse Hessian$$
$index Hessian, sparse speed cppad$$
$index sparse, Hessian speed cppad$$

$head Operation Sequence$$
Note that the 
$cref/operation sequence/glossary/Operation/Sequence/$$
depends on the vectors $italic i$$ and $italic j$$.
Hence we use a different $cref/ADFun/$$ object for 
each choice of $italic i$$ and $italic j$$.

$head CppAD Sparse Hessian$$
If the preprocessor symbol $code USE_CPPAD_SPARSE_HESSIAN$$ is 
true, the routine $cref/SparseHessian/sparse_hessian/$$ 
is used for the calculation.
Otherwise, the routine $cref/Hessian/$$ is used.

$head compute_sparse_hessian$$
$index compute_sparse_hessian$$
Routine that computes the gradient of determinant using CppAD:
$codep */
# include <cppad/cppad.hpp>
# include <cppad/speed/uniform_01.hpp>

// value can be true or false
# define USE_CPPAD_SPARSE_HESSIAN  1

bool compute_sparse_hessian(
	size_t                     size     , 
	size_t                     repeat   , 
	size_t                     ell      ,
	CppAD::vector<size_t>     &i        ,
	CppAD::vector<size_t>     &j        ,
	CppAD::vector<double>     &hessian  )
{
	// -----------------------------------------------------
	// setup
	using CppAD::AD;
	typedef CppAD::vector<double>       DblVector;
	typedef CppAD::vector< AD<double> > ADVector;
	typedef CppAD::vector<size_t>       SizeVector;

	size_t m = 1;             // number of dependent variables
	size_t n = size;          // number of independent variables
	ADVector   X(n);          // AD domain space vector
	ADVector   Y(m);          // AD range space vector
	DblVector  x(n);          // double domain space vector
	DblVector  w(m);          // double range space vector
	DblVector tmp(2 * ell);   // double temporary vector

	
	// choose a value for x (does not matter because f is sparse_hessian)
	CppAD::uniform_01(n, x);
	size_t k;
	for(k = 0; k < n; k++)
		X[k] = x[k];

	// weights for hessian calculation (only one component of f)
	w[0] = 1.;

	// ------------------------------------------------------
	while(repeat--)
	{
		// get the next set of indices
		CppAD::uniform_01(2 * ell, tmp);
		for(k = 0; k < ell; k++)
		{	i[k] = size_t( n * tmp[k] );
			i[k] = std::min(n-1, i[k]);
			//
			j[k] = size_t( n * tmp[k + ell] );
			j[k] = std::min(n-1, j[k]);
		}

		// declare independent variables
		Independent(X);	

		// AD computation of f(x)
		Y[0] = 0.;
		for(k = 0; k < ell; k++)
			Y[0] += X[i[k]] * X[j[k]];

		// create function object f : X -> Y
		CppAD::ADFun<double> f(X, Y);

		// evaluate and return the hessian of f
# if USE_CPPAD_SPARSE_HESSIAN
		hessian = f.SparseHessian(x, w);
# else
		hessian = f.Hessian(x, w);
# endif
	}
	return true;
}
/* $$
$end
*/