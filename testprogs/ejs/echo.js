#!/usr/bin/env smbscript
/*
	test echo pipe calls from ejs
*/	

var options = new Object();

ok = GetOptions(ARGV, options,
		"POPT_AUTOHELP",
		"POPT_COMMON_SAMBA",
		"POPT_COMMON_CREDENTIALS");
if (ok == false) {
   println("Failed to parse options: " + options.ERROR);
   return -1;
}

libinclude("base.js");

echo = rpcecho_init();

/*
  generate a ramp as an integer array
 */
function ramp_array(N)
{
	var a = new Array(N);
	for (i=0;i<N;i++) {
		a[i] = i;
	}
	return a;
}


/*
  test the echo_AddOne interface
*/
function test_AddOne(conn)
{
	var io = irpcObj();

	print("Testing echo_AddOne\n");

	for (i=0;i<10;i++) {
		io.input.in_data = i;
		status = echo.echo_AddOne(conn, io);
		check_status_ok(status);
		assert(io.output.out_data == i + 1);
	}
}

/*
  test the echo_EchoData interface
*/
function test_EchoData(conn)
{
	var io = irpcObj();

	print("Testing echo_EchoData\n");

	for (i=0; i<30; i=i+5) {
		io.input.len = i;
		io.input.in_data = ramp_array(i);
		status = echo.echo_EchoData(conn, io);
		check_status_ok(status);
		check_array_equal(io.input.in_data, io.output.out_data);
	}
}


/*
  test the echo_SinkData interface
*/
function test_SinkData(conn)
{
	var io = irpcObj();

	print("Testing echo_SinkData\n");

	for (i=0; i<30; i=i+5) {
		io.input.len = i;
		io.input.data = ramp_array(i);
		status = echo.echo_SinkData(conn, io);
		check_status_ok(status);
	}
}


/*
  test the echo_SourceData interface
*/
function test_SourceData(conn)
{
	var io = irpcObj();

	print("Testing echo_SourceData\n");

	for (i=0; i<30; i=i+5) {
		io.input.len = i;
		status = echo.echo_SourceData(conn, io);
		check_status_ok(status);
		correct = ramp_array(i);
		check_array_equal(correct, io.output.data);
	}
}


/*
  test the echo_TestCall interface
*/
function test_TestCall(conn)
{
	var io = irpcObj();

	print("Testing echo_TestCall\n");

	io.input.s1 = "my test string";
	status = echo.echo_TestCall(conn, io);
	check_status_ok(status);
	assert("this is a test string" == io.output.s2);
}

/*
  test the echo_TestCall2 interface
*/
function test_TestCall2(conn)
{
	var io = irpcObj();

	print("Testing echo_TestCall2\n");

	for (i=1;i<=7;i++) {
		io.input.level = i;
		status = echo.echo_TestCall2(conn, io);
		check_status_ok(status);
	}
}

/*
  test the echo_TestSleep interface
*/
function test_TestSleep(conn)
{
	var io = irpcObj();

	print("Testing echo_TestSleep\n");

	io.input.seconds = 1;
	status = echo.echo_TestSleep(conn, io);
	check_status_ok(status);
}

/*
  test the echo_TestEnum interface
*/
function test_TestEnum(conn)
{
	var io = irpcObj();

	print("Testing echo_TestEnum\n");

	io.input.foo1 = echo.ECHO_ENUM1;
	io.input.foo2 = new Object();
	io.input.foo2.e1 = echo.ECHO_ENUM1;
	io.input.foo2.e2 = echo.ECHO_ENUM1_32;
	io.input.foo3 = new Object();
	io.input.foo3.e1 = echo.ECHO_ENUM2;
	status = echo.echo_TestEnum(conn, io);
	check_status_ok(status);
	assert(io.output.foo1    == echo.ECHO_ENUM1);
	assert(io.output.foo2.e1 == echo.ECHO_ENUM2);
	assert(io.output.foo2.e2 == echo.ECHO_ENUM1_32);
	assert(io.output.foo3.e1 == echo.ECHO_ENUM2);
}

/*
  test the echo_TestSurrounding interface
*/
function test_TestSurrounding(conn)
{
	var io = irpcObj();

	print("Testing echo_TestSurrounding\n");
	
	io.input.data = new Object();
	io.input.data.x = 10;
	io.input.data.surrounding = ramp_array(10);
	status = echo.echo_TestSurrounding(conn, io);
	check_status_ok(status);
	assert(io.output.data.surrounding.length == 20);
	check_array_zero(io.output.data.surrounding);
}

/*
  test the echo_TestDoublePointer interface
*/
function test_TestDoublePointer(conn)
{
	var io = irpcObj();

	print("Testing echo_TestDoublePointer\n");
	
	io.input.data = 7;
	status = echo.echo_TestDoublePointer(conn, io);
	check_status_ok(status);
	assert(io.input.data == io.input.data);
}


if (ARGV.length == 0) {
   print("Usage: echo.js <RPCBINDING>\n");
   exit(0);
}

if (options.ARGV.length != 1) {
   println("Usage: samr.js <BINDING>");
   return -1;
}
var binding = options.ARGV[0];
var conn = new Object();

print("Connecting to " + binding + "\n");
status = rpc_connect(conn, binding, "rpcecho");
if (status.is_ok != true) {
   print("Failed to connect to " + binding + " - " + status.errstr + "\n");
   return;
}

test_AddOne(conn);
test_EchoData(conn);
test_SinkData(conn);
test_SourceData(conn);
test_TestCall(conn);
test_TestCall2(conn);
test_TestSleep(conn);
test_TestEnum(conn);
test_TestSurrounding(conn);
test_TestDoublePointer(conn);

print("All OK\n");
return 0;
