// Copyright (C) 2003-2009 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#include <math.h>
#include <limits>

#ifdef _WIN32
#define _interlockedbittestandset workaround_ms_header_bug_platform_sdk6_set
#define _interlockedbittestandreset workaround_ms_header_bug_platform_sdk6_reset
#define _interlockedbittestandset64 workaround_ms_header_bug_platform_sdk6_set64
#define _interlockedbittestandreset64 workaround_ms_header_bug_platform_sdk6_reset64
#include <intrin.h>
#undef _interlockedbittestandset
#undef _interlockedbittestandreset
#undef _interlockedbittestandset64
#undef _interlockedbittestandreset64
#else
#include <xmmintrin.h>
#endif

#include "../../Core.h"
#include "Interpreter.h"

// SUPER MONKEY BALL IS BEING A ROYAL PAIN
// We are missing the caller of 800070ec
// POSSIBLE APPROACHES:
// * Full SW FPU. Urgh.
// * Partial SW FPU, emulate just as much as necessary for monkey ball. Feasible but a lot of work.
// * HLE hacking. Figure out what all the evil functions really do and fake them. DONE (well, works okay-ish)

// Interesting places in Super Monkey Ball:
// 80036654: fctwixz stuff
// 80007e08:
//	-98: Various entry points that loads various odd fp values into f1
// 800070b0: Estimate inverse square root.
// 800070ec: Examine f1. Reads a value out of locked cache into f2 (fixed address). Some cases causes us to call the above thing.
//           If all goes well, jump to 70b0, which estimates the inverse square root. 
//           Then multiply the loaded variable with the original value of f1. Result should be the square root. (1 / sqrt(x)) * x  = x / sqrt(x) = sqrt(x)
// 8000712c: Similar, but does not do the multiply at the end, just an frspx.
// 8000716c: Sort of similar, but has extra junk at the end.
//
// 
// 800072a4 - nightmare of nightmares
// Fun stuff used:
// bso+
// mcrfs (ARGH pulls stuff out of .. FPSCR). it uses this to check the result of frsp mostly (!!!!)
// crclr
// crset
// crxor
// fnabs
// Super Monkey Ball reads FPRF & friends after fmadds, fmuls, frspx
// WHY do the FR & FI flags affect it so much?

namespace Interpreter
{

void UpdateFPSCR(UReg_FPSCR fp);
void UpdateSSEState();


// start of unit test - Dolphin needs more of these!
/*
void TestFPRF()
{
	UpdateFPRF(1.0);
	if (FPSCR.FPRF != 0x4)
		PanicAlert("Error 1");
	UpdateFPRF(-1.0);
	if (FPSCR.FPRF != 0x8)
		PanicAlert("Error 2");
	PanicAlert("Test done");
}*/


// extremely rare
void Helper_UpdateCR1(double _fValue)
{
	// Should just update exception flags, not do any compares.
	PanicAlert("CR1");
}

inline bool IsNAN(double _dValue) 
{ 
	return _dValue != _dValue; 
}

inline bool _IsNAN(float x) {
	//return ((*(u32*)&x) & 0x7f800000UL) == 0x7f800000UL && ((*(u32*)&x) & 0x007fffffUL);
	return x != x;
}

void fcmpo(UGeckoInstruction _inst)
{
	/*
	float fa = static_cast<float>(rPS0(_inst.FA));
	float fb = static_cast<float>(rPS0(_inst.FB));
	// normalize
	if (((*(u32*)&fa) & 0x7f800000UL) == 0) (*(u32*)&fa) &= 0x80000000UL;
	if (((*(u32*)&fb) & 0x7f800000UL) == 0) (*(u32*)&fb) &= 0x80000000UL;
	*/

	// normalize if conversion to float gives denormalized number
	if ((riPS0(_inst.FA) & 0x7ff0000000000000ULL) < 0x3800000000000000ULL)
		riPS0(_inst.FA) &= 0x8000000000000000ULL;
	if ((riPS0(_inst.FB) & 0x7ff0000000000000ULL) < 0x3800000000000000ULL)
		riPS0(_inst.FB) &= 0x8000000000000000ULL;
	double fa =	rPS0(_inst.FA);
	double fb =	rPS0(_inst.FB);

	u32 compareResult;
	if (IsNAN(fa) || IsNAN(fb))  compareResult = 1;
	else if (fa < fb)            compareResult = 8; 
	else if (fa > fb)            compareResult = 4; 
	else                         compareResult = 2;

	FPSCR.FPRF = compareResult;
	SetCRField(_inst.CRFD, compareResult);

/* missing part
	if ((frA) is an SNaN or (frB) is an SNaN )
		then VXSNAN � 1
		if VE = 0
			then VXVC � 1
		else if ((frA) is a QNaN or (frB) is a QNaN )
		then VXVC � 1 */
}

void fcmpu(UGeckoInstruction _inst)
{
	

	/*
	float fa = static_cast<float>(rPS0(_inst.FA));
	float fb = static_cast<float>(rPS0(_inst.FB));
	// normalize
	if (((*(u32*)&fa) & 0x7f800000UL) == 0) (*(u32*)&fa) &= 0x80000000UL;
	if (((*(u32*)&fb) & 0x7f800000UL) == 0) (*(u32*)&fb) &= 0x80000000UL;
	*/

	// normalize if conversion to float gives denormalized number
	if ((riPS0(_inst.FA) & 0x7ff0000000000000ULL) < 0x3800000000000000ULL)
		riPS0(_inst.FA) &= 0x8000000000000000ULL;
	if ((riPS0(_inst.FB) & 0x7ff0000000000000ULL) < 0x3800000000000000ULL)
		riPS0(_inst.FB) &= 0x8000000000000000ULL;
	double fa =	rPS0(_inst.FA);
	double fb =	rPS0(_inst.FB);

	u32 compareResult;
	if (IsNAN(fa) || IsNAN(fb))  compareResult = 1; 
	else if (fa < fb)            compareResult = 8; 
	else if (fa > fb)            compareResult = 4; 
	else                         compareResult = 2;

	FPSCR.FPRF = compareResult;
	SetCRField(_inst.CRFD, compareResult);

/* missing part
	if ((frA) is an SNaN or (frB) is an SNaN)
		then VXSNAN � 1 */
}

// Apply current rounding mode
void fctiwx(UGeckoInstruction _inst)
{
	//UpdateSSEState();
	const double b = rPS0(_inst.FB);
	u32 value;
	if (b > (double)0x7fffffff)
	{
		value = 0x7fffffff;
		FPSCR.VXCVI = 1;
	}
	else if (b < -(double)0x7fffffff) 
	{
		value = 0x80000000; 
		FPSCR.VXCVI = 1;
	}
	else
	{
		value = (u32)(s32)_mm_cvtsd_si32(_mm_set_sd(b));  // obey current rounding mode
//		double d_value = (double)value;
//		bool inexact = (d_value != b);
//		FPSCR.FI = inexact ? 1 : 0;
//		FPSCR.XX |= FPSCR.FI;
//		FPSCR.FR = fabs(d_value) > fabs(b);
	}

	//TODO: FR
	//FPRF undefined

	riPS0(_inst.FD) = (u64)value; // zero extend
	if (_inst.Rc) 
		Helper_UpdateCR1(rPS0(_inst.FD));
}

/*
In the float -> int direction, floating point input values larger than the largest 
representable int result in 0x80000000 (a very negative number) rather than the 
largest representable int on PowerPC. */

// Always round toward zero
void fctiwzx(UGeckoInstruction _inst)
{
	//UpdateSSEState();
	const double b = rPS0(_inst.FB);
	u32 value;
	if (b > (double)0x7fffffff)
	{
		value = 0x7fffffff;
		FPSCR.VXCVI = 1;
	}
	else if (b < -(double)0x7fffffff)
	{
		value = 0x80000000;
		FPSCR.VXCVI = 1;
	}
	else
	{
		value = (u32)(s32)_mm_cvttsd_si32(_mm_set_sd(b)); // truncate
//		double d_value = (double)value;
//		bool inexact = (d_value != b);
//		FPSCR.FI = inexact ? 1 : 0;
//		FPSCR.XX |= FPSCR.FI;
//		FPSCR.FR = 1; //fabs(d_value) > fabs(b);
	}

	riPS0(_inst.FD) = (u64)value;
	if (_inst.Rc) 
		Helper_UpdateCR1(rPS0(_inst.FD));
}

void fmrx(UGeckoInstruction _inst)
{
	riPS0(_inst.FD) = riPS0(_inst.FB);
	// This is a binary instruction. Does not alter FPSCR
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD)); 
}

void fabsx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = fabs(rPS0(_inst.FB));
	// This is a binary instruction. Does not alter FPSCR
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}

void fnabsx(UGeckoInstruction _inst)
{
	riPS0(_inst.FD) = riPS0(_inst.FB) | (1ULL << 63);
	// This is a binary instruction. Does not alter FPSCR
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}	

void fnegx(UGeckoInstruction _inst)
{
	riPS0(_inst.FD) = riPS0(_inst.FB) ^ (1ULL << 63);
	// This is a binary instruction. Does not alter FPSCR
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}

void fselx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = (rPS0(_inst.FA) >= -0.0) ? rPS0(_inst.FC) : rPS0(_inst.FB);
	// This is a binary instruction. Does not alter FPSCR
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}


// !!! warning !!!
// PS1 must be set to the value of PS0 or DragonballZ will be f**ked up
// PS1 is said to be undefined
// Super Monkey Ball is using this to do wacky tricks so we need 100% correct emulation.
void frspx(UGeckoInstruction _inst)  // round to single
{
	if (true || FPSCR.RN != 0)
	{
		// Not used in Super Monkey Ball
		// UpdateSSEState();
		double b = rPS0(_inst.FB);
		double rounded = (double)(float)b;
		//FPSCR.FI = b != rounded;  // changing both of these affect Super Monkey Ball behaviour greatly.
		if (Core::g_CoreStartupParameter.bEnableFPRF)
			UpdateFPRF(rounded);
		rPS0(_inst.FD) = rPS1(_inst.FD) = rounded;
		return;
		// PanicAlert("frspx: FPSCR.RN=%i", FPSCR.RN);
	}

	// OK, let's try it in 100% software! Not yet working right.
	union {
		double d;
		u64 i;
	} in, out;
	in.d = rPS0(_inst.FB);
	out = in;
	int sign = (int)(in.i >> 63);
	int exp = (int)((in.i >> 52) & 0x7FF);
	u64 mantissa = in.i & 0x000FFFFFFFFFFFFFULL;
	u64 mantissa_single = mantissa & 0x000FFFFFE0000000ULL;
	u64 leftover_single = mantissa & 0x000000001FFFFFFFULL;

	// OK. First make sure that we have a "normal" number.
	if (exp >= 1 && exp <= 2046) {
		// OK. Check for overflow. TODO

		FPSCR.FI = leftover_single != 0; // Inexact
		if (leftover_single >= 0x10000000ULL) {
			//PanicAlert("rounding up");
			FPSCR.FR = 1;
			mantissa_single += 0x20000000;
			if (mantissa_single & 0x0010000000000000ULL) {
				// PanicAlert("renormalizing");
				mantissa_single >>= 1;
				exp += 1;
				// if (exp > 2046) { OVERFLOW }
			}
		}
		out.i = ((u64)sign << 63) | ((u64)exp << 52) | mantissa_single;
	} else {
		if (!exp && !mantissa) {
			// Positive or negative Zero. All is well.
			FPSCR.FI = 0;
			FPSCR.FR = 0;
		} else if (exp == 0 && mantissa) {
			// Denormalized number.
			PanicAlert("denorm");
		} else if (exp == 2047 && !mantissa) {
			// Infinite.
			//PanicAlert("infinite");
			FPSCR.FI = 1;
			FPSCR.FR = 1;
//			FPSCR.OX = 1;
		} else {
			//PanicAlert("NAN %08x %08x", in.i >> 32, in.i);
		}
	}

	UpdateFPRF(out.d);
	rPS0(_inst.FD) = rPS1(_inst.FD) = out.d;

	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}


void fmulx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = rPS0(_inst.FA) * rPS0(_inst.FC);
	FPSCR.FI = 0;
	FPSCR.FR = 1;
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD)); 
}
void fmulsx(UGeckoInstruction _inst)
{
	double d_value = rPS0(_inst.FA) * rPS0(_inst.FC);
	rPS0(_inst.FD) = rPS1(_inst.FD) = static_cast<float>(d_value);
	FPSCR.FI = d_value != rPS0(_inst.FD);
	UpdateFPRF(rPS0(_inst.FD));
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));  
}


void fmaddx(UGeckoInstruction _inst)
{
	double result = (rPS0(_inst.FA) * rPS0(_inst.FC)) + rPS0(_inst.FB);
	rPS0(_inst.FD) = result;
	UpdateFPRF(result);
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}

void fmaddsx(UGeckoInstruction _inst)
{
	double d_value = (rPS0(_inst.FA) * rPS0(_inst.FC)) + rPS0(_inst.FB);
	rPS0(_inst.FD) = rPS1(_inst.FD) = static_cast<float>(d_value);
	FPSCR.FI = d_value != rPS0(_inst.FD);
	FPSCR.FR = 0;
 	UpdateFPRF(rPS0(_inst.FD));
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD)); 
}


void faddx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = rPS0(_inst.FA) + rPS0(_inst.FB);
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}
void faddsx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = rPS1(_inst.FD) = static_cast<float>(rPS0(_inst.FA) + rPS0(_inst.FB));
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD)); 
}


void fdivx(UGeckoInstruction _inst)
{
	double a = rPS0(_inst.FA);
	double b = rPS0(_inst.FB);
	if (a == 0.0f && b == 0.0f)
		rPS0(_inst.FD) = rPS1(_inst.FD) = 0.0;  // NAN?
	else
		rPS0(_inst.FD) = rPS1(_inst.FD) = a / b;
	if (fabs(rPS0(_inst.FB)) == 0.0) {
		if (!FPSCR.ZX)
			FPSCR.FX = 1;
		FPSCR.ZX = 1;
		FPSCR.XX = 1;
	}
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}
void fdivsx(UGeckoInstruction _inst)
{
	float a = rPS0(_inst.FA);
	float b = rPS0(_inst.FB);
	if (a != a || b != b)
		rPS0(_inst.FD) = rPS1(_inst.FD) = 0.0;  // NAN?
	else
		rPS0(_inst.FD) = rPS1(_inst.FD) = a / b;
	if (b == 0.0) {
		if (!FPSCR.ZX)
			FPSCR.FX = 1;
		FPSCR.ZX = 1;
		FPSCR.XX = 1;
	}
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));  
}
void fresx(UGeckoInstruction _inst)
{
	double b = rPS0(_inst.FB);
	rPS0(_inst.FD) = rPS1(_inst.FD) = 1.0 / b;
	if (fabs(rPS0(_inst.FB)) == 0.0) {
		if (!FPSCR.ZX)
			FPSCR.FX = 1;
		FPSCR.ZX = 1;
		FPSCR.XX = 1;
	}
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD)); 
}


void fmsubx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = (rPS0(_inst.FA) * rPS0(_inst.FC)) - rPS0(_inst.FB);
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD)); 
}

void fmsubsx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = rPS1(_inst.FD) =
		static_cast<float>((rPS0(_inst.FA) * rPS0(_inst.FC)) - rPS0(_inst.FB));
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD)); 
}


void fnmaddx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = -((rPS0(_inst.FA) * rPS0(_inst.FC)) + rPS0(_inst.FB));
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}
void fnmaddsx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = rPS1(_inst.FD) = 
		static_cast<float>(-((rPS0(_inst.FA) * rPS0(_inst.FC)) + rPS0(_inst.FB)));
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD)); 
}


void fnmsubx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = -((rPS0(_inst.FA) * rPS0(_inst.FC)) - rPS0(_inst.FB));
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}
void fnmsubsx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = rPS1(_inst.FD) = 
		static_cast<float>(-((rPS0(_inst.FA) * rPS0(_inst.FC)) - rPS0(_inst.FB)));
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD)); 
}


void fsubx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = rPS0(_inst.FA) - rPS0(_inst.FB);
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}
void fsubsx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = rPS1(_inst.FD) = static_cast<float>(rPS0(_inst.FA) - rPS0(_inst.FB));
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}

void frsqrtex(UGeckoInstruction _inst)
{
	double b = rPS0(_inst.FB);
	if (b <= 0.0)
		rPS0(_inst.FD) = 0.0;
	else
		rPS0(_inst.FD) = 1.0f / (sqrt(b));
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}

void fsqrtx(UGeckoInstruction _inst)
{
	double b = rPS0(_inst.FB);
	if (b < 0.0)
	{
		FPSCR.VXSQRT = 1;
	}
	rPS0(_inst.FD) = sqrt(b);
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}

}  // namespace
