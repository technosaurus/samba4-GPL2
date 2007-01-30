/*                                                                                                                    
 * Copyright::                                                                                                         
 *   (C) 2006 by Simo Sorce
 * 
 * License: 
 *   GPL v2 or later
 */

/**
 * Ldb Modifier Class
 */

qx.OO.defineClass("swat.module.ldbbrowse.LdbModify", qx.ui.layout.VerticalBoxLayout,
function(fsm)
{
  qx.ui.layout.VerticalBoxLayout.call(this);

  this._mainArea = new qx.ui.layout.VerticalBoxLayout();
  this._mainArea.set({
                     height: "1*",
                     spacing: 5
                    });

  // Add an horizonatl layout for the "New" and "Modify" buttons
  // We need a vertical box layout for the tree and the buttons
  this._hlayout = new qx.ui.layout.HorizontalBoxLayout();
  this._hlayout.set({
               height: "auto",
               spacing: 10
           });

  // add a spacer to align buttons to the right
  this._leftSpacer = new qx.ui.basic.HorizontalSpacer();

  // Add the "Cancel" button
  this._cancelbtn = new qx.ui.form.Button("Cancel");
  this._cancelbtn.addEventListener("execute", this._cancelOp, this);

  // Add the "OK" button
  this._okbtn = new qx.ui.form.Button("OK");
  this._okbtn.addEventListener("execute", fsm.eventListener, fsm);

  // We'll be receiving events on the object, so save its friendly name
  fsm.addObject("domod", this._okbtn, "swat.main.fsmUtils.disable_during_rpc");

  // Add the buttons to the hlayout
  this._hlayout.add(this._leftSpacer, this._cancelbtn, this._okbtn);
  
  // Add the hlayout to the vlayout.
  this.add(this._mainArea, this._hlayout);

  // By default this is a new record creator
  this._type = "new";

  // By default this is inactive
  this._active = false;

  this.basedn = "";
});

qx.OO.addProperty({ name : "basedn", type : "string" });

/**
 * Set the type of operation
 * 
 * @param type {String}
 *   A string containing "new" or "modify"
 *
 * @param data {object}
 *   An LDB object with the current object parameters
 *   Used only if type = "modify"
 *
 */

qx.Proto.isActive = function() {
  if (this._active == true) {
    return true;
  }
}

/** 
 * Set the base of the object to add
 *
 * @param type {string}
 *   A string containing the base DN
 */

qx.Proto.setBase = function(base) {

  this.basedn = base;

  if (this._active) {
    if (this._type == "new") {

      this._basedn.setValue(this.basedn);
      this._basedn.setWidth(8 * this.basedn.length);
    }
  }
}

qx.Proto.initNew = function(callback, obj) {

  this._setExitCallback(callback, obj);

  this._active = true;
  this._type = "new";

  var hlayout = new qx.ui.layout.HorizontalBoxLayout();
  hlayout.set({ height: "auto", spacing: 10 });

  var dnlabel = new qx.ui.basic.Label("DN: ");

  // The name of the new/modified object
  // TODO: add validator
  this._rdn = new qx.ui.form.TextField(""); 
  this._rdn.setWidth(128);

  var dnsep = new qx.ui.basic.Label(",");

  // The basedn of the object
  // TODO: add validator
  this._basedn = new qx.ui.form.TextField(this.basedn);
  this._basedn.setWidth(8 * this.basedn.length);

  hlayout.add(dnlabel, this._rdn, dnsep, this._basedn);

  this._mainArea.add(hlayout);

  return;
}

qx.Proto.initMod = function(table, callback, obj) {

  this._setExitCallback(callback, obj);

  if (this.basedn == "") {
    this._callExitCallback();
    return;
  }

  this._active = true;
  this._type = "modify";

  this._dn = new qx.ui.basic.Label("DN: " + this.basedn);

  this._mainArea.add(this._dn);

  //TODO: for each entry in the table, add new entries in the object

  return;
}

qx.Proto._setExitCallback = function(vFunction, vObject) {

  if(typeof vFunction !== "function") {
    throw new Error("swat.module.ldbbrowse.LdbModify: setExitCallback(" + vFunction + "' is not a function!");
  }

  this._exitCallback = {
      handler : vFunction,
      object : vObject
    }
}

qx.Proto._callExitCallback = function() {

  // Shortcuts for handler and object
  vFunction = this._exitCallback.handler;
  vObject = this._exitCallback.object;

  // Call object function
  try
  {
    if(typeof vFunction === "function") {
      vFunction.call(qx.util.Validation.isValid(vObject) ? vObject : this);
    }
  }
  catch(ex)
  {
    this.error("swat.module.ldbbrowse.LdbModify: Could not call exit callback: ", ex);
  }
}

qx.Proto._reset = function() {

  // Remove existing attributes
  this._mainArea.removeAll();
  this._active = false;
  this._type = "null";
  return;
}

qx.Proto._cancelOp = function() {

  this._reset();
  this._callExitCallback();
}
