/* $Id$ */
# ifndef CPPAD_OMP_ALLOC_INCLUDED
# define CPPAD_OMP_ALLOC_INCLUDED

/* --------------------------------------------------------------------------
CppAD: C++ Algorithmic Differentiation: Copyright (C) 2003-11 Bradley M. Bell

CppAD is distributed under multiple licenses. This distribution is under
the terms of the 
                    Common Public License Version 1.0.

A copy of this license is included in the COPYING file of this distribution.
Please visit http://www.coin-or.org/CppAD/ for information on other licenses.
-------------------------------------------------------------------------- */

# include <sstream>
# include <limits>
# include <memory>

# ifdef _OPENMP
# include <omp.h>
# endif

# ifdef _MSC_VER
// Supress warning that Microsoft compiler changed its behavior and is now 
// doing the correct thing at the statement:
//			new(array + i) Type();
# pragma warning(disable:4345)
# endif

# include <cppad/local/cppad_assert.hpp>
# include <cppad/local/define.hpp>
CPPAD_BEGIN_NAMESPACE
/*!
\file omp_alloc.hpp
File used to define the CppAD OpenMP allocator class
*/

/*!
\def CPPAD_MAX_NUM_CAPACITY
Maximum number of different capacities the allocator will attempt.
This must be larger than the log base two of numeric_limit<size_t>::max().
*/
# define CPPAD_MAX_NUM_CAPACITY 100

/*!
\def CPPAD_MIN_DOUBLE_CAPACITY
Minimum number of double values that will fit in an allocation.
*/
# define CPPAD_MIN_DOUBLE_CAPACITY 16

/*!
\def CPPAD_TRACE_CAPACITY
If NDEBUG is not defined, print all calls to \c get_memory and \c return_memory
that correspond to this capacity and thread CPPAD_TRACE_THEAD.
(Note that if CPPAD_TRACE_CAPACITY is zero, or any other value not in the list
of capacities, no tracing will be done.)
*/
# define CPPAD_TRACE_CAPACITY 0

/*!
\def CPPAD_TRACE_THREAD
If NDEBUG is not defined, print all calls to \c get_memory and \c return_memory
that correspond to this thead and capacity CPPAD_TRACE_CAPACITY.
*/
# define CPPAD_TRACE_THREAD 0

/*
Note that Section 3.6.2 of ISO/IEC 14882:1998(E) states: "The storage for 
objects with static storage duration (3.7.1) shall be zero-initialized
(8.5) before any other initialization takes place."
*/

/*
Capacity vector for memory allocation block sizes.
*/

class omp_alloc_capacity {
public:
	size_t number;
	size_t value[CPPAD_MAX_NUM_CAPACITY];
	omp_alloc_capacity(void)
	{
# ifdef _OPENMP
		CPPAD_ASSERT_UNKNOWN( ! omp_in_parallel() );
# endif
		number           = 0;
		size_t capacity  = CPPAD_MIN_DOUBLE_CAPACITY * sizeof(double);
		while( capacity < std::numeric_limits<size_t>::max() / 2 )
		{	CPPAD_ASSERT_UNKNOWN( number < CPPAD_MAX_NUM_CAPACITY );
			value[number++] = capacity;
			// next capactiy is 3/2 times the current one
			capacity        = 3 * ( (capacity + 1) / 2 );
		} 		 
		CPPAD_ASSERT_UNKNOWN( number > 0 );
	}
};

/*!
Allocator class that works well with an OpenMP multi-threading environment.
*/
class omp_alloc{
// ============================================================================
private:
	/// extra information (currently only used by create and delete array)
	size_t             extra_;
	/// index in the root_list correspondinig to a memory allocation
	size_t             index_;
	/// pointer to the next memory allocation with the same index
	void*              next_;
	// ---------------------------------------------------------------------
	/// make default constructor private. It is only used by the constructor
	/// for \c root arrays below.
	omp_alloc(void) : extra_(0), index_(0), next_(0) 
	{ }
	// ---------------------------------------------------------------------
	static const omp_alloc_capacity* capacity_info(void)
	{	static omp_alloc_capacity capacity;
		return &capacity;
	}
	// ---------------------------------------------------------------------
	/// number of bytes of memory that are currently in use for each thread
	static size_t* inuse_vector(void)
	{	static size_t inuse[CPPAD_MAX_NUM_THREADS];
		return inuse;
	}
	// ---------------------------------------------------------------------
	/// number of bytes that are currrently available for each thread; i.e.,
	/// have been obtained for each thread and not yet returned to the system.
	static size_t* available_vector(void)
	{	static size_t available[CPPAD_MAX_NUM_THREADS];
		return available;
	}

	// -----------------------------------------------------------------------
	/*!
 	Increase the number of bytes of memory that are currently in use; i.e.,
	that been obtained with \c get_memory and not yet returned. 

	\param inc [in]
	amount to increase memory in use.

	\param thread [in]
	Thread for which we are increasing the number of bytes in use
	(must be < CPPAD_MAX_NUM_THREADS).
	Durring parallel execution, this must be the thread 
	that is currently executing.
	*/
	static void inc_inuse(size_t inc, size_t thread)
	{	
		CPPAD_ASSERT_UNKNOWN( thread < CPPAD_MAX_NUM_THREADS);
		CPPAD_ASSERT_UNKNOWN( 
			thread == get_thread_num() || (! in_parallel()) 
		);
		size_t* inuse_vec = inuse_vector();
		
		// do the addition
		size_t result = inuse_vec[thread] + inc;
		CPPAD_ASSERT_UNKNOWN( result >= inuse_vec[thread] );

		inuse_vec[thread] = result;
	}
	// -----------------------------------------------------------------------
	/*!
 	Increase the number of bytes of memory that are currently avaialble; i.e.,
	have been obtained obtained from the system and are being held future use.

	\copydetails inc_inuse
	*/
	static void inc_available(size_t inc, size_t thread)
	{	
		CPPAD_ASSERT_UNKNOWN( thread < CPPAD_MAX_NUM_THREADS);
		CPPAD_ASSERT_UNKNOWN( 
			thread == get_thread_num() || (! in_parallel()) 
		);
		size_t* available_vec = available_vector();
		// do the addition
		size_t result = available_vec[thread] + inc;
		CPPAD_ASSERT_UNKNOWN( result >= available_vec[thread] );

		available_vec[thread] = result;
	}
	// -----------------------------------------------------------------------
	/*!
 	Decrease the number of bytes of memory that are currently in use; i.e.,
	that been obtained with \c get_memory and not yet returned. 

	\param dec [in]
	amount to decrease number of bytes in use.

	\param thread [in]
	Thread for which we are decreasing the number of bytes in use
	(must be < CPPAD_MAX_NUM_THREADS).
	Durring parallel execution, this must be the thread 
	that is currently executing.
	*/
	static void dec_inuse(size_t dec, size_t thread)
	{	
		CPPAD_ASSERT_UNKNOWN( thread < CPPAD_MAX_NUM_THREADS);
		CPPAD_ASSERT_UNKNOWN( 
			thread == get_thread_num() || (! in_parallel()) 
		);
		size_t* inuse_vec = inuse_vector();
		// do the subtraction
		CPPAD_ASSERT_UNKNOWN( inuse_vec[thread] >= dec );
		inuse_vec[thread] = inuse_vec[thread] - dec;
	}
	// -----------------------------------------------------------------------
	/*!
 	Decrease the number of bytes of memory that are currently avaialble; i.e.,
	have been obtained obtained from the system and are being held future use.

	\copydetails dec_inuse
	*/
	static void dec_available(size_t dec, size_t thread)
	{	
		CPPAD_ASSERT_UNKNOWN( thread < CPPAD_MAX_NUM_THREADS);
		CPPAD_ASSERT_UNKNOWN( 
			thread == get_thread_num() || (! in_parallel()) 
		);
		size_t* available_vec = available_vector();
		// do the subtraction
		CPPAD_ASSERT_UNKNOWN( available_vec[thread] >= dec );
		available_vec[thread] =  available_vec[thread] - dec;
	}

	// ----------------------------------------------------------------------
	/// Vector of length CPPAD_MAX_NUM_THREADS times CPPAD_MAX_NUM_CAPACITIES 
	/// for use as root nodes of inuse lists.
	static omp_alloc* root_inuse(void)
	{	static omp_alloc  
		root[CPPAD_MAX_NUM_THREADS * CPPAD_MAX_NUM_CAPACITY];
		return root;
	}

	// ----------------------------------------------------------------------
	/// Vector of length CPPAD_MAX_NUM_THREADS times CPPAD_MAX_NUM_CAPACITIES 
	/// for use as root nodes of available lists.
	static omp_alloc* root_available(void)
	{	static omp_alloc  
		root[CPPAD_MAX_NUM_THREADS * CPPAD_MAX_NUM_CAPACITY];
		return root;
	}

// ============================================================================
public:
/* -----------------------------------------------------------------------
$begin get_thread_num$$
$spell
	CppAD
	num
	omp_alloc
	cppad.hpp
$$

$section Get the Current OpenMP Thread Number$$

$index get_thread_num, omp_alloc$$
$index omp_alloc, get_thread_num$$
$index thread, current$$
$index current, thread$$

$head Syntax$$
$icode%thread% = omp_alloc::get_thread_num()%$$

$head Purpose$$
Some of the $cref/omp_alloc/$$ allocation routines have a thread number.
This routine enables you to determine the current thread.

$head thread$$
The return value $icode thread$$ is the currently executing thread number.
If $code _OPENMP$$ is not defined, $icode thread$$ is zero.

$head Example$$
$cref/omp_alloc.cpp/$$

$end
*/
	/// Get current OpenMP thread number (zero if _OpenMP not defined).
	static size_t get_thread_num(void)
	{
# ifdef _OPENMP
		size_t thread = static_cast<size_t>( omp_get_thread_num() );
		CPPAD_ASSERT_KNOWN(
			thread < CPPAD_MAX_NUM_THREADS,
			"more than CPPAD_MAX_NUM_THREADS are running"
		);
		return thread;
# else
		return 0;
# endif
	}

/* -----------------------------------------------------------------------
$begin in_parallel$$

$section Is The Current Execution in OpenMP Parallel Mode$$
$spell
	omp_alloc
	bool
$$

$index in_parallel, omp_alloc$$
$index omp_alloc, in_parallel$$
$index parallel, execution$$
$index execution, parallel$$
$index sequential, execution$$

$head Syntax$$
$icode%flag% = omp_alloc::in_parallel()%$$

$head Purpose$$
Some of the $cref/omp_alloc/$$ allocation routines have different
specifications for parallel (not sequential) execution mode.
This routine enables you to determine if the current execution mode
is sequential or parallel.

$head flag$$
The return value has prototype
$codei%
	bool %flag%
%$$
It is true if the current execution is in parallel mode 
(possibly multi-threaded) and false otherwise (sequential mode).

$head Example$$
$cref/omp_alloc.cpp/$$

$end
*/
	/// Are we in a parallel execution state; i.e., is it possible that
	/// other threads are currently executing.
	static bool in_parallel(void)
	{
# ifdef _OPENMP
		return static_cast<bool>( omp_in_parallel() );
# else
		return false;
# endif
	}
/* -----------------------------------------------------------------------
$begin get_memory$$
$spell
	num
	ptr
	omp_alloc
$$

$section Get At Least A Specified Amount of Memory$$

$index get_thread_num, omp_alloc$$
$index omp_alloc, get_thread_num$$
$index memory, allocate$$
$index allocate, memory$$

$head Syntax$$
$icode%v_ptr% = omp_alloc::get_memory(%min_bytes%, %cap_bytes%)%$$

$head Purpose$$
Use $cref/omp_alloc/$$ to obtain a minimum number of bytes of memory
(for use by the $cref/current thread/get_thread_num/$$).

$head min_bytes$$
This argument has prototype
$codei%
	size_t %min_bytes%
%$$
It specifies the minimum number of bytes to allocate.

$head cap_bytes$$
This argument has prototype
$codei%
	size_t& %cap_bytes%
%$$
It's input value does not matter.
Upon return, it is the actual number of bytes (capacity) 
that have been allocated for use,
$codei%
	%min_bytes% <= %cap_bytes%
%$$

$head v_ptr$$
The return value $icode v_ptr$$ has prototype
$codei%
	void* %v_ptr%
%$$
It is the location where the $icode cap_bytes$$ of memory 
that have been allocated for use begins.

$head Allocation Speed$$
This allocation should be faster if the following conditions hold:
$list number$$
The memory allocated by a previous call to $code get_memory$$ 
is currently available for use.
$lnext
The current $icode min_bytes$$ is between 
the previous $icode min_bytes$$ and previous $icode cap_bytes$$.
$lend

$head Example$$
$cref/omp_alloc.cpp/$$

$end
*/
	/*!
 	Use omp_alloc to get a specified amount of memory.

	If the memory allocated by a previous call to \c get_memory is now 
	avaialable, and \c min_bytes is between its previous value
	and the previous \c cap_bytes, this memory allocation will have
	optimal speed. Otherwise, the memory allocation is more complicated and
	may have to wait for other threads to complete an allocation.

	\param min_bytes [in]
	The minimum number of bytes of memory to be obtained for use.

	\param cap_bytes [out]
	The actual number of bytes of memory obtained for use.

	\return
	pointer to the beginning of the memory allocted for use.
 	*/
	static void* get_memory(size_t min_bytes, size_t& cap_bytes)
	{	size_t num_cap = capacity_info()->number;
		using std::cout;
		using std::endl;

		// determine the capacity for this request
		size_t cap       = 0;
		const size_t* capacity_vec = capacity_info()->value;
		while( capacity_vec[cap] < min_bytes )
		{	++cap;	
			CPPAD_ASSERT_UNKNOWN(cap < num_cap );
		}
		cap_bytes = capacity_vec[cap];

		// determine the thread and index
		size_t thread = get_thread_num();
		size_t index  = thread * num_cap + cap;

# ifndef NDEBUG
		// trace allocation
		static bool first_trace = true;
		if(	cap_bytes == CPPAD_TRACE_CAPACITY && 
		     thread    ==  CPPAD_TRACE_THREAD  && first_trace )
		{	cout << endl;	
			cout << "omp_alloc: Trace for Thread = " << thread;
			cout << " and capacity = " << cap_bytes << endl;
			first_trace = false;
		}

		// Root nodes for both lists. Note these are different for different 
		// threads because index is different for different threads.
		omp_alloc* inuse_root     = root_inuse() + index;
# endif
		omp_alloc* available_root = root_available() + index;

		// check if we already have a node we can use
		void* v_ptr               = available_root->next_;
		omp_alloc* node           = reinterpret_cast<omp_alloc*>(v_ptr);
		if( node != 0 )
		{	CPPAD_ASSERT_UNKNOWN( node->index_ == index );

			// remove node from available list
			available_root->next_ = node->next_;

# ifndef NDEBUG
			// add node to inuse list
			node->next_           = inuse_root->next_;
			inuse_root->next_     = v_ptr;

			// trace allocation
			if(	cap_bytes == CPPAD_TRACE_CAPACITY && 
			     thread    ==  CPPAD_TRACE_THREAD   )
			{	cout << "get_memory:    v_ptr = " << v_ptr << endl; } 
# endif

			// adjust counts
			inc_inuse(cap_bytes, thread);
			dec_available(cap_bytes, thread);

			// return pointer to memory, do not inclue omp_alloc information
			return reinterpret_cast<void*>(node + 1);
		}

		// Create a new node with omp_alloc information at front.
		// This uses the system allocator, which is thread safe, but slower,
		// because the thread might wait for a lock on the allocator.
		v_ptr           = ::operator new(sizeof(omp_alloc) + cap_bytes);
		node            = reinterpret_cast<omp_alloc*>(v_ptr);
		node->index_    = index;

# ifndef NDEBUG
		// add node to inuse list
		node->next_       = inuse_root->next_;
		inuse_root->next_ = v_ptr;

		// trace allocation
		if( cap_bytes == CPPAD_TRACE_CAPACITY && 
		    thread    == CPPAD_TRACE_THREAD    )
		{	cout << "get_memory:    v_ptr = " << v_ptr << endl; }
# endif

		// adjust counts
		inc_inuse(cap_bytes, thread);

		return reinterpret_cast<void*>(node + 1);
	}

/* -----------------------------------------------------------------------
$begin return_memory$$
$spell
	ptr
	omp_alloc
$$

$section Make Memory Available for Future Use by Same Thread$$

$index return_memory, omp_alloc$$
$index omp_alloc, return_memory$$
$index memory, available$$
$index available, memory$$
$index thread, available memory$$

$head Syntax$$
$codei%omp_alloc::return_memory(%v_ptr%)%$$

$head Purpose$$
Makes memory that is in use for a specific thread available (quickly)
for future use (by the same thread).

$head v_ptr$$
This argument has prototype
$codei%
	void* %v_ptr%
%$$.
It must be a pointer to memory that is currently in use; i.e.
obtained by a previous call to $cref/get_memory/$$ and not yet returned.

$head Thread$$
Either the $cref/current thread/get_thread_num/$$ must be the same as during
the corresponding call to $cref/get_memory/$$,
or the current execution mode must be sequential 
(not $cref/parallel/in_parallel/$$).

$head NDEBUG$$
If $code NDEBUG$$ is defined, $icode v_ptr$$ is not checked (this is faster).
Otherwise, a list of in use pointers is searched to make sure
that $icode v_ptr$$ is in the list. 

$head Example$$
$cref/omp_alloc.cpp/$$

$end
*/
	/*!
 	Return memory that was obtained by \c get_memory.
	The returned memory becomes available for use by 
	\c get_memory for this thread.

	\param v_ptr [in]
	Value of the pointer returned by \c get_memory and still in use.
	After this call, this pointer will available (and not in use).

	\par
	We must either be in sequential (not parallel) execution mode,
	or the current thread must be the same as for the corresponding call
	to \c get_memory.
 	*/
	static void return_memory(void* v_ptr)
	{	size_t num_cap   = capacity_info()->number;

		omp_alloc* node  = reinterpret_cast<omp_alloc*>(v_ptr) - 1;
		v_ptr            = reinterpret_cast<void*>(node);
		size_t index     = node->index_;
		size_t thread    = index / num_cap;
		size_t cap       = index % num_cap;
		size_t capacity  = capacity_info()->value[cap];

		CPPAD_ASSERT_UNKNOWN( thread < CPPAD_MAX_NUM_THREADS );
		CPPAD_ASSERT_KNOWN( 
			thread == get_thread_num() || (! in_parallel()),
			"Attempt to return memory for a different thread "
			"while in parallel mode"
		);

# ifndef NDEBUG
		// remove node from inuse list
		omp_alloc* inuse_root     = root_inuse() + index;
		omp_alloc* previous       = inuse_root;
		while( (previous->next_ != 0) & (previous->next_ != v_ptr) )
			previous = reinterpret_cast<omp_alloc*>(previous->next_);	

		// check that v_ptr is valid
		if( previous->next_ != v_ptr )
		{	using std::endl;
			std::ostringstream oss;
			oss << "return_memory: attempt to return memory not in use";
			oss << endl;
			oss << "v_ptr    = " << v_ptr    << endl;   
			oss << "thread   = " << thread   << endl;   
			oss << "capacity = " << capacity << endl;   
			oss << "See CPPAD_TRACE_THREAD & CPPAD_TRACE_CAPACITY in";
			oss << endl << "# include <cppad/omp_alloc.hpp>" << endl;
			CPPAD_ASSERT_KNOWN(false, oss.str().c_str()	); 
		}

		// trace option
		if( capacity==CPPAD_TRACE_CAPACITY && thread==CPPAD_TRACE_THREAD )
		{	std::cout << "return_memory: v_ptr = " << v_ptr << std::endl; }

		// remove v_ptr from inuse list
		previous->next_  = node->next_;
# endif
		// add node to available list
		omp_alloc* available_root = root_available() + index;
		node->next_               = available_root->next_;
		available_root->next_     = reinterpret_cast<void*>(node);

		// adjust counts
		dec_inuse(capacity, thread);
		inc_available(capacity, thread);
	}
/* -----------------------------------------------------------------------
$begin free_available$$
$spell
	omp_alloc
$$

$section Free Memory Currently Available for Quick Use by a Thread$$

$index free_available, omp_alloc$$
$index omp_alloc, free_available$$
$index free, available$$
$index available, free$$
$index thread, free memory$$

$head Syntax$$
$codei%omp_alloc::free_available(%thread%)%$$

$head Purpose$$
Free memory, currently available for quick use by a specific thread, 
for general future use.

$head thread$$
This argument has prototype
$codei%
	size_t %thread%
%$$
Either $cref/get_thread_num/$$ must be the same as $icode thread$$,
or the current execution mode must be sequential 
(not $cref/parallel/in_parallel/$$).

$head Example$$
$cref/omp_alloc.cpp/$$

$end
*/
	/*!
	Return all the memory being held as available for a thread to the system.

	\param thread [in]
	this thread that will no longer have any available memory after this call.
	This must either be the thread currently executing, or we must be 
	in sequential (not parallel) execution mode.
	*/
	static void free_available(size_t thread)
	{	CPPAD_ASSERT_KNOWN(
			thread < CPPAD_MAX_NUM_THREADS,
			"Attempt to free memory for a thread >= CPPAD_MAX_NUM_THREADS"
		);
		CPPAD_ASSERT_KNOWN( 
			thread == get_thread_num() || (! in_parallel()),
			"Attempt to free memory for a different thread "
			"while in parallel mode"
		);
	
		size_t num_cap = capacity_info()->number;
		if( num_cap == 0 )
			return;
		const size_t*     capacity_vec  = capacity_info()->value;
		size_t cap, index;
		for(cap = 0; cap < num_cap; cap++)
		{	size_t capacity = capacity_vec[cap];
			index                     = thread * num_cap + cap;
			omp_alloc* available_root = root_available() + index;
			void* v_ptr               = available_root->next_;
			while( v_ptr != 0 )
			{	omp_alloc* node = reinterpret_cast<omp_alloc*>(v_ptr); 
				void* next      = node->next_;
				::operator delete(v_ptr);
				v_ptr           = next;

				dec_available(capacity, thread);
			}
			available_root->next_ = 0;
		}
		CPPAD_ASSERT_UNKNOWN( available(thread) == 0 );
	}
/* -----------------------------------------------------------------------
$begin inuse$$
$spell
	num
	inuse
	omp_alloc
$$

$section Amount of Memory a Thread is Currently Using$$

$index inuse, omp_alloc$$
$index omp_alloc, inuse$$
$index use, memory$$
$index thread, memory inuse$$

$head Syntax$$
$icode%num_bytes% = omp_alloc::inuse(%thread%)%$$

$head Purpose$$
Memory being managed by $cref/omp_alloc/$$ has two states,
currently in use by the specified thread,
and quickly available for future use by the specified thread.
This function informs the program how much memory is in use.

$head thread$$
This argument has prototype
$codei%
	size_t %thread%
%$$
Either $cref/get_thread_num/$$ must be the same as $icode thread$$,
or the current execution mode must be sequential 
(not $cref/parallel/in_parallel/$$).

$head num_bytes$$
The return value has prototype
$codei%
	size_t %num_bytes%
%$$
It is the number of bytes currently in use by the specified thread.

$head Example$$
$cref/omp_alloc.cpp/$$

$end
*/
	/*!
	Determine the amount of memory that is currently inuse.

	\param thread [in]
	Thread for which we are determining the amount of memory
	(must be < CPPAD_MAX_NUM_THREADS).
	Durring parallel execution, this must be the thread 
	that is currently executing.

	\return
	The amount of memory in bytes.
	*/
	static size_t inuse(size_t thread)
	{ 
		CPPAD_ASSERT_UNKNOWN( thread < CPPAD_MAX_NUM_THREADS);
		CPPAD_ASSERT_UNKNOWN( 
			thread == get_thread_num() || (! in_parallel()) 
		);
		return inuse_vector()[thread];
	}
/* -----------------------------------------------------------------------
$begin available$$
$spell
	num
	omp_alloc
$$

$section Amount of Memory Available for Quick Use by a Thread$$

$index available, omp_alloc$$
$index omp_alloc, available$$
$index memory, available$$
$index thread, available memory$$

$head Syntax$$
$icode%num_bytes% = omp_alloc::available(%thread%)%$$

$head Purpose$$
Memory being managed by $cref/omp_alloc/$$ has two states,
currently in use by the specified thread,
and quickly available for future use by the specified thread.
This function informs the program how much memory is available.

$head thread$$
This argument has prototype
$codei%
	size_t %thread%
%$$
Either $cref/get_thread_num/$$ must be the same as $icode thread$$,
or the current execution mode must be sequential 
(not $cref/parallel/in_parallel/$$).

$head num_bytes$$
The return value has prototype
$codei%
	size_t %num_bytes%
%$$
It is the number of bytes currently available for use by the specified thread.

$head Example$$
$cref/omp_alloc.cpp/$$

$end
*/
	/*!
	Determine the amount of memory that is currently available for use.

	\copydetails inuse
	*/
	static size_t available(size_t thread)
	{
		CPPAD_ASSERT_UNKNOWN( thread < CPPAD_MAX_NUM_THREADS);
		CPPAD_ASSERT_UNKNOWN( 
			thread == get_thread_num() || (! in_parallel()) 
		);
		return available_vector()[thread];
	}
/* -----------------------------------------------------------------------
$begin create_array$$
$spell
	omp_alloc
	sizeof
$$

$section Allocate Memory and Create A Raw Array$$

$index create_array, omp_alloc$$
$index omp_alloc, create_array$$
$index array, allocate$$
$index allocate, array$$

$head Syntax$$
$icode%array% = omp_alloc::create_array<%Type%>(%size_min%, %size_out%)%$$.

$head Purpose$$
Create a new raw array using $cref/omp_alloc/$$ a fast memory allocator 
that works well in a multi-threading OpenMP environment.

$head Type$$
The type of the elements of the array.

$head size_min$$
This argument has prototype
$codei%
	size_t %size_min%
%$$
This is the minimum number of elements that there can be
in the resulting $icode array$$.

$head size_out$$
This argument has prototype
$codei%
	size_t& %size_out%
%$$
The input value of this argument does not matter.
Upon return, it is the actual number of elements 
in $icode array$$ 
($icode% size_min %<=% size_out%$$).

$head array$$
The return value $icode array$$ has prototype
$codei%
	%Type%* %array%
%$$
It is array with $icode size_out$$ elements.
The default constructor for $icode Type$$ is used to initialize the 
elements of $icode array$$.
Note that $cref/delete_array/$$
should be used to destroy the array when it is no longer needed.

$head Delta$$
The amount of memory $cref inuse$$ by the current thread, 
will increase $icode delta$$ where
$codei%
	sizeof(%Type%) * (%size_out% + 1) > %delta% >= sizeof(%Type%) * %size_out%
%$$
The $cref available$$ memory will decrease by $icode delta$$,
(and the allocation will be faster)
if a previous allocation with $icode size_min$$ between its current value
and $icode size_out$$ is available. 

$head Example$$
$cref/omp_alloc.cpp/$$

$end 
*/
	/*!
	Use omp_alloc to Create a Raw Array.

	\tparam Type
	The type of the elements of the array.

	\param size_min [in]
	The minimum number of elements in the array.

	\param size_out [out]
	The actual number of elements in the array.

	\return
	pointer to the first element of the array.
	The default constructor is used to initialize 
	all the elements of the array.

	\par
	The \c extra_ field, in the \c omp_alloc node before the return value,
	is set to size_out.
	*/
	template <class Type>
	static Type* create_array(size_t size_min, size_t& size_out)
	{	// minimum number of bytes to allocate
		size_t min_bytes = size_min * sizeof(Type);
		// do the allocation 
		size_t num_bytes;
		void*  v_ptr     = get_memory(min_bytes, num_bytes);
		// This is where the array starts
		Type*  array     = reinterpret_cast<Type*>(v_ptr);
		// number of Type values in the allocation
		size_out         = num_bytes / sizeof(Type);
		// store this number in the extra field
		omp_alloc* node  = reinterpret_cast<omp_alloc*>(v_ptr) - 1;
		node->extra_     = size_out;

		// call default constructor for each element
		size_t i;
		for(i = 0; i < size_out; i++)
			new(array + i) Type();

		return array;
	}
/* -----------------------------------------------------------------------
$begin delete_array$$
$spell
	omp_alloc
	sizeof
$$

$section Return A Raw Array to The Available Memory for a Thread$$

$index delete_array, omp_alloc$$
$index omp_alloc, delete_array$$
$index array, allocate$$
$index allocate, array$$

$head Syntax$$
$codei%omp_alloc::delete_array(%array%)%$$.

$head Purpose$$
Returns memory corresponding to a raw array 
(create by $cref/create_array/$$) to the 
$cref/available/$$ memory pool for the current thread.

$head Type$$
The type of the elements of the array.

$head array$$
The argument $icode array$$ has prototype
$codei%
	%Type%* %array%
%$$
It is a value returned by $cref/create_array/$$ and not yet deleted.
The $icode Type$$ destructor is called for each element in the array.

$head Thread$$
The $cref/current thread/get_thread_num/$$ must be the
same as when $cref/create_array/$$ returned the value $icode array$$.
There is an exception to this rule:
when the current execution mode is sequential
(not $cref/parallel/in_parallel/$$) the current thread number does not matter.

$head Delta$$
The amount of memory $cref inuse$$ will decrease by $icode delta$$,
and the $cref available$$ memory will increase by $icode delta$$,
where $cref/delta/create_array/Delta/$$ 
is the same as for the corresponding call to $code create_array$$.

$head Example$$
$cref/omp_alloc.cpp/$$

$end 
*/
	/*!
	Return Memory Used for a Raw Array to the Available Pool.

	\tparam Type
	The type of the elements of the array.

	\param array [in]
	A value returned by \c create_array that has not yet been deleted.
	The \c Type destructor is used to destroy each of the elements 
	of the array.

	\par
	Durring parallel execution, the current thread must be the same
	as during the corresponding call to \c create_array.
	*/
	template <class Type>
	static void delete_array(Type* array)
	{	// determine the number of values in the array
		omp_alloc* node = reinterpret_cast<omp_alloc*>(array) - 1;
		size_t size     = node->extra_;

		// call destructor for each element
		size_t i;
		for(i = 0; i < size; i++)
			(array + i)->~Type();

		// return the memory to the available pool for this thread
		omp_alloc::return_memory( reinterpret_cast<void*>(array) );
	}
};

CPPAD_END_NAMESPACE
# endif