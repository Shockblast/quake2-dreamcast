/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
/* port from quake by BERO */


#include "../client/client.h"
#include "../linux/rw_linux.h"

#define	Key_Event(key,down)	Do_Key_Event(key,down)

#include <dc/maple/keyboard.h>
#include <dc/maple/controller.h>
#include <dc/maple/mouse.h>

cvar_t	*in_mouse;
cvar_t	*in_joystick;

// mouse variables
cvar_t	*m_filter;
qboolean	mlooking;

void IN_MLookDown (void) { mlooking = true; }
void IN_MLookUp (void) {
mlooking = false;
if (!freelook->value && lookspring->value)
		IN_CenterView ();
}

// none of these cvars are saved over a session
// this means that advanced controller configuration needs to be executed
// each time.  this avoids any problems with getting back to a default usage
// or when changing from one controller to another.  this way at least something
// works.
cvar_t	*joy_name;
cvar_t	*joy_advanced;
cvar_t	*joy_advaxisx;
cvar_t	*joy_advaxisy;
cvar_t	*joy_advaxisz;
cvar_t	*joy_advaxisr;
cvar_t	*joy_advaxisu;
cvar_t	*joy_advaxisv;
cvar_t	*joy_forwardthreshold;
cvar_t	*joy_sidethreshold;
cvar_t	*joy_pitchthreshold;
cvar_t	*joy_yawthreshold;
cvar_t	*joy_forwardsensitivity;
cvar_t	*joy_sidesensitivity;
cvar_t	*joy_pitchsensitivity;
cvar_t	*joy_yawsensitivity;
cvar_t	*joy_upthreshold;
cvar_t	*joy_upsensitivity;

cvar_t	*v_centermove;
cvar_t	*v_centerspeed;


void IN_Init (void)
{
	// mouse variables
	m_filter				= Cvar_Get ("m_filter",					"0",		0);
	in_mouse				= Cvar_Get ("in_mouse",					"1",		CVAR_ARCHIVE);

	// joystick variables
	in_joystick				= Cvar_Get ("in_joystick",				"1",		CVAR_ARCHIVE);
	joy_name				= Cvar_Get ("joy_name",					"joystick",	0);
	joy_advanced			= Cvar_Get ("joy_advanced",				"0",		0);
	joy_advaxisx			= Cvar_Get ("joy_advaxisx",				"0",		0);
	joy_advaxisy			= Cvar_Get ("joy_advaxisy",				"0",		0);
	joy_advaxisz			= Cvar_Get ("joy_advaxisz",				"0",		0);
	joy_advaxisr			= Cvar_Get ("joy_advaxisr",				"0",		0);
	joy_advaxisu			= Cvar_Get ("joy_advaxisu",				"0",		0);
	joy_advaxisv			= Cvar_Get ("joy_advaxisv",				"0",		0);
	joy_forwardthreshold	= Cvar_Get ("joy_forwardthreshold",		"0.15",		0);
	joy_sidethreshold		= Cvar_Get ("joy_sidethreshold",		"0.15",		0);
	joy_upthreshold  		= Cvar_Get ("joy_upthreshold",			"0.15",		0);
	joy_pitchthreshold		= Cvar_Get ("joy_pitchthreshold",		"0.15",		0);
	joy_yawthreshold		= Cvar_Get ("joy_yawthreshold",			"0.15",		0);
	joy_forwardsensitivity	= Cvar_Get ("joy_forwardsensitivity",	"-1",		0);
	joy_sidesensitivity		= Cvar_Get ("joy_sidesensitivity",		"-1",		0);
	joy_upsensitivity		= Cvar_Get ("joy_upsensitivity",		"-1",		0);
	joy_pitchsensitivity	= Cvar_Get ("joy_pitchsensitivity",		"1",		0);
	joy_yawsensitivity		= Cvar_Get ("joy_yawsensitivity",		"-1",		0);

	// centering
	v_centermove			= Cvar_Get ("v_centermove",				"0.15",		0);
	v_centerspeed			= Cvar_Get ("v_centerspeed",			"500",		0);

	Cmd_AddCommand ("+mlook", IN_MLookDown);
	Cmd_AddCommand ("-mlook", IN_MLookUp);

//	Cmd_AddCommand ("joy_advancedupdate", Joy_AdvancedUpdate_f);
}

void IN_Shutdown (void)
{
}

static void convert_event(int buttons,int prev_buttons,const unsigned char *cvtbl,int n)
{
	int changed = buttons^prev_buttons;
	int i;

	if (!changed) return;

	for(i=0;i<n;i++) {
		if (cvtbl[i] && changed&(1<<i)) {
			Key_Event(cvtbl[i],(buttons>>i)&1);
		}
	}
}

static void IN_Joystick(void)
{
	const static unsigned	char	cvtbl[16] = {
#if 1
	/* same code of keyboard */
	0, /* C */
	K_SPACE, /* B */
	K_ENTER, /* A */
	K_ESCAPE, /* START */
	K_UPARROW, /* UP */
	K_DOWNARROW, /* DOWN */
	K_LEFTARROW, /* LEFT */
	K_RIGHTARROW, /* RIGHT */
	0, /* Z */
	'y', /* Y */
	K_JOY3, /* X */
	0, /* D */
	0, /* UP2 */
	0, /* DOWN2 */
	0, /* LEFT2 */
	0, /* RIGHT2 */
#else
	0, /* C */
	K_JOY2, /* B */
	K_JOY1, /* A */
	0, /* START */
	K_AUX29, /* UP */
	K_AUX31, /* DOWN */
	K_AUX32, /* LEFT */
	K_AUX30, /* RIGHT */
	0, /* Z */
	K_JOY4, /* Y */
	K_JOY3, /* X */
	0, /* D */
	0, /* UP2 */
	0, /* DOWN2 */
	0, /* LEFT2 */
	0, /* RIGHT2 */
#endif
	};

	cont_cond_t	cond;
	static int prev_buttons;
	int buttons;

	if (cont_get_cond(maple_first_controller(), &cond)<0) return;
	
	buttons = cond.buttons^0xffff;
	convert_event(buttons,prev_buttons,cvtbl,sizeof(cvtbl));
	prev_buttons = buttons;

}

static void IN_Mouse(void)
{
	const static unsigned char cvtbl[] = {
	K_MOUSE1,	/* rbutton */
	K_MOUSE2,	/* lbutton */
	K_MOUSE3,	/* sidebutton */
	};

	mouse_cond_t	cond;

	static int prev_buttons;
	int buttons;

	if (mouse_get_cond(maple_first_mouse(), &cond)<0) return;

	buttons = cond.buttons^0xffff;
	convert_event(buttons,prev_buttons,cvtbl,sizeof(cvtbl));
	prev_buttons = buttons;
}

static void analog_move(usercmd_t *cmd,int mx,int my)
{
	float	mouse_x,mouse_y;

	mouse_x = (float)mx * sensitivity->value;
	mouse_y = (float)my * sensitivity->value;

	if ( (in_strafe.state & 1) || (lookstrafe->value && mlooking ))
		cmd->sidemove += m_side->value * mouse_x;
	else
		cl.viewangles[YAW] -= m_yaw->value * mouse_x;

	if ( (mlooking || freelook->value) && !(in_strafe.state & 1))
	{
		cl.viewangles[PITCH] += m_pitch->value * mouse_y;
	}
	else
	{
		cmd->forwardmove -= m_forward->value * mouse_y;
	}
}

static void IN_MouseMove(usercmd_t *cmd)
{
	mouse_cond_t	cond;
	int mouse_x,mouse_y;

	if (mouse_get_cond(maple_first_mouse(), &cond)<0) return;

	analog_move(cmd, cond.dx,cond.dy);
}

#define JOY_ABSOLUTE_AXIS	0x00000000		// control like a joystick
#define JOY_RELATIVE_AXIS	0x00000010		// control like a mouse, spinner, tr
#define	JOY_MAX_AXES	6
enum _ControlList
{
	AxisNada = 0, AxisForward, AxisLook, AxisSide, AxisTurn, AxisUp
};
const static int	dwAxisMap[JOY_MAX_AXES] = {
	AxisNada,	/* rtrig */
	AxisNada,	/* ltrig */
	AxisTurn,	/* joyx */
	AxisForward,	/* joyy */
	AxisNada,	/* joy2x */
	AxisNada,	/* joy2y */
};
const static int	dwControlMap[JOY_MAX_AXES] = {
	JOY_ABSOLUTE_AXIS,
	JOY_ABSOLUTE_AXIS,
	JOY_ABSOLUTE_AXIS,
	JOY_ABSOLUTE_AXIS,
	JOY_ABSOLUTE_AXIS,
	JOY_ABSOLUTE_AXIS,
};

static void IN_JoyMove(usercmd_t *cmd)
{
	cont_cond_t	cond;
	float	speed, aspeed;
	float	fAxisValue, fTemp;
	int		i;

	if (cont_get_cond(maple_first_controller(), &cond)<0) return;
	
	if ( (in_speed.state & 1) ^ (int)cl_run->value)
		speed = 2;
	else
		speed = 1;
	aspeed = speed * cls.frametime;

	// loop through the axes
	for (i = 0; i < JOY_MAX_AXES; i++)
	{
		// get the floating point zero-centered, potentially-inverted data for the current axis
		fAxisValue = (float) ((&cond.rtrig)[i]-128)/32768;
		// move centerpoint to zero
		// convert range from -32768..32767 to -1..1 

		switch (dwAxisMap[i])
		{
		case AxisForward:
			if ((joy_advanced->value == 0.0) && mlooking)
			{
				// user wants forward control to become look control
				if (fabs(fAxisValue) > joy_pitchthreshold->value)
				{		
					// if mouse invert is on, invert the joystick pitch value
					// only absolute control support here (joy_advanced is false)
					if (m_pitch->value < 0.0)
					{
						cl.viewangles[PITCH] -= (fAxisValue * joy_pitchsensitivity->value) * aspeed * cl_pitchspeed->value;
					}
					else
					{
						cl.viewangles[PITCH] += (fAxisValue * joy_pitchsensitivity->value) * aspeed * cl_pitchspeed->value;
					}
				}
			}
			else
			{
				// user wants forward control to be forward control
				if (fabs(fAxisValue) > joy_forwardthreshold->value)
				{
					cmd->forwardmove += (fAxisValue * joy_forwardsensitivity->value) * speed * cl_forwardspeed->value;
				}
			}
			break;

		case AxisSide:
			if (fabs(fAxisValue) > joy_sidethreshold->value)
			{
				cmd->sidemove += (fAxisValue * joy_sidesensitivity->value) * speed * cl_sidespeed->value;
			}
			break;

		case AxisUp:
			if (fabs(fAxisValue) > joy_upthreshold->value)
			{
				cmd->upmove += (fAxisValue * joy_upsensitivity->value) * speed * cl_upspeed->value;
			}
			break;

		case AxisTurn:
			if ((in_strafe.state & 1) || (lookstrafe->value && mlooking))
			{
				// user wants turn control to become side control
				if (fabs(fAxisValue) > joy_sidethreshold->value)
				{
					cmd->sidemove -= (fAxisValue * joy_sidesensitivity->value) * speed * cl_sidespeed->value;
				}
			}
			else
			{
				// user wants turn control to be turn control
				if (fabs(fAxisValue) > joy_yawthreshold->value)
				{
					if(dwControlMap[i] == JOY_ABSOLUTE_AXIS)
					{
						cl.viewangles[YAW] += (fAxisValue * joy_yawsensitivity->value) * aspeed * cl_yawspeed->value;
					}
					else
					{
						cl.viewangles[YAW] += (fAxisValue * joy_yawsensitivity->value) * speed * 180.0;
					}

				}
			}
			break;

		case AxisLook:
			if (mlooking)
			{
				if (fabs(fAxisValue) > joy_pitchthreshold->value)
				{
					// pitch movement detected and pitch movement desired by user
					if(dwControlMap[i] == JOY_ABSOLUTE_AXIS)
					{
						cl.viewangles[PITCH] += (fAxisValue * joy_pitchsensitivity->value) * aspeed * cl_pitchspeed->value;
					}
					else
					{
						cl.viewangles[PITCH] += (fAxisValue * joy_pitchsensitivity->value) * speed * 180.0;
					}
				}
			}
			break;

		default:
			break;
		}
	}
}


void IN_Commands (void)
{
	IN_Joystick();
	IN_Mouse();
}

/*
==================
IN_Frame

Called every frame, even if not generating commands
==================
*/
void IN_Frame (void)
{
}

void IN_Move (usercmd_t *cmd)
{
	IN_JoyMove(cmd);
	IN_MouseMove(cmd);
}

const static unsigned char q_key[]= {
	/*0*/	0, 0, 0, 0, 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i',
		'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't',
		'u', 'v', 'w', 'x', 'y', 'z',
	/*1e*/	'1', '2', '3', '4', '5', '6', '7', '8', '9', '0',
	/*28*/	K_ENTER, K_ESCAPE, K_BACKSPACE, K_TAB, K_SPACE, '-', '=', '[', ']', '\\', 0, ';', '\'',
	/*35*/	'`', ',', '.', '/', 0, K_F1, K_F2, K_F3, K_F4, K_F5, K_F6, K_F7, K_F8, K_F9, K_F10, K_F11, K_F12,
	/*46*/	0, 0, K_PAUSE, K_INS, K_HOME, K_PGUP, K_DEL, K_END, K_PGDN, K_RIGHTARROW, K_LEFTARROW, K_DOWNARROW, K_UPARROW,
	/*53*/	0, '/', '*', '-', '+', 13, '1', '2', '3', '4', '5', '6',
	/*5f*/	'7', '8', '9', '0', '.', 0
};

const static unsigned char q_shift[] = {
	K_CTRL,K_SHIFT,K_ALT
};

void KBD_Init(Key_Event_fp_t fp)
{
}

void KBD_Close(void)
{
}

void KBD_Update (void)
{
	extern	kbd_state_t	*kbd_state;
	static kbd_state_t	old_state,*state;
	int shiftkeys;

	int i;

	state = kbd_state;
	if (!state) return;

	shiftkeys = state->shift_keys ^ old_state.shift_keys;
	for(i=0;i<3;i++) {
		if ((shiftkeys>>i)&1) {
			Key_Event(q_shift[i],(state->shift_keys>>i)&1);
		}
	}

	for(i=0;i<sizeof(q_key);i++) {
		if (state->matrix[i]!=old_state.matrix[i]) {
			int key = q_key[i];
			if (key) Key_Event(key,state->matrix[i]);
		}
	}

	old_state = *state;
}

