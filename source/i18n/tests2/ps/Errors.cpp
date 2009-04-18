/* Copyright (C) 2009 Wildfire Games.
 * This file is part of 0 A.D.
 *
 * 0 A.D. is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * 0 A.D. is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with 0 A.D.  If not, see <http://www.gnu.org/licenses/>.
 */

// Auto-generated by errorlist.pl - do not edit.

#include "precompiled.h"

#include "Errors.h"

class PSERROR_Error : public PSERROR {};
class PSERROR_I18n : public PSERROR {};

class PSERROR_I18n_Script : public PSERROR_I18n {};

class PSERROR_Error_InvalidError : public PSERROR_Error { public: PSRETURN getCode() const; };
class PSERROR_I18n_Script_SetupFailed : public PSERROR_I18n_Script { public: PSRETURN getCode() const; };

extern const PSRETURN PSRETURN_Error_InvalidError = 0x01000001;
extern const PSRETURN PSRETURN_I18n_Script_SetupFailed = 0x02010001;

extern const PSRETURN MASK__PSRETURN_Error = 0xff000000;
extern const PSRETURN CODE__PSRETURN_Error = 0x01000000;
extern const PSRETURN MASK__PSRETURN_I18n = 0xff000000;
extern const PSRETURN CODE__PSRETURN_I18n = 0x02000000;
extern const PSRETURN MASK__PSRETURN_I18n_Script = 0xffff0000;
extern const PSRETURN CODE__PSRETURN_I18n_Script = 0x02010000;

extern const PSRETURN MASK__PSRETURN_Error_InvalidError = 0xffffffff;
extern const PSRETURN CODE__PSRETURN_Error_InvalidError = 0x01000001;
extern const PSRETURN MASK__PSRETURN_I18n_Script_SetupFailed = 0xffffffff;
extern const PSRETURN CODE__PSRETURN_I18n_Script_SetupFailed = 0x02010001;

PSRETURN PSERROR_Error_InvalidError::getCode() const { return 0x01000001; }
PSRETURN PSERROR_I18n_Script_SetupFailed::getCode() const { return 0x02010001; }

const char* PSERROR::what() const throw ()
{
	return GetErrorString(getCode());
}

const char* GetErrorString(const PSERROR& err)
{
	return GetErrorString(err.getCode());
}

const char* GetErrorString(PSRETURN code)
{
	switch (code)
	{
	case 0x01000001: return "Error_InvalidError";
	case 0x02010001: return "I18n_Script_SetupFailed";

	default: return "Unrecognised error";
	}
}

void ThrowError(PSRETURN code)
{
	switch (code)  // Use 'break' in case someone tries to continue from the exception
	{
	case 0x01000001: throw PSERROR_Error_InvalidError(); break;
	case 0x02010001: throw PSERROR_I18n_Script_SetupFailed(); break;

	default: throw PSERROR_Error_InvalidError(); // Hmm...
	}
}
