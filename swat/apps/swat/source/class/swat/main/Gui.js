/**
 * The graphical user interface for the main menu
 */
qx.OO.defineClass("swat.main.Gui");


qx.Class.buildGui = function(modules)
{
  var o;

  // TODO: these predefined yellow and blue colors are the wrong shade.  find
  // the correct colors.
  var topColor = new qx.renderer.color.Color("yellow");
  var bottomColor = new qx.renderer.color.Color("blue");

  // Create the yellow bar at the top
  o = new qx.ui.basic.Terminator();
  o.set({
            top: 0,
            left: 0,
            right: 0,
            height: 40,
        });
  o.setBackgroundColor(topColor);
  o.addToDocument();

  // Create the tag line
  var title = new qx.ui.basic.Label("Samba Web Administration Tool");
  title.set({
            top: 12,
            left: 20
        });
  title.setBackgroundColor(topColor);
  title.setFont("bold");
  title.addToDocument();

  // Create a small black separator between the yellow and blue bars
  o = new qx.ui.basic.Terminator();
  o.set({
            top: 40,
            left: 0,
            right: 0,
            height: 1
        });
  o.setBackgroundColor("black");
  o.addToDocument();

  // Create the yellow bar at the top
  o = new qx.ui.basic.Terminator();
  o.set({
            top: 41,
            left: 0,
            right: 0,
            height: 60
        });
  o.setBackgroundColor(bottomColor);
  o.addToDocument();

  // Create the "Samba" image
  o = new qx.ui.basic.Image("../../../images/logo.png");
  o.set({
            top: 16,
            right: 20
        });
  o.setZIndex(100000000);
  o.addToDocument();

  // Create a menu bar
  var menubar = new qx.ui.toolbar.ToolBar();
  menubar.set({
                  top: 100,
                  height: 20,
                  left: 0,
                  right: 0
              });
  menubar.addToDocument();
      
  // Create a menu item for selecting the requested module 
  var menubutton = new qx.ui.toolbar.MenuButton("Modules");
  menubar.add(menubutton);

  // Create a Command to be executed upon any menu selection
  var command = new qx.client.Command();
  //  command.addEventListener("execute", fsm.eventListener, fsm);
      
  // Create a radio manager for selecting one of the modules
  var moduleManager = new qx.manager.selection.RadioManager("main.gui.Module");

  // Create a menu containing the subitems
  var menu = new qx.ui.menu.Menu();

  // We'll also track the current module's canvas in the modules object
  modules.currentCanvas = null;

  // For each menu item...
  for (moduleName in modules.list)
  {
    // create a radio button menu item
    o = new qx.ui.menu.RadioButton(moduleName, null, command);

    // Associate this button menu item with the module list
    o.moduleName = moduleName;

    // Associate the menu item with the radio manager
    moduleManager.add(o);

    // Create this module's canvas
    var canvas = new qx.ui.layout.CanvasLayout();
    canvas.set({
                   top: 120,
                   bottom: 0,
                   left: 0,
                   right: 0
               });
    canvas.setBackgroundColor("white");
    canvas.setDisplay(false); // initially not displayed

    var fsm = modules.list[moduleName].fsm;
    fsm.addObject("canvas", canvas);
    canvas.addEventListener("appear", fsm.eventListener, fsm);
    canvas.addEventListener("disappear", fsm.eventListener, fsm);

    // Save the canvas
    modules.list[moduleName].canvas = canvas;

    // Add the canvas to the document
    canvas.addToDocument();

    // When a Module menu item is selected:
    o.addEventListener("changeChecked", function(e)
                       {
                         // If there's a current canvas, ...
                         if (modules.currentCanvas)
                         {
                           // ... then remove display of it.
                           modules.currentCanvas.setDisplay(false);
                           
                           // Dispatch an event on the canvas to notify old
                           // module it's coming into disuse.
                           canvas.createDispatchEvent("disappear");
                         }

                         // If we are being selected...
                         if (e.getData())
                         {
                           // then display our canvas
                           var canvas = modules.list[this.moduleName].canvas;
                           canvas.setDisplay(true);

                           // Track the current canvas (now ours)
                           modules.currentCanvas = canvas;

                           // Set the application title
                           title.setHtml("<span>" +
                                         "Samba Web Administration Tool" +
                                         " &raquo; " +
                                         this.moduleName +
                                         "</span>");
                           
                           // Dispatch an event on the canvas to notify new
                           // module it's coming into use.
                           canvas.createDispatchEvent("appear");
                         }
                       });

    // Add the menu item to the menu
    menu.add(o);
  }

  // We've built a complete menu.  Add it to the document.
  menu.addToDocument();

  // Specify that the menu is to be displayed upon menu button selection
  menubutton.setMenu(menu);

  // Create a menu item for selecting debug options
  var menubutton = new qx.ui.toolbar.MenuButton("Debug");
  menubar.add(menubutton);

  // Create a menu containing the subitems
  var menu = new qx.ui.menu.Menu;
  var menu_01 = new qx.ui.menu.Button("Show Debug Window", null, command);
  menu.add(menu_01);
  menu.addToDocument();

  // Specify that the menu is to be displayed upon menu button selection
  menubutton.setMenu(menu);

};