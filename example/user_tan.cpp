// $Id$
/* --------------------------------------------------------------------------
CppAD: C++ Algorithmic Differentiation: Copyright (C) 2003-11 Bradley M. Bell

CppAD is distributed under multiple licenses. This distribution is under
the terms of the 
                    Common Public License Version 1.0.

A copy of this license is included in the COPYING file of this distribution.
Please visit http://www.coin-or.org/CppAD/ for information on other licenses.
-------------------------------------------------------------------------- */

/*
$begin user_tan.cpp$$
$spell
	Tanh
$$

$section Tan and Tanh as User Atomic Operations: Example and Test$$

$index tan, user_atomic$$
$index user, atomic tan$$
$index atomic, tan$$
$index test, user_atomic$$
$index user_atomic, example$$
$index example, user_atomic$$

$head Theory$$
The code below uses the $cref/tan_forward/$$ and $cref/tan_reverse/$$
to implement the tangent ($icode%id% == 0%$$) and hyperbolic tangent
($icode%id% == 1%$$) functions as user atomic operations.

$code
$verbatim%example/user_tan.cpp%0%// BEGIN PROGRAM%// END PROGRAM%1%$$
$$

$end
*/
// BEGIN PROGRAM
# include <cppad/cppad.hpp>

namespace { // Begin empty namespace 
	using CppAD::vector;

	// a utility to compute the union of two sets.
	void my_union(
		std::set<size_t>&         result  ,
		const std::set<size_t>&   left    ,
		const std::set<size_t>&   right   )
	{	std::set<size_t> temp;
		std::set_union(
			left.begin()              ,
			left.end()                ,
			right.begin()             ,
			right.end()               ,
			std::inserter(temp, temp.begin())
		);
		result.swap(temp);
	}

	// ----------------------------------------------------------------------
	// forward mode routine called by CppAD
	bool user_tan_forward(
		size_t                   id ,
		size_t                order ,
		size_t                    n ,
		size_t                    m ,
		const vector<bool>&      vx ,
		vector<bool>&           vzy ,
		const vector<float>&     tx ,
		vector<float>&          tzy
	)
	{
		assert( id == 0 || id == 1 );
		assert( n == 1 );
		assert( m == 2 );
		assert( tx.size() >= (order+1) * n );
		assert( tzy.size() >= (order+1) * m );

		size_t n_order = order + 1;
		size_t j = order;
		size_t k;

		// check if this is during the call to user_tan(id, ax, ay)
		if( vx.size() > 0 )
		{	assert( vx[0] == true  );
			assert( vx.size() >= n );
			assert( vzy.size() >= m );
			
			// now setvzy
			vzy[0] = true;
			vzy[1] = true;
		}

		if( j == 0 )
		{	// z^{(0)} = tan( x^{(0)} ) or tanh( x^{(0)} )
			if( id == 0 )
				tzy[0] = tan( tx[0] );
			else	tzy[0] = tanh( tx[0] );

			// y^{(0)} = z^{(0)} * z^{(0)}
			tzy[n_order + 0] = tzy[0] * tzy[0];
		}
		else
		{	float j_inv = 1.f / float(j);
			if( id == 1 )
				j_inv = - j_inv;

			// z^{(j)} = x^{(j)} +- sum_{k=1}^j k x^{(k)} y^{(j-k)} / j
			tzy[j] = tx[j];  
			for(k = 1; k <= j; k++)
				tzy[j] += tx[k] * tzy[n_order + j-k] * k * j_inv;

			// y^{(j)} = sum_{k=0}^j z^{(k)} z^{(j-k)}
			tzy[n_order + j] = 0.;
			for(k = 0; k <= j; k++)
				tzy[n_order + j] += tzy[k] * tzy[j-k];
		}
			
		// All orders are implemented and there are no possible errors
		return true;
	}
	// ----------------------------------------------------------------------
	// reverse mode routine called by CppAD
	bool user_tan_reverse(
		size_t                   id ,
		size_t                order ,
		size_t                    n ,
		size_t                    m ,
		const vector<float>&     tx ,
		const vector<float>&    tzy ,
		vector<float>&           px ,
		const vector<float>&    pzy
	)
	{
		assert( id == 0 || id == 1 );
		assert( n == 1 );
		assert( m == 2 );
		assert( tx.size() >= (order+1) * n );
		assert( tzy.size() >= (order+1) * m );
		assert( px.size() >= (order+1) * n );
		assert( pzy.size() >= (order+1) * m );

		size_t n_order = order + 1;
		size_t j, k;

		// copy because partials w.r.t. y and z need to change
		vector<float> qzy = pzy;

		// initialize accumultion of reverse mode partials
		for(k = 0; k < n_order; k++)
			px[k] = 0.;

		// eliminate positive orders
		for(j = order; j > 0; j--)
		{	float j_inv = 1.f / float(j);
			if( id == 1 )
				j_inv = - j_inv;

			// H_{x^{(k)}} += delta(j-k) +- H_{z^{(j)} y^{(j-k)} * k / j
			px[j] += qzy[j];
			for(k = 1; k <= j; k++)
				px[k] += qzy[j] * tzy[n_order + j-k] * k * j_inv;  

			// H_{y^{j-k)} += +- H_{z^{(j)} x^{(k)} * k / j
			for(k = 1; k <= j; k++)
				qzy[n_order + j-k] += qzy[j] * tx[k] * k * j_inv;  

			// H_{z^{(k)}} += H_{y^{(j-1)}} * z^{(j-k-1)} * 2. 
			for(k = 0; k < j; k++)
				qzy[k] += qzy[n_order + j-1] * tzy[j-k-1] * 2.f; 
		}

		// eliminate order zero
		if( id == 0 )
			px[0] += qzy[0] * (1.f + tzy[n_order + 0]);
		else
			px[0] += qzy[0] * (1.f - tzy[n_order + 0]);

		return true; 
	}
	// ----------------------------------------------------------------------
	// forward Jacobian sparsity routine called by CppAD
	bool user_tan_for_jac_sparse(
		size_t                               id ,             
		size_t                                n ,
		size_t                                m ,
		size_t                                q ,
		const vector< std::set<size_t> >&     r ,
		vector< std::set<size_t> >&           s )
	{
		assert( n == 1 );
		assert( m == 2 );
		assert( id == 0 || id == 1 );
		assert( r.size() >= n );
		assert( s.size() >= m );

		// sparsity for z and y are the same as for x
		s[0] = r[0];
		s[1] = r[0];

		return true;
	}
	// ----------------------------------------------------------------------
	// reverse Jacobian sparsity routine called by CppAD
	bool user_tan_rev_jac_sparse(
		size_t                               id ,             
		size_t                                n ,
		size_t                                m ,
		size_t                                q ,
		vector< std::set<size_t> >&           r ,
		const vector< std::set<size_t> >&     s )
	{
		assert( n == 1 );
		assert( m == 2 );
		assert( id == 0 || id == 1 );
		assert( r.size() >= n );
		assert( s.size() >= m );

		// note that, if the users code only uses z, and not y,
		// we could just set r[0] = s[0]	
		my_union(r[0], s[0], s[1]);
		return true; 
	}
	// ----------------------------------------------------------------------
	// reverse Hessian sparsity routine called by CppAD
	bool user_tan_rev_hes_sparse(
		size_t                               id ,             
		size_t                                n ,
		size_t                                m ,
		size_t                                q ,
		const vector< std::set<size_t> >&     r ,
		const vector<bool>&                   s ,
		vector<bool>&                         t ,
		const vector< std::set<size_t> >&     u ,
		vector< std::set<size_t> >&           v )
	{
		assert( n == 1 );
		assert( m == 2 );
		assert( id == 0 || id == 1 );
		assert( r.size() >= n );
		assert( s.size() >= m );
		assert( t.size() >= n );
		assert( u.size() >= m );
		assert( v.size() >= n );

		// back propogate Jacobian sparsity. If users code only uses z,
		// we could just set t[0] = s[0];
		t[0] =  s[0] | s[1];

		// back propogate Hessian sparsity, ...
		my_union(v[0], u[0], u[1]);

		// convert forward Jacobian sparsity to Hessian sparsity
		// because tan and tanh are nonlinear
		if( t[0] )
			my_union(v[0], v[0], r[0]);

		return true;
	}
	// ---------------------------------------------------------------------
	// Declare the AD<float> routine user_tan(id, ax, ay)
	CPPAD_USER_ATOMIC(
		user_tan                 , 
		CPPAD_TEST_VECTOR        ,
		float                    , 
		user_tan_forward         , 
		user_tan_reverse         ,
		user_tan_for_jac_sparse  ,
		user_tan_rev_jac_sparse  ,
		user_tan_rev_hes_sparse  
	)
} // End empty namespace

bool user_tan(void)
{	bool ok = true;
	using CppAD::AD;
	using CppAD::NearEqual;
	float eps = 10.f * CppAD::epsilon<float>();

	// domain space vector
	size_t n  = 1;
	float  x0 = 0.5;
	CPPAD_TEST_VECTOR< AD<float> > ax(n);
	ax[0]     = x0;

	// declare independent variables and start tape recording
	CppAD::Independent(ax);

	// range space vector 
	size_t m = 3;
	CPPAD_TEST_VECTOR< AD<float> > af(m);

	// temporary vector for user_tan computations
	// (user_tan computes tan or tanh and its square)
	CPPAD_TEST_VECTOR< AD<float> > az(2);

	// call user tan function and store tan(x) in f[0] (ignore tan(x)^2)
	size_t id = 0;
	user_tan(id, ax, az);
	af[0] = az[0];

	// call user tanh function and store tanh(x) in f[1] (ignore tanh(x)^2)
	id = 1;
	user_tan(id, ax, az);
	af[1] = az[0];

	// put a constant in f[2] (for sparsity pattern testing)
	af[2] = 1.; 

	// create f: x -> f and stop tape recording
	CppAD::ADFun<float> F(ax, af); 

	// check value 
	float tan = std::tan(x0);
	ok &= NearEqual(af[0] , tan,  eps, eps);
	float tanh = std::tanh(x0);
	ok &= NearEqual(af[1] , tanh,  eps, eps);

	// compute first partial of f w.r.t. x[0] using forward mode
	CPPAD_TEST_VECTOR<float> dx(n), df(m);
	dx[0] = 1.;
	df    = F.Forward(1, dx);

	// compute derivative of tan - tanh using reverse mode
	CPPAD_TEST_VECTOR<float> w(m), dw(n);
	w[0]  = 1.;
	w[1]  = 1.;
	dw    = F.Reverse(1, w);

	// tan'(x)   = 1 + tan(x)  * tan(x) 
	// tanh'(x)  = 1 - tanh(x) * tanh(x) 
	float tanp  = 1.f + tan * tan; 
	float tanhp = 1.f - tanh * tanh; 
	ok   &= NearEqual(df[0], tanp, eps, eps);
	ok   &= NearEqual(df[1], tanhp, eps, eps);
	ok   &= NearEqual(dw[0], w[0]*tanp + w[1]*tanhp, eps, eps);

	// compute second partial of f w.r.t. x[0] using forward mode
	CPPAD_TEST_VECTOR<float> ddx(n), ddf(m);
	ddx[0] = 0.;
	ddf    = F.Forward(2, ddx);

	// compute second derivative of tan - tanh using reverse mode
	CPPAD_TEST_VECTOR<float> ddw(2);
	ddw   = F.Reverse(2, w);

	// tan''(x)   = 2 *  tan(x) * tan'(x) 
	// tanh''(x)  = - 2 * tanh(x) * tanh'(x) 
	// Note that second order Taylor coefficient for u half the
	// corresponding second derivative.
	float two    = 2;
	float tanpp  =   two * tan * tanp;
	float tanhpp = - two * tanh * tanhp;
	ok   &= NearEqual(two * ddf[0], tanpp, eps, eps);
	ok   &= NearEqual(two * ddf[1], tanhpp, eps, eps);
	ok   &= NearEqual(ddw[0], w[0]*tanp  + w[1]*tanhp , eps, eps);
	ok   &= NearEqual(ddw[1], w[0]*tanpp + w[1]*tanhpp, eps, eps);

	// Forward mode computation of sparsity pattern for F.
	size_t q = n;
	// user vectorBool because m and n are small
	CppAD::vectorBool r1(q), s1(m * q);
	r1[0] = true;            // propogate sparsity for x[0]
	s1    = F.ForSparseJac(q, r1);
	ok  &= (s1[0] == true);  // f[0] depends on x[0]
	ok  &= (s1[1] == true);  // f[1] depends on x[0]
	ok  &= (s1[2] == false); // f[2] does not depend on x[0]

	// Reverse mode computation of sparsity pattern for F.
	size_t p = m;
	CppAD::vectorBool s2(p * m), r2(p * n);
	// Sparsity pattern for identity matrix
	size_t i, j;
	for(i = 0; i < p; i++)
	{	for(j = 0; j < m; j++)
			s2[i * p + j] = (i == j);
	}
	r2   = F.RevSparseJac(p, s2);
	ok  &= (r2[0] == true);  // f[0] depends on x[0]
	ok  &= (r2[1] == true);  // f[1] depends on x[0]
	ok  &= (r2[2] == false); // f[2] does not depend on x[0]

	// Hessian sparsity for f[0]
	CppAD::vectorBool s3(m), h(q * n);
	s3[0] = true;
	s3[1] = false;
	s3[2] = false;
	h    = F.RevSparseHes(q, s3);
	ok  &= (h[0] == true);  // Hessian is non-zero

	// Hessian sparsity for f[2]
	s3[0] = false;
	s3[2] = true;
	h    = F.RevSparseHes(q, s3);
	ok  &= (h[0] == false);  // Hessian is zero

	// check tanh results for a large value of x
	CPPAD_TEST_VECTOR<float> x(n), f(m);
	x[0]  = std::numeric_limits<float>::max() / two;
	f     = F.Forward(0, x);
	tanh  = 1.;
	ok   &= NearEqual(f[1], tanh, eps, eps);
	df    = F.Forward(1, dx);
	tanhp = 0.;
	ok   &= NearEqual(df[1], tanhp, eps, eps);
 
	// --------------------------------------------------------------------
	// Free all temporary work space associated with user_atomic objects. 
	// (If there are future calls to user atomic functions, they will 
	// create new temporary work space.)
	CppAD::user_atomic<float>::clear();

	return ok;
}
// END PROGRAM