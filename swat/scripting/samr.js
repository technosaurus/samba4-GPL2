/*
	samr rpc utility functions 
*/	

/*
  helper function to setup a rpc io object, ready for input
*/
function irpcObj()
{
	var o = new Object();
	o.input = new Object();
	return o;
}

/*
  check that a status result is OK
*/
function check_status_ok(status)
{
	if (status.is_ok != true) {
		printVars(status);
	}
	assert(status.is_ok == true);
}

/*
  return a list of names and indexes from a samArray
*/
function samArray(output)
{
	var list = new Array(output.num_entries);
	if (output.sam == NULL) {
		return list;
	}
	var entries = output.sam.entries;
	for (i=0;i<output.num_entries;i++) {
		list[i] = new Object();
                list[i].name = entries[i].name;
                list[i].idx  = entries[i].idx;
	}
	return list;
}

/*
	connect to the sam database
*/
function samrConnect(conn)
{
	var io = irpcObj();
	io.input.system_name = NULL;
	io.input.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	status = dcerpc_samr_Connect(conn, io);
	check_status_ok(status);
	return io.output.connect_handle;
}

/*
	close a handle
*/
function samrClose(conn, handle)
{
	var io = irpcObj();
	io.input.handle = handle;
	status = dcerpc_samr_Close(conn, io);
	check_status_ok(status);
}

/*
   get the sid for a domain
*/
function samrLookupDomain(conn, handle, domain)
{
	var io = irpcObj();
	io.input.connect_handle = handle;
	io.input.domain_name = domain;
	status = dcerpc_samr_LookupDomain(conn, io);
	check_status_ok(status);
	return io.output.sid;
}

/*
  open a domain by sid
*/
function samrOpenDomain(conn, handle, sid)
{
	var io = irpcObj();
	io.input.connect_handle = handle;
	io.input.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	io.input.sid = sid;
	status = dcerpc_samr_OpenDomain(conn, io);
	check_status_ok(status);
	return io.output.domain_handle;
}

/*
  open a user by rid
*/
function samrOpenUser(conn, handle, rid)
{
	var io = irpcObj();
	io.input.domain_handle = handle;
	io.input.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	io.input.rid = rid;
	status = dcerpc_samr_OpenUser(conn, io);
	check_status_ok(status);
	return io.output.user_handle;
}

/*
  return a list of all users
*/
function samrEnumDomainUsers(conn, dom_handle)
{
	var io = irpcObj();
	io.input.domain_handle = dom_handle;
	io.input.resume_handle = 0;
	io.input.acct_flags = 0;
	io.input.max_size = -1;
	status = dcerpc_samr_EnumDomainUsers(conn, io);
	check_status_ok(status);
	return samArray(io.output);
}

/*
  return a list of domains
*/
function samrEnumDomains(conn, handle)
{
	var io = irpcObj();
	io.input.connect_handle = handle;
	io.input.resume_handle = 0;
	io.input.buf_size = -1;
	status = dcerpc_samr_EnumDomains(conn, io);
	check_status_ok(status);
	return samArray(io.output);
}

/*
  return information about a user
*/
function samrQueryUserInfo(conn, user_handle, level)
{
	var r, io = irpcObj();
	io.input.user_handle = user_handle;
	io.input.level = level;
	status = dcerpc_samr_QueryUserInfo(conn, io);
	check_status_ok(status);
	return io.output.info.info3;
}


/*
  fill a user array with user information from samrQueryUserInfo
*/
function samrFillUserInfo(conn, dom_handle, users, level)
{
	var i;
	for (i=0;i<users.length;i++) {
		var r, user_handle, info;
		user_handle = samrOpenUser(conn, dom_handle, users[i].idx);
		info = samrQueryUserInfo(conn, user_handle, level);
		info.name = users[i].name;
		info.idx  = users[i].idx;
		users[i] = info;
		samrClose(conn, user_handle);
	}
}
