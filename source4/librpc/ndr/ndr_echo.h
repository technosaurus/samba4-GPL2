/* header auto-generated by pidl */

struct echo_AddOne {
	struct {
		uint32 *v;
	} in;

	struct {
		uint32 *v;
	} out;

};

struct echo_EchoData {
	struct {
		uint32 len;
		uint8 *in_data;
	} in;

	struct {
		uint8 *out_data;
	} out;

};

struct echo_SinkData {
	struct {
		uint32 len;
		uint8 *data;
	} in;

	struct {
	} out;

};

struct echo_SourceData {
	struct {
		uint32 len;
	} in;

	struct {
		uint8 *data;
	} out;

};

struct Struct1 {
	uint32 bar;
	uint32 count;
	uint32 foo;
	uint32 *s;
};

struct TestCall {
	struct {
	} in;

	struct {
		struct Struct1 **s1;
	} out;

};

#define DCERPC_ECHO_ADDONE 0
#define DCERPC_ECHO_ECHODATA 1
#define DCERPC_ECHO_SINKDATA 2
#define DCERPC_ECHO_SOURCEDATA 3
#define DCERPC_TESTCALL 4
