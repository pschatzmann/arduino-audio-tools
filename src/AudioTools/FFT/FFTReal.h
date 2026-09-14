/*****************************************************************************

        FFTReal.h
        By Laurent de Soras

--- Legal stuff ---

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://sam.zoy.org/wtfpl/COPYING for more details.

*Tab=3***********************************************************************/
// #pragma GCC diagnostic ignored "-Wdeprecated-enum-enum-conversion" // commented out for platformio

// We use the DefaultAllocator which supports PSRAM
#include "AudioTools/CoreAudio/AudioBasic/Collections/Allocator.h"
#define FFT_CUSTOM_ALLOC audio_tools::DefaultAllocator


#if ! defined (ffft_FFTReal_HEADER_INCLUDED)
#define	ffft_FFTReal_HEADER_INCLUDED

#if defined (_MSC_VER)
	#pragma once
	#pragma warning (4 : 4250) // "Inherits via dominance."
#endif



/*\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

/*****************************************************************************

        def.h
        By Laurent de Soras

--- Legal stuff ---

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://sam.zoy.org/wtfpl/COPYING for more details.

*Tab=3***********************************************************************/



#if ! defined (ffft_def_HEADER_INCLUDED)
#define	ffft_def_HEADER_INCLUDED

#if defined (_MSC_VER)
	#pragma once
	#pragma warning (4 : 4250) // "Inherits via dominance."
#endif



/*\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\*/


/// @brief AudioTools internal: FFT
namespace ffft {


#ifndef PI
const double	PI		= 3.1415926535897932384626433832795;
#endif
const double	SQRT2	= 1.41421356237309514547462185873883;

#if defined (_MSC_VER)

	#define	ffft_FORCEINLINE	__forceinline

#else

	#define	ffft_FORCEINLINE	inline

#endif



}	// namespace ffft



#endif	// ffft_def_HEADER_INCLUDED



/*\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

/*****************************************************************************

        DynArray.h
        By Laurent de Soras

--- Legal stuff ---

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://sam.zoy.org/wtfpl/COPYING for more details.

*Tab=3***********************************************************************/



#if ! defined (ffft_DynArray_HEADER_INCLUDED)
#define	ffft_DynArray_HEADER_INCLUDED

#if defined (_MSC_VER)
	#pragma once
	#pragma warning (4 : 4250) // "Inherits via dominance."
#endif



/*\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\*/


/// @brief AudioTools internal: FFT
namespace ffft
{



template <class T>
class DynArray
{

/*\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

public:

	typedef	T	DataType;

						DynArray ();
	explicit			DynArray (long size);
						~DynArray ();

	inline long		size () const;
	inline void		resize (long size);

	inline const DataType &
						operator [] (long pos) const;
	inline DataType &
						operator [] (long pos);



/*\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

protected:



/*\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

private:

	DataType *		_data_ptr;
	long				_len;



/*\\ FORBIDDEN MEMBER FUNCTIONS \\\\\\\\\\\\\\\\\\\\\\*/

private:

						DynArray (const DynArray &other);
	DynArray &		operator = (const DynArray &other);
	bool				operator == (const DynArray &other);
	bool				operator != (const DynArray &other);

};	// class DynArray



}	// namespace ffft



/*****************************************************************************

        DynArray.hpp
        By Laurent de Soras

--- Legal stuff ---

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://sam.zoy.org/wtfpl/COPYING for more details.

*Tab=3***********************************************************************/



#if defined (ffft_DynArray_CURRENT_CODEHEADER)
	#error Recursive inclusion of DynArray code header.
#endif
#define	ffft_DynArray_CURRENT_CODEHEADER

#if ! defined (ffft_DynArray_CODEHEADER_INCLUDED)
#define	ffft_DynArray_CODEHEADER_INCLUDED



/*\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

#include	<assert.h>



namespace ffft
{



/*\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



template <class T>
DynArray <T>::DynArray ()
:	_data_ptr (0)
,	_len (0)
{
	// Nothing
}



template <class T>
DynArray <T>::DynArray (long size)
:	_data_ptr (0)
,	_len (0)
{
	assert (size >= 0);
	if (size > 0)
	{
#ifdef FFT_CUSTOM_ALLOC
		_data_ptr = FFT_CUSTOM_ALLOC.createArray<DataType>(size);
#else
		_data_ptr = new DataType [size];
#endif
		_len = size;
	}
}



template <class T>
DynArray <T>::~DynArray ()
{
#ifdef FFT_CUSTOM_ALLOC
  	FFT_CUSTOM_ALLOC.removeArray<DataType>(_data_ptr, _len);
#else
	delete [] _data_ptr;
#endif
	_data_ptr = 0;
	_len = 0;
}



template <class T>
long	DynArray <T>::size () const
{
	return (_len);
}



template <class T>
void	DynArray <T>::resize (long size)
{
	assert (size >= 0);
	if (size > 0)
	{
		DataType *		old_data_ptr = _data_ptr;
#ifdef FFT_CUSTOM_ALLOC
		DataType *		tmp_data_ptr = FFT_CUSTOM_ALLOC.createArray<DataType>(size);
#else
		DataType *		tmp_data_ptr = new DataType [size];
#endif
		_data_ptr = tmp_data_ptr;
		_len = size;

#ifdef FFT_CUSTOM_ALLOC
		FFT_CUSTOM_ALLOC.removeArray<DataType>(old_data_ptr, _len);
#else
		delete [] old_data_ptr;
#endif
	}
}



template <class T>
const typename DynArray <T>::DataType &	DynArray <T>::operator [] (long pos) const
{
	assert (pos >= 0);
	assert (pos < _len);

	return (_data_ptr [pos]);
}



template <class T>
typename DynArray <T>::DataType &	DynArray <T>::operator [] (long pos)
{
	assert (pos >= 0);
	assert (pos < _len);

	return (_data_ptr [pos]);
}



/*\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



/*\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



}	// namespace ffft



#endif	// ffft_DynArray_CODEHEADER_INCLUDED

#undef ffft_DynArray_CURRENT_CODEHEADER



/*\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/




#endif	// ffft_DynArray_HEADER_INCLUDED



/*\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

/*****************************************************************************

        OscSinCos.h
        By Laurent de Soras

--- Legal stuff ---

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://sam.zoy.org/wtfpl/COPYING for more details.

*Tab=3***********************************************************************/



#if ! defined (ffft_OscSinCos_HEADER_INCLUDED)
#define	ffft_OscSinCos_HEADER_INCLUDED

#if defined (_MSC_VER)
	#pragma once
	#pragma warning (4 : 4250) // "Inherits via dominance."
#endif



/*\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\*/





namespace ffft
{



template <class T>
class OscSinCos
{

/*\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

public:

	typedef	T	DataType;

						OscSinCos ();

	ffft_FORCEINLINE void
						set_step (double angle_rad);

	ffft_FORCEINLINE DataType
						get_cos () const;
	ffft_FORCEINLINE DataType
						get_sin () const;
	ffft_FORCEINLINE void
						step ();
	ffft_FORCEINLINE void
						clear_buffers ();



/*\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

protected:



/*\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

private:

	DataType			_pos_cos;		// Current phase expressed with sin and cos. [-1 ; 1]
	DataType			_pos_sin;		// -
	DataType			_step_cos;		// Phase increment per step, [-1 ; 1]
	DataType			_step_sin;		// -



/*\\ FORBIDDEN MEMBER FUNCTIONS \\\\\\\\\\\\\\\\\\\\\\*/

private:

						OscSinCos (const OscSinCos &other);
	OscSinCos &		operator = (const OscSinCos &other);
	bool				operator == (const OscSinCos &other);
	bool				operator != (const OscSinCos &other);

};	// class OscSinCos



}	// namespace ffft



/*****************************************************************************

        OscSinCos.hpp
        By Laurent de Soras

--- Legal stuff ---

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://sam.zoy.org/wtfpl/COPYING for more details.

*Tab=3***********************************************************************/



#if defined (ffft_OscSinCos_CURRENT_CODEHEADER)
	#error Recursive inclusion of OscSinCos code header.
#endif
#define	ffft_OscSinCos_CURRENT_CODEHEADER

#if ! defined (ffft_OscSinCos_CODEHEADER_INCLUDED)
#define	ffft_OscSinCos_CODEHEADER_INCLUDED



/*\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
#ifdef AVR
#  include	<math.h>
#else
#  include	<cmath>
#endif

namespace std { }



namespace ffft
{



/*\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



template <class T>
OscSinCos <T>::OscSinCos ()
:	_pos_cos (1)
,	_pos_sin (0)
,	_step_cos (1)
,	_step_sin (0)
{
	// Nothing
}



template <class T>
void	OscSinCos <T>::set_step (double angle_rad)
{
	using namespace std;

	_step_cos = static_cast <DataType> (cos (angle_rad));
	_step_sin = static_cast <DataType> (sin (angle_rad));
}



template <class T>
typename OscSinCos <T>::DataType	OscSinCos <T>::get_cos () const
{
	return (_pos_cos);
}



template <class T>
typename OscSinCos <T>::DataType	OscSinCos <T>::get_sin () const
{
	return (_pos_sin);
}



template <class T>
void	OscSinCos <T>::step ()
{
	const DataType	old_cos = _pos_cos;
	const DataType	old_sin = _pos_sin;

	_pos_cos = old_cos * _step_cos - old_sin * _step_sin;
	_pos_sin = old_cos * _step_sin + old_sin * _step_cos;
}



template <class T>
void	OscSinCos <T>::clear_buffers ()
{
	_pos_cos = static_cast <DataType> (1);
	_pos_sin = static_cast <DataType> (0);
}



/*\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



/*\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



}	// namespace ffft



#endif	// ffft_OscSinCos_CODEHEADER_INCLUDED

#undef ffft_OscSinCos_CURRENT_CODEHEADER



/*\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/




#endif	// ffft_OscSinCos_HEADER_INCLUDED



/*\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/




namespace ffft
{



template <class DT>
class FFTReal
{

/*\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

public:

	enum {			MAX_BIT_DEPTH	= 30	};	// So length can be represented as long int

	typedef	DT	DataType;

	explicit			FFTReal (long length);
	virtual			~FFTReal () {}

	long				get_length () const;
	void				do_fft (DataType f [], const DataType x []) const;
	void				do_ifft (const DataType f [], DataType x []) const;
	void				rescale (DataType x []) const;
	DataType *		use_buffer () const;



/*\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

protected:



/*\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

private:

   // Over this bit depth, we use direct calculation for sin/cos
   enum {	      TRIGO_BD_LIMIT	= 12  };

	typedef	OscSinCos <DataType>	OscType;

	void				init_br_lut ();
	void				init_trigo_lut ();
	void				init_trigo_osc ();

	ffft_FORCEINLINE const long *
						get_br_ptr () const;
	ffft_FORCEINLINE const DataType	*
						get_trigo_ptr (int level) const;
	ffft_FORCEINLINE long
						get_trigo_level_index (int level) const;

	inline void		compute_fft_general (DataType f [], const DataType x []) const;
	inline void		compute_direct_pass_1_2 (DataType df [], const DataType x []) const;
	inline void		compute_direct_pass_3 (DataType df [], const DataType sf []) const;
	inline void		compute_direct_pass_n (DataType df [], const DataType sf [], int pass) const;
	inline void		compute_direct_pass_n_lut (DataType df [], const DataType sf [], int pass) const;
	inline void		compute_direct_pass_n_osc (DataType df [], const DataType sf [], int pass) const;

	inline void		compute_ifft_general (const DataType f [], DataType x []) const;
	inline void		compute_inverse_pass_n (DataType df [], const DataType sf [], int pass) const;
	inline void		compute_inverse_pass_n_osc (DataType df [], const DataType sf [], int pass) const;
	inline void		compute_inverse_pass_n_lut (DataType df [], const DataType sf [], int pass) const;
	inline void		compute_inverse_pass_3 (DataType df [], const DataType sf []) const;
	inline void		compute_inverse_pass_1_2 (DataType x [], const DataType sf []) const;

	const long		_length;
	const int		_nbr_bits;
	DynArray <long>
						_br_lut;
	DynArray <DataType>
						_trigo_lut;
	mutable DynArray <DataType>
						_buffer;
   mutable DynArray <OscType>
						_trigo_osc;



/*\\ FORBIDDEN MEMBER FUNCTIONS \\\\\\\\\\\\\\\\\\\\\\*/

private:

						FFTReal ();
						FFTReal (const FFTReal &other);
	FFTReal &		operator = (const FFTReal &other);
	bool				operator == (const FFTReal &other);
	bool				operator != (const FFTReal &other);

};	// class FFTReal



}	// namespace ffft



/*****************************************************************************

        FFTReal.hpp
        By Laurent de Soras

--- Legal stuff ---

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://sam.zoy.org/wtfpl/COPYING for more details.

*Tab=3***********************************************************************/



#if defined (ffft_FFTReal_CURRENT_CODEHEADER)
	#error Recursive inclusion of FFTReal code header.
#endif
#define	ffft_FFTReal_CURRENT_CODEHEADER

#if ! defined (ffft_FFTReal_CODEHEADER_INCLUDED)
#define	ffft_FFTReal_CODEHEADER_INCLUDED



namespace ffft
{



static inline bool	FFTReal_is_pow2 (long x)
{
	assert (x > 0);

	return  ((x & -x) == x);
}



static inline int	FFTReal_get_next_pow2 (long x)
{
	--x;

	int				p = 0;
	while ((x & ~0xFFFFL) != 0)
	{
		p += 16;
		x >>= 16;
	}
	while ((x & ~0xFL) != 0)
	{
		p += 4;
		x >>= 4;
	}
	while (x > 0)
	{
		++p;
		x >>= 1;
	}

	return (p);
}



/*\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



/*
==============================================================================
Name: ctor
Input parameters:
	- length: length of the array on which we want to do a FFT. Range: power of
		2 only, > 0.
Throws: std::bad_alloc
==============================================================================
*/

template <class DT>
FFTReal <DT>::FFTReal (long length)
:	_length (length)
,	_nbr_bits (FFTReal_get_next_pow2 (length))
,	_br_lut ()
,	_trigo_lut ()
,	_buffer (length)
,	_trigo_osc ()
{
	assert (FFTReal_is_pow2 (length));
	assert (_nbr_bits <= MAX_BIT_DEPTH);

	init_br_lut ();
	init_trigo_lut ();
	init_trigo_osc ();
}



/*
==============================================================================
Name: get_length
Description:
	Returns the number of points processed by this FFT object.
Returns: The number of points, power of 2, > 0.
Throws: Nothing
==============================================================================
*/

template <class DT>
long	FFTReal <DT>::get_length () const
{
	return (_length);
}



/*
==============================================================================
Name: do_fft
Description:
	Compute the FFT of the array.
Input parameters:
	- x: pointer on the source array (time).
Output parameters:
	- f: pointer on the destination array (frequencies).
		f [0...length(x)/2] = real values,
		f [length(x)/2+1...length(x)-1] = negative imaginary values of
		coefficents 1...length(x)/2-1.
Throws: Nothing
==============================================================================
*/

template <class DT>
void	FFTReal <DT>::do_fft (DataType f [], const DataType x []) const
{
	assert (f != 0);
	assert (f != use_buffer ());
	assert (x != 0);
	assert (x != use_buffer ());
	assert (x != f);

	// General case
	if (_nbr_bits > 2)
	{
		compute_fft_general (f, x);
	}

	// 4-point FFT
	else if (_nbr_bits == 2)
	{
		f [1] = x [0] - x [2];
		f [3] = x [1] - x [3];

		const DataType	b_0 = x [0] + x [2];
		const DataType	b_2 = x [1] + x [3];
		
		f [0] = b_0 + b_2;
		f [2] = b_0 - b_2;
	}

	// 2-point FFT
	else if (_nbr_bits == 1)
	{
		f [0] = x [0] + x [1];
		f [1] = x [0] - x [1];
	}

	// 1-point FFT
	else
	{
		f [0] = x [0];
	}
}



/*
==============================================================================
Name: do_ifft
Description:
	Compute the inverse FFT of the array. Note that data must be post-scaled:
	IFFT (FFT (x)) = x * length (x).
Input parameters:
	- f: pointer on the source array (frequencies).
		f [0...length(x)/2] = real values
		f [length(x)/2+1...length(x)-1] = negative imaginary values of
		coefficents 1...length(x)/2-1.
Output parameters:
	- x: pointer on the destination array (time).
Throws: Nothing
==============================================================================
*/

template <class DT>
void	FFTReal <DT>::do_ifft (const DataType f [], DataType x []) const
{
	assert (f != 0);
	assert (f != use_buffer ());
	assert (x != 0);
	assert (x != use_buffer ());
	assert (x != f);

	// General case
	if (_nbr_bits > 2)
	{
		compute_ifft_general (f, x);
	}

	// 4-point IFFT
	else if (_nbr_bits == 2)
	{
		const DataType	b_0 = f [0] + f [2];
		const DataType	b_2 = f [0] - f [2];

		x [0] = b_0 + f [1] * 2;
		x [2] = b_0 - f [1] * 2;
		x [1] = b_2 + f [3] * 2;
		x [3] = b_2 - f [3] * 2;
	}

	// 2-point IFFT
	else if (_nbr_bits == 1)
	{
		x [0] = f [0] + f [1];
		x [1] = f [0] - f [1];
	}

	// 1-point IFFT
	else
	{
		x [0] = f [0];
	}
}



/*
==============================================================================
Name: rescale
Description:
	Scale an array by divide each element by its length. This function should
	be called after FFT + IFFT.
Input parameters:
	- x: pointer on array to rescale (time or frequency).
Throws: Nothing
==============================================================================
*/

template <class DT>
void	FFTReal <DT>::rescale (DataType x []) const
{
	const DataType	mul = DataType (1.0 / _length);

	if (_length < 4)
	{
		long				i = _length - 1;
		do
		{
			x [i] *= mul;
			--i;
		}
		while (i >= 0);
	}

	else
	{
		assert ((_length & 3) == 0);

		// Could be optimized with SIMD instruction sets (needs alignment check)
		long				i = _length - 4;
		do
		{
			x [i + 0] *= mul;
			x [i + 1] *= mul;
			x [i + 2] *= mul;
			x [i + 3] *= mul;
			i -= 4;
		}
		while (i >= 0);
	}
}



/*
==============================================================================
Name: use_buffer
Description:
	Access the internal buffer, whose length is the FFT one.
	Buffer content will be erased at each do_fft() / do_ifft() call!
	This buffer cannot be used as:
		- source for FFT or IFFT done with this object
		- destination for FFT or IFFT done with this object
Returns:
	Buffer start address
Throws: Nothing
==============================================================================
*/

template <class DT>
typename FFTReal <DT>::DataType *	FFTReal <DT>::use_buffer () const
{
	return (&_buffer [0]);
}



/*\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



/*\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



template <class DT>
void	FFTReal <DT>::init_br_lut ()
{
	const long		length = 1L << _nbr_bits;
	_br_lut.resize (length);

	_br_lut [0] = 0;
	long				br_index = 0;
	for (long cnt = 1; cnt < length; ++cnt)
	{
		// ++br_index (bit reversed)
		long				bit = length >> 1;
		while (((br_index ^= bit) & bit) == 0)
		{
			bit >>= 1;
		}

		_br_lut [cnt] = br_index;
	}
}



template <class DT>
void	FFTReal <DT>::init_trigo_lut ()
{
	using namespace std;

	if (_nbr_bits > 3)
	{
		const long		total_len = (1L << (_nbr_bits - 1)) - 4;
		_trigo_lut.resize (total_len);

		for (int level = 3; level < _nbr_bits; ++level)
		{
			const long		level_len = 1L << (level - 1);
			DataType	* const	level_ptr =
				&_trigo_lut [get_trigo_level_index (level)];
			const double	mul = PI / (level_len << 1);

			for (long i = 0; i < level_len; ++ i)
			{
				level_ptr [i] = static_cast <DataType> (cos (i * mul));
			}
		}
	}
}



template <class DT>
void	FFTReal <DT>::init_trigo_osc ()
{
	const int		nbr_osc = _nbr_bits - TRIGO_BD_LIMIT;
	if (nbr_osc > 0)
	{
		_trigo_osc.resize (nbr_osc);

		for (int osc_cnt = 0; osc_cnt < nbr_osc; ++osc_cnt)
		{
			OscType &		osc = _trigo_osc [osc_cnt];

			const long		len = 1L << (TRIGO_BD_LIMIT + osc_cnt);
			const double	mul = (0.5 * PI) / len;
			osc.set_step (mul);
		}
	}
}



template <class DT>
const long *	FFTReal <DT>::get_br_ptr () const
{
	return (&_br_lut [0]);
}



template <class DT>
const typename FFTReal <DT>::DataType *	FFTReal <DT>::get_trigo_ptr (int level) const
{
	assert (level >= 3);

	return (&_trigo_lut [get_trigo_level_index (level)]);
}



template <class DT>
long	FFTReal <DT>::get_trigo_level_index (int level) const
{
	assert (level >= 3);

	return ((1L << (level - 1)) - 4);
}



// Transform in several passes
template <class DT>
void	FFTReal <DT>::compute_fft_general (DataType f [], const DataType x []) const
{
	assert (f != 0);
	assert (f != use_buffer ());
	assert (x != 0);
	assert (x != use_buffer ());
	assert (x != f);

	DataType *		sf;
	DataType *		df;

	if ((_nbr_bits & 1) != 0)
	{
		df = use_buffer ();
		sf = f;
	}
	else
	{
		df = f;
		sf = use_buffer ();
	}

	compute_direct_pass_1_2 (df, x);
	compute_direct_pass_3 (sf, df);

	for (int pass = 3; pass < _nbr_bits; ++ pass)
	{
		compute_direct_pass_n (df, sf, pass);

		DataType * const	temp_ptr = df;
		df = sf;
		sf = temp_ptr;
	}
}



template <class DT>
void	FFTReal <DT>::compute_direct_pass_1_2 (DataType df [], const DataType x []) const
{
	assert (df != 0);
	assert (x != 0);
	assert (df != x);

	const long * const	bit_rev_lut_ptr = get_br_ptr ();
	long				coef_index = 0;
	do
	{
		const long		rev_index_0 = bit_rev_lut_ptr [coef_index];
		const long		rev_index_1 = bit_rev_lut_ptr [coef_index + 1];
		const long		rev_index_2 = bit_rev_lut_ptr [coef_index + 2];
		const long		rev_index_3 = bit_rev_lut_ptr [coef_index + 3];

		DataType	* const	df2 = df + coef_index;
		df2 [1] = x [rev_index_0] - x [rev_index_1];
		df2 [3] = x [rev_index_2] - x [rev_index_3];

		const DataType	sf_0 = x [rev_index_0] + x [rev_index_1];
		const DataType	sf_2 = x [rev_index_2] + x [rev_index_3];

		df2 [0] = sf_0 + sf_2;
		df2 [2] = sf_0 - sf_2;
		
		coef_index += 4;
	}
	while (coef_index < _length);
}



template <class DT>
void	FFTReal <DT>::compute_direct_pass_3 (DataType df [], const DataType sf []) const
{
	assert (df != 0);
	assert (sf != 0);
	assert (df != sf);

	const DataType	sqrt2_2 = DataType (SQRT2 * 0.5);
	long				coef_index = 0;
	do
	{
		DataType			v;

		df [coef_index] = sf [coef_index] + sf [coef_index + 4];
		df [coef_index + 4] = sf [coef_index] - sf [coef_index + 4];
		df [coef_index + 2] = sf [coef_index + 2];
		df [coef_index + 6] = sf [coef_index + 6];

		v = (sf [coef_index + 5] - sf [coef_index + 7]) * sqrt2_2;
		df [coef_index + 1] = sf [coef_index + 1] + v;
		df [coef_index + 3] = sf [coef_index + 1] - v;

		v = (sf [coef_index + 5] + sf [coef_index + 7]) * sqrt2_2;
		df [coef_index + 5] = v + sf [coef_index + 3];
		df [coef_index + 7] = v - sf [coef_index + 3];

		coef_index += 8;
	}
	while (coef_index < _length);
}



template <class DT>
void	FFTReal <DT>::compute_direct_pass_n (DataType df [], const DataType sf [], int pass) const
{
	assert (df != 0);
	assert (sf != 0);
	assert (df != sf);
	assert (pass >= 3);
	assert (pass < _nbr_bits);

	if (pass <= TRIGO_BD_LIMIT)
	{
		compute_direct_pass_n_lut (df, sf, pass);
	}
	else
	{
		compute_direct_pass_n_osc (df, sf, pass);
	}
}



template <class DT>
void	FFTReal <DT>::compute_direct_pass_n_lut (DataType df [], const DataType sf [], int pass) const
{
	assert (df != 0);
	assert (sf != 0);
	assert (df != sf);
	assert (pass >= 3);
	assert (pass < _nbr_bits);

	const long		nbr_coef = 1 << pass;
	const long		h_nbr_coef = nbr_coef >> 1;
	const long		d_nbr_coef = nbr_coef << 1;
	long				coef_index = 0;
	const DataType	* const	cos_ptr = get_trigo_ptr (pass);
	do
	{
		const DataType	* const	sf1r = sf + coef_index;
		const DataType	* const	sf2r = sf1r + nbr_coef;
		DataType			* const	dfr = df + coef_index;
		DataType			* const	dfi = dfr + nbr_coef;

		// Extreme coefficients are always real
		dfr [0] = sf1r [0] + sf2r [0];
		dfi [0] = sf1r [0] - sf2r [0];	// dfr [nbr_coef] =
		dfr [h_nbr_coef] = sf1r [h_nbr_coef];
		dfi [h_nbr_coef] = sf2r [h_nbr_coef];

		// Others are conjugate complex numbers
		const DataType * const	sf1i = sf1r + h_nbr_coef;
		const DataType * const	sf2i = sf1i + nbr_coef;
		for (long i = 1; i < h_nbr_coef; ++ i)
		{
			const DataType	c = cos_ptr [i];					// cos (i*PI/nbr_coef);
			const DataType	s = cos_ptr [h_nbr_coef - i];	// sin (i*PI/nbr_coef);
			DataType	 		v;

			v = sf2r [i] * c - sf2i [i] * s;
			dfr [i] = sf1r [i] + v;
			dfi [-i] = sf1r [i] - v;	// dfr [nbr_coef - i] =

			v = sf2r [i] * s + sf2i [i] * c;
			dfi [i] = v + sf1i [i];
			dfi [nbr_coef - i] = v - sf1i [i];
		}

		coef_index += d_nbr_coef;
	}
	while (coef_index < _length);
}



template <class DT>
void	FFTReal <DT>::compute_direct_pass_n_osc (DataType df [], const DataType sf [], int pass) const
{
	assert (df != 0);
	assert (sf != 0);
	assert (df != sf);
	assert (pass > TRIGO_BD_LIMIT);
	assert (pass < _nbr_bits);

	const long		nbr_coef = 1 << pass;
	const long		h_nbr_coef = nbr_coef >> 1;
	const long		d_nbr_coef = nbr_coef << 1;
	long				coef_index = 0;
	OscType &		osc = _trigo_osc [pass - (TRIGO_BD_LIMIT + 1)];
	do
	{
		const DataType	* const	sf1r = sf + coef_index;
		const DataType	* const	sf2r = sf1r + nbr_coef;
		DataType			* const	dfr = df + coef_index;
		DataType			* const	dfi = dfr + nbr_coef;

		osc.clear_buffers ();

		// Extreme coefficients are always real
		dfr [0] = sf1r [0] + sf2r [0];
		dfi [0] = sf1r [0] - sf2r [0];	// dfr [nbr_coef] =
		dfr [h_nbr_coef] = sf1r [h_nbr_coef];
		dfi [h_nbr_coef] = sf2r [h_nbr_coef];

		// Others are conjugate complex numbers
		const DataType * const	sf1i = sf1r + h_nbr_coef;
		const DataType * const	sf2i = sf1i + nbr_coef;
		for (long i = 1; i < h_nbr_coef; ++ i)
		{
			osc.step ();
			const DataType	c = osc.get_cos ();
			const DataType	s = osc.get_sin ();
			DataType	 		v;

			v = sf2r [i] * c - sf2i [i] * s;
			dfr [i] = sf1r [i] + v;
			dfi [-i] = sf1r [i] - v;	// dfr [nbr_coef - i] =

			v = sf2r [i] * s + sf2i [i] * c;
			dfi [i] = v + sf1i [i];
			dfi [nbr_coef - i] = v - sf1i [i];
		}

		coef_index += d_nbr_coef;
	}
	while (coef_index < _length);
}



// Transform in several pass
template <class DT>
void	FFTReal <DT>::compute_ifft_general (const DataType f [], DataType x []) const
{
	assert (f != 0);
	assert (f != use_buffer ());
	assert (x != 0);
	assert (x != use_buffer ());
	assert (x != f);

	DataType *		sf = const_cast <DataType *> (f);
	DataType *		df;
	DataType *		df_temp;

	if (_nbr_bits & 1)
	{
		df = use_buffer ();
		df_temp = x;
	}
	else
	{
		df = x;
		df_temp = use_buffer ();
	}

	for (int pass = _nbr_bits - 1; pass >= 3; -- pass)
	{
		compute_inverse_pass_n (df, sf, pass);

		if (pass < _nbr_bits - 1)
		{
			DataType	* const	temp_ptr = df;
			df = sf;
			sf = temp_ptr;
		}
		else
		{
			sf = df;
			df = df_temp;
		}
	}

	compute_inverse_pass_3 (df, sf);
	compute_inverse_pass_1_2 (x, df);
}



template <class DT>
void	FFTReal <DT>::compute_inverse_pass_n (DataType df [], const DataType sf [], int pass) const
{
	assert (df != 0);
	assert (sf != 0);
	assert (df != sf);
	assert (pass >= 3);
	assert (pass < _nbr_bits);

	if (pass <= TRIGO_BD_LIMIT)
	{
		compute_inverse_pass_n_lut (df, sf, pass);
	}
	else
	{
		compute_inverse_pass_n_osc (df, sf, pass);
	}
}



template <class DT>
void	FFTReal <DT>::compute_inverse_pass_n_lut (DataType df [], const DataType sf [], int pass) const
{
	assert (df != 0);
	assert (sf != 0);
	assert (df != sf);
	assert (pass >= 3);
	assert (pass < _nbr_bits);

	const long		nbr_coef = 1 << pass;
	const long		h_nbr_coef = nbr_coef >> 1;
	const long		d_nbr_coef = nbr_coef << 1;
	long				coef_index = 0;
	const DataType * const	cos_ptr = get_trigo_ptr (pass);
	do
	{
		const DataType	* const	sfr = sf + coef_index;
		const DataType	* const	sfi = sfr + nbr_coef;
		DataType			* const	df1r = df + coef_index;
		DataType			* const	df2r = df1r + nbr_coef;

		// Extreme coefficients are always real
		df1r [0] = sfr [0] + sfi [0];		// + sfr [nbr_coef]
		df2r [0] = sfr [0] - sfi [0];		// - sfr [nbr_coef]
		df1r [h_nbr_coef] = sfr [h_nbr_coef] * 2;
		df2r [h_nbr_coef] = sfi [h_nbr_coef] * 2;

		// Others are conjugate complex numbers
		DataType * const	df1i = df1r + h_nbr_coef;
		DataType * const	df2i = df1i + nbr_coef;
		for (long i = 1; i < h_nbr_coef; ++ i)
		{
			df1r [i] = sfr [i] + sfi [-i];		// + sfr [nbr_coef - i]
			df1i [i] = sfi [i] - sfi [nbr_coef - i];

			const DataType	c = cos_ptr [i];					// cos (i*PI/nbr_coef);
			const DataType	s = cos_ptr [h_nbr_coef - i];	// sin (i*PI/nbr_coef);
			const DataType	vr = sfr [i] - sfi [-i];		// - sfr [nbr_coef - i]
			const DataType	vi = sfi [i] + sfi [nbr_coef - i];

			df2r [i] = vr * c + vi * s;
			df2i [i] = vi * c - vr * s;
		}

		coef_index += d_nbr_coef;
	}
	while (coef_index < _length);
}



template <class DT>
void	FFTReal <DT>::compute_inverse_pass_n_osc (DataType df [], const DataType sf [], int pass) const
{
	assert (df != 0);
	assert (sf != 0);
	assert (df != sf);
	assert (pass > TRIGO_BD_LIMIT);
	assert (pass < _nbr_bits);

	const long		nbr_coef = 1 << pass;
	const long		h_nbr_coef = nbr_coef >> 1;
	const long		d_nbr_coef = nbr_coef << 1;
	long				coef_index = 0;
	OscType &		osc = _trigo_osc [pass - (TRIGO_BD_LIMIT + 1)];
	do
	{
		const DataType	* const	sfr = sf + coef_index;
		const DataType	* const	sfi = sfr + nbr_coef;
		DataType			* const	df1r = df + coef_index;
		DataType			* const	df2r = df1r + nbr_coef;

		osc.clear_buffers ();

		// Extreme coefficients are always real
		df1r [0] = sfr [0] + sfi [0];		// + sfr [nbr_coef]
		df2r [0] = sfr [0] - sfi [0];		// - sfr [nbr_coef]
		df1r [h_nbr_coef] = sfr [h_nbr_coef] * 2;
		df2r [h_nbr_coef] = sfi [h_nbr_coef] * 2;

		// Others are conjugate complex numbers
		DataType * const	df1i = df1r + h_nbr_coef;
		DataType * const	df2i = df1i + nbr_coef;
		for (long i = 1; i < h_nbr_coef; ++ i)
		{
			df1r [i] = sfr [i] + sfi [-i];		// + sfr [nbr_coef - i]
			df1i [i] = sfi [i] - sfi [nbr_coef - i];

			osc.step ();
			const DataType	c = osc.get_cos ();
			const DataType	s = osc.get_sin ();
			const DataType	vr = sfr [i] - sfi [-i];		// - sfr [nbr_coef - i]
			const DataType	vi = sfi [i] + sfi [nbr_coef - i];

			df2r [i] = vr * c + vi * s;
			df2i [i] = vi * c - vr * s;
		}

		coef_index += d_nbr_coef;
	}
	while (coef_index < _length);
}



template <class DT>
void	FFTReal <DT>::compute_inverse_pass_3 (DataType df [], const DataType sf []) const
{
	assert (df != 0);
	assert (sf != 0);
	assert (df != sf);

	const DataType	sqrt2_2 = DataType (SQRT2 * 0.5);
	long				coef_index = 0;
	do
	{
		df [coef_index] = sf [coef_index] + sf [coef_index + 4];
		df [coef_index + 4] = sf [coef_index] - sf [coef_index + 4];
		df [coef_index + 2] = sf [coef_index + 2] * 2;
		df [coef_index + 6] = sf [coef_index + 6] * 2;

		df [coef_index + 1] = sf [coef_index + 1] + sf [coef_index + 3];
		df [coef_index + 3] = sf [coef_index + 5] - sf [coef_index + 7];

		const DataType	vr = sf [coef_index + 1] - sf [coef_index + 3];
		const DataType	vi = sf [coef_index + 5] + sf [coef_index + 7];

		df [coef_index + 5] = (vr + vi) * sqrt2_2;
		df [coef_index + 7] = (vi - vr) * sqrt2_2;

		coef_index += 8;
	}
	while (coef_index < _length);
}



template <class DT>
void	FFTReal <DT>::compute_inverse_pass_1_2 (DataType x [], const DataType sf []) const
{
	assert (x != 0);
	assert (sf != 0);
	assert (x != sf);

	const long *	bit_rev_lut_ptr = get_br_ptr ();
	const DataType *	sf2 = sf;
	long				coef_index = 0;
	do
	{
		{
			const DataType	b_0 = sf2 [0] + sf2 [2];
			const DataType	b_2 = sf2 [0] - sf2 [2];
			const DataType	b_1 = sf2 [1] * 2;
			const DataType	b_3 = sf2 [3] * 2;

			x [bit_rev_lut_ptr [0]] = b_0 + b_1;
			x [bit_rev_lut_ptr [1]] = b_0 - b_1;
			x [bit_rev_lut_ptr [2]] = b_2 + b_3;
			x [bit_rev_lut_ptr [3]] = b_2 - b_3;
		}
		{
			const DataType	b_0 = sf2 [4] + sf2 [6];
			const DataType	b_2 = sf2 [4] - sf2 [6];
			const DataType	b_1 = sf2 [5] * 2;
			const DataType	b_3 = sf2 [7] * 2;

			x [bit_rev_lut_ptr [4]] = b_0 + b_1;
			x [bit_rev_lut_ptr [5]] = b_0 - b_1;
			x [bit_rev_lut_ptr [6]] = b_2 + b_3;
			x [bit_rev_lut_ptr [7]] = b_2 - b_3;
		}

		sf2 += 8;
		coef_index += 8;
		bit_rev_lut_ptr += 8;
	}
	while (coef_index < _length);
}



}	// namespace ffft



#endif	// ffft_FFTReal_CODEHEADER_INCLUDED

#undef ffft_FFTReal_CURRENT_CODEHEADER



/*\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/




#endif	// ffft_FFTReal_HEADER_INCLUDED



/*\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

/*****************************************************************************

        FFTRealFixLen.h
        By Laurent de Soras

--- Legal stuff ---

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://sam.zoy.org/wtfpl/COPYING for more details.

*Tab=3***********************************************************************/



#if ! defined (ffft_FFTRealFixLen_HEADER_INCLUDED)
#define	ffft_FFTRealFixLen_HEADER_INCLUDED

#if defined (_MSC_VER)
	#pragma once
	#pragma warning (4 : 4250) // "Inherits via dominance."
#endif



/*\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

/*****************************************************************************

        Array.h
        By Laurent de Soras

--- Legal stuff ---

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://sam.zoy.org/wtfpl/COPYING for more details.

*Tab=3***********************************************************************/



#if ! defined (ffft_Array_HEADER_INCLUDED)
#define	ffft_Array_HEADER_INCLUDED

#if defined (_MSC_VER)
	#pragma once
	#pragma warning (4 : 4250) // "Inherits via dominance."
#endif



/*\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



namespace ffft
{



template <class T, long LEN>
class Array
{

/*\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

public:

	typedef	T	DataType;

						Array ();

	inline const DataType &
						operator [] (long pos) const;
	inline DataType &
						operator [] (long pos);

	static inline long
						size ();



/*\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

protected:



/*\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

private:

	DataType			_data_arr [LEN];



/*\\ FORBIDDEN MEMBER FUNCTIONS \\\\\\\\\\\\\\\\\\\\\\*/

private:

						Array (const Array &other);
	Array &			operator = (const Array &other);
	bool				operator == (const Array &other);
	bool				operator != (const Array &other);

};	// class Array



}	// namespace ffft



/*****************************************************************************

        Array.hpp
        By Laurent de Soras

--- Legal stuff ---

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://sam.zoy.org/wtfpl/COPYING for more details.

*Tab=3***********************************************************************/



#if defined (ffft_Array_CURRENT_CODEHEADER)
	#error Recursive inclusion of Array code header.
#endif
#define	ffft_Array_CURRENT_CODEHEADER

#if ! defined (ffft_Array_CODEHEADER_INCLUDED)
#define	ffft_Array_CODEHEADER_INCLUDED



/*\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\*/


namespace ffft
{



/*\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



template <class T, long LEN>
Array <T, LEN>::Array ()
{
	// Nothing
}



template <class T, long LEN>
const typename Array <T, LEN>::DataType &	Array <T, LEN>::operator [] (long pos) const
{
	assert (pos >= 0);
	assert (pos < LEN);

	return (_data_arr [pos]);
}



template <class T, long LEN>
typename Array <T, LEN>::DataType &	Array <T, LEN>::operator [] (long pos)
{
	assert (pos >= 0);
	assert (pos < LEN);

	return (_data_arr [pos]);
}



template <class T, long LEN>
long	Array <T, LEN>::size ()
{
	return (LEN);
}



/*\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



/*\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



}	// namespace ffft



#endif	// ffft_Array_CODEHEADER_INCLUDED

#undef ffft_Array_CURRENT_CODEHEADER



/*\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/




#endif	// ffft_Array_HEADER_INCLUDED



/*\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

/*****************************************************************************

        DynArray.h
        By Laurent de Soras

--- Legal stuff ---

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://sam.zoy.org/wtfpl/COPYING for more details.

*Tab=3***********************************************************************/



#if ! defined (ffft_DynArray_HEADER_INCLUDED)
#define	ffft_DynArray_HEADER_INCLUDED

#if defined (_MSC_VER)
	#pragma once
	#pragma warning (4 : 4250) // "Inherits via dominance."
#endif



/*\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



namespace ffft
{



template <class T>
class DynArray
{

/*\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

public:

	typedef	T	DataType;

						DynArray ();
	explicit			DynArray (long size);
						~DynArray ();

	inline long		size () const;
	inline void		resize (long size);

	inline const DataType &
						operator [] (long pos) const;
	inline DataType &
						operator [] (long pos);



/*\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

protected:



/*\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

private:

	DataType *		_data_ptr;
	long				_len;



/*\\ FORBIDDEN MEMBER FUNCTIONS \\\\\\\\\\\\\\\\\\\\\\*/

private:

						DynArray (const DynArray &other);
	DynArray &		operator = (const DynArray &other);
	bool				operator == (const DynArray &other);
	bool				operator != (const DynArray &other);

};	// class DynArray



}	// namespace ffft



/*****************************************************************************

        DynArray.hpp
        By Laurent de Soras

--- Legal stuff ---

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://sam.zoy.org/wtfpl/COPYING for more details.

*Tab=3***********************************************************************/



#if defined (ffft_DynArray_CURRENT_CODEHEADER)
	#error Recursive inclusion of DynArray code header.
#endif
#define	ffft_DynArray_CURRENT_CODEHEADER

#if ! defined (ffft_DynArray_CODEHEADER_INCLUDED)
#define	ffft_DynArray_CODEHEADER_INCLUDED



/*\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\*/


namespace ffft
{



/*\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



template <class T>
DynArray <T>::DynArray ()
:	_data_ptr (0)
,	_len (0)
{
	// Nothing
}



template <class T>
DynArray <T>::DynArray (long size)
:	_data_ptr (0)
,	_len (0)
{
	assert (size >= 0);
	if (size > 0)
	{
#ifdef FFT_CUSTOM_ALLOC
		_data_ptr = FFT_CUSTOM_ALLOC.createArray<DataType>(size);
#else
		_data_ptr = new DataType [size];
#endif
		_len = size;
	}
}



template <class T>
DynArray <T>::~DynArray ()
{
#ifdef FFT_CUSTOM_ALLOC
	FFT_CUSTOM_ALLOC.removeArray<T>(_data_ptr, _len);
#else
	delete [] _data_ptr;
#endif
	_data_ptr = 0;
	_len = 0;
}



template <class T>
long	DynArray <T>::size () const
{
	return (_len);
}



template <class T>
void	DynArray <T>::resize (long size)
{
	assert (size >= 0);
	if (size > 0)
	{
		DataType *		old_data_ptr = _data_ptr;
#ifdef FFT_CUSTOM_ALLOC
#else
		DataType *		tmp_data_ptr = new DataType [size];
#endif

		_data_ptr = tmp_data_ptr;
		_len = size;

#ifdef FFT_CUSTOM_ALLOC
#else
		delete [] old_data_ptr;
#endif
	}
}



template <class T>
const typename DynArray <T>::DataType &	DynArray <T>::operator [] (long pos) const
{
	assert (pos >= 0);
	assert (pos < _len);

	return (_data_ptr [pos]);
}



template <class T>
typename DynArray <T>::DataType &	DynArray <T>::operator [] (long pos)
{
	assert (pos >= 0);
	assert (pos < _len);

	return (_data_ptr [pos]);
}



/*\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



/*\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



}	// namespace ffft



#endif	// ffft_DynArray_CODEHEADER_INCLUDED

#undef ffft_DynArray_CURRENT_CODEHEADER



/*\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/




#endif	// ffft_DynArray_HEADER_INCLUDED



/*\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

/*****************************************************************************

        FFTRealFixLenParam.h
        By Laurent de Soras

--- Legal stuff ---

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://sam.zoy.org/wtfpl/COPYING for more details.

*Tab=3***********************************************************************/



#if ! defined (ffft_FFTRealFixLenParam_HEADER_INCLUDED)
#define	ffft_FFTRealFixLenParam_HEADER_INCLUDED

#if defined (_MSC_VER)
	#pragma once
	#pragma warning (4 : 4250) // "Inherits via dominance."
#endif



/*\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



namespace ffft
{



class FFTRealFixLenParam
{

/*\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

public:

   // Over this bit depth, we use direct calculation for sin/cos
   enum {	      TRIGO_BD_LIMIT	= 12  };

	typedef	float	DataType;



/*\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

protected:



/*\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

private:



/*\\ FORBIDDEN MEMBER FUNCTIONS \\\\\\\\\\\\\\\\\\\\\\*/

private:

						FFTRealFixLenParam ();
						FFTRealFixLenParam (const FFTRealFixLenParam &other);
	FFTRealFixLenParam &
						operator = (const FFTRealFixLenParam &other);
	bool				operator == (const FFTRealFixLenParam &other);
	bool				operator != (const FFTRealFixLenParam &other);

};	// class FFTRealFixLenParam



}	// namespace ffft



//#include	"ffft/FFTRealFixLenParam.hpp"



#endif	// ffft_FFTRealFixLenParam_HEADER_INCLUDED



/*\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

/*****************************************************************************

        OscSinCos.h
        By Laurent de Soras

--- Legal stuff ---

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://sam.zoy.org/wtfpl/COPYING for more details.

*Tab=3***********************************************************************/



#if ! defined (ffft_OscSinCos_HEADER_INCLUDED)
#define	ffft_OscSinCos_HEADER_INCLUDED

#if defined (_MSC_VER)
	#pragma once
	#pragma warning (4 : 4250) // "Inherits via dominance."
#endif



/*\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\*/





namespace ffft
{



template <class T>
class OscSinCos
{

/*\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

public:

	typedef	T	DataType;

						OscSinCos ();

	ffft_FORCEINLINE void
						set_step (double angle_rad);

	ffft_FORCEINLINE DataType
						get_cos () const;
	ffft_FORCEINLINE DataType
						get_sin () const;
	ffft_FORCEINLINE void
						step ();
	ffft_FORCEINLINE void
						clear_buffers ();



/*\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

protected:



/*\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

private:

	DataType			_pos_cos;		// Current phase expressed with sin and cos. [-1 ; 1]
	DataType			_pos_sin;		// -
	DataType			_step_cos;		// Phase increment per step, [-1 ; 1]
	DataType			_step_sin;		// -



/*\\ FORBIDDEN MEMBER FUNCTIONS \\\\\\\\\\\\\\\\\\\\\\*/

private:

						OscSinCos (const OscSinCos &other);
	OscSinCos &		operator = (const OscSinCos &other);
	bool				operator == (const OscSinCos &other);
	bool				operator != (const OscSinCos &other);

};	// class OscSinCos



}	// namespace ffft



/*****************************************************************************

        OscSinCos.hpp
        By Laurent de Soras

--- Legal stuff ---

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://sam.zoy.org/wtfpl/COPYING for more details.

*Tab=3***********************************************************************/



#if defined (ffft_OscSinCos_CURRENT_CODEHEADER)
	#error Recursive inclusion of OscSinCos code header.
#endif
#define	ffft_OscSinCos_CURRENT_CODEHEADER

#if ! defined (ffft_OscSinCos_CODEHEADER_INCLUDED)
#define	ffft_OscSinCos_CODEHEADER_INCLUDED



/*\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

#include	<cmath>

namespace std { }



namespace ffft
{



/*\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



template <class T>
OscSinCos <T>::OscSinCos ()
:	_pos_cos (1)
,	_pos_sin (0)
,	_step_cos (1)
,	_step_sin (0)
{
	// Nothing
}



template <class T>
void	OscSinCos <T>::set_step (double angle_rad)
{
	using namespace std;

	_step_cos = static_cast <DataType> (cos (angle_rad));
	_step_sin = static_cast <DataType> (sin (angle_rad));
}



template <class T>
typename OscSinCos <T>::DataType	OscSinCos <T>::get_cos () const
{
	return (_pos_cos);
}



template <class T>
typename OscSinCos <T>::DataType	OscSinCos <T>::get_sin () const
{
	return (_pos_sin);
}



template <class T>
void	OscSinCos <T>::step ()
{
	const DataType	old_cos = _pos_cos;
	const DataType	old_sin = _pos_sin;

	_pos_cos = old_cos * _step_cos - old_sin * _step_sin;
	_pos_sin = old_cos * _step_sin + old_sin * _step_cos;
}



template <class T>
void	OscSinCos <T>::clear_buffers ()
{
	_pos_cos = static_cast <DataType> (1);
	_pos_sin = static_cast <DataType> (0);
}



/*\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



/*\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



}	// namespace ffft



#endif	// ffft_OscSinCos_CODEHEADER_INCLUDED

#undef ffft_OscSinCos_CURRENT_CODEHEADER



/*\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/




#endif	// ffft_OscSinCos_HEADER_INCLUDED



/*\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/




namespace ffft
{



template <int LL2>
class FFTRealFixLen
{
	typedef	int	CompileTimeCheck1 [(LL2 >=  0) ? 1 : -1];
	typedef	int	CompileTimeCheck2 [(LL2 <= 30) ? 1 : -1];

/*\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

public:

   typedef	FFTRealFixLenParam::DataType   DataType;
	typedef	OscSinCos <DataType>	OscType;

	enum {			FFT_LEN_L2	= LL2	};
	enum {			FFT_LEN		= 1 << FFT_LEN_L2	};

						FFTRealFixLen ();

	inline long		get_length () const;
	void				do_fft (DataType f [], const DataType x []);
	void				do_ifft (const DataType f [], DataType x []);
	void				rescale (DataType x []) const;



/*\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

protected:



/*\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

private:

	enum {			TRIGO_BD_LIMIT	= FFTRealFixLenParam::TRIGO_BD_LIMIT	};

	enum {			BR_ARR_SIZE_L2	= ((FFT_LEN_L2 - 3) < 0) ? 0 : (FFT_LEN_L2 - 2)	};
	enum {			BR_ARR_SIZE		= 1 << BR_ARR_SIZE_L2	};

   enum {			TRIGO_BD			=   ((FFT_LEN_L2 - TRIGO_BD_LIMIT) < 0)
											  ? (int)FFT_LEN_L2
											  : (int)TRIGO_BD_LIMIT };
	enum {			TRIGO_TABLE_ARR_SIZE_L2	= (LL2 < 4) ? 0 : (TRIGO_BD - 2)	};
	enum {			TRIGO_TABLE_ARR_SIZE	= 1 << TRIGO_TABLE_ARR_SIZE_L2	};

	enum {			NBR_TRIGO_OSC			= FFT_LEN_L2 - TRIGO_BD	};
	enum {			TRIGO_OSC_ARR_SIZE	=	(NBR_TRIGO_OSC > 0) ? NBR_TRIGO_OSC : 1	};

	void				build_br_lut ();
	void				build_trigo_lut ();
	void				build_trigo_osc ();

	DynArray <DataType>
						_buffer;
	DynArray <long>
						_br_data;
	DynArray <DataType>
						_trigo_data;
   Array <OscType, TRIGO_OSC_ARR_SIZE>
						_trigo_osc;



/*\\ FORBIDDEN MEMBER FUNCTIONS \\\\\\\\\\\\\\\\\\\\\\*/

private:

						FFTRealFixLen (const FFTRealFixLen &other);
	FFTRealFixLen&	operator = (const FFTRealFixLen &other);
	bool				operator == (const FFTRealFixLen &other);
	bool				operator != (const FFTRealFixLen &other);

};	// class FFTRealFixLen



}	// namespace ffft



/*****************************************************************************

        FFTRealFixLen.hpp
        By Laurent de Soras

--- Legal stuff ---

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://sam.zoy.org/wtfpl/COPYING for more details.

*Tab=3***********************************************************************/



#if defined (ffft_FFTRealFixLen_CURRENT_CODEHEADER)
	#error Recursive inclusion of FFTRealFixLen code header.
#endif
#define	ffft_FFTRealFixLen_CURRENT_CODEHEADER

#if ! defined (ffft_FFTRealFixLen_CODEHEADER_INCLUDED)
#define	ffft_FFTRealFixLen_CODEHEADER_INCLUDED



/*\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\*/


/*****************************************************************************

        FFTRealPassDirect.h
        By Laurent de Soras

--- Legal stuff ---

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://sam.zoy.org/wtfpl/COPYING for more details.

*Tab=3***********************************************************************/



#if ! defined (ffft_FFTRealPassDirect_HEADER_INCLUDED)
#define	ffft_FFTRealPassDirect_HEADER_INCLUDED

#if defined (_MSC_VER)
	#pragma once
	#pragma warning (4 : 4250) // "Inherits via dominance."
#endif



/*\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\*/







namespace ffft
{



template <int PASS>
class FFTRealPassDirect
{

/*\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

public:

   typedef	FFTRealFixLenParam::DataType	DataType;
	typedef	OscSinCos <DataType>	OscType;

	ffft_FORCEINLINE static void
						process (long len, DataType dest_ptr [], DataType src_ptr [], const DataType x_ptr [], const DataType cos_ptr [], long cos_len, const long br_ptr [], OscType osc_list []);



/*\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

protected:



/*\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

private:



/*\\ FORBIDDEN MEMBER FUNCTIONS \\\\\\\\\\\\\\\\\\\\\\*/

private:

						FFTRealPassDirect ();
						FFTRealPassDirect (const FFTRealPassDirect &other);
	FFTRealPassDirect &
						operator = (const FFTRealPassDirect &other);
	bool				operator == (const FFTRealPassDirect &other);
	bool				operator != (const FFTRealPassDirect &other);

};	// class FFTRealPassDirect



}	// namespace ffft



/*****************************************************************************

        FFTRealPassDirect.hpp
        By Laurent de Soras

--- Legal stuff ---

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://sam.zoy.org/wtfpl/COPYING for more details.

*Tab=3***********************************************************************/



#if defined (ffft_FFTRealPassDirect_CURRENT_CODEHEADER)
	#error Recursive inclusion of FFTRealPassDirect code header.
#endif
#define	ffft_FFTRealPassDirect_CURRENT_CODEHEADER

#if ! defined (ffft_FFTRealPassDirect_CODEHEADER_INCLUDED)
#define	ffft_FFTRealPassDirect_CODEHEADER_INCLUDED



/*\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

/*****************************************************************************

        FFTRealUseTrigo.h
        By Laurent de Soras

--- Legal stuff ---

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://sam.zoy.org/wtfpl/COPYING for more details.

*Tab=3***********************************************************************/



#if ! defined (ffft_FFTRealUseTrigo_HEADER_INCLUDED)
#define	ffft_FFTRealUseTrigo_HEADER_INCLUDED

#if defined (_MSC_VER)
	#pragma once
	#pragma warning (4 : 4250) // "Inherits via dominance."
#endif



/*\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\*/







namespace ffft
{



template <int ALGO>
class FFTRealUseTrigo
{

/*\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

public:

   typedef	FFTRealFixLenParam::DataType	DataType;
	typedef	OscSinCos <DataType>	OscType;

	ffft_FORCEINLINE static void
						prepare (OscType &osc);
	ffft_FORCEINLINE	static void
						iterate (OscType &osc, DataType &c, DataType &s, const DataType cos_ptr [], long index_c, long index_s);



/*\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

protected:



/*\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

private:



/*\\ FORBIDDEN MEMBER FUNCTIONS \\\\\\\\\\\\\\\\\\\\\\*/

private:

						FFTRealUseTrigo ();
						~FFTRealUseTrigo ();
						FFTRealUseTrigo (const FFTRealUseTrigo &other);
	FFTRealUseTrigo &
						operator = (const FFTRealUseTrigo &other);
	bool				operator == (const FFTRealUseTrigo &other);
	bool				operator != (const FFTRealUseTrigo &other);

};	// class FFTRealUseTrigo



}	// namespace ffft



/*****************************************************************************

        FFTRealUseTrigo.hpp
        By Laurent de Soras

--- Legal stuff ---

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://sam.zoy.org/wtfpl/COPYING for more details.

*Tab=3***********************************************************************/



#if defined (ffft_FFTRealUseTrigo_CURRENT_CODEHEADER)
	#error Recursive inclusion of FFTRealUseTrigo code header.
#endif
#define	ffft_FFTRealUseTrigo_CURRENT_CODEHEADER

#if ! defined (ffft_FFTRealUseTrigo_CODEHEADER_INCLUDED)
#define	ffft_FFTRealUseTrigo_CODEHEADER_INCLUDED



/*\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\*/





namespace ffft
{



/*\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



template <int ALGO>
void	FFTRealUseTrigo <ALGO>::prepare (OscType &osc)
{
	osc.clear_buffers ();
}

template <>
inline void	FFTRealUseTrigo <0>::prepare (OscType &osc)
{
	// Nothing
}



template <int ALGO>
void	FFTRealUseTrigo <ALGO>::iterate (OscType &osc, DataType &c, DataType &s, const DataType cos_ptr [], long index_c, long index_s)
{
	osc.step ();
	c = osc.get_cos ();
	s = osc.get_sin ();
}

template <>
inline void	FFTRealUseTrigo <0>::iterate (OscType &osc, DataType &c, DataType &s, const DataType cos_ptr [], long index_c, long index_s)
{
	c = cos_ptr [index_c];
	s = cos_ptr [index_s];
}



/*\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



/*\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



}	// namespace ffft



#endif	// ffft_FFTRealUseTrigo_CODEHEADER_INCLUDED

#undef ffft_FFTRealUseTrigo_CURRENT_CODEHEADER



/*\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/




#endif	// ffft_FFTRealUseTrigo_HEADER_INCLUDED



/*\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/




namespace ffft
{



/*\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



template <>
inline void	FFTRealPassDirect <1>::process (long len, DataType dest_ptr [], DataType src_ptr [], const DataType x_ptr [], const DataType cos_ptr [], long cos_len, const long br_ptr [], OscType osc_list [])
{
	// First and second pass at once
	const long		qlen = len >> 2;

	long				coef_index = 0;
	do
	{
		// To do: unroll the loop (2x).
		const long		ri_0 = br_ptr [coef_index >> 2];
		const long		ri_1 = ri_0 + 2 * qlen;	// bit_rev_lut_ptr [coef_index + 1];
		const long		ri_2 = ri_0 + 1 * qlen;	// bit_rev_lut_ptr [coef_index + 2];
		const long		ri_3 = ri_0 + 3 * qlen;	// bit_rev_lut_ptr [coef_index + 3];

		DataType	* const	df2 = dest_ptr + coef_index;
		df2 [1] = x_ptr [ri_0] - x_ptr [ri_1];
		df2 [3] = x_ptr [ri_2] - x_ptr [ri_3];

		const DataType	sf_0 = x_ptr [ri_0] + x_ptr [ri_1];
		const DataType	sf_2 = x_ptr [ri_2] + x_ptr [ri_3];

		df2 [0] = sf_0 + sf_2;
		df2 [2] = sf_0 - sf_2;

		coef_index += 4;
	}
	while (coef_index < len);
}

template <>
inline void	FFTRealPassDirect <2>::process (long len, DataType dest_ptr [], DataType src_ptr [], const DataType x_ptr [], const DataType cos_ptr [], long cos_len, const long br_ptr [], OscType osc_list [])
{
	// Executes "previous" passes first. Inverts source and destination buffers
	FFTRealPassDirect <1>::process (
		len,
		src_ptr,
		dest_ptr,
		x_ptr,
		cos_ptr,
		cos_len,
		br_ptr,
		osc_list
	);

	// Third pass
	const DataType	sqrt2_2 = DataType (SQRT2 * 0.5);

	long				coef_index = 0;
	do
	{
		dest_ptr [coef_index    ] = src_ptr [coef_index] + src_ptr [coef_index + 4];
		dest_ptr [coef_index + 4] = src_ptr [coef_index] - src_ptr [coef_index + 4];
		dest_ptr [coef_index + 2] = src_ptr [coef_index + 2];
		dest_ptr [coef_index + 6] = src_ptr [coef_index + 6];

		DataType			v;

		v = (src_ptr [coef_index + 5] - src_ptr [coef_index + 7]) * sqrt2_2;
		dest_ptr [coef_index + 1] = src_ptr [coef_index + 1] + v;
		dest_ptr [coef_index + 3] = src_ptr [coef_index + 1] - v;

		v = (src_ptr [coef_index + 5] + src_ptr [coef_index + 7]) * sqrt2_2;
		dest_ptr [coef_index + 5] = v + src_ptr [coef_index + 3];
		dest_ptr [coef_index + 7] = v - src_ptr [coef_index + 3];

		coef_index += 8;
	}
	while (coef_index < len);
}

template <int PASS>
void	FFTRealPassDirect <PASS>::process (long len, DataType dest_ptr [], DataType src_ptr [], const DataType x_ptr [], const DataType cos_ptr [], long cos_len, const long br_ptr [], OscType osc_list [])
{
	// Executes "previous" passes first. Inverts source and destination buffers
	FFTRealPassDirect <PASS - 1>::process (
		len,
		src_ptr,
		dest_ptr,
		x_ptr,
		cos_ptr,
		cos_len,
		br_ptr,
		osc_list
	);

	const long		dist = 1L << (PASS - 1);
	const long		c1_r = 0;
	const long		c1_i = dist;
	const long		c2_r = dist * 2;
	const long		c2_i = dist * 3;
	const long		cend = dist * 4;
	const long		table_step = cos_len >> (PASS - 1);

   enum {	TRIGO_OSC		= PASS - FFTRealFixLenParam::TRIGO_BD_LIMIT	};
	enum {	TRIGO_DIRECT	= (TRIGO_OSC >= 0) ? 1 : 0	};

	long				coef_index = 0;
	do
	{
		const DataType	* const	sf = src_ptr + coef_index;
		DataType			* const	df = dest_ptr + coef_index;

		// Extreme coefficients are always real
		df [c1_r] = sf [c1_r] + sf [c2_r];
		df [c2_r] = sf [c1_r] - sf [c2_r];
		df [c1_i] = sf [c1_i];
		df [c2_i] = sf [c2_i];

		FFTRealUseTrigo <TRIGO_DIRECT>::prepare (osc_list [TRIGO_OSC]);

		// Others are conjugate complex numbers
		for (long i = 1; i < dist; ++ i)
		{
			DataType			c;
			DataType			s;
			FFTRealUseTrigo <TRIGO_DIRECT>::iterate (
				osc_list [TRIGO_OSC],
				c,
				s,
				cos_ptr,
				i * table_step,
				(dist - i) * table_step
			);

			const DataType	sf_r_i = sf [c1_r + i];
			const DataType	sf_i_i = sf [c1_i + i];

			const DataType	v1 = sf [c2_r + i] * c - sf [c2_i + i] * s;
			df [c1_r + i] = sf_r_i + v1;
			df [c2_r - i] = sf_r_i - v1;

			const DataType	v2 = sf [c2_r + i] * s + sf [c2_i + i] * c;
			df [c2_r + i] = v2 + sf_i_i;
			df [cend - i] = v2 - sf_i_i;
		}

		coef_index += cend;
	}
	while (coef_index < len);
}



/*\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



/*\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



}	// namespace ffft



#endif	// ffft_FFTRealPassDirect_CODEHEADER_INCLUDED

#undef ffft_FFTRealPassDirect_CURRENT_CODEHEADER



/*\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/




#endif	// ffft_FFTRealPassDirect_HEADER_INCLUDED



/*\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

/*****************************************************************************

        FFTRealPassInverse.h
        By Laurent de Soras

--- Legal stuff ---

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://sam.zoy.org/wtfpl/COPYING for more details.

*Tab=3***********************************************************************/



#if ! defined (ffft_FFTRealPassInverse_HEADER_INCLUDED)
#define	ffft_FFTRealPassInverse_HEADER_INCLUDED

#if defined (_MSC_VER)
	#pragma once
	#pragma warning (4 : 4250) // "Inherits via dominance."
#endif



/*\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\*/








namespace ffft
{



template <int PASS>
class FFTRealPassInverse
{

/*\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

public:

   typedef	FFTRealFixLenParam::DataType	DataType;
	typedef	OscSinCos <DataType>	OscType;

	ffft_FORCEINLINE static void
						process (long len, DataType dest_ptr [], DataType src_ptr [], const DataType f_ptr [], const DataType cos_ptr [], long cos_len, const long br_ptr [], OscType osc_list []);
	ffft_FORCEINLINE static void
						process_rec (long len, DataType dest_ptr [], DataType src_ptr [], const DataType cos_ptr [], long cos_len, const long br_ptr [], OscType osc_list []);
	ffft_FORCEINLINE static void
						process_internal (long len, DataType dest_ptr [], const DataType src_ptr [], const DataType cos_ptr [], long cos_len, const long br_ptr [], OscType osc_list []);



/*\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

protected:



/*\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

private:



/*\\ FORBIDDEN MEMBER FUNCTIONS \\\\\\\\\\\\\\\\\\\\\\*/

private:

						FFTRealPassInverse ();
						FFTRealPassInverse (const FFTRealPassInverse &other);
	FFTRealPassInverse &
						operator = (const FFTRealPassInverse &other);
	bool				operator == (const FFTRealPassInverse &other);
	bool				operator != (const FFTRealPassInverse &other);

};	// class FFTRealPassInverse



}	// namespace ffft



/*****************************************************************************

        FFTRealPassInverse.hpp
        By Laurent de Soras

--- Legal stuff ---

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://sam.zoy.org/wtfpl/COPYING for more details.

*Tab=3***********************************************************************/



#if defined (ffft_FFTRealPassInverse_CURRENT_CODEHEADER)
	#error Recursive inclusion of FFTRealPassInverse code header.
#endif
#define	ffft_FFTRealPassInverse_CURRENT_CODEHEADER

#if ! defined (ffft_FFTRealPassInverse_CODEHEADER_INCLUDED)
#define	ffft_FFTRealPassInverse_CODEHEADER_INCLUDED



/*\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

/*****************************************************************************

        FFTRealUseTrigo.h
        By Laurent de Soras

--- Legal stuff ---

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://sam.zoy.org/wtfpl/COPYING for more details.

*Tab=3***********************************************************************/



#if ! defined (ffft_FFTRealUseTrigo_HEADER_INCLUDED)
#define	ffft_FFTRealUseTrigo_HEADER_INCLUDED

#if defined (_MSC_VER)
	#pragma once
	#pragma warning (4 : 4250) // "Inherits via dominance."
#endif



/*\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\*/







namespace ffft
{



template <int ALGO>
class FFTRealUseTrigo
{

/*\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

public:

   typedef	FFTRealFixLenParam::DataType	DataType;
	typedef	OscSinCos <DataType>	OscType;

	ffft_FORCEINLINE static void
						prepare (OscType &osc);
	ffft_FORCEINLINE	static void
						iterate (OscType &osc, DataType &c, DataType &s, const DataType cos_ptr [], long index_c, long index_s);



/*\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

protected:



/*\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

private:



/*\\ FORBIDDEN MEMBER FUNCTIONS \\\\\\\\\\\\\\\\\\\\\\*/

private:

						FFTRealUseTrigo ();
						~FFTRealUseTrigo ();
						FFTRealUseTrigo (const FFTRealUseTrigo &other);
	FFTRealUseTrigo &
						operator = (const FFTRealUseTrigo &other);
	bool				operator == (const FFTRealUseTrigo &other);
	bool				operator != (const FFTRealUseTrigo &other);

};	// class FFTRealUseTrigo



}	// namespace ffft



/*****************************************************************************

        FFTRealUseTrigo.hpp
        By Laurent de Soras

--- Legal stuff ---

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://sam.zoy.org/wtfpl/COPYING for more details.

*Tab=3***********************************************************************/



#if defined (ffft_FFTRealUseTrigo_CURRENT_CODEHEADER)
	#error Recursive inclusion of FFTRealUseTrigo code header.
#endif
#define	ffft_FFTRealUseTrigo_CURRENT_CODEHEADER

#if ! defined (ffft_FFTRealUseTrigo_CODEHEADER_INCLUDED)
#define	ffft_FFTRealUseTrigo_CODEHEADER_INCLUDED



/*\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\*/





namespace ffft
{



/*\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



template <int ALGO>
void	FFTRealUseTrigo <ALGO>::prepare (OscType &osc)
{
	osc.clear_buffers ();
}

template <>
inline void	FFTRealUseTrigo <0>::prepare (OscType &osc)
{
	// Nothing
}



template <int ALGO>
void	FFTRealUseTrigo <ALGO>::iterate (OscType &osc, DataType &c, DataType &s, const DataType cos_ptr [], long index_c, long index_s)
{
	osc.step ();
	c = osc.get_cos ();
	s = osc.get_sin ();
}

template <>
inline void	FFTRealUseTrigo <0>::iterate (OscType &osc, DataType &c, DataType &s, const DataType cos_ptr [], long index_c, long index_s)
{
	c = cos_ptr [index_c];
	s = cos_ptr [index_s];
}



/*\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



/*\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



}	// namespace ffft



#endif	// ffft_FFTRealUseTrigo_CODEHEADER_INCLUDED

#undef ffft_FFTRealUseTrigo_CURRENT_CODEHEADER



/*\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/




#endif	// ffft_FFTRealUseTrigo_HEADER_INCLUDED



/*\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/




namespace ffft
{



/*\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



template <int PASS>
void	FFTRealPassInverse <PASS>::process (long len, DataType dest_ptr [], DataType src_ptr [], const DataType f_ptr [], const DataType cos_ptr [], long cos_len, const long br_ptr [], OscType osc_list [])
{
	process_internal (
		len,
		dest_ptr,
		f_ptr,
		cos_ptr,
		cos_len,
		br_ptr,
		osc_list
	);
	FFTRealPassInverse <PASS - 1>::process_rec (
		len,
		src_ptr,
		dest_ptr,
		cos_ptr,
		cos_len,
		br_ptr,
		osc_list
	);
}



template <int PASS>
void	FFTRealPassInverse <PASS>::process_rec (long len, DataType dest_ptr [], DataType src_ptr [], const DataType cos_ptr [], long cos_len, const long br_ptr [], OscType osc_list [])
{
	process_internal (
		len,
		dest_ptr,
		src_ptr,
		cos_ptr,
		cos_len,
		br_ptr,
		osc_list
	);
	FFTRealPassInverse <PASS - 1>::process_rec (
		len,
		src_ptr,
		dest_ptr,
		cos_ptr,
		cos_len,
		br_ptr,
		osc_list
	);
}

template <>
inline void	FFTRealPassInverse <0>::process_rec (long len, DataType dest_ptr [], DataType src_ptr [], const DataType cos_ptr [], long cos_len, const long br_ptr [], OscType osc_list [])
{
	// Stops recursion
}



template <int PASS>
void	FFTRealPassInverse <PASS>::process_internal (long len, DataType dest_ptr [], const DataType src_ptr [], const DataType cos_ptr [], long cos_len, const long br_ptr [], OscType osc_list [])
{
	const long		dist = 1L << (PASS - 1);
	const long		c1_r = 0;
	const long		c1_i = dist;
	const long		c2_r = dist * 2;
	const long		c2_i = dist * 3;
	const long		cend = dist * 4;
	const long		table_step = cos_len >> (PASS - 1);

   enum {	TRIGO_OSC		= PASS - FFTRealFixLenParam::TRIGO_BD_LIMIT	};
	enum {	TRIGO_DIRECT	= (TRIGO_OSC >= 0) ? 1 : 0	};

	long				coef_index = 0;
	do
	{
		const DataType	* const	sf = src_ptr + coef_index;
		DataType			* const	df = dest_ptr + coef_index;

		// Extreme coefficients are always real
		df [c1_r] = sf [c1_r] + sf [c2_r];
		df [c2_r] = sf [c1_r] - sf [c2_r];
		df [c1_i] = sf [c1_i] * 2;
		df [c2_i] = sf [c2_i] * 2;

		FFTRealUseTrigo <TRIGO_DIRECT>::prepare (osc_list [TRIGO_OSC]);

		// Others are conjugate complex numbers
		for (long i = 1; i < dist; ++ i)
		{
			df [c1_r + i] = sf [c1_r + i] + sf [c2_r - i];
			df [c1_i + i] = sf [c2_r + i] - sf [cend - i];

			DataType			c;
			DataType			s;
			FFTRealUseTrigo <TRIGO_DIRECT>::iterate (
				osc_list [TRIGO_OSC],
				c,
				s,
				cos_ptr,
				i * table_step,
				(dist - i) * table_step
			);

			const DataType	vr = sf [c1_r + i] - sf [c2_r - i];
			const DataType	vi = sf [c2_r + i] + sf [cend - i];

			df [c2_r + i] = vr * c + vi * s;
			df [c2_i + i] = vi * c - vr * s;
		}

		coef_index += cend;
	}
	while (coef_index < len);
}

template <>
inline void	FFTRealPassInverse <2>::process_internal (long len, DataType dest_ptr [], const DataType src_ptr [], const DataType cos_ptr [], long cos_len, const long br_ptr [], OscType osc_list [])
{
	// Antepenultimate pass
	const DataType	sqrt2_2 = DataType (SQRT2 * 0.5);

	long				coef_index = 0;
	do
	{
		dest_ptr [coef_index    ] = src_ptr [coef_index] + src_ptr [coef_index + 4];
		dest_ptr [coef_index + 4] = src_ptr [coef_index] - src_ptr [coef_index + 4];
		dest_ptr [coef_index + 2] = src_ptr [coef_index + 2] * 2;
		dest_ptr [coef_index + 6] = src_ptr [coef_index + 6] * 2;

		dest_ptr [coef_index + 1] = src_ptr [coef_index + 1] + src_ptr [coef_index + 3];
		dest_ptr [coef_index + 3] = src_ptr [coef_index + 5] - src_ptr [coef_index + 7];

		const DataType	vr = src_ptr [coef_index + 1] - src_ptr [coef_index + 3];
		const DataType	vi = src_ptr [coef_index + 5] + src_ptr [coef_index + 7];

		dest_ptr [coef_index + 5] = (vr + vi) * sqrt2_2;
		dest_ptr [coef_index + 7] = (vi - vr) * sqrt2_2;

		coef_index += 8;
	}
	while (coef_index < len);
}

template <>
inline void	FFTRealPassInverse <1>::process_internal (long len, DataType dest_ptr [], const DataType src_ptr [], const DataType cos_ptr [], long cos_len, const long br_ptr [], OscType osc_list [])
{
	// Penultimate and last pass at once
	const long		qlen = len >> 2;

	long				coef_index = 0;
	do
	{
		const long		ri_0 = br_ptr [coef_index >> 2];

		const DataType	b_0 = src_ptr [coef_index    ] + src_ptr [coef_index + 2];
		const DataType	b_2 = src_ptr [coef_index    ] - src_ptr [coef_index + 2];
		const DataType	b_1 = src_ptr [coef_index + 1] * 2;
		const DataType	b_3 = src_ptr [coef_index + 3] * 2;

		dest_ptr [ri_0           ] = b_0 + b_1;
		dest_ptr [ri_0 + 2 * qlen] = b_0 - b_1;
		dest_ptr [ri_0 + 1 * qlen] = b_2 + b_3;
		dest_ptr [ri_0 + 3 * qlen] = b_2 - b_3;

		coef_index += 4;
	}
	while (coef_index < len);
}



/*\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



/*\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



}	// namespace ffft



#endif	// ffft_FFTRealPassInverse_CODEHEADER_INCLUDED

#undef ffft_FFTRealPassInverse_CURRENT_CODEHEADER



/*\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/




#endif	// ffft_FFTRealPassInverse_HEADER_INCLUDED



/*\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

/*****************************************************************************

        FFTRealSelect.h
        By Laurent de Soras

--- Legal stuff ---

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://sam.zoy.org/wtfpl/COPYING for more details.

*Tab=3***********************************************************************/



#if ! defined (ffft_FFTRealSelect_HEADER_INCLUDED)
#define	ffft_FFTRealSelect_HEADER_INCLUDED

#if defined (_MSC_VER)
	#pragma once
#endif



/*\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\*/





namespace ffft
{



template <int P>
class FFTRealSelect
{

/*\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

public:

	ffft_FORCEINLINE static float *
						sel_bin (float *e_ptr, float *o_ptr);



/*\\ FORBIDDEN MEMBER FUNCTIONS \\\\\\\\\\\\\\\\\\\\\\*/

private:

						FFTRealSelect ();
						~FFTRealSelect ();
						FFTRealSelect (const FFTRealSelect &other);
	FFTRealSelect&	operator = (const FFTRealSelect &other);
	bool				operator == (const FFTRealSelect &other);
	bool				operator != (const FFTRealSelect &other);

};	// class FFTRealSelect



}	// namespace ffft



/*****************************************************************************

        FFTRealSelect.hpp
        By Laurent de Soras

--- Legal stuff ---

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://sam.zoy.org/wtfpl/COPYING for more details.

*Tab=3***********************************************************************/



#if defined (ffft_FFTRealSelect_CURRENT_CODEHEADER)
	#error Recursive inclusion of FFTRealSelect code header.
#endif
#define	ffft_FFTRealSelect_CURRENT_CODEHEADER

#if ! defined (ffft_FFTRealSelect_CODEHEADER_INCLUDED)
#define	ffft_FFTRealSelect_CODEHEADER_INCLUDED



namespace ffft
{



/*\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



template <int P>
float *	FFTRealSelect <P>::sel_bin (float *e_ptr, float *o_ptr)
{
	return (o_ptr);
}



template <>
inline float *	FFTRealSelect <0>::sel_bin (float *e_ptr, float *o_ptr)
{
	return (e_ptr);
}



}	// namespace ffft



#endif	// ffft_FFTRealSelect_CODEHEADER_INCLUDED

#undef ffft_FFTRealSelect_CURRENT_CODEHEADER



/*\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/




#endif	// ffft_FFTRealSelect_HEADER_INCLUDED



/*\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/


namespace std { }



namespace ffft
{



/*\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



template <int LL2>
FFTRealFixLen <LL2>::FFTRealFixLen ()
:	_buffer (FFT_LEN)
,	_br_data (BR_ARR_SIZE)
,	_trigo_data (TRIGO_TABLE_ARR_SIZE)
,	_trigo_osc ()
{
	build_br_lut ();
	build_trigo_lut ();
	build_trigo_osc ();
}



template <int LL2>
long	FFTRealFixLen <LL2>::get_length () const
{
	return (FFT_LEN);
}



// General case
template <int LL2>
void	FFTRealFixLen <LL2>::do_fft (DataType f [], const DataType x [])
{
	assert (f != 0);
	assert (x != 0);
	assert (x != f);
	assert (FFT_LEN_L2 >= 3);

	// Do the transform in several passes
	const DataType	*	cos_ptr = &_trigo_data [0];
	const long *	br_ptr = &_br_data [0];

	FFTRealPassDirect <FFT_LEN_L2 - 1>::process (
		FFT_LEN,
		f,
		&_buffer [0],
		x,
		cos_ptr,
		TRIGO_TABLE_ARR_SIZE,
		br_ptr,
		&_trigo_osc [0]
	);
}

// 4-point FFT
template <>
inline void	FFTRealFixLen <2>::do_fft (DataType f [], const DataType x [])
{
	assert (f != 0);
	assert (x != 0);
	assert (x != f);

	f [1] = x [0] - x [2];
	f [3] = x [1] - x [3];

	const DataType	b_0 = x [0] + x [2];
	const DataType	b_2 = x [1] + x [3];
	
	f [0] = b_0 + b_2;
	f [2] = b_0 - b_2;
}

// 2-point FFT
template <>
inline void	FFTRealFixLen <1>::do_fft (DataType f [], const DataType x [])
{
	assert (f != 0);
	assert (x != 0);
	assert (x != f);

	f [0] = x [0] + x [1];
	f [1] = x [0] - x [1];
}

// 1-point FFT
template <>
inline void	FFTRealFixLen <0>::do_fft (DataType f [], const DataType x [])
{
	assert (f != 0);
	assert (x != 0);

	f [0] = x [0];
}



// General case
template <int LL2>
void	FFTRealFixLen <LL2>::do_ifft (const DataType f [], DataType x [])
{
	assert (f != 0);
	assert (x != 0);
	assert (x != f);
	assert (FFT_LEN_L2 >= 3);

	// Do the transform in several passes
	DataType *		s_ptr =
		FFTRealSelect <FFT_LEN_L2 & 1>::sel_bin (&_buffer [0], x);
	DataType *		d_ptr =
		FFTRealSelect <FFT_LEN_L2 & 1>::sel_bin (x, &_buffer [0]);
	const DataType	*	cos_ptr = &_trigo_data [0];
	const long *	br_ptr = &_br_data [0];

	FFTRealPassInverse <FFT_LEN_L2 - 1>::process (
		FFT_LEN,
		d_ptr,
		s_ptr,
		f,
		cos_ptr,
		TRIGO_TABLE_ARR_SIZE,
		br_ptr,
		&_trigo_osc [0]
	);
}

// 4-point IFFT
template <>
inline void	FFTRealFixLen <2>::do_ifft (const DataType f [], DataType x [])
{
	assert (f != 0);
	assert (x != 0);
	assert (x != f);

	const DataType	b_0 = f [0] + f [2];
	const DataType	b_2 = f [0] - f [2];

	x [0] = b_0 + f [1] * 2;
	x [2] = b_0 - f [1] * 2;
	x [1] = b_2 + f [3] * 2;
	x [3] = b_2 - f [3] * 2;
}

// 2-point IFFT
template <>
inline void	FFTRealFixLen <1>::do_ifft (const DataType f [], DataType x [])
{
	assert (f != 0);
	assert (x != 0);
	assert (x != f);

	x [0] = f [0] + f [1];
	x [1] = f [0] - f [1];
}

// 1-point IFFT
template <>
inline void	FFTRealFixLen <0>::do_ifft (const DataType f [], DataType x [])
{
	assert (f != 0);
	assert (x != 0);
	assert (x != f);

	x [0] = f [0];
}




template <int LL2>
void	FFTRealFixLen <LL2>::rescale (DataType x []) const
{
	assert (x != 0);

	const DataType	mul = DataType (1.0 / FFT_LEN);

	if (FFT_LEN < 4)
	{
		long				i = FFT_LEN - 1;
		do
		{
			x [i] *= mul;
			--i;
		}
		while (i >= 0);
	}

	else
	{
		assert ((FFT_LEN & 3) == 0);

		// Could be optimized with SIMD instruction sets (needs alignment check)
		long				i = FFT_LEN - 4;
		do
		{
			x [i + 0] *= mul;
			x [i + 1] *= mul;
			x [i + 2] *= mul;
			x [i + 3] *= mul;
			i -= 4;
		}
		while (i >= 0);
	}
}



/*\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



/*\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



template <int LL2>
void	FFTRealFixLen <LL2>::build_br_lut ()
{
	_br_data [0] = 0;
	for (long cnt = 1; cnt < BR_ARR_SIZE; ++cnt)
	{
		long				index = cnt << 2;
		long				br_index = 0;

		int				bit_cnt = FFT_LEN_L2;
		do
		{
			br_index <<= 1;
			br_index += (index & 1);
			index >>= 1;

			-- bit_cnt;
		}
		while (bit_cnt > 0);

		_br_data [cnt] = br_index;
	}
}



template <int LL2>
void	FFTRealFixLen <LL2>::build_trigo_lut ()
{
	const double	mul = (0.5 * PI) / TRIGO_TABLE_ARR_SIZE;
	for (long i = 0; i < TRIGO_TABLE_ARR_SIZE; ++ i)
	{
		using namespace std;

		_trigo_data [i] = DataType (cos (i * mul));
	}
}



template <int LL2>
void	FFTRealFixLen <LL2>::build_trigo_osc ()
{
	for (int i = 0; i < NBR_TRIGO_OSC; ++i)
	{
		OscType &		osc = _trigo_osc [i];

		const long		len = static_cast <long> (TRIGO_TABLE_ARR_SIZE) << (i + 1);
		const double	mul = (0.5 * PI) / len;
		osc.set_step (mul);
	}
}



}	// namespace ffft



#endif	// ffft_FFTRealFixLen_CODEHEADER_INCLUDED

#undef ffft_FFTRealFixLen_CURRENT_CODEHEADER



/*\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/




#endif	// ffft_FFTRealFixLen_HEADER_INCLUDED



/*\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
