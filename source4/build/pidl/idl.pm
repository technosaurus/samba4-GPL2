####################################################################
#
#    This file was generated using Parse::Yapp version 1.05.
#
#        Don't edit this file, use source file instead.
#
#             ANY CHANGE MADE HERE WILL BE LOST !
#
####################################################################
package idl;
use vars qw ( @ISA );
use strict;

@ISA= qw ( Parse::Yapp::Driver );
#Included Parse/Yapp/Driver.pm file----------------------------------------
{
#
# Module Parse::Yapp::Driver
#
# This module is part of the Parse::Yapp package available on your
# nearest CPAN
#
# Any use of this module in a standalone parser make the included
# text under the same copyright as the Parse::Yapp module itself.
#
# This notice should remain unchanged.
#
# (c) Copyright 1998-2001 Francois Desarmenien, all rights reserved.
# (see the pod text in Parse::Yapp module for use and distribution rights)
#

package Parse::Yapp::Driver;

require 5.004;

use strict;

use vars qw ( $VERSION $COMPATIBLE $FILENAME );

$VERSION = '1.05';
$COMPATIBLE = '0.07';
$FILENAME=__FILE__;

use Carp;

#Known parameters, all starting with YY (leading YY will be discarded)
my(%params)=(YYLEX => 'CODE', 'YYERROR' => 'CODE', YYVERSION => '',
			 YYRULES => 'ARRAY', YYSTATES => 'ARRAY', YYDEBUG => '');
#Mandatory parameters
my(@params)=('LEX','RULES','STATES');

sub new {
    my($class)=shift;
	my($errst,$nberr,$token,$value,$check,$dotpos);
    my($self)={ ERROR => \&_Error,
				ERRST => \$errst,
                NBERR => \$nberr,
				TOKEN => \$token,
				VALUE => \$value,
				DOTPOS => \$dotpos,
				STACK => [],
				DEBUG => 0,
				CHECK => \$check };

	_CheckParams( [], \%params, \@_, $self );

		exists($$self{VERSION})
	and	$$self{VERSION} < $COMPATIBLE
	and	croak "Yapp driver version $VERSION ".
			  "incompatible with version $$self{VERSION}:\n".
			  "Please recompile parser module.";

        ref($class)
    and $class=ref($class);

    bless($self,$class);
}

sub YYParse {
    my($self)=shift;
    my($retval);

	_CheckParams( \@params, \%params, \@_, $self );

	if($$self{DEBUG}) {
		_DBLoad();
		$retval = eval '$self->_DBParse()';#Do not create stab entry on compile
        $@ and die $@;
	}
	else {
		$retval = $self->_Parse();
	}
    $retval
}

sub YYData {
	my($self)=shift;

		exists($$self{USER})
	or	$$self{USER}={};

	$$self{USER};
	
}

sub YYErrok {
	my($self)=shift;

	${$$self{ERRST}}=0;
    undef;
}

sub YYNberr {
	my($self)=shift;

	${$$self{NBERR}};
}

sub YYRecovering {
	my($self)=shift;

	${$$self{ERRST}} != 0;
}

sub YYAbort {
	my($self)=shift;

	${$$self{CHECK}}='ABORT';
    undef;
}

sub YYAccept {
	my($self)=shift;

	${$$self{CHECK}}='ACCEPT';
    undef;
}

sub YYError {
	my($self)=shift;

	${$$self{CHECK}}='ERROR';
    undef;
}

sub YYSemval {
	my($self)=shift;
	my($index)= $_[0] - ${$$self{DOTPOS}} - 1;

		$index < 0
	and	-$index <= @{$$self{STACK}}
	and	return $$self{STACK}[$index][1];

	undef;	#Invalid index
}

sub YYCurtok {
	my($self)=shift;

        @_
    and ${$$self{TOKEN}}=$_[0];
    ${$$self{TOKEN}};
}

sub YYCurval {
	my($self)=shift;

        @_
    and ${$$self{VALUE}}=$_[0];
    ${$$self{VALUE}};
}

sub YYExpect {
    my($self)=shift;

    keys %{$self->{STATES}[$self->{STACK}[-1][0]]{ACTIONS}}
}

sub YYLexer {
    my($self)=shift;

	$$self{LEX};
}


#################
# Private stuff #
#################


sub _CheckParams {
	my($mandatory,$checklist,$inarray,$outhash)=@_;
	my($prm,$value);
	my($prmlst)={};

	while(($prm,$value)=splice(@$inarray,0,2)) {
        $prm=uc($prm);
			exists($$checklist{$prm})
		or	croak("Unknow parameter '$prm'");
			ref($value) eq $$checklist{$prm}
		or	croak("Invalid value for parameter '$prm'");
        $prm=unpack('@2A*',$prm);
		$$outhash{$prm}=$value;
	}
	for (@$mandatory) {
			exists($$outhash{$_})
		or	croak("Missing mandatory parameter '".lc($_)."'");
	}
}

sub _Error {
	print "Parse error.\n";
}

sub _DBLoad {
	{
		no strict 'refs';

			exists(${__PACKAGE__.'::'}{_DBParse})#Already loaded ?
		and	return;
	}
	my($fname)=__FILE__;
	my(@drv);
	open(DRV,"<$fname") or die "Report this as a BUG: Cannot open $fname";
	while(<DRV>) {
                	/^\s*sub\s+_Parse\s*{\s*$/ .. /^\s*}\s*#\s*_Parse\s*$/
        	and     do {
                	s/^#DBG>//;
                	push(@drv,$_);
        	}
	}
	close(DRV);

	$drv[0]=~s/_P/_DBP/;
	eval join('',@drv);
}

#Note that for loading debugging version of the driver,
#this file will be parsed from 'sub _Parse' up to '}#_Parse' inclusive.
#So, DO NOT remove comment at end of sub !!!
sub _Parse {
    my($self)=shift;

	my($rules,$states,$lex,$error)
     = @$self{ 'RULES', 'STATES', 'LEX', 'ERROR' };
	my($errstatus,$nberror,$token,$value,$stack,$check,$dotpos)
     = @$self{ 'ERRST', 'NBERR', 'TOKEN', 'VALUE', 'STACK', 'CHECK', 'DOTPOS' };

#DBG>	my($debug)=$$self{DEBUG};
#DBG>	my($dbgerror)=0;

#DBG>	my($ShowCurToken) = sub {
#DBG>		my($tok)='>';
#DBG>		for (split('',$$token)) {
#DBG>			$tok.=		(ord($_) < 32 or ord($_) > 126)
#DBG>					?	sprintf('<%02X>',ord($_))
#DBG>					:	$_;
#DBG>		}
#DBG>		$tok.='<';
#DBG>	};

	$$errstatus=0;
	$$nberror=0;
	($$token,$$value)=(undef,undef);
	@$stack=( [ 0, undef ] );
	$$check='';

    while(1) {
        my($actions,$act,$stateno);

        $stateno=$$stack[-1][0];
        $actions=$$states[$stateno];

#DBG>	print STDERR ('-' x 40),"\n";
#DBG>		$debug & 0x2
#DBG>	and	print STDERR "In state $stateno:\n";
#DBG>		$debug & 0x08
#DBG>	and	print STDERR "Stack:[".
#DBG>					 join(',',map { $$_[0] } @$stack).
#DBG>					 "]\n";


        if  (exists($$actions{ACTIONS})) {

				defined($$token)
            or	do {
				($$token,$$value)=&$lex($self);
#DBG>				$debug & 0x01
#DBG>			and	print STDERR "Need token. Got ".&$ShowCurToken."\n";
			};

            $act=   exists($$actions{ACTIONS}{$$token})
                    ?   $$actions{ACTIONS}{$$token}
                    :   exists($$actions{DEFAULT})
                        ?   $$actions{DEFAULT}
                        :   undef;
        }
        else {
            $act=$$actions{DEFAULT};
#DBG>			$debug & 0x01
#DBG>		and	print STDERR "Don't need token.\n";
        }

            defined($act)
        and do {

                $act > 0
            and do {        #shift

#DBG>				$debug & 0x04
#DBG>			and	print STDERR "Shift and go to state $act.\n";

					$$errstatus
				and	do {
					--$$errstatus;

#DBG>					$debug & 0x10
#DBG>				and	$dbgerror
#DBG>				and	$$errstatus == 0
#DBG>				and	do {
#DBG>					print STDERR "**End of Error recovery.\n";
#DBG>					$dbgerror=0;
#DBG>				};
				};


                push(@$stack,[ $act, $$value ]);

					$$token ne ''	#Don't eat the eof
				and	$$token=$$value=undef;
                next;
            };

            #reduce
            my($lhs,$len,$code,@sempar,$semval);
            ($lhs,$len,$code)=@{$$rules[-$act]};

#DBG>			$debug & 0x04
#DBG>		and	$act
#DBG>		and	print STDERR "Reduce using rule ".-$act." ($lhs,$len): ";

                $act
            or  $self->YYAccept();

            $$dotpos=$len;

                unpack('A1',$lhs) eq '@'    #In line rule
            and do {
                    $lhs =~ /^\@[0-9]+\-([0-9]+)$/
                or  die "In line rule name '$lhs' ill formed: ".
                        "report it as a BUG.\n";
                $$dotpos = $1;
            };

            @sempar =       $$dotpos
                        ?   map { $$_[1] } @$stack[ -$$dotpos .. -1 ]
                        :   ();

            $semval = $code ? &$code( $self, @sempar )
                            : @sempar ? $sempar[0] : undef;

            splice(@$stack,-$len,$len);

                $$check eq 'ACCEPT'
            and do {

#DBG>			$debug & 0x04
#DBG>		and	print STDERR "Accept.\n";

				return($semval);
			};

                $$check eq 'ABORT'
            and	do {

#DBG>			$debug & 0x04
#DBG>		and	print STDERR "Abort.\n";

				return(undef);

			};

#DBG>			$debug & 0x04
#DBG>		and	print STDERR "Back to state $$stack[-1][0], then ";

                $$check eq 'ERROR'
            or  do {
#DBG>				$debug & 0x04
#DBG>			and	print STDERR 
#DBG>				    "go to state $$states[$$stack[-1][0]]{GOTOS}{$lhs}.\n";

#DBG>				$debug & 0x10
#DBG>			and	$dbgerror
#DBG>			and	$$errstatus == 0
#DBG>			and	do {
#DBG>				print STDERR "**End of Error recovery.\n";
#DBG>				$dbgerror=0;
#DBG>			};

			    push(@$stack,
                     [ $$states[$$stack[-1][0]]{GOTOS}{$lhs}, $semval ]);
                $$check='';
                next;
            };

#DBG>			$debug & 0x04
#DBG>		and	print STDERR "Forced Error recovery.\n";

            $$check='';

        };

        #Error
            $$errstatus
        or   do {

            $$errstatus = 1;
            &$error($self);
                $$errstatus # if 0, then YYErrok has been called
            or  next;       # so continue parsing

#DBG>			$debug & 0x10
#DBG>		and	do {
#DBG>			print STDERR "**Entering Error recovery.\n";
#DBG>			++$dbgerror;
#DBG>		};

            ++$$nberror;

        };

			$$errstatus == 3	#The next token is not valid: discard it
		and	do {
				$$token eq ''	# End of input: no hope
			and	do {
#DBG>				$debug & 0x10
#DBG>			and	print STDERR "**At eof: aborting.\n";
				return(undef);
			};

#DBG>			$debug & 0x10
#DBG>		and	print STDERR "**Dicard invalid token ".&$ShowCurToken.".\n";

			$$token=$$value=undef;
		};

        $$errstatus=3;

		while(	  @$stack
			  and (		not exists($$states[$$stack[-1][0]]{ACTIONS})
			        or  not exists($$states[$$stack[-1][0]]{ACTIONS}{error})
					or	$$states[$$stack[-1][0]]{ACTIONS}{error} <= 0)) {

#DBG>			$debug & 0x10
#DBG>		and	print STDERR "**Pop state $$stack[-1][0].\n";

			pop(@$stack);
		}

			@$stack
		or	do {

#DBG>			$debug & 0x10
#DBG>		and	print STDERR "**No state left on stack: aborting.\n";

			return(undef);
		};

		#shift the error token

#DBG>			$debug & 0x10
#DBG>		and	print STDERR "**Shift \$error token and go to state ".
#DBG>						 $$states[$$stack[-1][0]]{ACTIONS}{error}.
#DBG>						 ".\n";

		push(@$stack, [ $$states[$$stack[-1][0]]{ACTIONS}{error}, undef ]);

    }

    #never reached
	croak("Error in driver logic. Please, report it as a BUG");

}#_Parse
#DO NOT remove comment

1;

}
#End of include--------------------------------------------------




sub new {
        my($class)=shift;
        ref($class)
    and $class=ref($class);

    my($self)=$class->SUPER::new( yyversion => '1.05',
                                  yystates =>
[
	{#State 0
		DEFAULT => -1,
		GOTOS => {
			'idl' => 1
		}
	},
	{#State 1
		ACTIONS => {
			'' => 2
		},
		DEFAULT => -60,
		GOTOS => {
			'interface' => 3,
			'coclass' => 4,
			'property_list' => 5
		}
	},
	{#State 2
		DEFAULT => 0
	},
	{#State 3
		DEFAULT => -2
	},
	{#State 4
		DEFAULT => -3
	},
	{#State 5
		ACTIONS => {
			"coclass" => 6,
			"interface" => 8,
			"[" => 7
		}
	},
	{#State 6
		ACTIONS => {
			'IDENTIFIER' => 9
		},
		GOTOS => {
			'identifier' => 10
		}
	},
	{#State 7
		ACTIONS => {
			'IDENTIFIER' => 9
		},
		GOTOS => {
			'identifier' => 11,
			'properties' => 13,
			'property' => 12
		}
	},
	{#State 8
		ACTIONS => {
			'IDENTIFIER' => 9
		},
		GOTOS => {
			'identifier' => 14
		}
	},
	{#State 9
		DEFAULT => -85
	},
	{#State 10
		ACTIONS => {
			"{" => 15
		}
	},
	{#State 11
		ACTIONS => {
			"(" => 16
		},
		DEFAULT => -64
	},
	{#State 12
		DEFAULT => -62
	},
	{#State 13
		ACTIONS => {
			"," => 17,
			"]" => 18
		}
	},
	{#State 14
		ACTIONS => {
			":" => 19
		},
		DEFAULT => -8,
		GOTOS => {
			'base_interface' => 20
		}
	},
	{#State 15
		DEFAULT => -5,
		GOTOS => {
			'interfaces' => 21
		}
	},
	{#State 16
		ACTIONS => {
			'CONSTANT' => 28,
			'TEXT' => 22,
			'IDENTIFIER' => 9
		},
		DEFAULT => -70,
		GOTOS => {
			'identifier' => 23,
			'listtext' => 26,
			'anytext' => 25,
			'text' => 24,
			'constant' => 27
		}
	},
	{#State 17
		ACTIONS => {
			'IDENTIFIER' => 9
		},
		GOTOS => {
			'identifier' => 11,
			'property' => 29
		}
	},
	{#State 18
		DEFAULT => -61
	},
	{#State 19
		ACTIONS => {
			'IDENTIFIER' => 9
		},
		GOTOS => {
			'identifier' => 30
		}
	},
	{#State 20
		ACTIONS => {
			"{" => 31
		}
	},
	{#State 21
		ACTIONS => {
			"}" => 32
		},
		DEFAULT => -60,
		GOTOS => {
			'interface' => 33,
			'property_list' => 34
		}
	},
	{#State 22
		DEFAULT => -87
	},
	{#State 23
		DEFAULT => -71
	},
	{#State 24
		DEFAULT => -73
	},
	{#State 25
		ACTIONS => {
			"-" => 35,
			"<" => 36,
			"+" => 37,
			"&" => 39,
			"{" => 38,
			"/" => 40,
			"|" => 41,
			"(" => 42,
			"*" => 43,
			"." => 44,
			">" => 45
		},
		DEFAULT => -66
	},
	{#State 26
		ACTIONS => {
			"," => 46,
			")" => 47
		}
	},
	{#State 27
		DEFAULT => -72
	},
	{#State 28
		DEFAULT => -86
	},
	{#State 29
		DEFAULT => -63
	},
	{#State 30
		DEFAULT => -9
	},
	{#State 31
		ACTIONS => {
			"typedef" => 48,
			"declare" => 53,
			"const" => 56
		},
		DEFAULT => -60,
		GOTOS => {
			'const' => 55,
			'declare' => 54,
			'function' => 49,
			'typedef' => 57,
			'definitions' => 50,
			'definition' => 52,
			'property_list' => 51
		}
	},
	{#State 32
		ACTIONS => {
			";" => 59
		},
		DEFAULT => -88,
		GOTOS => {
			'optional_semicolon' => 58
		}
	},
	{#State 33
		DEFAULT => -6
	},
	{#State 34
		ACTIONS => {
			"[" => 7,
			"interface" => 8
		}
	},
	{#State 35
		ACTIONS => {
			'CONSTANT' => 28,
			'TEXT' => 22,
			'IDENTIFIER' => 9
		},
		DEFAULT => -70,
		GOTOS => {
			'identifier' => 23,
			'text' => 24,
			'anytext' => 60,
			'constant' => 27
		}
	},
	{#State 36
		ACTIONS => {
			'CONSTANT' => 28,
			'TEXT' => 22,
			'IDENTIFIER' => 9
		},
		DEFAULT => -70,
		GOTOS => {
			'identifier' => 23,
			'text' => 24,
			'anytext' => 61,
			'constant' => 27
		}
	},
	{#State 37
		ACTIONS => {
			'CONSTANT' => 28,
			'TEXT' => 22,
			'IDENTIFIER' => 9
		},
		DEFAULT => -70,
		GOTOS => {
			'identifier' => 23,
			'text' => 24,
			'anytext' => 62,
			'constant' => 27
		}
	},
	{#State 38
		ACTIONS => {
			'CONSTANT' => 28,
			'TEXT' => 22,
			'IDENTIFIER' => 9
		},
		DEFAULT => -70,
		GOTOS => {
			'identifier' => 23,
			'text' => 24,
			'anytext' => 63,
			'constant' => 27,
			'commalisttext' => 64
		}
	},
	{#State 39
		ACTIONS => {
			'CONSTANT' => 28,
			'TEXT' => 22,
			'IDENTIFIER' => 9
		},
		DEFAULT => -70,
		GOTOS => {
			'identifier' => 23,
			'text' => 24,
			'anytext' => 65,
			'constant' => 27
		}
	},
	{#State 40
		ACTIONS => {
			'CONSTANT' => 28,
			'TEXT' => 22,
			'IDENTIFIER' => 9
		},
		DEFAULT => -70,
		GOTOS => {
			'identifier' => 23,
			'text' => 24,
			'anytext' => 66,
			'constant' => 27
		}
	},
	{#State 41
		ACTIONS => {
			'CONSTANT' => 28,
			'TEXT' => 22,
			'IDENTIFIER' => 9
		},
		DEFAULT => -70,
		GOTOS => {
			'identifier' => 23,
			'text' => 24,
			'anytext' => 67,
			'constant' => 27
		}
	},
	{#State 42
		ACTIONS => {
			'CONSTANT' => 28,
			'TEXT' => 22,
			'IDENTIFIER' => 9
		},
		DEFAULT => -70,
		GOTOS => {
			'identifier' => 23,
			'text' => 24,
			'anytext' => 63,
			'constant' => 27,
			'commalisttext' => 68
		}
	},
	{#State 43
		ACTIONS => {
			'CONSTANT' => 28,
			'TEXT' => 22,
			'IDENTIFIER' => 9
		},
		DEFAULT => -70,
		GOTOS => {
			'identifier' => 23,
			'text' => 24,
			'anytext' => 69,
			'constant' => 27
		}
	},
	{#State 44
		ACTIONS => {
			'CONSTANT' => 28,
			'TEXT' => 22,
			'IDENTIFIER' => 9
		},
		DEFAULT => -70,
		GOTOS => {
			'identifier' => 23,
			'text' => 24,
			'anytext' => 70,
			'constant' => 27
		}
	},
	{#State 45
		ACTIONS => {
			'CONSTANT' => 28,
			'TEXT' => 22,
			'IDENTIFIER' => 9
		},
		DEFAULT => -70,
		GOTOS => {
			'identifier' => 23,
			'text' => 24,
			'anytext' => 71,
			'constant' => 27
		}
	},
	{#State 46
		ACTIONS => {
			'CONSTANT' => 28,
			'TEXT' => 22,
			'IDENTIFIER' => 9
		},
		DEFAULT => -70,
		GOTOS => {
			'identifier' => 23,
			'text' => 24,
			'anytext' => 72,
			'constant' => 27
		}
	},
	{#State 47
		DEFAULT => -65
	},
	{#State 48
		DEFAULT => -60,
		GOTOS => {
			'property_list' => 73
		}
	},
	{#State 49
		DEFAULT => -12
	},
	{#State 50
		ACTIONS => {
			"}" => 74,
			"typedef" => 48,
			"declare" => 53,
			"const" => 56
		},
		DEFAULT => -60,
		GOTOS => {
			'const' => 55,
			'declare' => 54,
			'function' => 49,
			'typedef' => 57,
			'definition' => 75,
			'property_list' => 51
		}
	},
	{#State 51
		ACTIONS => {
			'IDENTIFIER' => 9,
			"union" => 76,
			"enum" => 77,
			"[" => 7,
			'void' => 79,
			"bitmap" => 78,
			"struct" => 86
		},
		GOTOS => {
			'identifier' => 81,
			'struct' => 82,
			'enum' => 83,
			'type' => 84,
			'union' => 85,
			'bitmap' => 80
		}
	},
	{#State 52
		DEFAULT => -10
	},
	{#State 53
		DEFAULT => -60,
		GOTOS => {
			'property_list' => 87
		}
	},
	{#State 54
		DEFAULT => -15
	},
	{#State 55
		DEFAULT => -13
	},
	{#State 56
		ACTIONS => {
			'IDENTIFIER' => 9
		},
		GOTOS => {
			'identifier' => 88
		}
	},
	{#State 57
		DEFAULT => -14
	},
	{#State 58
		DEFAULT => -4
	},
	{#State 59
		DEFAULT => -89
	},
	{#State 60
		ACTIONS => {
			"<" => 36,
			"{" => 38
		},
		DEFAULT => -74
	},
	{#State 61
		ACTIONS => {
			"-" => 35,
			"<" => 36,
			"+" => 37,
			"*" => 43,
			"{" => 38,
			"&" => 39,
			"/" => 40,
			"|" => 41,
			"(" => 42,
			"." => 44,
			">" => 45
		},
		DEFAULT => -78
	},
	{#State 62
		ACTIONS => {
			"<" => 36,
			"{" => 38
		},
		DEFAULT => -82
	},
	{#State 63
		ACTIONS => {
			"-" => 35,
			"<" => 36,
			"+" => 37,
			"&" => 39,
			"{" => 38,
			"/" => 40,
			"(" => 42,
			"|" => 41,
			"*" => 43,
			"." => 44,
			">" => 45
		},
		DEFAULT => -68
	},
	{#State 64
		ACTIONS => {
			"}" => 89,
			"," => 90
		}
	},
	{#State 65
		ACTIONS => {
			"<" => 36,
			"{" => 38
		},
		DEFAULT => -80
	},
	{#State 66
		ACTIONS => {
			"<" => 36,
			"{" => 38
		},
		DEFAULT => -81
	},
	{#State 67
		ACTIONS => {
			"<" => 36,
			"{" => 38
		},
		DEFAULT => -79
	},
	{#State 68
		ACTIONS => {
			"," => 90,
			")" => 91
		}
	},
	{#State 69
		ACTIONS => {
			"<" => 36,
			"{" => 38
		},
		DEFAULT => -76
	},
	{#State 70
		ACTIONS => {
			"<" => 36,
			"{" => 38
		},
		DEFAULT => -75
	},
	{#State 71
		ACTIONS => {
			"<" => 36,
			"{" => 38
		},
		DEFAULT => -77
	},
	{#State 72
		ACTIONS => {
			"-" => 35,
			"<" => 36,
			"+" => 37,
			"&" => 39,
			"{" => 38,
			"/" => 40,
			"(" => 42,
			"|" => 41,
			"*" => 43,
			"." => 44,
			">" => 45
		},
		DEFAULT => -67
	},
	{#State 73
		ACTIONS => {
			'IDENTIFIER' => 9,
			"union" => 76,
			"enum" => 77,
			"[" => 7,
			'void' => 79,
			"bitmap" => 78,
			"struct" => 86
		},
		GOTOS => {
			'identifier' => 81,
			'struct' => 82,
			'enum' => 83,
			'type' => 92,
			'union' => 85,
			'bitmap' => 80
		}
	},
	{#State 74
		ACTIONS => {
			";" => 59
		},
		DEFAULT => -88,
		GOTOS => {
			'optional_semicolon' => 93
		}
	},
	{#State 75
		DEFAULT => -11
	},
	{#State 76
		ACTIONS => {
			"{" => 94
		}
	},
	{#State 77
		ACTIONS => {
			"{" => 95
		}
	},
	{#State 78
		ACTIONS => {
			"{" => 96
		}
	},
	{#State 79
		DEFAULT => -30
	},
	{#State 80
		DEFAULT => -28
	},
	{#State 81
		DEFAULT => -29
	},
	{#State 82
		DEFAULT => -25
	},
	{#State 83
		DEFAULT => -27
	},
	{#State 84
		ACTIONS => {
			'IDENTIFIER' => 9
		},
		GOTOS => {
			'identifier' => 97
		}
	},
	{#State 85
		DEFAULT => -26
	},
	{#State 86
		ACTIONS => {
			"{" => 98
		}
	},
	{#State 87
		ACTIONS => {
			"enum" => 99,
			"[" => 7,
			"bitmap" => 100
		},
		GOTOS => {
			'decl_enum' => 101,
			'decl_bitmap' => 102,
			'decl_type' => 103
		}
	},
	{#State 88
		ACTIONS => {
			'IDENTIFIER' => 9
		},
		GOTOS => {
			'identifier' => 104
		}
	},
	{#State 89
		ACTIONS => {
			'CONSTANT' => 28,
			'TEXT' => 22,
			'IDENTIFIER' => 9
		},
		DEFAULT => -70,
		GOTOS => {
			'identifier' => 23,
			'text' => 24,
			'anytext' => 105,
			'constant' => 27
		}
	},
	{#State 90
		ACTIONS => {
			'CONSTANT' => 28,
			'TEXT' => 22,
			'IDENTIFIER' => 9
		},
		DEFAULT => -70,
		GOTOS => {
			'identifier' => 23,
			'text' => 24,
			'anytext' => 106,
			'constant' => 27
		}
	},
	{#State 91
		ACTIONS => {
			'CONSTANT' => 28,
			'TEXT' => 22,
			'IDENTIFIER' => 9
		},
		DEFAULT => -70,
		GOTOS => {
			'identifier' => 23,
			'text' => 24,
			'anytext' => 107,
			'constant' => 27
		}
	},
	{#State 92
		ACTIONS => {
			'IDENTIFIER' => 9
		},
		GOTOS => {
			'identifier' => 108
		}
	},
	{#State 93
		DEFAULT => -7
	},
	{#State 94
		DEFAULT => -45,
		GOTOS => {
			'union_elements' => 109
		}
	},
	{#State 95
		ACTIONS => {
			'IDENTIFIER' => 9
		},
		GOTOS => {
			'identifier' => 110,
			'enum_element' => 111,
			'enum_elements' => 112
		}
	},
	{#State 96
		ACTIONS => {
			'IDENTIFIER' => 9
		},
		GOTOS => {
			'identifier' => 115,
			'bitmap_elements' => 114,
			'bitmap_element' => 113
		}
	},
	{#State 97
		ACTIONS => {
			"(" => 116
		}
	},
	{#State 98
		DEFAULT => -51,
		GOTOS => {
			'element_list1' => 117
		}
	},
	{#State 99
		DEFAULT => -22
	},
	{#State 100
		DEFAULT => -23
	},
	{#State 101
		DEFAULT => -20
	},
	{#State 102
		DEFAULT => -21
	},
	{#State 103
		ACTIONS => {
			'IDENTIFIER' => 9
		},
		GOTOS => {
			'identifier' => 118
		}
	},
	{#State 104
		ACTIONS => {
			"[" => 121,
			"=" => 120
		},
		GOTOS => {
			'array_len' => 119
		}
	},
	{#State 105
		ACTIONS => {
			"-" => 35,
			"<" => 36,
			"+" => 37,
			"*" => 43,
			"{" => 38,
			"&" => 39,
			"/" => 40,
			"|" => 41,
			"(" => 42,
			"." => 44,
			">" => 45
		},
		DEFAULT => -84
	},
	{#State 106
		ACTIONS => {
			"-" => 35,
			"<" => 36,
			"+" => 37,
			"&" => 39,
			"{" => 38,
			"/" => 40,
			"(" => 42,
			"|" => 41,
			"*" => 43,
			"." => 44,
			">" => 45
		},
		DEFAULT => -69
	},
	{#State 107
		ACTIONS => {
			"<" => 36,
			"{" => 38
		},
		DEFAULT => -83
	},
	{#State 108
		ACTIONS => {
			"[" => 121
		},
		DEFAULT => -57,
		GOTOS => {
			'array_len' => 122
		}
	},
	{#State 109
		ACTIONS => {
			"}" => 123
		},
		DEFAULT => -60,
		GOTOS => {
			'optional_base_element' => 125,
			'property_list' => 124
		}
	},
	{#State 110
		ACTIONS => {
			"=" => 126
		},
		DEFAULT => -34
	},
	{#State 111
		DEFAULT => -32
	},
	{#State 112
		ACTIONS => {
			"}" => 127,
			"," => 128
		}
	},
	{#State 113
		DEFAULT => -37
	},
	{#State 114
		ACTIONS => {
			"}" => 129,
			"," => 130
		}
	},
	{#State 115
		ACTIONS => {
			"=" => 131
		}
	},
	{#State 116
		ACTIONS => {
			"," => -53,
			"void" => 134,
			")" => -53
		},
		DEFAULT => -60,
		GOTOS => {
			'base_element' => 132,
			'element_list2' => 135,
			'property_list' => 133
		}
	},
	{#State 117
		ACTIONS => {
			"}" => 136
		},
		DEFAULT => -60,
		GOTOS => {
			'base_element' => 137,
			'property_list' => 133
		}
	},
	{#State 118
		ACTIONS => {
			";" => 138
		}
	},
	{#State 119
		ACTIONS => {
			"=" => 139
		}
	},
	{#State 120
		ACTIONS => {
			'IDENTIFIER' => 9,
			'CONSTANT' => 28,
			'TEXT' => 22
		},
		DEFAULT => -70,
		GOTOS => {
			'identifier' => 23,
			'text' => 24,
			'anytext' => 140,
			'constant' => 27
		}
	},
	{#State 121
		ACTIONS => {
			'IDENTIFIER' => 9,
			'CONSTANT' => 28,
			'TEXT' => 22,
			"]" => 142
		},
		DEFAULT => -70,
		GOTOS => {
			'identifier' => 23,
			'text' => 24,
			'anytext' => 141,
			'constant' => 27
		}
	},
	{#State 122
		ACTIONS => {
			";" => 143
		}
	},
	{#State 123
		DEFAULT => -47
	},
	{#State 124
		ACTIONS => {
			"[" => 7
		},
		DEFAULT => -60,
		GOTOS => {
			'base_or_empty' => 144,
			'base_element' => 145,
			'empty_element' => 146,
			'property_list' => 147
		}
	},
	{#State 125
		DEFAULT => -46
	},
	{#State 126
		ACTIONS => {
			'CONSTANT' => 28,
			'TEXT' => 22,
			'IDENTIFIER' => 9
		},
		DEFAULT => -70,
		GOTOS => {
			'identifier' => 23,
			'text' => 24,
			'anytext' => 148,
			'constant' => 27
		}
	},
	{#State 127
		DEFAULT => -31
	},
	{#State 128
		ACTIONS => {
			'IDENTIFIER' => 9
		},
		GOTOS => {
			'identifier' => 110,
			'enum_element' => 149
		}
	},
	{#State 129
		DEFAULT => -36
	},
	{#State 130
		ACTIONS => {
			'IDENTIFIER' => 9
		},
		GOTOS => {
			'identifier' => 115,
			'bitmap_element' => 150
		}
	},
	{#State 131
		ACTIONS => {
			'CONSTANT' => 28,
			'TEXT' => 22,
			'IDENTIFIER' => 9
		},
		DEFAULT => -70,
		GOTOS => {
			'identifier' => 23,
			'text' => 24,
			'anytext' => 151,
			'constant' => 27
		}
	},
	{#State 132
		DEFAULT => -55
	},
	{#State 133
		ACTIONS => {
			'IDENTIFIER' => 9,
			"union" => 76,
			"enum" => 77,
			"[" => 7,
			'void' => 79,
			"bitmap" => 78,
			"struct" => 86
		},
		GOTOS => {
			'identifier' => 81,
			'struct' => 82,
			'enum' => 83,
			'type' => 152,
			'union' => 85,
			'bitmap' => 80
		}
	},
	{#State 134
		DEFAULT => -54
	},
	{#State 135
		ACTIONS => {
			"," => 153,
			")" => 154
		}
	},
	{#State 136
		DEFAULT => -40
	},
	{#State 137
		ACTIONS => {
			";" => 155
		}
	},
	{#State 138
		DEFAULT => -19
	},
	{#State 139
		ACTIONS => {
			'IDENTIFIER' => 9,
			'CONSTANT' => 28,
			'TEXT' => 22
		},
		DEFAULT => -70,
		GOTOS => {
			'identifier' => 23,
			'text' => 24,
			'anytext' => 156,
			'constant' => 27
		}
	},
	{#State 140
		ACTIONS => {
			"-" => 35,
			"<" => 36,
			";" => 157,
			"+" => 37,
			"&" => 39,
			"{" => 38,
			"/" => 40,
			"(" => 42,
			"|" => 41,
			"*" => 43,
			"." => 44,
			">" => 45
		}
	},
	{#State 141
		ACTIONS => {
			"-" => 35,
			"<" => 36,
			"+" => 37,
			"&" => 39,
			"{" => 38,
			"/" => 40,
			"(" => 42,
			"|" => 41,
			"*" => 43,
			"]" => 158,
			"." => 44,
			">" => 45
		}
	},
	{#State 142
		DEFAULT => -58
	},
	{#State 143
		DEFAULT => -24
	},
	{#State 144
		DEFAULT => -44
	},
	{#State 145
		ACTIONS => {
			";" => 159
		}
	},
	{#State 146
		DEFAULT => -43
	},
	{#State 147
		ACTIONS => {
			'IDENTIFIER' => 9,
			"union" => 76,
			";" => 160,
			"enum" => 77,
			"[" => 7,
			'void' => 79,
			"bitmap" => 78,
			"struct" => 86
		},
		GOTOS => {
			'identifier' => 81,
			'struct' => 82,
			'enum' => 83,
			'type' => 152,
			'union' => 85,
			'bitmap' => 80
		}
	},
	{#State 148
		ACTIONS => {
			"-" => 35,
			"<" => 36,
			"+" => 37,
			"&" => 39,
			"{" => 38,
			"/" => 40,
			"(" => 42,
			"|" => 41,
			"*" => 43,
			"." => 44,
			">" => 45
		},
		DEFAULT => -35
	},
	{#State 149
		DEFAULT => -33
	},
	{#State 150
		DEFAULT => -38
	},
	{#State 151
		ACTIONS => {
			"-" => 35,
			"<" => 36,
			"+" => 37,
			"&" => 39,
			"{" => 38,
			"/" => 40,
			"(" => 42,
			"|" => 41,
			"*" => 43,
			"." => 44,
			">" => 45
		},
		DEFAULT => -39
	},
	{#State 152
		DEFAULT => -49,
		GOTOS => {
			'pointers' => 161
		}
	},
	{#State 153
		DEFAULT => -60,
		GOTOS => {
			'base_element' => 162,
			'property_list' => 133
		}
	},
	{#State 154
		ACTIONS => {
			";" => 163
		}
	},
	{#State 155
		DEFAULT => -52
	},
	{#State 156
		ACTIONS => {
			"-" => 35,
			"<" => 36,
			";" => 164,
			"+" => 37,
			"&" => 39,
			"{" => 38,
			"/" => 40,
			"(" => 42,
			"|" => 41,
			"*" => 43,
			"." => 44,
			">" => 45
		}
	},
	{#State 157
		DEFAULT => -16
	},
	{#State 158
		DEFAULT => -59
	},
	{#State 159
		DEFAULT => -42
	},
	{#State 160
		DEFAULT => -41
	},
	{#State 161
		ACTIONS => {
			'IDENTIFIER' => 9,
			"*" => 166
		},
		GOTOS => {
			'identifier' => 165
		}
	},
	{#State 162
		DEFAULT => -56
	},
	{#State 163
		DEFAULT => -18
	},
	{#State 164
		DEFAULT => -17
	},
	{#State 165
		ACTIONS => {
			"[" => 121
		},
		DEFAULT => -57,
		GOTOS => {
			'array_len' => 167
		}
	},
	{#State 166
		DEFAULT => -50
	},
	{#State 167
		DEFAULT => -48
	}
],
                                  yyrules  =>
[
	[#Rule 0
		 '$start', 2, undef
	],
	[#Rule 1
		 'idl', 0, undef
	],
	[#Rule 2
		 'idl', 2,
sub
#line 19 "build/pidl/idl.yp"
{ push(@{$_[1]}, $_[2]); $_[1] }
	],
	[#Rule 3
		 'idl', 2,
sub
#line 20 "build/pidl/idl.yp"
{ push(@{$_[1]}, $_[2]); $_[1] }
	],
	[#Rule 4
		 'coclass', 7,
sub
#line 24 "build/pidl/idl.yp"
{$_[3] => {
               "TYPE" => "COCLASS", 
	       "PROPERTIES" => $_[1],
	       "NAME" => $_[3],
	       "DATA" => $_[5],
          }}
	],
	[#Rule 5
		 'interfaces', 0, undef
	],
	[#Rule 6
		 'interfaces', 2,
sub
#line 34 "build/pidl/idl.yp"
{ push(@{$_[1]}, $_[2]); $_[1] }
	],
	[#Rule 7
		 'interface', 8,
sub
#line 38 "build/pidl/idl.yp"
{$_[3] => {
               "TYPE" => "INTERFACE", 
	       "PROPERTIES" => $_[1],
	       "NAME" => $_[3],
	       "BASE" => $_[4],
	       "DATA" => $_[6],
          }}
	],
	[#Rule 8
		 'base_interface', 0, undef
	],
	[#Rule 9
		 'base_interface', 2,
sub
#line 49 "build/pidl/idl.yp"
{ $_[2] }
	],
	[#Rule 10
		 'definitions', 1,
sub
#line 53 "build/pidl/idl.yp"
{ [ $_[1] ] }
	],
	[#Rule 11
		 'definitions', 2,
sub
#line 54 "build/pidl/idl.yp"
{ push(@{$_[1]}, $_[2]); $_[1] }
	],
	[#Rule 12
		 'definition', 1, undef
	],
	[#Rule 13
		 'definition', 1, undef
	],
	[#Rule 14
		 'definition', 1, undef
	],
	[#Rule 15
		 'definition', 1, undef
	],
	[#Rule 16
		 'const', 6,
sub
#line 62 "build/pidl/idl.yp"
{{
                     "TYPE"  => "CONST", 
		     "DTYPE"  => $_[2],
		     "NAME"  => $_[3],
		     "VALUE" => $_[5]
        }}
	],
	[#Rule 17
		 'const', 7,
sub
#line 69 "build/pidl/idl.yp"
{{
                     "TYPE"  => "CONST", 
		     "DTYPE"  => $_[2],
		     "NAME"  => $_[3],
		     "ARRAY_LEN" => $_[4],
		     "VALUE" => $_[6],
        }}
	],
	[#Rule 18
		 'function', 7,
sub
#line 80 "build/pidl/idl.yp"
{{
		"TYPE" => "FUNCTION",
		"NAME" => $_[3],
		"RETURN_TYPE" => $_[2],
		"PROPERTIES" => $_[1],
		"DATA" => $_[5]
	 }}
	],
	[#Rule 19
		 'declare', 5,
sub
#line 90 "build/pidl/idl.yp"
{{
	             "TYPE" => "DECLARE", 
                     "PROPERTIES" => $_[2],
		     "NAME" => $_[4],
		     "DATA" => $_[3],
        }}
	],
	[#Rule 20
		 'decl_type', 1, undef
	],
	[#Rule 21
		 'decl_type', 1, undef
	],
	[#Rule 22
		 'decl_enum', 1,
sub
#line 102 "build/pidl/idl.yp"
{{
                     "TYPE" => "ENUM"
        }}
	],
	[#Rule 23
		 'decl_bitmap', 1,
sub
#line 108 "build/pidl/idl.yp"
{{
                     "TYPE" => "BITMAP"
        }}
	],
	[#Rule 24
		 'typedef', 6,
sub
#line 114 "build/pidl/idl.yp"
{{
	             "TYPE" => "TYPEDEF", 
                     "PROPERTIES" => $_[2],
		     "NAME" => $_[4],
		     "DATA" => $_[3],
		     "ARRAY_LEN" => $_[5]
        }}
	],
	[#Rule 25
		 'type', 1, undef
	],
	[#Rule 26
		 'type', 1, undef
	],
	[#Rule 27
		 'type', 1, undef
	],
	[#Rule 28
		 'type', 1, undef
	],
	[#Rule 29
		 'type', 1, undef
	],
	[#Rule 30
		 'type', 1,
sub
#line 124 "build/pidl/idl.yp"
{ "void" }
	],
	[#Rule 31
		 'enum', 4,
sub
#line 129 "build/pidl/idl.yp"
{{
                     "TYPE" => "ENUM", 
		     "ELEMENTS" => $_[3]
        }}
	],
	[#Rule 32
		 'enum_elements', 1,
sub
#line 136 "build/pidl/idl.yp"
{ [ $_[1] ] }
	],
	[#Rule 33
		 'enum_elements', 3,
sub
#line 137 "build/pidl/idl.yp"
{ push(@{$_[1]}, $_[3]); $_[1] }
	],
	[#Rule 34
		 'enum_element', 1, undef
	],
	[#Rule 35
		 'enum_element', 3,
sub
#line 141 "build/pidl/idl.yp"
{ "$_[1]$_[2]$_[3]" }
	],
	[#Rule 36
		 'bitmap', 4,
sub
#line 145 "build/pidl/idl.yp"
{{
                     "TYPE" => "BITMAP", 
		     "ELEMENTS" => $_[3]
        }}
	],
	[#Rule 37
		 'bitmap_elements', 1,
sub
#line 152 "build/pidl/idl.yp"
{ [ $_[1] ] }
	],
	[#Rule 38
		 'bitmap_elements', 3,
sub
#line 153 "build/pidl/idl.yp"
{ push(@{$_[1]}, $_[3]); $_[1] }
	],
	[#Rule 39
		 'bitmap_element', 3,
sub
#line 156 "build/pidl/idl.yp"
{ "$_[1] ( $_[3] )" }
	],
	[#Rule 40
		 'struct', 4,
sub
#line 160 "build/pidl/idl.yp"
{{
                     "TYPE" => "STRUCT", 
		     "ELEMENTS" => $_[3]
        }}
	],
	[#Rule 41
		 'empty_element', 2,
sub
#line 167 "build/pidl/idl.yp"
{{
		 "NAME" => "",
		 "TYPE" => "EMPTY",
		 "PROPERTIES" => $_[0],
		 "POINTERS" => 0
	 }}
	],
	[#Rule 42
		 'base_or_empty', 2, undef
	],
	[#Rule 43
		 'base_or_empty', 1, undef
	],
	[#Rule 44
		 'optional_base_element', 2,
sub
#line 178 "build/pidl/idl.yp"
{ $_[2]->{PROPERTIES} = util::FlattenHash([$_[1],$_[2]->{PROPERTIES}]); $_[2] }
	],
	[#Rule 45
		 'union_elements', 0, undef
	],
	[#Rule 46
		 'union_elements', 2,
sub
#line 183 "build/pidl/idl.yp"
{ push(@{$_[1]}, $_[2]); $_[1] }
	],
	[#Rule 47
		 'union', 4,
sub
#line 187 "build/pidl/idl.yp"
{{
                     "TYPE" => "UNION", 
		     "ELEMENTS" => $_[3]
        }}
	],
	[#Rule 48
		 'base_element', 5,
sub
#line 194 "build/pidl/idl.yp"
{{
			   "NAME" => $_[4],
			   "TYPE" => $_[2],
			   "PROPERTIES" => $_[1],
			   "POINTERS" => $_[3],
			   "ARRAY_LEN" => $_[5]
              }}
	],
	[#Rule 49
		 'pointers', 0,
sub
#line 206 "build/pidl/idl.yp"
{ 0 }
	],
	[#Rule 50
		 'pointers', 2,
sub
#line 207 "build/pidl/idl.yp"
{ $_[1]+1 }
	],
	[#Rule 51
		 'element_list1', 0, undef
	],
	[#Rule 52
		 'element_list1', 3,
sub
#line 212 "build/pidl/idl.yp"
{ push(@{$_[1]}, $_[2]); $_[1] }
	],
	[#Rule 53
		 'element_list2', 0, undef
	],
	[#Rule 54
		 'element_list2', 1, undef
	],
	[#Rule 55
		 'element_list2', 1,
sub
#line 218 "build/pidl/idl.yp"
{ [ $_[1] ] }
	],
	[#Rule 56
		 'element_list2', 3,
sub
#line 219 "build/pidl/idl.yp"
{ push(@{$_[1]}, $_[3]); $_[1] }
	],
	[#Rule 57
		 'array_len', 0, undef
	],
	[#Rule 58
		 'array_len', 2,
sub
#line 224 "build/pidl/idl.yp"
{ "*" }
	],
	[#Rule 59
		 'array_len', 3,
sub
#line 225 "build/pidl/idl.yp"
{ "$_[2]" }
	],
	[#Rule 60
		 'property_list', 0, undef
	],
	[#Rule 61
		 'property_list', 4,
sub
#line 231 "build/pidl/idl.yp"
{ util::FlattenHash([$_[1],$_[3]]); }
	],
	[#Rule 62
		 'properties', 1,
sub
#line 234 "build/pidl/idl.yp"
{ $_[1] }
	],
	[#Rule 63
		 'properties', 3,
sub
#line 235 "build/pidl/idl.yp"
{ util::FlattenHash([$_[1], $_[3]]); }
	],
	[#Rule 64
		 'property', 1,
sub
#line 238 "build/pidl/idl.yp"
{{ "$_[1]" => "1"     }}
	],
	[#Rule 65
		 'property', 4,
sub
#line 239 "build/pidl/idl.yp"
{{ "$_[1]" => "$_[3]" }}
	],
	[#Rule 66
		 'listtext', 1, undef
	],
	[#Rule 67
		 'listtext', 3,
sub
#line 244 "build/pidl/idl.yp"
{ "$_[1] $_[3]" }
	],
	[#Rule 68
		 'commalisttext', 1, undef
	],
	[#Rule 69
		 'commalisttext', 3,
sub
#line 249 "build/pidl/idl.yp"
{ "$_[1],$_[3]" }
	],
	[#Rule 70
		 'anytext', 0,
sub
#line 253 "build/pidl/idl.yp"
{ "" }
	],
	[#Rule 71
		 'anytext', 1, undef
	],
	[#Rule 72
		 'anytext', 1, undef
	],
	[#Rule 73
		 'anytext', 1, undef
	],
	[#Rule 74
		 'anytext', 3,
sub
#line 255 "build/pidl/idl.yp"
{ "$_[1]$_[2]$_[3]" }
	],
	[#Rule 75
		 'anytext', 3,
sub
#line 256 "build/pidl/idl.yp"
{ "$_[1]$_[2]$_[3]" }
	],
	[#Rule 76
		 'anytext', 3,
sub
#line 257 "build/pidl/idl.yp"
{ "$_[1]$_[2]$_[3]" }
	],
	[#Rule 77
		 'anytext', 3,
sub
#line 258 "build/pidl/idl.yp"
{ "$_[1]$_[2]$_[3]" }
	],
	[#Rule 78
		 'anytext', 3,
sub
#line 259 "build/pidl/idl.yp"
{ "$_[1]$_[2]$_[3]" }
	],
	[#Rule 79
		 'anytext', 3,
sub
#line 260 "build/pidl/idl.yp"
{ "$_[1]$_[2]$_[3]" }
	],
	[#Rule 80
		 'anytext', 3,
sub
#line 261 "build/pidl/idl.yp"
{ "$_[1]$_[2]$_[3]" }
	],
	[#Rule 81
		 'anytext', 3,
sub
#line 262 "build/pidl/idl.yp"
{ "$_[1]$_[2]$_[3]" }
	],
	[#Rule 82
		 'anytext', 3,
sub
#line 263 "build/pidl/idl.yp"
{ "$_[1]$_[2]$_[3]" }
	],
	[#Rule 83
		 'anytext', 5,
sub
#line 264 "build/pidl/idl.yp"
{ "$_[1]$_[2]$_[3]$_[4]$_[5]" }
	],
	[#Rule 84
		 'anytext', 5,
sub
#line 265 "build/pidl/idl.yp"
{ "$_[1]$_[2]$_[3]$_[4]$_[5]" }
	],
	[#Rule 85
		 'identifier', 1, undef
	],
	[#Rule 86
		 'constant', 1, undef
	],
	[#Rule 87
		 'text', 1,
sub
#line 274 "build/pidl/idl.yp"
{ "\"$_[1]\"" }
	],
	[#Rule 88
		 'optional_semicolon', 0, undef
	],
	[#Rule 89
		 'optional_semicolon', 1, undef
	]
],
                                  @_);
    bless($self,$class);
}

#line 285 "build/pidl/idl.yp"


use util;

sub _Error {
        if (exists $_[0]->YYData->{ERRMSG}) {
		print $_[0]->YYData->{ERRMSG};
		delete $_[0]->YYData->{ERRMSG};
		return;
	};
	my $line = $_[0]->YYData->{LINE};
	my $last_token = $_[0]->YYData->{LAST_TOKEN};
	my $file = $_[0]->YYData->{INPUT_FILENAME};
	
	print "$file:$line: Syntax error near '$last_token'\n";
}

sub _Lexer($)
{
	my($parser)=shift;

        $parser->YYData->{INPUT}
        or  return('',undef);

again:
	$parser->YYData->{INPUT} =~ s/^[ \t]*//;

	for ($parser->YYData->{INPUT}) {
		if (/^\#/) {
			if (s/^\# (\d+) \"(.*?)\"( \d+|)//) {
				$parser->YYData->{LINE} = $1-1;
				$parser->YYData->{INPUT_FILENAME} = $2;
				goto again;
			}
			if (s/^\#line (\d+) \"(.*?)\"( \d+|)//) {
				$parser->YYData->{LINE} = $1-1;
				$parser->YYData->{INPUT_FILENAME} = $2;
				goto again;
			}
			if (s/^(\#.*)$//m) {
				goto again;
			}
		}
		if (s/^(\n)//) {
			$parser->YYData->{LINE}++;
			goto again;
		}
		if (s/^\"(.*?)\"//) {
			$parser->YYData->{LAST_TOKEN} = $1;
			return('TEXT',$1); 
		}
		if (s/^(\d+)(\W|$)/$2/) {
			$parser->YYData->{LAST_TOKEN} = $1;
			return('CONSTANT',$1); 
		}
		if (s/^([\w_]+)//) {
			$parser->YYData->{LAST_TOKEN} = $1;
			if ($1 =~ 
			    /^(coclass|interface|const|typedef|declare|union
			      |struct|enum|bitmap|void)$/x) {
				return $1;
			}
			return('IDENTIFIER',$1);
		}
		if (s/^(.)//s) {
			$parser->YYData->{LAST_TOKEN} = $1;
			return($1,$1);
		}
	}
}

sub parse_idl($$)
{
	my $self = shift;
	my $filename = shift;

	my $saved_delim = $/;
	undef $/;
	my $cpp = $ENV{CPP};
	if (! defined $cpp) {
		$cpp = "cpp"
	}
	my $data = `$cpp -xc $filename`;
	$/ = $saved_delim;

    $self->YYData->{INPUT} = $data;
    $self->YYData->{LINE} = 0;
    $self->YYData->{LAST_TOKEN} = "NONE";

	my $idl = $self->YYParse( yylex => \&_Lexer, yyerror => \&_Error );

	foreach my $x (@{$idl}) {
		# Add [in] ORPCTHIS *this, [out] ORPCTHAT *that
		# for 'object' interfaces
		if (defined($x->{PROPERTIES}->{object})) {
			foreach my $e (@{$x->{DATA}}) {
				if($e->{TYPE} eq "FUNCTION") {
					$e->{PROPERTIES}->{object} = 1;
					unshift(@{$e->{DATA}}, 
                        { 'NAME' => 'ORPCthis',
                          'POINTERS' => 0,
                          'PROPERTIES' => { 'in' => '1' },
                          'TYPE' => 'ORPCTHIS'
                        });
					unshift(@{$e->{DATA}},
                        { 'NAME' => 'ORPCthat',
                          'POINTERS' => 0,
                          'PROPERTIES' => { 'out' => '1' },
						  'TYPE' => 'ORPCTHAT'
                        });
				}
			}
		}
		
		# Do the inheritance
		if (defined($x->{BASE}) and $x->{BASE} ne "") {
			my $parent = util::get_interface($idl, $x->{BASE});

			if(not defined($parent)) { 
				die("No such parent interface " . $x->{BASE});
			}
			
			@{$x->{INHERITED_DATA}} = (@{$parent->{INHERITED_DATA}}, @{$x->{DATA}});
		} else {
			$x->{INHERITED_DATA} = $x->{DATA};
		}
	}

	return $idl;
}

1;
