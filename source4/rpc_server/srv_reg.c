/*
 *  Unix SMB/CIFS implementation.
 *  RPC Pipe client / server routines
 *  Copyright (C) Andrew Tridgell              1992-1997,
 *  Copyright (C) Luke Kenneth Casson Leighton 1996-1997,
 *  Copyright (C) Paul Ashton                       1997,
 *  Copyright (C) Marc Jacobsen	   		    2000,
 *  Copyright (C) Jeremy Allison		    2001,
 *  Copyright (C) Gerald Carter 		    2002,
 *  Copyright (C) Anthony Liguori                   2003.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* This is the interface for the registry functions. */

#include "includes.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_RPC_SRV

/*******************************************************************
 api_reg_close
 ********************************************************************/

static BOOL api_reg_close(pipes_struct *p)
{
	REG_Q_CLOSE q_u;
	REG_R_CLOSE r_u;
	prs_struct *data = &p->in_data.data;
	prs_struct *rdata = &p->out_data.rdata;

	ZERO_STRUCT(q_u);
	ZERO_STRUCT(r_u);

	/* grab the reg unknown 1 */
	if(!reg_io_q_close("", &q_u, data, 0))
		return False;

	r_u.status = _reg_close(p, &q_u, &r_u);

	if(!reg_io_r_close("", &r_u, rdata, 0))
		return False;

	return True;
}

/*******************************************************************
 api_reg_open_khlm
 ********************************************************************/

static BOOL api_reg_open_hklm(pipes_struct *p)
{
	REG_Q_OPEN_HKLM q_u;
	REG_R_OPEN_HKLM r_u;
	prs_struct *data = &p->in_data.data;
	prs_struct *rdata = &p->out_data.rdata;

	ZERO_STRUCT(q_u);
	ZERO_STRUCT(r_u);

	/* grab the reg open */
	if(!reg_io_q_open_hklm("", &q_u, data, 0))
		return False;

	r_u.status = _reg_open_hklm(p, &q_u, &r_u);

	if(!reg_io_r_open_hklm("", &r_u, rdata, 0))
		return False;

	return True;
}

/*******************************************************************
 api_reg_open_khu
 ********************************************************************/

static BOOL api_reg_open_hku(pipes_struct *p)
{
	REG_Q_OPEN_HKU q_u;
	REG_R_OPEN_HKU r_u;
	prs_struct *data = &p->in_data.data;
	prs_struct *rdata = &p->out_data.rdata;

	ZERO_STRUCT(q_u);
	ZERO_STRUCT(r_u);

	/* grab the reg open */
	if(!reg_io_q_open_hku("", &q_u, data, 0))
		return False;

	r_u.status = _reg_open_hku(p, &q_u, &r_u);

	if(!reg_io_r_open_hku("", &r_u, rdata, 0))
		return False;

	return True;
}

/*******************************************************************
 api_reg_open_khcr
 ********************************************************************/

static BOOL api_reg_open_hkcr(pipes_struct *p)
{
	REG_Q_OPEN_HKCR q_u;
	REG_R_OPEN_HKCR r_u;
	prs_struct *data = &p->in_data.data;
	prs_struct *rdata = &p->out_data.rdata;

	ZERO_STRUCT(q_u);
	ZERO_STRUCT(r_u);

	/* grab the reg open */
	if(!reg_io_q_open_hkcr("", &q_u, data, 0))
		return False;

	r_u.status = _reg_open_hkcr(p, &q_u, &r_u);

	if(!reg_io_r_open_hkcr("", &r_u, rdata, 0))
		return False;

	return True;
}


/*******************************************************************
 api_reg_open_entry
 ********************************************************************/

static BOOL api_reg_open_entry(pipes_struct *p)
{
	REG_Q_OPEN_ENTRY q_u;
	REG_R_OPEN_ENTRY r_u;
	prs_struct *data = &p->in_data.data;
	prs_struct *rdata = &p->out_data.rdata;

	ZERO_STRUCT(q_u);
	ZERO_STRUCT(r_u);

	/* grab the reg open entry */
	if(!reg_io_q_open_entry("", &q_u, data, 0))
		return False;

	/* construct reply. */
	r_u.status = _reg_open_entry(p, &q_u, &r_u);

	if(!reg_io_r_open_entry("", &r_u, rdata, 0))
		return False;

	return True;
}

/*******************************************************************
 api_reg_info
 ********************************************************************/

static BOOL api_reg_info(pipes_struct *p)
{
	REG_Q_INFO q_u;
	REG_R_INFO r_u;
	prs_struct *data = &p->in_data.data;
	prs_struct *rdata = &p->out_data.rdata;

	ZERO_STRUCT(q_u);
	ZERO_STRUCT(r_u);

	/* grab the reg unknown 0x11*/
	if(!reg_io_q_info("", &q_u, data, 0))
		return False;

	r_u.status = _reg_info(p, &q_u, &r_u);

	if(!reg_io_r_info("", &r_u, rdata, 0))
		return False;

	return True;
}

/*******************************************************************
 api_reg_shutdown
 ********************************************************************/

static BOOL api_reg_shutdown(pipes_struct *p)
{
	REG_Q_SHUTDOWN q_u;
	REG_R_SHUTDOWN r_u;
	prs_struct *data = &p->in_data.data;
	prs_struct *rdata = &p->out_data.rdata;

	ZERO_STRUCT(q_u);
	ZERO_STRUCT(r_u);

	/* grab the reg shutdown */
	if(!reg_io_q_shutdown("", &q_u, data, 0))
		return False;

	r_u.status = _reg_shutdown(p, &q_u, &r_u);

	if(!reg_io_r_shutdown("", &r_u, rdata, 0))
		return False;

	return True;
}

/*******************************************************************
 api_reg_abort_shutdown
 ********************************************************************/

static BOOL api_reg_abort_shutdown(pipes_struct *p)
{
	REG_Q_ABORT_SHUTDOWN q_u;
	REG_R_ABORT_SHUTDOWN r_u;
	prs_struct *data = &p->in_data.data;
	prs_struct *rdata = &p->out_data.rdata;

	ZERO_STRUCT(q_u);
	ZERO_STRUCT(r_u);

	/* grab the reg shutdown */
	if(!reg_io_q_abort_shutdown("", &q_u, data, 0))
		return False;

	r_u.status = _reg_abort_shutdown(p, &q_u, &r_u);

	if(!reg_io_r_abort_shutdown("", &r_u, rdata, 0))
		return False;

	return True;
}


/*******************************************************************
 api_reg_query_key
 ********************************************************************/

static BOOL api_reg_query_key(pipes_struct *p)
{
	REG_Q_QUERY_KEY q_u;
	REG_R_QUERY_KEY r_u;
	prs_struct *data = &p->in_data.data;
	prs_struct *rdata = &p->out_data.rdata;

	ZERO_STRUCT(q_u);
	ZERO_STRUCT(r_u);

	if(!reg_io_q_query_key("", &q_u, data, 0))
		return False;

	r_u.status = _reg_query_key(p, &q_u, &r_u);

	if(!reg_io_r_query_key("", &r_u, rdata, 0))
		return False;

	return True;
}

/*******************************************************************
 api_reg_unknown_1a
 ********************************************************************/

static BOOL api_reg_unknown_1a(pipes_struct *p)
{
	REG_Q_UNKNOWN_1A q_u;
	REG_R_UNKNOWN_1A r_u;
	prs_struct *data = &p->in_data.data;
	prs_struct *rdata = &p->out_data.rdata;

	ZERO_STRUCT(q_u);
	ZERO_STRUCT(r_u);

	if(!reg_io_q_unknown_1a("", &q_u, data, 0))
		return False;

	r_u.status = _reg_unknown_1a(p, &q_u, &r_u);

	if(!reg_io_r_unknown_1a("", &r_u, rdata, 0))
		return False;

	return True;
}

/*******************************************************************
 api_reg_enum_key
 ********************************************************************/

static BOOL api_reg_enum_key(pipes_struct *p)
{
	REG_Q_ENUM_KEY q_u;
	REG_R_ENUM_KEY r_u;
	prs_struct *data = &p->in_data.data;
	prs_struct *rdata = &p->out_data.rdata;

	ZERO_STRUCT(q_u);
	ZERO_STRUCT(r_u);

	if(!reg_io_q_enum_key("", &q_u, data, 0))
		return False;

	r_u.status = _reg_enum_key(p, &q_u, &r_u);

	if(!reg_io_r_enum_key("", &r_u, rdata, 0))
		return False;

	return True;
}

/*******************************************************************
 api_reg_enum_value
 ********************************************************************/

static BOOL api_reg_enum_value(pipes_struct *p)
{
	REG_Q_ENUM_VALUE q_u;
	REG_R_ENUM_VALUE r_u;
	prs_struct *data = &p->in_data.data;
	prs_struct *rdata = &p->out_data.rdata;

	ZERO_STRUCT(q_u);
	ZERO_STRUCT(r_u);

	if(!reg_io_q_enum_val("", &q_u, data, 0))
		return False;
		
	r_u.status = _reg_enum_value(p, &q_u, &r_u);

	if(!reg_io_r_enum_val("", &r_u, rdata, 0))
		return False;

	return True;
}

/*******************************************************************
 api_reg_save_key
 ********************************************************************/

static BOOL api_reg_save_key(pipes_struct *p)
{
	REG_Q_SAVE_KEY q_u;
	REG_R_SAVE_KEY r_u;
	prs_struct *data = &p->in_data.data;
	prs_struct *rdata = &p->out_data.rdata;

	ZERO_STRUCT(q_u);
	ZERO_STRUCT(r_u);

	if(!reg_io_q_save_key("", &q_u, data, 0))
		return False;
		
	r_u.status = _reg_save_key(p, &q_u, &r_u);

	if(!reg_io_r_save_key("", &r_u, rdata, 0))
		return False;

	return True;
}



/*******************************************************************
 array of \PIPE\reg operations
 ********************************************************************/

#ifdef RPC_REG_DYNAMIC
int init_module(void)
#else
int rpc_reg_init(void)
#endif
{
  static struct api_struct api_reg_cmds[] =
    {
      { "REG_CLOSE"              , REG_CLOSE              , api_reg_close            },
      { "REG_OPEN_ENTRY"         , REG_OPEN_ENTRY         , api_reg_open_entry       },
      { "REG_OPEN_HKCR"          , REG_OPEN_HKCR          , api_reg_open_hkcr        },
      { "REG_OPEN_HKLM"          , REG_OPEN_HKLM          , api_reg_open_hklm        },
      { "REG_OPEN_HKU"           , REG_OPEN_HKU           , api_reg_open_hku         },
      { "REG_ENUM_KEY"           , REG_ENUM_KEY           , api_reg_enum_key         },
      { "REG_ENUM_VALUE"         , REG_ENUM_VALUE         , api_reg_enum_value       },
      { "REG_QUERY_KEY"          , REG_QUERY_KEY          , api_reg_query_key        },
      { "REG_INFO"               , REG_INFO               , api_reg_info             },
      { "REG_SHUTDOWN"           , REG_SHUTDOWN           , api_reg_shutdown         },
      { "REG_ABORT_SHUTDOWN"     , REG_ABORT_SHUTDOWN     , api_reg_abort_shutdown   },
      { "REG_UNKNOWN_1A"         , REG_UNKNOWN_1A         , api_reg_unknown_1a       },
      { "REG_SAVE_KEY"           , REG_SAVE_KEY           , api_reg_save_key         }
    };
  return rpc_pipe_register_commands("winreg", "winreg", api_reg_cmds,
				    sizeof(api_reg_cmds) / sizeof(struct api_struct));
}
