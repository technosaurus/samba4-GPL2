/*
 * Copyright (c) 1997 - 2006 Kungliga Tekniska H�gskolan
 * (Royal Institute of Technology, Stockholm, Sweden). 
 * All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 *
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright 
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the distribution. 
 *
 * 3. Neither the name of the Institute nor the names of its contributors 
 *    may be used to endorse or promote products derived from this software 
 *    without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE 
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS 
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY 
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE. 
 */

#include "gen_locl.h"
#include "lex.h"

RCSID("$Id: gen_decode.c 19572 2006-12-29 17:30:32Z lha $");

static void
decode_primitive (const char *typename, const char *name, const char *forwstr)
{
#if 0
    fprintf (codefile,
	     "e = decode_%s(p, len, %s, &l);\n"
	     "%s;\n",
	     typename,
	     name,
	     forwstr);
#else
    fprintf (codefile,
	     "e = der_get_%s(p, len, %s, &l);\n"
	     "if(e) %s;\np += l; len -= l; ret += l;\n",
	     typename,
	     name,
	     forwstr);
#endif
}

static int
is_primitive_type(int type)
{
    switch(type) {
    case TInteger:
    case TBoolean:
    case TOctetString:
    case TBitString:
    case TEnumerated:
    case TGeneralizedTime:
    case TGeneralString:
    case TOID:
    case TUTCTime:
    case TUTF8String:
    case TPrintableString:
    case TIA5String:
    case TBMPString:
    case TUniversalString:
    case TVisibleString:
    case TNull:
	return 1;
    default:
	return 0;
    }
}

static void
find_tag (const Type *t,
	  Der_class *cl, Der_type *ty, unsigned *tag)
{
    switch (t->type) {
    case TBitString:
	*cl  = ASN1_C_UNIV;
	*ty  = PRIM;
	*tag = UT_BitString;
	break;
    case TBoolean:
	*cl  = ASN1_C_UNIV;
	*ty  = PRIM;
	*tag = UT_Boolean;
	break;
    case TChoice: 
	errx(1, "Cannot have recursive CHOICE");
    case TEnumerated:
	*cl  = ASN1_C_UNIV;
	*ty  = PRIM;
	*tag = UT_Enumerated;
	break;
    case TGeneralString: 
	*cl  = ASN1_C_UNIV;
	*ty  = PRIM;
	*tag = UT_GeneralString;
	break;
    case TGeneralizedTime: 
	*cl  = ASN1_C_UNIV;
	*ty  = PRIM;
	*tag = UT_GeneralizedTime;
	break;
    case TIA5String:
	*cl  = ASN1_C_UNIV;
	*ty  = PRIM;
	*tag = UT_IA5String;
	break;
    case TInteger: 
	*cl  = ASN1_C_UNIV;
	*ty  = PRIM;
	*tag = UT_Integer;
	break;
    case TNull:
	*cl  = ASN1_C_UNIV;
	*ty  = PRIM;
	*tag = UT_Null;
	break;
    case TOID: 
	*cl  = ASN1_C_UNIV;
	*ty  = PRIM;
	*tag = UT_OID;
	break;
    case TOctetString: 
	*cl  = ASN1_C_UNIV;
	*ty  = PRIM;
	*tag = UT_OctetString;
	break;
    case TPrintableString:
	*cl  = ASN1_C_UNIV;
	*ty  = PRIM;
	*tag = UT_PrintableString;
	break;
    case TSequence: 
    case TSequenceOf:
	*cl  = ASN1_C_UNIV;
	*ty  = CONS;
	*tag = UT_Sequence;
	break;
    case TSet: 
    case TSetOf:
	*cl  = ASN1_C_UNIV;
	*ty  = CONS;
	*tag = UT_Set;
	break;
    case TTag: 
	*cl  = t->tag.tagclass;
	*ty  = is_primitive_type(t->subtype->type) ? PRIM : CONS;
	*tag = t->tag.tagvalue;
	break;
    case TType: 
	if ((t->symbol->stype == Stype && t->symbol->type == NULL)
	    || t->symbol->stype == SUndefined) {
	    error_message("%s is imported or still undefined, "
			  " can't generate tag checking data in CHOICE "
			  "without this information",
			  t->symbol->name);
	    exit(1);
	}
	find_tag(t->symbol->type, cl, ty, tag);
	return;
    case TUTCTime: 
	*cl  = ASN1_C_UNIV;
	*ty  = PRIM;
	*tag = UT_UTCTime;
	break;
    case TUTF8String:
	*cl  = ASN1_C_UNIV;
	*ty  = PRIM;
	*tag = UT_UTF8String;
	break;
    case TBMPString:
	*cl  = ASN1_C_UNIV;
	*ty  = PRIM;
	*tag = UT_BMPString;
	break;
    case TUniversalString:
	*cl  = ASN1_C_UNIV;
	*ty  = PRIM;
	*tag = UT_UniversalString;
	break;
    case TVisibleString:
	*cl  = ASN1_C_UNIV;
	*ty  = PRIM;
	*tag = UT_VisibleString;
	break;
    default:
	abort();
    }
}

static int
decode_type (const char *name, const Type *t, int optional, 
	     const char *forwstr, const char *tmpstr)
{
    switch (t->type) {
    case TType: {
	if (optional)
	    fprintf(codefile, 
		    "%s = calloc(1, sizeof(*%s));\n"
		    "if (%s == NULL) %s;\n",
		    name, name, name, forwstr);
	fprintf (codefile,
		 "e = decode_%s(p, len, %s, &l);\n",
		 t->symbol->gen_name, name);
	if (optional) {
	    fprintf (codefile,
		     "if(e) {\n"
		     "free(%s);\n"
		     "%s = NULL;\n"
		     "} else {\n"
		     "p += l; len -= l; ret += l;\n"
		     "}\n",
		     name, name);
	} else {
	    fprintf (codefile,
		     "if(e) %s;\n",
		     forwstr);
	    fprintf (codefile,
		     "p += l; len -= l; ret += l;\n");
	}
	break;
    }
    case TInteger:
	if(t->members) {
	    char *s;
	    asprintf(&s, "(int*)%s", name);
	    if (s == NULL)
		errx (1, "out of memory");
	    decode_primitive ("integer", s, forwstr);
	    free(s);
	} else if (t->range == NULL) {
	    decode_primitive ("heim_integer", name, forwstr);
	} else if (t->range->min == INT_MIN && t->range->max == INT_MAX) {
	    decode_primitive ("integer", name, forwstr);
	} else if (t->range->min == 0 && t->range->max == UINT_MAX) {
	    decode_primitive ("unsigned", name, forwstr);
	} else if (t->range->min == 0 && t->range->max == INT_MAX) {
	    decode_primitive ("unsigned", name, forwstr);
	} else
	    errx(1, "%s: unsupported range %d -> %d", 
		 name, t->range->min, t->range->max);
	break;
    case TBoolean:
      decode_primitive ("boolean", name, forwstr);
      break;
    case TEnumerated:
	decode_primitive ("enumerated", name, forwstr);
	break;
    case TOctetString:
	decode_primitive ("octet_string", name, forwstr);
	break;
    case TBitString: {
	Member *m;
	int pos = 0;

	if (ASN1_TAILQ_EMPTY(t->members)) {
	    decode_primitive ("bit_string", name, forwstr);
	    break;
	}
	fprintf(codefile,
		"if (len < 1) return ASN1_OVERRUN;\n"
		"p++; len--; ret++;\n");
	fprintf(codefile,
		"do {\n"
		"if (len < 1) break;\n");
	ASN1_TAILQ_FOREACH(m, t->members, members) {
	    while (m->val / 8 > pos / 8) {
		fprintf (codefile,
			 "p++; len--; ret++;\n"
			 "if (len < 1) break;\n");
		pos += 8;
	    }
	    fprintf (codefile,
		     "(%s)->%s = (*p >> %d) & 1;\n",
		     name, m->gen_name, 7 - m->val % 8);
	}
	fprintf(codefile,
		"} while(0);\n");
	fprintf (codefile,
		 "p += len; ret += len;\n");
	break;
    }
    case TSequence: {
	Member *m;

	if (t->members == NULL)
	    break;

	ASN1_TAILQ_FOREACH(m, t->members, members) {
	    char *s;

	    if (m->ellipsis)
		continue;

	    asprintf (&s, "%s(%s)->%s", m->optional ? "" : "&",
		      name, m->gen_name);
	    if (s == NULL)
		errx(1, "malloc");
	    decode_type (s, m->type, m->optional, forwstr, m->gen_name);
	    free (s);
	}
	
	break;
    }
    case TSet: {
	Member *m;
	unsigned int memno;

	if(t->members == NULL)
	    break;

	fprintf(codefile, "{\n");
	fprintf(codefile, "unsigned int members = 0;\n");
	fprintf(codefile, "while(len > 0) {\n");
	fprintf(codefile, 
		"Der_class class;\n"
		"Der_type type;\n"
		"int tag;\n"
		"e = der_get_tag (p, len, &class, &type, &tag, NULL);\n"
		"if(e) %s;\n", forwstr);
	fprintf(codefile, "switch (MAKE_TAG(class, type, tag)) {\n");
	memno = 0;
	ASN1_TAILQ_FOREACH(m, t->members, members) {
	    char *s;

	    assert(m->type->type == TTag);

	    fprintf(codefile, "case MAKE_TAG(%s, %s, %s):\n",
		    classname(m->type->tag.tagclass),
		    is_primitive_type(m->type->subtype->type) ? "PRIM" : "CONS",
		    valuename(m->type->tag.tagclass, m->type->tag.tagvalue));

	    asprintf (&s, "%s(%s)->%s", m->optional ? "" : "&", name, m->gen_name);
	    if (s == NULL)
		errx(1, "malloc");
	    if(m->optional)
		fprintf(codefile, 
			"%s = calloc(1, sizeof(*%s));\n"
			"if (%s == NULL) { e = ENOMEM; %s; }\n",
			s, s, s, forwstr);
	    decode_type (s, m->type, 0, forwstr, m->gen_name);
	    free (s);

	    fprintf(codefile, "members |= (1 << %d);\n", memno);
	    memno++;
	    fprintf(codefile, "break;\n");
	}
	fprintf(codefile, 
		"default:\n"
		"return ASN1_MISPLACED_FIELD;\n"
		"break;\n");
	fprintf(codefile, "}\n");
	fprintf(codefile, "}\n");
	memno = 0;
	ASN1_TAILQ_FOREACH(m, t->members, members) {
	    char *s;

	    asprintf (&s, "%s->%s", name, m->gen_name);
	    if (s == NULL)
		errx(1, "malloc");
	    fprintf(codefile, "if((members & (1 << %d)) == 0)\n", memno);
	    if(m->optional)
		fprintf(codefile, "%s = NULL;\n", s);
	    else if(m->defval)
		gen_assign_defval(s, m->defval);
	    else
		fprintf(codefile, "return ASN1_MISSING_FIELD;\n");
	    free(s);
	    memno++;
	}
	fprintf(codefile, "}\n");
	break;
    }
    case TSetOf:
    case TSequenceOf: {
	char *n;
	char *sname;

	fprintf (codefile,
		 "{\n"
		 "size_t %s_origlen = len;\n"
		 "size_t %s_oldret = ret;\n"
		 "void *%s_tmp;\n"
		 "ret = 0;\n"
		 "(%s)->len = 0;\n"
		 "(%s)->val = NULL;\n"
		 "while(ret < %s_origlen) {\n"
		 "%s_tmp = realloc((%s)->val, "
		 "    sizeof(*((%s)->val)) * ((%s)->len + 1));\n"
		 "if (%s_tmp == NULL) { %s; }\n"
		 "(%s)->val = %s_tmp;\n",
		 tmpstr, tmpstr, tmpstr,
		 name, name,
		 tmpstr, tmpstr,
		 name, name, name,
		 tmpstr, forwstr, 
		 name, tmpstr);

	asprintf (&n, "&(%s)->val[(%s)->len]", name, name);
	if (n == NULL)
	    errx(1, "malloc");
	asprintf (&sname, "%s_s_of", tmpstr);
	if (sname == NULL)
	    errx(1, "malloc");
	decode_type (n, t->subtype, 0, forwstr, sname);
	fprintf (codefile, 
		 "(%s)->len++;\n"
		 "len = %s_origlen - ret;\n"
		 "}\n"
		 "ret += %s_oldret;\n"
		 "}\n",
		 name,
		 tmpstr, tmpstr);
	free (n);
	free (sname);
	break;
    }
    case TGeneralizedTime:
	decode_primitive ("generalized_time", name, forwstr);
	break;
    case TGeneralString:
	decode_primitive ("general_string", name, forwstr);
	break;
    case TTag:{
    	char *tname;

	fprintf(codefile, 
		"{\n"
		"size_t %s_datalen, %s_oldlen;\n",
		tmpstr, tmpstr);
	if(dce_fix)
	    fprintf(codefile, 
		    "int dce_fix;\n");
	fprintf(codefile, "e = der_match_tag_and_length(p, len, %s, %s, %s, "
		"&%s_datalen, &l);\n",
		classname(t->tag.tagclass),
		is_primitive_type(t->subtype->type) ? "PRIM" : "CONS",
		valuename(t->tag.tagclass, t->tag.tagvalue),
		tmpstr);
	if(optional) {
	    fprintf(codefile, 
		    "if(e) {\n"
		    "%s = NULL;\n"
		    "} else {\n"
		     "%s = calloc(1, sizeof(*%s));\n"
		     "if (%s == NULL) { e = ENOMEM; %s; }\n",
		     name, name, name, name, forwstr);
	} else {
	    fprintf(codefile, "if(e) %s;\n", forwstr);
	}
	fprintf (codefile,
		 "p += l; len -= l; ret += l;\n"
		 "%s_oldlen = len;\n",
		 tmpstr);
	if(dce_fix)
	    fprintf (codefile,
		     "if((dce_fix = _heim_fix_dce(%s_datalen, &len)) < 0)\n"
		     "{ e = ASN1_BAD_FORMAT; %s; }\n",
		     tmpstr, forwstr);
	else
	    fprintf(codefile, 
		    "if (%s_datalen > len) { e = ASN1_OVERRUN; %s; }\n"
		    "len = %s_datalen;\n", tmpstr, forwstr, tmpstr);
	asprintf (&tname, "%s_Tag", tmpstr);
	if (tname == NULL)
	    errx(1, "malloc");
	decode_type (name, t->subtype, 0, forwstr, tname);
	if(dce_fix)
	    fprintf(codefile,
		    "if(dce_fix){\n"
		    "e = der_match_tag_and_length (p, len, "
		    "(Der_class)0,(Der_type)0, UT_EndOfContent, "
		    "&%s_datalen, &l);\n"
		    "if(e) %s;\np += l; len -= l; ret += l;\n"
		    "} else \n", tmpstr, forwstr);
	fprintf(codefile, 
		"len = %s_oldlen - %s_datalen;\n",
		tmpstr, tmpstr);
	if(optional)
	    fprintf(codefile, 
		    "}\n");
	fprintf(codefile, 
		"}\n");
	free(tname);
	break;
    }
    case TChoice: {
	Member *m, *have_ellipsis = NULL;
	const char *els = "";

	if (t->members == NULL)
	    break;

	ASN1_TAILQ_FOREACH(m, t->members, members) {
	    const Type *tt = m->type;
	    char *s;
	    Der_class cl;
	    Der_type  ty;
	    unsigned  tag;
	    
	    if (m->ellipsis) {
		have_ellipsis = m;
		continue;
	    }

	    find_tag(tt, &cl, &ty, &tag);

	    fprintf(codefile,
		    "%sif (der_match_tag(p, len, %s, %s, %s, NULL) == 0) {\n",
		    els,
		    classname(cl),
		    ty ? "CONS" : "PRIM",
		    valuename(cl, tag));
	    asprintf (&s, "%s(%s)->u.%s", m->optional ? "" : "&",
		      name, m->gen_name);
	    if (s == NULL)
		errx(1, "malloc");
	    decode_type (s, m->type, m->optional, forwstr, m->gen_name);
	    fprintf(codefile,
		    "(%s)->element = %s;\n",
		    name, m->label);
	    free(s);
	    fprintf(codefile,
		    "}\n");
	    els = "else ";
	}
	if (have_ellipsis) {
	    fprintf(codefile,
		    "else {\n"
		    "(%s)->u.%s.data = calloc(1, len);\n"
		    "if ((%s)->u.%s.data == NULL) {\n"
		    "e = ENOMEM; %s;\n"
		    "}\n"
		    "(%s)->u.%s.length = len;\n"
		    "memcpy((%s)->u.%s.data, p, len);\n"
		    "(%s)->element = %s;\n"
		    "p += len;\n"
		    "ret += len;\n"
		    "len -= len;\n"
		    "}\n",
		    name, have_ellipsis->gen_name,
		    name, have_ellipsis->gen_name,
		    forwstr, 
		    name, have_ellipsis->gen_name,
		    name, have_ellipsis->gen_name,
		    name, have_ellipsis->label);
	} else {
	    fprintf(codefile,
		    "else {\n"
		    "e = ASN1_PARSE_ERROR;\n"
		    "%s;\n"
		    "}\n",
		    forwstr);
	}
	break;
    }
    case TUTCTime:
	decode_primitive ("utctime", name, forwstr);
	break;
    case TUTF8String:
	decode_primitive ("utf8string", name, forwstr);
	break;
    case TPrintableString:
	decode_primitive ("printable_string", name, forwstr);
	break;
    case TIA5String:
	decode_primitive ("ia5_string", name, forwstr);
	break;
    case TBMPString:
	decode_primitive ("bmp_string", name, forwstr);
	break;
    case TUniversalString:
	decode_primitive ("universal_string", name, forwstr);
	break;
    case TVisibleString:
	decode_primitive ("visible_string", name, forwstr);
	break;
    case TNull:
	fprintf (codefile, "/* NULL */\n");
	break;
    case TOID:
	decode_primitive ("oid", name, forwstr);
	break;
    default :
	abort ();
    }
    return 0;
}

void
generate_type_decode (const Symbol *s)
{
    int preserve = preserve_type(s->name) ? TRUE : FALSE;

    fprintf (headerfile,
	     "int    "
	     "decode_%s(const unsigned char *, size_t, %s *, size_t *);\n",
	     s->gen_name, s->gen_name);

    fprintf (codefile, "int\n"
	     "decode_%s(const unsigned char *p,"
	     " size_t len, %s *data, size_t *size)\n"
	     "{\n",
	     s->gen_name, s->gen_name);

    switch (s->type->type) {
    case TInteger:
    case TBoolean:
    case TOctetString:
    case TOID:
    case TGeneralizedTime:
    case TGeneralString:
    case TUTF8String:
    case TPrintableString:
    case TIA5String:
    case TBMPString:
    case TUniversalString:
    case TVisibleString:
    case TUTCTime:
    case TNull:
    case TEnumerated:
    case TBitString:
    case TSequence:
    case TSequenceOf:
    case TSet:
    case TSetOf:
    case TTag:
    case TType:
    case TChoice:
	fprintf (codefile,
		 "size_t ret = 0;\n"
		 "size_t l;\n"
		 "int e;\n");
	if (preserve)
	    fprintf (codefile, "const unsigned char *begin = p;\n");

	fprintf (codefile, "\n");
	fprintf (codefile, "memset(data, 0, sizeof(*data));\n"); /* hack to avoid `unused variable' */

	decode_type ("data", s->type, 0, "goto fail", "Top");
	if (preserve)
	    fprintf (codefile,
		     "data->_save.data = calloc(1, ret);\n"
		     "if (data->_save.data == NULL) { \n"
		     "e = ENOMEM; goto fail; \n"
		     "}\n"
		     "data->_save.length = ret;\n"
		     "memcpy(data->_save.data, begin, ret);\n");
	fprintf (codefile, 
		 "if(size) *size = ret;\n"
		 "return 0;\n");
	fprintf (codefile,
		 "fail:\n"
		 "free_%s(data);\n"
		 "return e;\n",
		 s->gen_name);
	break;
    default:
	abort ();
    }
    fprintf (codefile, "}\n\n");
}
