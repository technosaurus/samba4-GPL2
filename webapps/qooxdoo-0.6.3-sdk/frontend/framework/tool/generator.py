#!/usr/bin/env python

import sys, re, os, optparse

# reconfigure path to import own modules from modules subfolder
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(sys.argv[0])), "modules"))

import config, tokenizer, loader, api, tree, treegenerator, settings, resources, filetool, stringoptimizer, optparseext, variableoptimizer, obfuscator, compiler, migrator, textutil






def getparser():
  parser = optparse.OptionParser("usage: %prog [options]", option_class=optparseext.ExtendAction)


  #################################################################################
  # GENERAL
  #################################################################################

  # From/To File
  parser.add_option("--from-file", dest="fromFile", metavar="FILENAME", help="Read options from FILENAME.")
  parser.add_option("--export-to-file", dest="exportToFile", metavar="FILENAME", help="Store options to FILENAME.")

  # Directories (Lists, Match using index)
  parser.add_option("--script-input", action="extend", dest="scriptInput", metavar="DIRECTORY", type="string", default=[], help="Define a script input directory.")
  parser.add_option("--script-encoding", action="extend", dest="scriptEncoding", metavar="ENCODING", type="string", default=[], help="Define the encoding for a script input directory.")
  parser.add_option("--source-script-path", action="extend", dest="sourceScriptPath", metavar="PATH", type="string", default=[], help="Define a script path for the source version.")
  parser.add_option("--resource-input", action="extend", dest="resourceInput", metavar="DIRECTORY", type="string", default=[], help="Define a resource input directory.")
  parser.add_option("--resource-output", action="extend", dest="resourceOutput", metavar="DIRECTORY", type="string", default=[], help="Define a resource output directory.")

  # Available Actions
  parser.add_option("--generate-compiled-script", action="store_true", dest="generateCompiledScript", default=False, help="Compile source files.")
  parser.add_option("--generate-source-script", action="store_true", dest="generateSourceScript", default=False, help="Generate source version.")
  parser.add_option("--generate-api-documentation", action="store_true", dest="generateApiDocumentation", default=False, help="Generate API documentation.")
  parser.add_option("--copy-resources", action="store_true", dest="copyResources", default=False, help="Copy resource files.")
  parser.add_option("--fix-source", action="store_true", dest="fixSource", default=False, help="Fix source files")
  parser.add_option("--pretty-print", action="store_true", dest="prettyPrint", default=False, help="Pretty print source code.")
  parser.add_option("--migrate-source", action="store_true", dest="migrateSource", default=False, help="Migrate existing code to new version.")

  # Debug Actions
  parser.add_option("--store-tokens", action="store_true", dest="storeTokens", default=False, help="Store tokenized content of source files. (Debugging)")
  parser.add_option("--store-tree", action="store_true", dest="storeTree", default=False, help="Store tree content of source files. (Debugging)")
  parser.add_option("--print-files", action="store_true", dest="printFiles", default=False, help="Output known files. (Debugging)")
  parser.add_option("--print-modules", action="store_true", dest="printModules", default=False, help="Output known modules. (Debugging)")
  parser.add_option("--print-files-without-modules", action="store_true", dest="printFilesWithoutModules", default=False, help="Output files which have no module connection. (Debugging)")
  parser.add_option("--print-includes", action="store_true", dest="printIncludes", default=False, help="Output sorted file list. (Debugging)")
  parser.add_option("--print-dependencies", action="store_true", dest="printDeps", default=False, help="Output dependencies of files. (Debugging)")

  # Output files
  parser.add_option("--source-script-file", dest="sourceScriptFile", metavar="FILENAME", help="Name of output file from source build process.")
  parser.add_option("--compiled-script-file", dest="compiledScriptFile", metavar="FILENAME", help="Name of output file from compiler.")
  parser.add_option("--api-documentation-json-file", dest="apiDocumentationJsonFile", metavar="FILENAME", help="Name of JSON API file.")
  parser.add_option("--api-documentation-xml-file", dest="apiDocumentationXmlFile", metavar="FILENAME", help="Name of XML API file.")
  parser.add_option("--settings-script-file", dest="settingsScriptFile", metavar="FILENAME", help="Name of settings script file.")

  # Encoding
  parser.add_option("--script-output-encoding", dest="scriptOutputEncoding", default="utf-8", metavar="ENCODING", help="Defines the encoding used for script output files.")
  parser.add_option("--xml-output-encoding", dest="xmlOutputEncoding", default="utf-8", metavar="ENCODING", help="Defines the encoding used for XML output files.")



  #################################################################################
  # OPTIONS
  #################################################################################

  # General options
  parser.add_option("-q", "--quiet", action="store_false", dest="verbose", default=False, help="Quiet output mode.")
  parser.add_option("-v", "--verbose", action="store_true", dest="verbose", help="Verbose output mode.")
  parser.add_option("-d", "--debug", action="store_true", dest="enableDebug", help="Enable debug mode.")
  parser.add_option("--package-id", dest="packageId", default="", metavar="ID", help="Defines a package ID (required for string optimization etc.)")
  parser.add_option("--disable-internal-check", action="store_true", dest="disableInternalCheck", default=False, help="Disable check of modifications to internal files.")

  # Options for source and compiled version
  parser.add_option("--define-runtime-setting", action="append", dest="defineRuntimeSetting", metavar="NAMESPACE.KEY:VALUE", default=[], help="Define a setting.")
  parser.add_option("--add-new-lines", action="store_true", dest="addNewLines", default=False, help="Keep newlines in compiled files.")

  # Options for source version
  parser.add_option("--source-loader-type", action="store", dest="sourceLoaderType", metavar="TYPE", choices=("auto", "docwrite", "domappend"), default="auto", help="Generated source loader type: auto, docwrite, domappend [default: %default].")

  # Options for compiled version
  parser.add_option("--add-file-ids", action="store_true", dest="addFileIds", default=False, help="Add file IDs to compiled output.")
  parser.add_option("--optimize-strings", action="store_true", dest="optimizeStrings", default=False, help="Optimize strings. Increase mshtml performance.")
  parser.add_option("--optimize-variables", action="store_true", dest="optimizeVariables", default=False, help="Optimize variables. Reducing size.")
  parser.add_option("--obfuscate-identifiers", action="store_true", dest="obfuscateIdentifiers", default=False, help="Obfuscate public names like function names. (ALPHA!)")

  # Options for resource copying
  parser.add_option("--override-resource-output", action="append", dest="overrideResourceOutput", metavar="CLASSNAME.ID:DIRECTORY", default=[], help="Define a resource input directory.")

  # Options for token/tree storage
  parser.add_option("--token-output-directory", dest="tokenOutputDirectory", metavar="DIRECTORY", help="Define output directory for tokenizer result of the incoming JavaScript files. (Debugging)")
  parser.add_option("--tree-output-directory", dest="treeOutputDirectory", metavar="DIRECTORY", help="Define output directory for generated tree of the incoming JavaScript files. (Debugging)")

  # Cache Directory
  parser.add_option("--cache-directory", dest="cacheDirectory", metavar="DIRECTORY", help="If this is defined the loader trys to use cache to optimize the performance.")

  # Options for migration support
  parser.add_option("--migration-target", dest="migrationTarget", metavar="VERSION", help="Define the target for migration of source code.")
  parser.add_option("--migration-input", action="extend", dest="migrationInput", metavar="DIRECTORY", type="string", default=[], help="Define additional directories for to directories to migrate e.g. HTML files, ...")
  



  #################################################################################
  # INCLUDE/EXCLUDE
  #################################################################################

  # Include/Exclude
  parser.add_option("-i", "--include", action="extend", dest="includeWithDeps", metavar="ID", type="string", default=[], help="Include ID")
  parser.add_option("-e", "--exclude", action="extend", dest="excludeWithDeps", metavar="ID", type="string", default=[], help="Exclude ID")
  parser.add_option("--include-without-dependencies", action="extend", dest="includeWithoutDeps", metavar="ID", type="string", default=[], help="Include ID")
  parser.add_option("--exclude-without-dependencies", action="extend", dest="excludeWithoutDeps", metavar="ID", type="string", default=[], help="Exclude ID")

  # Include/Exclude options
  parser.add_option("--disable-auto-dependencies", action="store_false", dest="enableAutoDependencies", default=True, help="Disable detection of dependencies.")

  return parser






def argparser(cmdlineargs):

  # Parse arguments
  (options, args) = getparser().parse_args(cmdlineargs)

  # Export to file
  if options.exportToFile != None:
    print
    print "  EXPORTING:"
    print "----------------------------------------------------------------------------"

    print " * Translating options..."

    optionString = "# Exported configuration from build.py\n\n"
    ignoreValue = True
    lastWasKey = False

    for arg in cmdlineargs:
      if arg == "--export-to-file":
        ignoreValue = True

      elif arg.startswith("--"):
        if lastWasKey:
          optionString += "\n"

        optionString += arg[2:]
        ignoreValue = False
        lastWasKey = True

      elif arg.startswith("-"):
        print "   * Couldn't export short argument: %s" % arg
        optionString += "\n# Ignored short argument %s\n" % arg
        ignoreValue = True

      elif not ignoreValue:
        optionString += " = %s\n" % arg
        ignoreValue = True
        lastWasKey = False



    print " * Export to file: %s" % options.exportToFile
    filetool.save(options.exportToFile, optionString)

    sys.exit(0)

  # Read from file
  elif options.fromFile != None:

    print
    print "  INITIALIZATION:"
    print "----------------------------------------------------------------------------"

    print "  * Reading configuration..."

    # Convert file content into arguments
    fileargs = {}
    fileargpos = 0
    fileargid = "default"
    currentfileargs = []
    fileargs[fileargid] = currentfileargs

    alternativeFormatBegin = re.compile("\s*\[\s*")
    alternativeFormatEnd = re.compile("\s*\]\s*=\s*")
    emptyLine = re.compile("^\s*$")

    for line in file(options.fromFile).read().split("\n"):
      line = line.strip()

      if emptyLine.match(line) or line.startswith("#") or line.startswith("//"):
        continue

      # Translating...
      line = alternativeFormatBegin.sub(" = ", line)
      line = alternativeFormatEnd.sub(":", line)

      # Splitting line
      line = line.split("=")

      # Extract key element
      key = line.pop(0).strip()

      # Separate packages
      if key == "package":
        fileargpos += 1
        fileargid = line[0].strip()

        print "    - Found package: %s" % fileargid

        currentfileargs = []
        fileargs[fileargid] = currentfileargs
        continue

      currentfileargs.append("--%s" % key)

      if len(line) > 0:
        value = line[0].strip()
        currentfileargs.append(value)

    # Parse
    defaultargs = fileargs["default"]

    if len(fileargs) > 1:
      (fileDb, moduleDb) = load(getparser().parse_args(defaultargs)[0])

      if options.obfuscateIdentifiers:
        sharednames = {}

        for filearg in fileargs:
          if filearg == "default":
            continue

          combinedargs = []
          combinedargs.extend(defaultargs)
          combinedargs.extend(fileargs[filearg])

          options = getparser().parse_args(defaultargs)[0]
          findnames(fileDb, moduleDb, options, sharednames)

        names = obfuscator.sort(sharednames)

      for filearg in fileargs:
        if filearg == "default":
          continue

        print
        print
        print
        print
        print "  PACKAGE: %s" % filearg
        print "----------------------------------------------------------------------------"

        combinedargs = []
        combinedargs.extend(defaultargs)
        combinedargs.extend(fileargs[filearg])

        options = getparser().parse_args(combinedargs)[0]
        execute(fileDb, moduleDb, options, filearg, names)

    else:
      options = getparser().parse_args(defaultargs)[0]
      (fileDb, moduleDb) = load(options)

      if options.obfuscateIdentifiers:
        execute(fileDb, moduleDb, options, "", obfuscator.sort(findnames(fileDb, moduleDb, options)))
      else:
        execute(fileDb, moduleDb, options, "", names)

  else:
    print
    print "  INITIALIZATION:"
    print "----------------------------------------------------------------------------"

    print "  * Processing arguments..."

    (fileDb, moduleDb) = load(options)

    if options.obfuscateIdentifiers:
      execute(fileDb, moduleDb, options, options.packageId, obfuscator.sort(findnames(fileDb, moduleDb, options)))
    else:
      execute(fileDb, moduleDb, options, options.packageId)







def main():
  if len(sys.argv[1:]) == 0:
    basename = os.path.basename(sys.argv[0])
    print "usage: %s [options]" % basename
    print "Try '%s -h' or '%s --help' to show the help message." % (basename, basename)
    sys.exit(1)

  argparser(sys.argv[1:])






def load(options):

  ######################################################################
  #  SOURCE LOADER
  ######################################################################

  print
  print "  SOURCE LOADER:"
  print "----------------------------------------------------------------------------"

  if options.scriptInput == None or len(options.scriptInput) == 0:
    if len(options.migrationInput) == 0:
      basename = os.path.basename(sys.argv[0])
      print "You must define at least one script input directory!"
      print "usage: %s [options]" % basename
      print "Try '%s -h' or '%s --help' to show the help message." % (basename, basename)
      sys.exit(1)

  (fileDb, moduleDb) = loader.indexScriptInput(options)





  ######################################################################
  #  DEBUG OUTPUT JOBS
  ######################################################################

  if options.printFiles:
    print
    print "  OUTPUT OF KNOWN FILES:"
    print "----------------------------------------------------------------------------"
    print "  * These are all known files:"
    for fileEntry in fileDb:
      print "    - %s (%s)" % (fileEntry, fileDb[fileEntry]["path"])

  if options.printModules:
    print
    print "  OUTPUT OF KNOWN MODULES:"
    print "----------------------------------------------------------------------------"
    print "  * These are all known modules:"
    for moduleEntry in moduleDb:
      print "    * %s" % moduleEntry
      for fileEntry in moduleDb[moduleEntry]:
        print "      - %s" % fileEntry

  if options.printFilesWithoutModules:
    print
    print "  OUTPUT OF FILES WHICH HAVE NO MODULE CONNECTION:"
    print "----------------------------------------------------------------------------"
    print "  * These are all files without a module connection:"
    for fileEntry in fileDb:
      fileFound = False

      for moduleEntry in moduleDb:
        for moduleFile in moduleDb[moduleEntry]:
          if moduleFile == fileEntry:
            fileFound = True
            break

      if not fileFound:
        print "    - %s" % fileEntry



  return fileDb, moduleDb





def findnames(fileDb, moduleDb, options, names={}):

  print
  print "  SEARCHING FOR IDENTIFIERS:"
  print "----------------------------------------------------------------------------"

  if options.verbose:
    print "  * Searching..."
  else:
    print "  * Searching: ",


  sortedIncludeList = loader.getSortedList(options, fileDb, moduleDb)

  for fileId in sortedIncludeList:
    if options.verbose:
      print "    - %s" % fileId

    else:
      sys.stdout.write(".")
      sys.stdout.flush()

    obfuscator.search(loader.getTree(fileDb, fileId, options), names)

  if not options.verbose:
    print

  return names








def execute(fileDb, moduleDb, options, pkgid="", names=[]):

  additionalOutput = []


  ######################################################################
  #  SORT OF INCLUDE LIST
  ######################################################################

  print
  print "  SORT OF INCLUDE LIST:"
  print "----------------------------------------------------------------------------"

  if options.verbose:
    print "  * Include (with dependencies): %s" % options.includeWithDeps
    print "  * Include (without dependencies): %s" % options.includeWithoutDeps
    print "  * Exclude (with dependencies): %s" % options.excludeWithDeps
    print "  * Exclude (without dependencies): %s" % options.excludeWithoutDeps

  print "  * Sorting classes..."

  sortedIncludeList = loader.getSortedList(options, fileDb, moduleDb)

  if len(sortedIncludeList) == len(fileDb):
    print "  * Including all classes"

  print "  * Arranged %s classes" % len(sortedIncludeList)

  if options.printIncludes:
    print
    print "  PRINT OF INCLUDE ORDER:"
    print "----------------------------------------------------------------------------"
    print "  * The files will be included in this order:"
    for fileId in sortedIncludeList:
      print "    - %s" % fileId

  if options.printDeps:
    print
    print "  OUTPUT OF DEPENDENCIES:"
    print "----------------------------------------------------------------------------"
    print "  * These are all included files with their dependencies:"
    for fileId in sortedIncludeList:
      print "    - %s" % fileId
      if len(fileDb[fileId]["loadtimeDeps"]) > 0:
        print "      - Loadtime: "
        for depEntry in fileDb[fileId]["loadtimeDeps"]:
          print "        - %s" % depEntry

      if len(fileDb[fileId]["afterDeps"]) > 0:
        print "      - After: "
        for depEntry in fileDb[fileId]["afterDeps"]:
          print "        - %s" % depEntry

      if len(fileDb[fileId]["runtimeDeps"]) > 0:
        print "      - Runtime: "
        for depEntry in fileDb[fileId]["runtimeDeps"]:
          print "        - %s" % depEntry

      if len(fileDb[fileId]["beforeDeps"]) > 0:
        print "      - Before: "
        for depEntry in fileDb[fileId]["beforeDeps"]:
          print "        - %s" % depEntry

      if len(fileDb[fileId]["optionalDeps"]) > 0:
        print "      - Optional: "
        for depEntry in fileDb[fileId]["optionalDeps"]:
          print "        - %s" % depEntry






  ######################################################################
  #  SOURCE MIGRATION
  ######################################################################

  if options.migrateSource:
    print
    print "  SOURCE MIGRATION:"
    print "----------------------------------------------------------------------------"

    print "  * Migrate Source Code..."

    migrator.handle(sortedIncludeList, fileDb, options)

    # Return after migration: Ignore other jobs
    return


  ######################################################################
  #  GENERATION OF PRETTY PRINTED CODE
  ######################################################################

  if options.fixSource:
    print
    print "  FIX SOURCE CODE:"
    print "----------------------------------------------------------------------------"

    if options.verbose:
      print "  * Fixing code..."
    else:
      print "  * Fixing code: ",

    for fileId in sortedIncludeList:
      if options.verbose:
        print "    - Reading %s" % fileId

      fileEntry = fileDb[fileId]

      filePath = fileEntry["path"]
      fileEncoding = fileEntry["encoding"]

      fileContent = filetool.read(filePath, fileEncoding)
      fixedContent = textutil.removeTrailingSpaces(textutil.tab2Space(textutil.any2Unix(fileContent), 2))

      if fixedContent != fileContent:
        if options.verbose:
          print "      - Storing modifications..."
        else:
          sys.stdout.write("!")
          sys.stdout.flush()

        filetool.save(filePath, fixedContent, fileEncoding)

      elif not options.verbose:
        sys.stdout.write(".")
        sys.stdout.flush()

    if not options.verbose:
      print

    # Return after fixing: Ignore other jobs
    return



  ######################################################################
  #  GENERATION OF PRETTY PRINTED CODE
  ######################################################################

  if options.prettyPrint:
    print
    print "  GENERATION OF PRETTY PRINTED CODE:"
    print "----------------------------------------------------------------------------"

    if options.verbose:
      print "  * Pretty printing..."
    else:
      print "  * Pretty printing: ",

    for fileId in sortedIncludeList:
      if options.verbose:
        print "    - Compiling %s" % fileId
      else:
        sys.stdout.write(".")
        sys.stdout.flush()

      prettyFileContent = compiler.compile(loader.getTree(fileDb, fileId, options), True)

      if not prettyFileContent.endswith("\n"):
        prettyFileContent += "\n"

      filetool.save(fileDb[fileId]["path"], prettyFileContent)

    if not options.verbose:
      print

    # Return after pretty print: Ignore other jobs
    return



  ######################################################################
  #  STRING OPTIMIZATION
  ######################################################################

  if options.optimizeStrings:
    print
    print "  STRING OPTIMIZATION:"
    print "----------------------------------------------------------------------------"

    if options.verbose:
      print "  * Searching strings..."
    else:
      print "  * Searching strings: ",

    stringMap = {}

    for fileId in sortedIncludeList:
      if options.verbose:
        print "    - %s" % fileId
      else:
        sys.stdout.write(".")
        sys.stdout.flush()

      localMap = loader.getStrings(fileDb, fileId, options)

      for value in localMap:
        if value in stringMap:
          stringMap[value] += localMap[value]
        else:
          stringMap[value] = localMap[value]

    if not options.verbose:
      print

    counter = 0
    for value in stringMap:
      counter += stringMap[value]

    stringList = stringoptimizer.sort(stringMap)

    print "  * Found %s strings (used %s times)" % (len(stringMap), counter)

    if options.verbose:
      print "  * Replacing strings..."
    else:
      print "  * Replacing strings: ",

    for fileId in sortedIncludeList:
      if options.verbose:
        print "    - %s" % fileId
      else:
        sys.stdout.write(".")
        sys.stdout.flush()

      stringoptimizer.replace(loader.getTree(fileDb, fileId, options), stringList, "$" + pkgid, options.verbose)

    if not options.verbose:
      print

    print "  * Generating replacement..."
    additionalOutput.append(stringoptimizer.replacement(stringList, "$" + pkgid))






  ######################################################################
  #  LOCAL VARIABLE OPTIMIZATION
  ######################################################################

  if options.optimizeVariables:
    print
    print "  LOCAL VARIABLE OPTIMIZATION:"
    print "----------------------------------------------------------------------------"

    if options.verbose:
      print "  * Optimizing variables..."
    else:
      print "  * Optimizing variables: ",

    for fileId in sortedIncludeList:
      if options.verbose:
        print "    - %s" % fileId
      else:
        sys.stdout.write(".")
        sys.stdout.flush()

      variableoptimizer.search(loader.getTree(fileDb, fileId, options), [], 0, "$")

    if not options.verbose:
      print






  ######################################################################
  #  NAME OBFUSCATION
  ######################################################################

  if options.obfuscateIdentifiers:
    print
    print "  OBFUSCATE IDENTIFIERS:"
    print "----------------------------------------------------------------------------"

    if options.verbose:
      print "  * Obfuscating identifiers..."
    else:
      print "  * Obfuscating identifiers: ",

    counter = 0

    for fileId in sortedIncludeList:
      if options.verbose:
        print "    - %s" % fileId
      else:
        sys.stdout.write(".")
        sys.stdout.flush()

      counter += obfuscator.update(loader.getTree(fileDb, fileId, options), names, "$$")

    if not options.verbose:
      print

    print "  * Updated %s names" % counter






  ######################################################################
  #  TOKEN STORAGE
  ######################################################################

  if options.storeTokens:
    print
    print "  TOKEN STORAGE:"
    print "----------------------------------------------------------------------------"

    if options.tokenOutputDirectory == None:
      print "  * You must define the token output directory!"
      sys.exit(1)

    if options.verbose:
      print "  * Storing tokens..."
    else:
      print "  * Storing tokens: ",

    for fileId in sortedIncludeList:
      tokenString = tokenizer.convertTokensToString(loader.getTokens(fileDb, fileId, options))

      if options.verbose:
        print "    * writing tokens for %s (%s KB)..." % (fileIdm, len(tokenString) / 1000.0)
      else:
        sys.stdout.write(".")
        sys.stdout.flush()

      filetool.save(os.path.join(filetool.normalize(options.tokenOutputDirectory), fileId + config.TOKENEXT), tokenString)

    if not options.verbose:
      print




  ######################################################################
  #  TREE STORAGE
  ######################################################################

  if options.storeTree:
    print
    print "  TREE STORAGE:"
    print "----------------------------------------------------------------------------"

    if options.treeOutputDirectory == None:
      print "  * You must define the tree output directory!"
      sys.exit(1)

    if options.verbose:
      print "  * Storing tree..."
    else:
      print "  * Storing tree: ",

    for fileId in sortedIncludeList:
      treeString = "<?xml version=\"1.0\" encoding=\"" + options.xmlOutputEncoding + "\"?>\n" + tree.nodeToXmlString(loader.getTree(fileDb, fileId, options))

      if options.verbose:
        print "    * writing tree for %s (%s KB)..." % (fileId, len(treeString) / 1000.0)
      else:
        sys.stdout.write(".")
        sys.stdout.flush()

      filetool.save(os.path.join(filetool.normalize(options.treeOutputDirectory), fileId + config.XMLEXT), treeString)

    if not options.verbose:
      print





  ######################################################################
  #  GENERATION OF API
  ######################################################################

  if options.generateApiDocumentation:
    print
    print "  GENERATION OF API:"
    print "----------------------------------------------------------------------------"

    if options.apiDocumentationJsonFile == None and options.apiDocumentationXmlFile == None:
      print "  * You must define one of JSON or XML API documentation file!"

    docTree = None

    if options.verbose:
      print "  * Generating API tree..."
    else:
      print "  * Generating API tree: ",

    for fileId in sortedIncludeList:
      if options.verbose:
        print "    - %s" % fileId
      else:
        sys.stdout.write(".")
        sys.stdout.flush()

      docTree = api.createDoc(loader.getTree(fileDb, fileId, options), docTree)

    if not options.verbose:
      print

    if docTree:
      print "  * Finalising tree..."
      api.postWorkPackage(docTree, docTree)

    if options.apiDocumentationXmlFile != None:
      print "  * Writing XML API file to %s" % options.apiDocumentationXmlFile

      xmlContent = "<?xml version=\"1.0\" encoding=\"" + options.xmlOutputEncoding + "\"?>\n"

      if options.addNewLines:
        xmlContent += "\n" + tree.nodeToXmlString(docTree)
      else:
        xmlContent += tree.nodeToXmlString(docTree, "", "", "")

      filetool.save(options.apiDocumentationXmlFile, xmlContent, options.xmlOutputEncoding)

    if options.apiDocumentationJsonFile != None:
      print "  * Writing JSON API file to %s" % options.apiDocumentationJsonFile

      if options.addNewLines:
        jsonContent = tree.nodeToJsonString(docTree)
      else:
        jsonContent = tree.nodeToJsonString(docTree, "", "", "")

      filetool.save(options.apiDocumentationJsonFile, jsonContent, options.scriptOutputEncoding)





  ######################################################################
  #  CREATE COPY OF RESOURCES
  ######################################################################

  if options.copyResources:

    print
    print "  CREATE COPY OF RESOURCES:"
    print "----------------------------------------------------------------------------"

    resources.copy(options, sortedIncludeList, fileDb)






  ######################################################################
  #  GENERATION OF SETTINGS
  ######################################################################

  if options.generateSourceScript or options.generateCompiledScript:
    settingsStr = ""

    if len(options.defineRuntimeSetting) != 0:
      print
      print "  GENERATION OF SETTINGS:"
      print "----------------------------------------------------------------------------"

      print "  * Processing input data..."
      settingsStr = settings.generate(options)

      if options.settingsScriptFile:
        print "   * Storing result to %s" % options.settingsScriptFile
        filetool.save(options.settingsScriptFile, settingsStr)

        # clear settings for build and source
        settingsStr = ""





  ######################################################################
  #  GENERATION OF SOURCE VERSION
  ######################################################################

  if options.generateSourceScript:
    print
    print "  GENERATION OF SOURCE SCRIPT:"
    print "----------------------------------------------------------------------------"

    if options.sourceScriptFile == None:
      print "  * You must define the source script file!"
      sys.exit(1)

    else:
      options.sourceScriptFile = os.path.normpath(options.sourceScriptFile)

    print "  * Generating includer..."

    sourceOutput = settingsStr

    srcEol = "";
    if options.addNewLines:
      srcEol = "\n";

    if sourceOutput != "":
      settingsStr += srcEol

    # Define javascript loaders
    jsLoaders = {}

    # HTML-only: create <script> tags using document.write()
    jsLoaders["docwrite"] = """var includeJs=function(src){document.write('<script type="text/javascript" src="'+src+'"></script>')};""" 

    # XHTML-compatible: create and append DOM script nodes 
    jsLoaders["domappend"] = """if(document.createElementNS&&parentNode.namespaceURI)""" + srcEol + """var includeJs=function(src){var js=document.createElementNS(parentNode.namespaceURI,"script");js.type="text/javascript";js.src=src;parentNode.appendChild(js)};""" + srcEol + """else """ + srcEol + """var includeJs=function(src){var js=document.createElement("script");js.type="text/javascript";js.src=src;parentNode.appendChild(js)};"""

    # Source loader closure
    sourceOutput += """(function(sources){""" + srcEol

    # Detect the node we are being called from
    sourceOutput += """var parentNode=document.getElementsByTagName('body')[0]||document.getElementsByTagName('head')[0];""" + srcEol

    # Autoselect a loader based on client engine
    if options.sourceLoaderType == "auto":
      sourceOutput += """var clientEngine=null;""" + srcEol

      # Opera
      sourceOutput += """if(window.opera&&/Opera[\s\/]([0-9\.]*)/.test(navigator.userAgent))clientEngine="opera";else """ + srcEol

      # Khtml
      sourceOutput += """if(typeof navigator.vendor==="string"&&navigator.vendor==="KDE"&&/KHTML\/([0-9-\.]*)/.test(navigator.userAgent))clientEngine="khtml";else """ + srcEol

      # Webkit
      sourceOutput += """if(navigator.userAgent.indexOf("AppleWebKit")!=-1&&/AppleWebKit\/([0-9-\.]*)/.test(navigator.userAgent))clientEngine="webkit";else """ + srcEol

      # Gecko
      sourceOutput += """if(window.controllers&&typeof navigator.product==="string"&&navigator.product==="Gecko"&&/rv\:([^\);]+)(\)|;)/.test(navigator.userAgent))clientEngine="gecko";else """ + srcEol

      # MShtml
      sourceOutput += """if(/MSIE\s+([^\);]+)(\)|;)/.test(navigator.userAgent))clientEngine="mshtml";""" + srcEol

      # Select the loader based on the engine
      sourceOutput += """switch(clientEngine){""" + srcEol

      # Use DOM
      sourceOutput += """case "opera": case "gecko":""" + srcEol
      sourceOutput += jsLoaders["domappend"] + srcEol
      sourceOutput += """break;""" + srcEol

      # Use document.write()
      sourceOutput += """case "webkit": case "khtml": case "mshtml": default:""" + srcEol
      sourceOutput += jsLoaders["docwrite"] + srcEol
      sourceOutput += """break;""" + srcEol

      sourceOutput += """}""" + srcEol

    # Use a fixed loader
    else:
      sourceOutput += jsLoaders[options.sourceLoaderType] + srcEol

    # Loading loop
    sourceOutput += """for(var i=0;i<sources.length;++i)includeJs(sources[i])})""" + srcEol

    sources = ""
    for fileId in sortedIncludeList:
      if fileDb[fileId]["sourceScriptPath"] == None:
        print "  * Missing source path definition for script input %s. Could not create source script file!" % fileDb[fileId]["scriptInput"]
        sys.exit(1)

      sources += srcEol + '"%s%s",' % (os.path.join(fileDb[fileId]["sourceScriptPath"], fileDb[fileId]["pathId"].replace(".", os.sep)), config.JSEXT) 

    # Pass the array with source files to include
    sourceOutput += "([" + sources[:-1] + srcEol + "]);" + srcEol

    print "  * Storing output as %s..." % options.sourceScriptFile
    filetool.save(options.sourceScriptFile, sourceOutput, options.scriptOutputEncoding)





  ######################################################################
  #  GENERATION OF COMPILED VERSION
  ######################################################################

  if options.generateCompiledScript:
    print
    print "  GENERATION OF COMPILED SCRIPT:"
    print "----------------------------------------------------------------------------"

    compiledOutput = settingsStr + "".join(additionalOutput)

    if options.compiledScriptFile == None:
      print "  * You must define the compiled script file!"
      sys.exit(1)

    if options.verbose:
      print "  * Compiling..."
    else:
      print "  * Compiling: ",

    for fileId in sortedIncludeList:
      if options.verbose:
        print "    - Compiling %s" % fileId
      else:
        sys.stdout.write(".")
        sys.stdout.flush()

      compiledFileContent = compiler.compile(loader.getTree(fileDb, fileId, options), False, options.addNewLines, options.enableDebug)

      if options.addFileIds:
        compiledOutput += "\n\n\n/* ID: " + fileId + " */\n" + compiledFileContent + "\n"
      else:
        compiledOutput += compiledFileContent

      if not compiledOutput.endswith(";") and not compiledOutput.endswith("\n"):
        compiledOutput += ";"

    if not options.verbose:
      print

    print "  * Storing output as %s..." % options.compiledScriptFile
    filetool.save(options.compiledScriptFile, compiledOutput, options.scriptOutputEncoding)







######################################################################
#  MAIN LOOP
######################################################################

if __name__ == '__main__':
  try:
    main()

  except KeyboardInterrupt:
    print
    print "  * Keyboard Interrupt"
    sys.exit(1)