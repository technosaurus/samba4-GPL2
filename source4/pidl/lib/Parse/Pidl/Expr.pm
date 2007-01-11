####################################################################
#
#    This file was generated using Parse::Yapp version 1.05.
#
#        Don't edit this file, use source file instead.
#
#             ANY CHANGE MADE HERE WILL BE LOST !
#
####################################################################
package Parse::Pidl::Expr;
use vars qw ( @ISA );
use strict;

@ISA= qw ( Parse::Yapp::Driver );
use Parse::Yapp::Driver;



sub new {
        my($class)=shift;
        ref($class)
    and $class=ref($class);

    my($self)=$class->SUPER::new( yyversion => '1.05',
                                  yystates =>
[
	{#State 0
		ACTIONS => {
			"-" => 1,
			"~" => 3,
			"&" => 4,
			'NUM' => 5,
			'TEXT' => 6,
			"(" => 7,
			"!" => 8,
			"*" => 9,
			'VAR' => 10
		},
		GOTOS => {
			'exp' => 2,
			'func' => 11
		}
	},
	{#State 1
		ACTIONS => {
			"-" => 1,
			"~" => 3,
			"&" => 4,
			'TEXT' => 6,
			'NUM' => 5,
			"(" => 7,
			"!" => 8,
			"*" => 9,
			'VAR' => 10
		},
		GOTOS => {
			'exp' => 12,
			'func' => 11
		}
	},
	{#State 2
		ACTIONS => {
			'' => 14,
			"-" => 13,
			"<" => 15,
			"+" => 16,
			"%" => 17,
			"==" => 18,
			"^" => 19,
			"*" => 20,
			">>" => 21,
			"!=" => 22,
			"?" => 23,
			"&&" => 24,
			"||" => 25,
			"&" => 26,
			"/" => 27,
			"->" => 28,
			"|" => 29,
			"<<" => 31,
			"=>" => 30,
			"<=" => 33,
			"." => 32,
			">" => 34
		}
	},
	{#State 3
		ACTIONS => {
			"-" => 1,
			"~" => 3,
			"&" => 4,
			'TEXT' => 6,
			'NUM' => 5,
			"!" => 8,
			"(" => 7,
			"*" => 9,
			'VAR' => 10
		},
		GOTOS => {
			'exp' => 35,
			'func' => 11
		}
	},
	{#State 4
		ACTIONS => {
			"-" => 1,
			"~" => 3,
			"&" => 4,
			'TEXT' => 6,
			'NUM' => 5,
			"!" => 8,
			"(" => 7,
			"*" => 9,
			'VAR' => 10
		},
		GOTOS => {
			'exp' => 36,
			'func' => 11
		}
	},
	{#State 5
		DEFAULT => -1
	},
	{#State 6
		DEFAULT => -2
	},
	{#State 7
		ACTIONS => {
			"-" => 1,
			"~" => 3,
			"&" => 4,
			'TEXT' => 6,
			'NUM' => 5,
			"!" => 8,
			"(" => 7,
			"*" => 9,
			'VAR' => 10
		},
		GOTOS => {
			'exp' => 37,
			'func' => 11
		}
	},
	{#State 8
		ACTIONS => {
			"-" => 1,
			"~" => 3,
			"&" => 4,
			'TEXT' => 6,
			'NUM' => 5,
			"!" => 8,
			"(" => 7,
			"*" => 9,
			'VAR' => 10
		},
		GOTOS => {
			'exp' => 38,
			'func' => 11
		}
	},
	{#State 9
		ACTIONS => {
			"-" => 1,
			"~" => 3,
			"&" => 4,
			'TEXT' => 6,
			'NUM' => 5,
			"!" => 8,
			"(" => 7,
			"*" => 9,
			'VAR' => 10
		},
		GOTOS => {
			'exp' => 39,
			'func' => 11
		}
	},
	{#State 10
		ACTIONS => {
			"(" => 40
		},
		DEFAULT => -5
	},
	{#State 11
		DEFAULT => -3
	},
	{#State 12
		ACTIONS => {
			"^" => 19,
			"=>" => 30,
			"." => 32,
			"<=" => 33
		},
		DEFAULT => -29
	},
	{#State 13
		ACTIONS => {
			"-" => 1,
			"~" => 3,
			"&" => 4,
			'TEXT' => 6,
			'NUM' => 5,
			"!" => 8,
			"(" => 7,
			"*" => 9,
			'VAR' => 10
		},
		GOTOS => {
			'exp' => 41,
			'func' => 11
		}
	},
	{#State 14
		DEFAULT => 0
	},
	{#State 15
		ACTIONS => {
			"-" => 1,
			"~" => 3,
			"&" => 4,
			'TEXT' => 6,
			'NUM' => 5,
			"!" => 8,
			"(" => 7,
			"*" => 9,
			'VAR' => 10
		},
		GOTOS => {
			'exp' => 42,
			'func' => 11
		}
	},
	{#State 16
		ACTIONS => {
			"-" => 1,
			"~" => 3,
			"&" => 4,
			'TEXT' => 6,
			'NUM' => 5,
			"!" => 8,
			"(" => 7,
			"*" => 9,
			'VAR' => 10
		},
		GOTOS => {
			'exp' => 43,
			'func' => 11
		}
	},
	{#State 17
		ACTIONS => {
			"-" => 1,
			"~" => 3,
			"&" => 4,
			'TEXT' => 6,
			'NUM' => 5,
			"!" => 8,
			"(" => 7,
			"*" => 9,
			'VAR' => 10
		},
		GOTOS => {
			'exp' => 44,
			'func' => 11
		}
	},
	{#State 18
		ACTIONS => {
			"-" => 1,
			"~" => 3,
			"&" => 4,
			'TEXT' => 6,
			'NUM' => 5,
			"!" => 8,
			"(" => 7,
			"*" => 9,
			'VAR' => 10
		},
		GOTOS => {
			'exp' => 45,
			'func' => 11
		}
	},
	{#State 19
		ACTIONS => {
			"-" => 1,
			"~" => 3,
			"&" => 4,
			'TEXT' => 6,
			'NUM' => 5,
			"!" => 8,
			"(" => 7,
			"*" => 9,
			'VAR' => 10
		},
		GOTOS => {
			'exp' => 46,
			'func' => 11
		}
	},
	{#State 20
		ACTIONS => {
			"-" => 1,
			"~" => 3,
			"&" => 4,
			'TEXT' => 6,
			'NUM' => 5,
			"!" => 8,
			"(" => 7,
			"*" => 9,
			'VAR' => 10
		},
		GOTOS => {
			'exp' => 47,
			'func' => 11
		}
	},
	{#State 21
		ACTIONS => {
			"-" => 1,
			"~" => 3,
			"&" => 4,
			'TEXT' => 6,
			'NUM' => 5,
			"!" => 8,
			"(" => 7,
			"*" => 9,
			'VAR' => 10
		},
		GOTOS => {
			'exp' => 48,
			'func' => 11
		}
	},
	{#State 22
		ACTIONS => {
			"-" => 1,
			"~" => 3,
			"&" => 4,
			'TEXT' => 6,
			'NUM' => 5,
			"!" => 8,
			"(" => 7,
			"*" => 9,
			'VAR' => 10
		},
		GOTOS => {
			'exp' => 49,
			'func' => 11
		}
	},
	{#State 23
		ACTIONS => {
			"-" => 1,
			"~" => 3,
			"&" => 4,
			'TEXT' => 6,
			'NUM' => 5,
			"!" => 8,
			"(" => 7,
			"*" => 9,
			'VAR' => 10
		},
		GOTOS => {
			'exp' => 50,
			'func' => 11
		}
	},
	{#State 24
		ACTIONS => {
			"-" => 1,
			"~" => 3,
			"&" => 4,
			'TEXT' => 6,
			'NUM' => 5,
			"!" => 8,
			"(" => 7,
			"*" => 9,
			'VAR' => 10
		},
		GOTOS => {
			'exp' => 51,
			'func' => 11
		}
	},
	{#State 25
		ACTIONS => {
			"-" => 1,
			"~" => 3,
			"&" => 4,
			'TEXT' => 6,
			'NUM' => 5,
			"!" => 8,
			"(" => 7,
			"*" => 9,
			'VAR' => 10
		},
		GOTOS => {
			'exp' => 52,
			'func' => 11
		}
	},
	{#State 26
		ACTIONS => {
			"-" => 1,
			"~" => 3,
			"&" => 4,
			'TEXT' => 6,
			'NUM' => 5,
			"!" => 8,
			"(" => 7,
			"*" => 9,
			'VAR' => 10
		},
		GOTOS => {
			'exp' => 53,
			'func' => 11
		}
	},
	{#State 27
		ACTIONS => {
			"-" => 1,
			"~" => 3,
			"&" => 4,
			'TEXT' => 6,
			'NUM' => 5,
			"!" => 8,
			"(" => 7,
			"*" => 9,
			'VAR' => 10
		},
		GOTOS => {
			'exp' => 54,
			'func' => 11
		}
	},
	{#State 28
		ACTIONS => {
			'VAR' => 55
		}
	},
	{#State 29
		ACTIONS => {
			"-" => 1,
			"~" => 3,
			"&" => 4,
			'TEXT' => 6,
			'NUM' => 5,
			"!" => 8,
			"(" => 7,
			"*" => 9,
			'VAR' => 10
		},
		GOTOS => {
			'exp' => 56,
			'func' => 11
		}
	},
	{#State 30
		ACTIONS => {
			"-" => 1,
			"~" => 3,
			"&" => 4,
			'TEXT' => 6,
			'NUM' => 5,
			"!" => 8,
			"(" => 7,
			"*" => 9,
			'VAR' => 10
		},
		GOTOS => {
			'exp' => 57,
			'func' => 11
		}
	},
	{#State 31
		ACTIONS => {
			"-" => 1,
			"~" => 3,
			"&" => 4,
			'TEXT' => 6,
			'NUM' => 5,
			"!" => 8,
			"(" => 7,
			"*" => 9,
			'VAR' => 10
		},
		GOTOS => {
			'exp' => 58,
			'func' => 11
		}
	},
	{#State 32
		ACTIONS => {
			'VAR' => 59
		}
	},
	{#State 33
		ACTIONS => {
			"-" => 1,
			"~" => 3,
			"&" => 4,
			'TEXT' => 6,
			'NUM' => 5,
			"!" => 8,
			"(" => 7,
			"*" => 9,
			'VAR' => 10
		},
		GOTOS => {
			'exp' => 60,
			'func' => 11
		}
	},
	{#State 34
		ACTIONS => {
			"-" => 1,
			"~" => 3,
			"&" => 4,
			'TEXT' => 6,
			'NUM' => 5,
			"!" => 8,
			"(" => 7,
			"*" => 9,
			'VAR' => 10
		},
		GOTOS => {
			'exp' => 61,
			'func' => 11
		}
	},
	{#State 35
		ACTIONS => {
			"^" => 19,
			"=>" => 30,
			"." => 32,
			"<=" => 33
		},
		DEFAULT => -7
	},
	{#State 36
		ACTIONS => {
			"^" => 19,
			"=>" => 30,
			"." => 32,
			"<=" => 33
		},
		DEFAULT => -30
	},
	{#State 37
		ACTIONS => {
			"-" => 13,
			"<" => 15,
			"+" => 16,
			"%" => 17,
			"==" => 18,
			"^" => 19,
			"*" => 20,
			")" => 62,
			">>" => 21,
			"!=" => 22,
			"?" => 23,
			"&&" => 24,
			"||" => 25,
			"&" => 26,
			"/" => 27,
			"->" => 28,
			"|" => 29,
			"=>" => 30,
			"<<" => 31,
			"." => 32,
			"<=" => 33,
			">" => 34
		}
	},
	{#State 38
		ACTIONS => {
			"-" => 13,
			"<" => 15,
			"+" => 16,
			"%" => 17,
			"==" => 18,
			"^" => 19,
			"*" => 20,
			">>" => 21,
			"!=" => 22,
			"?" => 23,
			"&&" => 24,
			"||" => 25,
			"&" => 26,
			"/" => 27,
			"|" => 29,
			"=>" => 30,
			"<<" => 31,
			"." => 32,
			"<=" => 33,
			">" => 34
		},
		DEFAULT => -27
	},
	{#State 39
		ACTIONS => {
			"^" => 19,
			"=>" => 30,
			"." => 32,
			"<=" => 33
		},
		DEFAULT => -6
	},
	{#State 40
		ACTIONS => {
			"-" => 1,
			"~" => 3,
			"&" => 4,
			'TEXT' => 6,
			'NUM' => 5,
			"!" => 8,
			"(" => 7,
			"*" => 9,
			'VAR' => 10
		},
		DEFAULT => -34,
		GOTOS => {
			'exp' => 64,
			'args' => 63,
			'func' => 11,
			'opt_args' => 65
		}
	},
	{#State 41
		ACTIONS => {
			"<" => 15,
			"==" => 18,
			"^" => 19,
			">>" => 21,
			"!=" => 22,
			"?" => 23,
			"&&" => 24,
			"||" => 25,
			"&" => 26,
			"|" => 29,
			"=>" => 30,
			"<<" => 31,
			"." => 32,
			"<=" => 33,
			">" => 34
		},
		DEFAULT => -9
	},
	{#State 42
		ACTIONS => {
			"==" => 18,
			"^" => 19,
			"!=" => 22,
			"?" => 23,
			"&&" => 24,
			"||" => 25,
			"&" => 26,
			"|" => 29,
			"=>" => 30,
			"." => 32,
			"<=" => 33
		},
		DEFAULT => -12
	},
	{#State 43
		ACTIONS => {
			"<" => 15,
			"==" => 18,
			"^" => 19,
			">>" => 21,
			"!=" => 22,
			"?" => 23,
			"&&" => 24,
			"||" => 25,
			"&" => 26,
			"|" => 29,
			"=>" => 30,
			"<<" => 31,
			"." => 32,
			"<=" => 33,
			">" => 34
		},
		DEFAULT => -8
	},
	{#State 44
		ACTIONS => {
			"-" => 13,
			"<" => 15,
			"+" => 16,
			"==" => 18,
			"^" => 19,
			">>" => 21,
			"!=" => 22,
			"?" => 23,
			"&&" => 24,
			"||" => 25,
			"&" => 26,
			"|" => 29,
			"=>" => 30,
			"<<" => 31,
			"." => 32,
			"<=" => 33,
			">" => 34
		},
		DEFAULT => -11
	},
	{#State 45
		ACTIONS => {
			"^" => 19,
			"?" => 23,
			"&&" => 24,
			"||" => 25,
			"&" => 26,
			"|" => 29,
			"=>" => 30,
			"." => 32,
			"<=" => 33
		},
		DEFAULT => -15
	},
	{#State 46
		ACTIONS => {
			"-" => 13,
			"<" => 15,
			"+" => 16,
			"%" => 17,
			"==" => 18,
			"^" => 19,
			"*" => 20,
			">>" => 21,
			"!=" => 22,
			"?" => 23,
			"&&" => 24,
			"||" => 25,
			"&" => 26,
			"/" => 27,
			"->" => 28,
			"|" => 29,
			"=>" => 30,
			"<<" => 31,
			"." => 32,
			"<=" => 33,
			">" => 34
		},
		DEFAULT => -31
	},
	{#State 47
		ACTIONS => {
			"-" => 13,
			"<" => 15,
			"+" => 16,
			"==" => 18,
			"^" => 19,
			">>" => 21,
			"!=" => 22,
			"?" => 23,
			"&&" => 24,
			"||" => 25,
			"&" => 26,
			"|" => 29,
			"=>" => 30,
			"<<" => 31,
			"." => 32,
			"<=" => 33,
			">" => 34
		},
		DEFAULT => -10
	},
	{#State 48
		ACTIONS => {
			"<" => 15,
			"==" => 18,
			"^" => 19,
			"!=" => 22,
			"?" => 23,
			"&&" => 24,
			"||" => 25,
			"&" => 26,
			"|" => 29,
			"=>" => 30,
			"." => 32,
			"<=" => 33,
			">" => 34
		},
		DEFAULT => -19
	},
	{#State 49
		ACTIONS => {
			"^" => 19,
			"?" => 23,
			"&&" => 24,
			"||" => 25,
			"&" => 26,
			"|" => 29,
			"=>" => 30,
			"." => 32,
			"<=" => 33
		},
		DEFAULT => -20
	},
	{#State 50
		ACTIONS => {
			":" => 66,
			"-" => 13,
			"<" => 15,
			"+" => 16,
			"%" => 17,
			"==" => 18,
			"^" => 19,
			"*" => 20,
			">>" => 21,
			"!=" => 22,
			"?" => 23,
			"&&" => 24,
			"||" => 25,
			"&" => 26,
			"/" => 27,
			"->" => 28,
			"|" => 29,
			"=>" => 30,
			"<<" => 31,
			"." => 32,
			"<=" => 33,
			">" => 34
		}
	},
	{#State 51
		ACTIONS => {
			"^" => 19,
			"?" => 23,
			"||" => 25,
			"=>" => 30,
			"." => 32,
			"<=" => 33
		},
		DEFAULT => -22
	},
	{#State 52
		ACTIONS => {
			"^" => 19,
			"?" => 23,
			"=>" => 30,
			"." => 32,
			"<=" => 33
		},
		DEFAULT => -21
	},
	{#State 53
		ACTIONS => {
			"^" => 19,
			"?" => 23,
			"&&" => 24,
			"||" => 25,
			"|" => 29,
			"=>" => 30,
			"." => 32,
			"<=" => 33
		},
		DEFAULT => -23
	},
	{#State 54
		ACTIONS => {
			"-" => 13,
			"<" => 15,
			"+" => 16,
			"==" => 18,
			"^" => 19,
			">>" => 21,
			"!=" => 22,
			"?" => 23,
			"&&" => 24,
			"||" => 25,
			"&" => 26,
			"|" => 29,
			"=>" => 30,
			"<<" => 31,
			"." => 32,
			"<=" => 33,
			">" => 34
		},
		DEFAULT => -28
	},
	{#State 55
		DEFAULT => -24
	},
	{#State 56
		ACTIONS => {
			"^" => 19,
			"?" => 23,
			"&&" => 24,
			"||" => 25,
			"=>" => 30,
			"." => 32,
			"<=" => 33
		},
		DEFAULT => -14
	},
	{#State 57
		ACTIONS => {
			"-" => 13,
			"<" => 15,
			"+" => 16,
			"%" => 17,
			"==" => 18,
			"^" => 19,
			"*" => 20,
			">>" => 21,
			"!=" => 22,
			"?" => 23,
			"&&" => 24,
			"||" => 25,
			"&" => 26,
			"/" => 27,
			"->" => 28,
			"|" => 29,
			"=>" => 30,
			"<<" => 31,
			"." => 32,
			"<=" => 33,
			">" => 34
		},
		DEFAULT => -17
	},
	{#State 58
		ACTIONS => {
			"<" => 15,
			"==" => 18,
			"^" => 19,
			"!=" => 22,
			"?" => 23,
			"&&" => 24,
			"||" => 25,
			"&" => 26,
			"|" => 29,
			"=>" => 30,
			"." => 32,
			"<=" => 33,
			">" => 34
		},
		DEFAULT => -18
	},
	{#State 59
		DEFAULT => -4
	},
	{#State 60
		ACTIONS => {
			"-" => 13,
			"<" => 15,
			"+" => 16,
			"%" => 17,
			"==" => 18,
			"^" => 19,
			"*" => 20,
			">>" => 21,
			"!=" => 22,
			"?" => 23,
			"&&" => 24,
			"||" => 25,
			"&" => 26,
			"/" => 27,
			"->" => 28,
			"|" => 29,
			"=>" => 30,
			"<<" => 31,
			"." => 32,
			"<=" => 33,
			">" => 34
		},
		DEFAULT => -16
	},
	{#State 61
		ACTIONS => {
			"==" => 18,
			"^" => 19,
			"!=" => 22,
			"?" => 23,
			"&&" => 24,
			"||" => 25,
			"&" => 26,
			"|" => 29,
			"=>" => 30,
			"." => 32,
			"<=" => 33
		},
		DEFAULT => -13
	},
	{#State 62
		DEFAULT => -32
	},
	{#State 63
		DEFAULT => -35
	},
	{#State 64
		ACTIONS => {
			"-" => 13,
			"<" => 15,
			"+" => 16,
			"%" => 17,
			"," => 67,
			"==" => 18,
			"^" => 19,
			"*" => 20,
			">>" => 21,
			"!=" => 22,
			"?" => 23,
			"&&" => 24,
			"||" => 25,
			"&" => 26,
			"->" => 28,
			"/" => 27,
			"|" => 29,
			"=>" => 30,
			"<<" => 31,
			"." => 32,
			"<=" => 33,
			">" => 34
		},
		DEFAULT => -36
	},
	{#State 65
		ACTIONS => {
			")" => 68
		}
	},
	{#State 66
		ACTIONS => {
			"-" => 1,
			"~" => 3,
			"&" => 4,
			'TEXT' => 6,
			'NUM' => 5,
			"!" => 8,
			"(" => 7,
			"*" => 9,
			'VAR' => 10
		},
		GOTOS => {
			'exp' => 69,
			'func' => 11
		}
	},
	{#State 67
		ACTIONS => {
			"-" => 1,
			"~" => 3,
			"&" => 4,
			'TEXT' => 6,
			'NUM' => 5,
			"!" => 8,
			"(" => 7,
			"*" => 9,
			'VAR' => 10
		},
		GOTOS => {
			'exp' => 64,
			'args' => 70,
			'func' => 11
		}
	},
	{#State 68
		DEFAULT => -33
	},
	{#State 69
		ACTIONS => {
			"^" => 19,
			"=>" => 30,
			"." => 32,
			"<=" => 33
		},
		DEFAULT => -25
	},
	{#State 70
		DEFAULT => -37
	}
],
                                  yyrules  =>
[
	[#Rule 0
		 '$start', 2, undef
	],
	[#Rule 1
		 'exp', 1, undef
	],
	[#Rule 2
		 'exp', 1,
sub
#line 22 "expr.yp"
{ "\"$_[1]\"" }
	],
	[#Rule 3
		 'exp', 1, undef
	],
	[#Rule 4
		 'exp', 3,
sub
#line 24 "expr.yp"
{ "$_[1].$_[3]" }
	],
	[#Rule 5
		 'exp', 1,
sub
#line 25 "expr.yp"
{ $_[0]->_Lookup($_[1]) }
	],
	[#Rule 6
		 'exp', 2,
sub
#line 26 "expr.yp"
{ $_[0]->_Dereference($_[2]); "*$_[2]" }
	],
	[#Rule 7
		 'exp', 2,
sub
#line 27 "expr.yp"
{ "~$_[2]" }
	],
	[#Rule 8
		 'exp', 3,
sub
#line 28 "expr.yp"
{ "$_[1] + $_[3]" }
	],
	[#Rule 9
		 'exp', 3,
sub
#line 29 "expr.yp"
{ "$_[1] - $_[3]" }
	],
	[#Rule 10
		 'exp', 3,
sub
#line 30 "expr.yp"
{ "$_[1] * $_[3]" }
	],
	[#Rule 11
		 'exp', 3,
sub
#line 31 "expr.yp"
{ "$_[1] % $_[3]" }
	],
	[#Rule 12
		 'exp', 3,
sub
#line 32 "expr.yp"
{ "$_[1] < $_[3]" }
	],
	[#Rule 13
		 'exp', 3,
sub
#line 33 "expr.yp"
{ "$_[1] > $_[3]" }
	],
	[#Rule 14
		 'exp', 3,
sub
#line 34 "expr.yp"
{ "$_[1] | $_[3]" }
	],
	[#Rule 15
		 'exp', 3,
sub
#line 35 "expr.yp"
{ "$_[1] == $_[3]" }
	],
	[#Rule 16
		 'exp', 3,
sub
#line 36 "expr.yp"
{ "$_[1] <= $_[3]" }
	],
	[#Rule 17
		 'exp', 3,
sub
#line 37 "expr.yp"
{ "$_[1] => $_[3]" }
	],
	[#Rule 18
		 'exp', 3,
sub
#line 38 "expr.yp"
{ "$_[1] << $_[3]" }
	],
	[#Rule 19
		 'exp', 3,
sub
#line 39 "expr.yp"
{ "$_[1] >> $_[3]" }
	],
	[#Rule 20
		 'exp', 3,
sub
#line 40 "expr.yp"
{ "$_[1] != $_[3]" }
	],
	[#Rule 21
		 'exp', 3,
sub
#line 41 "expr.yp"
{ "$_[1] || $_[3]" }
	],
	[#Rule 22
		 'exp', 3,
sub
#line 42 "expr.yp"
{ "$_[1] && $_[3]" }
	],
	[#Rule 23
		 'exp', 3,
sub
#line 43 "expr.yp"
{ "$_[1] & $_[3]" }
	],
	[#Rule 24
		 'exp', 3,
sub
#line 44 "expr.yp"
{ $_[1]."->".$_[3] }
	],
	[#Rule 25
		 'exp', 5,
sub
#line 45 "expr.yp"
{ "$_[1]?$_[3]:$_[5]" }
	],
	[#Rule 26
		 'exp', 2,
sub
#line 46 "expr.yp"
{ "~$_[1]" }
	],
	[#Rule 27
		 'exp', 2,
sub
#line 47 "expr.yp"
{ "not $_[1]" }
	],
	[#Rule 28
		 'exp', 3,
sub
#line 48 "expr.yp"
{ "$_[1] / $_[3]" }
	],
	[#Rule 29
		 'exp', 2,
sub
#line 49 "expr.yp"
{ "-$_[2]" }
	],
	[#Rule 30
		 'exp', 2,
sub
#line 50 "expr.yp"
{ "&$_[2]" }
	],
	[#Rule 31
		 'exp', 3,
sub
#line 51 "expr.yp"
{ "$_[1]^$_[3]" }
	],
	[#Rule 32
		 'exp', 3,
sub
#line 52 "expr.yp"
{ "($_[2])" }
	],
	[#Rule 33
		 'func', 4,
sub
#line 55 "expr.yp"
{ "$_[1]($_[3])" }
	],
	[#Rule 34
		 'opt_args', 0,
sub
#line 56 "expr.yp"
{ "" }
	],
	[#Rule 35
		 'opt_args', 1, undef
	],
	[#Rule 36
		 'args', 1, undef
	],
	[#Rule 37
		 'args', 3,
sub
#line 57 "expr.yp"
{ "$_[1], $_[3]" }
	]
],
                                  @_);
    bless($self,$class);
}

#line 59 "expr.yp"


package Parse::Pidl::Expr;

sub _Lexer {
    my($parser)=shift;

    $parser->YYData->{INPUT}=~s/^[ \t]//;

    for ($parser->YYData->{INPUT}) {
        if (s/^(0x[0-9A-Fa-f]+)//) {
			$parser->YYData->{LAST_TOKEN} = $1;
            return('NUM',$1);
		}
        if (s/^([0-9]+(?:\.[0-9]+)?)//) {
			$parser->YYData->{LAST_TOKEN} = $1;
            return('NUM',$1);
		}
        if (s/^([A-Za-z_][A-Za-z0-9_]*)//) {
			$parser->YYData->{LAST_TOKEN} = $1;
        	return('VAR',$1);
		}
		if (s/^\"(.*?)\"//) {
			$parser->YYData->{LAST_TOKEN} = $1;
			return('TEXT',$1); 
		}
		if (s/^(==|!=|<=|>=|->|\|\||<<|>>|&&)//s) {
			$parser->YYData->{LAST_TOKEN} = $1;
            return($1,$1);
		}
        if (s/^(.)//s) {
			$parser->YYData->{LAST_TOKEN} = $1;
            return($1,$1);
		}
    }
}

sub _Lookup($$) 
{
	my ($self, $x) = @_;
	return $self->YYData->{LOOKUP}->($x);
}

sub _Dereference($$)
{
	my ($self, $x) = @_;
	if (defined($self->YYData->{DEREFERENCE})) {
		$self->YYData->{DEREFERENCE}->($x);
	}
}

sub _Error($)
{
	my ($self) = @_;
	if (defined($self->YYData->{LAST_TOKEN})) {
		$self->YYData->{ERROR}->("Parse error in `".$self->YYData->{FULL_INPUT}."' near `". $self->YYData->{LAST_TOKEN} . "'");
	} else {
		$self->YYData->{ERROR}->("Parse error in `".$self->YYData->{FULL_INPUT}."'");
	}
}

sub Run {
    my($self, $data, $error, $lookup, $deref) = @_;
    $self->YYData->{FULL_INPUT} = $data;
    $self->YYData->{INPUT} = $data;
    $self->YYData->{LOOKUP} = $lookup;
    $self->YYData->{DEREFERENCE} = $deref;
    $self->YYData->{ERROR} = $error;
    return $self->YYParse( yylex => \&_Lexer, yyerror => \&_Error);
}

1;