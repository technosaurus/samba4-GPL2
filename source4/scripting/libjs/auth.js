/*
	auth js library functions
	Copyright Andrew Tridgell 2005
	released under the GNU GPL v2 or later
*/


/*
  get a list of domains for SWAT authentication
*/
function getDomainList()
{
	var ret = new Array(2);
	ret[0] = "System User";
	ret[1] = lpGet("workgroup");
	return ret;
}
