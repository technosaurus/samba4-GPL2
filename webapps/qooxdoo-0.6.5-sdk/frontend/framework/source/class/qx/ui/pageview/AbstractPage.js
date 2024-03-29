/* ************************************************************************

   qooxdoo - the new era of web development

   http://qooxdoo.org

   Copyright:
     2004-2007 1&1 Internet AG, Germany, http://www.1and1.org

   License:
     LGPL: http://www.gnu.org/licenses/lgpl.html
     EPL: http://www.eclipse.org/org/documents/epl-v10.php
     See the LICENSE file in the project's top-level directory for details.

   Authors:
     * Sebastian Werner (wpbasti)
     * Andreas Ecker (ecker)

************************************************************************ */

/* ************************************************************************


************************************************************************ */

qx.OO.defineClass("qx.ui.pageview.AbstractPage", qx.ui.layout.CanvasLayout,
function(vButton)
{
  qx.ui.layout.CanvasLayout.call(this);

  if (vButton != null) {
    this.setButton(vButton);
  }
});





/*
---------------------------------------------------------------------------
  PROPERTIES
---------------------------------------------------------------------------
*/

/*!
  The attached tab of this page.
*/
qx.OO.addProperty({ name : "button", type : "object" });

/*!
  Make element displayed (if switched to true the widget will be created, if needed, too).
  Instead of qx.ui.core.Widget, the default is false here.
*/
qx.OO.changeProperty({ name : "display", type : "boolean", defaultValue : false });




/*
---------------------------------------------------------------------------
  MODIFIER
---------------------------------------------------------------------------
*/

qx.Proto._modifyButton = function(propValue, propOldValue, propData)
{
  if (propOldValue) {
    propOldValue.setPage(null);
  }

  if (propValue) {
    propValue.setPage(this);
  }

  return true;
}
