/*
	js functions and code common to all pages
*/

/* define some global variables for this request */
global.page = new Object();

/* fill in some defaults */
global.page.title = "Samba Web Administration Tool";

/* to cope with browsers that don't support cookies we append the sessionid
   to the URI */
global.SESSIONURI = "";
if (request['COOKIE_SUPPORT'] != "True") {
	global.SESSIONURI="?SwatSessionId=" + request['SESSION_ID'];
}

/*
  possibly adjust a local URI to have the session id appended
  used for browsers that don't support cookies
*/
function session_uri(uri) {
	return uri + global.SESSIONURI;
}



/* if the browser was too dumb to set the HOST header, then
   set it now */
if (headers['HOST'] == undefined) {
	headers['HOST'] = server['SERVER_HOST'] + ":" + server['SERVER_PORT'];
}

/*
  show the page header. page types include "plain" and "column" 
*/
function page_header(pagetype, title) {
	global.page.pagetype = pagetype;
	global.page.title = title;
	include("/scripting/header_" + pagetype + ".esp");
}

/*
  show the page footer, getting the page type from page.pagetype
  set in page_header()
*/
function page_footer() {
	include("/scripting/footer_" + global.page.pagetype + ".esp");
}


/*
  check if a uri is one of the 'always allowed' pages, even when not logged in
  This allows the login page to use the same style sheets and images
*/
function always_allowed(uri) {
	var allowed = new Array("/images/favicon.ico", 
				"/images/linkpad.gif",
				"/images/logo.png",
				"/images/logo.gif",
				"/style/main.css",
				"/style/common.css");
	for (i in allowed) {
		if (allowed[i] == uri) {
			return true;
		}
	}
	return false;
}

/*
  display a simple menu. First argument is menu title, followed by
  pairs of menu item name and link
*/
function simple_menu() {
	write("<i>" + arguments[0] + "</i><br /><ul>\n");
	for (i = 1; i < arguments.length; i = i + 2) {
		write("<li><a href=\"" + arguments[i+1] + "\">" + arguments[i] + "</a></li>\n");
	}
	write("</ul>\n");
}


/*
  display a ejs object as a table
*/
function simple_table(v) {
	write("<table class=\"data\">\n");
	for (r in v) {
		write("<tr><td>" + r + "</td><td>" + v[r] + "</td></tr>\n");
	}
	write("</table>\n");
}

/*
  display an array of objects, with the header for each element from the given 
  attribute
*/
function multi_table(array, header) {
	write("<table class=\"data\">\n");
	for (i in array) {
		var v = array[i];
		write("<tr><th colspan=2>" + v[header] + "</th></tr>\n");
		for (r in v) {
			if (r != header) {
				write("<tr><td align=left>" + r + 
				      "</td><td align=left>" + v[r] + "</td></tr>\n");
			}
		}
	}
	write("</table>\n");
}

/*
  create a Form object with the defaults filled in, ready for display_form()
 */
function Form(name, num_elements, num_submits)
{
	var f = new Object();
	f.name = name;
	f.element = new Array(num_elements);
	f.submit =  new Array(num_submits);
	f.action = session_uri(request.REQUEST_URI);
	f.class = "form";
	for (i in f.element) {
		f.element[i] = new Object();
		f.element[i].type = "text";
		f.element[i].value = "";
	}
	return f;
}

/*
  display a simple form from a ejs Form object
  caller should fill in
    f.name          = form name
    f.action        = action to be taken on submit (optional, defaults to current page)
    f.class         = css class (optional, defaults to 'form')
    f.submit        = an array of submit labels
    f.element[i].label = element label
    f.element[i].name  = element name (defaults to label)
    f.element[i].type  = element type (defaults to text)
    f.element[i].value = current value (optional, defaults to "")
 */
function display_form(f) {
	write('<form name="' + f.name +
	      '" method="post" action="' + f.action + 
	      '" class="' + f.class + '">\n');
	write("<table>\n");
	for (i in f.element) {
		var e = f.element[i];
		if (e.name == undefined) {
			e.name = e.label;
		}
		if (e.value == undefined) {
			e.value = '""';
		}
		write("<tr>");
		write("<td>" + e.label + "</td>");
		if (e.type == "select") {
			write('<td><select name="' + e.name + '">\n');
			for (s in e.list) {
				if (e.value == e.list[s]) {
					write('<option selected=selected>' + e.list[s] + '</option>\n');
				} else {
					write('<option>' + e.list[s] + '</option>\n');
				}
			}
			write('</select></td>\n');
		} else {
			write('<td><input name="' + e.name + '" type="' + 
			      e.type + '" value="' + e.value + '"></td>\n');
		}
	}
	write("</table>\n");
	for (i in f.submit) {
		write('<input name="submit" type="submit" value="' + f.submit[i] + '">\n');
	}
	write("</form>\n");
}

