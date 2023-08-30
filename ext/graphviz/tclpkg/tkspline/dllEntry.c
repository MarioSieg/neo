/*************************************************************************
 * Copyright (c) 2011 AT&T Intellectual Property 
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * which accompanies this distribution, and is available at
 * http://www.eclipse.org/legal/epl-v10.html
 *
 * Contributors: Details at https://graphviz.org
 *************************************************************************/

/*
 *	dllEntry.c
 */

#include <windows.h>

/*
 * The following declarations are for the Borland and VC++
 * DLL entry points.
 */

BOOL APIENTRY DllEntryPoint(HINSTANCE hInst,
			    DWORD reason, LPVOID reserved);
BOOL APIENTRY DllMain(HINSTANCE hInst, DWORD reason, LPVOID reserved);


/*
 *----------------------------------------------------------------------
 *
 * DllEntryPoint --
 *
 *	This routine is called by Borland to invoke the
 *	initialization code for the library.
 *
 * Results:
 *	TRUE.
 *
 * Side effects:
 *	See DllMain.
 *
 *----------------------------------------------------------------------
 */

BOOL APIENTRY DllEntryPoint(HINSTANCE hInst,	/* Library instance handle. */
			    DWORD reason,	/* Reason this function is being called. */
			    LPVOID reserved)
{				/* Not used. */
    return TRUE;
}

/*
 *----------------------------------------------------------------------
 *
 * DllMain --
 *
 *	This routine is called by the VC++ C run time library init
 *	code invoke the initialization code for the library.
 *
 * Results:
 *	TRUE.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

BOOL APIENTRY DllMain(HINSTANCE hInst,	/* Library instance handle. */
		      DWORD reason,	/* Reason this function is being called. */
		      LPVOID reserved)
{				/* Not used. */
    return TRUE;
}
